#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H
#include "lib/inttypes.h"

/* Module supports fixed-point arithmetic in
the following p.q fixed-point format: */
#define FIXED_P (17)
#define FIXED_Q (14)
/* Define f = 2 ^ q as the following macro: */
#define FIXED_F (1 << FIXED_Q) 

/* Structure to represent fixed-point integers */
typedef struct fp_int {
    int32_t val; /* Value of fixed-point integer */
} fp_int;

fp_int inline convert_fp(int32_t );
int32_t inline convert__int_zero(fp_int );
int32_t inline convert_int_nearest(fp_int );
fp_int inline add_fps(fp_int , fp_int );
fp_int inline add_fps_int(fp_int , int32_t );
fp_int inline sub_fps(fp_int , fp_int );
fp_int inline sub_fps_int(fp_int , int32_t );
fp_int inline mult_fps(fp_int , fp_int );
fp_int inline mult_fps_int(fp_int , int32_t );
fp_int inline div_fps(fp_int , fp_int );
fp_int inline div_fps_int(fp_int , int32_t );

#endif /* threads/fixed-point.h */