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


#ifndef DelayHandlerDistributorInterface_h
#define DelayHandlerDistributorInterface_h

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
#include <pthread.h>

#include <difxio.h>


#ifdef __cplusplus
namespace DiFX {
namespace Delay {
namespace Handler {
extern "C" {
#endif


#define DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE 32
#define NUM_DIFX_DELAYHANDLERDISTRIBUTOR_KFLAGS 64

struct SERVER_MODEL_DELAY_ARGUMENT_STATION {
    /* This structure defines data related to a station */
    char station_name[DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE];
	/* The name of the station */
    char antenna_name[DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE];
	/* The name of the antenna */
    char site_name[DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE];
	/* The name of the site */
    uint16_t site_ID; /* The 2-character standardized site ID code. */
	                  /* Note: to convert to a character
						 representation, do
						 sprintf(s, "%c%c", (char)(site_ID&0xFF), (char)(site_ID>>8))
						 To convert a two-character code to a site_ID,
						 do size_ID = (unsigned short)(code[0]) | ((unsigned short)(code[1]) << 8) */
    char site_type[DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE];
	/* The station site type.
	   Allowed values are: "fixed" and "earth_orit"
	*/
    char axis_type[DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE];
	/* The station axis type
	   'altz', 'equa', 'xyns', 'xyew' */
    double station_pos[3];
	/* station position in the specified frame 
	   (right-handed coord)
	   (meters)
	   Note that most delay servers only accept
	   "ITRF2008" frames for fixed stations
	*/
    double station_vel[3];
	/* station velocity
	   (right-handed coord)
	   (m/s)  Note: not used for site_type=="fixed" */
    double station_acc[3];
	/* station acceleration
	   (right-handed coord) (m/s/s)  Note: not used for
	   site_type=="fixed" */
    double station_pointing_dir[3];
	/* A vector describing the pointing direction of
	   the station at this date/time.  Support for
	   pointing directions different from source
	   directions is server software dependent (none
	   provide this yet).  This may be a unit vector
	   or a full vector.  The coordinate system is
	   described by pointing_coord_frame.
	*/
    double station_reference_dir[3];
	/* vector of the station
	   reference direction of the antenna in a
	   right-handed coordinate frame.  This is
	   only used for "earth_orbit" site_types.
	   The reference direction of the antenna
	   should be orthogonal to the pointing
	   direction of the antenna.  "fixed" antennas
	   have their reference direction determined by
	   the axis_type and the pointing direction.

	   TODO:  allow for rotating "fixed" antennas such
	   as the ASKAP design.
	*/
    int32_t station_coord_frame;
    int32_t pointing_coord_frame;
	/* The coordinate frames of the ephemeris object
	   and of the pointing direction of the station:
	   "":            Alias for "J2000".
	   "J2000":       The standard J2000 coordinate
	   system that assumes the
	   Solar barycenter is fixed in
	   space
	   "J2000_CMB":   A J2000-like frame at
	   rest with respect to the
	   cosmic microwave background
	   frame as seen by the Solar
	   barycenter at epoch J2000.
	   The CMB frame is presumed to
	   undergo no acceleration.
	   The position
	   information provided is
	   with respect to the Solar
	   barycenter at the depoch
	   epoch.  Corrections will
	   be applied for the velocity
	   and acceleration of the
	   Milky Way barycenter with
	   respect to the CMB, and the
	   orbit and extraordinary
	   acceleration of the Solar
	   barycenter with respect to
	   the Milky Way barycenter, and
	   the usual velocity of the
	   Earth with respect to the
	   Solar barycenter.

	   ***Note that this is not yet
	   implemented.
	   "J2000_CMB_1": A J2000-like frame at
	   rest with respect to the
	   SSB at the J2000 epoch.
	   The position
	   information provided is
	   with respect to the Solar
	   barycenter at the depoch
	   epoch.  Corrections will
	   be applied for the
	   acceleration of the
	   Milky Way barycenter with
	   respect to the CMB, and the
	   orbital acceleration
	   and extraordinary
	   acceleration of the Solar
	   barycenter with respect to
	   the Milky Way barycenter, and
	   the usual velocity of the
	   Earth with respect to the
	   Solar barycenter.

	   ***Note that this is not yet
	   implemented.
	   "J2000_MWB":   A J2000-like frame at
	   rest with respect to the
	   Milky Way barycenter at
	   epoch J2000.  The MWB is
	   presumed to undergo no
	   acceleration.  The position
	   information provided is
	   with respect to the Solar
	   barycenter at the depoch
	   epoch.  Corrections will
	   be applied for the
	   orbit and extraordinary
	   acceleration of the Solar
	   barycenter with respect to
	   the Milky Way barycenter, and
	   the usual velocity of the
	   Earth with respect to the
	   Solar barycenter.

	   ***Note that this is not yet
	   implemented.
	   "J2000_MWB_1": A J2000-like frame at
	   rest with respect to the
	   SSB at the J2000 epoch.
	   The position
	   information provided is
	   with respect to the Solar
	   barycenter at the depoch
	   epoch.  Corrections will
	   be applied for the
	   orbital acceleration
	   and extraordinary
	   acceleration of the Solar
	   barycenter with respect to
	   the Milky Way barycenter, and
	   the usual velocity of the
	   Earth with respect to the
	   Solar barycenter.

	   ***Note that this is not yet
	   implemented.
	   "J2000_SSB":   Alias for "J2000".
	   A J2000-like frame at
	   rest with respect to the
	   Solar barycenter at
	   epoch J2000.  The SSB is
	   presumed to undergo no
	   acceleration.  The position
	   information provided is
	   with respect to the SSB at
	   the depoch epoch, which has
	   the same position as at
	   J2000.  Corrections will
	   be applied for the usual
	   velocity of the
	   Earth with respect to the
	   Solar barycenter.
	   "J2000_Earth": A J2000-like frame at
	   rest with respect to the
	   Solar barycenter at
	   epoch J2000.  The SSB is
	   presumed to undergo no
	   acceleration.  The position
	   information provided is
	   with respect to the Earth
	   center at the depoch epoch.
	   Corrections will
	   be applied for the usual
	   velocity of the
	   Earth with respect to the
	   Solar barycenter.

	   ***Note that this is not yet
	   implemented.
	   "ITRF2008":    A frame at rest with
	   respect to the Earth.
	   Position information is in
	   the ITRF2008 frame.

	   ***Note that this is not yet
	   implemented.
	*/
    int32_t pointing_corrections_applied;
	/* Flag indicating whether or not the station
	   pointing and reference directions supplied
	   above are the actual coordinates, or whether
	   the antenna pointing has been subsequently
	   corrected for aberration, refraction, and so
	   on.
	   0: No corrections applied
	   1: Abberation corrections applied
	   2: Aberration and refraction applied
	*/
    double station_position_delay_offset;
	/* For "earth_orbit" site_type, this value provides
	   the delay offset with respect to station 0 at
	   which the station_pos, station_vel, and
	   station_acc vectors are provided.  Typically,
	   this value should be <~ 1 second.  It allows
	   for more precise computations using the
	   spacecraft position information to be calculated.
	   Start with this value set to 0.0.  Then, after
	   an initial delay calculation, compute the
	   spacecraft position information using the
	   time of the model calculation offset by the
	   delay between source 0 (normally the center of
	   the Earth) and the spacecraft for the 0th
	   source (the pointing direction).  Then
	   set this parameter to that delay, and
	   re-calculate the measurement delays.
	   Note: note yet implemented in any delay
	   server.
	*/
    double axis_off;        /* station non-intersecting axis offset (meters) */
    int32_t primary_axis_wrap;  /* Axis wrap information for the 
                               primary axis (az, ha, x).  This
                               parameter allows cable wrap to be specified.
                               Specify 0 for no data.
                               az:  -1: clockwise wrap
							   0: neutral
							   +1: counterclockwise wrap
							   Note: CW and CCW are as seen from above the
							   station.
                               ha:  -1: -24: 00 hours
							   0: -12:+12 hours
							   +1:  00:+24 hours
                               x:   must be 0
							*/
    int32_t secondary_axis_wrap;/* Axis wrap information for the 
                               secondary axis (el, dec, y).  This
                               parameter allows over the top to be specified.
                               Specify 0 for no data
                               el:  0:  0:90 degrees
							   +1: 90:180 degrees
                               dec:-1: -180:-90 degrees
							   0:  -90:+90 degrees
							   +1:  +90:+180 degrees
                               y:  must be 0
							*/
    char receiver_name[DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE];
	/* The station receivers used (one per station) */
    double pressure;        /* station surface pressure (millibars) 
                             * enter 0.0 for none availiable */
    double antenna_pressure;/* station pressure at the effective aperture
							   location of the antenna (millibars) 
							   * enter 0.0 for none availiable */
    double temperature;     /* station atmospheric temperature at reference
                               time in Kelvin.  Enter 0.0 for none available. */
    double wind_speed;      /* The wind speed (m/s).  Provide NAN if no
                               data are available. */
    double wind_direction;  /* The wind direction in radians from North through
                               East.  Specify NAN when no data are available
                            */
    double antenna_phys_temperature;
	/* station physical temperature of the antenna
	   (possibly allowing for thermal propagation
	   delays in the antenna) in Kelvin.
	   Enter 0.0 for none available. */
};    
struct SERVER_MODEL_DELAY_ARGUMENT_SOURCE {
    /* This structure defines data related to a source */
    char source_name[DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE];
	/* The source name (usually up to 16 char) */
    char IAU_name[DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE];
	/* The IAU name (usually up to 9 char)
	   Example: "0102-0304"
	   Note that the delay servers may still expect
	   the old, non-IAU format such as "0102-030"
	   with the name in B1950 coordinates
	*/
    char source_type[DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE];
	/* The source type:
	   "star":  the standard designator.  Whether
	   the delay server uses ra, dec, dra,
	   ddec, depoch, parallax, source_epoch,
	   and assumes the coordinate system is
	   J2000, or whether the delay server
	   uses source_pos, source_vel,
	   source_acc, and coord_frame depends
	   on which delay server is actually
	   used.
	   "ephemeris": uses source_pos, source_vel,
	   source_acc, and coord_frame
	*/
    double ra;              /* J2000.0 coordinates (radians) (retarded) */
    double dec;             /* J2000.0 coordinates (radians) (retarded) */
    double dra;             /* J2000.0 arcsecs/year */
    double ddec;            /* J2000.0 arcsecs/year */
    double depoch;          /* reference date for which proper motion
                               corrections are zero, as mjd.fract_day.
                               If depoch == 0.0, then the epoch is the
                               current instant at which the delay
                               computation is to be performed.
							*/
    double parallax;        /* source distance in arcsecs of annual
                             * parallax, = 206265.0 / distance (au) */
    int32_t coord_frame;
	/* The coordinate frame of the ephemeris object:
	   "":            Alias for "J2000".
	   "J2000"      : The standard J2000 coordinate
	   system that assumes the
	   Solar barycenter is fixed in
	   space
	   "J2000_CMB":   A J2000-like frame at
	   rest with respect to the
	   cosmic microwave background
	   frame as seen by the Solar
	   barycenter at epoch J2000.
	   The CMB frame is presumed to
	   undergo no acceleration.
	   The position
	   information provided is
	   with respect to the Solar
	   barycenter at the depoch
	   epoch.  Corrections will
	   be applied for the velocity
	   and acceleration of the
	   Milky Way barycenter with
	   respect to the CMB, and the
	   orbit and extraordinary
	   acceleration of the Solar
	   barycenter with respect to
	   the Milky Way barycenter, and
	   the usual velocity of the
	   Earth with respect to the
	   Solar barycenter.

	   ***Note that this is not yet
	   implemented.
	   "J2000_CMB_1": A J2000-like frame at
	   rest with respect to the
	   SSB at the J2000 epoch.
	   The position
	   information provided is
	   with respect to the Solar
	   barycenter at the depoch
	   epoch.  Corrections will
	   be applied for the
	   acceleration of the
	   Milky Way barycenter with
	   respect to the CMB, and the
	   orbital acceleration
	   and extraordinary
	   acceleration of the Solar
	   barycenter with respect to
	   the Milky Way barycenter, and
	   the usual velocity of the
	   Earth with respect to the
	   Solar barycenter.

	   ***Note that this is not yet
	   implemented.
	   "J2000_MWB":   A J2000-like frame at
	   rest with respect to the
	   Milky Way barycenter at
	   epoch J2000.  The MWB is
	   presumed to undergo no
	   acceleration.  The position
	   information provided is
	   with respect to the Solar
	   barycenter at the depoch
	   epoch.  Corrections will
	   be applied for the
	   orbit and extraordinary
	   acceleration of the Solar
	   barycenter with respect to
	   the Milky Way barycenter, and
	   the usual velocity of the
	   Earth with respect to the
	   Solar barycenter.

	   ***Note that this is not yet
	   implemented.
	   "J2000_MWB_1": A J2000-like frame at
	   rest with respect to the
	   SSB at the J2000 epoch.
	   The position
	   information provided is
	   with respect to the Solar
	   barycenter at the depoch
	   epoch.  Corrections will
	   be applied for the
	   orbital acceleration
	   and extraordinary
	   acceleration of the Solar
	   barycenter with respect to
	   the Milky Way barycenter, and
	   the usual velocity of the
	   Earth with respect to the
	   Solar barycenter.

	   ***Note that this is not yet
	   implemented.
	   "J2000_SSB":   Alias for "J2000".
	   A J2000-like frame at
	   rest with respect to the
	   Solar barycenter at
	   epoch J2000.  The SSB is
	   presumed to undergo no
	   acceleration.  The position
	   information provided is
	   with respect to the SSB at
	   the depoch epoch, which has
	   the same position as at
	   J2000.  Corrections will
	   be applied for the usual
	   velocity of the
	   Earth with respect to the
	   Solar barycenter.
	   "J2000_Earth": A J2000-like frame at
	   rest with respect to the
	   Solar barycenter at
	   epoch J2000.  The SSB is
	   presumed to undergo no
	   acceleration.  The position
	   information provided is
	   with respect to the Earth
	   center at the depoch epoch.
	   Corrections will
	   be applied for the usual
	   velocity of the
	   Earth with respect to the
	   Solar barycenter.

	   ***Note that this is not yet
	   implemented.
	   "ITRF2008":    A frame at rest with
	   respect to the Earth.
	   Position information is in
	   the ITRF2008 frame.

	   ***Note that this is not yet
	   implemented.
	*/
    double source_pos[3];   /* source position vector (m)*/
    double source_vel[3];   /* source velocity vector (m/s)*/
    double source_acc[3];   /* source velocity vector (m/s)
                                           Note: The source position,
                                           velocity, and acceleration are
                                           all at the retarded time at which
                                           the center of the Earth sees the
                                           source at the depoch epoch, which,
                                           if depoch == 0.0, is the instant
                                           at which station 0 sees the signal
                                           in this calculation.
                                        */
    double source_pointing_dir[3];
	/* source pointing direction unit
	   vector.  For spacecraft antennas,
	   this is the nominal pointing
	   direction of the antenna at the
	   retarded time that the center of
	   the Earth sees the spacecraft at
	   the delay model calculation time.
	   For celestial sources, have this
	   point at the center of the Earth.
	*/
    double source_pointing_reference_dir[3];
	/* source pointing direction unit
	   vector for the reference direction
	   of the source.  This is used to
	   handle calculations of the phase
	   of signals from rotating
	   spacecraft.  This reference
	   direction should be some fixed
	   direction relative to the antenna
	   that is orthogonal to the
	   source_pointing_dir direction.
	   For celestial sources, this should
	   point toward the J2000 North Pole.
	   This direction is the direction
	   at the retarded time that the
	   center of the Earth sees the
	   spacecraft at the delay model
	   calculation time.
	*/
};
struct SERVER_MODEL_DELAY_ARGUMENT_EOP {
    double EOP_time;        /* EOP epoch date.time (MJD) */
    double tai_utc;         /* TAI - UTC (secs) */
    double ut1_utc;         /* UT1 - UTC (secs) */
    double xpole;           /* earth pole offset, x (arcsec) */
    double ypole;           /* earth pole offset, y (arcsecs) */
};



struct SERVER_MODEL_DELAY_ARGUMENT {
    /*************************************************************************/
    /*** Setup ***************************************************************/
    int64_t request_id;         /* request id number, user's choice          */
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
    int64_t date;           /* DIFX_DELAY_SERVER model date (MJD) */
    double time;            /* DIFX_DELAY_SERVER model time UTC (fraction of day) */
	int64_t utc_second;     /* UTC seconds since MJD=0 (ignores leap seconds */
    double utc_second_fraction; /* fractional second offset from utc_second,
                                   in range [0--1).                          */
	int64_t tai_second;     /* TAI seconds since MJD=0
							   Note that this is only properly defined for
							   times after year 1977.0.
							 */
    double tai_second_fraction; /* fractional second offset from tai_second,
                                   in range [0--1).                          */
    int16_t kflags[NUM_DIFX_DELAYHANDLERDISTRIBUTOR_KFLAGS];
	/* DIFX_DELAY_SERVER model component control flags */

    int32_t verbosity;      /* How verbose should logging be? Higher means more messages */
    int32_t ref_frame;      /* DIFX_DELAY_SERVER reference frame: 0 = geocentric */
    double sky_frequency;   /* The sky frequency for this observation (Hz) */
    /*************************************************************************/
    /*** Stations*************************************************************/
    int32_t Use_Server_Station_Table;
                            /* Flag to specify whether the delay server
							   should use station information from its own
							   station table (1) or to use the data from
							   this RPC message (0).  Alternatively, a flag
							   value of 2 means that the server should use
							   the position information in these RPC data,
							   but use other information (weather, loading,
							   and so on) from the server's own tables.
							   Stations will be looked
							   up based on the station_name, antenna_name,
							   site_name, and site_ID fields below.
							*/
    uint32_t Num_Stations;  /* The number of stations in this call */
    struct SERVER_MODEL_DELAY_ARGUMENT_STATION* station; /* The station data */
    /*************************************************************************/
    /*** Sources *************************************************************/
    int32_t Use_Server_Source_Table;
                            /* Flag to specify whether the delay server
							   should use source information from its own
							   source table (1) or to use the data from
							   this RPC message (0).  Sources will be looked
							   up based on the source_name and/or IAU_name
							   fields below.
							*/
    uint32_t Num_Sources;   /* The number of sources to process.  */
    struct SERVER_MODEL_DELAY_ARGUMENT_SOURCE* source; /* The source data */
    /*************************************************************************/
    /*** Earth Orientation Parameters ****************************************/
    int32_t Use_Server_EOP_Table;
                              /* Flag to specify whether the delay server
                                 should use EOP information from its own
                                 EOP table (1) or to use the data from
                                 this RPC message (0).
							  */
    uint32_t Num_EOPs;        /* The number of EOP parameters provided.
                                 Note that this should be at least 5,
                                 and preferably 15 or more for many
                                 delay servers.
							  */
    struct SERVER_MODEL_DELAY_ARGUMENT_EOP* EOP; /* The EOP data */
};




struct SERVER_MODEL_DELAY_RESPONSE_DATA {
    /* These data are provided for a single station/source combination */
    double delay;           /* total group delay in seconds.
                               Note that for the 0th station, the delay
                               reported here, if not 0.0, is the
                               total delay from the emission by the source
                               itself.
							*/
    double dry_atmos;       /* dry atmosphere delay in seconds */
    double wet_atmos;       /* wet atmosphere delay in seconds */
    double iono_atmos;      /* ionospheric delay, in seconds, at a
                               frequency of 1 GHz */
    double az_corr;         /* azimuth in radians from North through East
                               corrected for refraction
							*/
    double el_corr;         /* elevation angle in degrees
                               corrected for refraction
							*/
    double az_geom;         /* azimuth in radians from North through East
                               for the geometric direction of the source
                               (uncorrected for refraction)
							*/
    double el_geom;         /* elevation angle in degrees
                               for the geometric direction of the source
                               (uncorrected for refraction)
							*/
    double primary_axis_angle;/* station azimuth angle in radians */
    double secondary_axis_angle;/* station elevation angle in radians */
    double mount_source_angle; /* mount-source angle (parallactic
								  angle for altaz mounted telescopes)
								  in radians.  This
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
								  angles.  For equatorial mounts, the
								  mount-source angle for a target source
								  at the center of the pointing direction
								  is zero.  For other mount types, and for
								  target sources away from the pointing
								  direction, the values may be different.
								  The mount-source angle is important for
								  properly dealing with linear
								  polarization and phase offsets for
								  circular polarization.  Corrections for the
								  motion of the source for spacecraft transmitters
								  are taken into account.
							   */
    double station_antenna_theta;
	/* This value is the angle between the
	   nominal pointing direction of the
	   station antenna receiver and the
	   direction of the source.  (How far
	   off-axis is the source from the
	   station antenna.)
	*/
    double station_antenna_phi;
	/* This value is rotation angle from the
	   point of view of the station anntenna
	   at which the source is seen.  The phi
	   angle is the angle from the x direction
	   through the y direction of the station
	   antenna (with the z direction being the
	   pointing direction of the station
	   antenna receiver system and the x
	   direction the reference direction of
	   the station antenna, in a right-handed
	   coordinate system).
	*/
    double source_antenna_theta;
	/* For spacecraft sources, this value is
	   the angle between the nominal pointing
	   direction of the spacecraft antenna and
	   the direction of the station as seen by
	   the source.  (How far off-axis is the
	   station from the spacecraft antenna.)
	   This is typically used in the
	   calculation of a phase offset for
	   direction, such as from a GNSS
	   spacecraft transmitter.
	*/
    double source_antenna_phi;
	/* For spacecraft sources, this value is
	   the azimuthal angle of the spacecraft
	   antenna at which the station is seen.
	   The phi angle is the angle from the x
	   direction through the y direction of
	   the spacecraft antenna (with the z
	   direction being the pointing direction
	   of the spacecraft antenna and the x
	   direction the reference direction of
	   the spacecraft antenna, in a
	   right-handed coordinate system).  This
	   is typically used in the calculation of
	   a phase offset for direction, such as
	   from a GNSS spacecraft transmitter.
	*/
    double UVW[3];/* u, v, w coordinates in J2000.0 frame (meters)
					 written into the x,y,z members of the struct. */
    double baselineP2000[3];
	/* the baseline in the J2000 frame (m).  This is
	   the position of station 0 minus the position
	   of station s, in the J2000 frame, after all
	   position corrections. */
    double baselineV2000[3];
	/* the baseline velocity in the J2000 frame
	   (m/s).  This is the velocity of station 0
	   minus the velocity of station s,
	   in the J2000 frame, after all
	   position corrections. */
    double baselineA2000[3];
	/* the baseline acceleration in the J2000 frame
	   (m/s/s).  This is the acceleration of station 0
	   minus the acceleration of station s,
	   in the J2000 frame, after all
	   position corrections. */
};    

struct SERVER_MODEL_DELAY_RESPONSE {
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
    int64_t date;           /* DIFX_DELAY_SERVER model date (MJD) */
    double time;            /* DIFX_DELAY_SERVER model time UTC (fraction of day) */
	int64_t utc_second;     /* UTC seconds since MJD=0 (ignores leap seconds */
    double utc_second_fraction; /* fractional second offset from utc_second,
                                   in range [0--1).                          */
	int64_t tai_second;     /* TAI seconds since MJD=0
							   Note that this is only properly defined for
							   times after year 1977.0.
							 */
    double tai_second_fraction; /* fractional second offset from tai_second,
                                   in range [0--1).                          */
    /*************************************************************************/
    /*** Stations*************************************************************/
    uint32_t Num_Stations; /* The number of stations in this call */
    /*************************************************************************/
    /*** Sources *************************************************************/
    uint32_t Num_Sources; /* The number of sources to process.  Note that
							 the 0th source is used as the pointing
							 direction of the stations, for those delay
							 model servers that support off-axis
							 calculations.  For array observations where
							 stations are pointed in different directions,
							 multiple calls to this delay server must be
							 made.
						  */
    /*************************************************************************/
    /*** Results *************************************************************/
    /* Note that the array below is actually two-dimensional, and for
       indices station and source goes as
       result[station*Num_Sources + source]
    */
    struct SERVER_MODEL_DELAY_RESPONSE_DATA* result;/* The delay results */
};



struct SERVER_MODEL_PARAMETERS_ARGUMENT {
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


struct SERVER_MODEL_PARAMETERS_RESPONSE {
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




// CLASS FUNCTIONS



// HELPER FUNCTIONS



#ifdef __cplusplus
}
}  // end namespace Handler
}  // end namespace Delay
}  // end namespace DiFX
#endif

#endif // DelayHandlerDistributorInterface_h
