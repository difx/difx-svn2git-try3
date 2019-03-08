// JMA_math.h
// add in stuff for good math stuff under GCC
// 2000 Sep 17  James M Anderson  --NMT  Original Version
// 2005 Sep 20  JMA  --update some \pi related constants to more precision
// 2007 Dec 03  JMA  --MPIfR update for complex
// 2008 Sep 22  JMA  --fix M_PI stuff
// 2009 Jan 19  JMA  --chnage complex to full bit depth numbers
// 2010 Feb 08  JMA  --add tgmath stuff
// 2011 Mar 31  JMA  --MPIfR  make *_t typedefs for floating point
// 2012 Jan 30  JMA  --add __float128 stuff
// 2012 Feb 05  JMA  --update __float128 stuff and int128_t stuff
// 2012 Feb 13  JMA  --update restrict stuff

// Copyright (c) 2012, James M. Anderson <anderson@mpifr-bonn.mpg.de>

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
//      copyright notice, this list of conditions and the following
//      disclaimer in the documentation and/or other materials
//      provided with the distribution.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.


#ifndef JMA_MATH_H
#define JMA_MATH_H


// make sure we have some type things
#include "JMA_code.h"



// are we usign GCC ?
// if not, barf
#ifndef __GNUC__
# 	error "GCC required for compilation"
#elif (__GNUC__ < 2)
#	error "at least GCC 2 required for compilation"
#endif

// we want to use ISO C9X stuff
// we want to use some GNU stuff
// But this sometimes breaks time.h
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif
#include <time.h>
#ifndef __USE_ISOC99
#  define __USE_ISOC99 1
#endif
#ifndef _ISOC99_SOURCE
#  define _ISOC99_SOURCE
#endif
#ifndef __USE_MISC
#  define __USE_MISC 1
#endif





// get the math functions
#ifdef __cplusplus
#  include <complex>
#  include <cmath>
#  include <math.h>
typedef std::complex<float>         Complex64;
typedef std::complex<double>        Complex128;
typedef std::complex<long double>   Complex160;
typedef std::complex<float>         Complex64_t;
typedef std::complex<double>        Complex128_t;
typedef std::complex<long double>   Complex160_t;
#  if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) \
    && (defined(__i386__) || defined(__x86_64__) || defined(__ia64__))
typedef std::complex<__float128>    Complex256_t;
#  endif
#else
#  include <complex.h>
#  include <math.h>
#  include <tgmath.h>
typedef _Complex float              Complex64;
typedef _Complex double             Complex128;
typedef _Complex long double        Complex160;
typedef _Complex float              Complex64_t;
typedef _Complex double             Complex128_t;
typedef _Complex long double        Complex160_t;
#  define Complex64(r,i) ((float)(r) + ((float)(i))*I)
#  define Complex128(r,i) ((double)(r) + ((double)(i))*I)
#  define Complex160(r,i) ((long double)(r) + ((long double)(i))*I)
#  define Complex64_t(r,i) ((float)(r) + ((float)(i))*I)
#  define Complex128_t(r,i) ((double)(r) + ((double)(i))*I)
#  define Complex160_t(r,i) ((long double)(r) + ((long double)(i))*I)
#  if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) \
    && (defined(__i386__) || defined(__x86_64__) || defined(__ia64__))
typedef __complex128                Complex256_t;
#    define Complex256_t(r,i) ((__float128)(r) + ((__float128)(i))*I)
#  endif
//#define real(x) creal(x)
//#define imag(x) cimag(x)
//#define abs(x) fabs(x)
//#define arg(x) carg(x)
#endif







// put anything else intersting here

/* restrict
   This is a really useful modifier, but it is not supported by
   all compilers.  Furthermore, the different ways to specify it
   (double * restrict dp0, double dp1[restrict]) are not available
   in the same release of a compiler.  If you are still using an old
   compiler, your performace is going to suck anyway, so this code
   will only give you restrict when it is fully available.
*/
#ifdef __GNUC__
#  ifdef restrict
/*   Someone else has already defined it.  Hope they got it right. */
#  elif !defined(__GNUG__) && (__STDC_VERSION__ >= 199901L)
/*   Restrict already available */
#  elif !defined(__GNUG__) && (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#    define restrict __restrict
#  elif (__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
#    define restrict __restrict
#  else
#    define restrict
#  endif
#else
/* C99 has restrict */
#  if (__STDC_VERSION__ >= 199901L)
#  else 
#    ifndef restrict
#      define restrict
#    endif
#  endif
#endif






// Radian to Degree conversions
// #define M_RAD2DEG 57.29577951308232087665
// #define M_DEG2RAD  0.0174532925199432976913
#ifndef M_RAD2DEG
#  define M_RAD2DEG 57.29577951308232087679815
#endif
#ifndef M_DEG2RAD
#  define M_DEG2RAD  0.01745329251994329576923691
#endif

// C99 decided to drop M_PI and other things.  Define them here
#ifndef M_PI
#  define M_PI 3.14159265358979323846264338327950288419716939937510
#endif
#ifndef M_PIf
#  define M_PIf 3.14159265358979323846264338327950288419716939937510f
#endif
#ifndef M_PIl
#  define M_PIl 3.14159265358979323846264338327950288419716939937510L
#endif
#ifndef M_2PI
#  define M_2PI (2.0f*M_PI)
#endif
#ifndef M_2PIf
#  define M_2PIf (2.0f*M_PIf)
#endif
#ifndef M_2PIl
#  define M_2PIl (2.0f*M_PIl)
#endif
#ifndef M_PI_2
#  define M_PI_2 (0.5f*M_PI)
#endif
#ifndef M_PI_2f
#  define M_PI_2f (0.5f*M_PIf)
#endif
#ifndef M_PI_2l
#  define M_PI_2l (0.5f*M_PIl)
#endif







#endif // JMA_MATH_H

