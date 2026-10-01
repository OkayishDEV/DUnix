#ifndef _LIBC_MATH_H
#define _LIBC_MATH_H

#define M_PI 3.14159265358979323846
#define PI   3.14159265358979323846
#define M_E  2.71828182845904523536

double fabs(double x);
double sqrt(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double floor(double x);
double ceil(double x);
double pow(double base, double exp);

static inline float sqrtf(float x) { return (float)sqrt((double)x); }
static inline float sinf(float x) { return (float)sin((double)x); }
static inline float cosf(float x) { return (float)cos((double)x); }
static inline float tanf(float x) { return (float)tan((double)x); }
static inline float fabsf(float x) { return (float)fabs((double)x); }
static inline float fminf(float a, float b) { return a < b ? a : b; }
static inline float fmaxf(float a, float b) { return a > b ? a : b; }

#endif /* _LIBC_MATH_H */
