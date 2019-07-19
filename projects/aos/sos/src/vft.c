
#include <fcntl.h>

#include "vft.h"
#include "frame_table.h"
#include "fs/vfs.h"
#include "syscall.h"
#include "utils/idalloc.h"

#define TMP 0x600000000000

typedef struct kn {
    frame_ref_t frame;
    char reference;
} kn_t;

static fd_table_t *fdt = NULL;
static int pagefile_fd;

static unsigned long int used = 0;
static kn_t frames[CONFIG_SOS_FRAME_LIMIT];

static unsigned long int ptr = 0;

frame_ref_t virtual_alloc_frame(frame_ref_t f)
{
    if (CONFIG_SOS_FRAME_LIMIT == 0ul || used < CONFIG_SOS_FRAME_LIMIT) {
        frames[used].frame = alloc_frame();
        frames[used].reference = 1;
        return frames[used++].frame;
    } else {
        while (1) {
            if (ptr == CONFIG_SOS_FRAME_LIMIT) { ptr = 0; }

            if(f != NULL && frames[ptr].frame == f) {
                frames[ptr].reference = 1;
                return frames[ptr].frame;
            }

            if (frames[ptr].reference == 1) {
                frames[ptr].reference = 0;
                ptr++;
            } else {
                if (fdt == NULL) {
                    fdt = fdt_init();
                    pagefile_fd = vfs_open(fdt, "pagefile", O_RDWR);
                    if (pagefile_fd != 3) {
                        // error
                    }
                }
                seL4_CPtr frame_cap = cspace_alloc_slot(get_global_cspace());
                addrspace_map_one_frame(get_global_addrspace(), get_global_cspace(),
                                    frame_cap, get_global_vspace, TMP,
                                    frames[ptr].frame);
                vfs_pwrite(fdt, pagefile_fd, TMP, frames[ptr].frame * (1<<12), 1<<12);
                free_frame(frames[ptr].frame);
                if (f == 0) {
                    frames[ptr].frame = alloc_frame();
                    //memset(frame_data(frames[ptr].frame), 0, 1<<12);
                } else {
                    seL4_ARM_Page_Unmap(frame_cap);
                    addrspace_map_one_frame(get_global_addrspace(), get_global_cspace(),
                                        frame_cap, get_global_vspace, TMP,
                                        f);
                    vfs_pread(fdt, pagefile_fd, TMP, f * (1<<12), 1<<12);
                    frames[ptr].frame = f;
                    
                }
                seL4_ARM_Page_Unmap(frame_cap);
                ptr++;
                return frames[ptr].frame;
            }
        }
    }
}

void print_limit()
{
    printf("limit %d\n", CONFIG_SOS_FRAME_LIMIT);
}
