/* Small math routines for the inference domain. No libm, no runtime calls.
 * SSE2 only: firmware may leave the shared x87/MMX register stack occupied.
 * Range-reduced double polynomials implement exp/sin/cos and positive pow.
 * The polynomial error on [-ln(2)/2, ln(2)/2] is below float rounding error.
 */
#include <stdint.h>

static double exp_reduced(double x) {
    const double ln2=0.69314718055994530942;
    int n=(int)(x*1.4426950408889634074+(x>=0 ? 0.5 : -0.5));
    double r=x-n*ln2;
    double p=1.0/479001600.0;
    p=1.0/39916800.0+r*p; p=1.0/3628800.0+r*p;
    p=1.0/362880.0+r*p; p=1.0/40320.0+r*p;
    p=1.0/5040.0+r*p; p=1.0/720.0+r*p;
    p=1.0/120.0+r*p; p=1.0/24.0+r*p;
    p=1.0/6.0+r*p; p=0.5+r*p; p=1.0+r*p; p=1.0+r*p;
    union { uint64_t u; double d; } scale={.u=(uint64_t)(n+1023)<<52};
    return p*scale.d;
}
float bm_expf(float x) {
    if (x!=x) return x;
    if (x>88.722839f) return __builtin_inff();
    if (x<-103.972084f) return 0;
    return (float)exp_reduced(x);
}
float bm_powf(float x, float y) {
    /* Used by RoPE only with a positive finite base and 0 <= y < 1. */
    if (y==0) return 1;
    if (!(x>0) || !__builtin_isfinite(x) || !__builtin_isfinite(y)) return __builtin_nanf("");
    /* x is a float, so conversion to double also normalizes float subnormals.
     * x = m * 2^exponent, with 1/sqrt(2) <= m <= sqrt(2).
     * ln(m) = 2 * (z + z^3/3 + ...), z=(m-1)/(m+1), |z| <= 0.172.
     * Through z^23, the omitted tail is below 7e-21. */
    union { uint64_t u; double d; } bits={.d=(double)x};
    int exponent=(int)(bits.u>>52)-1023;
    bits.u=(bits.u&UINT64_C(0x000fffffffffffff))|UINT64_C(0x3ff0000000000000);
    double m=bits.d;
    if (m>1.4142135623730950488) { m*=0.5; exponent++; }
    double z=(m-1)/(m+1), square=z*z;
    double series=1.0/23.0;
    for (int denominator=21;denominator>=3;denominator-=2)
        series=1.0/denominator+square*series;
    double logarithm=exponent*0.69314718055994530942+2*z*(1+square*series);
    double e=logarithm*(double)y;
    if (e>88.72283905206835) return __builtin_inff();
    if (e<-103.972084045410) return 0;
    return (float)exp_reduced(e);
}
static float trig(float x, int cosine) {
    /* RoPE angles have |x| <= 8192. Split pi/2 so n*high is exact here;
     * subtracting the low part preserves accuracy near multiples of pi/2. */
    if (!__builtin_isfinite(x) || x>8192 || x<-8192) return __builtin_nanf("");
    if (x==0) return cosine ? 1.0f : x;
    int n=(int)((double)x*0.63661977236758134308+(x>=0 ? 0.5 : -0.5));
    double r=((double)x-n*1.57079632673412561417)-n*6.07710050650619224932e-11;
    double t=r*r;
    unsigned quadrant=((unsigned)n+(unsigned)cosine)&3;
    double value;
    if (quadrant&1) {
        double p=1.0/20922789888000.0;
        p=-1.0/87178291200.0+t*p; p=1.0/479001600.0+t*p;
        p=-1.0/3628800.0+t*p; p=1.0/40320.0+t*p;
        p=-1.0/720.0+t*p; p=1.0/24.0+t*p;
        p=-0.5+t*p; value=1+t*p;
    } else {
        double p=1.0/355687428096000.0;
        p=-1.0/1307674368000.0+t*p; p=1.0/6227020800.0+t*p;
        p=-1.0/39916800.0+t*p; p=1.0/362880.0+t*p;
        p=-1.0/5040.0+t*p; p=1.0/120.0+t*p;
        p=-1.0/6.0+t*p; value=r*(1+t*p);
    }
    return (float)(quadrant&2 ? -value : value);
}
float bm_sinf(float x) { return trig(x,0); }
float bm_cosf(float x) { return trig(x,1); }
float bm_sqrtf(float x) {
    float y; __asm__("sqrtss %1,%0" : "=x"(y) : "x"(x)); return y;
}
float bm_ldexpf(float x, int n) {
    /* Binary16 subnormal conversion calls this only with n = -24. */
    union { uint64_t u; double d; } scale={.u=(uint64_t)(n+1023)<<52};
    return (float)((double)x*scale.d);
}
