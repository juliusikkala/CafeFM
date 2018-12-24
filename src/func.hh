#ifndef CAFEFM_FUNC_HH
#define CAFEFM_FUNC_HH
#include <stdint.h>

inline int32_t i32sin(int32_t x)
{
    int sign = x < 0;
    x = sign ? x : -x;
    if(x < -0x40000000) x = -0x80000000 - x;

    int64_t u = -701344734;
    u = (u * x + -6845928214293853184l) >> 32;
    u = (u * x + -318968611464870144l) >> 32;
    u = (u * x + 5834740796096394240l) >> 31;
    u = (u * x + -7692973104070968l) >> 32;
    u = (u * x + -5958333797304125440l) >> 30;
    u = (u * x + -35096281021490l) >> 31;
    u = (u * x + 7244018807034865664l) >> 30;
    u = (u * x + -3936575508l) >> 31;

    return sign ? u : -u;
}

inline int32_t i32square(int32_t x)
{
    return x < 0 ? -0x7FFFFFFF : 0x7FFFFFFF;
}

inline int32_t i32triangle(int32_t x)
{
    return 0x7FFFFFFF - ((x < 0 ? -x-1 : x) << 1);
}

inline int32_t i32saw(int32_t x)
{
    return x;
}

// This function makes sure the fraction components fit in 32 bits.
inline void normalize_fract(uint64_t& num, uint64_t& denom)
{
    int64_t mask = num|denom;
#if defined(__GNUC__) || defined(__clang__)
    int of = 32 - __builtin_clzl(mask|1);
    if(of > 0)
    {
        num >>= of;
        denom >>= of;
        // Prevent division by zero
        denom |= !denom;
    }
#else
#error "CLZ not yet implemented for compilers other than GCC or Clang!"
#endif
}

inline int64_t lerp(
    int64_t a,
    int64_t b,
    int64_t num,
    int64_t denom
){
    return a + num * (b - a) / denom;
}

#endif
