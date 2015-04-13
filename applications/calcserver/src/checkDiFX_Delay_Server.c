#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <inttypes.h>
#include "MATHCNST.H"
#include "CALCServer.h"
#include "CALC_9_1_RA_Server.h"
#include "DiFX_Delay_Server.h"

static struct timeval TIMEOUT = {25, 0};
#define SECONDS_PER_DAY INT64_C(86400)
#define SECONDS_PER_DAY_DBL (86400.0)



char* difx_strlcpy(char *dest, const char *src, size_t n)
{
	char* r = strncpy(dest, src, n);
	if(n>0)
	{
		dest[n-1] = 0;
	}
	return r;
}


getDIFX_DELAY_SERVER_1_res *
getdifx_delay_server_1(argp, clnt)
struct getDIFX_DELAY_SERVER_1_arg *argp;
CLIENT *clnt;
{
	static getDIFX_DELAY_SERVER_1_res clnt_res;
    enum clnt_stat clnt_stat;

	memset((char *)&clnt_res, 0, sizeof (clnt_res));
	clnt_stat = clnt_call(clnt, GETDIFX_DELAY_SERVER,
                          (xdrproc_t) xdr_getDIFX_DELAY_SERVER_1_arg, (caddr_t) argp,
                          (xdrproc_t) xdr_getDIFX_DELAY_SERVER_1_res, (caddr_t) &clnt_res,
                          TIMEOUT);

    fprintf(stderr, "Return from clnt_call\n");
    if (clnt_stat != RPC_SUCCESS)
    {
        fprintf (stderr, "clnt_call failed in getdifx_delay_server_1\n");
        return NULL;
    }

	return (&clnt_res);
}













int test_CALCServer_mode(CLIENT *cl, const char *host)
{
    getDIFX_DELAY_SERVER_1_arg request_args, *p_request;
    getDIFX_DELAY_SERVER_1_res *p_result;

    double delta_dly;
    char   stnnamea[8];
    char   stnnameb[8];
    unsigned short station_ID;
    int    i, v, mode_failures;
    size_t station_size, source_size, EOP_size;
    void *station_mem, *source_mem, *EOP_mem;

    printf("Checking CALCServer\n");
    mode_failures = 0;
    p_request = &request_args;
    memset(p_request, 0, sizeof(getDIFX_DELAY_SERVER_1_arg));
    station_size = sizeof(DIFX_DELAY_SERVER_1_station)*2;
    if((station_mem = malloc(station_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc station memory\n");
        exit(EXIT_FAILURE);
    }
    memset(station_mem, 0, station_size);
    p_request->Num_Stations = 2;
    p_request->station.station_len = 2;
    p_request->station.station_val = station_mem;
    source_size = sizeof(DIFX_DELAY_SERVER_1_source)*1;
    if((source_mem = malloc(source_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc source memory\n");
        exit(EXIT_FAILURE);
    }
    memset(source_mem, 0, source_size);
    p_request->Num_Sources = 1;
    p_request->source.source_len = 1;
    p_request->source.source_val = source_mem;
    EOP_size = sizeof(DIFX_DELAY_SERVER_1_EOP)*5;
    if((EOP_mem = malloc(EOP_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc EOP memory\n");
        exit(EXIT_FAILURE);
    }
    memset(EOP_mem, 0, EOP_size);
    p_request->Num_EOPs = 5;
    p_request->EOP.EOP_len = 5;
    p_request->EOP.EOP_val = EOP_mem;

    p_request->date = 50774;
    p_request->time = 22.0/24.0 + 2.0/(24.0*60.0);
    p_request->verbosity = 5;
    p_request->delay_server = CALCPROG;
    p_request->server_struct_setup_code = 0;
    p_request->request_id = 150;
    p_request->ref_frame = 0;

    for (i = 0; i < NUM_DIFX_DELAY_SERVER_1_KFLAGS; i++)
        p_request->kflags[i] = -1;
    p_request->sky_frequency = 10.E9;
    p_request->Use_Server_Station_Table = 0;
    p_request->Use_Server_Source_Table = 0;
    p_request->Use_Server_EOP_Table = 0;

    strcpy (stnnamea, "EC");
    station_ID = (unsigned short)('E') | ((unsigned short)('C') << 8);
    difx_strlcpy(p_request->station.station_val[0].station_name, stnnamea, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[0].antenna_name, stnnamea, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[0].site_name, stnnamea, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->station.station_val[0].site_ID = station_ID;
    p_request->station.station_val[0].station_pos.x =  0.000;
    p_request->station.station_val[0].station_pos.y =  0.000;
    p_request->station.station_val[0].station_pos.z =  0.000;
    p_request->station.station_val[0].station_vel.x =  0.0;
    p_request->station.station_val[0].station_vel.y =  0.0;
    p_request->station.station_val[0].station_vel.z =  0.0;
    p_request->station.station_val[0].station_acc.x =  0.0;
    p_request->station.station_val[0].station_acc.y =  0.0;
    p_request->station.station_val[0].station_acc.z =  0.0;

    difx_strlcpy(p_request->station.station_val[0].axis_type, "altz", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[0].site_type, "fixed", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->station.station_val[0].axis_off = 0.00;
    difx_strlcpy(p_request->station.station_val[0].station_coord_frame, "ITRF2008", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->station.station_val[0].pointing_coord_frame[0] = 0;
    p_request->station.station_val[0].receiver_name[0] = 0;
    p_request->station.station_val[0].pressure = 0.0;
    p_request->station.station_val[0].antenna_pressure = 0.0;
    p_request->station.station_val[0].temperature = 0.0;
    p_request->station.station_val[0].wind_speed = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
    p_request->station.station_val[0].wind_direction = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
    p_request->station.station_val[0].antenna_phys_temperature = 0.0;
    /*
      strcpy (stnnameb, "FD");
      p_request->station_b = &stnnameb[0];
      p_request->b_x =     -1324009.0026;
      p_request->b_y =     -5332182.0834;
      p_request->b_z =      3231962.4355;

      strcpy (axistypeb, "altz");
      p_request->axis_type_b = &axistypeb[0];
      p_request->axis_off_b = 2.1226;
    */

    strcpy (stnnameb, "KP");
    station_ID = (unsigned short)('K') | ((unsigned short)('P') << 8);
    difx_strlcpy(p_request->station.station_val[1].station_name, stnnameb, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[1].antenna_name, stnnameb, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[1].site_name, stnnameb, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->station.station_val[1].site_ID = station_ID;
    p_request->station.station_val[1].station_pos.x =  0.000;
    

    p_request->station.station_val[1].station_pos.x =     -1995678.4969;
    p_request->station.station_val[1].station_pos.y =     -5037317.8209;
    p_request->station.station_val[1].station_pos.z =      3357328.0825;
    p_request->station.station_val[1].station_vel.x =  0.0;
    p_request->station.station_val[1].station_vel.y =  0.0;
    p_request->station.station_val[1].station_vel.z =  0.0;
    p_request->station.station_val[1].station_acc.x =  0.0;
    p_request->station.station_val[1].station_acc.y =  0.0;
    p_request->station.station_val[1].station_acc.z =  0.0;

    difx_strlcpy(p_request->station.station_val[1].axis_type, "altz", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[1].site_type, "fixed", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->station.station_val[1].axis_off = 2.1377;
    difx_strlcpy(p_request->station.station_val[1].station_coord_frame, "ITRF2008", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->station.station_val[1].pointing_coord_frame[0] = 0;
    p_request->station.station_val[1].receiver_name[0] = 0;
    p_request->station.station_val[1].pressure = 0.0;
    p_request->station.station_val[1].antenna_pressure = 0.0;
    p_request->station.station_val[1].temperature = 0.0;
    p_request->station.station_val[1].wind_speed = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
    p_request->station.station_val[1].wind_direction = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
    p_request->station.station_val[1].antenna_phys_temperature = 0.0;


    difx_strlcpy(p_request->source.source_val[0].source_name, "B1937+21", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->source.source_val[0].IAU_name, "B1937+21", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->source.source_val[0].source_type, "star", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->source.source_val[0].coord_frame[0] = 0;
    p_request->source.source_val[0].ra  =  (TWOPI/24.0)*(19.0 + 39.0/60.0 + 38.560210/3600.0);
    p_request->source.source_val[0].dec =  (TWOPI/360.)*(21.0 + 34.0/60.0 + 59.141000/3600.0);

    p_request->source.source_val[0].dra  = 0.0;
    p_request->source.source_val[0].ddec = 0.0;
    p_request->source.source_val[0].depoch = 0.0;
    p_request->source.source_val[0].parallax = 0.0;

    for (i = 0; i < 5; i++)
    {
        p_request->EOP.EOP_val[i].EOP_time = 50773.0 + (double) i; 
        p_request->EOP.EOP_val[i].tai_utc = 31.0;
    }
    p_request->EOP.EOP_val[0].ut1_utc = 0.285033;
    p_request->EOP.EOP_val[0].xpole   = 0.19744;
    p_request->EOP.EOP_val[0].ypole   = 0.24531;
     
    p_request->EOP.EOP_val[1].ut1_utc = 0.283381;
    p_request->EOP.EOP_val[1].xpole   = 0.19565;
    p_request->EOP.EOP_val[1].ypole   = 0.24256;
     
    p_request->EOP.EOP_val[2].ut1_utc = 0.281678;
    p_request->EOP.EOP_val[2].xpole   = 0.19400;
    p_request->EOP.EOP_val[2].ypole   = 0.24000;
     
    p_request->EOP.EOP_val[3].ut1_utc = 0.280121;
    p_request->EOP.EOP_val[3].xpole   = 0.19244;
    p_request->EOP.EOP_val[3].ypole   = 0.23700;
     
    p_request->EOP.EOP_val[4].ut1_utc = 0.278435;
    p_request->EOP.EOP_val[4].xpole   = 0.19016;
    p_request->EOP.EOP_val[4].ypole   = 0.23414;


    v = system ("date -u \"+\%FT\%T.\%NZ\"");
    if(v == -1)
    {
        fprintf(stderr, "Warning -- system() failed\n");
    }

    p_result = getdifx_delay_server_1(p_request, cl);

    /**/
    printf ("return from RPC call\n");
    v = system ("date -u \"+\%FT\%T.\%NZ\"");
    if(v == -1)
    {
        fprintf(stderr, "Warning -- system() failed\n");
    }
    if(!p_result)
    {
        fprintf(stderr, "NULL pointer returned from RPC call\n");
        free(station_mem);
        free(source_mem);
        free(EOP_mem);
        return 100;
    }

    printf ("result: this_error = %d\n", p_result->this_error);
    if(!p_result->this_error)
    {
        printf ("result: delay_server_error = %d\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.delay_server_error);
        printf ("result: server_error       = %d\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.server_error);
        printf ("result: model_error        = %d\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.model_error);
        printf ("result: request_id = %ld\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.request_id);
        printf ("result: delay_server = 0x%lX\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.delay_server);
        printf ("result: server_struct_setup_code = 0x%lX\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.server_struct_setup_code);
        printf ("result: date  = %ld\n",        p_result->getDIFX_DELAY_SERVER_1_res_u.response.date);
        printf ("result: time  = %20.16f\n",         p_result->getDIFX_DELAY_SERVER_1_res_u.response.time);
        printf ("result: Num_Stations = %u\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.Num_Stations);
        printf ("result: Num_Sources  = %u\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources);
        printf ("result: result_len   = %u\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_len);
        for(i=0; i < 2; ++i)
        {
            DIFX_DELAY_SERVER_vec V;
            printf ("result: station %d delay  = %24.16E\n", i, p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].delay);
            printf ("result: station %d dry_atmos     = %E\n", i, p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].dry_atmos);
            printf ("result: station %d wet_atmos     = %E\n", i, p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].wet_atmos);
            printf ("result: station %d iono_atmos    = %E\n", i, p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].iono_atmos);
            printf ("result: station %d elev_corr  = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].el_corr*57.296);
            printf ("result: station %d azim_corr  = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].az_corr*57.296);
            printf ("result: station %d elev_geom  = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].el_geom*57.296);
            printf ("result: station %d azim_geom  = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].az_geom*57.296);
            printf ("result: station %d paa   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].primary_axis_angle*57.296);
            printf ("result: station %d saa   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].secondary_axis_angle*57.296);
            printf ("result: station %d msa   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].mount_source_angle*57.296);
            printf ("result: station %d stt   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].station_antenna_theta*57.296);
            printf ("result: station %d stp   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].station_antenna_phi*57.296);
            printf ("result: station %d sot   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].source_antenna_theta*57.296);
            printf ("result: station %d sop   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].source_antenna_phi*57.296);

            V = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].UVW;
            printf ("result: station %d UVW = [%24.16E, %24.16E, %24.16E]\n", i, V.x, V.y, V.z);
            V = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].baselineP2000;
            printf ("result: station %d baselineP2000 = [%24.16E, %24.16E, %24.16E]\n", i, V.x, V.y, V.z);
            V = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].baselineV2000;
            printf ("result: station %d baselineV2000 = [%24.16E, %24.16E, %24.16E]\n", i, V.x, V.y, V.z);
            V = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].baselineA2000;
            printf ("result: station %d baselineA2000 = [%24.16E, %24.16E, %24.16E]\n", i, V.x, V.y, V.z);
            /**/
        }
        delta_dly = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[1].delay;
        delta_dly = fabs(delta_dly - (-2.04212341289221e-02));

        if ((delta_dly >= 1.0e-13)
           || ((p_result->getDIFX_DELAY_SERVER_1_res_u.response.delay_server_error))
           || ((p_result->getDIFX_DELAY_SERVER_1_res_u.response.server_error))
           || ((p_result->getDIFX_DELAY_SERVER_1_res_u.response.model_error))
            )
        {
            printf ("ERROR : CALCServer with version 0x%lX input is returning BAD DATA.\n", (long)(CALC_9_1_RA_SERVER_STRUCT_CODE_5_0_0));
            printf ("        Restart the CALCServer.\n");
            mode_failures++;
        }
        else
        {
            printf ("CALCServer is running normally.\n");
        }
    }
    else {
        printf ("ERROR : CALCServer with version 0x%lX input is returning BAD DATA.\n", (long)(CALC_9_1_RA_SERVER_STRUCT_CODE_5_0_0));
        printf ("        Restart the CALCServer.\n");
        mode_failures++;
    }
        
    if(clnt_freeres(cl, (xdrproc_t) xdr_getDIFX_DELAY_SERVER_1_res, (caddr_t) p_result) != 1)
    {
        fprintf(stderr, "Failed to free results buffer\n");
        exit(EXIT_FAILURE);
    }

    free(station_mem);
    free(source_mem);
    free(EOP_mem);
    
    return mode_failures;
};


int test_CALC_9_1_RA_Server_mode(CLIENT *cl, const char *host)
{
    getDIFX_DELAY_SERVER_1_arg request_args, *p_request;
    getDIFX_DELAY_SERVER_1_res *p_result;

    double delta_dly;
    char   stnnamea[8];
    char   stnnameb[8];
    unsigned short station_ID;
    int    i, v, mode_failures;
    long calcserver_version;
    size_t station_size, source_size, EOP_size;
    void *station_mem, *source_mem, *EOP_mem;

    printf("Checking CALC_9_1_RA_Server\n");
    mode_failures = 0;
    p_request = &request_args;
    memset(p_request, 0, sizeof(getDIFX_DELAY_SERVER_1_arg));
    station_size = sizeof(DIFX_DELAY_SERVER_1_station)*2;
    if((station_mem = malloc(station_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc station memory\n");
        exit(EXIT_FAILURE);
    }
    memset(station_mem, 0, station_size);
    p_request->Num_Stations = 2;
    p_request->station.station_len = 2;
    p_request->station.station_val = station_mem;
    source_size = sizeof(DIFX_DELAY_SERVER_1_source)*1;
    if((source_mem = malloc(source_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc source memory\n");
        exit(EXIT_FAILURE);
    }
    memset(source_mem, 0, source_size);
    p_request->Num_Sources = 1;
    p_request->source.source_len = 1;
    p_request->source.source_val = source_mem;
    EOP_size = sizeof(DIFX_DELAY_SERVER_1_EOP)*5;
    if((EOP_mem = malloc(EOP_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc EOP memory\n");
        exit(EXIT_FAILURE);
    }
    memset(EOP_mem, 0, EOP_size);
    p_request->Num_EOPs = 5;
    p_request->EOP.EOP_len = 5;
    p_request->EOP.EOP_val = EOP_mem;

    p_request->date = 50774;
    p_request->time = 22.0/24.0 + 2.0/(24.0*60.0);
    p_request->verbosity = 5;
    p_request->delay_server = CALC_9_1_RAPROG;
    p_request->server_struct_setup_code = CALC_9_1_RA_SERVER_STRUCT_CODE_5_0_0;
    p_request->request_id = CALC_9_1_RA_SERVER_STRUCT_CODE_5_0_0;
    p_request->ref_frame = 0;

    for (i = 0; i < NUM_DIFX_DELAY_SERVER_1_KFLAGS; i++)
        p_request->kflags[i] = -1;
    p_request->sky_frequency = 10.E9;
    p_request->Use_Server_Station_Table = 0;
    p_request->Use_Server_Source_Table = 0;
    p_request->Use_Server_EOP_Table = 0;

    strcpy (stnnamea, "EC");
    station_ID = (unsigned short)('E') | ((unsigned short)('C') << 8);
    difx_strlcpy(p_request->station.station_val[0].station_name, stnnamea, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[0].antenna_name, stnnamea, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[0].site_name, stnnamea, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->station.station_val[0].site_ID = station_ID;
    p_request->station.station_val[0].station_pos.x =  0.000;
    p_request->station.station_val[0].station_pos.y =  0.000;
    p_request->station.station_val[0].station_pos.z =  0.000;
    p_request->station.station_val[0].station_vel.x =  0.0;
    p_request->station.station_val[0].station_vel.y =  0.0;
    p_request->station.station_val[0].station_vel.z =  0.0;
    p_request->station.station_val[0].station_acc.x =  0.0;
    p_request->station.station_val[0].station_acc.y =  0.0;
    p_request->station.station_val[0].station_acc.z =  0.0;

    difx_strlcpy(p_request->station.station_val[0].axis_type, "altz", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[0].site_type, "fixed", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->station.station_val[0].axis_off = 0.00;
    difx_strlcpy(p_request->station.station_val[0].station_coord_frame, "ITRF2008", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->station.station_val[0].pointing_coord_frame[0] = 0;
    p_request->station.station_val[0].receiver_name[0] = 0;
    p_request->station.station_val[0].pressure = 0.0;
    p_request->station.station_val[0].antenna_pressure = 0.0;
    p_request->station.station_val[0].temperature = 0.0;
    p_request->station.station_val[0].wind_speed = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
    p_request->station.station_val[0].wind_direction = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
    p_request->station.station_val[0].antenna_phys_temperature = 0.0;
    /*
      strcpy (stnnameb, "FD");
      p_request->station_b = &stnnameb[0];
      p_request->b_x =     -1324009.0026;
      p_request->b_y =     -5332182.0834;
      p_request->b_z =      3231962.4355;

      strcpy (axistypeb, "altz");
      p_request->axis_type_b = &axistypeb[0];
      p_request->axis_off_b = 2.1226;
    */

    strcpy (stnnameb, "KP");
    station_ID = (unsigned short)('K') | ((unsigned short)('P') << 8);
    difx_strlcpy(p_request->station.station_val[1].station_name, stnnameb, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[1].antenna_name, stnnameb, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[1].site_name, stnnameb, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->station.station_val[1].site_ID = station_ID;
    p_request->station.station_val[1].station_pos.x =  0.000;
    

    p_request->station.station_val[1].station_pos.x =     -1995678.4969;
    p_request->station.station_val[1].station_pos.y =     -5037317.8209;
    p_request->station.station_val[1].station_pos.z =      3357328.0825;
    p_request->station.station_val[1].station_vel.x =  0.0;
    p_request->station.station_val[1].station_vel.y =  0.0;
    p_request->station.station_val[1].station_vel.z =  0.0;
    p_request->station.station_val[1].station_acc.x =  0.0;
    p_request->station.station_val[1].station_acc.y =  0.0;
    p_request->station.station_val[1].station_acc.z =  0.0;

    difx_strlcpy(p_request->station.station_val[1].axis_type, "altz", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[1].site_type, "fixed", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->station.station_val[1].axis_off = 2.1377;
    difx_strlcpy(p_request->station.station_val[1].station_coord_frame, "ITRF2008", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->station.station_val[1].pointing_coord_frame[0] = 0;
    p_request->station.station_val[1].receiver_name[0] = 0;
    p_request->station.station_val[1].pressure = 0.0;
    p_request->station.station_val[1].antenna_pressure = 0.0;
    p_request->station.station_val[1].temperature = 0.0;
    p_request->station.station_val[1].wind_speed = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
    p_request->station.station_val[1].wind_direction = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
    p_request->station.station_val[1].antenna_phys_temperature = 0.0;


    difx_strlcpy(p_request->source.source_val[0].source_name, "B1937+21", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->source.source_val[0].IAU_name, "B1937+21", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->source.source_val[0].source_type, "star", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    p_request->source.source_val[0].coord_frame[0] = 0;
    p_request->source.source_val[0].ra  =  (TWOPI/24.0)*(19.0 + 39.0/60.0 + 38.560210/3600.0);
    p_request->source.source_val[0].dec =  (TWOPI/360.)*(21.0 + 34.0/60.0 + 59.141000/3600.0);

    p_request->source.source_val[0].dra  = 0.0;
    p_request->source.source_val[0].ddec = 0.0;
    p_request->source.source_val[0].depoch = 0.0;
    p_request->source.source_val[0].parallax = 0.0;

    for (i = 0; i < 5; i++)
    {
        p_request->EOP.EOP_val[i].EOP_time = 50773.0 + (double) i; 
        p_request->EOP.EOP_val[i].tai_utc = 31.0;
    }
    p_request->EOP.EOP_val[0].ut1_utc = 0.285033;
    p_request->EOP.EOP_val[0].xpole   = 0.19744;
    p_request->EOP.EOP_val[0].ypole   = 0.24531;
     
    p_request->EOP.EOP_val[1].ut1_utc = 0.283381;
    p_request->EOP.EOP_val[1].xpole   = 0.19565;
    p_request->EOP.EOP_val[1].ypole   = 0.24256;
     
    p_request->EOP.EOP_val[2].ut1_utc = 0.281678;
    p_request->EOP.EOP_val[2].xpole   = 0.19400;
    p_request->EOP.EOP_val[2].ypole   = 0.24000;
     
    p_request->EOP.EOP_val[3].ut1_utc = 0.280121;
    p_request->EOP.EOP_val[3].xpole   = 0.19244;
    p_request->EOP.EOP_val[3].ypole   = 0.23700;
     
    p_request->EOP.EOP_val[4].ut1_utc = 0.278435;
    p_request->EOP.EOP_val[4].xpole   = 0.19016;
    p_request->EOP.EOP_val[4].ypole   = 0.23414;


    /* set struct_code to check value */
    p_request->server_struct_setup_code = CALC_9_1_RA_SERVER_STRUCT_CODE_0;
    printf("Checking CALC_9_1_RA_SERVER version call to host %s:\n", host);
    printf ("making RPC call to : %s\n", host);
    v = system ("date -u \"+\%FT\%T.\%NZ\"");
    if(v == -1)
    {
     	fprintf(stderr, "Warning -- system() failed\n");
    }

    p_result = getdifx_delay_server_1(p_request, cl);

    /**/
    printf ("return from RPC call\n");
    v = system ("date -u \"+\%FT\%T.\%NZ\"");
    if(v == -1)
    {
     	fprintf(stderr, "Warning -- system() failed\n");
    }
    if(!p_result)
    {
        fprintf(stderr, "NULL pointer returned from RPC call\n");
        free(station_mem);
        free(source_mem);
        free(EOP_mem);
        return 100;
    }
    printf ("CALC_9_1_RA_SERVER implementation version = 0x%lX\n", p_result->getDIFX_DELAY_SERVER_1_res_u.response.server_version);
    calcserver_version = p_result->getDIFX_DELAY_SERVER_1_res_u.response.server_struct_setup_code;
    
    if(clnt_freeres(cl, (xdrproc_t) xdr_getDIFX_DELAY_SERVER_1_res, (caddr_t) p_result) != 1)
    {
        fprintf(stderr, "Failed to free results buffer\n");
        exit(EXIT_FAILURE);
    }
    
    printf ("CALC_9_1_RA_SERVER interface version = 0x%lX\n", 
            calcserver_version);
    if(calcserver_version < CALC_9_1_RA_SERVER_STRUCT_CODE_CURRENT) {
        printf("Warning -- %s CALC_9_1_RA_SERVER version 0x%lX is less than the expected version 0x%lX.\nSome functionality may be missing.  Please consider upgrading to a newer\nversion of CALC_9_1_RA_SERVER.\n", host, calcserver_version, (long)(CALC_9_1_RA_SERVER_STRUCT_CODE_CURRENT));
        mode_failures = 10;
    }
    else if(calcserver_version > CALC_9_1_RA_SERVER_STRUCT_CODE_CURRENT) {
        printf("Warning -- %s CALC_9_1_RA_SERVER version 0x%lX is greater than the expected version 0x%lX.\nSome functionality may have changed.  Please check whether or not this\ncalcserver is backwards compatible.\n", host, calcserver_version, (long)(CALC_9_1_RA_SERVER_STRUCT_CODE_CURRENT));
    }


    /* now test the functionality of known versions */
    if(calcserver_version >= CALC_9_1_RA_SERVER_STRUCT_CODE_5_0_0)
    {
        printf("Checking %s CALC_9_1_RA_SERVER version 0x%lX call:\n", host, (long)(CALC_9_1_RA_SERVER_STRUCT_CODE_5_0_0));
        v = system ("date -u \"+\%FT\%T.\%NZ\"");
        if(v == -1)
        {
            fprintf(stderr, "Warning -- system() failed\n");
        }

        p_request->server_struct_setup_code = CALC_9_1_RA_SERVER_STRUCT_CODE_5_0_0;
        p_result = getdifx_delay_server_1(p_request, cl);

        /**/
        printf ("return from RPC call\n");
        v = system ("date -u \"+\%FT\%T.\%NZ\"");
        if(v == -1)
        {
            fprintf(stderr, "Warning -- system() failed\n");
        }
        if(!p_result)
        {
            fprintf(stderr, "NULL pointer returned from RPC call\n");
            free(station_mem);
            free(source_mem);
            free(EOP_mem);
            return 100;
        }

        printf ("result: this_error = %d\n", p_result->this_error);
        if(!p_result->this_error)
        {
            printf ("result: delay_server_error = %d\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.delay_server_error);
            printf ("result: server_error       = %d\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.server_error);
            printf ("result: model_error        = %d\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.model_error);
            printf ("result: request_id = %ld\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.request_id);
            printf ("result: delay_server = 0x%lX\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.delay_server);
            printf ("result: server_struct_setup_code = 0x%lX\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.server_struct_setup_code);
            printf ("result: date  = %ld\n",        p_result->getDIFX_DELAY_SERVER_1_res_u.response.date);
            printf ("result: time  = %20.16f\n",         p_result->getDIFX_DELAY_SERVER_1_res_u.response.time);
            printf ("result: Num_Stations = %u\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.Num_Stations);
            printf ("result: Num_Sources  = %u\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources);
            printf ("result: result_len   = %u\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_len);
            for(i=0; i < 2; ++i)
            {
                DIFX_DELAY_SERVER_vec V;
                printf ("result: station %d delay  = %24.16E\n", i, p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].delay);
                printf ("result: station %d dry_atmos     = %E\n", i, p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].dry_atmos);
                printf ("result: station %d wet_atmos     = %E\n", i, p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].wet_atmos);
                printf ("result: station %d iono_atmos    = %E\n", i, p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].iono_atmos);
                printf ("result: station %d elev_corr  = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].el_corr*57.296);
                printf ("result: station %d azim_corr  = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].az_corr*57.296);
                printf ("result: station %d elev_geom  = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].el_geom*57.296);
                printf ("result: station %d azim_geom  = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].az_geom*57.296);
                printf ("result: station %d paa   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].primary_axis_angle*57.296);
                printf ("result: station %d saa   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].secondary_axis_angle*57.296);
                printf ("result: station %d msa   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].mount_source_angle*57.296);
                printf ("result: station %d stt   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].station_antenna_theta*57.296);
                printf ("result: station %d stp   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].station_antenna_phi*57.296);
                printf ("result: station %d sot   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].source_antenna_theta*57.296);
                printf ("result: station %d sop   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].source_antenna_phi*57.296);

                V = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].UVW;
                printf ("result: station %d UVW = [%24.16E, %24.16E, %24.16E]\n", i, V.x, V.y, V.z);
                V = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].baselineP2000;
                printf ("result: station %d baselineP2000 = [%24.16E, %24.16E, %24.16E]\n", i, V.x, V.y, V.z);
                V = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].baselineV2000;
                printf ("result: station %d baselineV2000 = [%24.16E, %24.16E, %24.16E]\n", i, V.x, V.y, V.z);
                V = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].baselineA2000;
                printf ("result: station %d baselineA2000 = [%24.16E, %24.16E, %24.16E]\n", i, V.x, V.y, V.z);
                /**/
            }
            delta_dly = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[1].delay;
            delta_dly = fabs(delta_dly - (-2.04212341289221e-02));

            if ((delta_dly >= 1.0e-13)
               || ((p_result->getDIFX_DELAY_SERVER_1_res_u.response.delay_server_error))
               || ((p_result->getDIFX_DELAY_SERVER_1_res_u.response.server_error))
               || ((p_result->getDIFX_DELAY_SERVER_1_res_u.response.model_error))
                )
            {
                printf ("ERROR : CALC_9_1_RA_Server with version 0x%lX input is returning BAD DATA.\n", (long)(CALC_9_1_RA_SERVER_STRUCT_CODE_5_0_0));
                printf ("        Restart the CALC_9_1_RA_Server.\n");
                mode_failures++;
            }
            else
            {
                printf ("CALC_9_1_RA_Server with version 0x%lX input is running normally.\n", (long)(CALC_9_1_RA_SERVER_STRUCT_CODE_5_0_0));
            }
        }
        else {
            printf ("ERROR : CALC_9_1_RA_Server with version 0x%lX input is returning BAD DATA.\n", (long)(CALC_9_1_RA_SERVER_STRUCT_CODE_5_0_0));
            printf ("        Restart the CALC_9_1_RA_Server.\n");
            mode_failures++;
        }
        
        if(clnt_freeres(cl, (xdrproc_t) xdr_getDIFX_DELAY_SERVER_1_res, (caddr_t) p_result) != 1)
        {
            fprintf(stderr, "Failed to free results buffer\n");
            exit(EXIT_FAILURE);
        }
            
    }
    if(calcserver_version >= CALC_9_1_RA_SERVER_STRUCT_CODE_5_1_0)
    {
        strcpy (stnnameb, "RA");
        station_ID = (unsigned short)('R') | ((unsigned short)('A') << 8);
        difx_strlcpy(p_request->station.station_val[1].station_name, stnnameb, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
        difx_strlcpy(p_request->station.station_val[1].antenna_name, stnnameb, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
        difx_strlcpy(p_request->station.station_val[1].site_name, stnnameb, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
        p_request->station.station_val[1].site_ID = station_ID;
        p_request->station.station_val[1].station_pos.x =  0.000;
    

        p_request->station.station_val[1].station_pos.x =   3.376890878000000e+07;
        p_request->station.station_val[1].station_pos.y =  -2.575678163900000e+07;
        p_request->station.station_val[1].station_pos.z =   9.492685457100001e+07;
        p_request->station.station_val[1].station_vel.x =  -2.644577860000000e+02;
        p_request->station.station_val[1].station_vel.y =   1.117674795000000e+03;
        p_request->station.station_val[1].station_vel.z =   2.011362193000000e+03;
        p_request->station.station_val[1].station_acc.x =  -1.196600E-02;
        p_request->station.station_val[1].station_acc.y =   9.138000E-03;
        p_request->station.station_val[1].station_acc.z =  -3.365000E-02;

        difx_strlcpy(p_request->station.station_val[1].axis_type, "space", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
        difx_strlcpy(p_request->station.station_val[1].site_type, "earth_orbit", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
        p_request->station.station_val[1].axis_off = 2.1377;
    difx_strlcpy(p_request->station.station_val[1].station_coord_frame, "J2000_Earth", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
        p_request->station.station_val[1].pointing_coord_frame[0] = 0;
        p_request->station.station_val[1].receiver_name[0] = 0;
        p_request->station.station_val[1].pressure = 0.0;
        p_request->station.station_val[1].antenna_pressure = 0.0;
        p_request->station.station_val[1].temperature = 0.0;
        p_request->station.station_val[1].wind_speed = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
        p_request->station.station_val[1].wind_direction = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
        p_request->station.station_val[1].antenna_phys_temperature = 0.0;

        printf("Checking %s CALC_9_1_RA_SERVER version 0x%lX call:\n", host, (long)(CALC_9_1_RA_SERVER_STRUCT_CODE_5_1_0));
        v = system ("date -u \"+\%FT\%T.\%NZ\"");
        if(v == -1)
        {
            fprintf(stderr, "Warning -- system() failed\n");
        }
        if(!p_result)
        {
            fprintf(stderr, "NULL pointer returned from RPC call\n");
            free(station_mem);
            free(source_mem);
            free(EOP_mem);
            return 100;
        }

        p_request->server_struct_setup_code = CALC_9_1_RA_SERVER_STRUCT_CODE_5_1_0;
        p_result = getdifx_delay_server_1(p_request, cl);

        /**/
        printf ("return from RPC call\n");
        v = system ("date -u \"+\%FT\%T.\%NZ\"");
        if(v == -1)
        {
            fprintf(stderr, "Warning -- system() failed\n");
        }

        printf ("result: this_error = %d\n", p_result->this_error);
        if(!p_result->this_error)
        {
            printf ("result: delay_server_error = %d\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.delay_server_error);
            printf ("result: server_error       = %d\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.server_error);
            printf ("result: model_error        = %d\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.model_error);
            printf ("result: request_id = %ld\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.request_id);
            printf ("result: delay_server = 0x%lX\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.delay_server);
            printf ("result: server_struct_setup_code = 0x%lX\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.server_struct_setup_code);
            printf ("result: date  = %ld\n",        p_result->getDIFX_DELAY_SERVER_1_res_u.response.date);
            printf ("result: time  = %20.16f\n",         p_result->getDIFX_DELAY_SERVER_1_res_u.response.time);
            printf ("result: Num_Stations = %u\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.Num_Stations);
            printf ("result: Num_Sources  = %u\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources);
            printf ("result: result_len   = %u\n",   p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_len);
            for(i=0; i < 2; ++i)
            {
                DIFX_DELAY_SERVER_vec V;
                printf ("result: station %d delay  = %24.16E\n", i, p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].delay);
                printf ("result: station %d dry_atmos     = %E\n", i, p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].dry_atmos);
                printf ("result: station %d wet_atmos     = %E\n", i, p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].wet_atmos);
                printf ("result: station %d iono_atmos     = %E\n", i, p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].iono_atmos);
                printf ("result: station %d elev_corr  = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].el_corr*57.296);
                printf ("result: station %d azim_corr  = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].az_corr*57.296);
                printf ("result: station %d elev_geom  = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].el_geom*57.296);
                printf ("result: station %d azim_geom  = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].az_geom*57.296);
                printf ("result: station %d paa   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].primary_axis_angle*57.296);
                printf ("result: station %d saa   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].secondary_axis_angle*57.296);
                printf ("result: station %d msa   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].mount_source_angle*57.296);
                printf ("result: station %d stt   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].station_antenna_theta*57.296);
                printf ("result: station %d stp   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].station_antenna_phi*57.296);
                printf ("result: station %d sot   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].source_antenna_theta*57.296);
                printf ("result: station %d sop   = %20.16f\n", i,      p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].source_antenna_phi*57.296);

                V = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].UVW;
                printf ("result: station %d UVW = [%24.16E, %24.16E, %24.16E]\n", i, V.x, V.y, V.z);
                V = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].baselineP2000;
                printf ("result: station %d baselineP2000 = [%24.16E, %24.16E, %24.16E]\n", i, V.x, V.y, V.z);
                V = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].baselineV2000;
                printf ("result: station %d baselineV2000 = [%24.16E, %24.16E, %24.16E]\n", i, V.x, V.y, V.z);
                V = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[i].baselineA2000;
                printf ("result: station %d baselineA2000 = [%24.16E, %24.16E, %24.16E]\n", i, V.x, V.y, V.z);
                /**/
            }
            delta_dly = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[1].delay;
            delta_dly = fabs(delta_dly - (-2.3306155663663350e-01));

            if ((delta_dly >= 1.0e-13)
               || ((p_result->getDIFX_DELAY_SERVER_1_res_u.response.delay_server_error))
               || ((p_result->getDIFX_DELAY_SERVER_1_res_u.response.server_error))
               || ((p_result->getDIFX_DELAY_SERVER_1_res_u.response.model_error))
                )
            {
                printf ("ERROR : CALC_9_1_RA_Server with version 0x%lX input is returning BAD DATA.\n", (long)(CALC_9_1_RA_SERVER_STRUCT_CODE_5_1_0));
                printf ("        Restart the CALC_9_1_RA_Server.\n");
                mode_failures++;
            }
            else
            {
                printf ("CALC_9_1_RA_Server with version 0x%lX input is running normally.\n", (long)(CALC_9_1_RA_SERVER_STRUCT_CODE_5_1_0));
            }
        }
        else {
            printf ("ERROR : CALC_9_1_RA_Server with version 0x%lX input is returning BAD DATA.\n", (long)(CALC_9_1_RA_SERVER_STRUCT_CODE_5_1_0));
            printf ("        Restart the CALC_9_1_RA_Server.\n");
            mode_failures++;
        }
        
        if(clnt_freeres(cl, (xdrproc_t) xdr_getDIFX_DELAY_SERVER_1_res, (caddr_t) p_result) != 1)
        {
            fprintf(stderr, "Failed to free results buffer\n");
            exit(EXIT_FAILURE);
        }
    }

    free(station_mem);
    free(source_mem);
    free(EOP_mem);
    
    return mode_failures;
};










int main (int argc, char *argv[])
{
    int failures = 0;
    int CALCServer = 0;
    int CALC_9_1_RA_Server = 0;
    CLIENT *cl;
    char *host;
    char *localhost = "localhost";

    if(argc > 2)
    {
        fprintf(stderr, "Usage : %s server_hostname\n\n", argv[0]);
        return 0;
    }
    if(argc == 2)
    {
        host = argv[1];
    }
    else if((host = getenv("DIFX_DELAY_SERVER")) != NULL)
    {
    }
    else {
        host = localhost;
    }

    if (!(cl = clnt_create (host, DIFX_DELAY_SERVER_PROG, DIFX_DELAY_SERVER_VERS_1, "tcp")))
    {
        clnt_pcreateerror (host);
        fprintf(stderr, "ERROR: rpc clnt_create fails for host : '%s'\n", host);
        exit(EXIT_FAILURE);
    }

    CALCServer = test_CALCServer_mode(cl, host);
    failures += CALCServer;
    CALC_9_1_RA_Server = test_CALC_9_1_RA_Server_mode(cl, host);
    failures += CALC_9_1_RA_Server;
    
    clnt_destroy (cl);

    if((CALCServer))
    {
        fprintf(stderr, "ERROR: failures getting results from CALCServer\n");
    }
    if((CALC_9_1_RA_Server))
    {
        fprintf(stderr, "ERROR: failures getting results from CALC_9_1_RA_Server\n");
    }
    if((failures))
    {
        fprintf (stderr, "ERROR: DiFX_Delay_Server is receiving BAD DATA from some servers.\n");
        fprintf (stderr, "       You may need to restart the delay servers.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
};
