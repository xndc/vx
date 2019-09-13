#pragma once

#define RingBufferDeclare(name, itemtype, size) \
    const ptrdiff_t name ## __Size = size; \
    const ptrdiff_t name ## __Item = sizeof(itemtype); \
    ptrdiff_t name ## __Used = 0; \
    ptrdiff_t name ## __Head = 0; \
    ptrdiff_t name ## __Last = 0; \
    ptrdiff_t name ## __Next = 0; \
    itemtype* name ## __Temp = NULL; \
    itemtype name [size] = {0};

#define RingBufferDeclareStatic(name, itemtype, size) \
    static const ptrdiff_t name ## __Size = size; \
    static const ptrdiff_t name ## __Item = sizeof(itemtype); \
    static ptrdiff_t name ## __Used = 0; \
    static ptrdiff_t name ## __Head = 0; \
    static ptrdiff_t name ## __Last = 0; \
    static ptrdiff_t name ## __Next = 0; \
    static itemtype* name ## __Temp = NULL; \
    static itemtype name [size] = {0};

#define RingBufferFull(rb) (rb ## __Used >= rb ## __Size)
#define RingBufferSize(rb) (rb ## __Size)
#define RingBufferUsed(rb) (rb ## __Used)

// Pushes an empty item to the given ringbuffer. Returns a pointer to the item.
#define RingBufferPush(rb) (\
    (rb ## __Used ++), \
    (rb ## __Last = rb ## __Next), \
    (rb ## __Next = ((rb ## __Next + 1) % rb ## __Size)), \
    &(rb [(rb ## __Last)]))

// Pushes an item to the given ringbuffer. Returns a pointer to the item.
#define RingBufferPushItem(rb, item) (\
    (rb ## __Temp = RingBufferPush(rb)), \
    (*(rb ## __Temp) = (item)), \
    (rb ## __Temp)) \

// Removes an item from the front of the given ringbuffer.
#define RingBufferPop(rb) ((rb ## __Used > 0)? \
    ((rb ## __Used --), (rb ## __Head = ((rb ## __Head + 1) % rb ## __Size))) : \
    (ptrdiff_t)(0))

// static void test() {
//     RingBufferDeclare(buf, int, 8);
//     while (!RingBufferFull(buf)) {
//         RingBufferPushItem(buf, 1);
//         RingBufferPushItem(buf, 2);
//         RingBufferPushItem(buf, 3);
//         RingBufferPushItem(buf, 4);
//         RingBufferPushItem(buf, 5);
//         RingBufferPushItem(buf, 6);
//     }
//     for (int* pitem = RingBufferFirst(buf); pitem != NULL; pitem = RingBufferNext(buf, pitem)) {
//         printf("* %d\n", *pitem);
//     }
// }