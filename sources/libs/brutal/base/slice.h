#pragma once

#define Slice(T)    \
    struct          \
    {               \
        size_t len; \
        T *ptr;     \
    }