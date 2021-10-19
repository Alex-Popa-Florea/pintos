#include "threads/fixed-point.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#ifdef USERPROG
#include "userprog/process.h"
#endif

fp_int inline convert_to_fixed_point(int32_t n) 
{
    return (fp_int){n * FIXED_F};
}

int32_t inline convert_to_int_round_zero(fp_int x) 
{
    return (int32_t) x.val / FIXED_F;
}

int32_t inline convert_to_int_round_nearest(fp_int x) 
{
    return (int32_t) x.val >= 0 ? (x.val + FIXED_F/2 ) /FIXED_F : (x.val - FIXED_F/2 ) /FIXED_F;
}
fp_int inline add_fixed_points(fp_int x, fp_int y)
{
    return (fp_int){x.val + y.val};
}

fp_int inline add_fixed_point_and_integer(fp_int x, int32_t n)
{
    return (fp_int){ x.val + n*FIXED_F};
}

fp_int inline subtract_fixed_points(fp_int x, fp_int y)
{
    return (fp_int){x.val - y.val};
}

fp_int inline subtract_fixed_point_and_integer(fp_int x, int32_t n)
{
    return (fp_int){x.val - n*FIXED_F};
}

fp_int inline multiply_fixed_points(fp_int x, fp_int y)
{
    return (fp_int){((int64_t) x.val) * y.val/FIXED_F};
}

fp_int inline multiply_fixed_point_and_integer(fp_int x, int32_t n)
{
    return (fp_int){x.val * n};
}

fp_int inline divide_fixed_points(fp_int x, fp_int y)
{
    return (fp_int){((int64_t) x.val) * FIXED_F/y.val};
}

fp_int inline divide_fixed_point_and_integer(fp_int x, int32_t n)
{
    return (fp_int){x.val/n};
}