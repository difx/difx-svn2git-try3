// JMA_code.h 
// JMA's generic programming stuff
// 2000 Sep 17  James M Anderson  --NMT  Original Version
// 2007 Jan 08  JMA  --define __STDC_LIMIT_MACROS
// 2011 Mar 31  JMA  --MPIfR  make *_t typedefs for floating point
// 2012 Jan 30  JMA  --add __float128 stuff
// 2012 Feb 05  JMA  --update __float128 stuff and int128_t stuff


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


#ifndef JMA_CODE_H
#define JMA_CODE_H



// MACRO TO MAKE UNIX F77 ROUTINES AVAILABLE
#define FTN_NAME(a)                               a##_



// JMA typedefs for standard type names of things
#ifdef __cplusplus
#  include <cstddef>
#else
#  include <stddef.h>
#endif
#ifndef __STDC_CONSTANT_MACROS
#  define __STDC_CONSTANT_MACROS
#endif
#ifndef __STDC_LIMIT_MACROS
#  define __STDC_LIMIT_MACROS
#endif
#include <stdint.h>
// Exact types
typedef  int8_t                     Sint8;
typedef uint8_t                     Uint8;
typedef  int16_t                    Sint16;
typedef uint16_t                    Uint16;
typedef  int32_t                    Sint32;
typedef uint32_t                    Uint32;
typedef  int64_t                    Sint64;
typedef uint64_t                    Uint64;
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) \
    && (defined(__i386__) || defined(__x86_64__) || defined(__ia64__))
#  if ((defined(__LP64__) && !defined(__hppa__)) || defined(_WIN64)) 
typedef signed __int128             int128_t;
typedef unsigned __int128           uint128_t;
typedef int128_t                    int_fast128_t;
typedef uint128_t                   uint_fast128_t;
typedef int128_t                    int_least128_t;
typedef uint128_t                   uint_least128_t;
typedef int128_t                    Sint128;
typedef uint128_t                   Uint128;
#  define JMA_CODE_HAVE_NATIVE_128_BIT_INT
#  endif
#endif

typedef float                       Real32;
typedef double                      Real64;
typedef long double                 Real80;
typedef float                       Real32_t;
typedef double                      Real64_t;
typedef long double                 Real80_t;

#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) \
    && (defined(__i386__) || defined(__x86_64__) || defined(__ia64__))
#  include <quadmath.h>
typedef __float128                  Real128_t;
#  define JMA_CODE_HAVE_NATIVE_128_BIT_FLOAT
#endif

// non exact types

typedef ptrdiff_t                   Index_t;

// FORTRAN INTERFACE
// integer should be the size of a real, which is probably 32 bits
typedef int32_t                     Finteger;






#endif // JMA_CODE_H

