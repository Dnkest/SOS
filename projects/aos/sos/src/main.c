/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#include <autoconf.h>
#include <utils/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <cspace/cspace.h>
#include <picoro/picoro.h>
#include <aos/sel4_zf_logif.h>
#include <aos/debug.h>
#include "picoro/picoro.h"

#include <clock/clock.h>
#include <cpio/cpio.h>
#include <elf/elf.h>

#include "bootstrap.h"
#include "irq.h"
#include "network.h"
#include "frame_table.h"
#include "drivers/uart.h"
#include "ut.h"
#include "vmem_layout.h"
#include "mapping.h"
#include "elfload.h"
#include "syscalls.h"
#include "tests.h"
#include "syscall.h"
#include "addrspace.h"
#include "utils/kmalloc.h"
#include "paging.h"
#include "globals.h"
#include "proc.h"
#include "utils/eventq.h"
#include "utils/vmq.h"

#include <aos/vsyscall.h>

/*
 * To differentiate between signals from notification objects and and IPC messages,
 * we assign a badge to the notification object. The badge that we receive will
 * be the bitwise 'OR' of the notification object badge and the badges
 * of all pending IPC messages.
 *
 * All badged IRQs set high bet, then we use uniqe bits to
 * distinguish interrupt sources.
 */
#define IRQ_EP_BADGE         BIT(seL4_BadgeBits - 1ul)
#define IRQ_IDENT_BADGE_BITS MASK(seL4_BadgeBits - 1ul)

/* The linker will link this symbol to the start address  *
 * of an archive of attached applications.                */
extern char __eh_frame_start[];
/* provided by gcc */
extern void (__register_frame)(void *);

/* root tasks cspace */
static cspace_t cspace;

/* helper to allocate a ut + cslot, and retype the ut into the cslot */
static ut_t *alloc_retype(seL4_CPtr *cptr, seL4_Word type, size_t size_bits)
{
    /* Allocate the object */
    ut_t *ut = ut_alloc(size_bits, &cspace);
    if (ut == NULL) {
        ZF_LOGE("No memory for object of size %zu", size_bits);
        return NULL;
    }

    /* allocate a slot to retype the memory for object into */
    *cptr = cspace_alloc_slot(&cspace);
    if (*cptr == seL4_CapNull) {
        ut_free(ut);
        ZF_LOGE("Failed to allocate slot");
        return NULL;
    }

    /* now do the retype */
    seL4_Error err = cspace_untyped_retype(&cspace, ut->cap, *cptr, type, size_bits);
    ZF_LOGE_IFERR(err, "Failed retype untyped");
    if (err != seL4_NoError) {
        ut_free(ut);
        cspace_free_slot(&cspace, *cptr);
        return NULL;
    }

    return ut;
}

NORETURN void syscall_loop(seL4_CPtr ep)
{
    while (1) {
        seL4_Word badge = 0;
        /* Block on ep, waiting for an IPC sent over ep, or
         * a notification from our bound notification object */
        seL4_MessageInfo_t message = seL4_Recv(ep, &badge);
        /* Awake! We got a message - check the label and badge to
         * see what the message is about */
        seL4_Word label = seL4_MessageInfo_get_label(message);
        //printf("badge %u\n", badge);

        proc_t *proc = NULL;
        if (process_exists_by_badge(badge)) {
            proc = process_get_by_badge(badge);
            //printf("proc %p\n", proc);
            seL4_CPtr reply = cspace_alloc_slot(global_cspace());
            seL4_Error err = cspace_save_reply_cap(global_cspace(), reply);
            ZF_LOGF_IFERR(err, "Failed to save reply");
            process_set_reply_cap(proc, reply);
        }

        if (badge & IRQ_EP_BADGE) {
            //printf("(1) irq badge %u\n", badge);

            /* It's a notification from our bound notification
             * object! */
            sos_handle_irq_notification(&badge);

        } else if (proc != NULL && !process_exiting(proc) && label == seL4_Fault_NullFault) {
            //printf("proc %d syscall %u\n", process_id(proc), seL4_GetMR(0));

            /* It's not a fault or an interrupt, it must be an IPC
             * message from app! */
            process_set_data0(proc, seL4_GetMR(0));
            process_set_data1(proc, seL4_GetMR(1));
            process_set_data2(proc, seL4_GetMR(2));
            process_set_data3(proc, seL4_GetMR(3));
            eventQ_produce(sos_handle_syscall, (void *)proc);

        } else if (proc != NULL && label == seL4_Fault_VMFault) {
            printf("proc %d vm fault at %p\n", process_id(proc), seL4_GetMR(seL4_VMFault_Addr));

            process_set_data0(proc, seL4_GetMR(seL4_VMFault_Addr));
            vmQ_produce(sos_handle_vm_fault, (void *)proc);

        } else {

            /* some kind of fault */
            debug_print_fault(message, process_name(proc));
            /* dump registers too */
            debug_dump_registers(process_tcb(proc));

            ZF_LOGF("The SOS skeleton does not know how to handle faults!");
        }

        //printf("consuming\n");
        if(!vmQ_empty()) {
            vmQ_consume();
        } else {
            eventQ_consume();
        }
    }
}

/* Allocate an endpoint and a notification object for sos.
 * Note that these objects will never be freed, so we do not
 * track the allocated ut objects anywhere
 */
static void sos_ipc_init(seL4_CPtr *ipc_ep, seL4_CPtr *ntfn)
{
    /* Create an notification object for interrupts */
    ut_t *ut = alloc_retype(ntfn, seL4_NotificationObject, seL4_NotificationBits);
    ZF_LOGF_IF(!ut, "No memory for notification object");

    /* Bind the notification object to our TCB */
    seL4_Error err = seL4_TCB_BindNotification(seL4_CapInitThreadTCB, *ntfn);
    ZF_LOGF_IFERR(err, "Failed to bind notification object to TCB");

    /* Create an endpoint for user application IPC */
    ut = alloc_retype(ipc_ep, seL4_EndpointObject, seL4_EndpointBits);
    ZF_LOGF_IF(!ut, "No memory for endpoint");
}

/* called by crt */
seL4_CPtr get_seL4_CapInitThreadTCB(void)
{
    return seL4_CapInitThreadTCB;
}

/* tell muslc about our "syscalls", which will bve called by muslc on invocations to the c library */
void init_muslc(void)
{
    muslcsys_install_syscall(__NR_set_tid_address, sys_set_tid_address);
    muslcsys_install_syscall(__NR_writev, sys_writev);
    muslcsys_install_syscall(__NR_exit, sys_exit);
    muslcsys_install_syscall(__NR_rt_sigprocmask, sys_rt_sigprocmask);
    muslcsys_install_syscall(__NR_gettid, sys_gettid);
    muslcsys_install_syscall(__NR_getpid, sys_getpid);
    muslcsys_install_syscall(__NR_tgkill, sys_tgkill);
    muslcsys_install_syscall(__NR_tkill, sys_tkill);
    muslcsys_install_syscall(__NR_exit_group, sys_exit_group);
    muslcsys_install_syscall(__NR_ioctl, sys_ioctl);
    muslcsys_install_syscall(__NR_mmap, sys_mmap);
    muslcsys_install_syscall(__NR_brk,  sys_brk);
    muslcsys_install_syscall(__NR_clock_gettime, sys_clock_gettime);
    muslcsys_install_syscall(__NR_nanosleep, sys_nanosleep);
    muslcsys_install_syscall(__NR_getuid, sys_getuid);
    muslcsys_install_syscall(__NR_getgid, sys_getgid);
    muslcsys_install_syscall(__NR_openat, sys_openat);
    muslcsys_install_syscall(__NR_close, sys_close);
    muslcsys_install_syscall(__NR_socket, sys_socket);
    muslcsys_install_syscall(__NR_bind, sys_bind);
    muslcsys_install_syscall(__NR_listen, sys_listen);
    muslcsys_install_syscall(__NR_connect, sys_connect);
    muslcsys_install_syscall(__NR_accept, sys_accept);
    muslcsys_install_syscall(__NR_sendto, sys_sendto);
    muslcsys_install_syscall(__NR_recvfrom, sys_recvfrom);
    muslcsys_install_syscall(__NR_readv, sys_readv);
    muslcsys_install_syscall(__NR_getsockname, sys_getsockname);
    muslcsys_install_syscall(__NR_getpeername, sys_getpeername);
    muslcsys_install_syscall(__NR_fcntl, sys_fcntl);
    muslcsys_install_syscall(__NR_setsockopt, sys_setsockopt);
    muslcsys_install_syscall(__NR_getsockopt, sys_getsockopt);
    muslcsys_install_syscall(__NR_ppoll, sys_ppoll);
    muslcsys_install_syscall(__NR_madvise, sys_madvise);
}

void *start_proc_init(void *p)
{
    while (!paging_ready()) { yield(0); }
    printf("Start first process\n");
    process_init("sosh", (seL4_CPtr)p);
}

void timer_callback(uint32_t id, void *data)
{
    printf("{1}got timer %u at %u\n", id, get_time()/1000);
    register_timer(100000, timer_callback, NULL);
}

void timer_callback2(uint32_t id, void *data)
{
    printf("{2}got timer %u at %u\n", id, get_time()/1000);
    register_timer(50000, timer_callback2, NULL);
}

NORETURN void *main_continued(UNUSED void *arg)
{
    /* Initialise other system compenents here */
    seL4_CPtr ipc_ep, ntfn;
    sos_ipc_init(&ipc_ep, &ntfn);
    sos_init_irq_dispatch(
        &cspace,
        seL4_CapIRQControl,
        ntfn,
        IRQ_EP_BADGE,
        IRQ_IDENT_BADGE_BITS
    );

    set_ipc_ep(ipc_ep);

    /* run sos initialisation tests */
    run_tests(&cspace);

    /* Map the timer device (NOTE: this is the same mapping you will use for your timer driver -
     * sos uses the watchdog timers on this page to implement reset infrastructure & network ticks,
     * so touching the watchdog timers here is not recommended!) */
    void *timer_vaddr = sos_map_device(&cspace, PAGE_ALIGN_4K(TIMER_MAP_BASE), PAGE_SIZE_4K);

    /* Initialise the network hardware. */
    printf("Network init\n");
    network_init(&cspace, timer_vaddr);

    /* Initialises the timer */
    printf("Timer init\n");
    start_timer(timer_vaddr);
    /* You will need to register an IRQ handler for the timer here.
     * See "irq.h". */
    sos_register_irq_handler(meson_timeout_irq(MESON_TIMER_A), true, timer_irq, NULL, NULL);

    //register_timer(1000000, timer_callback, NULL);
    //register_timer(1000000, timer_callback2, NULL);
    syscall_handlers_init();

    kmalloc_tests();
    //id_alloc_tests();

    vmQ_init();

    eventQ_init();
    eventQ_produce(paging_init, NULL);
    eventQ_produce(start_proc_init, (void *)ipc_ep);

    printf("\nSOS entering syscall loop\n");
    syscall_loop(ipc_ep);
}
/*
 * Main entry point - called by crt.
 */
int main(void)
{
    init_muslc();

    /* bootinfo was set as an environment variable in _sel4_start */
    char *bi_string = getenv("bootinfo");
    ZF_LOGF_IF(!bi_string, "Could not parse bootinfo from env.");

    /* register the location of the unwind_tables -- this is required for
     * backtrace() to work */
    __register_frame(&__eh_frame_start);

    seL4_BootInfo *boot_info;
    if (sscanf(bi_string, "%p", &boot_info) != 1) {
        ZF_LOGF("bootinfo environment value '%s' was not valid.", bi_string);
    }

    debug_print_bootinfo(boot_info);

    printf("\nSOS Starting...\n");

    NAME_THREAD(seL4_CapInitThreadTCB, "SOS:root");

    /* Initialise the cspace manager, ut manager and dma */
    sos_bootstrap(&cspace, boot_info);

    /* switch to the real uart to output (rather than seL4_DebugPutChar, which only works if the
     * kernel is built with support for printing, and is much slower, as each character print
     * goes via the kernel)
     *
     * NOTE we share this uart with the kernel when the kernel is in debug mode. */
    uart_init(&cspace);
    update_vputchar(uart_putchar);

    /* test print */
    printf("SOS Started!\n");
    set_global_cspace(&cspace);
    set_global_vspace(seL4_CapInitThreadVSpace);

    frame_table_init(&cspace, seL4_CapInitThreadVSpace);

    /* allocate a bigger stack and switch to it -- we'll also have a guard page, which makes it much
     * easier to detect stack overruns */
    seL4_Word vaddr = SOS_STACK;
    for (int i = 0; i < SOS_STACK_PAGES; i++) {
        seL4_CPtr frame_cap;
        ut_t *frame = alloc_retype(&frame_cap, seL4_ARM_SmallPageObject, seL4_PageBits);
        ZF_LOGF_IF(frame == NULL, "Failed to allocate stack page");
        seL4_Error err = map_frame(&cspace, frame_cap, seL4_CapInitThreadVSpace,
                                   vaddr, seL4_AllRights, seL4_ARM_Default_VMAttributes);
        ZF_LOGF_IFERR(err, "Failed to map stack");

        vaddr += PAGE_SIZE_4K;
    }
    utils_run_on_stack((void *) vaddr, main_continued, NULL);

    UNREACHABLE();
}
