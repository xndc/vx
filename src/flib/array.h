#pragma once
#include "common.h"

#define FARRAY_DEFAULT_CAPACITY 8

typedef void (*FArrayItemFreeFunction) (void* item);

typedef struct {
    size_t size;
    size_t capacity;
    size_t itemsize;
    void* items;
    VXAllocator mem_alloc;
    bool mem_free_self;
    FArrayItemFreeFunction mem_free_item;
} FArray;

static inline void FArrayInit (FArray* array, size_t itemsize, size_t capacity,
    VXAllocator alloc, FArrayItemFreeFunction free_item)
{
    array->mem_alloc     = alloc ? alloc : vxGenAlloc;
    array->mem_free_item = free_item;
    array->mem_free_self = false;
    array->size = 0;
    array->capacity = capacity;
    array->itemsize = itemsize;
    array->items = array->mem_alloc(NULL, capacity, itemsize, 0, VXLOCATION);
}

static inline FArray* FArrayAlloc (size_t itemsize, size_t capacity,
    VXAllocator alloc, FArrayItemFreeFunction free_item)
{
    if (!alloc) { alloc = vxGenAlloc; }
    FArray* array = (FArray*) alloc(NULL, 1, sizeof(FArray), sizeof(void*), VXLOCATION);
    FArrayInit(array, itemsize, capacity, alloc, free_item);
    return array;
}

static inline void FArrayClear (FArray* array) {
    if (array->mem_free_item) {
        for (size_t i = 0; i < array->size; i++) {
            array->mem_free_item((void*)(((char*) array->items) + (array->itemsize * i)));
        }
    }
    array->size = 0;
}

static inline void FArrayFree (FArray* array) {
    FArrayClear(array);
    array->mem_alloc(array->items, 0, 0, 0, VXLOCATION);
    if (array->mem_free_self) {
        array->mem_alloc(array, 0, 0, 0, VXLOCATION);
    }
}

static inline void* FArrayReserve (FArray* array, size_t count) {
    if (count == 0) {
        return NULL;
    } else {
        // Resize array if required:
        size_t old_size = array->size;
        array->size += count;
        if (array->capacity < array->size) {
            while (array->capacity < array->size) {
                if (array->capacity == 0) {
                    array->capacity = FARRAY_DEFAULT_CAPACITY;
                }
                array->capacity *= 2;
            }
            array->items =
                array->mem_alloc(array->items, array->capacity, array->itemsize, 0, VXLOCATION);
        }
        // Zero out new items:
        void* firstnew = (void*) ((char*) array->items + (array->itemsize * old_size));
        memset(firstnew, 0, count * array->itemsize);
        return firstnew;
    }
}

static inline void FArrayPush (FArray* array, size_t count, void* items) {
    void* dst = FArrayReserve(array, count);
    memcpy(dst, items, count * array->itemsize);
}