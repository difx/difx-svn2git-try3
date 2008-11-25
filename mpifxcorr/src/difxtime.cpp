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

#include "difxtime.h"

//CONSTRUCTORS
Difxtime::Difxtime(s32 upper, s32 middle, s32 lower)
{
  this->upper = upper;
  this->middle = middle;
  this->lower = lower;
  normalise();
}

Difxtime::Difxtime(f64 val)
{
  upper = static_cast<s32>(val);
  middle = static_cast<s32>((val-f64(upper))*1.0e9);
  lower = static_cast<s32>((val - ((f64)middle)/1.0e9)*1.0e18);
}

Difxtime::Difxtime(s32 intval, f64 fracval)
{
  if(fracval >= 1.0 || fracval < 0.0)
    throw FPInitialiseException();

  upper = intval;
  middle = static_cast<s32>(fracval*1.0e9);
  lower = static_cast<s32>((fracval-((f64)middle)/1.0e9)*1.0e18);
  normalise();
}

Difxtime::Difxtime()
{
  upper = 0;
  middle = 0;
  lower = 0;
}

std::ostream& operator<<(std::ostream& s, const Difxtime& i)
{
  int precision = s.precision();
  int width = s.width();
  std::ios_base::fmtflags options = s.flags();
  int base = 10;
  if(options & std::ios_base::hex)
    base = 16;
  else if(options & std::ios_base::oct)
    base = 8;
  bool prefix = options & std::ios_base::showbase;
  bool lower_case = !(options & std::ios_base::uppercase);
  int sign_flag = (options & std::ios_base::showpos) ? -1:0;
  int justification = (options & std::ios_base::right) ? 0 : ((options & std::ios_base::left) ? 1:2);
  char fill = s.fill();
  bool zero_pad = (fill == '0') ? true:false;
  std::string str = difxtimetostring(i, base, sign_flag, prefix, lower_case, precision,
                                     width, zero_pad, justification, fill);
  s << str;
  return s;
}



void Difxtime::normalise()
{
  middle += lower/1000000000;
  lower %= 1000000000;
  upper += middle/1000000000;
  middle %= 1000000000;
  if ((upper >= 0) && (middle >= 0) && (lower >= 0) ||
      (upper <= 0) && (middle <= 0) && (lower <= 0))
    return;
  if (upper == 0) {
    if (lower < 0) {
      lower += 1000000000; middle--;
    }
    else {
      lower -= 1000000000; middle++;
    }
    return;
  }
  if (upper > 0) {
    if (lower < 0) {
      lower += 1000000000; middle--;
    }
    else {
      middle += 1000000000; upper--;
    }
  }
  else {
    if (lower > 0) {
      lower -= 1000000000; middle++;
    }
    else {
      middle -= 1000000000; upper++;
    }
  }
}

// EXCEPTION CLASS "WHAT" METHOD DEFINITIONS
const char* FPException::what()
{
  return "General fixed point exception has occurred";
}

const char* FPInitialiseException::what()
{
  return "Attempt to initialise a fixed point number with bad values";
}

const char* FPOverflowException::what()
{
  return "Fixed point overflow has occurred";
}

