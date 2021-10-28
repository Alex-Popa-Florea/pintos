#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H
#include "lib/stdint.h"

/* Module supports fixed-point arithmetic in
the following p.q fixed-point format: */
#define FIXED_P (17)
#define FIXED_Q (14)
/* Define f = 2 ^ q as the following macro: */
#define FIXED_F (1 << FIXED_Q)

/* Structure to represent fixed-point integers */
typedef struct {
    int32_t val; /* Value of fixed-point integer */
} fp_int;

fp_int convert_fp (int32_t);
int32_t convert__int_zero (fp_int);
int32_t convert_int_nearest (fp_int);
fp_int add_fps (fp_int, fp_int);
fp_int add_fps_int (fp_int, int32_t);
fp_int sub_fps (fp_int, fp_int);
fp_int sub_fps_int (fp_int, int32_t);
fp_int mult_fps (fp_int, fp_int);
fp_int mult_fps_int (fp_int, int32_t);
fp_int div_fps (fp_int, fp_int);
fp_int div_fps_int (fp_int, int32_t);

#endif /* threads/fixed-point.h */  