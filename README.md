# Custom Memory Allocator Implementation

## Overview

This project implements a custom memory allocator in C, inspired by foundational tutorials and memory management techniques. The allocator supports dynamic memory allocation (`malloc`, `calloc`, `realloc`) and deallocation (`free`) with advanced features like block splitting, coalescing, and mmap integration.

## Features

- **Dynamic Memory Allocation**:
  - Implements `malloc`, `calloc`, `realloc`, and `free` functions for memory management.
  - Optimized for performance and memory efficiency.
- **Block Management**:
  - Allocates memory using `sbrk` and `mmap` for small and large allocations, respectively.
  - Splits large blocks to minimize fragmentation.
  - Coalesces adjacent free blocks to optimize memory reuse.
- **Alignment**:
  - Ensures all allocated memory is aligned to an 8-byte boundary for compatibility and performance.
- **Error Handling**:
  - Robust error checking for system calls like `sbrk` and `mmap`.
- **Customizable Threshold**:
  - Uses a configurable threshold (`MMAP_THRESHOLD`) to determine when to allocate memory via `mmap`.

## Project Structure

- **`os_malloc()`**:
  - Core allocation function. Allocates memory blocks based on size and alignment.
  - Handles both small (via `sbrk`) and large (via `mmap`) allocations.
- **`os_free()`**:
  - Frees allocated memory, coalescing adjacent free blocks to reduce fragmentation.
  - Unmaps memory for blocks allocated with `mmap`.
- **`os_calloc()`**:
  - Allocates and initializes memory to zero for multiple elements.
  - Supports efficient zero-initialization using `mmap` or `malloc` and `memset`.
- **`os_realloc()`**:
  - Resizes an allocated memory block, preserving data where possible.
  - Uses a combination of block resizing and new block allocation with data copying.
- **Block Meta Management**:
  - Metadata (`struct block_meta`) tracks block status, size, and links in a doubly linked list.
  - Supports block splitting and coalescing for efficient memory management.

## Memory Management Techniques

1. **Block Splitting**:
   - Large blocks are split into smaller blocks when the requested size is less than the available size.
2. **Block Coalescing**:
   - Adjacent free blocks are merged to reduce fragmentation and optimize space usage.
3. **Best-Fit Allocation**:
   - Searches for the smallest free block that satisfies the requested size to minimize waste.
4. **Threshold-Based Allocation**:
   - Allocates large blocks using `mmap` when the requested size exceeds `MMAP_THRESHOLD`.

## Constants and Definitions

- **Alignment**: 8-byte boundary for memory alignment.
- **Meta Size**: Size of the `struct block_meta`, which stores block metadata.
- **MMAP Threshold**: Blocks larger than 128 KB (`MMAP_THRESHOLD`) are allocated using `mmap`.
- **Page Size**: Uses system-defined page size for memory allocation granularity.

## References

- [Malloc Tutorial by Dan Luu](https://danluu.com/malloc-tutorial/)
- [CS351 Memory Management Slides](https://moss.cs.iit.edu/cs351/slides/slides-malloc.pdf)
