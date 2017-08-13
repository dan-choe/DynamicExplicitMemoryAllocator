# DynamicExplicitMemoryAllocator

### What is Dynamic Explicit Memory Allocator?

This console program is implemented for developing better understanding of **Dynamic Explicit Memory Allocation**.
Instead of using build-in malloc, calloc, realloc, and free, it has own those functions.

Evenytime, when you allocate or reallocate some space, you can see the block size, padding size, and splinter size on header and bottom of each block. If you know about the difference between implict and explicit, you will have better understanding of the block structure.

The header is defined as **a single 64-bit quadword with the block size and allocated bit**.
Note: 1 full memory row is 64-bits in size (8 bytes = 1 quad word in x86_64)

Each page size is **4096 bytes**.
You can't allocate more than 4 pages such that the maximum size is 4096 *4 bytes (=16,384bytes).

Free blocks will be inserted into the list in sorted ascending address order.

This followes **Best Fit Placement Policy** ,and performs **immediate coalescing**.


### How to use?

Clone this project, then open the terminal.
Move the current location to the directory on terminal.
Then, type 'make' on terminal to make the executional file.
Run the file (Ex. bin/main)

It will show the allocation blocks as requested in main.c file.
You can customize whatever you want to allocate the different sizes.

### Screenshots
(updating ...)

### Unit testing in C

Under bin folder, there is test_main file which is for running test cases.

## License
Copyright [2017] [Dan Choe](https://github.com/dan-choe)
