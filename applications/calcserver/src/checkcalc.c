#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <rpc/rpc.h>
#include "MATHCNST.H"
#include "CALCServer.h"

static struct timeval TIMEOUT = {10, 0};

int main (argc, argv)
     int argc;
     char *argv[];
{

     struct getCALC_arg request_args, *p_request;
     struct getCALC_res *p_result;

     double delta_dly;
     char   stnnamea[8], srcname[12], axistypea[12];
     char   stnnameb[8], axistypeb[12];
     int    i, v, mode_failures;
     long calcserver_version;

     CLIENT    *cl;

     if(argc != 2)
     {
         fprintf(stderr, "Usage : %s <CalcServer host>\n\n", argv[0]);
	 return 0;
     }

     if (!(cl = clnt_create (argv[1], CALCPROG, CALCVERS, "tcp")))
     {
        clnt_pcreateerror (argv[1]);
        printf("ERROR: rpc clnt_create fails for host : %-s\n", argv[1]);
        exit(0);
     }

     mode_failures = 0;
     p_request = &request_args;
     memset(p_request, 0, sizeof(struct getCALC_arg));

     p_request->date = 50774;
     p_request->time = 22.0/24.0 + 2.0/(24.0*60.0);
     p_request->struct_code = CALC_SERVER_STRUCT_CODE_5_0_0;
     p_request->request_id = CALC_SERVER_STRUCT_CODE_5_0_0;
     p_request->ref_frame = 0;

     for (i = 0; i < 64; i++)
         p_request->kflags[i] = -1;

     strcpy (stnnamea, "EC");
     p_request->station_a = &stnnamea[0];
     p_request->a_x =  0.000;
     p_request->a_y =  0.000;
     p_request->a_z =  0.000;
     p_request->a_dx =  0.0;
     p_request->a_dy =  0.0;
     p_request->a_dz =  0.0;
     p_request->a_ddx =  0.0;
     p_request->a_ddy =  0.0;
     p_request->a_ddz =  0.0;

     strcpy (axistypea, "altz");
     p_request->axis_type_a = &axistypea[0];
     p_request->axis_off_a = 0.00;
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
     p_request->station_b = &stnnameb[0];
     p_request->b_x =     -1995678.4969;
     p_request->b_y =     -5037317.8209;
     p_request->b_z =      3357328.0825;
     p_request->b_dx =  0.0;
     p_request->b_dy =  0.0;
     p_request->b_dz =  0.0;
     p_request->b_ddx =  0.0;
     p_request->b_ddy =  0.0;
     p_request->b_ddz =  0.0;

     strcpy (axistypeb, "altz");
     p_request->axis_type_b = &axistypeb[0];
     p_request->axis_off_b = 2.1377;


     strcpy (srcname, "B1937+21");

     p_request->source = &srcname[0];
     p_request->ra  =  (TWOPI/24.0)*(19.0 + 39.0/60.0 + 38.560210/3600.0);
     p_request->dec =  (TWOPI/360.)*(21.0 + 34.0/60.0 + 59.141000/3600.0);

     p_request->dra  = 0.0;
     p_request->ddec = 0.0;
     p_request->depoch = 0.0;
     p_request->parallax = 0.0;

     p_request->pressure_a = 0.0;
     p_request->pressure_b = 0.0;

     for (i = 0; i < 5; i++)
     {
         p_request->EOP_time[i] = 50773.0 + (double) i; 
         p_request->tai_utc[i] = 31.0;
     }
     p_request->ut1_utc[0] = 0.285033;
     p_request->xpole[0]   = 0.19744;
     p_request->ypole[0]   = 0.24531;
     
     p_request->ut1_utc[1] = 0.283381;
     p_request->xpole[1]   = 0.19565;
     p_request->ypole[1]   = 0.24256;
     
     p_request->ut1_utc[2] = 0.281678;
     p_request->xpole[2]   = 0.19400;
     p_request->ypole[2]   = 0.24000;
     
     p_request->ut1_utc[3] = 0.280121;
     p_request->xpole[3]   = 0.19244;
     p_request->ypole[3]   = 0.23700;
     
     p_request->ut1_utc[4] = 0.278435;
     p_request->xpole[4]   = 0.19016;
     p_request->ypole[4]   = 0.23414;


     /* set struct_code to check value */
     p_request->struct_code = CALC_SERVER_STRUCT_CODE_0;
     printf("Checking %s version call:\n", argv[1]);
     printf ("making RPC call to : %s\n", argv[1]);
     v = system ("date -u \"+\%FT\%T.\%NZ\"");
     if(v == -1)
     {
     	fprintf(stderr, "Warning -- system() failed\n");
     }

     p_result = getcalc_1(p_request, cl);

     /**/
     printf ("return from RPC call\n");
     v = system ("date -u \"+\%FT\%T.\%NZ\"");
     if(v == -1)
     {
     	fprintf(stderr, "Warning -- system() failed\n");
     }
     calcserver_version = p_result->getCALC_res_u.record.struct_code;
     printf ("calcserver version = 0x%lX\n", 
              calcserver_version);
     if(calcserver_version < CALC_SERVER_STRUCT_CODE_CURRENT) {
         printf("Warning -- %s version 0x%lX is less than the expected version 0x%lX.\nSome functionality may be missing.  Please consider upgrading to a newer\nversion of calcserver.\n", argv[1], calcserver_version, (long)(CALC_SERVER_STRUCT_CODE_CURRENT));
     }
     else if(calcserver_version > CALC_SERVER_STRUCT_CODE_CURRENT) {
         printf("Warning -- %s version 0x%lX is greater than the expected version 0x%lX.\nSome functionality may have changed.  Please check whether or not this\ncalcserver is backwards compatible.\n", argv[1], calcserver_version, (long)(CALC_SERVER_STRUCT_CODE_CURRENT));
     }


     /* now test the functionality of known versions */
     if(calcserver_version >= CALC_SERVER_STRUCT_CODE_5_0_0)
     {
         printf("Checking %s version 0x%lX call:\n", argv[1], (long)(CALC_SERVER_STRUCT_CODE_5_0_0));
         v = system ("date -u \"+\%FT\%T.\%NZ\"");
         if(v == -1)
         {
             fprintf(stderr, "Warning -- system() failed\n");
         }

         p_request->struct_code = CALC_SERVER_STRUCT_CODE_5_0_0;
         p_result = getcalc_1(p_request, cl);

         /**/
         printf ("return from RPC call\n");
         v = system ("date -u \"+\%FT\%T.\%NZ\"");
         if(v == -1)
         {
             fprintf(stderr, "Warning -- system() failed\n");
         }

         printf ("result: request_id = %ld\n", 
                 p_result->getCALC_res_u.record.request_id);
         printf ("result: date  = %ld\n", 
                 p_result->getCALC_res_u.record.date);
         printf ("result: time  = %e\n",
                 p_result->getCALC_res_u.record.time);
         printf ("result: delay[0] = %16.10e\n", 
                 p_result->getCALC_res_u.record.delay[0]);
         printf ("result: delay[1]  = %16.10e\n", 
                 p_result->getCALC_res_u.record.delay[1]);
         printf ("result: dry_atmos[0]  = %e\n", 
                 p_result->getCALC_res_u.record.dry_atmos[0]);
         printf ("result: wet_atmos[0]  = %e\n", 
                 p_result->getCALC_res_u.record.wet_atmos[0]);
         printf ("result: elev[0]  = %e\n", 
                 p_result->getCALC_res_u.record.el[0]*57.296);
         printf ("result: azim[0]  = %e\n", 
                 p_result->getCALC_res_u.record.az[0]*57.296);
         /**/
         delta_dly = p_result->getCALC_res_u.record.delay[0];
         delta_dly = fabs(delta_dly - (-2.04212341289221e-02));

         if (delta_dly >= 1.0e-13)
         {
             printf ("ERROR : CalcServer with version 0x%lX input is returning BAD DATA.\n", (long)(CALC_SERVER_STRUCT_CODE_5_0_0));
             printf ("        Restart the CALCServer.\n");
             mode_failures++;
         }
         else
         {
             printf ("CALCServer with version 0x%lX input is running normally.\n", (long)(CALC_SERVER_STRUCT_CODE_5_0_0));
         }
     }
     if(calcserver_version >= CALC_SERVER_STRUCT_CODE_5_1_0)
     {
         strcpy (stnnameb, "RA");
         p_request->station_b = &stnnameb[0];
         p_request->b_x =    3.376890878000000e+07;
         p_request->b_y =   -2.575678163900000e+07;
         p_request->b_z =    9.492685457100001e+07;
         p_request->b_dx =  -2.644577860000000e+02;
         p_request->b_dy =   1.117674795000000e+03;
         p_request->b_dz =   2.011362193000000e+03;
         p_request->b_ddx = -1.196600E-02;
         p_request->b_ddy =  9.138000E-03;
         p_request->b_ddz = -3.365000E-02;

         
         strcpy (axistypeb, "space");
         p_request->axis_type_b = &axistypeb[0];
         p_request->axis_off_b = 2.1377;

         printf("Checking %s version 0x%lX call:\n", argv[1], (long)(CALC_SERVER_STRUCT_CODE_5_1_0));
         v = system ("date -u \"+\%FT\%T.\%NZ\"");
         if(v == -1)
         {
             fprintf(stderr, "Warning -- system() failed\n");
         }

         p_request->struct_code = CALC_SERVER_STRUCT_CODE_5_1_0;
         p_result = getcalc_1(p_request, cl);

         /**/
         printf ("return from RPC call\n");
         v = system ("date -u \"+\%FT\%T.\%NZ\"");
         if(v == -1)
         {
             fprintf(stderr, "Warning -- system() failed\n");
         }

         printf ("result: request_id = %ld\n", 
                 p_result->getCALC_res_u.record.request_id);
         printf ("result: date  = %ld\n", 
                 p_result->getCALC_res_u.record.date);
         printf ("result: time  = %e\n",
                 p_result->getCALC_res_u.record.time);
         printf ("result: delay[0] = %23.16e\n", 
                 p_result->getCALC_res_u.record.delay[0]);
         printf ("result: delay[1]  = %23.16e\n", 
                 p_result->getCALC_res_u.record.delay[1]);
         printf ("result: dry_atmos[0]  = %e\n", 
                 p_result->getCALC_res_u.record.dry_atmos[0]);
         printf ("result: wet_atmos[0]  = %e\n", 
                 p_result->getCALC_res_u.record.wet_atmos[0]);
         printf ("result: elev[0]  = %e\n", 
                 p_result->getCALC_res_u.record.el[0]*57.296);
         printf ("result: azim[0]  = %e\n", 
                 p_result->getCALC_res_u.record.az[0]*57.296);
         /**/
         delta_dly = p_result->getCALC_res_u.record.delay[0];
         delta_dly = fabs(delta_dly - (-2.3306155663663350e-01));

         if (delta_dly >= 1.0e-13)
         {
             printf ("ERROR : CalcServer with version 0x%lX input is returning BAD DATA.\n", (long)(CALC_SERVER_STRUCT_CODE_5_1_0));
             printf ("        Restart the CALCServer.\n");
             mode_failures++;
         }
         else
         {
             printf ("CALCServer with version 0x%lX input is running normally.\n", (long)(CALC_SERVER_STRUCT_CODE_5_1_0));
         }
     }
    
     clnt_destroy (cl);

     if((mode_failures))
     {
         printf ("ERROR : CalcServer is returning BAD DATA.\n");
         printf ("        Restart the CALCServer.\n");
         return 1;
     }

     return 0;
};

getCALC_res *
getcalc_1(argp, clnt)
	struct getCALC_arg *argp;
	CLIENT *clnt;
{
	static getCALC_res clnt_res;
        enum clnt_stat clnt_stat;

	memset((char *)&clnt_res, 0, sizeof (clnt_res));
	clnt_stat = clnt_call(clnt, GETCALC,
		(xdrproc_t) xdr_getCALC_arg, (caddr_t) argp,
		(xdrproc_t) xdr_getCALC_res, (caddr_t) &clnt_res,
		TIMEOUT);

        if (clnt_stat != RPC_SUCCESS)
           printf ("clnt_call failed\n");

	return (&clnt_res);
}








