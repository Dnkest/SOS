## SOS

### Timer Driver
projects/aos/libclock/include/clock/clock.h

- Registered timer and associated callback functions are saved in a priority queue.

- Newly registered timer is push onto the priority queue, and the next timeout/interrupt is set to the - scheduled time of the head of the queue substracts by current time.

- When an interrupt is triggered, all items at front of the queue that have scheduled time greater than currently time are unpacked and the callback functions inside are executed.

### Virtual Memory

### File System

### Demand Paging

### Process Management
