#pragma once
#include <common.h>

#define FARRAY_DEFAULT_CAPACITY 8

typedef void (*FArrayItemFreeFunction) (void* item);

typedef struct {
    size_t size;
    size_t capacity;
    size_t itemsize;
    void* items;
    VXAllocFunction mem_alloc;
    VXFreeFunction  mem_free;
    FArrayItemFreeFunction mem_free_item;
    bool mem_free_self;
} FArray;

static inline void FArrayInit (FArray* array, size_t itemsize, size_t capacity,
    VXAllocFunction alloc, VXFreeFunction free, FArrayItemFreeFunction free_item)
{
    array->mem_alloc     = alloc ? alloc : vxGenAllocEx;
    array->mem_free      = alloc ? free : vxGenFreeEx;
    array->mem_free_item = free_item;
    array->mem_free_self = false;
    array->size = 0;
    array->capacity = capacity;
    array->itemsize = itemsize;
    array->items = array->mem_alloc(capacity, itemsize, itemsize, __FILE__, __LINE__, VXFUNCTION);
}

static inline FArray* FArrayAlloc (size_t itemsize, size_t capacity,
    VXAllocFunction alloc, VXFreeFunction free, FArrayItemFreeFunction free_item)
{
    if (!alloc) {
        alloc = vxGenAllocEx;
        free = vxGenFreeEx;
    }
    FArray* array = (FArray*) alloc(1, sizeof(FArray), sizeof(void*),
        __FILE__, __LINE__, VXFUNCTION);
    FArrayInit(array, itemsize, capacity, alloc, free, free_item);
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
    if (array->mem_free) {
        array->mem_free(array->items, __FILE__, __LINE__, VXFUNCTION);
        if (array->mem_free_self) {
            array->mem_free(array, __FILE__, __LINE__, VXFUNCTION);
        }
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
            void* old_items = array->items;
            // Determine new capacity and allocate new items:
            while (array->capacity < array->size) {
                if (array->capacity == 0) {
                    array->capacity = FARRAY_DEFAULT_CAPACITY;
                }
                array->capacity *= 2;
            }
            array->items = array->mem_alloc(array->capacity, array->itemsize, array->itemsize,
                __FILE__, __LINE__, VXFUNCTION);
            // Copy old items into new array:
            memcpy(array->items, old_items, array->itemsize * old_size);
            // Deallocate old items:
            if (array->mem_free) {
                array->mem_free(old_items, __FILE__, __LINE__, VXFUNCTION);
            }
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