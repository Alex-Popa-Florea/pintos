#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H
#include "lib/inttypes.h"

#define FIXED_P (17)
#define FIXED_Q (14)
#define FIXED_F (1 << FIXED_Q)

typedef struct fp_int {
    int32_t val;
} fp_int;

fp_int convert_to_fixed_point(int32_t );
int32_t convert_to_int_round_zero(fp_int );
int32_t convert_to_int_round_nearest(fp_int );
fp_int add_fixed_points(fp_int , fp_int );
fp_int add_fixed_point_and_integer(fp_int , int32_t );
fp_int subtract_fixed_points(fp_int , fp_int );
fp_int subtract_fixed_point_and_integer(fp_int , int32_t );
fp_int multiply_fixed_points(fp_int , fp_int );
fp_int multiply_fixed_point_and_integer(fp_int , int32_t );
fp_int divide_fixed_points(fp_int , fp_int );
fp_int divide_fixed_point_and_integer(fp_int , int32_t );

#endif /* threads/fixed-point.h */