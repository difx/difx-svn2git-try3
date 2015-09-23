/***************************************************************************
*   Copyright (C) 2015 by James M Anderson                                *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 3 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/


#ifndef DelayTimestamp_h
#define DelayTimestamp_h

// INCLUDES
#ifndef __STDC_FORMAT_MACROS       // Defines for broken C++ implementations
#  define __STDC_FORMAT_MACROS
#endif
#ifndef __STDC_CONSTANT_MACROS
#  define __STDC_CONSTANT_MACROS
#endif
#ifndef __STDC_LIMIT_MACROS
#  define __STDC_LIMIT_MACROS
#endif
#ifndef _ISOC99_SOURCE
#  define _ISOC99_SOURCE
#endif
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif
#ifndef __USE_ISOC99
#  define __USE_ISOC99 1
#endif
#ifndef _ISOC99_SOURCE
#  define _ISOC99_SOURCE
#endif
#ifndef __USE_MISC
#  define __USE_MISC 1
#endif
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include <inttypes.h>
#include <limits>
#ifdef __cplusplus
#  include <cstddef>
#else
#  include <stddef.h>
#endif
#include <stdint.h>
// we want to use ISO C9X stuff
// we want to use some GNU stuff
// But this sometimes breaks time.h
#include <time.h>
#include <cmath>


namespace DiFX {
namespace Delay {
namespace Time {

#ifndef SEC_DAY_INT
# define SEC_DAY_INT 86400
#endif
#ifndef SEC_DAY_DBL
#  define SEC_DAY_DBL 86400.0
#endif

enum DelayTimestampTypeEnum
	{
		DIFX_TIME_SYSTEM_UTC=0,
		DIFX_TIME_SYSTEM_TAI,
		DIFX_TIME_SYSTEM_MJD,
		DIFX_TIME_SYSTEM_UNIX
	};

class DelayTimestamp{

public:
	// The default value is MJD=0.0
	DelayTimestamp() throw() {i_=0;f_=0;return;}
	DelayTimestamp(int64_t i, double f) throw()
	{
		i_=i;
		if((!std::isinf(f)) && (!std::isnan(f)))
		{
			// check for +0.0 and -0.0
			if(f != 0.0)
			{
				double ff = std::floor(f);
				i_ += int64_t(ff);
				f_=f - ff;
			}
			else
			{
				f_ = 0.0;
			}
		}
		else
		{
			f_ = f;
		}
		return;
	}
	DelayTimestamp(int64_t i, double f, enum DelayTimestampTypeEnum t) throw();
	DelayTimestamp(struct tm* const tm, double f) throw();

	int64_t i() const throw() {return i_;}
	double f() const throw() {return f_;}
	

	// Return an integer MJD plus floating point fraction of day
	void to_MJD(int64_t& MJDi, double& MJDf) const throw()
	{
		int64_t mi = i_ / SEC_DAY_INT;
		int64_t r = i_ % SEC_DAY_INT;
		if(r < 0)
		{
			--mi;
			r += SEC_DAY_INT;
		}
		double mf = (f_ + r) / SEC_DAY_DBL;
		if(mf == 1.0)
		{
			mf = 0.0;
			++mi;
		}
		MJDi = mi;
		MJDf = mf;
		return;
	}
	// Return an integer UNIX timestamp plus floating point fraction of second
	void to_UNIX(int64_t& UNIXi, double& UNIXf) const throw()
	{
		UNIXi = i_ - INT64_C(3506716800); // constant is MJD_SEC_UNIX_0,
		                                  // the number of seconds since MJD=0
		                                  // at the Unix 0 timestamp
		UNIXf = f_;
		return;
	}
	// Returns TAI-UTC, if the current timestamp is UTC or TAI
	DelayTimestamp TAI_UTC(enum DelayTimestampTypeEnum t) const throw();
	
	// Convert from UTC time to TAI time
	DelayTimestamp UTC_to_TAI() const throw();
	// Convert from TAI time to UTC time
	DelayTimestamp TAI_to_UTC() const throw();
	




protected:
	int64_t i_;
	double f_;


private:
	// Use this fucntion for when f is guaranteed to be in range
	DelayTimestamp(int64_t i, double f, bool b) throw() {i_=i;f_=f;return;}
};


// CLASS FUNCTIONS



// HELPER FUNCTIONS




}  // end namespace Time
}  // end namespace Delay
}  // end namespace DiFX

#endif // DelayTimestamp_h
