#pragma once

#define FLIB_DECLARE_RINGBUFFER_STATIC(name, itemtype, size) \
    static ptrdiff_t name ## _Size = size; \
    static ptrdiff_t name ## _Used = 0; \
    static ptrdiff_t name ## _Head = 0; \
    static ptrdiff_t name ## _Last = 0; \
    static ptrdiff_t name ## _Next = 0; \
    static itemtype name [size];

#define FRingBufferFull(rb) (rb ## _Used >= rb ## _Size)

// Pushes an empty item to the given ringbuffer. Returns a pointer to the item.
#define FRingBufferPush(rb) (\
    (rb ## _Used ++), \
    (rb ## _Last = rb ## _Next), \
    (rb ## _Next = (rb ## _Next + 1) % rb ## _Size), \
    &(rb [(rb ## _Last)]))