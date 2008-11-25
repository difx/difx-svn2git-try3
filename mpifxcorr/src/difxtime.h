/***************************************************************************
*   Copyright (C) 2008 by James M Anderson <anderson@mpifr-bonn.mpg.de>   *
*   Adapted from FxTime to DifxTime by Adam Deller (2008)                 *
*                                                                         *
*   This program is free for non-commercial use: see the license file     *
*   at http://astronomy.swin.edu.au:~adeller/software/difx/ for more      *
*   details.                                                              *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
***************************************************************************/
//===========================================================================
// SVN properties (DO NOT CHANGE)
//
// $Id$
// $HeadURL: $
// $LastChangedRevision$
// $Author$
// $LastChangedDate$
//
//============================================================================

//Fixed point class using 3 integers to achieve +- 2*10^9 at an increment of 1*10^-18.

#ifndef DIFXTIME_H
#define DIFXTIME_H

#include <exception>
#include <fstream>
#include "architecture.h"

/**
@class Difxtime
@brief Uses 3 integers to represent a number between +-2*10^9, at increments of 1*10^-18

Fixed point number class.  Allows addition and subtraction, and multiplication/division only by 32/64 bit integers.
Checks for overflow on multiplication, throws an exception if overflow occurs.

@author Adam Deller
*/
class Difxtime{
public:
 /**
  * Constructor: Three integers fully populates the fixed precision number
  * @param upper The 1->10^9 integer
  * @param middle The 10^-9->1 integer
  * @param lower The 10^-18->10^-9 integer
  */
  Difxtime(s32 upper, s32 middle, s32 lower);

 /**
  * Constructor: One double is converted to fixed point
  * @param val The double value to be represented in fixed point units
  */
  Difxtime(f64 val);

 /**
  * Constructor: Int and double assumed to be integer component and fractional
  * component respectively
  * @param intval The integer component of the number to be represented
  * @param fracval The fractional component of the number to be represented
  */
  Difxtime(s32 intval, f64 fracval);

 /**
  * Null constructor: Value set to zero
  */
  Difxtime();

 /**
  * Destructor
  */
  ~Difxtime();

 /**
  * Returns the integer number value held by upper (thus truncating towards zero)
  * @return Integer value from the fixed point number
  */
  inline s32 intval() { return upper; }

 /**
  * Returns the double precision value which best approximates this number
  * @return Double precision number approximating this number
  */
  inline f64 doubleval() { return (f64)upper + ((f64)middle)/1.0e-9 + ((f64)lower)/1.0e-18; }

 /**
  * Returns the long long number value held in middle and lower
  * @return Fractional part of the number, in long long form
  */
  inline s64 longfracval() { return ((s64)middle)*1000000000 + (s64)lower; }

 /**
  * Returns the double precision value which best approximates the fractional component
  * @return Double precision number approximating this number
  */
  inline f64 doublefracval() { return ((f64)middle)/1.0e-9 + ((f64)lower)/1.0e-18; }

 /**
  * Print to screen
  */
  friend std::ostream& operator<<(std::ostream& s, const Difxtime& i);

 /**
  * Overloaded arithmetic operators for this fixed-point precision class
  */
  friend Difxtime operator * (Difxtime val, s32 a);
  friend Difxtime operator * (Difxtime val, s64 a);
  friend Difxtime operator / (Difxtime val, s32 a);
  friend Difxtime operator / (Difxtime val, s64 a);
  friend Difxtime operator % (Difxtime val, s32 a);
  friend Difxtime operator % (Difxtime val, s64 a);

 /**
  * Inlined, overloaded arithmetic operators for this fixed-point precision class
  */
  friend inline Difxtime operator + (Difxtime a, Difxtime b) {
    return Difxtime(a.upper+b.upper,a.middle+b.middle,a.lower+b.lower);
  }
  friend inline Difxtime operator - (Difxtime a, Difxtime b) {
    return Difxtime(a.upper-b.upper,a.middle-b.middle,a.lower-b.lower);
  }
  inline Difxtime operator-(Difxtime a) {
    return Difxtime(-a.upper,-a.middle,-a.lower);
  }
  friend inline void operator += (Difxtime val) {
    upper += val.upper; middle += val.middle; lower += val.lower;
    normalise();
  }
  friend inline void operator -= (Difxtime val) {
    upper -= val.upper; middle -= val.middle; lower -= val.lower;
    normalise();
  }
  friend inline bool operator == (Difxtime val) {
    return ((upper == val.upper) && (middle == val.middle) && (lower == val.lower));
  }
  friend inline bool operator >= (Difxtime val) {
    if (upper > val.upper) return true;
    if (upper < val.upper) return false;
    if (middle > val.middle) return true;
    if (middle < val.middle) return false;
    return (lower >= val.lower);
  }
  friend inline bool operator <= (Difxtime val) {
    if (upper < val.upper) return true;
    if (upper > val.upper) return false;
    if (middle < val.middle) return true;
    if (middle > val.middle) return false;
    return (lower <= val.lower);
  }
  friend inline bool operator > (Difxtime val) {
    if (upper > val.upper) return true;
    if (upper < val.upper) return false;
    if (middle > val.middle) return true;
    if (middle < val.middle) return false;
    return (lower > val.lower);
  }
  friend inline bool operator < (Difxtime val) {
    if (upper < val.upper) return true;
    if (upper > val.upper) return false;
    if (middle < val.middle) return true;
    if (middle > val.middle) return false;
    return (lower < val.lower);
  }

private:
  void normalise();

  s32 upper, middle, lower;
};


/** Exception classes that are necessary */
class FPException: public std::exception {
public:
  virtual const char* what();
};

class FPInitialiseException: public FPException {
public:
  virtual const char* what();
};

class FPOverflowException: public FPException {
public:
  virtual const char* what();
};

/** Printing formatter */
std::string difxtimetostring(difxtime val, int base=10, int sign_flag=0, bool pref
ix=false, bool lower_case=false, int precision = 0, int width=0, bool zero_pad=f
alse, int justification=0, char fill=' ');

#endif

