#include <debug.h>
#include "threads/fixed-point.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Converts an integer to fixed-point */
inline fp_int
convert_fp (int32_t n)
{
    ASSERT ((n >= INT32_MIN) && (n <= INT32_MAX));
    return (fp_int) {n * FIXED_F};
}

/* Converts fixed-point to integer (rounding to zero) */
inline int32_t
convert_int_zero (fp_int x)
{
    return (int32_t) (x.val / FIXED_F);
}

/* Converts fixed-point to integer (rounding to nearest int) */
inline int32_t
convert_int_nearest (fp_int x)
{
    return (int32_t) (x.val >= 0 ? ((x.val + FIXED_F/2 ) / FIXED_F) : ((x.val - FIXED_F / 2) / FIXED_F));
}

/* Adds two fixed-point numbers */
inline fp_int
add_fps (fp_int x, fp_int y)
{
    return (fp_int) {x.val + y.val};
}

/* Adds two fixed-point numbers */
inline fp_int
add_fps_int (fp_int x, int32_t n)
{
    ASSERT ((n >= INT32_MIN) && (n <= INT32_MAX));
    return (fp_int){x.val + n * FIXED_F};
}

/* Subtract two fixed-point numbers */
inline fp_int
sub_fps (fp_int x, fp_int y)
{
    return (fp_int) {x.val - y.val};
}

/* Subtract integer from fixed-point number */
inline fp_int
sub_fps_int (fp_int x, int32_t n)
{
    ASSERT ((n >= INT32_MIN) && (n <= INT32_MAX));
    return (fp_int) {x.val - n * FIXED_F};
}

/* Multiply two fixed point numbers */
inline fp_int
mult_fps (fp_int x, fp_int y)
{
    return (fp_int) {((int64_t) x.val) * y.val / FIXED_F};
}

/* Multiply fixed-point and int */
inline fp_int
mult_fps_int (fp_int x, int32_t n)
{
    ASSERT ((n >= INT32_MIN) && (n <= INT32_MAX));
    return (fp_int) {x.val * n};
}

/* Divide fixed-point numbers */
inline fp_int
div_fps (fp_int x, fp_int y)
{
    return (fp_int) {((int64_t) x.val) * FIXED_F / y.val};
}

/* Divide fixed-point number by an int */
inline fp_int
div_fps_int(fp_int x, int32_t n)
{
    ASSERT ((n >= INT32_MIN) && (n <= INT32_MAX));
    return (fp_int) {x.val / n};
}