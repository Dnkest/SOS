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
/****************************************************************************
 *
 *      $Id:  $
 *
 *      Description: Simple milestone 0 test.
 *
 *      Author:         Godfrey van der Linden
 *      Original Author:    Ben Leslie
 *
 ****************************************************************************/

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <utils/time.h>
#include <utils/page.h>
#include <syscalls.h>

#include <sos.h>
#include "benchmark.h"

size_t sos_write(void *vData, size_t count)
{
    //implement this to use your syscall
    return sos_sys_write(3, vData, count);
}

int main(void)
{
    sosapi_init_syscall_table();

    // int in = open("console", 1);
    // assert(in >= 0);

    // printf("task:\tHello world, I'm ben!\n");
    sos_bench(0);
    return 0;
}
