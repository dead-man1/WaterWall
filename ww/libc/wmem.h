#pragma once

#include <stddef.h> /* for size_t */

struct dedicated_memory_s;
typedef struct dedicated_memory_s dedicated_memory_t;


void *memoryAllocate(size_t size);
void *memoryAllocateZero(size_t size);
void *memoryReAllocate(void *ptr, size_t size);
void *memoryCalloc(size_t n,size_t size);
void  memoryFree(void *ptr);

void *memoryDedicatedAllocate(dedicated_memory_t *dm, size_t size);
void *memoryDedicatedReallocate(dedicated_memory_t *dm, void *ptr, size_t size);
void  memoryDedicatedFree(dedicated_memory_t *dm, void *ptr);

