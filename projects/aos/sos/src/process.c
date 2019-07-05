#include <stdbool.h>
#include <sel4/sel4.h>
#include <cpio/cpio.h>
#include <elf/elf.h>
#include <aos/sel4_zf_logif.h>
#include <aos/debug.h>
#include <adt/id.h>

#include "process.h"
#include "frame_table.h"
#include "vmem_layout.h"

#define CAPACITY 4096 - sizeof(unsigned int)

#define TTY_PRIORITY         (0)
#define TTY_EP_BADGE         (101)

static process_t *cur_proc;

static id_table_t *ids;
static seL4_Word base = SOS_MAP;

seL4_Word process_init_stack(process_t *proc, cspace_t *cspace, addrspace_t *global_addrspace,
                                    seL4_CPtr global_vspace, elf_t *elf_file);

/* helper to allocate a ut + cslot, and retype the ut into the cslot */
static ut_t *process_alloc_retype(process_t *proc, seL4_CPtr *cptr, seL4_Word type, size_t size_bits)
{
    /* Allocate the object */
    ut_t *ut = ut_alloc(size_bits, proc->global_cspace);
    if (ut == NULL) {
        ZF_LOGE("No memory for object of size %zu", size_bits);
        return NULL;
    }

    /* allocate a slot to retype the memory for object into */
    *cptr = cspace_alloc_slot(proc->global_cspace);
    if (*cptr == seL4_CapNull) {
        ut_free(ut);
        ZF_LOGE("Failed to allocate slot");
        return NULL;
    }

    /* now do the retype */
    seL4_Error err = cspace_untyped_retype(proc->global_cspace, ut->cap, *cptr, type, size_bits);
    ZF_LOGE_IFERR(err, "Failed retype untyped");
    if (err != seL4_NoError) {
        ut_free(ut);
        cspace_free_slot(proc->global_cspace, *cptr);
        return NULL;
    }

    return ut;
}

process_t *process_create(cspace_t *cspace, char *cpio_archive, char *cpio_archive_end)
{
    ids = id_table_init(0);
    id_tests(ids);
    process_t *proc = (process_t *)alloc_one_page();
    proc->global_cspace = cspace;
    proc->addrspace = addrspace_create();
    proc->cpio_archive = cpio_archive;
    proc->cpio_archive_end = cpio_archive_end;
    return proc;
}

/* Start the first process, and return true if successful
 *
 * This function will leak memory if the process does not start successfully.
 * TODO: avoid leaking memory once you implement real processes, otherwise a user
 *       can force your OS to run out of memory by creating lots of failed processes.
 */
void process_start(process_t *proc, char *app_name, addrspace_t *global_addrspace, seL4_CPtr ep)
{
    /* Create a VSpace */
    proc->vspace_ut = process_alloc_retype(proc, &proc->vspace, seL4_ARM_PageGlobalDirectoryObject,
                                              seL4_PGDBits);
    if (proc->vspace_ut == NULL) {
        return false;
    }

    /* assign the vspace to an asid pool */
    seL4_Word err = seL4_ARM_ASIDPool_Assign(seL4_CapInitThreadASIDPool, proc->vspace);
    if (err != seL4_NoError) {
        ZF_LOGE("Failed to assign asid pool");
        return false;
    }

    /* Create a simple 1 level CSpace */
    err = cspace_create_one_level(proc->global_cspace, &proc->cspace);
    if (err != CSPACE_NOERROR) {
        ZF_LOGE("Failed to create cspace");
        return false;
    }


    /* Create an IPC buffer */
    proc->ipc_buffer = cspace_alloc_slot(proc->global_cspace);
    if (proc->ipc_buffer == seL4_CapNull) {
        ZF_LOGE("Failed to alloc user ep slot");
        return false;
    }
    err = addrspace_alloc_map_one_page(proc->addrspace, proc->global_cspace,
                                    proc->ipc_buffer, proc->vspace, PROCESS_IPC_BUFFER);
    if (err) {
        ZF_LOGE("Unable to map stack for user app");
        return 0;
    }

    /* allocate a new slot in the target cspace which we will mint a badged endpoint cap into --
     * the badge is used to identify the process, which will come in handy when you have multiple
     * processes. */
    seL4_CPtr user_ep = cspace_alloc_slot(&proc->cspace);
    if (user_ep == seL4_CapNull) {
        ZF_LOGE("Failed to alloc user ep slot");
        return false;
    }

    /* now mutate the cap, thereby setting the badge */
    err = cspace_mint(&proc->cspace, user_ep, proc->global_cspace, ep, seL4_AllRights, TTY_EP_BADGE);
    if (err) {
        ZF_LOGE("Failed to mint user ep");
        return false;
    }

    /* Create a new TCB object */
    proc->tcb_ut = process_alloc_retype(proc, &proc->tcb, seL4_TCBObject, seL4_TCBBits);
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
    err = seL4_TCB_SetPriority(proc->tcb, seL4_CapInitThreadTCB, TTY_PRIORITY);
    if (err != seL4_NoError) {
        ZF_LOGE("Unable to set priority of new TCB");
        return false;
    }

    /* Provide a name for the thread -- Helpful for debugging */
    NAME_THREAD(proc->tcb, app_name);

    /* parse the cpio image */
    ZF_LOGI("\nStarting \"%s\"...\n", app_name);

    elf_t elf_file = {};
    unsigned long elf_size;
    size_t cpio_len = proc->cpio_archive_end - proc->cpio_archive;
    char *elf_base = cpio_get_file(proc->cpio_archive, cpio_len, app_name, &elf_size);
    if (elf_base == NULL) {
        ZF_LOGE("Unable to locate cpio header for %s", app_name);
        return false;
    }

    /* Ensure that the file is an elf file. */
    if (elf_newFile(elf_base, elf_size, &elf_file)) {
        ZF_LOGE("Invalid elf file");
        return -1;
    }

    /* set up the stack */
    seL4_Word sp = process_init_stack(proc, proc->global_cspace, global_addrspace,
                                    seL4_CapInitThreadVSpace, &elf_file);

    /* load the elf image from the cpio file */
    err = elf_load(proc->addrspace, proc->global_cspace, proc->vspace, &elf_file);
    if (err) {
        ZF_LOGE("Failed to load elf image");
        return false;
    }

    /* Start the new process */
    seL4_UserContext context = {
        .pc = elf_getEntryPoint(&elf_file),
        .sp = sp,
    };
    printf("Starting ttytest at %p\n", (void *) context.pc);
    err = seL4_TCB_WriteRegisters(proc->tcb, 1, 0, 2, &context);
    ZF_LOGE_IF(err, "Failed to write registers");
}

int process_stack_write(seL4_Word *mapped_stack, int index, uintptr_t val)
{
    mapped_stack[index] = val;
    return index - 1;
}

/* set up System V ABI compliant stack, so that the process can
 * start up and initialise the C library */
seL4_Word process_init_stack(process_t *proc, cspace_t *cspace, addrspace_t *global_addrspace,
                                    seL4_CPtr global_vspace, elf_t *elf_file)
{
    /* virtual addresses in the target process' address space */
    uintptr_t stack_top = PROCESS_STACK_TOP;
    uintptr_t stack_bottom = PROCESS_STACK_TOP - PAGE_SIZE_4K;
    /* virtual addresses in the SOS's address space */
    void *local_stack_top  = (seL4_Word *) SOS_SCRATCH;
    uintptr_t local_stack_bottom = SOS_SCRATCH - PAGE_SIZE_4K;

    addrspace_t *addrspace = proc->addrspace;

    /* find the vsyscall table */
    uintptr_t sysinfo = *((uintptr_t *) elf_getSectionNamed(elf_file, "__vsyscall", NULL));
    if (sysinfo == 0) {
        ZF_LOGE("could not find syscall table for c library");
        return 0;
    }

    /* create slot for the frame to load the data into */
    seL4_CPtr user_stack = cspace_alloc_slot(cspace);
    if (user_stack == seL4_CapNull) {
        ZF_LOGD("Failed to alloc slot");
        return -1;
    }
    proc->stack = user_stack;

    seL4_Error err = addrspace_alloc_map_one_page(addrspace, cspace,
                                    proc->stack, proc->vspace, stack_bottom);
    if (err) {
        ZF_LOGE("Unable to map stack for user app");
        return 0;
    }

    /* allocate a slot to duplicate the stack frame cap so we can map it into our address space */
    seL4_CPtr local_stack = cspace_alloc_slot(cspace);
    if (local_stack == seL4_CapNull) {
        ZF_LOGE("Failed to alloc slot for stack");
        return 0;
    }

    err = addrspace_map_one_page(global_addrspace, cspace, local_stack, global_vspace,
                                    local_stack_bottom, cspace, user_stack);
    if (err) {
        ZF_LOGE("Unable to map stack for sos");
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
    err = cspace_delete(proc->global_cspace, local_stack);
    assert(err == seL4_NoError);

    /* mark the slot as free */
    cspace_free_slot(proc->global_cspace, local_stack);

    /* Exend the stack with extra pages */
    for (int page = 0; page < 4; page++) {
        stack_bottom -= PAGE_SIZE_4K;

        seL4_CPtr frame_cap = cspace_alloc_slot(cspace);
        if (frame_cap == seL4_CapNull) {
            ZF_LOGD("Failed to alloc slot");
            return -1;
        }

        err = addrspace_alloc_map_one_page(addrspace, cspace,
                                    frame_cap, proc->vspace, stack_bottom);
        if (err != 0) {
            ZF_LOGE("Unable to map extra stack frame for user app");
            return 0;
        }
    }
    addrspace_define_region(proc->addrspace, PROCESS_STACK_BOTTOM, 
                            PROCESS_STACK_TOP-PROCESS_STACK_BOTTOM, 0b110);
    addrspace_define_region(proc->addrspace, PROCESS_HEAP_BASE, 
                            PROCESS_HEAP_TOP-PROCESS_HEAP_BASE, 0b110);

    return stack_top;
}

seL4_Word process_write(process_t *proc, addrspace_t *global_addrspace, seL4_CPtr vspace,
                                seL4_Word user_vaddr, seL4_Word size)
{
    seL4_Word user_base_vaddr = user_vaddr & 0xFFFFFFFFF000;
    unsigned int num_pages = (user_vaddr - user_base_vaddr + size) / PAGE_SIZE_4K;
    if ((user_vaddr - user_base_vaddr + size) % PAGE_SIZE_4K) {
        num_pages++;
    }

    seL4_CPtr frame_caps[num_pages];
    unsigned int index = 0;

    unsigned int addr_ids[num_pages];
    unsigned int start = id_find_n(ids, num_pages);
    for (int i = 0; i < num_pages; i++) {
        addr_ids[i] = id_next_start_at(ids, start++);
    }

    seL4_Word kernel_base_vaddr = base + addr_ids[0] * PAGE_SIZE_4K;

    seL4_Word kernel_vaddr_tmp, user_vaddr_tmp = user_base_vaddr;

    for (int i = 0; i < num_pages; i++) {
        kernel_vaddr_tmp = base + addr_ids[i] * PAGE_SIZE_4K;

        seL4_CPtr user_frame_cap = frame_page(addrspace_lookup(proc->addrspace, user_vaddr_tmp));

        seL4_CPtr kernel_frame_cap = cspace_alloc_slot(proc->global_cspace);
        if (kernel_frame_cap == seL4_CapNull) {
            ZF_LOGE("Failed to alloc slot for stack");
            return 0;
        }
        seL4_Error err = addrspace_map_one_page(global_addrspace, proc->global_cspace,
                        kernel_frame_cap, vspace, kernel_vaddr_tmp, frame_table_cspace(), user_frame_cap);
        if (err) {
            printf("err %d\n", err);
        }
        frame_caps[index++] = kernel_frame_cap;
        user_vaddr_tmp += PAGE_SIZE_4K;
    }

    int actual = vfs_write(seL4_GetMR(3), kernel_base_vaddr + (user_vaddr - user_base_vaddr), size);
    memset(kernel_base_vaddr, 0, size);
    for (unsigned int i = 0; i < num_pages; i++) {
        seL4_ARM_Page_Unmap(frame_caps[i]);
        cspace_delete(proc->global_cspace, frame_caps[i]);
        cspace_free_slot(proc->global_cspace, frame_caps[i]);
        id_free(ids, addr_ids[i]);
    }
    return actual;
}

seL4_Word process_read(process_t *proc, addrspace_t *global_addrspace, seL4_CPtr vspace,
                                seL4_Word user_vaddr, seL4_Word size)
{
    seL4_Word user_base_vaddr = user_vaddr & 0xFFFFFFFFF000;
    unsigned int num_pages = (user_vaddr - user_base_vaddr + size) / PAGE_SIZE_4K;
    if ((user_vaddr - user_base_vaddr + size) % PAGE_SIZE_4K) {
        num_pages++;
    }

    seL4_CPtr frame_caps[num_pages];
    unsigned int index = 0;

    unsigned int addr_ids[num_pages];
    unsigned int start = id_find_n(ids, num_pages);
    for (int i = 0; i < num_pages; i++) {
        addr_ids[i] = id_next_start_at(ids, start++);
    }

    seL4_Word kernel_base_vaddr = base + addr_ids[0] * PAGE_SIZE_4K;

    seL4_Word kernel_vaddr_tmp, user_vaddr_tmp = user_base_vaddr;

    for (int i = 0; i < num_pages; i++) {
        kernel_vaddr_tmp = base + addr_ids[i] * PAGE_SIZE_4K;

        seL4_CPtr user_frame_cap = frame_page(addrspace_lookup(proc->addrspace, user_vaddr_tmp));

        seL4_CPtr kernel_frame_cap = cspace_alloc_slot(proc->global_cspace);
        if (kernel_frame_cap == seL4_CapNull) {
            ZF_LOGE("Failed to alloc slot for stack");
            return 0;
        }
        seL4_Error err = addrspace_map_one_page(global_addrspace, proc->global_cspace,
                        kernel_frame_cap, vspace, kernel_vaddr_tmp, frame_table_cspace(), user_frame_cap);
        if (err) {
            printf("err %d\n", err);
        }
        frame_caps[index++] = kernel_frame_cap;
        user_vaddr_tmp += PAGE_SIZE_4K;
    }

    int actual = vfs_read(seL4_GetMR(3), kernel_base_vaddr + (user_vaddr - user_base_vaddr), size);

    for (unsigned int i = 0; i < num_pages; i++) {
        seL4_ARM_Page_Unmap(frame_caps[i]);
        cspace_delete(proc->global_cspace, frame_caps[i]);
        cspace_free_slot(proc->global_cspace, frame_caps[i]);
        id_free(ids, addr_ids[i]);
    }
    return actual;
}

void proc_set(process_t *proc)
{
    cur_proc = proc;
}

process_t *proc_get()
{
    return cur_proc;
}
