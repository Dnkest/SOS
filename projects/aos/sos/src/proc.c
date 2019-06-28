#include "proc.h"

static pcb_t *cur_proc;

void set_cur_proc(pcb_t *proc)
{
    cur_proc = proc;
}

pcb_t *get_cur_proc()
{
    return cur_proc;
}