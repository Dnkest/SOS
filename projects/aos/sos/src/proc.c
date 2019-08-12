#include <elf/elf64.h>
#include <cspace/cspace.h>
#include <string.h>
#include <aos/sel4_zf_logif.h>
#include <aos/debug.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <clock/clock.h>

#include "proc.h"
#include "ut.h"
#include "globals.h"
#include "addrspace.h"
#include "vmem_layout.h"
#include "elfload.h"
#include "mapping.h"
#include "utils/eventq.h"

#include "fs/sos_nfs.h"
#include "fs/fd_table.h"
#include "fs/serial.h"

#include "utils/circular_id.h"
#include "utils/kmalloc.h"

#define MAX_PROCESS 128
#define BADGE_BASE  100

#define N_NAME 32

struct proc {
    char app_name[N_NAME];
    int id;
    int exiting;

    ut_t *tcb_ut;
    seL4_CPtr tcb;

    ut_t *vspace_ut;
    seL4_CPtr vspace;
    
    ut_t *ipc_buffer_ut;
    seL4_CPtr ipc_buffer;

    cspace_t cspace;
    int cspace_inited;

    addrspace_t *addrspace;
    fd_table_t *fdt;

    uint64_t start_timestamp;
    seL4_Word data[4];
    seL4_CPtr reply;

    int parents[128];
    int p_size;
};

static proc_t *processes[MAX_PROCESS];
static circular_id_t *pids = NULL;

/* helper to allocate a ut + cslot, and retype the ut into the cslot */
static ut_t *process_alloc_retype(proc_t *proc, cspace_t *cspace, seL4_CPtr *cptr, seL4_Word type, size_t size_bits)
{
    /* Allocate the object */
    ut_t *ut = ut_alloc(size_bits, cspace);
    if (ut == NULL) {
        ZF_LOGE("No memory for object of size %zu", size_bits);
        return NULL;
    }

    /* allocate a slot to retype the memory for object into */
    *cptr = cspace_alloc_slot(cspace);
    if (*cptr == seL4_CapNull) {
        ut_free(ut);
        ZF_LOGE("Failed to allocate slot");
        return NULL;
    }

    /* now do the retype */
    seL4_Error err = cspace_untyped_retype(cspace, ut->cap, *cptr, type, size_bits);
    ZF_LOGE_IFERR(err, "Failed retype untyped");
    if (err != seL4_NoError) {
        ut_free(ut);
        cspace_free_slot(cspace, *cptr);
        return NULL;
    }

    return ut;
}

static int process_stack_write(seL4_Word *mapped_stack, int index, uintptr_t val)
{
    mapped_stack[index] = val;
    return index - 1;
}

/* set up System V ABI compliant stack, so that the process can
 * start up and initialise the C library */
static seL4_Word process_init_stack(proc_t *proc, const char *app_name, cspace_t *cspace,
                                        seL4_CPtr vspace, elf_t *elf_file)
{
    /* virtual addresses in the target process' address space */
    uintptr_t stack_top = PROCESS_STACK_TOP;
    uintptr_t stack_bottom = PROCESS_STACK_TOP - PAGE_SIZE_4K;
    /* virtual addresses in the SOS's address space */
    void *local_stack_top  = (seL4_Word *) SOS_SCRATCH;
    uintptr_t local_stack_bottom = SOS_SCRATCH - PAGE_SIZE_4K;

    /* find the vsyscall table */
    struct nfsfh *fh;
    sos_nfs_open(&fh, app_name, 0);

    assert(elf_isElf64(elf_file) == 1);
    Elf64_Ehdr header = elf64_getHeader(elf_file);

    Elf64_Shdr section_headers[header.e_shentsize * header.e_shnum];
    sos_nfs_read(fh, section_headers, header.e_shoff, header.e_shentsize * header.e_shnum);

    Elf64_Shdr string_table = section_headers[header.e_shstrndx];
    Elf64_Off string_table_offset = string_table.sh_offset;
    char names[string_table.sh_size];
    sos_nfs_read(fh, names, string_table_offset, string_table.sh_size);

    Elf64_Off sysinfo_offset;
    for (int i = 0; i < header.e_shnum; i++) {
        if (strcmp(names + section_headers[i].sh_name, "__vsyscall") == 0) {
            sysinfo_offset = section_headers[i].sh_offset;
        }
    }
    uintptr_t sysinfo;
    sos_nfs_read(fh, &sysinfo, sysinfo_offset, sizeof(uintptr_t));

    sos_nfs_close(fh);

    /* create slot for the frame to load the data into */
    seL4_CPtr user_stack = cspace_alloc_slot(cspace);
    if (user_stack == seL4_CapNull) {
        ZF_LOGD("Failed to alloc slot");
        return 0;
    }

    seL4_Error err = addrspace_alloc_map_one_page(proc->addrspace, cspace,
                                    user_stack, proc->vspace, stack_bottom);
    if (err) {
        cspace_delete(cspace, user_stack);
        cspace_free_slot(cspace, user_stack);
        ZF_LOGE("Unable to map stack for user app");
        return 0;
    }

    /* allocate a slot to duplicate the stack frame cap so we can map it into our address space */
    seL4_CPtr local_stack = cspace_alloc_slot(cspace);
    if (local_stack == seL4_CapNull) {
        cspace_delete(cspace, user_stack);
        cspace_free_slot(cspace, user_stack);
        ZF_LOGE("Failed to alloc slot for stack");
        return 0;
    }

    /* copy the stack frame cap into the slot */
    err = cspace_copy(cspace, local_stack, cspace, user_stack, seL4_AllRights);
    if (err != seL4_NoError) {
        cspace_delete(cspace, user_stack);
        cspace_free_slot(cspace, user_stack);
        cspace_free_slot(cspace, local_stack);
        ZF_LOGE("Failed to copy cap");
        return 0;
    }

    /* map it into the sos address space */
    err = map_frame(cspace, local_stack, vspace, local_stack_bottom, seL4_AllRights,
                    seL4_ARM_Default_VMAttributes);
    if (err != seL4_NoError) {
        cspace_delete(cspace, user_stack);
        cspace_free_slot(cspace, user_stack);
        cspace_delete(cspace, local_stack);
        cspace_free_slot(cspace, local_stack);
        return 0;
    }

    int index = -2;

    /* null terminate the aux vectors */
    index = process_stack_write(local_stack_top, index, 0);
    index = process_stack_write(local_stack_top, index, 0);

    /* write the aux vectors */
    index = process_stack_write(local_stack_top, index, PAGE_SIZE_4K);
    index = process_stack_write(local_stack_top, index, AT_PAGESZ);

    index = process_stack_write(local_stack_top, index, sysinfo);
    index = process_stack_write(local_stack_top, index, AT_SYSINFO);

    /* null terminate the environment pointers */
    index = process_stack_write(local_stack_top, index, 0);


    /* we don't have any env pointers - skip */

    /* null terminate the argument pointers */
    index = process_stack_write(local_stack_top, index, 0);

    /* no argpointers - skip */

    /* set argc to 0 */
    process_stack_write(local_stack_top, index, 0);

    /* adjust the initial stack top */
    stack_top += (index * sizeof(seL4_Word));
    // printf("stack_top------>%p\n", stack_top);
    // printf("stack_bottom------>%p\n", stack_bottom);

    /* the stack *must* remain aligned to a double word boundary,
     * as GCC assumes this, and horrible bugs occur if this is wrong */
    assert(index % 2 == 0);
    assert(stack_top % (sizeof(seL4_Word) * 2) == 0);

    /* unmap our copy of the stack */
    err = seL4_ARM_Page_Unmap(local_stack);
    assert(err == seL4_NoError);

    /* delete the copy of the stack frame cap */
    err = cspace_delete(cspace, local_stack);
    assert(err == seL4_NoError);

    /* mark the slot as free */
    cspace_free_slot(cspace, local_stack);

    addrspace_define_region(proc->addrspace, PROCESS_STACK_BOTTOM, 
                            PROCESS_STACK_TOP-PROCESS_STACK_BOTTOM, 0b110);
    return stack_top;
}

int process_init(char *app_name, seL4_CPtr ep)
{
    cspace_t *cspace = global_cspace();

    bool failed = false;

    proc_t *proc = (proc_t *)kmalloc(sizeof(proc_t));
    proc->addrspace = addrspace_create();
    proc->fdt = fdt_init();
    proc->start_timestamp = get_time();

    if (pids == NULL) { pids = circular_id_init(0, 1, MAX_PROCESS); }

    int id = circular_id_alloc(pids, 1);
    //printf("                                                  pid = %d\n", id);
    proc->id = id;
    
    /* Create a VSpace */
    proc->vspace_ut = process_alloc_retype(proc, cspace, &proc->vspace, seL4_ARM_PageGlobalDirectoryObject,
                                              seL4_PGDBits);
    if (proc->vspace_ut == NULL) {
        return false;
    }

    /* assign the vspace to an asid pool */
    seL4_Error err = seL4_ARM_ASIDPool_Assign(seL4_CapInitThreadASIDPool, proc->vspace);
    if (err != seL4_NoError) {
        ZF_LOGE("Failed to assign asid pool");
        return false;
    }

    /* Create a simple 1 level CSpace */
    err = cspace_create_one_level(cspace, &proc->cspace);
    if (err != CSPACE_NOERROR) {
        ZF_LOGE("Failed to create cspace");
        return false;
    }

    // proc->ipc_buffer_ut = process_alloc_retype(proc, cspace, &proc->ipc_buffer, seL4_ARM_SmallPageObject,
    //                                               seL4_PageBits);
    // err = map_frame(cspace, proc->ipc_buffer, proc->vspace, PROCESS_IPC_BUFFER,
    //                         seL4_AllRights, seL4_ARM_Default_VMAttributes);
    /* Create an IPC buffer */
    proc->ipc_buffer = cspace_alloc_slot(cspace);
    if (proc->ipc_buffer == seL4_CapNull) {
        ZF_LOGE("Failed to alloc user ep slot");
        return false;
    }

    err = addrspace_alloc_map_one_page(proc->addrspace, cspace,
                                    proc->ipc_buffer, proc->vspace, PROCESS_IPC_BUFFER);
    if (err) {
        ZF_LOGE("Unable to map stack for user app");
        return false;
    }
    addrspace_define_region(proc->addrspace, PROCESS_IPC_BUFFER, 120 *sizeof(seL4_Word), 0b110);

    /* allocate a new slot in the target cspace which we will mint a badged endpoint cap into. */
    seL4_CPtr user_ep = cspace_alloc_slot(&proc->cspace);
    if (user_ep == seL4_CapNull) {
        ZF_LOGE("Failed to alloc user ep slot");
        return false;
    }

    /* now mutate the cap, thereby setting the badge */
    err = cspace_mint(&proc->cspace, user_ep, cspace, ep, seL4_AllRights, id + BADGE_BASE);
    if (err) {
        ZF_LOGE("Failed to mint user ep");
        return false;
    }
    proc->cspace_inited = 1;

    /* Create a new TCB object */
    proc->tcb_ut = process_alloc_retype(proc, cspace, &proc->tcb, seL4_TCBObject, seL4_TCBBits);
    if (proc->tcb_ut == NULL) {
        ZF_LOGE("Failed to alloc tcb ut");
        return false;
    }

    /* Configure the TCB */
    err = seL4_TCB_Configure(proc->tcb, user_ep,
                            proc->cspace.root_cnode, seL4_NilData,
                            proc->vspace, seL4_NilData, PROCESS_IPC_BUFFER,
                            proc->ipc_buffer);
    if (err != seL4_NoError) {
        ZF_LOGE("Unable to configure new TCB");
        return false;
    }

    /* Set the priority */
    err = seL4_TCB_SetPriority(proc->tcb, seL4_CapInitThreadTCB, 0);
    if (err != seL4_NoError) {
        ZF_LOGE("Unable to set priority of new TCB");
        return false;
    }

    /* Provide a name for the thread -- Helpful for debugging */
    NAME_THREAD(proc->tcb, app_name);
    strncpy(proc->app_name, app_name, N_NAME);

    /* parse the cpio image */
    ZF_LOGI("\nStarting \"%s\"...\n", app_name);

    elf_t elf_file = {};
    char elf_base[PAGE_SIZE_4K];
    unsigned long elf_size;

    struct nfsfh *fh;
    sos_nfs_open(&fh, app_name, 0);
    sos_nfs_read(fh, elf_base, 0, PAGE_SIZE_4K);

    sos_stat_t stat_buf;
    sos_nfs_stat(app_name, &stat_buf);
    elf_size = stat_buf.st_size;

    sos_nfs_close(fh);

    /* Ensure that the file is an elf file. */
    if (elf_newFile(elf_base, elf_size, &elf_file)) {
        ZF_LOGE("Invalid elf file");
        return -1;
    }

    /* set up the stack */
    seL4_Word sp = process_init_stack(proc, app_name, cspace, global_vspace(), &elf_file);
    if (sp == 0) {
        ZF_LOGE("stack init failed");
        return false;
    }

    /* define heap. */
    addrspace_define_region(proc->addrspace, PROCESS_HEAP_BASE, PROCESS_HEAP_TOP - PROCESS_HEAP_BASE, 0b110);

    err = elf_load(proc->addrspace, cspace, proc->vspace, &elf_file, app_name);
    if (err) {
        ZF_LOGE("Failed to load elf image");
        return false;
    }
      
    /* Start the new process */
    seL4_UserContext context = {
        .pc = elf_getEntryPoint(&elf_file),
        .sp = sp,
    };
    printf("Starting %s at %p\n", app_name, (void *) context.pc);
    processes[id] = proc;
    err = seL4_TCB_WriteRegisters(proc->tcb, 1, 0, 2, &context);
    ZF_LOGE_IF(err, "Failed to write registers");

    return id;
}

void process_delete(proc_t *proc)
{
    if (proc->id == 0) { return; }

    if (proc->tcb_ut != NULL) {
        ut_free(proc->tcb_ut);
    }

    if (proc->tcb != seL4_CapNull) {
        cspace_delete(global_cspace(), proc->tcb);
        cspace_free_slot(global_cspace(), proc->tcb);
    }

    if (proc->vspace_ut != NULL) {
        ut_free(proc->vspace_ut);
    }

    if (proc->vspace != seL4_CapNull) {
        cspace_delete(global_cspace(), proc->vspace);
        cspace_free_slot(global_cspace(), proc->vspace);
    }

    if (proc->cspace_inited) {
        cspace_destroy(&proc->cspace);
    }
    addrspace_destory(proc->addrspace);
    fdt_destroy(proc->fdt);
    processes[proc->id] = NULL;
    circular_id_free(pids, proc->id, 1);
    kfree(proc);
}

int process_exists_by_badge(seL4_Word badge)
{
    return (int)badge >= BADGE_BASE && processes[badge-BADGE_BASE] != NULL;
}

void process_set_exiting(int pid)
{
    processes[pid]->exiting = 1;
}

int process_exists_by_id(int pid)
{
    return pid >= 0 && pid < MAX_PROCESS && processes[pid] != NULL;
}

int process_id_exits(int pid)
{
    return processes[pid]->exiting;
}

int process_exiting(proc_t *proc)
{
    return proc->exiting;
}

proc_t *process_get_by_badge(seL4_Word badge)
{
    return processes[badge-BADGE_BASE];
}

proc_t *process_get_by_id(int pid)
{
    return processes[pid];
}

int process_max()
{
    return MAX_PROCESS;
}

seL4_CPtr process_get_reply_cap(proc_t *proc)
{
    return proc->reply;
}

void process_set_reply_cap(proc_t *proc, seL4_CPtr reply)
{
    proc->reply = reply;
}

void process_reply(proc_t *proc, unsigned int msg_len)
{
    seL4_MessageInfo_t reply_msg = seL4_MessageInfo_new(0, 0, 0, msg_len);
    seL4_Send(proc->reply, reply_msg);
    cspace_delete(global_cspace(), proc->reply);
    cspace_free_slot(global_cspace(), proc->reply);
    if (process_exiting(proc)) { process_delete(proc); }
}

char *process_name(proc_t *proc)
{
    return proc->app_name;
}

seL4_CPtr process_tcb(proc_t *proc)
{
    return proc->tcb;
}

addrspace_t *process_addrspace(proc_t *proc)
{
    return proc->addrspace;
}

fd_table_t *process_fdt(proc_t *proc)
{
    return proc->fdt;
}

seL4_CPtr process_vspace(proc_t *proc)
{
    return proc->vspace;
}

int process_id(proc_t *proc)
{
    return proc->id;
}

unsigned process_time(proc_t *proc)
{
    return get_time() - proc->start_timestamp;
}

unsigned process_size(proc_t *proc)
{
    assert(proc->addrspace != NULL);
    return proc->addrspace->pages;
}

seL4_Word process_get_data0(proc_t *proc)
{
    return proc->data[0];
}

seL4_Word process_get_data1(proc_t *proc)
{
    return proc->data[1];
}

seL4_Word process_get_data2(proc_t *proc)
{
    return proc->data[2];
}

seL4_Word process_get_data3(proc_t *proc)
{
    return proc->data[3];
}

void process_set_data0(proc_t *proc, seL4_Word data)
{
    proc->data[0] = data;
}

void process_set_data1(proc_t *proc, seL4_Word data)
{
    proc->data[1] = data;
}

void process_set_data2(proc_t *proc, seL4_Word data)
{
    proc->data[2] = data;
}

void process_set_data3(proc_t *proc, seL4_Word data)
{
    proc->data[3] = data;
}
