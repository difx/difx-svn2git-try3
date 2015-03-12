/* CALC_9_1_RA_Server_test.x for testing rpcgen for CALC_9_1_RA_Server */

const CALC_9_1_RA_SERVER_STRUCT_CODE_0 = 0x0;
/* Request for the calcserver
   program to send back the current
   program version as a
   struct_code value in the
   request_id element slot
   through a struct_code=
   CALC_9_1_RA_SERVER_STRUCT_CODE_5_0_0
   call.
*/
const CALC_9_1_RA_SERVER_STRUCT_CODE_5_0_0 = 150; /* Original version 5 */
const CALC_9_1_RA_SERVER_STRUCT_CODE_5_1_0 = 0x0510; /* Include spacecraft capabilities by James M Anderson */
const CALC_9_1_RA_SERVER_STRUCT_CODE_CURRENT = CALC_9_1_RA_SERVER_STRUCT_CODE_5_1_0;

/* Client makes a CALC_9_1_RA request through getCALC_9_1_RA_arg arguments */
/* CALC_9_1_RA Server Version 5 */


struct getCALC_9_1_RA_arg {
	long request_id;		/* RPC request id number, user's choice */
	long struct_code;		/* CALC_9_1_RA Server struct code.	This specifies
							   which of the elements of this
							   structure are actually sent by the RPC
							   call.  Elements that are not sent are
							   automatically set to 0.	See
							   CALC_9_1_RA_SERVER_STRUCT_CODE_* for allowed
							   options.
							*/
	long date;				/* CALC_9_1_RA model date (MJD) */
	long ref_frame;			/* CALC_9_1_RA reference frame: 0 = geocentric */
	long dummy;
	short int kflags[64];	/* CALC_9_1_RA model component control flags */

	double time;			/* CALC_9_1_RA model time UTC (fraction of day) */

	double a_x;				/* geocentric right-hand x coord (meters) */
	double a_y;				/* geocentric right-hand y coord (meters) */
	double a_z;				/* geocentric right-hand z coord (meters) */
	double a_dx;			/* geocentric right-hand x velocity (m/s) */
	double a_dy;			/* geocentric right-hand y velocity (m/s) */
	double a_dz;			/* geocentric right-hand z velocity (m/s) */
	double a_ddx;			/* geocentric right-hand x acceleration (m/s/s) */
	double a_ddy;			/* geocentric right-hand y acceleration (m/s/s) */
	double a_ddz;			/* geocentric right-hand z acceleration (m/s/s) */
	double axis_off_a;		/* non-intersecting axis offset (meters) */
	double b_x;				/* geocentric right-hand x coord (meters) */
	double b_y;				/* geocentric right-hand y coord (meters) */ 
	double b_z;				/* geocentric right-hand z coord (meters) */
	double b_dx;			/* geocentric right-hand x velocity (m/s) */
	double b_dy;			/* geocentric right-hand y velocity (m/s) */
	double b_dz;			/* geocentric right-hand z velocity (m/s) */
	double b_ddx;			/* geocentric right-hand x acceleration (m/s/s) */
	double b_ddy;			/* geocentric right-hand y acceleration (m/s/s) */
	double b_ddz;			/* geocentric right-hand z acceleration (m/s/s) */
	double axis_off_b;		/* non-intersecting axis offset (meters) */
	double ra;				/* J2000.0 coordinates (radians) */
	double dec;				/* J2000.0 coordinates (radians) */
	double dra;				/* J2000.0 arcsecs/year */
	double ddec;			/* J2000.0 arcsecs/year */
	double depoch;			/* reference date for which proper motion
							 * corrections are zero, mjd.fract_day */
	double parallax;		/* source distance in arcsecs of annual
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
	double EOP_time[5];		/* EOP epoch date.time (MJD) */
	double tai_utc[5];		/* TAI - UTC (secs) */
	double ut1_utc[5];		/* UT1 - UTC (secs) */
	double xpole[5];		/* earth pole offset, x (arcsec) */
	double ypole[5];		/* earth pole offset, y (arcsecs) */
	 
	double pressure_a;		/* surface pressure stna (millibars) 
							 * enter 0.0 for none availiable */
	double pressure_b;		/* surface pressure stnb (millibars) 
							 * enter 0.0 for none availiable */

	char  *station_a;		/* station A name */
	char  *axis_type_a;		/* station A mount type, 'altz', 'equa',
							   ,xyns', 'xyew' */
	char  *station_b;		/* station B name */
	char  *axis_type_b;		/* station B mount type, 'altz', 'equa',
							   'xyns', 'xyew' */
	char  *source;			/* source name */

};




/* CALC_9_1_RA server reply */

struct CALC_9_1_RARecord {
	long struct_code;		/* CALC_9_1_RA Server struct code.	This specifies
							   which of the elements of this
							   structure are actually sent by the RPC
							   call.  Elements that are not sent are
							   automatically set to 0.	See
							   CALC_9_1_RA_SERVER_STRUCT_CODE_* for allowed
							   options.
							*/
	long request_id;		/* RPC request id number, returned by Server */
	long date;				/* CALC_9_1_RA model date (MJD) */
	double time;			/* CALC_9_1_RA model time UTC (fraction of day) */
	double delay[2];		/* total group delay, rate (secs, sec/sec) */
	double dry_atmos[4];	/* dry atmosphere delay, rate (secs, sec/sec) */
	/* The access pattern is as follows:
	   dry_atmos[0] => dry atm delay tel a
	   dry_atmos[1] => dry atm delay tel b
	   dry_atmos[2] => dry atm rate tel a
	   dry_atmos[3] => dry atm rate tel b
	*/
	double wet_atmos[4];	/* wet atmosphere delay, rate (secs, sec/sec) */
	double az[4];			/* stations azimuth angle, rate (rad, rad/sec) */
	double el[4];			/* station elevation angle, rate (rad, rad/sec) */
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
	double UV[3];			/* u, v, w coordinates in J2000.0 frame (meters) */
	double riseset[2];		/* estimated rise, set times for stnb VLBA Correlator */
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

union getCALC_9_1_RA_res switch (int error) {
case 0:
    struct CALC_9_1_RARecord record;
case 1:
    char *errmsg;
default:
    void; /* error ocurred */
};


/* CALCServer uses 0x20000340 */
/* CALC_9_1_RA_Server uses 0x20000341 */

const CALC_9_1_RA_SERVER_STRUCT_CODE_0 = 0x0;
program CALC_9_1_RAPROG {
    version CALC_9_1_RAVERS {
        int GETCALC_9_1_RA(void) = 1;
    } = 1;
} = 0x20000341;
