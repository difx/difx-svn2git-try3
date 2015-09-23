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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <limits>
#include <cmath>

#include "DelayTimestamp.h"

namespace DiFX {
namespace Delay {
namespace Time {






// GLOBALS
namespace {
// JD (Julian Date) 0 point starts at 12:00 UTC noon
// MJD (Modified Julian Date) 1858-11-17T00:00
// Unix timestamp 1970-01-01T00:00
// J2000        2000-01-01T12:00 TT.  To 1 ms accuracy, 
//              2000-01-01T11:58:55.816 UTC
//              2000-01-01T11:59:27.816 TAI
//
// const int64_t SEC_DAY_INT = 86400;
// const double SEC_DAY_DBL = 86400.0;
const double JD_J2000      = 2451545.000000;
const double MJD_J2000     =   51544.5;
const double JD_MJD_OFFSET = 2400000.5;
const double JD_UNIX_0     = 2440587.5;
const double MJD_UNIX_0    =   40587.0;
const int64_t MJD_SEC_UNIX_0 = INT64_C(3506716800); // difference in seconds
}

// FUNCTIONS

DelayTimestamp::DelayTimestamp(int64_t i, double f, enum DelayTimestampTypeEnum t) throw()
{
	switch(t) {
	case DIFX_TIME_SYSTEM_UTC:
	case DIFX_TIME_SYSTEM_TAI:
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
		break;
	case DIFX_TIME_SYSTEM_MJD:
		{
			int64_t mi = i*SEC_DAY_INT;
			if((!std::isinf(f)) && (!std::isnan(f)))
			{
				// check for +0.0 and -0.0
				if(f != 0.0)
				{
					double mf = f*SEC_DAY_DBL;
					double mf_ = std::floor(mf);
					double mff = f*SEC_DAY_DBL-mf_;// redo computation for those
					                               // computers with FMA
					double mff_ = std::floor(mff);
					double mfff = mff-mff_;
					i_ = mi + int64_t(mf_ + mff_);
					f_ = mfff;
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
		}
		break;
	case DIFX_TIME_SYSTEM_UNIX:
		i_=i_+MJD_SEC_UNIX_0;
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
		break;
	}
	return;
}
DelayTimestamp::DelayTimestamp(struct tm* const tm, double f) throw()
{
	time_t ret;
	char *tz;

	tz = getenv("TZ");
	setenv("TZ", "", 1);
	tzset();
	ret = mktime(tm);
	if (tz)
		setenv("TZ", tz, 1);
	else
		unsetenv("TZ");
	tzset();
	i_=ret+MJD_SEC_UNIX_0;
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


DelayTimestamp DelayTimestamp::UTC_to_TAI() const throw()
{
	DelayTimestamp Delta = TAI_UTC(DIFX_TIME_SYSTEM_UTC);
	int64_t ti = i_ + Delta.i();
	double tf = f_ + Delta.f();
	double tf_ = std::floor(tf);
	ti += int64_t(tf_);
	tf -= tf_;
	return DelayTimestamp(ti,tf,true);
}
DelayTimestamp DelayTimestamp::TAI_to_UTC() const throw()
{
	DelayTimestamp Delta = TAI_UTC(DIFX_TIME_SYSTEM_TAI);
	int64_t ti = i_ - Delta.i();
	double tf = f_ - Delta.f();
	double tf_ = std::floor(tf);
	ti += int64_t(tf_);
	tf -= tf_;
	return DelayTimestamp(ti,tf,true);
}


// Returns TAI-UTC, if the current timestamp is UTC or TAI
// See http://maia.usno.navy.mil/ser7/tai-utc.dat
DelayTimestamp DelayTimestamp::TAI_UTC(enum DelayTimestampTypeEnum t) const throw()
{
	int64_t i=0;
	double f=0.0;
	switch(t) {
	case DIFX_TIME_SYSTEM_UTC:
		{
			if(i_>= INT64_C(4942425600))
			{
				i = 36;
			}
			else if(i_>= INT64_C(4847817600))
			{
				i = 35;
			}
			else if(i_>= INT64_C(4737484800))
			{
				i = 34;
			}
			else if(i_>= INT64_C(4642790400))
			{
				i = 33;
			}
			else if(i_>= INT64_C(4421865600))
			{
				i = 32;
			}
			else if(i_>= INT64_C(4374432000))
			{
				i = 31;
			}
			else if(i_>= INT64_C(4327171200))
			{
				i = 30;
			}
			else if(i_>= INT64_C(4279737600))
			{
				i = 29;
			}
			else if(i_>= INT64_C(4248201600))
			{
				i = 28;
			}
			else if(i_>= INT64_C(4216665600))
			{
				i = 27;
			}
			else if(i_>= INT64_C(4169404800))
			{
				i = 26;
			}
			else if(i_>= INT64_C(4137868800))
			{
				i = 25;
			}
			else if(i_>= INT64_C(4074710400))
			{
				i = 24;
			}
			else if(i_>= INT64_C(3995740800))
			{
				i = 23;
			}
			else if(i_>= INT64_C(3932582400))
			{
				i = 22;
			}
			else if(i_>= INT64_C(3901046400))
			{
				i = 21;
			}
			else if(i_>= INT64_C(3869510400))
			{
				i = 20;
			}
			else if(i_>= INT64_C(3822249600))
			{
				i = 19;
			}
			else if(i_>= INT64_C(3790713600))
			{
				i = 18;
			}
			else if(i_>= INT64_C(3759177600))
			{
				i = 17;
			}
			else if(i_>= INT64_C(3727641600))
			{
				i = 16;
			}
			else if(i_>= INT64_C(3696019200))
			{
				i = 15;
			}
			else if(i_>= INT64_C(3664483200))
			{
				i = 14;
			}
			else if(i_>= INT64_C(3632947200))
			{
				i = 13;
			}
			else if(i_>= INT64_C(3601411200))
			{
				i = 12;
			}
			else if(i_>= INT64_C(3585513600))
			{
				i = 11;
			}
			else if(i_>= INT64_C(3569788800))
			{
				i = 10;
			}
			else if(i_>= INT64_C(3222720000))
			{
				// Change in how things work
				int64_t MJDi;
				double MJDf;
				int64_t C=0;
				double F=0.0;
				to_MJD(MJDi, MJDf);
		
				if(i_>= INT64_C(3446236800))
				{
					i = 4;
					f = 0.21317;
					C = 39126;
					F = 0.002592;
				}
				else if(i_>= INT64_C(3380486400))
				{
					i = 4;
					f = 0.31317;
					C = 39126;
					F = 0.002592;
				}
				else if(i_>= INT64_C(3369945600))
				{
					i = 3;
					f = 0.84013;
					C = 38761;
					F = 0.001296;
				}
				else if(i_>= INT64_C(3364588800))
				{
					i = 3;
					f = 0.74013;
					C = 38761;
					F = 0.001296;
				}
				else if(i_>= INT64_C(3354048000))
				{
					i = 3;
					f = 0.64013;
					C = 38761;
					F = 0.001296;
				}
				else if(i_>= INT64_C(3348950400))
				{
					i = 3;
					f = 0.54013;
					C = 38761;
					F = 0.001296;
				}
				else if(i_>= INT64_C(3338409600))
				{
					i = 3;
					f = 0.44013;
					C = 38761;
					F = 0.001296;
				}
				else if(i_>= INT64_C(3325190400))
				{
					i = 3;
					f = 0.34013;
					C = 38761;
					F = 0.001296;
				}
				else if(i_>= INT64_C(3317328000))
				{
					i = 3;
					f = 0.24013;
					C = 38761;
					F = 0.001296;
				}
				else if(i_>= INT64_C(3312057600))
				{
					i = 1;
					f = 0.945858;
					C = 37665;
					F = 0.0011232;
				}
				else if(i_>= INT64_C(3254256000))
				{
					i = 1;
					f = 0.845858;
					C = 37665;
					F = 0.0011232;
				}
				else if(i_>= INT64_C(3241036800))
				{
					i = 1;
					f = 0.372818;
					C = 37300;
					F = 0.001296;
				}
				else if(i_>= INT64_C(3222720000))
				{
					i = 1;
					f = 0.422818;
					C = 37300;
					F = 0.001296;
				}
				double d = ((MJDi-C)+MJDf)*F;
				double di = std::floor(d);
				i += int64_t(di);
				f += d-di;
				double fi = std::floor(f);
				i += int64_t(fi);
				f -= fi;
			}
			else
			{
				i = 1;
				f = 0.422818;
			}
		}
		break;
	case DIFX_TIME_SYSTEM_TAI:
		{
			if(i_>= INT64_C(4942425636))
			{
				i = 36;
			}
			else if(i_>= INT64_C(4847817635))
			{
				i = 35;
			}
			else if(i_>= INT64_C(4737484834))
			{
				i = 34;
			}
			else if(i_>= INT64_C(4642790433))
			{
				i = 33;
			}
			else if(i_>= INT64_C(4421865632))
			{
				i = 32;
			}
			else if(i_>= INT64_C(4374432031))
			{
				i = 31;
			}
			else if(i_>= INT64_C(4327171230))
			{
				i = 30;
			}
			else if(i_>= INT64_C(4279737629))
			{
				i = 29;
			}
			else if(i_>= INT64_C(4248201628))
			{
				i = 28;
			}
			else if(i_>= INT64_C(4216665627))
			{
				i = 27;
			}
			else if(i_>= INT64_C(4169404826))
			{
				i = 26;
			}
			else if(i_>= INT64_C(4137868825))
			{
				i = 25;
			}
			else if(i_>= INT64_C(4074710424))
			{
				i = 24;
			}
			else if(i_>= INT64_C(3995740823))
			{
				i = 23;
			}
			else if(i_>= INT64_C(3932582422))
			{
				i = 22;
			}
			else if(i_>= INT64_C(3901046421))
			{
				i = 21;
			}
			else if(i_>= INT64_C(3869510420))
			{
				i = 20;
			}
			else if(i_>= INT64_C(3822249619))
			{
				i = 19;
			}
			else if(i_>= INT64_C(3790713618))
			{
				i = 18;
			}
			else if(i_>= INT64_C(3759177617))
			{
				i = 17;
			}
			else if(i_>= INT64_C(3727641616))
			{
				i = 16;
			}
			else if(i_>= INT64_C(3696019215))
			{
				i = 15;
			}
			else if(i_>= INT64_C(3664483214))
			{
				i = 14;
			}
			else if(i_>= INT64_C(3632947213))
			{
				i = 13;
			}
			else if(i_>= INT64_C(3601411212))
			{
				i = 12;
			}
			else if(i_>= INT64_C(3585513611))
			{
				i = 11;
			}
			else if(i_>= INT64_C(3569788810))
			{
				i = 10;
			}
			else if((i_-INT64_C(3222720001))+(f_-0.4228181838989258) >= 0.0)
			{
				// Change in how things work
				int64_t MJDi;
				double MJDf;
				int64_t C=0;
				double F=0.0;
				to_MJD(MJDi, MJDf);
		
				if((i_-INT64_C(3446236806))+(f_-0.18568181991577148) >= 0.0)
				{
					i = 4;
					f = 0.21317;
					C = 39126;
					F = 0.002592;
				}
				else if((i_-INT64_C(3380486404))+(f_-0.3131699562072754) >= 0.0)
				{
					i = 4;
					f = 0.31317;
					C = 39126;
					F = 0.002592;
				}
				else if((i_-INT64_C(3369945604))+(f_-0.1550579071044922) >= 0.0)
				{
					i = 3;
					f = 0.84013;
					C = 38761;
					F = 0.001296;
				}
				else if((i_-INT64_C(3364588803))+(f_-0.9747061729431152) >= 0.0)
				{
					i = 3;
					f = 0.74013;
					C = 38761;
					F = 0.001296;
				}
				else if((i_-INT64_C(3354048003))+(f_-0.7165942192077637) >= 0.0)
				{
					i = 3;
					f = 0.64013;
					C = 38761;
					F = 0.001296;
				}
				else if((i_-INT64_C(3348950403))+(f_-0.5401301383972168) >= 0.0)
				{
					i = 3;
					f = 0.54013;
					C = 38761;
					F = 0.001296;
				}
				else if((i_-INT64_C(3338409603))+(f_-0.28201818466186523) >= 0.0)
				{
					i = 3;
					f = 0.44013;
					C = 38761;
					F = 0.001296;
				}
				else if((i_-INT64_C(3325190402))+(f_-0.9837298393249512) >= 0.0)
				{
					i = 3;
					f = 0.34013;
					C = 38761;
					F = 0.001296;
				}
				else if((i_-INT64_C(3317328002))+(f_-0.7657938003540039) >= 0.0)
				{
					i = 3;
					f = 0.24013;
					C = 38761;
					F = 0.001296;
				}
				else if((i_-INT64_C(3312057602))+(f_-0.6972789764404297) >= 0.0)
				{
					i = 1;
					f = 0.945858;
					C = 37665;
					F = 0.0011232;
				}
				else if((i_-INT64_C(3254256001))+(f_-0.845858097076416) >= 0.0)
				{
					i = 1;
					f = 0.845858;
					C = 37665;
					F = 0.0011232;
				}
				else if((i_-INT64_C(3241036801))+(f_-0.6475701332092285) >= 0.0)
				{
					i = 1;
					f = 0.372818;
					C = 37300;
					F = 0.001296;
				}
				else if((i_-INT64_C(3222720001))+(f_-0.4228181838989258) >= 0.0)
				{
					i = 1;
					f = 0.422818;
					C = 37300;
					F = 0.001296;
				}
				double d = ((MJDi-C)+MJDf)*F;
				double di = std::floor(d);
				i += int64_t(di);
				f += d-di;
				double fi = std::floor(f);
				i += int64_t(fi);
				f -= fi;
				// Now correct for MJD UTC to TAI difference
				double denom = 1.0 + F/SEC_DAY_DBL;
				double i_d = i / denom;
				double i_d_ = std::floor(i_d);
				double i_df = i_d - i_d_;
				double f_d = f / denom;
				double f2 = f_d + i_df;
				double f2_ = std::floor(f2);
				double f2f = f2 - f2_;
				i = int64_t(i_d_ + f2_);
				f = f2f;
			}
			else
			{
				i = 1;
				f = 0.422818;
			}
		}
		break;
	default:
		break;
	}
	return DelayTimestamp(i,f,true);
}

// From: http://maia.usno.navy.mil/ser7/tai-utc.dat
 // 1961 JAN  1 =JD 2437300.5  TAI-UTC=   1.4228180 S + (MJD - 37300.) X 0.001296 S
 // 1961 AUG  1 =JD 2437512.5  TAI-UTC=   1.3728180 S + (MJD - 37300.) X 0.001296 S
 // 1962 JAN  1 =JD 2437665.5  TAI-UTC=   1.8458580 S + (MJD - 37665.) X 0.0011232S
 // 1963 NOV  1 =JD 2438334.5  TAI-UTC=   1.9458580 S + (MJD - 37665.) X 0.0011232S
 // 1964 JAN  1 =JD 2438395.5  TAI-UTC=   3.2401300 S + (MJD - 38761.) X 0.001296 S
 // 1964 APR  1 =JD 2438486.5  TAI-UTC=   3.3401300 S + (MJD - 38761.) X 0.001296 S
 // 1964 SEP  1 =JD 2438639.5  TAI-UTC=   3.4401300 S + (MJD - 38761.) X 0.001296 S
 // 1965 JAN  1 =JD 2438761.5  TAI-UTC=   3.5401300 S + (MJD - 38761.) X 0.001296 S
 // 1965 MAR  1 =JD 2438820.5  TAI-UTC=   3.6401300 S + (MJD - 38761.) X 0.001296 S
 // 1965 JUL  1 =JD 2438942.5  TAI-UTC=   3.7401300 S + (MJD - 38761.) X 0.001296 S
 // 1965 SEP  1 =JD 2439004.5  TAI-UTC=   3.8401300 S + (MJD - 38761.) X 0.001296 S
 // 1966 JAN  1 =JD 2439126.5  TAI-UTC=   4.3131700 S + (MJD - 39126.) X 0.002592 S
 // 1968 FEB  1 =JD 2439887.5  TAI-UTC=   4.2131700 S + (MJD - 39126.) X 0.002592 S
 // 1972 JAN  1 =JD 2441317.5  TAI-UTC=  10.0       S + (MJD - 41317.) X 0.0      S
 // 1972 JUL  1 =JD 2441499.5  TAI-UTC=  11.0       S + (MJD - 41317.) X 0.0      S
 // 1973 JAN  1 =JD 2441683.5  TAI-UTC=  12.0       S + (MJD - 41317.) X 0.0      S
 // 1974 JAN  1 =JD 2442048.5  TAI-UTC=  13.0       S + (MJD - 41317.) X 0.0      S
 // 1975 JAN  1 =JD 2442413.5  TAI-UTC=  14.0       S + (MJD - 41317.) X 0.0      S
 // 1976 JAN  1 =JD 2442778.5  TAI-UTC=  15.0       S + (MJD - 41317.) X 0.0      S
 // 1977 JAN  1 =JD 2443144.5  TAI-UTC=  16.0       S + (MJD - 41317.) X 0.0      S
 // 1978 JAN  1 =JD 2443509.5  TAI-UTC=  17.0       S + (MJD - 41317.) X 0.0      S
 // 1979 JAN  1 =JD 2443874.5  TAI-UTC=  18.0       S + (MJD - 41317.) X 0.0      S
 // 1980 JAN  1 =JD 2444239.5  TAI-UTC=  19.0       S + (MJD - 41317.) X 0.0      S
 // 1981 JUL  1 =JD 2444786.5  TAI-UTC=  20.0       S + (MJD - 41317.) X 0.0      S
 // 1982 JUL  1 =JD 2445151.5  TAI-UTC=  21.0       S + (MJD - 41317.) X 0.0      S
 // 1983 JUL  1 =JD 2445516.5  TAI-UTC=  22.0       S + (MJD - 41317.) X 0.0      S
 // 1985 JUL  1 =JD 2446247.5  TAI-UTC=  23.0       S + (MJD - 41317.) X 0.0      S
 // 1988 JAN  1 =JD 2447161.5  TAI-UTC=  24.0       S + (MJD - 41317.) X 0.0      S
 // 1990 JAN  1 =JD 2447892.5  TAI-UTC=  25.0       S + (MJD - 41317.) X 0.0      S
 // 1991 JAN  1 =JD 2448257.5  TAI-UTC=  26.0       S + (MJD - 41317.) X 0.0      S
 // 1992 JUL  1 =JD 2448804.5  TAI-UTC=  27.0       S + (MJD - 41317.) X 0.0      S
 // 1993 JUL  1 =JD 2449169.5  TAI-UTC=  28.0       S + (MJD - 41317.) X 0.0      S
 // 1994 JUL  1 =JD 2449534.5  TAI-UTC=  29.0       S + (MJD - 41317.) X 0.0      S
 // 1996 JAN  1 =JD 2450083.5  TAI-UTC=  30.0       S + (MJD - 41317.) X 0.0      S
 // 1997 JUL  1 =JD 2450630.5  TAI-UTC=  31.0       S + (MJD - 41317.) X 0.0      S
 // 1999 JAN  1 =JD 2451179.5  TAI-UTC=  32.0       S + (MJD - 41317.) X 0.0      S
 // 2006 JAN  1 =JD 2453736.5  TAI-UTC=  33.0       S + (MJD - 41317.) X 0.0      S
 // 2009 JAN  1 =JD 2454832.5  TAI-UTC=  34.0       S + (MJD - 41317.) X 0.0      S
 // 2012 JUL  1 =JD 2456109.5  TAI-UTC=  35.0       S + (MJD - 41317.) X 0.0      S
 // 2015 JUL  1 =JD 2457204.5  TAI-UTC=  36.0       S + (MJD - 41317.) X 0.0      S

// Python code to get numbers for code above
// import math
// fp=open("tai-utc.dat","r")
// for line in fp:
//     JD=float(line[17:26])
//     MJD=JD-2400000.5
//     o=float(line[36:49])
//     MJDb=float(line[60:66])
//     f=float(line[69:79])
//     us=MJD*86400
//     Delta=o + (MJD-MJDb)*f
//     us_tai=us+Delta
//     MJDt=MJD+Delta/86400.
//     usi=int(us)
//     usti=int(us_tai)
//     ustf=us_tai-usti
//     print(JD, MJD, o, MJDb, f, usi, usti, ustf)







}  // end namespace Time
}  // end namespace Delay
}  // end namespace DiFX


