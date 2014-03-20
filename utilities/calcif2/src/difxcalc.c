/***************************************************************************
 *   Copyright (C) 2008-2013 by Walter Brisken & Adam Deller               *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "MATHCNST.H"
#include "difxcalc.h"
#include "externaldelay.h"
#include "poly.h"

#define MAX_EOPS 5

static struct timeval TIMEOUT = {10, 0};

/* see DifxPolyModel in difxio:difx_input.h */
struct modelTemp
{
	double delay[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
	double dry[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
	double wet[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
	double az[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
	double elcorr[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
	double elgeom[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
	double parangle[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
	double sc_gs_delay[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
	double gs_clock_delay[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
	double msa[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
	double u[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
	double v[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
	double w[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
};

struct CalcResults
{
	enum AberCorr aberCorr;
	int nRes;	/* 3 if UVW to be calculated via tau derivatives */
	double delta;
	struct getCALC_res res[3];
};


static int check_calcserver_operation(const DifxInput *D, CalcParams *p, int verbose, long* calcserver_version_available)
{

     struct getCALC_arg request_args, *p_request;
     struct getCALC_res request_result, *p_result;

     double delta_dly;
     char   stnnamea[8], srcname[12], axistypea[12];
     char   stnnameb[8], axistypeb[12];
     int    i, v, mode_failures, return_code;
     long calcserver_version;
     enum clnt_stat clnt_stat;

     CLIENT    *cl;

     mode_failures = 0;
     return_code = 0;
     *calcserver_version_available = 0;
     p_request = &request_args;
     p_result = &request_result;
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

     memset(p_result, 0, sizeof(struct getCALC_res));
     clnt_stat = clnt_call(p->clnt, GETCALC,
                           (xdrproc_t)xdr_getCALC_arg, 
                           (caddr_t)p_request,
                           (xdrproc_t)xdr_getCALC_res, 
                           (caddr_t)(p_result),
                           TIMEOUT);
     if(clnt_stat != RPC_SUCCESS)
     {
         fprintf(stderr, "clnt_call failed!\n");
         return -1;
     }
     if(p_result->error)
     {
         fprintf(stderr,"Error: callCalc: %s\n", p_result->getCALC_res_u.errmsg);
         return -2;
     }

     calcserver_version = p_result->getCALC_res_u.record.struct_code;
     *calcserver_version_available = calcserver_version;
     if(verbose >= 1) {
         printf("CALCServer version 0x%lX detected\n", calcserver_version);
     }
     if(calcserver_version < CALC_SERVER_STRUCT_CODE_CURRENT) {
         return_code = 1;
         if(verbose >= 0) {
             fprintf(stderr, "Warning -- CALCServer version 0x%lX is less than the expected version 0x%lX.\nSome functionality may be missing.  Please consider upgrading to a newer\nversion of CALCServer.\n", calcserver_version, (long)(CALC_SERVER_STRUCT_CODE_CURRENT));
         }
     }
     else if(calcserver_version > CALC_SERVER_STRUCT_CODE_CURRENT) {
         if(verbose >= 1) {
             printf("Warning -- CALCServer version 0x%lX is greater than the expected version 0x%lX.\nSome functionality may have changed.  Please check whether or not this\nCALCServer is backwards compatible.\n", calcserver_version, (long)(CALC_SERVER_STRUCT_CODE_CURRENT));
         }
     }


     /* now test the functionality of known versions */
     if(calcserver_version >= CALC_SERVER_STRUCT_CODE_5_0_0)
     {

         /**/
         p_request->struct_code = CALC_SERVER_STRUCT_CODE_5_0_0;
         memset(p_result, 0, sizeof(struct getCALC_res));
         clnt_stat = clnt_call(p->clnt, GETCALC,
                               (xdrproc_t)xdr_getCALC_arg, 
                               (caddr_t)p_request,
                               (xdrproc_t)xdr_getCALC_res, 
                               (caddr_t)(p_result),
                               TIMEOUT);
         if(clnt_stat != RPC_SUCCESS)
         {
             fprintf(stderr, "clnt_call failed!\n");
             return -1;
         }
         if(p_result->error)
         {
             fprintf(stderr,"Error: callCalc: %s\n", p_result->getCALC_res_u.errmsg);
             return -2;
         }

         if(verbose >= 2) {
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
         }
         /**/
         delta_dly = p_result->getCALC_res_u.record.delay[0];
         delta_dly = fabs(delta_dly - (-2.04212341289221e-02));

         if (delta_dly >= 1.0e-13)
         {
             fprintf (stderr, "ERROR : CalcServer with version 0x%lX input is returning BAD DATA.\n", (long)(CALC_SERVER_STRUCT_CODE_5_0_0));
             fprintf (stderr, "        Restart the CALCServer.\n");
             return_code = -3;
             mode_failures++;
         }
         else
         {
             if(verbose >= 2) {
                 printf ("CALCServer with version 0x%lX input is running normally.\n", (long)(CALC_SERVER_STRUCT_CODE_5_0_0));
             }
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

         /**/
         p_request->struct_code = CALC_SERVER_STRUCT_CODE_5_1_0;
         memset(p_result, 0, sizeof(struct getCALC_res));
         clnt_stat = clnt_call(p->clnt, GETCALC,
                               (xdrproc_t)xdr_getCALC_arg, 
                               (caddr_t)p_request,
                               (xdrproc_t)xdr_getCALC_res, 
                               (caddr_t)(p_result),
                               TIMEOUT);
         if(clnt_stat != RPC_SUCCESS)
         {
             fprintf(stderr, "clnt_call failed!\n");
             return -1;
         }
         if(p_result->error)
         {
             fprintf(stderr,"Error: callCalc: %s\n", p_result->getCALC_res_u.errmsg);
             return -2;
         }


         if(verbose >= 2) {
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
         }
         /**/
         delta_dly = p_result->getCALC_res_u.record.delay[0];
         delta_dly = fabs(delta_dly - (-2.3306155663663350e-01));
         if (delta_dly >= 1.0e-13)
         {
             fprintf (stderr, "ERROR : CalcServer with version 0x%lX input is returning BAD DATA.\n", (long)(CALC_SERVER_STRUCT_CODE_5_1_0));
             fprintf (stderr, "        Restart the CALCServer.\n");
             return_code = -4;
             mode_failures++;
         }
         else
         {
             if(verbose >= 2) {
             printf ("CALCServer with version 0x%lX input is running normally.\n", (long)(CALC_SERVER_STRUCT_CODE_5_1_0));
             }
         }
     }
     else {
         /* do we have spacecraft or otherwise need version 5 0 0? */
         int need_spacecraft = CheckInputForSpacecraft(D, p);
         if((need_spacecraft)) {
             fprintf(stderr, "Error: spacecraft delay calculation needs CALCServer version 0x%lX or above\n", (long)(CALC_SERVER_STRUCT_CODE_5_1_0));
             return_code = -5;
         }
     }

     if((mode_failures))
     {
         fprintf (stderr, "ERROR : CalcServer is returning BAD DATA.\n");
         fprintf (stderr, "        Restart the CALCServer.\n");
     }

     return return_code;
};




int difxCalcInit(const DifxInput *D, CalcParams *p, int verbose)
{
	struct getCALC_arg *request;
	int i;
        long calcserver_version_available;

        i = check_calcserver_operation(D, p, verbose, &calcserver_version_available);
        if(i < 0)
        {
            return -3;
        }

	request = &(p->request);

	memset(request, 0, sizeof(struct getCALC_arg));

        request->struct_code = calcserver_version_available;
	request->request_id = CALC_SERVER_STRUCT_CODE_5_0_0;
	for(i = 0; i < 64; i++)
	{
		request->kflags[i] = -1;
	}
	request->ref_frame = 0;
	
	request->pressure_a = 0.0;
	request->pressure_b = 0.0;

	request->station_a = "EC";
	request->a_x = 0.0;
	request->a_y = 0.0;
	request->a_z = 0.0;
	request->a_dx = 0.0;
	request->a_dy = 0.0;
	request->a_dz = 0.0;
	request->a_ddx = 0.0;
	request->a_ddy = 0.0;
	request->a_ddz = 0.0;
	request->axis_type_a = "AZEL";
	request->axis_off_a = 0.0;
	request->station_b = "GARBAGE";
	request->b_x = 0.0;
	request->b_y = 0.0;
	request->b_z = 0.0;
	request->b_dx = 0.0;
	request->b_dy = 0.0;
	request->b_dz = 0.0;
	request->b_ddx = 0.0;
	request->b_ddy = 0.0;
	request->b_ddz = 0.0;
	request->axis_type_b = "fail";
	request->axis_off_b = 0.0;

	if(D->nEOP >= MAX_EOPS)
	{
		for(i = 0; i < MAX_EOPS; ++i)
		{
			request->EOP_time[i] = D->eop[i].mjd;
			request->tai_utc[i]  = D->eop[i].tai_utc;
			request->ut1_utc[i]  = D->eop[i].ut1_utc;
			request->xpole[i]    = D->eop[i].xPole;
			request->ypole[i]    = D->eop[i].yPole;
		}
	}
	else
	{
		fprintf(stderr, "Not enough eop values present (%d < %d)\n", D->nEOP, MAX_EOPS);

		return -1;
	}

	/* check that eops bracket the observation */
	if(D->eop[MAX_EOPS-1].mjd < D->mjdStart ||
	   D->eop[0].mjd          > D->mjdStop)
	{
		fprintf(stderr, "EOPs don't bracket the observation.\n");

		return -2;
	}

	return 0;
}

int CheckInputForSpacecraft(const DifxInput *D, CalcParams *p)
{
    int sc;
    if(p->allowNegDelay)
    {
        return;
    }
    for(sc = 0; sc < D->nSpacecraft; sc++)
    {
        if((D->spacecraft[sc].is_antenna))
        {
            fprintf(stderr, "Warning: spacecraft antenna present, but allowNegDelay not set.\nUse the --allow-neg-delay option in the future.\nSetting allowNegDelay.\n");
            p->allowNegDelay = 1;
            return +1;
        }
    }
    return 0;
}


static int calcSpacecraftSourcePosition(const DifxInput *D, struct getCALC_arg *request, int spacecraftId, int sourceId)
{
	DifxSpacecraft *sc;
        DifxSource* source;
	sixVector pos;
	int r;
	long double r2, d;
	double muRA, muDec;
	
	sc = D->spacecraft + spacecraftId;
	source = D->source + sourceId;

	if((sc->is_antenna))
	{
		return -2;
	}
        if(source->sc_epoch == 0.0)
        {
            r = evaluateDifxSpacecraftSource(sc, request->date, request->time, &pos);
        }
        else
        {
            int mjd = source->sc_epoch;
            double frac = source->sc_epoch-mjd;
            r = evaluateDifxSpacecraftSource(sc, mjd, frac, &pos);
        }
	if(r < 0)
	{
		return -1;
	}

	r2 = pos.X*pos.X + pos.Y*pos.Y;
	d = sqrtl(r2 + pos.Z*pos.Z);

	request->ra  =  atan2(pos.Y, pos.X);
	request->dec =  atan2(pos.Z, sqrtl(r2));
	request->parallax = 3.08568025e16/d;

        if(source->sc_epoch == 0.0)
        {
            /* proper motion in radians/day */
            muRA = (pos.X*pos.dY - pos.Y*pos.dX)/r2;
            muDec = (r2*pos.dZ - pos.X*pos.Z*pos.dX - pos.Y*pos.Z*pos.dY)/
		(d*d*sqrtl(r2));
            
            /* convert to arcsec/yr */
            /* The J2000 system uses the Julian year for proper motion */
            muRA *= (180.0*3600.0/M_PI)*365.25;
            muDec *= (180.0*3600.0/M_PI)*365.25;

            request->dra  = muRA;
            request->ddec = muDec;

            request->depoch = pos.mjd + pos.fracDay;
        }
        else
        {
            request->dra  = 0.0;
            request->ddec = 0.0;

            request->depoch = 0.0;
        }

	return 0;
}

static int calcSpacecraftPhaseCenterPosition(const DifxInput *D, struct getCALC_arg *request, int spacecraftId, int phasecenterId)
{
	DifxSpacecraft *sc;
        DifxSource* phase_center;
	sixVector pos;
	int r;
	long double r2, d;
	double muRA, muDec;
	
	sc = D->spacecraft + spacecraftId;
	phase_center = D->source + phasecenterId;

	if((sc->is_antenna))
	{
		return -2;
	}
        if(phase_center->sc_epoch == 0.0)
        {
            r = evaluateDifxSpacecraftSource(sc, request->date, request->time, &pos);
        }
        else
        {
            int mjd = phase_center->sc_epoch;
            double frac = phase_center->sc_epoch-mjd;
            r = evaluateDifxSpacecraftSource(sc, mjd, frac, &pos);
        }
	if(r < 0)
	{
		return -1;
	}

        request->pointing_pos_b_z[0] = pos.X;
        request->pointing_pos_b_z[1] = pos.Y;
        request->pointing_pos_b_z[2] = pos.Z;
        
        if(phase_center->sc_epoch == 0.0)
        {
            /* proper motion comes back as radians per day.  Convert to rad/s */
            request->pointing_vel_b_z[0] = pos.dX / 86400.0;
            request->pointing_vel_b_z[1] = pos.dY / 86400.0;
            request->pointing_vel_b_z[2] = pos.dZ / 86400.0;
            request->pointing_epoch_b = pos.mjd + pos.fracDay;
            request->pointing_parallax = 0.0;
        }
        else
        {
            request->pointing_vel_b_z[0] = 0.0;
            request->pointing_vel_b_z[1] = 0.0;
            request->pointing_vel_b_z[2] = 0.0;
            request->pointing_epoch_b = 0.0;
            request->pointing_parallax = 0.0;
        }

	return 0;
}

static int calcSpacecraftAntennaPosition(const DifxInput *D, struct getCALC_arg *request, int spacecraftId)
{
	DifxSpacecraft *sc;
	nineVector pos;
        spacecraftAxisVectors pointing_direction, pointing_velocity;
	int r;
	
	sc = D->spacecraft + spacecraftId;

	if((!sc->is_antenna))
	{
		return -2;
	}
	r = evaluateDifxSpacecraftAntenna(sc, request->date, request->time, &pos);
	if(r < 0)
	{
		return -1;
	}
        request->b_x = pos.X;
	request->b_y = pos.Y;
	request->b_z = pos.Z;
        request->b_dx = pos.dX;
	request->b_dy = pos.dY;
	request->b_dz = pos.dZ;
        request->b_ddx = pos.ddX;
	request->b_ddy = pos.ddY;
	request->b_ddz = pos.ddZ;
        

        /* Now work on the pointing axes */
        r = evaluateDifxSpacecraftAntennaAxisVectors(sc,
                                                     request->date,
                                                     request->time,
                                                    &pointing_direction,
                                                    &pointing_velocity);
        if(r < 0) {
            return -2;
        }
        for(r=0; r < 3; r++) {
            request->pointing_pos_b_x[r] = pointing_direction.X[r];
            request->pointing_vel_b_x[r] =  pointing_velocity.X[r];
            request->pointing_pos_b_y[r] = pointing_direction.Y[r];
            request->pointing_vel_b_y[r] =  pointing_velocity.Y[r];
            request->pointing_pos_b_z[r] = pointing_direction.Z[r];
            request->pointing_vel_b_z[r] =  pointing_velocity.Z[r];
        }

	return 0;
}

static int callCalc(struct getCALC_arg *request, struct CalcResults *results, const CalcParams *p)
{
	double ra, dec;
	int i;
	enum clnt_stat clnt_stat;
#ifdef CALCIF2_DEBUG
        printf("CALCIF2_DEBUG %s:%d Source %s\n", __FILE__, __LINE__, request->source);
        printf("CALCIF2_DEBUG %s:%d MJD %d seconds %.3f\n", __FILE__, __LINE__, (int)request->date, request->time*86400.0);
        printf("CALCIF2_DEBUG %s:%d Antenna A %s %s\n", __FILE__, __LINE__, request->station_a, request->axis_type_a);
        printf("CALCIF2_DEBUG %s:%d %15.4f %15.4f %15.4f\n", __FILE__, __LINE__, request->a_x, request->a_y, request->a_z);
        printf("CALCIF2_DEBUG %s:%d %15.6f %15.6f %15.6f\n", __FILE__, __LINE__, request->a_dx, request->a_dy, request->a_dz);
        printf("CALCIF2_DEBUG %s:%d %15.6f %15.6f %15.6f\n", __FILE__, __LINE__, request->a_ddx, request->a_ddy, request->a_ddz);
        printf("CALCIF2_DEBUG %s:%d %15.6E %15.6E\n", __FILE__, __LINE__, request->axis_off_a, request->pressure_a);
        printf("CALCIF2_DEBUG %s:%d Antenna B %s %s\n", __FILE__, __LINE__, request->station_b, request->axis_type_b);
        printf("CALCIF2_DEBUG %s:%d %15.4f %15.4f %15.4f\n", __FILE__, __LINE__, request->b_x, request->b_y, request->b_z);
        printf("CALCIF2_DEBUG %s:%d %15.6f %15.6f %15.6f\n", __FILE__, __LINE__, request->b_dx, request->b_dy, request->b_dz);
        printf("CALCIF2_DEBUG %s:%d %15.6f %15.6f %15.6f\n", __FILE__, __LINE__, request->b_ddx, request->b_ddy, request->b_ddz);
        printf("CALCIF2_DEBUG %s:%d %15.6E %15.6E\n", __FILE__, __LINE__, request->axis_off_b, request->pressure_b);
        printf("CALCIF2_DEBUG %s:%d Source %s\n", __FILE__, __LINE__, request->source);
        printf("CALCIF2_DEBUG %s:%d RA Dec %15.6E %15.6E\n", __FILE__, __LINE__, request->ra, request->dec);
        printf("CALCIF2_DEBUG %s:%d Pmotio %15.6E %15.6E\n", __FILE__, __LINE__, request->dra, request->dec);
        printf("CALCIF2_DEBUG %s:%d epoch  %15.6E %15.6E\n", __FILE__, __LINE__, request->depoch, request->parallax);
        printf("CALCIF2_DEBUG %s:%d spos %15.6f %15.6f %15.6f\n", __FILE__, __LINE__, request->source_pos[0], request->source_pos[1], request->source_pos[2]);
        printf("CALCIF2_DEBUG %s:%d svel %15.6f %15.6f %15.6f\n", __FILE__, __LINE__, request->source_vel[0], request->source_vel[1], request->source_vel[2]);
        printf("CALCIF2_DEBUG %s:%d sepoch %15.6E %15.6E\n", __FILE__, __LINE__, request->source_epoch, request->source_parallax);
#endif /* CALCIF2_DEBUG */

        /* Default to letting CALC handle the (u,v,w)s */
        results->nRes = 1;
        results->delta = 0;
	results->aberCorr = p->aberCorr;
	if(p->delta > 0.0)
	{
            /* As long as we are far enough away from a pole, allow */
            /* variation of the source direction to determine (u,v,w)s */
            if(fabs(request->dec) < 1.5707963267948808) {
                /* source is more than 1E-14 rad away from pole */
                double delta = p->delta;
                double dd = delta/cos(request->dec);
                if(dd < 0.01) {
                    /* use delta as it is */
                    results->nRes = 3;
                    if(fabs(request->dec) < 1.5707963267948808 - delta) {
                        results->delta = delta;
                    }
                    else {
                        results->delta = -delta;
                    }
                }
                else if(dd < 0.1) {
                    /* getting close to pole, now within 3 arcmin, fudge lower */
                    delta *= 0.1;
                    results->nRes = 3;
                    if(fabs(request->dec) < 1.5707963267948808 - delta) {
                        results->delta = delta;
                    }
                    else {
                        results->delta = -delta;
                    }
                }
            }
	}

	for(i = 0; i < results->nRes; ++i)
	{
		memset(results->res+i, 0, sizeof(struct getCALC_res));
	}
	clnt_stat = clnt_call(p->clnt, GETCALC,
		(xdrproc_t)xdr_getCALC_arg, 
		(caddr_t)request,
		(xdrproc_t)xdr_getCALC_res, 
		(caddr_t)(results->res),
		TIMEOUT);
	if(clnt_stat != RPC_SUCCESS)
	{
		fprintf(stderr, "Error: callCalc: clnt_call failed!\n");

		return -1;
	}
	if(results->res[0].error)
	{
		fprintf(stderr, "Error: callCalc: %s\n", results->res[0].getCALC_res_u.errmsg);

		return -2;
	}

	if(results->nRes == 3)
	{
		ra  = request->ra;
		dec = request->dec;

		/* calculate delay offset in RA */
		request->ra  = ra - results->delta/cos(dec);
		request->dec = dec;
		clnt_stat = clnt_call(p->clnt, GETCALC,
			(xdrproc_t)xdr_getCALC_arg, 
			(caddr_t)request,
			(xdrproc_t)xdr_getCALC_res, 
			(caddr_t)(results->res + 1),
			TIMEOUT);
		if(clnt_stat != RPC_SUCCESS)
		{
			fprintf(stderr, "Error: callCalc (2): clnt_call failed!\n");

			return -1;
		}
		if(results->res[1].error)
		{
			fprintf(stderr,"Error: callCalc: %s\n", results->res[1].getCALC_res_u.errmsg);

			return -2;
		}

		/* calculate delay offset in Dec */
		request->ra  = ra;
		request->dec = dec + results->delta;
		clnt_stat = clnt_call(p->clnt, GETCALC,
			(xdrproc_t)xdr_getCALC_arg, 
			(caddr_t)request,
			(xdrproc_t)xdr_getCALC_res, 
			(caddr_t)(results->res + 2),
			TIMEOUT);
		if(clnt_stat != RPC_SUCCESS)
		{
			fprintf(stderr, "Error: callCalc (3): clnt_call failed!\n");

			return -1;
		}
		if(results->res[2].error)
		{
			fprintf(stderr,"Error: callCalc: %s\n", results->res[2].getCALC_res_u.errmsg);

			return -2;
		}
		
		request->ra  = ra;
		request->dec = dec;
	}
	
	return 0;
}

static int unwindAzimuth(double *az, int order)
{
	int i;
	double azmax, azmin;

	azmax = azmin = az[0];

	for(i = 1; i < order; ++i)
	{
		if(az[i] > azmax)
		{
			azmax = az[i];
		}
		if(az[i] < azmin)
		{
			azmin = az[i];
		}
	}

	if(fabs(azmax-azmin) > 180.0)
	{
		for(i = 0; i < order; ++i)
		{
			if(az[i] < 180.0)
			{
				az[i] += 360.0;
			}
		}
	}

	return 0;
}

static int extractCalcResults(struct modelTemp *mod, int index, struct CalcResults *results)
{
	struct getCALC_res *res0, *res1, *res2;
	double d, dx, dy;
	int rv=0;

	res0 = &results->res[0];
	res1 = &results->res[1];
	res2 = &results->res[2];

#ifdef CALCIF2_DEBUG        
        printf("CALCIF2_DEBUG %s:%d have calc results %E %E %E\n", __FILE__, __LINE__, -res0->getCALC_res_u.record.delay[0]*1e6,res0->getCALC_res_u.record.dry_atmos[0]*1e6,               res0->getCALC_res_u.record.wet_atmos[0]*1e6);
        printf("CALCIF2_DEBUG %s:%d have calc UVW %E %E %E\n", __FILE__, __LINE__, res0->getCALC_res_u.record.UV[0],res0->getCALC_res_u.record.UV[1],res0->getCALC_res_u.record.UV[2]);
#endif /* CALCIF2_DEBUG */
	
	mod->delay[index] = -res0->getCALC_res_u.record.delay[0]*1.0e6;
	mod->dry[index] = res0->getCALC_res_u.record.dry_atmos[0]*1.0e6;
	mod->wet[index] = res0->getCALC_res_u.record.wet_atmos[0]*1.0e6;
	mod->az[index] = res0->getCALC_res_u.record.az[1]*180.0/M_PI;
	mod->elgeom[index] = res0->getCALC_res_u.record.el[1]*180.0/M_PI;
	mod->msa[index] = res0->getCALC_res_u.record.msa[1];

/* FIXME: add elcorr, elgeom and parangle */
	mod->elcorr[index] = 0.0;
	mod->parangle[index] = 0.0;

	if(results->nRes == 3)
	{
		/* compute u, v, w by taking angular derivative of geometric delay */
		if(results->aberCorr == AberCorrExact)
		{
			d =  res0->getCALC_res_u.record.delay[0] -
			     res0->getCALC_res_u.record.wet_atmos[0] -
			     res0->getCALC_res_u.record.dry_atmos[0];
			dx = res1->getCALC_res_u.record.delay[0] -
			     res1->getCALC_res_u.record.wet_atmos[0] -
			     res1->getCALC_res_u.record.dry_atmos[0];
			dy = res2->getCALC_res_u.record.delay[0] -
			     res2->getCALC_res_u.record.wet_atmos[0] -
			     res2->getCALC_res_u.record.dry_atmos[0];
		}
		else if(results->aberCorr == AberCorrNoAtmos)
		{
			d =  res0->getCALC_res_u.record.delay[0];
			dx = res1->getCALC_res_u.record.delay[0];
			dy = res2->getCALC_res_u.record.delay[0];
		}
		else
		{
			fprintf(stderr, "Developer error: nRes is 3 but aberCorr is not AberCorrExact or AberCorrNoAtmos!\n");

			d = dx = dy = 0.0;

			rv = 1;
		}

		mod->u[index] = (C_LIGHT/results->delta)*(d-dx);
		mod->v[index] = (C_LIGHT/results->delta)*(dy-d);
		mod->w[index] = C_LIGHT*d;
	
		if(isnan(d) || isinf(d) || isnan(dx) || isinf(dx) || isnan(dy) || isinf(dy))
		{
			rv = 1;
		}
	}
	else
	{
		mod->u[index] = res0->getCALC_res_u.record.UV[0];
		mod->v[index] = res0->getCALC_res_u.record.UV[1];
		mod->w[index] = res0->getCALC_res_u.record.UV[2];
		
		if(isnan(mod->delay[index]) || isinf(mod->delay[index]))
		{
			rv = 1;
		}
	}
#ifdef CALCIF2_DEBUG        
        printf("CALCIF2_DEBUG %s:%d have delay %E \mu s\n", __FILE__, __LINE__, im->delay[index]);
        printf("CALCIF2_DEBUG %s:%d have UVW %E %E %E [m]\n", __FILE__, __LINE__, im->u[index], im->v[index], im->w[index]);
#endif /* CALCIF2_DEBUG */

	return rv;
}

static void unwrap_array_for_poly(const int N, double* array)
{
    /* take care of 2\pi ambiguity so that angles in radians can
       appear to be a continuous function to the polynomial fitting
       routine.  When calculating the angle from the polynomial terms
       later on, make sure to take the angle modulo 2\pi.
    */
    double last;
    double offset;
    int i;
    if(N<=0) {
        return;
    }
    last = array[0];
    offset = 0.0;
    for(i=1; i < N; ++i) {
        if(array[i] > last + M_PI) {
            offset -= 2.0*M_PI;
        }
        else if(array[i] < last - M_PI) {
            offset += 2.0*M_PI;
        }
        last = array[i];
        array[i] += offset;
    }
    return;
}
    

static void unwrap_deg_array_for_poly(const int N, double* array)
{
    /* take care of 360 degree ambiguity so that angles in degrees can
       appear to be a continuous function to the polynomial fitting
       routine.  When calculating the angle from the polynomial terms
       later on, make sure to take the angle modulo 360.
    */
    double last;
    double offset;
    int i;
    if(N<=0) {
        return;
    }
    last = array[0];
    offset = 0.0;
    for(i=1; i < N; ++i) {
        if(array[i] > last + 180.0) {
            offset -= 360.0;
        }
        else if(array[i] < last - 180.0) {
            offset += 360.0;
        }
        last = array[i];
        array[i] += offset;
    }
    return;
}
    

static double computePolyModel(DifxPolyModel *im, struct modelTemp *mod, double deltaT, int oversamp, int interpolationType)
{
	double r;

	/* FIXME: add interpolation mode */
	r = computePoly2(im->delay,      mod->delay,		im->order+1, oversamp, deltaT, interpolationType);
	computePoly2(im->dry,            mod->dry,		im->order+1, oversamp, deltaT, interpolationType);
	computePoly2(im->wet,            mod->wet,		im->order+1, oversamp, deltaT, interpolationType);
        unwrap_deg_array_for_poly((im->order+1)*oversamp, mod->az);
	computePoly2(im->az,             mod->az,		im->order+1, oversamp, deltaT, interpolationType);
	computePoly2(im->elcorr,         mod->elcorr,		im->order+1, oversamp, deltaT, interpolationType);
	computePoly2(im->elgeom,         mod->elgeom,		im->order+1, oversamp, deltaT, interpolationType);
        unwrap_deg_array_for_poly((im->order+1)*oversamp, mod->parangle);
	computePoly2(im->parangle,       mod->parangle,	im->order+1, oversamp, deltaT, interpolationType);
        unwrap_array_for_poly((im->order+1)*oversamp, 2014, mod->msa);
        computePoly2(im->msa,            mod->msa,		im->order+1, oversamp, deltaT, interpolationType);
        computePoly2(im->sc_gs_delay,    mod->sc_gs_delay,	im->order+1, oversamp, deltaT, interpolationType);
        computePoly2(im->gs_clock_delay, mod->gs_clock_delay,	im->order+1, oversamp, deltaT, interpolationType);
	computePoly2(im->u,              mod->u,		im->order+1, oversamp, deltaT, interpolationType);
	computePoly2(im->v,              mod->v,		im->order+1, oversamp, deltaT, interpolationType);
	computePoly2(im->w,              mod->w,		im->order+1, oversamp, deltaT, interpolationType);

	return r;	// The RMS interpolation error
}


static int adjustSpacecraftAntennaCalcResults(struct modelTemp *mod, int index, const DifxInput *D, const struct getCALC_arg *request, int antId, int spacecraftId, const CalcParams *calcparms_orig)
{
    DifxAntenna* antenna;
    DifxSpacecraft *sc;
    int i;
    int mjd_offset;
    int original_date;
    double original_day_fraction;
    int retarded_date;
    double retarded_day_fraction;
    int need_new_gs_recording_delay = 0;
    int store_new_gs_recording_delay = 0;
    double sc_gs_delay = 0.0;          /* Spacecraft to ground station delay,
                                          in seconds */
    double gs_clock_offset = 0.0;      /* in seconds */
    double sc_time_frame_delay = 0.0;  /* in seconds */

    antenna = D->antenna + antId;
    sc = D->spacecraft + spacecraftId;

    if((!sc->is_antenna))
    {
        return -1;
    }

    if(sc->spacecraft_time_type == SpacecraftTimeLocal)
    {
        need_new_gs_recording_delay = 0;
        store_new_gs_recording_delay = 0;
        sc_gs_delay = sc->GS_recording_delay;
        gs_clock_offset = 0.0;
        original_date = request->date;
        original_day_fraction = request->time;
    }
    else if(sc->spacecraft_time_type == SpacecraftTimeGroundReception)
    {
        if(sc->GS_recording_delay >= -1.0E100)
        {
            /* constant delay offset already calculated, so use it */
#ifdef CALCIF2_DEBUG        
            fprintf(stderr, "CALCIF2_DEBUG: using exisiting GS_recording_delay=%E, SC_recording_delay=%E, with target delay=%E\n", sc->GS_recording_delay, sc->SC_recording_delay, im->delay[index]);
#endif /* CALCIF2_DEBUG */
            need_new_gs_recording_delay = 0;
            store_new_gs_recording_delay = 0;
            sc_gs_delay = sc->GS_recording_delay;
            gs_clock_offset = sc->GS_clock_delay;
        }
        else {
            need_new_gs_recording_delay = 1;
            store_new_gs_recording_delay = 1;
        }
        original_date = sc->GS_mjd_sync;
        original_day_fraction = sc->GS_dayfraction_sync;
        /* processing handled below */
    }
    else if(sc->spacecraft_time_type == SpacecraftTimeGroundClock)
    {
        /* How do I deal with the spacecraft clock running off of the ground
           maser, correcting for all special and general relativistic effects
           for clock transfer? */
        fprintf(stderr, "Error: delay adjustment for spacecraft_time_type == SpacecraftTimeGroundClock is not yet implemented.  Contact your DiFX developer\n");
        return -3;
        need_new_gs_recording_delay = 1;
        store_new_gs_recording_delay = 0;
        original_date = request->date;
        original_day_fraction = request->time;
        /* processing handled below */
    }
    else
    {
        /* unknown time type */
        return -2;
    }

    if((need_new_gs_recording_delay)) {
        /* Get the ground station clock offset */
        double x = 1.0;
        double Delta_t = (original_date + original_day_fraction) - sc->GS_clockrefmjd;
        Delta_t *= 86400.0;
        for(i=0; i <= sc->GS_clockorder; i++) {
            gs_clock_offset += x*sc->GS_clockcoeff[i];
            x *= Delta_t;
        }
        gs_clock_offset *= 1.0E-6; /* convert from \mu s to s */
        gs_clock_offset += sc->GS_clock_break_fudge_sec;
        /* correct the indicated station time to the true TT time */
        original_day_fraction += gs_clock_offset / 86400.0;
        mjd_offset = (int)(floor(original_day_fraction));
        original_date += mjd_offset;
        original_day_fraction -= mjd_offset;

        if((store_new_gs_recording_delay)) {
            sc->GS_clock_delay = gs_clock_offset;
        }
    }

    if((need_new_gs_recording_delay)) {
        CalcParams p;
        nineVector pos;
        nineVector dir;
        int r;
        struct getCALC_arg sc_request;
        struct getCALC_arg gs_request;
        struct CalcResults sc_results;
        struct CalcResults gs_results;
        char sc_mount[MAX_ANTENNA_MOUNT_NAME_LENGTH];
        char gs_mount[MAX_ANTENNA_MOUNT_NAME_LENGTH];
        double pos_magnitude;
        double gs_delay = 0.0;             /* ground station to center of Earth
                                              delay in seconds */
        double last_sc_gs_delay = -2E100;  /* in seconds */
        int loop_count;
        char s_name[] = "sc";
        static const int MAX_LOOP_COUNT = 10;
        
#ifdef CALCIF2_DEBUG        
        fprintf(stderr, "CALCIF2_DEBUG: Determining GS_recording_delay for spacecraft '%s' (%d) at %d %.6f\n", sc->name, spacecraftId, request->date, request->time);
#endif /* CALCIF2_DEBUG */
        
        p = *calcparms_orig;
        p.delta = 0.0;
    
        /* Set up the CALC control codes. */
        /* This should be copied from calcinit.f as much as possible, but turn
           on topocentric corrections. */
        sc_request = *request;
/*     for(i = 0; i < 64; i++) */
/*     { */
/*         sc_request.kflags[i] = -1; */
/*     } */
/*     sc_request.kflags[ 0] = 1; /\* KATMC *\/ */
/*     sc_request.kflags[52] = 2; /\* KOCEC *\/ */
/*     sc_request.kflags[60] = 1; /\* KPANC *\/ */
/*     sc_request.kflags[23] = 1; /\* KUT1D *\/ */
/*     sc_request.kflags[62] = 1; /\* KTOPC *\/ */
/*     sc_request.kflags[63] = 1; /\* KTOPD *\/ */
        gs_request = sc_request;
    

    
        /* set up the spacecraft request */
        sc_request.date = original_date;
        sc_request.time = original_day_fraction;
        sc_request.station_b = antenna->calcname;
        sc_request.ra  =  0.0;
        sc_request.dec =  0.0;
        sc_request.dra  = 0.0;
        sc_request.ddec = 0.0;
        sc_request.parallax = 0.0;
        sc_request.depoch = 0.0;
        sc_request.pressure_b = 0.0;
        sc_request.source = s_name;
        /* this is needed to get around xdr_string not coping well with const strings */
        strncpy(sc_mount, antennaMountTypeNames[antenna->mount], MAX_ANTENNA_MOUNT_NAME_LENGTH-1);
        sc_mount[MAX_ANTENNA_MOUNT_NAME_LENGTH-1] = 0;
        sc_request.axis_type_b = sc_mount;
        sc_request.axis_off_b = antenna->offset[0];
        /* Now get the spacecraft position at the true ground reception time time */
        r = evaluateDifxSpacecraftAntenna(sc, original_date, original_day_fraction, &pos);
        if(r < 0)
        {
            printf("Error: %s:%d:adjustSpacecraftAntennaCalcResults: evaluateDifxSpacecraftAntenna = %d\n", __FILE__, __LINE__, r);
            return -4;
        }
        sc_request.b_x = pos.X;
        sc_request.b_y = pos.Y;
        sc_request.b_z = pos.Z;
        sc_request.b_dx = pos.dX;
        sc_request.b_dy = pos.dY;
        sc_request.b_dz = pos.dZ;
        sc_request.b_ddx = pos.ddX;
        sc_request.b_ddy = pos.ddY;
        sc_request.b_ddz = pos.ddZ;



        /* set up the ground station request */
        gs_request.date = original_date;
        gs_request.time = original_day_fraction;
        gs_request.station_b = sc->GS_calcName;
        gs_request.b_x = sc->GS_X;
        gs_request.b_y = sc->GS_Y;
        gs_request.b_z = sc->GS_Z;
        gs_request.b_dx = 0.0; /* for ground-based antennas, ground drift already*/
        gs_request.b_dy = 0.0; /* taken into account, so set the station */
        gs_request.b_dz = 0.0; /* velocities to 0 */
        gs_request.b_ddx = 0.0; /* for ground-based antennas, acceleration */
        gs_request.b_ddy = 0.0; /* calculated from Earth rotation, so set the */
        gs_request.b_ddz = 0.0; /* accelerations to 0 */
        gs_request.ra  =  0.0;
        gs_request.dec =  0.0;
        gs_request.dra  = 0.0;
        gs_request.ddec = 0.0;
        gs_request.parallax = 0.0;
        gs_request.depoch = 0.0;
        gs_request.pressure_b = 0.0;
        gs_request.source = s_name;
        /* this is needed to get around xdr_string not coping well with const strings */
        strncpy(gs_mount, antennaMountTypeNames[sc->GS_mount], MAX_ANTENNA_MOUNT_NAME_LENGTH-1);
        gs_mount[MAX_ANTENNA_MOUNT_NAME_LENGTH-1] = 0;
        gs_request.axis_type_b = gs_mount;
        gs_request.axis_off_b = sc->GS_offset[0];

        /* zero out the J2000 position of the ground station in the result area
           as a first order approximation */
        gs_results.res[0].getCALC_res_u.record.baselineP2000[0] = 0.0;
        gs_results.res[0].getCALC_res_u.record.baselineP2000[1] = 0.0;
        gs_results.res[0].getCALC_res_u.record.baselineP2000[2] = 0.0;
        gs_results.res[0].getCALC_res_u.record.baselineV2000[0] = 0.0;
        gs_results.res[0].getCALC_res_u.record.baselineV2000[1] = 0.0;
        gs_results.res[0].getCALC_res_u.record.baselineV2000[2] = 0.0;
        gs_results.res[0].getCALC_res_u.record.baselineA2000[0] = 0.0;
        gs_results.res[0].getCALC_res_u.record.baselineA2000[1] = 0.0;
        gs_results.res[0].getCALC_res_u.record.baselineA2000[2] = 0.0;

        for(loop_count = 0; loop_count < MAX_LOOP_COUNT; loop_count++)
        {
            /* get the spacecraft position at the instant of
               transmission, at the retarded time corresponding to the
               true recoding time at the ground station.
            */
            retarded_date = original_date;
            retarded_day_fraction = original_day_fraction - sc_gs_delay/86400.0;
            mjd_offset = (int)(floor(retarded_day_fraction));
            retarded_date += mjd_offset;
            retarded_day_fraction -= mjd_offset;
            r = evaluateDifxSpacecraftAntenna(sc, retarded_date, retarded_day_fraction, &pos);
            if(r < 0)
            {
                printf("Error: %s:%d:adjustSpacecraftAntennaCalcResults: evaluateDifxSpacecraftAntenna = %d\n", __FILE__, __LINE__, r);
                return -5;
            }
            /* Call CALC with the ground station as station b, with the
               spacecraft direction as the target source.  The 3-D source
               vector is used rather than RA,Dec to indicate that no
               annual abberation should be applied.  Note that the source
               position vector is multiplied to be vary far away, so that
               the direction is the same for the geocenter and the station
               topocentric direction.
            */
#ifdef CALCIF2_DEBUG        
            fprintf(stderr, "CALCIF2_DEBUG: Have Spacecraft    Ground  stations at\n");
            fprintf(stderr, "CALCIF2_DEBUG: X  %15.3f %15.3f\n", pos.X, -gs_results.res[0].getCALC_res_u.record.baselineP2000[0]);
            fprintf(stderr, "CALCIF2_DEBUG: Y  %15.3f %15.3f\n", pos.Y, -gs_results.res[0].getCALC_res_u.record.baselineP2000[1]);
            fprintf(stderr, "CALCIF2_DEBUG: Z  %15.3f %15.3f\n", pos.Z, -gs_results.res[0].getCALC_res_u.record.baselineP2000[2]);
            fprintf(stderr, "CALCIF2_DEBUG: Have Ground  stations velocities\n");
            fprintf(stderr, "CALCIF2_DEBUG: X %15.3f\n", -gs_results.res[0].getCALC_res_u.record.baselineV2000[0]);
            fprintf(stderr, "CALCIF2_DEBUG: Y %15.3f\n", -gs_results.res[0].getCALC_res_u.record.baselineV2000[1]);
            fprintf(stderr, "CALCIF2_DEBUG: Z %15.3f\n", -gs_results.res[0].getCALC_res_u.record.baselineV2000[2]);
#endif /* CALCIF2_DEBUG */
            dir.X = pos.X + gs_results.res[0].getCALC_res_u.record.baselineP2000[0];
            dir.Y = pos.Y + gs_results.res[0].getCALC_res_u.record.baselineP2000[1];
            dir.Z = pos.Z + gs_results.res[0].getCALC_res_u.record.baselineP2000[2];
            {
                double R = sqrt(dir.X*dir.X + dir.Y*dir.Y + dir.Z*dir.Z);
#ifdef CALCIF2_DEBUG        
                fprintf(stderr, "CALCIF2_DEBUG: spacecraft--ground station distance is %.3f m, %E s\n", R, R/C_LIGHT);
#endif /* CALCIF2_DEBUG */
                if(R > 0.0) {
                    static const double parsec = 3.085678E16; /* m */
                    const double multiplier = 1E16 * parsec / R;
                    dir.X *= multiplier;
                    dir.Y *= multiplier;
                    dir.Z *= multiplier;
                }
                else {
                    fprintf(stderr, "Error: the spacecraft %d ('%s') crashed into ground station ('%s') at MJD %d fracDay %f, and the observation came to an abrupt end.  Please contact your software developer so that this bug is fixed before launching future space missions.\n", spacecraftId, sc->name, sc->GS_Name, retarded_date, retarded_day_fraction);
                    exit(1);
                }
                dir.dX = 0.0;
                dir.dY = 0.0;
                dir.dZ = 0.0;
            }
            
            gs_request.source_pos[0] = dir.X;
            gs_request.source_pos[1] = dir.Y;
            gs_request.source_pos[2] = dir.Z;
            gs_request.source_vel[0] = dir.dX;
            gs_request.source_vel[1] = dir.dY;
            gs_request.source_vel[2] = dir.dZ;
            gs_request.source_epoch  = original_date + original_day_fraction;
            gs_request.source_parallax = 0.0;
            /* assume ground station is actually pointing at the spacecraft */
            gs_request.pointing_pos_b_z[0] = gs_request.source_pos[0];
            gs_request.pointing_pos_b_z[1] = gs_request.source_pos[1];
            gs_request.pointing_pos_b_z[2] = gs_request.source_pos[2];
            gs_request.pointing_vel_b_z[0] = gs_request.source_vel[0];
            gs_request.pointing_vel_b_z[1] = gs_request.source_vel[1];
            gs_request.pointing_vel_b_z[2] = gs_request.source_vel[2];
            gs_request.pointing_epoch_b  = gs_request.source_epoch;
            gs_request.pointing_parallax = gs_request.source_parallax;

            r = callCalc(&gs_request, &gs_results, &p);
            if(r < 0)
            {
                printf("Error: %s:%d:adjustSpacecraftAntennaCalcResults: callCalc = %d\n", __FILE__, __LINE__, r);
		
                return -5;
            }
            /* Now call CALC with the direction of the spacecraft as seen
               by the ground station as the target source direction.  CALC
               is called with the retarded time.
            */
            for(i=0; i < 3; i++) {
                sc_request.source_pos[i] = gs_request.source_pos[i];
                sc_request.source_vel[i] = gs_request.source_vel[i];
                sc_request.pointing_pos_b_z[i] = gs_request.pointing_pos_b_z[i];
                sc_request.pointing_vel_b_z[i] = gs_request.pointing_vel_b_z[i];
            }
            sc_request.source_epoch = retarded_date + retarded_day_fraction;
            sc_request.source_parallax = 0.0;
            sc_request.pointing_epoch_b = sc_request.source_epoch;
            sc_request.pointing_parallax = sc_request.source_parallax;

            /* the spacecraft velocity and acceleration is fudged in order
               that the abberation is the same as seen by the
               ground station
            */
            sc_request.b_dx = -gs_results.res[0].getCALC_res_u.record.baselineV2000[0];
            sc_request.b_dy = -gs_results.res[0].getCALC_res_u.record.baselineV2000[1];
            sc_request.b_dz = -gs_results.res[0].getCALC_res_u.record.baselineV2000[2];
            sc_request.b_ddx = -gs_results.res[0].getCALC_res_u.record.baselineA2000[0];
            sc_request.b_ddy = -gs_results.res[0].getCALC_res_u.record.baselineA2000[1];
            sc_request.b_ddz = -gs_results.res[0].getCALC_res_u.record.baselineA2000[2];
            
            r = callCalc(&sc_request, &sc_results, &p);
            if(r < 0)
            {
                printf("Error: %s:%d:adjustSpacecraftAntennaCalcResults: callCalc = %d\n", __FILE__, __LINE__, r);
		
                return -6;
            }

            /* The spacecraft to ground station delay is now calculated */
#ifdef CALCIF2_DEBUG        
            fprintf(stderr, "CALCIF2_DEBUG: for loop %d using start retarded delay=%E, found spacecraft delay=%E, ground station delay=%E\n", loop_count, last_sc_gs_delay, -sc_results.res[0].getCALC_res_u.record.delay[0], -gs_results.res[0].getCALC_res_u.record.delay[0]);
#endif /* CALCIF2_DEBUG */
            sc_gs_delay = -sc_results.res[0].getCALC_res_u.record.delay[0] - -gs_results.res[0].getCALC_res_u.record.delay[0];
            gs_delay = -gs_results.res[0].getCALC_res_u.record.delay[0];
#ifdef CALCIF2_DEBUG
            fprintf(stderr, "CALCIF2_DEBUG: for loop %d sc_gs_delay=%E last_sc_gs_delay=%E\n", loop_count, sc_gs_delay, last_sc_gs_delay);
            fprintf(stderr, "CALCIF2_DEBUG: for loop %d change from last iteration=%E, relative change=%E\n", loop_count, sc_gs_delay - last_sc_gs_delay, (sc_gs_delay - last_sc_gs_delay)/last_sc_gs_delay);
#endif /* CALCIF2_DEBUG */
            if(fabs(sc_gs_delay - last_sc_gs_delay) < 1E-13)
            {
                /* The delay value has chenged by less than 0.1 ps,
                   which is just 0.03 mm of path difference, or
                   33 times smaller than the wavelength at 300 GHz,
                   and so this should be good enough for radio interferometry.
                */
                break;
            }
            last_sc_gs_delay = sc_gs_delay;
        }
        if(loop_count == MAX_LOOP_COUNT)
        {
            fprintf(stderr, "Error: ground station delay not converging\n");
            return -7;
        }

        if((store_new_gs_recording_delay)) {
            sc->GS_recording_delay = sc_gs_delay;
        }
    }


    /* Adjust the delay to account for the difference between the ground time
       frame (TT) and the spacecraft (general) relativistic time frame.
       The evaluateDifxSpacecraftAntennaTimeFrameOffset function returns the
       differential time offset offset, such that when the center of Earth
       clock reads time t, the spacecraft clock reads time t - offset.  Adjust
       the model delay.  The model delay is defined such that if the wavefront
       from the source arrives at the center of Earth at time t, the wavefront
       should arrive at the telescope at time t - delay according to the
       station clock.  Thus, accounting for the time frame difference, the
       arrival at the spacecraft is seen at time t - delay - offset read
       on the spacecraft clock.  Correct the requested TT time for the
       delay offsets, to get the TT time at which the wavefront reaches the
       spacecraft.

       Note that if there is an error in the spacecraft
       ephemeris clock, that error must be corrected outside of DiFX.  That
       is, the ephemeris table itself should be fixed to give the
       correct timestamp corresponding to the spacecraft position.  Otherwise,
       yet another clock polynomial must be input in the v2d file,
       and the corresponding code propagated to here.
    */
    if((sc->spacecraft_time_type == SpacecraftTimeLocal)
      || (sc->spacecraft_time_type == SpacecraftTimeGroundReception))
    {
        spacecraftTimeFrameOffset offset;
        int retval;
        retarded_date = request->date;
        retarded_day_fraction = request->time - (im->delay[index]*1E-6)/86400.0;
        mjd_offset = (int)(floor(retarded_day_fraction));
        retarded_date += mjd_offset;
        retarded_day_fraction -= mjd_offset;
 
        retval = evaluateDifxSpacecraftAntennaTimeFrameOffset(sc,
                                                              retarded_date,
                                                              retarded_day_fraction,
                                                             &offset);
                 
#ifdef CALCIF2_DEBUG
        fprintf(stderr, "CALCIF2_DEBUG: at %d %f adjustSpacecraftAntennaCalcResults got evaluateDifxSpacecraftAntennaTimeFrameOffset result %d %E s, original delay is %E s\n", request->date, request->time, retval, offset.Delta_t, im->delay[index]*1E-6);
#endif /* CALCIF2_DEBUG */
        if(retval < 0)
        {
            return -8;
        }
        mod->delay[index] += (offset.Delta_t)*1E6;
    }
    else if(sc->spacecraft_time_type == SpacecraftTimeGroundClock)
    {
        fprintf(stderr, "Error: delay adjustment for spacecraft time frame for spacecraft_time_type == SpacecraftTimeGroundClock is not yet implemented.  Contact your DiFX developer\n");
        return -9;
    }
    else
    {
        /* unknown time type */
        return -2;
    }
    
#ifdef CALCIF2_DEBUG
    fprintf(stderr, "CALCIF2_DEBUG: Have GS_recording_delay=%E s, SC_recording_delay=%E s, gs_clock_offset=%E s,  with geometric target delay=%E s\n", sc_gs_delay, sc->SC_recording_delay, gs_clock_offset, im->delay[index]*1E-6);
#endif /* CALCIF2_DEBUG */
    /* correct the delay to account for the sc_gs_delay and the
       SC_recording_delay.  The *spacecraft* clock delay will be added in later
       in the actual correlator section, so do not add it in here, but *do*
       add in the ground station clock offset.
    */
    mod->delay[index] -= (sc_gs_delay - gs_clock_offset + sc->SC_recording_delay)*1E6;
    mod->sc_gs_delay[index] = sc_gs_delay*1E+6;
    mod->gs_clock_delay[index] = -gs_clock_offset*1E+6;

    return 0;
}

/* antenna here is a pointer to a particular antenna object */
static int antennaCalc(int scanId, int antId, const DifxInput *D, const char *prefix, CalcParams *p, int phasecentre, int verbose)
{
	struct getCALC_arg *request;
	struct CalcResults results;
	struct modelTemp mod;
	int i, j, v;
	double sec, subInc;
	double lastsec = -1000;
	DifxPolyModel **im;
	DifxScan *scan;
	DifxSource *source;
	DifxSource* pointing_center;
	DifxAntenna *antenna;
	int nInt;
	int spacecraftId = -1;
	int antennaspacecraftId = -1;
	int pointingspacecraftId = -1;
	int sourceId;
	int nError = 0;
        int spacecraft_delay_warning = 0; 
	char mount[MAX_ANTENNA_MOUNT_NAME_LENGTH];
	char externalDelayFilename[DIFXIO_FILENAME_LENGTH];
	ExternalDelay *ed;

	antenna = D->antenna + antId;
        antennaspacecraftId = antenna->spacecraftId;
	scan = D->scan + scanId;
	im = scan->im[antId];
	nInt = scan->nPoly;
        if(phasecentre == 0) // this is the pointing centre
	{
		sourceId = scan->pointingCentreSrc;
	}
	else
	{
		sourceId = scan->phsCentreSrcs[phasecentre-1];
	}
	source = D->source + sourceId;
	pointing_center = D->source + scan->pointingCentreSrc;
	subInc = p->increment/(double)(D->job->polyOrder*p->oversamp);
	request = &(p->request);
	spacecraftId = source->spacecraftId;
	pointingspacecraftId = pointing_center->spacecraftId;

	/* this is needed to get around xdr_string not coping well with const strings */
	strncpy(mount, antennaMountTypeNames[antenna->mount], MAX_ANTENNA_MOUNT_NAME_LENGTH-1);
	mount[MAX_ANTENNA_MOUNT_NAME_LENGTH-1] = 0;

	request->station_b = antenna->calcname;
	request->b_x = antenna->X;
	request->b_y = antenna->Y;
	request->b_z = antenna->Z;
	request->b_dx = 0.0; /* for ground-based antennas, ground drift already*/
	request->b_dy = 0.0; /* taken into account, so set the station */
	request->b_dz = 0.0; /* velocities to 0 */
	request->b_ddx = 0.0; /* for ground-based antennas, acceleration */
	request->b_ddy = 0.0; /* calculated from Earth rotation, so set the */
	request->b_ddz = 0.0; /* accelerations to 0 */
	request->axis_type_b = mount;
	request->axis_off_b = antenna->offset[0];

	request->source = source->name;
	if(spacecraftId < 0)
	{
	        request->ra       = source->ra;
	        request->dec      = source->dec;
	        request->dra      = source->pmRA;	
	        request->ddec     = source->pmDec;
	        request->parallax = source->parallax;
	        request->depoch   = source->pmEpoch;
	}
        else
        {
            request->ra       = 0.0;
            request->dec      = 0.0;
            request->dra      = 0.0;
            request->ddec     = 0.0;
            request->parallax = 0.0;
            request->depoch   = 0.0;
        }
        for(i=0; i < 3; ++i) {
            request->source_pos[i] = 0.0;
            request->source_vel[i] = 0.0;
            request->pointing_pos_b_x[i] = 0.0;
            request->pointing_vel_b_x[i] = 0.0;
            request->pointing_pos_b_y[i] = 0.0;
            request->pointing_vel_b_y[i] = 0.0;
            request->pointing_pos_b_z[i] = 0.0;
            request->pointing_vel_b_z[i] = 0.0;
        }
        request->source_epoch = 0.0;
        request->source_parallax = 0.0;
	if(pointingspacecraftId < 0)
	{
            double x, y, z;
            double muRA, muDec;
            x = cos(pointing_center->ra) * cos(pointing_center->dec);
            y = sin(pointing_center->ra) * cos(pointing_center->dec);
            z = sin(pointing_center->dec);
            request->pointing_pos_b_z[0] = x;
            request->pointing_pos_b_z[1] = y;
            request->pointing_pos_b_z[2] = z;
            /* convert proper motions to rad/s from arcsec/yr */
            muRA  = pointing_center->pmRA  * (M_PI/(180.0*3600.0))/ (365.25*86400.0);
            muDec = pointing_center->pmDec * (M_PI/(180.0*3600.0))/ (365.25*86400.0);
            x = -muRA*sin(pointing_center->ra)*cos(pointing_center->dec)
                - muDec*cos(pointing_center->ra)*sin(pointing_center->dec);
            y = +muRA*cos(pointing_center->ra)*cos(pointing_center->dec)
                - muDec*sin(pointing_center->ra)*sin(pointing_center->dec);
            z = muDec*cos(pointing_center->dec);
            request->pointing_vel_b_z[0] = x;
            request->pointing_vel_b_z[1] = y;
            request->pointing_vel_b_z[2] = z;
            request->pointing_parallax = pointing_center->parallax;
            request->pointing_epoch_b  = pointing_center->pmEpoch;
	}

	snprintf(externalDelayFilename, DIFXIO_FILENAME_LENGTH, "%s.%s.%s.delay", prefix, antenna->name, source->name);
	ed = newExternalDelay(externalDelayFilename);
	if(!ed)
	{
		snprintf(externalDelayFilename, DIFXIO_FILENAME_LENGTH, "%s_%s.delay", antenna->name, source->name);
		ed = newExternalDelay(externalDelayFilename);
		if(ed)
		{
			fprintf(stderr, "Warning: using %s to drive delays.  This filename designator is obsolete.\n", externalDelayFilename);
		}
	}

	for(i = 0; i < nInt; ++i)
	{
		double e;

		request->date = im[phasecentre][i].mjd;
		sec = im[phasecentre][i].sec;
		for(j = 0; j <= D->job->polyOrder*p->oversamp; ++j)
		{
			request->time = sec/86400.0;

			/* call calc if we didn't just for this time */
			if(fabs(lastsec - sec) > 1.0e-6)
			{
				if(spacecraftId >= 0)
				{
                                        v = calcSpacecraftSourcePosition(D, request, spacecraftId, sourceId);
					if(v < 0)
					{
						printf("Error: antennaCalc: Spacecraft %d table out of time range\n", spacecraftId);

						
                                                nError = -1;
                                                goto end_of_antennaCalc;
					}
				}
				if(pointingspacecraftId >= 0)
				{
                                        v = calcSpacecraftPhaseCenterPosition(D, request, pointingspacecraftId, scan->pointingCentreSrc);
					if(v < 0)
					{
						printf("Error: antennaCalc: calcSpacecraftPhaseCenterPosition %d table out of time range\n", pointingspacecraftId);

                                                nError = -1;
                                                goto end_of_antennaCalc;
					}
				}
				if(antennaspacecraftId >= 0)
				{
					v = calcSpacecraftAntennaPosition(D, request, antennaspacecraftId);
					if(v < 0)
					{
						printf("Error: antennaCalc: Antenna spacecraft %d table out of time range, or not yet supported\n", antennaspacecraftId);

                                                nError = -1;
                                                goto end_of_antennaCalc;
					}
				}
				v = callCalc(request, &results, p);
				if(v < 0)
				{
					printf("Error: antennaCalc: callCalc = %d\n", v);
                                        
                                        nError = -1;
                                        goto end_of_antennaCalc;
				}
			}

			/* use result to populate tabulated values */
			nError += extractCalcResults(&mod, j, &results);

                        if(antennaspacecraftId >= 0)
                        {
                            v = adjustSpacecraftAntennaCalcResults(&mod, j, D, request, antId, antennaspacecraftId, p);
                            if(v < 0)
                            {
                                printf("Error: antennaCalc: Antenna spacecraft %d problem %d calculating recording offset\n", antennaspacecraftId, v);
                                nError = -1;
                                goto end_of_antennaCalc;
                            }
                        }

			/* override delay and atmosphere values */
			if(ed)
			{
				int v;
				double exDelay, exDry, exWet;

				v = getExternalDelay(ed, request->date+request->time, &exDelay, &exDry, &exWet);
				if(v < 0)
				{
					fprintf(stderr, "Error: antennaCalc: request for external delay from stn %s source %s at time %14.8f failed with error code %d\n", antenna->name, source->name, request->date+request->time, v);

					exit(0);
				}

				mod.delay[j] = -(exDelay+exDry+exWet)*1.0e6;
				mod.dry[j] = exDry*1.0e6;
				mod.wet[j] = exWet*1.0e6;
			}

			lastsec = sec;
			sec += subInc;
			if(sec >= 86400.0)
			{
				sec -= 86400.0;
				request->date += 1;
			}
		}
		e = computePolyModel(&im[phasecentre][i], &mod, subInc, p->oversamp, p->interpol);
		if(verbose > 0 && p->oversamp > 1)
		{
			printf("Scan %d antId %d poly %d : max delay interpolation error = %e us\n", scanId, antId, i, e);
		}
	}


end_of_antennaCalc:
        if(ed)
	{
		deleteExternalDelay(ed);
                ed = 0;
	}

	if(nError > 0)
	{
		fprintf(stderr, "Error: antennaCalc: Antenna %s had %d invalid delays\n", D->antenna[antId].name, nError);
	}

	return nError;
}

static int scanCalc(int scanId, const DifxInput *D, const char *prefix, CalcParams *p, int isLast, int verbose)
{
	DifxPolyModel *im;
	int antId;
	int mjd, sec;
	int sec1, sec2;
	int jobStart;	/* seconds since last midnight */
	int int1, int2;	/* polynomial intervals */
	int nInt;
	int i, v, k;
	DifxJob *job;
	DifxScan *scan;

	job = D->job;
	scan = D->scan + scanId;

	scan->nAntenna = D->nAntenna;

	scan->im = (DifxPolyModel ***)calloc(scan->nAntenna, sizeof(DifxPolyModel **));

	mjd = (int)(job->mjdStart);
	jobStart = (int)(86400.0*(job->mjdStart - mjd) + 0.5);

	sec1 = jobStart + scan->startSeconds; 
	sec2 = sec1 + scan->durSeconds;
	int1 = sec1/D->job->polyInterval;
	int2 = (sec2 + D->job->polyInterval - 1)/D->job->polyInterval;
	nInt = int2 - int1;
	if(isLast || sec2 % D->job->polyInterval == 0)
	{
		++nInt;
	}
	scan->nPoly = nInt;

	for(antId = 0; antId < scan->nAntenna; ++antId)
	{
		scan->im[antId] = (DifxPolyModel **)calloc(scan->nPhaseCentres+1, sizeof(DifxPolyModel*));
		for(k = 0; k < scan->nPhaseCentres + 1; ++k)
		{
			scan->im[antId][k] = (DifxPolyModel *)calloc(nInt, sizeof(DifxPolyModel));
			im = scan->im[antId][k];
			sec = int1*D->job->polyInterval;
			mjd = (int)(job->mjdStart);
		
			for(i = 0; i < nInt; ++i)
			{
				if(sec >= 86400)
				{
					sec -= 86400;
					++mjd;
				}
	
				/* set up the intervals to calc polys over */
				im[i].mjd = mjd;
				im[i].sec = sec;
				im[i].order = D->job->polyOrder;
				im[i].validDuration = D->job->polyInterval;
				sec += D->job->polyInterval;
			}

			/* call calc to derive delay, etc... polys */
			v = antennaCalc(scanId, antId, D, prefix, p, k, verbose);
			if(v < 0)
			{
				return -1;
			}
		}
	}

	return 0;
}

int difxCalc(DifxInput *D, CalcParams *p, const char *prefix, int verbose)
{
	int scanId;
	int v;
	int isLast;
	DifxScan *scan;
	DifxJob *job;

	if(!D)
	{
		return -1;
	}

	for(scanId = 0; scanId < D->nScan; ++scanId)
	{
		scan = D->scan + scanId;
		job = D->job;

		job->aberCorr = p->aberCorr;
		if(scan->im)
		{
			fprintf(stderr, "Error: difxCalc: scan %d: model already exists\n", scanId);

			return -2;
		}
		if(scanId == D->nScan - 1)
		{
			isLast = 1;
		}
		else
		{
			isLast = 0;
		}
		v = scanCalc(scanId, D, prefix, p, isLast, verbose);
		if(v < 0)
		{
			return -1;
		}
	}

	return 0;
}
