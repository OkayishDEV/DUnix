#include <math.h>

double fabs(double x) {
    return (x < 0.0) ? -x : x;
}

double sqrt(double x) {
    if (x < 0.0) return 0.0;
    if (x == 0.0) return 0.0;

    double res = x;
    for (int i = 0; i < 20; i++) {
        res = 0.5 * (res + x / res);
    }
    return res;
}

double sin(double x) {
    /* Normalize x to [-PI, PI] */
    while (x > M_PI)  x -= 2.0 * M_PI;
    while (x < -M_PI) x += 2.0 * M_PI;

    /* Taylor series: x - x^3/3! + x^5/5! - x^7/7! + x^9/9! - x^11/11! */
    double term = x;
    double sum = x;
    double x2 = x * x;

    term = -term * x2 / (2.0 * 3.0);
    sum += term;
    term = -term * x2 / (4.0 * 5.0);
    sum += term;
    term = -term * x2 / (6.0 * 7.0);
    sum += term;
    term = -term * x2 / (8.0 * 9.0);
    sum += term;
    term = -term * x2 / (10.0 * 11.0);
    sum += term;
    term = -term * x2 / (12.0 * 13.0);
    sum += term;

    return sum;
}

double cos(double x) {
    return sin(x + M_PI / 2.0);
}

double tan(double x) {
    double c = cos(x);
    if (fabs(c) < 1e-12) return 0.0;
    return sin(x) / c;
}

double floor(double x) {
    long long i = (long long)x;
    if (x < 0.0 && (double)i != x) i--;
    return (double)i;
}

double ceil(double x) {
    long long i = (long long)x;
    if (x > 0.0 && (double)i != x) i++;
    return (double)i;
}

double pow(double base, double exp) {
    if (exp == 0.0) return 1.0;
    if (base == 0.0) return 0.0;

    double res = 1.0;
    long long n = (long long)fabs(exp);
    double b = base;

    while (n > 0) {
        if (n & 1) res *= b;
        b *= b;
        n >>= 1;
    }

    if (exp < 0.0) return 1.0 / res;
    return res;
}
