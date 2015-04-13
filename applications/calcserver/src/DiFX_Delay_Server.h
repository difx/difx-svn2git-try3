/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef _DIFX_DELAY_SERVER_H_RPCGEN
#define _DIFX_DELAY_SERVER_H_RPCGEN

#include <rpc/rpc.h>


#ifdef __cplusplus
extern "C" {
#endif

#define DIFX_DELAY_SERVER_STATION_STRING_SIZE 32
#define NUM_DIFX_DELAY_SERVER_1_KFLAGS 64
#define DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA -999

struct DIFX_DELAY_SERVER_vec {
	double x;
	double y;
	double z;
};
typedef struct DIFX_DELAY_SERVER_vec DIFX_DELAY_SERVER_vec;

struct DIFX_DELAY_SERVER_1_station {
	char station_name[DIFX_DELAY_SERVER_STATION_STRING_SIZE];
	char antenna_name[DIFX_DELAY_SERVER_STATION_STRING_SIZE];
	char site_name[DIFX_DELAY_SERVER_STATION_STRING_SIZE];
	u_short site_ID;
	char site_type[DIFX_DELAY_SERVER_STATION_STRING_SIZE];
	char axis_type[DIFX_DELAY_SERVER_STATION_STRING_SIZE];
	DIFX_DELAY_SERVER_vec station_pos;
	DIFX_DELAY_SERVER_vec station_vel;
	DIFX_DELAY_SERVER_vec station_acc;
	DIFX_DELAY_SERVER_vec station_pointing_dir;
	DIFX_DELAY_SERVER_vec station_reference_dir;
	char station_coord_frame[DIFX_DELAY_SERVER_STATION_STRING_SIZE];
	char pointing_coord_frame[DIFX_DELAY_SERVER_STATION_STRING_SIZE];
	int pointing_corrections_applied;
	double station_position_delay_offset;
	double axis_off;
	int primary_axis_wrap;
	int secondary_axis_wrap;
	char receiver_name[DIFX_DELAY_SERVER_STATION_STRING_SIZE];
	double pressure;
	double antenna_pressure;
	double temperature;
	double wind_speed;
	double wind_direction;
	double antenna_phys_temperature;
};
typedef struct DIFX_DELAY_SERVER_1_station DIFX_DELAY_SERVER_1_station;

struct DIFX_DELAY_SERVER_1_source {
	char source_name[DIFX_DELAY_SERVER_STATION_STRING_SIZE];
	char IAU_name[DIFX_DELAY_SERVER_STATION_STRING_SIZE];
	char source_type[DIFX_DELAY_SERVER_STATION_STRING_SIZE];
	double ra;
	double dec;
	double dra;
	double ddec;
	double depoch;
	double parallax;
	char coord_frame[DIFX_DELAY_SERVER_STATION_STRING_SIZE];
	DIFX_DELAY_SERVER_vec source_pos;
	DIFX_DELAY_SERVER_vec source_vel;
	DIFX_DELAY_SERVER_vec source_acc;
	DIFX_DELAY_SERVER_vec source_pointing_dir;
	DIFX_DELAY_SERVER_vec source_pointing_reference_dir;
};
typedef struct DIFX_DELAY_SERVER_1_source DIFX_DELAY_SERVER_1_source;

struct DIFX_DELAY_SERVER_1_EOP {
	double EOP_time;
	double tai_utc;
	double ut1_utc;
	double xpole;
	double ypole;
};
typedef struct DIFX_DELAY_SERVER_1_EOP DIFX_DELAY_SERVER_1_EOP;

struct DIFX_DELAY_SERVER_1_RESULTS {
	double delay;
	double dry_atmos;
	double wet_atmos;
	double iono_atmos;
	double az_corr;
	double el_corr;
	double az_geom;
	double el_geom;
	double primary_axis_angle;
	double secondary_axis_angle;
	double mount_source_angle;
	double station_antenna_theta;
	double station_antenna_phi;
	double source_antenna_theta;
	double source_antenna_phi;
	DIFX_DELAY_SERVER_vec UVW;
	DIFX_DELAY_SERVER_vec baselineP2000;
	DIFX_DELAY_SERVER_vec baselineV2000;
	DIFX_DELAY_SERVER_vec baselineA2000;
};
typedef struct DIFX_DELAY_SERVER_1_RESULTS DIFX_DELAY_SERVER_1_RESULTS;

struct getDIFX_DELAY_SERVER_1_arg {
	long request_id;
	u_long delay_server;
	long server_struct_setup_code;
	long date;
	long ref_frame;
	int verbosity;
	short kflags[NUM_DIFX_DELAY_SERVER_1_KFLAGS];
	double time;
	double sky_frequency;
	int Use_Server_Station_Table;
	u_int Num_Stations;
	struct {
		u_int station_len;
		DIFX_DELAY_SERVER_1_station *station_val;
	} station;
	int Use_Server_Source_Table;
	u_int Num_Sources;
	struct {
		u_int source_len;
		DIFX_DELAY_SERVER_1_source *source_val;
	} source;
	int Use_Server_EOP_Table;
	u_int Num_EOPs;
	struct {
		u_int EOP_len;
		DIFX_DELAY_SERVER_1_EOP *EOP_val;
	} EOP;
};
typedef struct getDIFX_DELAY_SERVER_1_arg getDIFX_DELAY_SERVER_1_arg;

struct DIFX_DELAY_SERVER_1_res {
	int delay_server_error;
	int server_error;
	int model_error;
	long request_id;
	u_long delay_server;
	long server_struct_setup_code;
	u_long server_version;
	long date;
	double time;
	u_long unix_utc_seconds_0;
	u_long unix_utc_seconds_1;
	double utc_second_fraction;
	u_int Num_Stations;
	u_int Num_Sources;
	struct {
		u_int result_len;
		DIFX_DELAY_SERVER_1_RESULTS *result_val;
	} result;
};
typedef struct DIFX_DELAY_SERVER_1_res DIFX_DELAY_SERVER_1_res;

struct getDIFX_DELAY_SERVER_1_res {
	int this_error;
	union {
		struct DIFX_DELAY_SERVER_1_res response;
		char *errmsg;
	} getDIFX_DELAY_SERVER_1_res_u;
};
typedef struct getDIFX_DELAY_SERVER_1_res getDIFX_DELAY_SERVER_1_res;

#define DIFX_DELAY_SERVER_PROG 0x20000342
#define DIFX_DELAY_SERVER_VERS_1 1

#if defined(__STDC__) || defined(__cplusplus)
#define GETDIFX_DELAY_SERVER 1
extern  getDIFX_DELAY_SERVER_1_res * getdifx_delay_server_1(getDIFX_DELAY_SERVER_1_arg *, CLIENT *);
extern  getDIFX_DELAY_SERVER_1_res * getdifx_delay_server_1_svc(getDIFX_DELAY_SERVER_1_arg *, struct svc_req *);
extern int difx_delay_server_prog_1_freeresult (SVCXPRT *, xdrproc_t, caddr_t);

#else /* K&R C */
#define GETDIFX_DELAY_SERVER 1
extern  getDIFX_DELAY_SERVER_1_res * getdifx_delay_server_1();
extern  getDIFX_DELAY_SERVER_1_res * getdifx_delay_server_1_svc();
extern int difx_delay_server_prog_1_freeresult ();
#endif /* K&R C */

/* the xdr functions */

#if defined(__STDC__) || defined(__cplusplus)
extern  bool_t xdr_DIFX_DELAY_SERVER_vec (XDR *, DIFX_DELAY_SERVER_vec*);
extern  bool_t xdr_DIFX_DELAY_SERVER_1_station (XDR *, DIFX_DELAY_SERVER_1_station*);
extern  bool_t xdr_DIFX_DELAY_SERVER_1_source (XDR *, DIFX_DELAY_SERVER_1_source*);
extern  bool_t xdr_DIFX_DELAY_SERVER_1_EOP (XDR *, DIFX_DELAY_SERVER_1_EOP*);
extern  bool_t xdr_DIFX_DELAY_SERVER_1_RESULTS (XDR *, DIFX_DELAY_SERVER_1_RESULTS*);
extern  bool_t xdr_getDIFX_DELAY_SERVER_1_arg (XDR *, getDIFX_DELAY_SERVER_1_arg*);
extern  bool_t xdr_DIFX_DELAY_SERVER_1_res (XDR *, DIFX_DELAY_SERVER_1_res*);
extern  bool_t xdr_getDIFX_DELAY_SERVER_1_res (XDR *, getDIFX_DELAY_SERVER_1_res*);

#else /* K&R C */
extern bool_t xdr_DIFX_DELAY_SERVER_vec ();
extern bool_t xdr_DIFX_DELAY_SERVER_1_station ();
extern bool_t xdr_DIFX_DELAY_SERVER_1_source ();
extern bool_t xdr_DIFX_DELAY_SERVER_1_EOP ();
extern bool_t xdr_DIFX_DELAY_SERVER_1_RESULTS ();
extern bool_t xdr_getDIFX_DELAY_SERVER_1_arg ();
extern bool_t xdr_DIFX_DELAY_SERVER_1_res ();
extern bool_t xdr_getDIFX_DELAY_SERVER_1_res ();

#endif /* K&R C */

#ifdef __cplusplus
}
#endif

#endif /* !_DIFX_DELAY_SERVER_H_RPCGEN */
