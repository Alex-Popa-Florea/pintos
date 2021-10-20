#include "threads/fixed-point.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Converts an integer to fixed-point */
fp_int inline convert_fp(int32_t n)
{
    return (fp_int){n * FIXED_F};
}

/* Converts fixed-point to integer (rounding to zero) */
int32_t inline convert__int_zero(fp_int x)
{
    return (int32_t) x.val / FIXED_F;
}

/* Converts fixed-point to integer (rounding to nearest int) */
int32_t inline convert_int_nearest(fp_int x)
{
    return (int32_t) x.val >= 0 ? (x.val + FIXED_F/2 ) /FIXED_F : (x.val - FIXED_F/2 ) /FIXED_F;
}

/* Adds two fixed-point numbers */
fp_int inline add_fps(fp_int x, fp_int y)
{
    return (fp_int){x.val + y.val};
}

/* Adds two fixed-point numbers */
fp_int inline add_fps_int(fp_int x, int32_t n)
{
    return (fp_int){ x.val + n*FIXED_F};
}

/* Subtract two fixed-point numbers */
fp_int inline sub_fps(fp_int x, fp_int y)
{
    return (fp_int){x.val - y.val};
}

/* Subtract integer from fixed-point number */
fp_int inline sub_fps_int(fp_int x, int32_t n)
{
    return (fp_int){x.val - n*FIXED_F};
}

/* Multiply two fixed point numbers */
fp_int inline mult_fps(fp_int x, fp_int y)
{
    return (fp_int){((int64_t) x.val) * y.val/FIXED_F};
}

/* Multiply fixed-point and int */
fp_int inline mult_fps_int(fp_int x, int32_t n)
{
    return (fp_int){x.val * n};
}

/* Divide fixed-point numbers */
fp_int inline div_fps(fp_int x, fp_int y)
{
    return (fp_int){((int64_t) x.val) * FIXED_F/y.val};
}

/* Divide fixed-point number by an int */
fp_int inline div_fps_int(fp_int x, int32_t n)
{
    return (fp_int){x.val/n};
}