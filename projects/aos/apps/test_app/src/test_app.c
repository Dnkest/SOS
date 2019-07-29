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

static size_t sos_debug_print(const void *vData, size_t count)
{
#ifdef CONFIG_DEBUG_BUILD
    size_t i;
    const char *realdata = vData;
    for (i = 0; i < count; i++) {
        seL4_DebugPutChar(realdata[i]);
    }
#endif
    return count;
}

size_t sos_write(void *vData, size_t count)
{
    //implement this to use your syscall
    return sos_debug_print(vData, count);
}

int main(void)
{
    /* set up the c library. printf will not work before this is called */
    sosapi_init_syscall_table();

    sos_debug_print("haha\n", 6);
}
