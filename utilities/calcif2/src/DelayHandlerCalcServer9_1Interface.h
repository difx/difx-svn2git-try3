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

// Note: This file is based on the CALCServer.h file from the
// original CALC5Server.c program


#ifndef DelayHandlerCalcServer9_1Interface_h
#define DelayHandlerCalcServer9_1Interface_h

// INCLUDES
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
#include <pthread.h>

#ifdef __cplusplus
namespace DiFX {
namespace Delay {
namespace Handler {
extern "C" {
#endif


#define DIFX_DELAYHANDLERCALCSERVER9_1_STRING_SIZE 32
#define NUM_DIFX_DELAYHANDLERCALCSERVER9_1_KFLAGS 64
#define NUM_DIFX_DELAYHANDLERCALCSERVER9_1_EOPS 5

struct CALCSERVER9_1_MODEL_DELAY_ARGUMENT {
	int64_t request_id;     /* RPC request id number, user's choice */
	int64_t date;           /* CALC model date (MJD) */
	int64_t ref_frame;      /* CALC reference frame: 0 = geocentric */
    int32_t verbosity;      /* How verbose should logging be?
						       Higher means more messages */
	int32_t dummy;
	int16_t kflags[NUM_DIFX_DELAYHANDLERCALCSERVER9_1_KFLAGS];
	                        /* CALC model component control flags */

	double time;            /* CALC model time UTC (fraction of day) */

	double a_x;             /* geocentric right-hand x coord (meters) */
	double a_y;             /* geocentric right-hand y coord (meters) */
	double a_z;             /* geocentric right-hand z coord (meters) */
	double axis_off_a;      /* non-intersecting axis offset (meters) */
	double b_x;             /* geocentric right-hand x coord (meters) */
	double b_y;             /* geocentric right-hand y coord (meters) */ 
	double b_z;             /* geocentric right-hand z coord (meters) */
	double axis_off_b;      /* non-intersecting axis offset (meters) */
	double ra;              /* J2000.0 coordinates (radians) */
	double dec;             /* J2000.0 coordinates (radians) */
	double dra;             /* J2000.0 arcsecs/year */
	double ddec;            /* J2000.0 arcsecs/year */
	double depoch;          /* reference date for which proper motion
                             * corrections are zero, mjd.fract_day */
	double parallax;        /* source distance in arcsecs of annual
                             * parallax, = 206265.0 / distance (au) */

    /* Earth Orientation Parameters */
	double EOP_time[NUM_DIFX_DELAYHANDLERCALCSERVER9_1_EOPS];
	                        /* EOP epoch date.time (MJD) */
	double tai_utc[NUM_DIFX_DELAYHANDLERCALCSERVER9_1_EOPS];
	                        /* TAI - UTC (secs) */
	double ut1_utc[NUM_DIFX_DELAYHANDLERCALCSERVER9_1_EOPS];
	                        /* UT1 - UTC (secs) */
	double xpole[NUM_DIFX_DELAYHANDLERCALCSERVER9_1_EOPS];
	                        /* earth pole offset, x (arcsec) */
	double ypole[NUM_DIFX_DELAYHANDLERCALCSERVER9_1_EOPS];
	                        /* earth pole offset, y (arcsecs) */
     
	double pressure_a;      /* surface pressure stna (millibars) 
                             * enter 0.0 for none availiable */
	double pressure_b;      /* surface pressure stnb (millibars) 
                             * enter 0.0 for none availiable */

	char  station_a[DIFX_DELAYHANDLERCALCSERVER9_1_STRING_SIZE];
	                        /* station A name */
	char  axis_type_a[DIFX_DELAYHANDLERCALCSERVER9_1_STRING_SIZE];
	                        /* station A mount type, 'altz', 'equa',
                               ,xyns', 'xyew' */
	char  station_b[DIFX_DELAYHANDLERCALCSERVER9_1_STRING_SIZE];
	                        /* station B name */
	char  axis_type_b[DIFX_DELAYHANDLERCALCSERVER9_1_STRING_SIZE];
	                        /* station B mount type, 'altz', 'equa',
                               'xyns', 'xyew' */
	char  source[DIFX_DELAYHANDLERCALCSERVER9_1_STRING_SIZE];
	                        /* source name */
};




struct CALCSERVER9_1_MODEL_DELAY_RESPONSE {
	int64_t request_id;     /* RPC request id number, returned by Server */
	int64_t date;           /* CALC model date (MJD) */
	double time;            /* CALC model time UTC (fraction of day) */
    uint64_t server_version;
	/* The exact version number of the delay server
	   software that was used.
	*/
	double delay[2];        /* total group delay, rate (secs, sec/sec) */
	double dry_atmos[4];    /* dry atmosphere delay, rate (secs, sec/sec) */
	double wet_atmos[4];    /* wet atmosphere delay, rate (secs, sec/sec) */
	double az[2];           /* stations azimuth angle, rate (rad, rad/sec) */
	double el[2];           /* station elevation angle, rate (rad, rad/sec) */
	double UV[3];           /* u, v, w coordinates in J2000.0 frame (meters) */
};






struct CALCSERVER9_1_MODEL_PARAMETERS_ARGUMENT {
    /*************************************************************************/
    /*** Setup ***************************************************************/
    int64_t request_id;        /* request id number, user's choice          */
    int64_t server_struct_setup_code;
	/* Server struct code. (struct_code in           */
	/* the original servers.) This specifies         */
	/* which of the elements of this
	   structure are actually sent by the RPC
	   call.  Elements that are not sent are
	   automatically set to 0.  See
	   the individual server codes for allowed
	   options.
	*/
    int32_t verbosity; /* How verbose should logging be?
						  Higher means more messages */
};


struct CALCSERVER9_1_MODEL_PARAMETERS_RESPONSE {
    /*************************************************************************/
    /*** Setup ***************************************************************/
	int32_t handler_error;  /* error code from the DelayHandlerDistributor */
    int32_t rpc_handler_error; /* error code from an RPC delay handler     */
    int32_t server_error;   /* error code from the called delay server program*/
    int32_t model_error;    /* error code from the underlying delay modeling
                               software */
    int64_t request_id;     /* request id number, returned to user         */
    int64_t server_struct_setup_code;
	/* Server struct code. (struct_code in           */
	/* the original servers.) This specifies         */
	/* which of the elements of this
	   structure are actually sent by the RPC
	   call.  Elements that are not sent are
	   automatically set to 0.  See
	   the individual server codes for allowed
	   options.
	*/
    uint64_t server_version;
	/* The exact version number of the delay server
	   software that was used.
	*/
    /*************************************************************************/
    /*** Results *************************************************************/
    double accelgrv;        /* acceleration of gravity at Earth's surface
                               in m s^{-2}
							*/
    double e_flat;          /* Earth's flattening factor, unitless.
                               This is the square of the eccentricity
                               of the ellipsoid which approximates the shape
                               of the Earth.
							*/
    double earthrad;        /* Earth's equatorial radius, in m
                             */
    double mmsems;          /* ratio of the mass of the Moon to the mass
                               of the Earth
                            */
    double ephepoc;         /* coordinate equinox (usually 2000.0)
                             */
    double gauss;           /* Gaussian gravitational constant in
                               AU^{3/2} day^{-1} M_\Sun^{-1/2}
							*/
    double u_grv_cn;        /* constant of gravitation, in
                               m^3 kg^{-1} s^{-2}
                            */
    double gmsun;           /* Heliocentric gravitational constant,
                               mass of the Sun times the Newtonian graviational
                               constant, in m^3 s^{-2}
                            */
    double gmmercury;       /* Mass of Mercury times the Newtonian graviational
                               constant, in m^3 s^{-3}
                            */
    double gmvenus;         /* Mass of Venus times the Newtonian graviational
                               constant, in m^3 s^{-3}
                            */
    double gmearth;         /* Mass of Earth times the Newtonian graviational
                               constant, in m^3 s^{-3}
                            */
    double gmmoon;          /* Lunar-centric gravitational constant,
                               mass of the Moon times the Newtonian graviational
                               constant, in m^3 s^{-3}
                            */
    double gmmars;          /* Mass of Mars times the Newtonian graviational
                               constant, in m^3 s^{-3}
                            */
    double gmjupiter;       /* Mass of Jupiter times the Newtonian graviational
                               constant, in m^3 s^{-3}
                            */
    double gmsaturn;        /* Mass of Saturn times the Newtonian graviational
                               constant, in m^3 s^{-3}
                            */
    double gmuranus;        /* Mass of Uranus times the Newtonian graviational
                               constant, in m^3 s^{-3}
                            */
    double gmneptune;       /* Mass of Neptune times the Newtonian graviational
                               constant, in m^3 s^{-3}
                            */
    double etidelag;        /* lag angle of Earth tides, in radians
                             */
    double love_h;          /* Earth tides: global Love Number H
                             */
    double love_l;          /* Earth tides: global Love Number L
                             */
    double pre_data;        /* general precession in longitude at standard
                               equinox J2000, in arcseconds per Julian century
                            */
    double rel_data;        /* Post-Newtonian expansion parameter
                             */
    double tidalut1;        /* ???
                             */
    double au;              /* size of an astronomical unit, in m
                             */
    double tsecau;          /* light travel time for 1 astronomical unit, in s
                             */
    double vlight;          /* speed of light, in m s^{-1}
                             */
};



#ifdef __cplusplus
}  // extern "C"
}  // end namespace Handler
}  // end namespace Delay
}  // end namespace DiFX
#endif


// CLASS FUNCTIONS



// HELPER FUNCTIONS


#endif // DelayHandlerCalcServer9_1Interface_h
