#pragma once

typedef void *NullPtr;

#if (__clang_major__ < 16)
#define nullptr ((NullPtr)0)
#endif

#define AutoType __auto_type

#define static_assert _Static_assert
