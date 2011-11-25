
#include <ipps.h>
#include <stdio.h>

int main(int argc, char** argv)
{
   const int N = 16;
   Ipp32fc v;

   #define ACCU_RE  3.653725000000000e+04
   #define A_B_RE   3.360162734985352e+01
   #define A_B_IM   4.114639663696289e+01

   Ipp32fc* a = ippsMalloc_32fc(N);
   Ipp32fc* b = ippsMalloc_32fc(N);
   Ipp32fc* accu_fma = ippsMalloc_32fc(N);
   Ipp32fc* accu_muladd = ippsMalloc_32fc(N);
   Ipp32fc* tmp = ippsMalloc_32fc(N);

   v.re = ACCU_RE;
   v.im = 0;
   ippsSet_32fc(v, accu_fma, N);
   ippsSet_32fc(v, accu_muladd, N);

   v.re = A_B_RE;
   v.im = A_B_IM;
   ippsSet_32fc(v, a, N);

   v.re = A_B_RE;
   v.im = -A_B_IM;
   ippsSet_32fc(v, b, N);

   double ref = ACCU_RE + (A_B_RE*A_B_RE + A_B_IM*A_B_IM);
   float  refF = float(ACCU_RE) + (float(A_B_RE)*float(A_B_RE) + float(A_B_IM)*float(A_B_IM));
   ippsAddProduct_32fc(a, b, accu_fma, N);
   ippsMul_32fc(a, b, tmp, N);
   ippsAdd_32fc_I(tmp, accu_muladd, N);

   printf("Reference (double): %6.15f + i*0.0\n", ref);
   printf("Reference (cast):   %6.15f + i*0.0\n", float(ref));
   printf("Reference (float):  %6.15f + i*0.0\n", refF);
   printf("ippsAddProduct:     %6.15f + i*%6.15f\n", accu_fma[0].re, accu_fma[0].im);
   printf("ippsMul, ippsAdd_I: %6.15f + i*%6.15f\n", accu_muladd[0].re, accu_muladd[0].im);
   return 0;
}
