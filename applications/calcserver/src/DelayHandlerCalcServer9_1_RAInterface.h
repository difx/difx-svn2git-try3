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


#ifndef DelayHandlercalcserver9_1_RAInterface_h
#define DelayHandlercalcserver9_1_RAInterface_h

// INCLUDES
#ifndef __STDC_FORMAT_MACROS
#  define __STDC_FORMAT_MACROS // For non-compliant C++ compilers
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


#define DIFX_DELAYHANDLERCALCSERVER9_1_RA_STRING_SIZE 32
#define NUM_DIFX_DELAYHANDLERCALCSERVER9_1_RA_KFLAGS 64
#define NUM_DIFX_DELAYHANDLERCALCSERVER9_1_RA_EOPS 5

struct CALCSERVER9_1_RA_MODEL_DELAY_ARGUMENT {
	int64_t request_id;     /* RPC request id number, user's choice */
	int64_t struct_code;    /* CALC_9_1_RA Server struct code.	This specifies
							   which of the elements of this
							   structure are actually sent by the RPC
							   call.  Elements that are not sent are
							   automatically set to 0.	See
							   CALC_9_1_RA_SERVER_STRUCT_CODE_* for allowed
							   options.
							*/
	int64_t date;           /* CALC model date (MJD) */
	int64_t ref_frame;      /* CALC reference frame: 0 = geocentric */
    int32_t verbosity;      /* How verbose should logging be?
						       Higher means more messages */
	int32_t dummy;
	int16_t kflags[NUM_DIFX_DELAYHANDLERCALCSERVER9_1_RA_KFLAGS];
	                        /* CALC model component control flags */

	double time;            /* CALC model time UTC (fraction of day) */

	double a_x;             /* geocentric right-hand x coord (meters) */
	double a_y;             /* geocentric right-hand y coord (meters) */
	double a_z;             /* geocentric right-hand z coord (meters) */
	double a_dx;			/* geocentric right-hand x velocity (m/s) */
	double a_dy;			/* geocentric right-hand y velocity (m/s) */
	double a_dz;			/* geocentric right-hand z velocity (m/s) */
	double a_ddx;			/* geocentric right-hand x acceleration (m/s/s) */
	double a_ddy;			/* geocentric right-hand y acceleration (m/s/s) */
	double a_ddz;			/* geocentric right-hand z acceleration (m/s/s) */
	double axis_off_a;      /* non-intersecting axis offset (meters) */
	double b_x;             /* geocentric right-hand x coord (meters) */
	double b_y;             /* geocentric right-hand y coord (meters) */ 
	double b_z;             /* geocentric right-hand z coord (meters) */
	double b_dx;			/* geocentric right-hand x velocity (m/s) */
	double b_dy;			/* geocentric right-hand y velocity (m/s) */
	double b_dz;			/* geocentric right-hand z velocity (m/s) */
	double b_ddx;			/* geocentric right-hand x acceleration (m/s/s) */
	double b_ddy;			/* geocentric right-hand y acceleration (m/s/s) */
	double b_ddz;			/* geocentric right-hand z acceleration (m/s/s) */
	double axis_off_b;      /* non-intersecting axis offset (meters) */
	double ra;              /* J2000.0 coordinates (radians) */
	double dec;             /* J2000.0 coordinates (radians) */
	double dra;             /* J2000.0 arcsecs/year */
	double ddec;            /* J2000.0 arcsecs/year */
	double depoch;          /* reference date for which proper motion
                             * corrections are zero, mjd.fract_day */
	double parallax;        /* source distance in arcsecs of annual
                             * parallax, = 206265.0 / distance (au) */

	double source_pos[3];	/* J2000 source position vector (m)*/
	double source_vel[3];	/* J2000 source velocity vector (m/s)*/
	double source_epoch;	/* source epoch mjd.fract_day */
	double source_parallax; /* source parallax in arcsec
							   = 206265.0 / distance (au) */
	/* The source_* vectors and scalars
	   are alternate forms of entries for
	   the source position information.
	   For the source vectors, if all
	   values of the position vector are
	   0.0, then the standard ra, dec, and
	   parallax values are used.  If
	   (x^2+y^2+z^2)^{1/2} <= 1.5 m, then
	   the position is assumed to be a unit
	   vector, and the distance of the
	   source is assumed to be very far away.
	   Then, the coordinates are in a
	   J2000 frame centered on the Earth, but
	   with the intertial frame corresponding to
	   the Solar system barycenter.
	   In this case, source_parallax is used
	   If the magnitude
	   of the position vector is greater than
	   1.5 m, then the position vector is the J2000,
	   Earth-centered inertial coordinate frame
	   and no parallax information
	   from the source_parallax value is used,
	   as the source position is assumed to be
	   fully described by the J2000 Earth-center
	   position and velocity values.
	*/
	double pointing_pos_a_x[3];/*J2000 x pointing dir position vector (m)*/
	double pointing_vel_a_x[3];/*J2000 x pointing dir velocity vector (m/s)*/
	double pointing_pos_a_y[3];/*J2000 y pointing dir position vector (m)*/
	double pointing_vel_a_y[3];/*J2000 y pointing dir velocity vector (m/s)*/
	double pointing_pos_a_z[3];/*J2000 z pointing dir position vector (m)*/
	double pointing_vel_a_z[3];/*J2000 z pointing dir velocity vector (m/s)*/
	double pointing_pos_b_x[3];/*J2000 x pointing dir position vector (m)*/
	double pointing_vel_b_x[3];/*J2000 x pointing dir velocity vector (m/s)*/
	double pointing_pos_b_y[3];/*J2000 y pointing dir position vector (m)*/
	double pointing_vel_b_y[3];/*J2000 y pointing dir velocity vector (m/s)*/
	double pointing_pos_b_z[3];/*J2000 z pointing dir position vector (m)*/
	double pointing_vel_b_z[3];/*J2000 z pointing dir velocity vector (m/s)*/
	double pointing_epoch_a;/* pointing dir epoch mjd.fract_day */
	double pointing_epoch_b;/* pointing dir epoch mjd.fract_day */
	double pointing_parallax;/* pointing dir parallax in arcsec
								= 206265.0 / distance (au).
								Note that this should normally only be
								used for the z (source direction) axis.
							 */
	/* The pointing_* vectors and scalars
	   are alternate forms of entries for
	   the pointing direction information for the
	   telescopes.	The _x, _y, and _z
	   endings indicate that the vectors represent
	   the x, y, and z axes of the telescope,
	   respectively.  The axes are defined as
	   x axis points up/North
	   y axis points horizontally/West
	   z axis points toward the source
	   in a right-handed coordinate frame.
	   Here, the up/North refers to an
	   Alt-Az/Equatorial mount telescope.  In the
	   more general case, the x axis describes
	   the polarization reference axis of the
	   telescope.
	   The indices of each array ([0], [1], and [2])
	   represent the x, y, and z axes of the J2000
	   cartesian coordinate systm.	
	   For the pointing vectors, if all
	   values of the z position vector are
	   0.0, then the standard source ra, dec, and
	   parallax values are used.  If
	   (x^2+y^2+z^2)^{1/2} <= 1.5 m, then
	   the position is assumed to be a unit
	   vector, and the distance of the
	   source is assumed to be very far away.
	   Then, the coordinates are in a
	   J2000 frame centered on the Earth, but
	   with the intertial frame corresponding to
	   the Solar system barycenter.
	   In this case, source_parallax is used
	   If the magnitude
	   of the position vector is greater than
	   1.5 m, then the position vector is the J2000,
	   Earth-centered inertial coordinate frame
	   and no parallax information
	   from the source_parallax value is used,
	   as the source position is assumed to be
	   fully described by the J2000 Earth-center
	   position and velocity values.
	*/

    /* Earth Orientation Parameters */
	double EOP_time[NUM_DIFX_DELAYHANDLERCALCSERVER9_1_RA_EOPS];
	                        /* EOP epoch date.time (MJD) */
	double tai_utc[NUM_DIFX_DELAYHANDLERCALCSERVER9_1_RA_EOPS];
	                        /* TAI - UTC (secs) */
	double ut1_utc[NUM_DIFX_DELAYHANDLERCALCSERVER9_1_RA_EOPS];
	                        /* UT1 - UTC (secs) */
	double xpole[NUM_DIFX_DELAYHANDLERCALCSERVER9_1_RA_EOPS];
	                        /* earth pole offset, x (arcsec) */
	double ypole[NUM_DIFX_DELAYHANDLERCALCSERVER9_1_RA_EOPS];
	                        /* earth pole offset, y (arcsecs) */
     
	double pressure_a;      /* surface pressure stna (millibars) 
                             * enter 0.0 for none availiable */
	double pressure_b;      /* surface pressure stnb (millibars) 
                             * enter 0.0 for none availiable */

	char  station_a[DIFX_DELAYHANDLERCALCSERVER9_1_RA_STRING_SIZE];
	                        /* station A name */
	char  axis_type_a[DIFX_DELAYHANDLERCALCSERVER9_1_RA_STRING_SIZE];
	                        /* station A mount type, 'altz', 'equa',
                               ,xyns', 'xyew' */
	char  station_b[DIFX_DELAYHANDLERCALCSERVER9_1_RA_STRING_SIZE];
	                        /* station B name */
	char  axis_type_b[DIFX_DELAYHANDLERCALCSERVER9_1_RA_STRING_SIZE];
	                        /* station B mount type, 'altz', 'equa',
                               'xyns', 'xyew' */
	char  source[DIFX_DELAYHANDLERCALCSERVER9_1_RA_STRING_SIZE];
	                        /* source name */
};




struct CALCSERVER9_1_RA_MODEL_DELAY_RESPONSE {
	int64_t request_id;     /* RPC request id number, returned by Server */
	int64_t struct_code;  	/* CALC_9_1_RA Server struct code.	This specifies
							   which of the elements of this
							   structure are actually sent by the RPC
							   call.  Elements that are not sent are
							   automatically set to 0.	See
							   CALC_9_1_RA_SERVER_STRUCT_CODE_* for allowed
							   options.
							*/
	uint64_t server_version;/* The exact version number of the delay server
                               software that was used. */
	int64_t date;           /* CALC model date (MJD) */
	double time;            /* CALC model time UTC (fraction of day) */
	double delay[2];        /* total group delay, rate (secs, sec/sec) */
	double dry_atmos[4];    /* dry atmosphere delay, rate (secs, sec/sec) */
	double wet_atmos[4];    /* wet atmosphere delay, rate (secs, sec/sec) */
	double az[4];           /* stations azimuth angle, rate (rad, rad/sec) */
	double el[4];           /* station elevation angle, rate (rad, rad/sec) */
	double msa[4];			/* mount-source angle (parallactic
							   angle for altaz mounted telescopes)
							   and rate, in rad, rad/s for the
							   telescope and target source.	 This
							   is the angular difference between the
							   direction of the X axis of the
							   telescope (for altaz mounts X is
							   up, Y is horizontal, Z is toward
							   the source, for equatorial mounts X
							   is North, Y is West, and Z is
							   toward the source) compared to the direction
							   of celestial North at the location
							   of the target source.  For altaz mounts,
							   with the target source at the center of
							   the pointing direction, the mount-source
							   angle is the same as the parallactic angle,
							   and is positive for positive hour
							   angles.	For equatorial mounts, the
							   mount-source angle for a target source
							   at the center of the pointing direction
							   is zero.	 For other mount types, and for
							   target sources away from the pointing
							   direction, the values may be different.
							   The mount-source angle is important for
							   properly dealing with linear
							   polarization and phase offsets for
							   circular polarization.
							*/
	double UV[3];           /* u, v, w coordinates in J2000.0 frame (meters) */
	double baselineP2000[3];/* the baseline in the J2000 frame (m).	 This is
							   the position of station a minus the position
							   of station b, in the J2000 frame, after all
							   position corrections. */
	double baselineV2000[3];/* the baseline velocity in the J2000 frame
							   (m/s).  This is the velocity of station a
							   minus the velocity of station b,
							   in the J2000 frame, after all
							   position corrections. */
	double baselineA2000[3];/* the baseline acceleration in the J2000 frame
							   (m/s).  This is the acceleration of station a
							   minus the acceleration of station b,
							   in the J2000 frame, after all
							   position corrections. */
};






struct CALCSERVER9_1_RA_MODEL_PARAMETERS_ARGUMENT {
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


struct CALCSERVER9_1_RA_MODEL_PARAMETERS_RESPONSE {
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


#endif // DelayHandlercalcserver9_1_RAInterface_h
