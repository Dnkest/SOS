## SOS
### Timer Driver
projects/aos/libclock/include/clock/clock.h

- Registered timer and associated callback functions are saved in a priority queue.

- Newly registered timer is push onto the priority queue, and the next timeout/interrupt is set to the scheduled time of the head of the queue substracts by current time.

- When an interrupt is triggered, all items at front of the queue that have scheduled time greater than currently time are unpacked and the callback functions inside are executed.

### Virtual Memory
projects/aos/sos/src/addrspace.h
projects/aos/sos/src/paagetable.h

- Each process's virtual memory is divided into regions with appropriate permissions.

- Four-level page table is used to map virtual addresses into frame references.

- Virtual memory faults are handle by firstly checking if the address falls within a valid region and the access rights are respected, if so then a lookup into the pagetable is done by the kernel to see if there is already a mapping for the given address. If there is none, a new page is allocated and mapped and the process is resumed.

- Note that the heap and stack region are fixed-sized hence the brk system call may fail if heap grows out of the valid heap region, which is quite large (16MB).

### File System

### Demand Paging

### Process Management
