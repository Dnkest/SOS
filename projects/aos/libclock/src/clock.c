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
#include <stdlib.h>
#include <stdint.h>
#include <clock/clock.h>
#include <adt/circular_id.h>
#include <adt/priority_queue.h>

/* The functions in src/device.h should help you interact with the timer
 * to set registers and configure timeouts. */
#include "device.h"

static struct {
    volatile meson_timer_reg_t *regs;
    /* Add fields as you see necessary */
    
} clock;

struct timer {
    uint32_t id;
    uint64_t delay;
    timer_callback_t callback;
    void *data;
};
static int cmp(void *a, void *b)
{
    return ((struct timer *)a)->delay > ((struct timer *)b)->delay;
}
static PQ pq = NULL;

static ccircular_id_t *ids = NULL;

int start_timer(unsigned char *timer_vaddr)
{
    int err = stop_timer();
    if (err != 0) {
        return err;
    }

    clock.regs = (meson_timer_reg_t *)(timer_vaddr + TIMER_REG_START);
    configure_timeout(clock.regs, MESON_TIMER_A, true, false, TIMEOUT_TIMEBASE_1_MS, 0);
    pq = pq_create(cmp);
    ids = ccircular_id_init(1, 1, 20);
    return CLOCK_R_OK;
}

timestamp_t get_time(void)
{
    return read_timestamp(clock.regs);
}

uint64_t get_time_ms(void)
{
    return get_time()/1000;
}

uint32_t register_timer(uint64_t delay, timer_callback_t callback, void *data)
{
    struct timer *t = (struct timer *)malloc(sizeof(struct timer));
    if (t == NULL) { return 0; }
    uint32_t id = (uint32_t)ccircular_id_alloc(ids, 1);
    t->id = id;
    t->delay = delay + get_time();
    t->callback = callback;
    t->data = data;
    pq_push(pq, (void *)t);

    t = (struct timer *)pq_front(pq);
    write_timeout(clock.regs, MESON_TIMER_A, (t->delay - get_time())/1000 & 0xffff);
    return id;
}

static int rm_cmp(void *a, void *b)
{
    return (uint64_t)a == ((struct timer *)b)->id;
}
int remove_timer(uint32_t id)
{
    struct timer *t = pq_remove(pq, rm_cmp, (void *)id);
    if (t != NULL) {
        free(t);
        return CLOCK_R_OK;
    }
    return CLOCK_R_FAIL;
}
int debug(void *a)
{
    printf("id:%u delay:%u\n", ((struct timer *)a)->id, ((struct timer *)a)->delay);
}

int timer_irq(
    void *data,
    seL4_Word irq,
    seL4_IRQHandler irq_handler
)
{
    /* Handle the IRQ */
    uint64_t diff, now = get_time();
    
    struct timer *t;
    while (pq != NULL && !pq_empty(pq)) {
        t = (struct timer *)pq_front(pq);
        if (now >= t->delay) {
            t->callback(t->id, t->data);
            pq_pop(pq);
            ccircular_id_free(ids, t->id, 1);
            free(t);
        } else {
            diff = t->delay - now;
            write_timeout(clock.regs, MESON_TIMER_A, (diff/1000) & 0xffff);
            break;
        }
    }

    /* Acknowledge that the IRQ has been handled */
    seL4_IRQHandler_Ack(irq_handler);
    return CLOCK_R_OK;
}

int stop_timer(void)
{
    /* Stop the timer from producing further interrupts and remove all
     * existing timeouts */
    if (pq != NULL && ids != NULL) {
        pq_destroy(pq);
        pq = NULL;
        ccircular_id_destroy(ids);
        ids = NULL;
        configure_timeout(clock.regs, MESON_TIMER_A, false, false, TIMEOUT_TIMEBASE_1_MS, 0);
    }
    return CLOCK_R_OK;
}
