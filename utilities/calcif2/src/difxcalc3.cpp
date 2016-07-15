/***************************************************************************
 *   Copyright (C) 2008-2015 by Walter Brisken, Adam Deller,               *
 *                              & James M Anderson                         *
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
// $Id: difxcalc3.c 6822 2015-07-07 09:09:32Z JamesAnderson $
// $HeadURL: $
// $LastChangedRevision: 6822 $
// $Author: JamesAnderson $
// $LastChangedDate: 2015-07-07 11:09:32 +0200 (Tue, 07 Jul 2015) $
//
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#ifndef __STDC_FORMAT_MACROS
#  define __STDC_FORMAT_MACROS // For non-compliant C++ compilers
#endif
#ifndef __STDC_CONSTANT_MACROS
#  define __STDC_CONSTANT_MACROS
#endif
#ifndef __STDC_LIMIT_MACROS
#  define __STDC_LIMIT_MACROS
#endif
#ifndef __STDC_FORMAT_MACROS
#  define __STDC_FORMAT_MACROS // For non-compliant C++ compilers
#endif
#include <inttypes.h>
#include <stdint.h>
#include "difxcalc3.h"
#include "externaldelay.h"
#include "poly.h"
#include "DelayHandlerDistributorInterface.h"
#include "DelayHandlerDistributor.h"
#include "DelayTimestamp.h"
#include "difxcalcrotation.h"  // needed for restrict
#include "MATHCNST.H"


namespace {
/* Note: This is a particular NaN variant the FITS-IDI format/convention 
 * wants, namely 0xFFFFFFFFFFFFFFFF */
static const union
{
	uint64_t u64;
	double d;
	float f;
} fitsnan = {UINT64_C(0xFFFFFFFFFFFFFFFF)};
}

using namespace DiFX::Delay::Handler;

#define MIN_NUM_EOPS 5

/* see DifxPolyModel in difxio:difx_input.h */
struct modelTemp
{
    double delay[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double dry[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double wet[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double iono[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double az[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double elcorr[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double elgeom[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double parangle[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double sc_gs_delay[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double gs_sc_delay[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double gs_clock_delay[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double msa[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double u[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double v[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double w[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
};
/* see DifxPolyModelLMNExtension in difxio:difx_input.h */
struct modelTempLMN
{
    double dDelay_dl[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double dDelay_dm[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double dDelay_dn[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double d2Delay_dldl[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double d2Delay_dldm[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double d2Delay_dldn[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double d2Delay_dmdm[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double d2Delay_dmdn[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double d2Delay_dndn[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
};
/* see DifxPolyModelXYZExtension in difxio:difx_input.h */
struct modelTempXYZ
{
    double dDelay_dX[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double dDelay_dY[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double dDelay_dZ[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double d2Delay_dXdX[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double d2Delay_dXdY[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double d2Delay_dXdZ[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double d2Delay_dYdY[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double d2Delay_dYdZ[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
    double d2Delay_dZdZ[MAX_MODEL_ORDER*MAX_MODEL_OVERSAMP+1];
};


static unsigned int select_lmn_derivative_indices(enum PerformDirectionDerivativeType perform_uvw, enum PerformDirectionDerivativeType perform_lmn, const int* restrict * restrict dp)
{
    /* derivative pointer (dp) has the following order, */
#define  SELECT_LMN_DERIVATIVE_INDICES_0          0  /*       */
#define  SELECT_LMN_DERIVATIVE_INDICES_Lp         1  /* l+    */
#define  SELECT_LMN_DERIVATIVE_INDICES_Lm         2  /* l-    */
#define  SELECT_LMN_DERIVATIVE_INDICES_Mp         3  /* m+    */
#define  SELECT_LMN_DERIVATIVE_INDICES_Mm         4  /* m-    */
#define  SELECT_LMN_DERIVATIVE_INDICES_Np         5  /* n+    */
#define  SELECT_LMN_DERIVATIVE_INDICES_Nm         6  /* n-    */
#define  SELECT_LMN_DERIVATIVE_INDICES_LpMp       7  /* l+m+  */
#define  SELECT_LMN_DERIVATIVE_INDICES_LpMm       8  /* l+m-  */
#define  SELECT_LMN_DERIVATIVE_INDICES_LmMp       9  /* l-m+  */
#define  SELECT_LMN_DERIVATIVE_INDICES_LmMm       10 /* l-m-  */
#define  SELECT_LMN_DERIVATIVE_INDICES_LpNp       11 /* l+n+  */
#define  SELECT_LMN_DERIVATIVE_INDICES_LpNm       12 /* l+n-  */
#define  SELECT_LMN_DERIVATIVE_INDICES_LmNp       13 /* l-n+  */
#define  SELECT_LMN_DERIVATIVE_INDICES_LmNm       14 /* l-n-  */
#define  SELECT_LMN_DERIVATIVE_INDICES_MpNp       15 /* m+n+  */
#define  SELECT_LMN_DERIVATIVE_INDICES_MpNm       16 /* m+n-  */
#define  SELECT_LMN_DERIVATIVE_INDICES_MmNp       17 /* m-n+  */
#define  SELECT_LMN_DERIVATIVE_INDICES_MmNm       18 /* m-n-  */
#define  SELECT_LMN_DERIVATIVE_INDICES_MAX        19
    /* The indices are relative to the non-offset source position. */
    static const int none[SELECT_LMN_DERIVATIVE_INDICES_MAX]     = {0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    static const unsigned int num_none = 1;
    static const int uv[SELECT_LMN_DERIVATIVE_INDICES_MAX]       = {0,+1,-1,+2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    static const unsigned int num_uv = 3;
    static const int uv2[SELECT_LMN_DERIVATIVE_INDICES_MAX]      = {0,+1,+2,+3,+4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    static const unsigned int num_uv2 = 5;
    static const int lmn[SELECT_LMN_DERIVATIVE_INDICES_MAX]      = {0,+1,-1,+2,-1,+3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    static const unsigned int num_lmn = 4;
    static const int lmn2[SELECT_LMN_DERIVATIVE_INDICES_MAX]     = {0,+1,+2,+3,+4,+5,+6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    static const unsigned int num_lmn2 = 7;
    static const int lmn_2[SELECT_LMN_DERIVATIVE_INDICES_MAX]    = {0,+1,+2,+3,+4,+5,+6,+7,-1,-1,+8,+9,-1,-1,10,11,-1,-1,12};
    static const unsigned int num_lmn_2 = 13;
    static const int lmn_22[SELECT_LMN_DERIVATIVE_INDICES_MAX]   = {0,+1,+2,+3,+4,+5,+6,+7,+8,+9,10,11,12,13,14,15,16,17,18};
    static const unsigned int num_lmn_22 = 19;
	if(  (perform_uvw == PerformDirectionDerivativeNone)
	  || (perform_uvw == PerformDirectionDerivativeFirstDerivative)
	  || (perform_uvw == PerformDirectionDerivativeFirstDerivative2)
		 )
	{
		/* Correct */
	}
	else
	{
		fprintf(stderr, "Error in select_lmn_derivative_indices: unsupported perform_uvw value %s\n", performDirectionDerivativeTypeNames[perform_uvw]);
		exit(EXIT_FAILURE);
	}
    if(perform_lmn == PerformDirectionDerivativeNone)
    {
        if(perform_uvw == PerformDirectionDerivativeNone)
        {
            *dp = none;
            return num_none;
        }
        else if(perform_uvw == PerformDirectionDerivativeFirstDerivative)
        {
            *dp = uv;
            return num_uv;
        }
        else
        {
            *dp = uv2;
            return num_uv2;
        }
    }
    else if(perform_lmn == PerformDirectionDerivativeFirstDerivative)
    {
        if(perform_uvw == PerformDirectionDerivativeFirstDerivative2)
        {
            *dp = lmn2;
            return num_lmn2;
        }
        *dp = lmn;
        return num_lmn;
    }
    else if(perform_lmn == PerformDirectionDerivativeFirstDerivative2)
    {
        *dp = lmn2;
        return num_lmn2;
    }
    else if(perform_lmn == PerformDirectionDerivativeSecondDerivative)
    {
        *dp = lmn_2;
        return num_lmn_2;
    }
    else if(perform_lmn == PerformDirectionDerivativeSecondDerivative2)
    {
        *dp = lmn_22;
        return num_lmn_22;
    }
	fprintf(stderr, "Error in select_lmn_derivative_indices: unsupported perform_lmn value %s\n", performDirectionDerivativeTypeNames[perform_lmn]);
	exit(EXIT_FAILURE);
    return 0;
}
static unsigned int select_xyz_derivative_indices(enum PerformDirectionDerivativeType perform_xyz, const int * restrict * restrict dp)
{
    /* derivative pointer (dp) has the following order, */
#define  SELECT_XYZ_DERIVATIVE_INDICES_Xp         0  /* x+    */
#define  SELECT_XYZ_DERIVATIVE_INDICES_Xm         1  /* x-    */
#define  SELECT_XYZ_DERIVATIVE_INDICES_Yp         2  /* y+    */
#define  SELECT_XYZ_DERIVATIVE_INDICES_Ym         3  /* y-    */
#define  SELECT_XYZ_DERIVATIVE_INDICES_Zp         4  /* z+    */
#define  SELECT_XYZ_DERIVATIVE_INDICES_Zm         5  /* z-    */
#define  SELECT_XYZ_DERIVATIVE_INDICES_XpYp       6  /* x+y+  */
#define  SELECT_XYZ_DERIVATIVE_INDICES_XpYm       7  /* x+y-  */
#define  SELECT_XYZ_DERIVATIVE_INDICES_XmYp       8  /* x-y+  */
#define  SELECT_XYZ_DERIVATIVE_INDICES_XmYm       9  /* x-y-  */
#define  SELECT_XYZ_DERIVATIVE_INDICES_XpZp       10 /* x+z+  */
#define  SELECT_XYZ_DERIVATIVE_INDICES_XpZm       11 /* x+z-  */
#define  SELECT_XYZ_DERIVATIVE_INDICES_XmZp       12 /* x-z+  */
#define  SELECT_XYZ_DERIVATIVE_INDICES_XmZm       13 /* x-z-  */
#define  SELECT_XYZ_DERIVATIVE_INDICES_YpZp       14 /* y+z+  */
#define  SELECT_XYZ_DERIVATIVE_INDICES_YpZm       15 /* y+z-  */
#define  SELECT_XYZ_DERIVATIVE_INDICES_YmZp       16 /* y-z+  */
#define  SELECT_XYZ_DERIVATIVE_INDICES_YmZm       17 /* y-z-  */
#define  SELECT_XYZ_DERIVATIVE_INDICES_MAX        18
    /* The indices are relative to the non-offset source position. */
    static const int none[SELECT_XYZ_DERIVATIVE_INDICES_MAX]     = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    static const unsigned int num_none = 0;
    static const int xyz[SELECT_XYZ_DERIVATIVE_INDICES_MAX]      = {+0,-1,+1,-1,+2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    static const unsigned int num_xyz = 3;
    static const int xyz2[SELECT_XYZ_DERIVATIVE_INDICES_MAX]     = {+0,+1,+2,+3,+4,+5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    static const unsigned int num_xyz2 = 6;
    static const int xyz_2[SELECT_XYZ_DERIVATIVE_INDICES_MAX]    = {+0,+1,+2,+3,+4,+5,+6,-1,-1,+7,+8,-1,-1,+9,10,-1,-1,11};
    static const unsigned int num_xyz_2 = 12;
    static const int xyz_22[SELECT_XYZ_DERIVATIVE_INDICES_MAX]   = {+0,+1,+2,+3,+4,+5,+6,+7,+8,+9,10,11,12,13,14,15,16,17};
    static const unsigned int num_xyz_22 = 18;
    if(perform_xyz == PerformDirectionDerivativeNone)
    {
        *dp = none;
        return num_none;
    }
    else if(perform_xyz == PerformDirectionDerivativeFirstDerivative)
    {
        *dp = xyz;
        return num_xyz;
    }
    else if(perform_xyz == PerformDirectionDerivativeFirstDerivative2)
    {
        *dp = xyz2;
        return num_xyz2;
    }
    else if(perform_xyz == PerformDirectionDerivativeSecondDerivative)
    {
        *dp = xyz_2;
        return num_xyz_2;
    }
    else if(perform_xyz == PerformDirectionDerivativeSecondDerivative2)
    {
        *dp = xyz_22;
        return num_xyz_22;
    }
	fprintf(stderr, "Error in select_xyz_derivative_indices: unsupported perform_xyz value %s\n", performDirectionDerivativeTypeNames[perform_xyz]);
	exit(EXIT_FAILURE);
    return 0;
}



static unsigned int calculateSourceIndexOffset(const DifxJob* const job, const DifxSource* const source)
{
    enum PerformDirectionDerivativeType perform_uvw;
    enum PerformDirectionDerivativeType perform_lmn;
    enum PerformDirectionDerivativeType perform_xyz;
    const int* junk;
    unsigned int count = 0;
    
    perform_uvw = (source->perform_uvw_deriv == PerformDirectionDerivativeDefault) ? job->perform_uvw_deriv : source->perform_uvw_deriv;
    perform_lmn = (source->perform_lmn_deriv == PerformDirectionDerivativeDefault) ? job->perform_lmn_deriv : source->perform_lmn_deriv;
    perform_xyz = (source->perform_xyz_deriv == PerformDirectionDerivativeDefault) ? job->perform_xyz_deriv : source->perform_xyz_deriv;
    
    count += select_lmn_derivative_indices(perform_uvw, perform_lmn, &junk);
    count += select_xyz_derivative_indices(perform_xyz, &junk);

    return count;
}
















/* When the source has a non-zero proper motion, this function should
   only be used when the proper motion is small enough that leap
   seconds are unimportant, and light travel time (retarded positions)
   are also unimportant.  It also does not perform relativistic
   corrections.  The output position is the position at this_MJD.
*/
static int convert_RA_Dec_PM_to_vector(const double RA, const double Dec, const double parallax, const double muRA, const double muDec, const double radial_vel, const double pmEpoch, const double this_MJD, double* const restrict pos, double* const restrict vel, double* const restrict ref_dir)
{
    /* RA and Dec come in in units of radians
       parallax has units of arcseconds
       muRA and muDec have units of arcseconds per Julian year
       radial_vel has units of meters per second
       pmEpoch is an MJD.  If 0.0, it is the current date/time
       this_MJD is the time at which to calculate the position, as an MJD

       pos has units of meters
       vel has units of meters per second
       ref_dir is the source reference direction and is a unit vector
    */
    double r;
    if(parallax == 0.0)
    {
        /* This usually means that the parallax is unknown.  Put in
           some very large value.  Let's assume a redshift of 1.65, which in
           today's current cosmology values gives an angular size distance
           of 1776.9 Mpc or 5.483E25 m (approximately the maximum angular size
           distance possible with the current cosmological values)
        */
        r = 5.483E25;
    }
    else
    {
        r = ASTR_UNIT_IAU_2012 / (parallax /(3600.0*180.0) * DIFX_PI);
    }
    if((muRA == 0.0) && (muDec == 0.0) && (radial_vel == 0.0))
    {
        /* no proper motion */
        difx_RADec_to_Cartesian(RA, Dec, r, pos);
        vel[2] = vel[1] = vel[0] = 0.0;
		/* handle reference direction */
		{
			double ca, sa;
			double cd, sd;
#ifdef _GNU_SOURCE
			sincos(RA, &sa, &ca);
			sincos(Dec, &sd, &cd);
#else
			sa = sin(RA);
			ca = cos(RA);
			sd = sin(Dec);
			cd = cos(Dec);
#endif
			ref_dir[0] = -ca*sd;
			ref_dir[1] = -sa*sd;
			ref_dir[2] = cd;
		}
    }
    else
    {
        /* correct for proper motion */
        double pos_0[DIFXCALC_3D_VEC_SIZE], ref_dir[DIFXCALC_3D_VEC_SIZE];
        double pmRA, pmDec;
        /* convert proper motions to rad/s from arcsec/yr */
        pmRA  = muRA  * (DIFX_PI/(180.0*3600.0))/ (JUL_YEAR*SEC_DAY_DBL);
        pmDec = muDec * (DIFX_PI/(180.0*3600.0))/ (JUL_YEAR*SEC_DAY_DBL);
        difx_RADec_proper_motion_to_Cartesian(RA, Dec, r, pmRA, pmDec, radial_vel, pos_0, vel, ref_dir);
        if(pmEpoch != 0.0)
        {
            double dt;
            dt = (this_MJD - pmEpoch) * SEC_DAY_DBL;
            pos[0] = pos_0[0] + dt*vel[0];
            pos[1] = pos_0[1] + dt*vel[1];
            pos[2] = pos_0[2] + dt*vel[2];
            return 1;
        }
    }
    return 0;
}
static int convert_vector_to_RA_Dec_PM(const double* const restrict pos, const double* const restrict vel, double* RA, double* Dec, double* parallax, double* muRA, double* muDec, double* radial_vel)
{
    /* 
       pos has units of meters
       vel has units of meters per second

       RA and Dec are in units of radians
       parallax has units of arcseconds
       muRA and muDec have units of arcseconds per Julian year
       radial_vel has units of meters per second
    */
    double r;
    double pmRA, pmDec;
    difx_Cartesian_to_RADec_proper_motion(pos, vel, RA, Dec, &r, &pmRA, &pmDec, radial_vel);
    if(r == 0.0)
    {
        /* The source is at the center of the coordinate system?
           (Center of Earth or Solar barycenter?)
        */
        *parallax = DBL_MAX;
    }
    else
    {
        *parallax = (ASTR_UNIT_IAU_2012/r) * (180.0*3600.0) / DIFX_PI;
    }
    /* convert proper motions from rad/s to arcsec/yr */
    *muRA  = pmRA  * (JUL_YEAR*SEC_DAY_DBL) * (180.0*3600.0) / DIFX_PI;
    *muDec = pmDec * (JUL_YEAR*SEC_DAY_DBL) * (180.0*3600.0) / DIFX_PI;
    return 0;
}















static int check_delayserver_operation(DifxInput* const D, CalcParams* const p, const int verbose)
{
	int_fast32_t retval = p->delayDistributor->test_delay_service(int_fast32_t(verbose));
	if(retval != 0)
	{
		fprintf(stderr, "ERROR : Delay server is returning BAD DATA.\n");
        fprintf(stderr, "        Restart the delay servers.\n");
        exit(EXIT_FAILURE);
	}
	retval = p->delayDistributor->test_parameter_service(int_fast32_t(verbose));
	if(retval != 0)
	{
		fprintf(stderr, "ERROR : Delay server is returning BAD PARAMETER DATA.\n");
        fprintf(stderr, "        Restart the delay servers.\n");
        exit(EXIT_FAILURE);
	}

    return 0;
};




static int get_Delay_Server_Parameters(DifxInput* const D, const DifxJob* const job, CalcParams* const p, const int verbose)
{
    struct SERVER_MODEL_PARAMETERS_ARGUMENT argument;
    struct SERVER_MODEL_PARAMETERS_RESPONSE response;

    memset(&argument, 0, sizeof(struct SERVER_MODEL_PARAMETERS_ARGUMENT));
    argument.request_id = p->argument_id++;
    argument.verbosity = verbose;
    if(D->job->delayServerType == CALC_9_1_RA_Server)
    {
        argument.server_struct_setup_code = 0x0510;
    }
    else
    {
        argument.server_struct_setup_code = 0;
    }

    int_fast32_t return_code = p->delayDistributor->process_parameter_service(D, job, NULL, &argument, &response, int_fast32_t(verbose));
    if((return_code))
    {
	    fprintf(stderr,"Error: get_Delay_Server_Parameters: got return_code=%"PRIdFAST32"\n", return_code);
        return -1;
    }
    
    if(((response.handler_error))
       || ((response.rpc_handler_error))
       || ((response.server_error))
       || ((response.model_error)))
    {
	    fprintf(stderr,"Error: get_Delay_Server_Parameters: error in processing parameters\n");
        return -2;
    }

    if(verbose)
    {
	    printf ("Delay Server is returning normal parameter data.\n");
    }
    free(D->job->calcParamTable);
    D->job->calcParamTable = (DifxCalcParamTable*)malloc(sizeof(DifxCalcParamTable));
            
    D->job->calcParamTable->accelgrv    = response.accelgrv     ;
    D->job->calcParamTable->e_flat      = response.e_flat       ;
    D->job->calcParamTable->earthrad    = response.earthrad     ;
    D->job->calcParamTable->mmsems      = response.mmsems       ;
    D->job->calcParamTable->ephepoc     = response.ephepoc      ;
    D->job->calcParamTable->gauss       = response.gauss        ;
    D->job->calcParamTable->u_grv_cn    = response.u_grv_cn     ;
    D->job->calcParamTable->gmsun       = response.gmsun        ;
    D->job->calcParamTable->gmmercury   = response.gmmercury    ;
    D->job->calcParamTable->gmvenus     = response.gmvenus      ;
    D->job->calcParamTable->gmearth     = response.gmearth      ;
    D->job->calcParamTable->gmmoon      = response.gmmoon       ;
    D->job->calcParamTable->gmmars      = response.gmmars       ;
    D->job->calcParamTable->gmjupiter   = response.gmjupiter    ;
    D->job->calcParamTable->gmsaturn    = response.gmsaturn     ;
    D->job->calcParamTable->gmuranus    = response.gmuranus     ;
    D->job->calcParamTable->gmneptune   = response.gmneptune    ;
    D->job->calcParamTable->etidelag    = response.etidelag     ;
    D->job->calcParamTable->love_h      = response.love_h       ;
    D->job->calcParamTable->love_l      = response.love_l       ;
    D->job->calcParamTable->pre_data    = response.pre_data     ;
    D->job->calcParamTable->rel_data    = response.rel_data     ;
    D->job->calcParamTable->tidalut1    = response.tidalut1     ;
    D->job->calcParamTable->au          = response.au           ;
    D->job->calcParamTable->tsecau      = response.tsecau       ;
    D->job->calcParamTable->vlight      = response.vlight       ;
    return 0;
};





int difxCalcInit(DifxInput* const D, CalcParams* const p, const int verbose)
{
	struct SERVER_MODEL_DELAY_ARGUMENT *argument = &(p->argument);
	struct SERVER_MODEL_DELAY_RESPONSE *response = &(p->response);
    int i;

    static const char Earth_Center[] = "EC";
    const char *delayServerHost;
    static const char * const localhost = "localhost";
    uint16_t station_ID;
    size_t station_size, source_size, EOP_size, result_size;
    void *station_mem, *source_mem, *EOP_mem, *result_mem;

    if(p->delayDistributor != NULL)
    {
      delete p->delayDistributor; p->delayDistributor = NULL;
    }

    delayServerHost = D->job->delayServerHost;
    if((delayServerHost == NULL) || (delayServerHost[0] == 0))
    {
	    if((delayServerHost = getenv("DIFX_DELAY_SERVER")) != NULL)
	    {
	    }
	    else {
		    delayServerHost = localhost;
	    }
		snprintf(D->job->delayServerHost, DIFXIO_HOSTNAME_LENGTH, "%s", delayServerHost);
    }
	if(verbose >= 2)
	{
		fprintf(stderr, "delay server handler %d %s server types %d %s verbosity %d\n", (int)D->job->delayServerHandlerType, delayServerHandlerTypeNames[D->job->delayServerHandlerType], (int)D->job->delayServerType, delayServerTypeNames[D->job->delayServerType], verbose);
	}
    p->delayDistributor = new DelayHandlerDistributor(p->Num_CALC_Threads, D->job->delayServerHost, D->job->delayServerHandlerType, D->job->delayServerType, int_fast32_t(verbose));

    i = check_delayserver_operation(D, p, verbose);
    if(i < 0)
    {
        fprintf(stderr, "Error: check_delayserver_operation not working in difxCalcInit, got return code %d\n", i);
        return -2;
    }
    i = get_Delay_Server_Parameters(D, D->job, p, verbose);
    if(i < 0)
    {
        fprintf(stderr, "Error: get_Delay_Server_Parameters not working in difxCalcInit, got return code %d\n", i);
        return -3;
    }

    unsigned long calcserver_version = p->delayDistributor->get_detailed_version_number();
    if(verbose >= 1) {
        printf("Delay Server version 0x%lX detected for type %s\n", calcserver_version, delayServerTypeNames[D->job->delayServerType]);
    }
    D->job->delayProgramDetailedVersion = calcserver_version;


    memset(argument, 0, sizeof(SERVER_MODEL_DELAY_ARGUMENT));
    p->Num_Allocated_Stations = 2;
    station_size = sizeof(struct SERVER_MODEL_DELAY_ARGUMENT_STATION)*p->Num_Allocated_Stations;
    if((station_mem = malloc(station_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc initial station memory for %"PRIu32" stations\n", p->Num_Allocated_Stations);
        exit(EXIT_FAILURE);
    }
    memset(station_mem, 0, station_size);
    argument->Num_Stations = p->Num_Allocated_Stations;
    argument->station = reinterpret_cast<struct SERVER_MODEL_DELAY_ARGUMENT_STATION*>(station_mem);
    p->Num_Allocated_Sources = 3;
    source_size = sizeof(struct SERVER_MODEL_DELAY_ARGUMENT_SOURCE)*p->Num_Allocated_Sources;
    if((source_mem = malloc(source_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc initial source memory for %"PRIu32" sources\n", p->Num_Allocated_Sources);
        exit(EXIT_FAILURE);
    }
    memset(source_mem, 0, source_size);
    argument->Num_Sources = p->Num_Allocated_Sources;
    argument->source = reinterpret_cast<struct SERVER_MODEL_DELAY_ARGUMENT_SOURCE*>(source_mem);
    argument->Num_EOPs = D->nEOP;
    EOP_size = sizeof(struct SERVER_MODEL_DELAY_ARGUMENT_EOP)*argument->Num_EOPs;
    if((EOP_mem = malloc(EOP_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc initial EOP memory for %"PRIu32" EOPs\n", argument->Num_EOPs);
        exit(EXIT_FAILURE);
    }
    memset(EOP_mem, 0, EOP_size);
    argument->EOP = reinterpret_cast<struct SERVER_MODEL_DELAY_ARGUMENT_EOP*>(EOP_mem);

    argument->verbosity = int32_t(verbose);
    if(D->job->delayServerType == CALC_9_1_RA_Server)
    {
        argument->server_struct_setup_code = 0x0510;
    }
    else
    {
        argument->server_struct_setup_code = 0;
    }
    argument->request_id = 0;
    argument->ref_frame = 0;
    
    for (uint_fast8_t k = 0; k < NUM_DIFX_DELAYHANDLERDISTRIBUTOR_KFLAGS; ++k)
        argument->kflags[k] = -1;
    argument->sky_frequency = 10.E9;
    argument->Use_Server_Station_Table = 0;
    argument->Use_Server_Source_Table = 0;
    argument->Use_Server_EOP_Table = 0;

    station_ID = (uint16_t)(Earth_Center[0]) | ((uint16_t)(Earth_Center[1]) << 8);
    difx_strlcpy(argument->station[0].station_name, Earth_Center, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    difx_strlcpy(argument->station[0].antenna_name, Earth_Center, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    difx_strlcpy(argument->station[0].site_name, Earth_Center, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    argument->station[0].site_ID = station_ID;
    difx_strlcpy(argument->station[0].site_type, "fixed", DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    difx_strlcpy(argument->station[0].axis_type, "altz", DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    for(uint_fast8_t k=0; k < 3; ++k)
    {
	    argument->station[0].station_pos[k] = 0.0;
	    argument->station[0].station_vel[k] = 0.0;
	    argument->station[0].station_acc[k] = 0.0;
	    argument->station[0].station_pointing_dir[k] = 0.0;
	    argument->station[0].station_reference_dir[k] = 0.0;
    }
    argument->station[0].station_coord_frame = int32_t(SourceCoordinateFrameITRF2008);
    argument->station[0].pointing_coord_frame = int32_t(SourceCoordinateFrameJ2000);
    argument->station[0].pointing_corrections_applied = 0;
    argument->station[0].station_position_delay_offset = 0.0;
    argument->station[0].axis_off = 0.0;
    argument->station[0].primary_axis_wrap = 0;
    argument->station[0].secondary_axis_wrap = 0;
    argument->station[0].receiver_name[0] = 0;
    argument->station[0].pressure = 0.0;
    argument->station[0].antenna_pressure = 0.0;
    argument->station[0].temperature = 0.0;
    argument->station[0].wind_speed = fitsnan.d;
    argument->station[0].wind_direction = fitsnan.d;
    argument->station[0].antenna_phys_temperature = 0.0;

    if(D->nEOP >= MIN_NUM_EOPS)
    {
        for(uint_fast32_t e = 0; e < argument->Num_EOPs; ++e)
        {
            argument->EOP[e].EOP_time = D->eop[e].mjd;
            argument->EOP[e].tai_utc  = D->eop[e].tai_utc;
            argument->EOP[e].ut1_utc  = D->eop[e].ut1_utc;
            argument->EOP[e].xpole    = D->eop[e].xPole;
            argument->EOP[e].ypole    = D->eop[e].yPole;
        }
    }
    else
    {
        fprintf(stderr, "Not enough eop values present (%d < %d)\n", D->nEOP, MIN_NUM_EOPS);

        return -4;
    }

    /* check that eops bracket the observation */
    if(D->eop[D->nEOP-1].mjd < D->mjdStart ||
       D->eop[0].mjd          > D->mjdStop)
    {
        fprintf(stderr, "EOPs don't bracket the observation.\n");

        return -5;
    }

    /* copy the primary argument data struct information over to the
       special spacecraft handling argument area
    */
    p->sc_argument = *argument;
    p->sc_argument.Num_Stations = 3;
    p->sc_argument.station = p->sc_station;
    p->sc_argument.station[0] = argument->station[0];
    p->sc_argument.station[1] = argument->station[0];
    p->sc_argument.station[2] = argument->station[0];
    p->sc_argument.Num_Sources = 1;
    p->sc_argument.source = p->sc_source;


    // Now allocate space for the response
    memset(response, 0, sizeof(SERVER_MODEL_DELAY_RESPONSE));
    p->Num_Allocated_Stations = 2;
    result_size = sizeof(struct SERVER_MODEL_DELAY_RESPONSE_DATA) * p->Num_Allocated_Stations * p->Num_Allocated_Sources;
    if((result_mem = malloc(result_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc initial result memory for %"PRIu32" stations*sources\n", p->Num_Allocated_Stations * p->Num_Allocated_Sources);
        exit(EXIT_FAILURE);
    }
    memset(result_mem, 0, result_size);
    response->Num_Stations = p->Num_Allocated_Stations;
    response->Num_Sources = p->Num_Allocated_Sources;
    response->result = reinterpret_cast<struct SERVER_MODEL_DELAY_RESPONSE_DATA*>(result_mem);

    /* copy the primary response data struct information over to the
       special spacecraft handling response area
    */
    p->sc_response = *response;
    p->sc_response.Num_Stations = p->sc_argument.Num_Stations;
    p->sc_response.Num_Sources = p->sc_argument.Num_Sources;
    p->sc_response.result = p->sc_result;
    p->sc_response.result[0] = response->result[0];
    p->sc_response.result[1] = response->result[0];
    p->sc_response.result[2] = response->result[0];

	if(verbose >= 3)
	{
		fprintf(stderr, "difxCalcInit finishing successfully.\n");
	}
	
    return 0;
}

int CheckInputForSpacecraft(const DifxInput* const D, CalcParams* const p)
{
    int sc;
    if((p->allowNegDelay))
    {
        return 0;
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


static int calcSpacecraftSourcePosition(const DifxInput* const D, DifxSpacecraft* const sc, DifxSource* source, struct SERVER_MODEL_DELAY_ARGUMENT *argument, int spacecraftId, unsigned int sourceIndex, double delay_to_station_0)
{
    sixVector pos;
    int r;
    double RA, Dec, parallax;
    double muRA, muDec, radial_vel;
    double dpos[DIFXCALC_3D_VEC_SIZE], dvel[DIFXCALC_3D_VEC_SIZE];
    static int warn = 1;
    
    if((sc->is_antenna))
    {
        if((warn))
        {
            fprintf(stderr, "Error: told to calculate SOURCE position for spacecraft ANTENNA\n");
            warn = 0;
        }
        return -2;
    }
    if(source->sc_epoch == 0.0)
    {
        int mjd = argument->date;
        double frac = argument->time;
        if((D->job->calculate_own_retarded_position))
        {
            int offset;
            frac -= delay_to_station_0 / SEC_DAY_DBL;
            offset = floor(frac);
            mjd += offset;
            frac -= offset;
        }
        r = evaluateDifxSpacecraftSource(sc, mjd, frac, &pos);
    }
    else
    {
        int mjd = source->sc_epoch;
        double frac = source->sc_epoch-mjd;
        if((D->job->calculate_own_retarded_position))
        {
            int offset;
            frac -= delay_to_station_0 / SEC_DAY_DBL;
            offset = floor(frac);
            mjd += offset;
            frac -= offset;
        }
        r = evaluateDifxSpacecraftSource(sc, mjd, frac, &pos);
    }
    if(r < 0)
    {
        fprintf(stderr, "Error: received code %d from evaluateDifxSpacecraftSource in calcSpacecraftSourcePosition\n", r);
        return -1;
    }
    dpos[0] = pos.X;
    dpos[1] = pos.Y;
    dpos[2] = pos.Z;
    dvel[0] = pos.dX;
    dvel[1] = pos.dY;
    dvel[2] = pos.dZ;

    /* TODO: determine whether or not double or long double is really needed */
    /* proper motion is returned in arcseconds per Julian year */
    convert_vector_to_RA_Dec_PM(dpos, dvel, &RA, &Dec, &parallax, &muRA, &muDec, &radial_vel);

    argument->source[sourceIndex].ra  =  RA;
    argument->source[sourceIndex].dec =  Dec;
    argument->source[sourceIndex].parallax = parallax;
    argument->source[sourceIndex].source_pos[0] = pos.X;
    argument->source[sourceIndex].source_pos[1] = pos.Y;
    argument->source[sourceIndex].source_pos[2] = pos.Z;

    /* TODO: put in calculations to find the source_pointing_dir and
       source_pointing_reference_dir
    */
    argument->source[sourceIndex].source_pointing_dir[0] = 0.0;
    argument->source[sourceIndex].source_pointing_dir[1] = 0.0;
    argument->source[sourceIndex].source_pointing_dir[2] = 0.0;
    argument->source[sourceIndex].source_pointing_reference_dir[0] = 0.0;
    argument->source[sourceIndex].source_pointing_reference_dir[1] = 0.0;
    argument->source[sourceIndex].source_pointing_reference_dir[2] = 0.0;

    if(source->sc_epoch == 0.0)
    {
        /* proper motion comes back as meters per second. */
        argument->source[sourceIndex].source_vel[0] = pos.dX;
        argument->source[sourceIndex].source_vel[1] = pos.dY;
        argument->source[sourceIndex].source_vel[2] = pos.dZ;
        /* TODO: return acceleration of spacecraft for better accuracy
           for some applications? */
        argument->source[sourceIndex].source_acc[0] = 0.0;
        argument->source[sourceIndex].source_acc[1] = 0.0;
        argument->source[sourceIndex].source_acc[2] = 0.0;
        argument->source[sourceIndex].dra  = muRA;
        argument->source[sourceIndex].ddec = muDec;
        argument->source[sourceIndex].depoch = pos.mjd + pos.fracDay;
    }
    else
    {
        /* Fixed epoch means no motion is to be applied */
        argument->source[sourceIndex].source_vel[0] = 0.0;
        argument->source[sourceIndex].source_vel[1] = 0.0;
        argument->source[sourceIndex].source_vel[2] = 0.0;
        argument->source[sourceIndex].source_acc[0] = 0.0;
        argument->source[sourceIndex].source_acc[1] = 0.0;
        argument->source[sourceIndex].source_acc[2] = 0.0;

        argument->source[sourceIndex].dra  = 0.0;
        argument->source[sourceIndex].ddec = 0.0;
        argument->source[sourceIndex].depoch = 0.0;
    }

    return 0;
}

static int calcSpacecraftAsPointingDirection(const DifxInput* const D, DifxSpacecraft* const sc, const double epoch, struct SERVER_MODEL_DELAY_ARGUMENT* const argument, int spacecraftId, unsigned int antennaIndex, int calculate_own_retarded_position, double delay_to_station_0)
{
    sixVector pos;
    int r;
    static int warn = 1;

    if((sc->is_antenna))
    {
        if((warn))
        {
            fprintf(stderr, "Warning: spacecraft ANTENNA being used as SOURCE.  You shold use a separate spacecraft SOURCE in your setup to compensate for the difference between the VLBI antenna and the communications antenn locations.  And ask your DiFX develper to actually implement this.\n");
        }
        warn = 0;
    }
    if(epoch == 0.0)
    {
        int mjd = argument->date;
        double frac = argument->time;
        if((calculate_own_retarded_position))
        {
            int offset;
            frac -= delay_to_station_0 / SEC_DAY_DBL;
            offset = floor(frac);
            mjd += offset;
            frac -= offset;
        }            
        r = evaluateDifxSpacecraftSource(sc, mjd, frac, &pos);
    }
    else
    {
        int mjd = epoch;
        double frac = epoch-mjd;
        if((calculate_own_retarded_position))
        {
            int offset;
            frac -= delay_to_station_0 / SEC_DAY_DBL;
            offset = floor(frac);
            mjd += offset;
            frac -= offset;
        }            
        r = evaluateDifxSpacecraftSource(sc, mjd, frac, &pos);
    }
    if(r < 0)
    {
        fprintf(stderr, "Error: received code %d from evaluateDifxSpacecraftSource in calcSpacecraftAsPointingDirection\n", r);
        return -1;
    }

    argument->station[antennaIndex].station_pointing_dir[0] = pos.X;
    argument->station[antennaIndex].station_pointing_dir[1] = pos.Y;
    argument->station[antennaIndex].station_pointing_dir[2] = pos.Z;
    /* TODO: implement the spacecraft reference direction */

    return 0;
}

static int calcSpacecraftAntennaPosition(const DifxInput* const D, struct SERVER_MODEL_DELAY_ARGUMENT* const argument, int spacecraftId, unsigned int stationIndex, double baseline_delay)
{
    DifxSpacecraft *sc;
    nineVector pos;
    spacecraftAxisVectors pointing_direction, pointing_velocity;
    int r;
    int mjd = argument->date;
    double frac = argument->time;
    
    sc = D->spacecraft + spacecraftId;

    if((!sc->is_antenna))
    {
        fprintf(stderr, "Error: asked to perform ANTENNA position on spacecraft SOURCE\n");
        return -2;
    }
    mjd = argument->date;
    frac = argument->time;
    if((D->job->calculate_own_retarded_position))
    {
        int offset;
        frac -= baseline_delay / SEC_DAY_DBL;
        offset = floor(frac);
        mjd += offset;
        frac -= offset;
    }
    r = evaluateDifxSpacecraftAntenna(sc, mjd, frac, &pos);
    if(r < 0)
    {
        fprintf(stderr, "Error: received code %d from evaluateDifxSpacecraftAntenna in calcSpacecraftAntennaPosition\n", r);
        return -1;
    }
    argument->station[stationIndex].station_pos[0] = pos.X;
    argument->station[stationIndex].station_pos[1] = pos.Y;
    argument->station[stationIndex].station_pos[2] = pos.Z;
    argument->station[stationIndex].station_vel[0] = pos.dX;
    argument->station[stationIndex].station_vel[1] = pos.dY;
    argument->station[stationIndex].station_vel[2] = pos.dZ;
    argument->station[stationIndex].station_acc[0] = pos.ddX;
    argument->station[stationIndex].station_acc[1] = pos.ddY;
    argument->station[stationIndex].station_acc[2] = pos.ddZ;
        

    /* Now work on the pointing axes */
    r = evaluateDifxSpacecraftAntennaAxisVectors(sc,
                                                 mjd,
                                                 frac,
                                                &pointing_direction,
                                                &pointing_velocity);
    if(r < 0) {
        fprintf(stderr, "Error: received code %d from evaluateDifxSpacecraftAntennaAxisVectors in calcSpacecraftAntennaPosition\n", r);
        return -2;
    }
    argument->station[stationIndex].station_pointing_dir[0] = pointing_direction.Z[0];
    argument->station[stationIndex].station_pointing_dir[1] = pointing_direction.Z[1];
    argument->station[stationIndex].station_pointing_dir[2] = pointing_direction.Z[2];
    argument->station[stationIndex].station_reference_dir[0] = pointing_direction.X[0];
    argument->station[stationIndex].station_reference_dir[1] = pointing_direction.X[1];
    argument->station[stationIndex].station_reference_dir[2] = pointing_direction.X[2];

    return 0;
}


static unsigned int extractCalcResultsSingleSourceDerivs(const double delta_lmn, const double delta_xyz, const enum PerformDirectionDerivativeType perform_uvw, const enum PerformDirectionDerivativeType perform_lmn, const enum PerformDirectionDerivativeType perform_xyz, const int* const lmn_indices, const int* const num_xyz, const double* const restrict dlmn, const double* const restrict dxyz, const unsigned int index, struct modelTemp* model, struct modelTempLMN* modelLMN, struct modelTempXYZ* modelXYZ, const int verbose)
{
    /* See http://en.wikipedia.org/wiki/Finite_difference */
    double lmn_mult = C_LIGHT/delta_lmn;
    double xyz_mult = C_LIGHT/delta_xyz;
    double lmn_mult2 = lmn_mult / delta_lmn;
    double xyz_mult2 = xyz_mult / delta_xyz;

    switch(perform_uvw) {
        /* Remeber that u points in the opposite direction to l */
    case PerformDirectionDerivativeFirstDerivative2:
        {
            if(verbose >= 3)
            {
                printf("extractCalcResultsSingleSourceDerivs results\n");
                printf("    original delay server  UVW = %14E %14E %14E\n", model->u[index], model->v[index], model->w[index]);
            }
            model->u[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lm] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lp]) * 0.5;
            model->v[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mm]) * 0.5;
            model->w[index] = C_LIGHT * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0];
            if(verbose >= 3)
            {
                printf("    numerical delay server UVW = %14E %14E %14E\n", model->u[index], model->v[index], model->w[index]);
            }
        }
        break;
    case PerformDirectionDerivativeFirstDerivative:
        {
            if(verbose >= 3)
            {
                printf("extractCalcResultsSingleSourceDerivs results\n");
                printf("    original delay server  UVW = %14E %14E %14E\n", model->u[index], model->v[index], model->w[index]);
            }
            model->u[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lp]);
            model->v[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]);
            model->w[index] = C_LIGHT * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0];
            /* model->u[index] = (C_LIGHT/results->delta)*(d-dx); */
            /* model->v[index] = (C_LIGHT/results->delta)*(dy-d); */
            /* model->w[index] = C_LIGHT*d; */
            if(verbose >= 3)
            {
                printf("    numerical delay server UVW = %14E %14E %14E\n", model->u[index], model->v[index], model->w[index]);
            }
        }
        break;
    default:;
    }
    /* we are supposed to convert to microseconds */
    lmn_mult *= MICROSECONDS_PER_SECOND;
    xyz_mult *= MICROSECONDS_PER_SECOND;
    lmn_mult2 *= MICROSECONDS_PER_SECOND;
    xyz_mult2 *= MICROSECONDS_PER_SECOND;
    switch(perform_lmn) {
    case PerformDirectionDerivativeSecondDerivative2:
        {
            modelLMN->dDelay_dl[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lm]) * 0.5;
            modelLMN->dDelay_dm[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mm]) * 0.5;
            modelLMN->dDelay_dn[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Np] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Nm]) * 0.5 / DIFXIO_DELTA_LMN_N_FACTOR;
            modelLMN->d2Delay_dldl[index] = lmn_mult2 * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lp] - 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] + dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lm]);
            modelLMN->d2Delay_dldm[index] = lmn_mult2 * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_LpMp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_LpMm] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_LmMp] + dlmn[SELECT_LMN_DERIVATIVE_INDICES_LmMm]) * 0.25;
            modelLMN->d2Delay_dldn[index] = lmn_mult2 * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_LpNp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_LpNm] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_LmNp] + dlmn[SELECT_LMN_DERIVATIVE_INDICES_LmNm]) * 0.25 / DIFXIO_DELTA_LMN_N_FACTOR;
            modelLMN->d2Delay_dmdm[index] = lmn_mult2 * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mp] - 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] + dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mm]);
            modelLMN->d2Delay_dmdn[index] = lmn_mult2 * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_MpNp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_MpNm] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_MmNp] + dlmn[SELECT_LMN_DERIVATIVE_INDICES_MmNm]) * 0.25 / DIFXIO_DELTA_LMN_N_FACTOR;
            modelLMN->d2Delay_dndn[index] = lmn_mult2 * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Np] - 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] + dlmn[SELECT_LMN_DERIVATIVE_INDICES_Nm]) / (DIFXIO_DELTA_LMN_N_FACTOR*DIFXIO_DELTA_LMN_N_FACTOR);
            
        }
        break;
    case PerformDirectionDerivativeSecondDerivative:
        {
            modelLMN->dDelay_dl[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lm]) * 0.5;
            modelLMN->dDelay_dm[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mm]) * 0.5;
            modelLMN->dDelay_dn[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Np] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Nm]) * 0.5 / DIFXIO_DELTA_LMN_N_FACTOR;
            modelLMN->d2Delay_dldl[index] = lmn_mult2 * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lp] - 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] + dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lm]);
            modelLMN->d2Delay_dldm[index] = lmn_mult2 * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_LpMp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mp] + 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lm] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mm]+ dlmn[SELECT_LMN_DERIVATIVE_INDICES_LmMm]) * 0.5;
            modelLMN->d2Delay_dldn[index] = lmn_mult2 * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_LpNp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Np] + 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lm] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Nm]+ dlmn[SELECT_LMN_DERIVATIVE_INDICES_LmNm]) * 0.5 / DIFXIO_DELTA_LMN_N_FACTOR;
            modelLMN->d2Delay_dmdm[index] = lmn_mult2 * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mp] - 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] + dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mm]);
            modelLMN->d2Delay_dmdn[index] = lmn_mult2 * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_MpNp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Np] + 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mm] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Nm]+ dlmn[SELECT_LMN_DERIVATIVE_INDICES_MmNm]) * 0.5 / DIFXIO_DELTA_LMN_N_FACTOR;
            modelLMN->d2Delay_dndn[index] = lmn_mult2 * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Np] - 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] + dlmn[SELECT_LMN_DERIVATIVE_INDICES_Nm]) / (DIFXIO_DELTA_LMN_N_FACTOR*DIFXIO_DELTA_LMN_N_FACTOR);
            
        }
        break;
    case PerformDirectionDerivativeFirstDerivative2:
        {
            modelLMN->dDelay_dl[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lm]) * 0.5;
            modelLMN->dDelay_dm[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mm]) * 0.5;
            modelLMN->dDelay_dn[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Np] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Nm]) * 0.5 / DIFXIO_DELTA_LMN_N_FACTOR;
            modelLMN->d2Delay_dldl[index] = fitsnan.d;
            modelLMN->d2Delay_dldm[index] = fitsnan.d;
            modelLMN->d2Delay_dldn[index] = fitsnan.d;
            modelLMN->d2Delay_dmdm[index] = fitsnan.d;
            modelLMN->d2Delay_dmdn[index] = fitsnan.d;
            modelLMN->d2Delay_dndn[index] = fitsnan.d;
        }
        break;
    case PerformDirectionDerivativeFirstDerivative:
        {
            modelLMN->dDelay_dl[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]);
            modelLMN->dDelay_dm[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]);
            modelLMN->dDelay_dn[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Np] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]) / DIFXIO_DELTA_LMN_N_FACTOR;
            modelLMN->d2Delay_dldl[index] = fitsnan.d;
            modelLMN->d2Delay_dldm[index] = fitsnan.d;
            modelLMN->d2Delay_dldn[index] = fitsnan.d;
            modelLMN->d2Delay_dmdm[index] = fitsnan.d;
            modelLMN->d2Delay_dmdn[index] = fitsnan.d;
            modelLMN->d2Delay_dndn[index] = fitsnan.d;
        }
        break;
    default:;
    }
    switch(perform_xyz) {
    case PerformDirectionDerivativeSecondDerivative2:
        {
            modelXYZ->dDelay_dX[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xm]) * 0.5;
            modelXYZ->dDelay_dY[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Yp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Ym]) * 0.5;
            modelXYZ->dDelay_dZ[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zm]) * 0.5;
            modelXYZ->d2Delay_dXdX[index] = xyz_mult2 * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xp] - 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] + dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xm]);
            modelXYZ->d2Delay_dXdY[index] = xyz_mult2 * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_XpYp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_XpYm] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_XmYp] + dxyz[SELECT_XYZ_DERIVATIVE_INDICES_XmYm]) * 0.25;
            modelXYZ->d2Delay_dXdZ[index] = xyz_mult2 * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_XpZp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_XpZm] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_XmZp] + dxyz[SELECT_XYZ_DERIVATIVE_INDICES_XmZm]) * 0.25;
            modelXYZ->d2Delay_dYdY[index] = xyz_mult2 * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Yp] - 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] + dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Ym]);
            modelXYZ->d2Delay_dYdZ[index] = xyz_mult2 * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_YpZp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_YpZm] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_YmZp] + dxyz[SELECT_XYZ_DERIVATIVE_INDICES_YmZm]) * 0.25;
            modelXYZ->d2Delay_dZdZ[index] = xyz_mult2 * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zp] - 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] + dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zm]);
            
        }
        break;
    case PerformDirectionDerivativeSecondDerivative:
        {
            modelXYZ->dDelay_dX[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xm]) * 0.5;
            modelXYZ->dDelay_dY[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Yp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Ym]) * 0.5;
            modelXYZ->dDelay_dZ[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zm]) * 0.5;
            modelXYZ->d2Delay_dXdX[index] = xyz_mult2 * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xp] - 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] + dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xm]);
            modelXYZ->d2Delay_dXdY[index] = xyz_mult2 * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_XpYp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Yp] + 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xm] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Ym]+ dxyz[SELECT_XYZ_DERIVATIVE_INDICES_XmYm]) * 0.5;
            modelXYZ->d2Delay_dXdZ[index] = xyz_mult2 * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_XpZp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zp] + 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xm] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zm]+ dxyz[SELECT_XYZ_DERIVATIVE_INDICES_XmZm]) * 0.5;
            modelXYZ->d2Delay_dYdY[index] = xyz_mult2 * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Yp] - 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] + dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Ym]);
            modelXYZ->d2Delay_dYdZ[index] = xyz_mult2 * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_YpZp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Yp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zp] + 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Ym] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zm]+ dxyz[SELECT_XYZ_DERIVATIVE_INDICES_YmZm]) * 0.5;
            modelXYZ->d2Delay_dZdZ[index] = xyz_mult2 * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zp] - 2.0 * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] + dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zm]);
            
        }
        break;
    case PerformDirectionDerivativeFirstDerivative2:
        {
            modelXYZ->dDelay_dX[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xm]) * 0.5;
            modelXYZ->dDelay_dY[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Yp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Ym]) * 0.5;
            modelXYZ->dDelay_dZ[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zp] - dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zm]) * 0.5;
            modelXYZ->d2Delay_dXdX[index] = fitsnan.d;
            modelXYZ->d2Delay_dXdY[index] = fitsnan.d;
            modelXYZ->d2Delay_dXdZ[index] = fitsnan.d;
            modelXYZ->d2Delay_dYdY[index] = fitsnan.d;
            modelXYZ->d2Delay_dYdZ[index] = fitsnan.d;
            modelXYZ->d2Delay_dZdZ[index] = fitsnan.d;
        }
        break;
    case PerformDirectionDerivativeFirstDerivative:
        {
            modelXYZ->dDelay_dX[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]);
            modelXYZ->dDelay_dY[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Yp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]);
            modelXYZ->dDelay_dZ[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]);
            modelXYZ->d2Delay_dXdX[index] = fitsnan.d;
            modelXYZ->d2Delay_dXdY[index] = fitsnan.d;
            modelXYZ->d2Delay_dXdZ[index] = fitsnan.d;
            modelXYZ->d2Delay_dYdY[index] = fitsnan.d;
            modelXYZ->d2Delay_dYdZ[index] = fitsnan.d;
            modelXYZ->d2Delay_dZdZ[index] = fitsnan.d;
        }
        break;
    default:;
    }
    return 0;
}




static unsigned int extractCalcResultsSingleSource(const DifxInput* const D, const CalcParams* const p, const DifxAntenna* const antenna, const DifxSource* const source, const unsigned int antIndex, const unsigned int sourceIndex, const unsigned int timeIndex, struct modelTemp* model, struct modelTempLMN* modelLMN, struct modelTempXYZ* modelXYZ, const struct SERVER_MODEL_DELAY_RESPONSE* const response, unsigned int* num_source_entries, const int verbose)
{
    DifxJob* job;
    DifxSpacecraft* sc;
    const struct SERVER_MODEL_DELAY_RESPONSE_DATA* res;
    unsigned int r_index;
    enum PerformDirectionDerivativeType perform_uvw;
    enum PerformDirectionDerivativeType perform_lmn;
    enum PerformDirectionDerivativeType perform_xyz;
    const int* lmn_indices;
    const int* xyz_indices;
    unsigned int num_lmn;
    unsigned int num_xyz;
    unsigned int i;
    double dlmn[SELECT_LMN_DERIVATIVE_INDICES_MAX];
    double dxyz[SELECT_XYZ_DERIVATIVE_INDICES_MAX];
    unsigned int rv=0;

    job = D->job;
    rv = 0;
    r_index = antIndex*response->Num_Sources + sourceIndex;
    res = &(response->result[r_index]);
    perform_uvw = (source->perform_uvw_deriv == PerformDirectionDerivativeDefault) ? job->perform_uvw_deriv : source->perform_uvw_deriv;
    perform_lmn = (source->perform_lmn_deriv == PerformDirectionDerivativeDefault) ? job->perform_lmn_deriv : source->perform_lmn_deriv;
    perform_xyz = (source->perform_xyz_deriv == PerformDirectionDerivativeDefault) ? job->perform_xyz_deriv : source->perform_xyz_deriv;

    num_lmn = select_lmn_derivative_indices(perform_uvw, perform_lmn, &lmn_indices);
    num_xyz = select_xyz_derivative_indices(perform_xyz, &xyz_indices);
    *num_source_entries = num_lmn + num_xyz;

    if(isnan(res[0].delay))      rv |=  0x1;
    if(isinf(res[0].delay))      rv |=  0x2;
    if(isnan(res[0].dry_atmos))  rv |=  0x4;
    if(isinf(res[0].dry_atmos))  rv |=  0x8;
    if(isnan(res[0].wet_atmos))  rv |= 0x10;
    if(isinf(res[0].wet_atmos))  rv |= 0x20;
    /* if(isnan(res[0].iono_atmos)) rv |= 0x40; */
    if(isinf(res[0].iono_atmos)) rv |= 0x80;

    model->delay[timeIndex]    = -res[0].delay*MICROSECONDS_PER_SECOND;
    model->dry[timeIndex]      =  res[0].dry_atmos*MICROSECONDS_PER_SECOND;
    model->wet[timeIndex]      =  res[0].wet_atmos*MICROSECONDS_PER_SECOND;
    model->iono[timeIndex]     =  res[0].iono_atmos*MICROSECONDS_PER_SECOND;
    model->az[timeIndex]       =  res[0].az_geom*180.0/DIFX_PI;
    model->elgeom[timeIndex]   =  res[0].el_geom*180.0/DIFX_PI;
    model->msa[timeIndex]      =  res[0].mount_source_angle*180.0/DIFX_PI;
    model->elcorr[timeIndex]   =  res[0].el_corr*180.0/DIFX_PI;
    model->parangle[timeIndex] =  res[0].mount_source_angle*180.0/DIFX_PI;
    model->u[timeIndex]        =  res[0].UVW[0];
    model->v[timeIndex]        =  res[0].UVW[1];
    model->w[timeIndex]        =  res[0].UVW[2];
    if(antenna->spacecraftId < 0)
    {
        model->sc_gs_delay[timeIndex]    = 0.0;
        model->gs_sc_delay[timeIndex]    = 0.0;
        model->gs_clock_delay[timeIndex] = 0.0;
    }
    else
    {
        sc = D->spacecraft + antenna->spacecraftId;
        model->sc_gs_delay[timeIndex]    = (sc->GS_recording_delay)*MICROSECONDS_PER_SECOND;
        model->gs_sc_delay[timeIndex]    = (sc->GS_transmission_delay + sc->GS_transmission_delay_sync)*MICROSECONDS_PER_SECOND;
        model->gs_clock_delay[timeIndex] = (-sc->GS_clock_delay - sc->GS_clock_delay_sync)*MICROSECONDS_PER_SECOND;
    }

    if(verbose >= 3)
    {
        printf("extractCalcResultsSingleSource results for antenna='%s' source='%s' at time MJD %ld %.16f\n", antenna->name, source->name, response->date, response->time);
        printf("    delay=%14E dry_delay=%14E wet_delay=%14E iono_delay=%14E\n", model->delay[timeIndex], model->dry[timeIndex], model->wet[timeIndex], model->iono[timeIndex]);
        printf("    delay server UVW = %14E %14E %14E\n", res[0].UVW[0], res[0].UVW[1], res[0].UVW[2]);
    }

    if(((perform_uvw)) || ((perform_lmn)) || ((perform_xyz)))
    {
        switch(D->job->aberCorr) {
        case AberCorrExact:
            {
                for(i=0; i < SELECT_LMN_DERIVATIVE_INDICES_MAX; i++)
                {
                    if(lmn_indices[i] >= 0)
                    {
                        dlmn[i] = res[lmn_indices[i]].delay - res[lmn_indices[i]].wet_atmos - res[lmn_indices[i]].dry_atmos;
						/* TODO: instead of blindly calculating an ionospheric
						   delay correction at 1 GHz, the structure needs to be
						   changed to output a polynomial correction depending
						   on \nu^{-2} for all numerical derivative products.
						*/
						if(isnan(res[lmn_indices[i]].iono_atmos))
						{
							/* do nothing --- many models do not implement iono
							 */
						}
						else
						{
							dlmn[i] -= res[lmn_indices[i]].iono_atmos;
						}
                        if(isnan(dlmn[i]))     rv |= 0x100;
                        if(isinf(dlmn[i]))     rv |= 0x200;
    
                    }
                }
                for(i=0; i < SELECT_XYZ_DERIVATIVE_INDICES_MAX; i++)
                {
                    if(xyz_indices[i] >= 0)
                    {
                        dxyz[i] = res[xyz_indices[i]].delay - res[xyz_indices[i]].wet_atmos - res[xyz_indices[i]].dry_atmos - res[xyz_indices[i]].iono_atmos;
						/* TODO: instead of blindly calculating an ionospheric
						   delay correction at 1 GHz, the structure needs to be
						   changed to output a polynomial correction depending
						   on \nu^{-2} for all numerical derivative products.
						*/
						if(isnan(res[xyz_indices[i]].iono_atmos))
						{
							/* do nothing --- many models do not implement iono
							 */
						}
						else
						{
							dxyz[i] -= res[xyz_indices[i]].iono_atmos;
						}
                        if(isnan(dlmn[i]))     rv |= 0x400;
                        if(isinf(dlmn[i]))     rv |= 0x800;
                    }
                }
            }
            break;
        case AberCorrNoAtmos:
            {
                for(i=0; i < SELECT_LMN_DERIVATIVE_INDICES_MAX; i++)
                {
                    if(lmn_indices[i] >= 0)
                    {
                        dlmn[i] = res[lmn_indices[i]].delay;
                        if(isnan(dlmn[i]))     rv |= 0x100;
                        if(isinf(dlmn[i]))     rv |= 0x200;
                    }
                }
                for(i=0; i < SELECT_XYZ_DERIVATIVE_INDICES_MAX; i++)
                {
                    if(xyz_indices[i] >= 0)
                    {
                        dxyz[i] = res[xyz_indices[i]].delay;
                        if(isnan(dlmn[i]))     rv |= 0x400;
                        if(isinf(dlmn[i]))     rv |= 0x800;
                    }
                }
            }
            break;
        default:
            {
                fprintf(stderr, "Developer error: performing numerical derivatives to get UVW|LMN|XYZ but aberCorr is not AberCorrExact or AberCorrNoAtmos!\n");
                rv |= 0x400;
            }
        }
        extractCalcResultsSingleSourceDerivs(source->delta_lmn_used, source->delta_xyz_used, perform_uvw, perform_lmn, perform_xyz, lmn_indices, xyz_indices, dlmn, dxyz, timeIndex, model, modelLMN, modelXYZ, verbose);
    }
    return rv;
}



    

static int extractCalcResults(const DifxScan* const scan, const DifxInput* const D, const CalcParams* const p, const int timeIndex, struct modelTemp** model, struct modelTempLMN** modelLMN, struct modelTempXYZ** modelXYZ, const struct SERVER_MODEL_DELAY_RESPONSE* response, const int verbose)
{
    DifxAntenna* antenna;
    DifxSource* source;
    int antId, k, sourceId;
    unsigned int s_index;
    unsigned int num_source_entries;
    unsigned int rv=0;
    int nError = 0;
    
    for(antId=0; antId < scan->nAntenna; ++antId)
    {
		struct modelTempLMN* modelLMN_1 = (modelLMN == NULL) ? NULL : modelLMN[antId];
		struct modelTempXYZ* modelXYZ_1 = (modelXYZ == NULL) ? NULL : modelXYZ[antId];
        antenna = D->antenna + antId;
        s_index = 0;
        for(k = 0; k < scan->nPhaseCentres +1; ++k)
        {
			struct modelTempLMN* modelLMN_0 = (modelLMN_1 == NULL) ? NULL : modelLMN_1 + k;
			struct modelTempXYZ* modelXYZ_0 = (modelXYZ_1 == NULL) ? NULL : modelXYZ_1 + k;
            if(k == 0) // this is the pointing centre
            {
                sourceId = scan->pointingCentreSrc;
            }
            else
            {
                sourceId = scan->phsCentreSrcs[k-1];
            }
            source = D->source + sourceId;
            rv = extractCalcResultsSingleSource(D, p, antenna, source, antId+1, s_index, timeIndex, &(model[antId][k]), modelLMN_0, modelXYZ_0, response, &num_source_entries, verbose);
            s_index += num_source_entries;
            if((rv))
            {
                if(verbose >= 2)
                {
                    fprintf(stderr, "Error: garbage data from delay server: 0x%X\n", rv);
                }
                ++nError;
            }   
        }
    }
    return nError;
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
    

static double computePolyModel(const DifxScan* const scan, const DifxInput* const D, const unsigned int polyIndex, struct modelTemp **model, struct modelTempLMN** modelLMN, struct modelTempXYZ** modelXYZ, double deltaT, int oversamp, int interpolationType, DifxPolyModel ***im, DifxPolyModelLMNExtension ***imLMN, DifxPolyModelXYZExtension ***imXYZ, const int verbose)
{
    double r;
    const DifxAntenna* antenna;
    const DifxSource* source;
    int antId, k, sourceId;
    unsigned int count = 0;
    double rSum = 0.0;
    int order1 = 0;
    for(antId=0; antId < scan->nAntenna; ++antId)
    {
        if((im[antId]))
        {
            antenna = D->antenna + antId;
            for(k = 0; k < scan->nPhaseCentres + 1; ++k)
            {
                if(k == 0)
                {
                    sourceId = scan->pointingCentreSrc;
                }
                else
                {
                    sourceId = scan->phsCentreSrcs[k-1];
                }
                source = D->source + sourceId;
                if((im[antId][k]))
                {
                    order1 = im[antId][k][polyIndex].order+1;
                    /* FIXME: add interpolation mode */
                    r = computePoly2(im[antId][k][polyIndex].delay,      model[antId][k].delay,            order1, oversamp, deltaT, interpolationType);
                    computePoly2(im[antId][k][polyIndex].dry,            model[antId][k].dry,              order1, oversamp, deltaT, interpolationType);
                    computePoly2(im[antId][k][polyIndex].wet,            model[antId][k].wet,              order1, oversamp, deltaT, interpolationType);
                    computePoly2(im[antId][k][polyIndex].iono,           model[antId][k].iono,             order1, oversamp, deltaT, interpolationType);
                    unwrap_deg_array_for_poly(order1*oversamp, model[antId][k].az);
                    computePoly2(im[antId][k][polyIndex].az,             model[antId][k].az,               order1, oversamp, deltaT, interpolationType);
                    computePoly2(im[antId][k][polyIndex].elcorr,         model[antId][k].elcorr,           order1, oversamp, deltaT, interpolationType);
                    computePoly2(im[antId][k][polyIndex].elgeom,         model[antId][k].elgeom,           order1, oversamp, deltaT, interpolationType);
                    unwrap_deg_array_for_poly(order1*oversamp, model[antId][k].parangle);
                    computePoly2(im[antId][k][polyIndex].parangle,       model[antId][k].parangle,         order1, oversamp, deltaT, interpolationType);
                    unwrap_array_for_poly(order1*oversamp, model[antId][k].msa);
                    computePoly2(im[antId][k][polyIndex].msa,            model[antId][k].msa,              order1, oversamp, deltaT, interpolationType);
                    computePoly2(im[antId][k][polyIndex].sc_gs_delay,    model[antId][k].sc_gs_delay,      order1, oversamp, deltaT, interpolationType);
                    computePoly2(im[antId][k][polyIndex].gs_sc_delay,    model[antId][k].gs_sc_delay,      order1, oversamp, deltaT, interpolationType);
                    computePoly2(im[antId][k][polyIndex].gs_clock_delay, model[antId][k].gs_clock_delay,   order1, oversamp, deltaT, interpolationType);
                    computePoly2(im[antId][k][polyIndex].u,              model[antId][k].u,                order1, oversamp, deltaT, interpolationType);
                    computePoly2(im[antId][k][polyIndex].v,              model[antId][k].v,                order1, oversamp, deltaT, interpolationType);
                    computePoly2(im[antId][k][polyIndex].w,              model[antId][k].w,                order1, oversamp, deltaT, interpolationType);

                    if(verbose > 0 && oversamp > 1)
                    {
                        printf("Scan %f %6d ant='%16s'(%2d) source='%16s'(%2d) poly %d : max delay interpolation error = %E us\n", scan->mjdStart, scan->startSeconds, antenna->name, antId, source->name, k, polyIndex, r);
                    }
                    rSum += r;
                    ++count;
                }
                if((imLMN))
                {
                    if((imLMN[antId]))
                    {
                        if((imLMN[antId][k]))
                        {
                            computePoly2(imLMN[antId][k][polyIndex].dDelay_dl,    modelLMN[antId][k].dDelay_dl,    order1, oversamp, deltaT, interpolationType);
                            computePoly2(imLMN[antId][k][polyIndex].dDelay_dm,    modelLMN[antId][k].dDelay_dm,    order1, oversamp, deltaT, interpolationType);
                            computePoly2(imLMN[antId][k][polyIndex].dDelay_dn,    modelLMN[antId][k].dDelay_dn,    order1, oversamp, deltaT, interpolationType);
                            computePoly2(imLMN[antId][k][polyIndex].d2Delay_dldl, modelLMN[antId][k].d2Delay_dldl, order1, oversamp, deltaT, interpolationType);
                            computePoly2(imLMN[antId][k][polyIndex].d2Delay_dldm, modelLMN[antId][k].d2Delay_dldm, order1, oversamp, deltaT, interpolationType);
                            computePoly2(imLMN[antId][k][polyIndex].d2Delay_dldn, modelLMN[antId][k].d2Delay_dldn, order1, oversamp, deltaT, interpolationType);
                            computePoly2(imLMN[antId][k][polyIndex].d2Delay_dmdm, modelLMN[antId][k].d2Delay_dmdm, order1, oversamp, deltaT, interpolationType);
                            computePoly2(imLMN[antId][k][polyIndex].d2Delay_dmdn, modelLMN[antId][k].d2Delay_dmdn, order1, oversamp, deltaT, interpolationType);
                            computePoly2(imLMN[antId][k][polyIndex].d2Delay_dndn, modelLMN[antId][k].d2Delay_dndn, order1, oversamp, deltaT, interpolationType);
                        }
                    }
                }
                if((imXYZ))
                {
                    if((imXYZ[antId]))
                    {
                        if((imXYZ[antId][k]))
                        {
                            computePoly2(imXYZ[antId][k][polyIndex].dDelay_dX,    modelXYZ[antId][k].dDelay_dX,    order1, oversamp, deltaT, interpolationType);
                            computePoly2(imXYZ[antId][k][polyIndex].dDelay_dY,    modelXYZ[antId][k].dDelay_dY,    order1, oversamp, deltaT, interpolationType);
                            computePoly2(imXYZ[antId][k][polyIndex].dDelay_dZ,    modelXYZ[antId][k].dDelay_dZ,    order1, oversamp, deltaT, interpolationType);
                            computePoly2(imXYZ[antId][k][polyIndex].d2Delay_dXdX, modelXYZ[antId][k].d2Delay_dXdX, order1, oversamp, deltaT, interpolationType);
                            computePoly2(imXYZ[antId][k][polyIndex].d2Delay_dXdY, modelXYZ[antId][k].d2Delay_dXdY, order1, oversamp, deltaT, interpolationType);
                            computePoly2(imXYZ[antId][k][polyIndex].d2Delay_dXdZ, modelXYZ[antId][k].d2Delay_dXdZ, order1, oversamp, deltaT, interpolationType);
                            computePoly2(imXYZ[antId][k][polyIndex].d2Delay_dYdY, modelXYZ[antId][k].d2Delay_dYdY, order1, oversamp, deltaT, interpolationType);
                            computePoly2(imXYZ[antId][k][polyIndex].d2Delay_dYdZ, modelXYZ[antId][k].d2Delay_dYdZ, order1, oversamp, deltaT, interpolationType);
                            computePoly2(imXYZ[antId][k][polyIndex].d2Delay_dZdZ, modelXYZ[antId][k].d2Delay_dZdZ, order1, oversamp, deltaT, interpolationType);
                        }
                    }
                }
            }
        }
    }
    r = ((count))? rSum/count:0.0;
    return r;   // The maximum interpolation error
}





static int adjustSingleSpacecraftAntennaGSRecording(const DifxScan* const scan, DifxInput* const D, CalcParams* p, const DifxAntenna* const antenna, DifxSpacecraft* sc, const int spacecraftId, const struct SERVER_MODEL_DELAY_RESPONSE* const response, int original_date, double original_day_fraction, double* const ret_sc_gs_delay, const int verbose)
{
    struct SERVER_MODEL_DELAY_ARGUMENT* sc_argument(&(p->sc_argument));
	struct SERVER_MODEL_DELAY_RESPONSE* sc_response(&(p->sc_response));
    uint16_t station_ID;
    int r;
    int_fast32_t r32;
    int offset;
    double gs_delay = 0.0;             /* ground station to center of Earth
                                          delay in seconds */
    double sc_delay = 0.0;             /* spacecraft to center of Earth
                                          delay in seconds */
    double sc_gs_delay;                /* difference betweensc and gs delays
                                          in seconds */
    double last_sc_gs_delay = -DBL_MAX;/* in seconds */
    double RA, Dec, radius;
    double dpos[DIFXCALC_3D_VEC_SIZE];
    int retarded_date = 0;
    double retarded_day_fraction = 0.0;
    int loop_count;
    static const int MAX_LOOP_COUNT = 10;

    /* verify proper time type */
    if(sc->spacecraft_time_type != SpacecraftTimeGroundReception)
    {
        fprintf(stderr, "Error: bad SpacecraftTimeType in adjustSingleSpacecraftAntennaGSRecording --- type %s should not have this function called\n", spacecraftTimeTypeNames[sc->spacecraft_time_type]);
        return -1;
    }

    sc_gs_delay = *ret_sc_gs_delay;

    {
        /* Get the ground station clock offset at the sync date/time */
        int i;
        double x = 1.0;
        double Delta_t = (original_date + original_day_fraction) - sc->GS_clockrefmjd;
        double gs_clock_offset = 0.0;
        Delta_t *= SEC_DAY_DBL;
        for(i=0; i <= sc->GS_clockorder; i++) {
            gs_clock_offset += x*sc->GS_clockcoeff[i];
            x *= Delta_t;
        }
        gs_clock_offset *= MICROSECONDS_PER_SECOND; /* convert from \mu s to s */
        gs_clock_offset += sc->GS_clock_break_fudge_sec;
        /* correct the indicated station time to the true TT time */
        original_day_fraction += gs_clock_offset / SEC_DAY_DBL;
        offset = (int)(floor(original_day_fraction));
        original_date += offset;
        original_day_fraction -= offset;

        /* store the ground station clock offset at the sync time */
        sc->GS_clock_delay_sync = gs_clock_offset;
    }


    if(verbose >= 3)
    {
        fprintf(stderr, "CALCIF3_DEBUG: Determining GS_recording_delay for spacecraft '%s' (%d) at %d %.6f\n", sc->name, spacecraftId, original_date, original_day_fraction);
    }
    sc_argument->date = original_date;
    sc_argument->time = original_day_fraction;
    {
	    DiFX::Delay::Time::DelayTimestamp t(sc_argument->date,sc_argument->time,DiFX::Delay::Time::DIFX_TIME_SYSTEM_MJD);
	    sc_argument->utc_second = t.i();
	    sc_argument->utc_second_fraction = t.f();
	    DiFX::Delay::Time::DelayTimestamp t_TAI = t.UTC_to_TAI();
	    sc_argument->tai_second = t_TAI.i();
	    sc_argument->tai_second_fraction = t_TAI.f();
    }

    /* Antenna 0 is the Earth Center (needed for some delay servers) */
    /* Antenna 0 information has already been filled in */

    /* Antenna 1 is the spacecraft */
    /* TODO: replace GS information with the spacecraft struct with
       an actual station that gets carried around by difx_input stuff.
    */
    difx_strlcpy(sc_argument->station[1].station_name, antenna->calcname, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    difx_strlcpy(sc_argument->station[1].antenna_name, antenna->calcname, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    difx_strlcpy(sc_argument->station[1].site_name, antenna->calcname, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    station_ID = (uint16_t)(antenna->calcname[0]) | ((uint16_t)(antenna->calcname[1]) << 8);
    sc_argument->station[1].site_ID = station_ID;
    difx_strlcpy(sc_argument->station[1].site_type, antennaSiteTypeNames[antenna->sitetype], DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    difx_strlcpy(sc_argument->station[1].axis_type, antennaMountTypeNames[antenna->mount], DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);

    /* Get the spacecraft position at the true ground reception time time */
    r = calcSpacecraftAntennaPosition(D, sc_argument, spacecraftId, 1, 0.0);
    if(r < 0)
    {
        printf("Error: %s:%d:adjustSingleSpacecraftAntennaGSRecording: calcSpacecraftAntennaPosition = %d\n", __FILE__, __LINE__, r);
        return -2;
    }
    sc_argument->station[1].station_coord_frame = int32_t(sc->position_coord_frame);
    sc_argument->station[1].pointing_coord_frame = int32_t(sc->pointing_coord_frame);
    sc_argument->station[1].pointing_corrections_applied = 1;

    sc_argument->station[1].station_position_delay_offset = 0.0;
    sc_argument->station[1].axis_off = antenna->offset[0];
    /* TODO: get wrap, pressure, temperature information from
       VEX/log files?
    */
    sc_argument->station[1].primary_axis_wrap = 0;
    sc_argument->station[1].secondary_axis_wrap = 0;
    sc_argument->station[1].receiver_name[0] = 0;
    sc_argument->station[1].pressure = 0.0;
    sc_argument->station[1].antenna_pressure = 0.0;
    sc_argument->station[1].temperature = 0.0;
    sc_argument->station[1].wind_speed = fitsnan.d;
    sc_argument->station[1].wind_direction = fitsnan.d;
    sc_argument->station[1].antenna_phys_temperature = 0.0;

    /* Antenna 2 is the ground station */
    /* TODO: replace GS information with the spacecraft struct with
       an actual station that gets carried around by difx_input stuff.
    */
    if(!sc->GS_exists)
    {
        fprintf(stderr, "Error: in adjustSingleSpacecraftAntennaGSRecording, spacecraft (id=%d name='%s') has no ground station!\n", spacecraftId, sc->name);
        return -3;
    }
    difx_strlcpy(sc_argument->station[2].station_name, sc->GS_calcName, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    difx_strlcpy(sc_argument->station[2].antenna_name, sc->GS_calcName, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    difx_strlcpy(sc_argument->station[2].site_name, sc->GS_calcName, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    station_ID = (uint16_t)(sc->GS_calcName[0]) | ((uint16_t)(sc->GS_calcName[1]) << 8);
    sc_argument->station[2].site_ID = station_ID;
    difx_strlcpy(sc_argument->station[2].site_type, antennaSiteTypeNames[AntennaSiteFixed], DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    difx_strlcpy(sc_argument->station[2].axis_type, antennaMountTypeNames[sc->GS_mount], DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);


    sc_argument->station[2].station_pos[0] = sc->GS_X;
    sc_argument->station[2].station_pos[1] = sc->GS_Y;
    sc_argument->station[2].station_pos[2] = sc->GS_Z;
    /* for ground-based antennas, ground drift already*/
    /* taken into account, so set the station */
    /* velocities to 0 */
    sc_argument->station[2].station_vel[0] =  0.0;
    sc_argument->station[2].station_vel[1] =  0.0;
    sc_argument->station[2].station_vel[2] =  0.0;
    /* for ground-based antennas, acceleration */
    /* calculated from Earth rotation, so set the */
    /* accelerations to 0 */
    sc_argument->station[2].station_acc[0] =  0.0;
    sc_argument->station[2].station_acc[1] =  0.0;
    sc_argument->station[2].station_acc[2] =  0.0;
    /* the pointing directions will be updated below.  For now just
       blank the pointing */
    sc_argument->station[2].station_pointing_dir[0] = 0.0;
    sc_argument->station[2].station_pointing_dir[1] = 0.0;
    sc_argument->station[2].station_pointing_dir[2] = 0.0;

    sc_argument->station[2].station_reference_dir[0] =  0.0;
    sc_argument->station[2].station_reference_dir[1] =  0.0;
    sc_argument->station[2].station_reference_dir[2] =  0.0;

    /* TODO: implement ground station coordinate frame */
    sc_argument->station[2].station_coord_frame = int32_t(SourceCoordinateFrameITRF2008);
    /* TODO: implement spacecraft source --- not the spacecraft
       antenna itself --- as the source here */
    sc_argument->station[0].pointing_coord_frame = int32_t(sc->position_coord_frame);
    sc_argument->station[2].pointing_coord_frame = int32_t(sc->position_coord_frame);
    sc_argument->station[0].pointing_corrections_applied = 2;
    sc_argument->station[2].pointing_corrections_applied = 2;

    sc_argument->station[2].station_position_delay_offset = 0.0;
    sc_argument->station[2].axis_off = sc->GS_offset[0];
    /* TODO: get wrap, pressure, temperature information from
       VEX/log files?
    */
    sc_argument->station[2].primary_axis_wrap = 0;
    sc_argument->station[2].secondary_axis_wrap = 0;
    sc_argument->station[2].receiver_name[0] = 0;
    sc_argument->station[2].pressure = 0.0;
    sc_argument->station[2].antenna_pressure = 0.0;
    sc_argument->station[2].temperature = 0.0;
    sc_argument->station[2].wind_speed = fitsnan.d;
    sc_argument->station[2].wind_direction = fitsnan.d;
    sc_argument->station[2].antenna_phys_temperature = 0.0;


    /* Setup source information */
    /* TODO: get source as spacecraft source instead of spacecraft
       antenna */
    difx_strlcpy(sc_argument->source[0].source_name, sc->name, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    sc_argument->source[0].IAU_name[0] = 0;
    difx_strlcpy(sc_argument->source[0].source_type, "ephemeris", DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    sc_argument->source[0].coord_frame = int32_t(sc->position_coord_frame);
    /* get the spacecraft position at the data/time origin offset by sc_gs_delay
       seconds of offset, and put it into the pointing direction of
       station 0 (Earth center)
    */
    retarded_date = original_date;
    retarded_day_fraction = original_day_fraction - sc_gs_delay/SEC_DAY_DBL;
    offset = (int)(floor(retarded_day_fraction));
    retarded_date += offset;
    retarded_day_fraction -= offset;
    sc_argument->date = retarded_date;
    sc_argument->time = retarded_day_fraction;
    {
	    DiFX::Delay::Time::DelayTimestamp t(sc_argument->date,sc_argument->time,DiFX::Delay::Time::DIFX_TIME_SYSTEM_MJD);
	    sc_argument->utc_second = t.i();
	    sc_argument->utc_second_fraction = t.f();
	    DiFX::Delay::Time::DelayTimestamp t_TAI = t.UTC_to_TAI();
	    sc_argument->tai_second = t_TAI.i();
	    sc_argument->tai_second_fraction = t_TAI.f();
    }
    r = calcSpacecraftAsPointingDirection(D, sc, /* epoch */ 0.0, sc_argument, spacecraftId, /* station */ 0, /* do own retardation */ 1, /* delay offset */ sc_gs_delay);
    if(r < 0)
    {
        printf("Error: %s:%d:adjustSingleSpacecraftAntennaGSRecording: calcSpacecraftAsPointingDirection = %d\n", __FILE__, __LINE__, r);
        return -4;
    }
    /* copy pointing direction to the ground station */
    dpos[0] = sc_argument->station[0].station_pointing_dir[0];
    dpos[1] = sc_argument->station[0].station_pointing_dir[1];
    dpos[2] = sc_argument->station[0].station_pointing_dir[2];
    sc_argument->station[2].station_pointing_dir[0] = dpos[0];
    sc_argument->station[2].station_pointing_dir[1] = dpos[1];
    sc_argument->station[2].station_pointing_dir[2] = dpos[2];
    difx_Cartesian_to_RADec(dpos, &RA, &Dec, &radius);
    if(radius == 0.0)
    {
        radius = 1.0;
    }
    sc_argument->source[0].ra  =  RA;
    sc_argument->source[0].dec =  Dec;
    sc_argument->source[0].parallax = 0.0;
    sc_argument->source[0].source_pos[0] = dpos[0] / radius * 1E30;
    sc_argument->source[0].source_pos[1] = dpos[1] / radius * 1E30;
    sc_argument->source[0].source_pos[2] = dpos[2] / radius * 1E30;

    /* TODO: put in calculations to find the source_pointing_dir and
       source_pointing_reference_dir
    */
    sc_argument->source[0].source_pointing_dir[0] = 0.0;
    sc_argument->source[0].source_pointing_dir[1] = 0.0;
    sc_argument->source[0].source_pointing_dir[2] = 0.0;
    sc_argument->source[0].source_pointing_reference_dir[0] = 0.0;
    sc_argument->source[0].source_pointing_reference_dir[1] = 0.0;
    sc_argument->source[0].source_pointing_reference_dir[2] = 0.0;
            
    sc_argument->source[0].source_vel[0] = 0.0;
    sc_argument->source[0].source_vel[1] = 0.0;
    sc_argument->source[0].source_vel[2] = 0.0;
    sc_argument->source[0].source_acc[0] = 0.0;
    sc_argument->source[0].source_acc[1] = 0.0;
    sc_argument->source[0].source_acc[2] = 0.0;
    sc_argument->source[0].dra  = 0.0;
    sc_argument->source[0].ddec = 0.0;
    sc_argument->source[0].depoch = original_date + original_day_fraction;
    /* reset time to original time */
    sc_argument->date = original_date;
    sc_argument->time = original_day_fraction;
    {
	    DiFX::Delay::Time::DelayTimestamp t(sc_argument->date,sc_argument->time,DiFX::Delay::Time::DIFX_TIME_SYSTEM_MJD);
	    sc_argument->utc_second = t.i();
	    sc_argument->utc_second_fraction = t.f();
	    DiFX::Delay::Time::DelayTimestamp t_TAI = t.UTC_to_TAI();
	    sc_argument->tai_second = t_TAI.i();
	    sc_argument->tai_second_fraction = t_TAI.f();
    }
    sc_argument->request_id = p->argument_id++;

    r32 = p->delayDistributor->process_delay_service(D, D->job, scan, sc_argument, sc_response, int_fast32_t(verbose));
    if((r32))
    {
        fprintf(stderr, "Error: adjustSingleSpacecraftAntennaGSRecording: fatal error %"PRIdFAST32" from process_delay_service\n", r32);
        return -5;
    }
    
    for(loop_count = 0; loop_count < MAX_LOOP_COUNT; loop_count++)
    {
        /* get the predicted delays */
        sc_delay = sc_response->result[1].delay;
        gs_delay = sc_response->result[2].delay;
		/* The delay server provides time relative to station 0, with
		   negative delays meaning arrving *before* station zero.  We want the
		   positive delay time of flight from the spacecraft to the station.
		 */
        sc_gs_delay = gs_delay - sc_delay;
        if(verbose >= 3)
        {
            fprintf(stderr, "CALCIF3_DEBUG: have loop_count=%d delays [s] sc=%E gs=%E sc_gs=%E\n", loop_count, sc_delay, gs_delay, sc_gs_delay);
            fprintf(stderr, "CALCIF3_DEBUG: for loop %d sc_gs_delay=%E last_sc_gs_delay=%E\n", loop_count, sc_gs_delay, last_sc_gs_delay);
            fprintf(stderr, "CALCIF3_DEBUG: for loop %d change from last iteration=%E, relative change=%E\n", loop_count, sc_gs_delay - last_sc_gs_delay, (sc_gs_delay - last_sc_gs_delay)/last_sc_gs_delay);
        }
        if(fabs(sc_gs_delay - last_sc_gs_delay) < D->job->delayModelPrecision)
        {
            /* The delay value has changed by less than
               D->job.delayModelPrecision, which defaults to 0.1 ps,
               which is just 0.03 mm of path difference, or
               33 times smaller than the wavelength at 300 GHz,
               and so this should be good enough for radio interferometry.
            */
            break;
        }
        last_sc_gs_delay = sc_gs_delay;
        /* get the spacecraft position at the instant of
           transmission, at the retarded time corresponding to the
           true recording time at the ground station.
        */
        retarded_date = original_date;
        retarded_day_fraction = original_day_fraction + gs_delay/SEC_DAY_DBL;
        offset = (int)(floor(retarded_day_fraction));
        retarded_date += offset;
        retarded_day_fraction -= offset;
        sc_argument->date = retarded_date;
        sc_argument->time = retarded_day_fraction;
        {
	        DiFX::Delay::Time::DelayTimestamp t(sc_argument->date,sc_argument->time,DiFX::Delay::Time::DIFX_TIME_SYSTEM_MJD);
	        sc_argument->utc_second = t.i();
	        sc_argument->utc_second_fraction = t.f();
	        DiFX::Delay::Time::DelayTimestamp t_TAI = t.UTC_to_TAI();
	        sc_argument->tai_second = t_TAI.i();
	        sc_argument->tai_second_fraction = t_TAI.f();
        }

        r = calcSpacecraftAntennaPosition(D, sc_argument, spacecraftId, 1, sc_delay);
        if(r < 0)
        {
            printf("Error: adjustSingleSpacecraftAntennaGSRecording: Antenna spacecraft %d table out of time range, or not yet supported\n", spacecraftId);
            
            return -6;
        }
        /* Get the pointing direction. */
        if((sc_argument->station[1].station_coord_frame == SourceCoordinateFrameJ2000_Earth)
          && (sc_argument->station[2].station_coord_frame == SourceCoordinateFrameITRF2008))
        {
            /* use the returned baseline positions to get the coordinates of
               the ground station in the J2000 frame */
            double* sc_pos = sc_argument->station[1].station_pos;
            double* bP = sc_response->result[2].baselineP2000;
            double* bV = sc_response->result[2].baselineV2000;
            if(verbose >= 3)
            {
                fprintf(stderr, "CALCIF3_DEBUG: Have Spacecraft     Ground  station positions [m] (J2000 and ITRF2008) at\n");
                fprintf(stderr, "CALCIF3_DEBUG: X  %15.3f %15.3f\n", sc_pos[0], -bP[0]);
                fprintf(stderr, "CALCIF3_DEBUG: Y  %15.3f %15.3f\n", sc_pos[1], -bP[1]);
                fprintf(stderr, "CALCIF3_DEBUG: Z  %15.3f %15.3f\n", sc_pos[2], -bP[2]);
                fprintf(stderr, "CALCIF3_DEBUG: Have Ground  stations velocity [m/s]\n");
                fprintf(stderr, "CALCIF3_DEBUG: X %15.9f\n", -bV[0]);
                fprintf(stderr, "CALCIF3_DEBUG: Y %15.9f\n", -bV[1]);
                fprintf(stderr, "CALCIF3_DEBUG: Z %15.9f\n", -bV[2]);
            }
            dpos[0] = sc_pos[0] + bP[0];
            dpos[1] = sc_pos[1] + bP[1];
            dpos[2] = sc_pos[2] + bP[2];
            difx_Cartesian_to_RADec(dpos, &RA, &Dec, &radius);
            if(radius == 0.0)
            {
                radius = 1.0;
            }
            sc_argument->source[0].ra  =  RA;
            sc_argument->source[0].dec =  Dec;
            sc_argument->source[0].parallax = 0.0;
            for(uint_fast8_t k=0; k < 3; ++k)
            {
	            double d = dpos[k] / radius * 1E30;
	            sc_argument->source[0].source_pos[k] = d;
	            sc_argument->station[0].station_pointing_dir[k] = d;
	            sc_argument->station[2].station_pointing_dir[k] = d;
            }
            /* TODO:  Is the aberration correction below actually needed? */
            /* /\* the spacecraft velocity and acceleration is fudged in order */
            /*    that the abberation is the same as seen by the */
            /*    ground station */
            /* *\/ */
            /* sc_argument.b_dx = -gs_results.res[0].getCALC_res_u.record.baselineV2000[0]; */
            /* sc_argument.b_dy = -gs_results.res[0].getCALC_res_u.record.baselineV2000[1]; */
            /* sc_argument.b_dz = -gs_results.res[0].getCALC_res_u.record.baselineV2000[2]; */
            /* sc_argument.b_ddx = -gs_results.res[0].getCALC_res_u.record.baselineA2000[0]; */
            /* sc_argument.b_ddy = -gs_results.res[0].getCALC_res_u.record.baselineA2000[1]; */
            /* sc_argument.b_ddz = -gs_results.res[0].getCALC_res_u.record.baselineA2000[2]; */
        }
        else if(sc_argument->station[1].station_coord_frame == sc_argument->station[2].station_coord_frame)
        {
            /* Calculate directly in the specified frame */
            double* sc_pos = sc_argument->station[1].station_pos;
            double* gs_pos = sc_argument->station[2].station_pos;
            if(verbose >= 3)
            {
                fprintf(stderr, "CALCIF3_DEBUG: Have Spacecraft     Ground  station positions [m] (same coordinate frame) at\n");
                fprintf(stderr, "CALCIF3_DEBUG: X  %15.3f %15.3f\n", sc_pos[0], gs_pos[0]);
                fprintf(stderr, "CALCIF3_DEBUG: Y  %15.3f %15.3f\n", sc_pos[1], gs_pos[1]);
                fprintf(stderr, "CALCIF3_DEBUG: Z  %15.3f %15.3f\n", sc_pos[2], gs_pos[2]);
            }
            dpos[0] = sc_pos[0] - gs_pos[0];
            dpos[1] = sc_pos[1] - gs_pos[1];
            dpos[2] = sc_pos[2] - gs_pos[2];
            difx_Cartesian_to_RADec(dpos, &RA, &Dec, &radius);
            if(radius == 0.0)
            {
                radius = 1.0;
            }
            sc_argument->source[0].ra  =  RA;
            sc_argument->source[0].dec =  Dec;
            sc_argument->source[0].parallax = 0.0;
            for(uint_fast8_t k=0; k < 3; ++k)
            {
	            double d = dpos[k] / radius * 1E30;
	            sc_argument->source[0].source_pos[k] = d;
	            sc_argument->station[0].station_pointing_dir[k] = d;
	            sc_argument->station[2].station_pointing_dir[k] = d;
            }
        }
        else
        {
            fprintf(stderr, "Error: this software does not currently support the requested spacecraft operations for spacecraft coordinate frame='%s' and ground station coordinate frame='%s'.  Please contact your developer.\n", sourceCoordinateFrameTypeNames[sc_argument->station[1].station_coord_frame], sourceCoordinateFrameTypeNames[sc_argument->station[2].station_coord_frame]);
            return -7;
        }
        for(uint_fast8_t k=0; k < 3; ++k)
        {
	        sc_argument->source[0].source_vel[k] = 0.0;
	        sc_argument->source[0].source_acc[k] = 0.0;
	        sc_argument->station[1].station_vel[k] =  0.0;
	        sc_argument->station[1].station_acc[0] =  0.0;
        }

        r32 = p->delayDistributor->process_delay_service(D, D->job, scan, sc_argument, sc_response, int_fast32_t(verbose));
        if((r32))
        {
	        fprintf(stderr, "Error: adjustSingleSpacecraftAntennaGSRecording: fatal error %"PRIdFAST32" from process_delay_service\n", r32);
	        return -8;
        }
    }
    if(loop_count == MAX_LOOP_COUNT)
    {
        fprintf(stderr, "Error: adjustSingleSpacecraftAntennaGSRecording: ground station delay not converging\n");
        return -9;
    }

    sc->GS_recording_delay = sc_gs_delay;
    return 0;
}


static int adjustSingleSpacecraftAntennaFrameTime(DifxSpacecraft* const sc, const int original_date, const double original_day_fraction, const double sc_delay, double* const frame_time_offset, const int verbose)
{
    /* sc_delay [s] is the delay (positive means before) with respect to
       original_date/original_day_fraction at which the spacecraft receives a
       signal.  sc_delay should be provided by the delay server, for each
       source direction, and for each source direction this function
       called separately.

       *frame_time_offset [s] is the offset time that should be
       *added* to the sc_delay value for this source direction.
    */
    
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
    switch(sc->spacecraft_time_type) {
    case SpacecraftTimeLocal:
    case SpacecraftTimeGroundReception:
        {
            int retarded_date = original_date;
            double retarded_day_fraction;
            int offset;
            spacecraftTimeFrameOffset Toffset;
            int r;
            retarded_day_fraction = original_day_fraction - sc_delay/SEC_DAY_DBL;
            offset = (int)(floor(retarded_day_fraction));
            retarded_date += offset;
            retarded_day_fraction -= offset;
 
            r = evaluateDifxSpacecraftAntennaTimeFrameOffset(sc,
                                                             retarded_date,
                                                             retarded_day_fraction,
                                                            &Toffset);

            if(verbose >= 3)
            {
                fprintf(stderr, "CALCIF3_DEBUG: at %d %f adjustSingleSpacecraftAntennaFrameTime got evaluateDifxSpacecraftAntennaTimeFrameOffset result %d %E s, original delay is %E s\n", original_date, original_day_fraction, r, Toffset.Delta_t, sc_delay);
            }
            if(r < 0)
            {
                fprintf(stderr, "Error: recieved code %d from evaluateDifxSpacecraftAntennaTimeFrameOffset in adjustSingleSpacecraftAntennaFrameTime\n", r);
                return -1;
            }
            *frame_time_offset = Toffset.Delta_t;
        }
        return 0;
    case SpacecraftTimeGroundClock:
    case SpacecraftTimeGroundClockReception:
        *frame_time_offset = 0.0;
        return 0;
    default:
        {
            /* unknown time type */
            fprintf(stderr, "Error: unknown time type (position 2)\n");
            return -2;
        }
    }
    return -3;
}







static int adjustSingleSpacecraftAntennaCalcResults(const DifxScan* const scan, DifxInput* const D, CalcParams* const p, const DifxAntenna* const antenna, DifxSpacecraft* const sc, const int spacecraftId, const struct SERVER_MODEL_DELAY_ARGUMENT* const argument, const struct SERVER_MODEL_DELAY_RESPONSE* const response, const int verbose)
{
    int r;
    int original_date;
    double original_day_fraction;

    if((!sc->is_antenna))
    {
        fprintf(stderr, "Error: spacecraft is not antenna in adjustSingleSpacecraftAntennaCalcResults\n");
        return -1;
    }

    if(sc->spacecraft_time_type == SpacecraftTimeLocal)
    {
        if(!isnan(sc->GS_recording_delay))
        {
            /* do nothing */
        }
        else
        {
            sc->GS_recording_delay = 0.0;
            sc->GS_transmission_delay = 0.0;
            sc->GS_transmission_delay_sync = 0.0;
            sc->SC_elec_delay = sc->SC_recording_delay;
            sc->GS_clock_delay = 0.0;
            sc->GS_clock_delay_sync = 0.0;
        }
        return 0;
    }
    else if(sc->spacecraft_time_type == SpacecraftTimeGroundReception)
    {
        if(!isnan(sc->GS_recording_delay))
        {
            /* constant delay offset already calculated, so use it */
            if(verbose >= 3)
            {
                fprintf(stderr, "CALCIF3_DEBUG: SpacecraftTimeGroundReception using exisiting GS_recording_delay=%E s, GS_clock_delay_sync=%E s\n", sc->GS_recording_delay, sc->GS_clock_delay_sync);
            }
        }
        else {
	        sc->GS_recording_delay = 0.0; /* initialize to a sane value */
            sc->GS_transmission_delay = 0.0;
            sc->GS_transmission_delay_sync = 0.0;
            sc->SC_elec_delay = sc->SC_recording_delay + sc->SC_Elec_to_Comm;
            sc->GS_clock_delay = 0.0;
            sc->GS_clock_delay_sync = 0.0;
            original_date = sc->GS_mjd_sync;
            original_day_fraction = sc->GS_dayfraction_sync;
            /* This sets GS_recording_delay and GS_clock_delay_sync
               within the spacecraft struct
            */
            r = adjustSingleSpacecraftAntennaGSRecording(scan, D, p, antenna, sc, spacecraftId, response, original_date, original_day_fraction, &(sc->GS_recording_delay), verbose);
            if(r < 0)
            {
                fprintf(stderr, "Error: adjustSingleSpacecraftAntennaGSRecording returned %d\n", r);
                return -2;
            }
            if(verbose >= 3)
            {
                fprintf(stderr, "CALCIF3_DEBUG: SpacecraftTimeGroundReception using new GS_recording_delay=%E s, GS_clock_delay_sync=%E s\n", sc->GS_recording_delay, sc->GS_clock_delay_sync);
            }
        }
        return 0;
    }
    else if(sc->spacecraft_time_type == SpacecraftTimeGroundClock)
    {
        /* How do I deal with the spacecraft clock running off of the ground
           maser, correcting for all special and general relativistic effects
           for clock transfer? */
        if(!isnan(sc->GS_transmission_delay))
        {
            /* initialize using last delay offset calculated */
            if(verbose >= 3)
            {
                fprintf(stderr, "CALCIF3_DEBUG: SpacecraftTimeGroundClock using exisiting GS_transmission_delay=%E s as starting value for iteration\n", sc->GS_transmission_delay);
            }
        }
        else
        {
            sc->GS_recording_delay = 0.0;
            sc->GS_transmission_delay = 0.0; /* initialize to a sane value */
            sc->GS_transmission_delay_sync = 0.0;
            sc->SC_elec_delay = sc->SC_recording_delay; /* ??? */
            sc->GS_clock_delay = 0.0;
            sc->GS_clock_delay_sync = 0.0;
        }
        fprintf(stderr, "Error: SpacecraftTimeGroundClock not yet implemented\n");
        exit(EXIT_FAILURE);
    }
    else if(sc->spacecraft_time_type == SpacecraftTimeGroundClockReception)
    {
        if(!isnan(sc->GS_recording_delay))
        {
            /* constant delay offset already calculated, so use it */
            if(verbose >= 3)
            {
                fprintf(stderr, "CALCIF3_DEBUG: SpacecraftTimeGroundClockReception using exisiting GS_recording_delay=%E s to start iteration\n", sc->GS_recording_delay);
            }
        }
        else {
	        sc->GS_recording_delay = 0.0; /* initialize to a sane value */
            sc->GS_transmission_delay = 0.0; /* initialize to a sane value */
            sc->GS_transmission_delay_sync = 0.0;
            sc->SC_elec_delay = sc->SC_recording_delay; /* ??? */
            sc->GS_clock_delay = 0.0;
            sc->GS_clock_delay_sync = 0.0;
            /* now calculate the delay at the start of reception */
            fprintf(stderr, "Error: SpacecraftTimeGroundClockReception not yet implemented\n");
            exit(EXIT_FAILURE);
            
        }
        original_date = sc->GS_mjd_sync;
        original_day_fraction = sc->GS_dayfraction_sync;
        /* processing handled below */
        fprintf(stderr, "Error: SpacecraftTimeGroundClockReception not yet implemented\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        /* unknown time type */
        fprintf(stderr, "Error: unknown time type\n");
        return -3;
    }
    return -4;
}








static int adjustSpacecraftAntennaCalcResults(const DifxScan* const scan, DifxInput* const D, CalcParams* const p, const struct SERVER_MODEL_DELAY_ARGUMENT* const argument, struct SERVER_MODEL_DELAY_RESPONSE* const response, const int verbose)
{
    DifxAntenna* antenna;
    DifxSpacecraft *sc;
    int antId;
    int spacecraftId;
    int r;
    unsigned int a_index;
    unsigned int l;
    double sc_extra_delay;       /* in seconds */
    double sc_time_frame_delay;  /* in seconds */

    for(antId=0; antId < scan->nAntenna; ++antId)
    {
        antenna = D->antenna + antId;
        spacecraftId = antenna->spacecraftId;
        if(spacecraftId < 0)
        {
            continue;
        }
        sc = D->spacecraft + spacecraftId;

        r = adjustSingleSpacecraftAntennaCalcResults(scan, D, p, antenna, sc, spacecraftId, argument, response, verbose);
        if(r < 0)
        {
            fprintf(stderr, "Error: received %d from adjustSingleSpacecraftAntennaCalcResults\n", r);
            return -1;
        }
        if(verbose >= 3)
        {
            fprintf(stderr, "CALCIF3_DEBUG: Have GS_recording_delay=%E s, GS_transmission_delay=%E s, GS_transmission_delay_sync=%E s, SC_elec_delay=%E s, GS_clock_delay=%E s, GS_clock_delay_sync=%E s s\n", sc->GS_recording_delay, sc->GS_transmission_delay, sc->GS_transmission_delay_sync, sc->SC_elec_delay, sc->GS_clock_delay, sc->GS_clock_delay_sync);
        }
        /* correct the delay to account for the extra propagation delay
           paths in the spacecraft and the ground station to spacecraft
           and spacecraft to ground station links.

           The *spacecraft* clock delay will be added in later
           in the actual correlator section, so do not add it in here, but *do*
           add in the ground station clock offset if necessary.
        */
        sc_extra_delay = sc->GS_recording_delay + sc->GS_transmission_delay + sc->GS_transmission_delay_sync + sc->SC_elec_delay - sc->GS_clock_delay - sc->GS_clock_delay_sync;

        /* FORMER CODE */
        /* /\* add sc_extra_delay to the delay value in the response area *\/ */
        
        /* mod->delay[index] -= (sc_gs_delay - gs_clock_offset + sc->SC_recording_delay)*1E6; */
        /* mod->sc_gs_delay[index] = sc_gs_delay*1E+6; */
        /* mod->gs_clock_delay[index] = -gs_clock_offset*1E+6; */


        a_index = (antId+1)*response->Num_Sources;
        for(l=0; l < response->Num_Sources; l++)
        {
            r = adjustSingleSpacecraftAntennaFrameTime(sc, argument->date, argument->time, -response->result[a_index+l].delay, &sc_time_frame_delay, verbose);
            if(r < 0)
            {
                fprintf(stderr, "Error: adjustSingleSpacecraftAntennaFrameTime returned %d\n", r);
                return -2;
            }
            response->result[a_index+l].delay += sc_extra_delay - sc_time_frame_delay;
        }
    }
    return 0;
}








/* Set up individual source delay derivative source information */
static unsigned int scanCalcSetupSingleSourceDerivs(unsigned int s_index, struct SERVER_MODEL_DELAY_ARGUMENT* const argument, const DifxJob* const job, DifxSource* const source, const int verbose)
{
    int spacecraftId = -1;
    enum PerformDirectionDerivativeType perform_uvw;
    enum PerformDirectionDerivativeType perform_lmn;
    enum PerformDirectionDerivativeType perform_xyz;
    const int* lmn_indices;
    const int* xyz_indices;
    unsigned int num_lmn;
    unsigned int num_xyz;
    unsigned int i;
    /* source direction, velocity, and reference direction */

        
    perform_uvw = (source->perform_uvw_deriv == PerformDirectionDerivativeDefault) ? job->perform_uvw_deriv : source->perform_uvw_deriv;
    perform_lmn = (source->perform_lmn_deriv == PerformDirectionDerivativeDefault) ? job->perform_lmn_deriv : source->perform_lmn_deriv;
    perform_xyz = (source->perform_xyz_deriv == PerformDirectionDerivativeDefault) ? job->perform_xyz_deriv : source->perform_xyz_deriv;

    num_lmn = select_lmn_derivative_indices(perform_uvw, perform_lmn, &lmn_indices);
    num_xyz = select_xyz_derivative_indices(perform_xyz, &xyz_indices);
    spacecraftId = source->spacecraftId;

    /* copy the source position into pos */
    

    /* Now handle all of the possible offset positions */
    if(num_lmn > 1)
    {
        double R[DIFXCALC_3D_MAT_SIZE];
        double pos[DIFXCALC_3D_VEC_SIZE];
        double npos[DIFXCALC_3D_VEC_SIZE];
        double theta = (source->delta_lmn == 0.0) ? job->delta_lmn : source->delta_lmn;
        double r_factor;
        source->delta_lmn_used = theta;
        pos[0] = argument->source[s_index].source_pos[0];
        pos[1] = argument->source[s_index].source_pos[1];
        pos[2] = argument->source[s_index].source_pos[2];
        /* Remember, l goes in the direction of decreasing RA */
        /* Individual indices */
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp]] = argument->source[s_index];
            difxcalc_R_Rotate_RA(pos, -theta, R);
            difxcalc_multiply_R_v(R, pos, npos);
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp]].source_pos[0] = npos[0];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp]].source_pos[1] = npos[1];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp]].source_pos[2] = npos[2];
            
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm]] = argument->source[s_index];
            difxcalc_R_Rotate_RA(pos, +theta, R);
            difxcalc_multiply_R_v(R, pos, npos);
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm]].source_pos[0] = npos[0];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm]].source_pos[1] = npos[1];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm]].source_pos[2] = npos[2];
            
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]] = argument->source[s_index];
            difxcalc_R_Rotate_Dec(pos, theta, R);
            difxcalc_multiply_R_v(R, pos, npos);
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]].source_pos[0] = npos[0];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]].source_pos[1] = npos[1];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]].source_pos[2] = npos[2];
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]] = argument->source[s_index];
            difxcalc_R_Rotate_Dec(pos, -theta, R);
            difxcalc_multiply_R_v(R, pos, npos);
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]].source_pos[0] = npos[0];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]].source_pos[1] = npos[1];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]].source_pos[2] = npos[2];
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Np] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Np]] = argument->source[s_index];
            r_factor = 1.0 + theta * DIFXIO_DELTA_LMN_N_FACTOR;
            npos[0] = pos[0] * r_factor;
            npos[1] = pos[1] * r_factor;
            npos[2] = pos[2] * r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]].source_pos[0] = npos[0];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]].source_pos[1] = npos[1];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]].source_pos[2] = npos[2];
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Nm] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Nm]] = argument->source[s_index];
            r_factor = 1.0 - theta * DIFXIO_DELTA_LMN_N_FACTOR;
            npos[0] = pos[0] * r_factor;
            npos[1] = pos[1] * r_factor;
            npos[2] = pos[2] * r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]].source_pos[0] = npos[0];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]].source_pos[1] = npos[1];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]].source_pos[2] = npos[2];
        }
        /* LM */
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMp] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMp]] = argument->source[s_index];
            difxcalc_R_Rotate_RADec(pos, theta*DIFX_ROOT_2, 135.0*DIFX_PI/180.0, R);
            difxcalc_multiply_R_v(R, pos, npos);
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMp]].source_pos[0] = npos[0];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMp]].source_pos[1] = npos[1];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMp]].source_pos[2] = npos[2];
            
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMm] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMm]] = argument->source[s_index];
            difxcalc_R_Rotate_RADec(pos, theta*DIFX_ROOT_2, -135.0*DIFX_PI/180.0, R);
            difxcalc_multiply_R_v(R, pos, npos);
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMm]].source_pos[0] = npos[0];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMm]].source_pos[1] = npos[1];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMm]].source_pos[2] = npos[2];
            
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMp] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMp]] = argument->source[s_index];
            difxcalc_R_Rotate_RADec(pos, theta*DIFX_ROOT_2, 45.0*DIFX_PI/180.0, R);
            difxcalc_multiply_R_v(R, pos, npos);
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMp]].source_pos[0] = npos[0];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMp]].source_pos[1] = npos[1];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMp]].source_pos[2] = npos[2];
            
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMm] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMm]] = argument->source[s_index];
            difxcalc_R_Rotate_RADec(pos, theta*DIFX_ROOT_2, -45.0*DIFX_PI/180.0, R);
            difxcalc_multiply_R_v(R, pos, npos);
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMm]].source_pos[0] = npos[0];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMm]].source_pos[1] = npos[1];
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMm]].source_pos[2] = npos[2];
            
        }
        /* LN */
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNp] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNp]] = argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp]];
            r_factor = 1.0 + theta * DIFXIO_DELTA_LMN_N_FACTOR;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNp]].source_pos[0] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNp]].source_pos[1] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNp]].source_pos[2] *= r_factor;
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNm] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNm]] = argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp]];
            r_factor = 1.0 - theta * DIFXIO_DELTA_LMN_N_FACTOR;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNm]].source_pos[0] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNm]].source_pos[1] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNm]].source_pos[2] *= r_factor;
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNp] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNp]] = argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm]];
            r_factor = 1.0 + theta * DIFXIO_DELTA_LMN_N_FACTOR;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNp]].source_pos[0] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNp]].source_pos[1] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNp]].source_pos[2] *= r_factor;
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNm] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNm]] = argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm]];
            r_factor = 1.0 - theta * DIFXIO_DELTA_LMN_N_FACTOR;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNm]].source_pos[0] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNm]].source_pos[1] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNm]].source_pos[2] *= r_factor;
        }
        /* MN */
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNp] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNp]] = argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]];
            r_factor = 1.0 + theta * DIFXIO_DELTA_LMN_N_FACTOR;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNp]].source_pos[0] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNp]].source_pos[1] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNp]].source_pos[2] *= r_factor;
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNm] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNm]] = argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]];
            r_factor = 1.0 - theta * DIFXIO_DELTA_LMN_N_FACTOR;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNm]].source_pos[0] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNm]].source_pos[1] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNm]].source_pos[2] *= r_factor;
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNp] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNp]] = argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]];
            r_factor = 1.0 + theta * DIFXIO_DELTA_LMN_N_FACTOR;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNp]].source_pos[0] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNp]].source_pos[1] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNp]].source_pos[2] *= r_factor;
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNm] >= 0)
        {
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNm]] = argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]];
            r_factor = 1.0 - theta * DIFXIO_DELTA_LMN_N_FACTOR;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNm]].source_pos[0] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNm]].source_pos[1] *= r_factor;
            argument->source[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNm]].source_pos[2] *= r_factor;
        }
    }
    if(num_xyz > 0)
    {
        double delta = (source->delta_xyz == 0.0) ? job->delta_xyz : source->delta_xyz;
        if(delta < 0.0)
        {
            double* tmp;
            double r;
            delta = -delta;
            tmp = argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]].source_pos;
            r = sqrt(tmp[0]*tmp[0] + tmp[1]*tmp[1] + tmp[2]*tmp[2]);
            delta = delta * r;
        }
        source->delta_xyz_used = delta;
        /* Individual indices */
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]] = argument->source[s_index];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]].source_pos[0] += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm]] = argument->source[s_index];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm]].source_pos[0] -= delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_Yp] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Yp]] = argument->source[s_index];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Yp]].source_pos[1] += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_Ym] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Ym]] = argument->source[s_index];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Ym]].source_pos[1] -= delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_Zp] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Zp]] = argument->source[s_index];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Zp]].source_pos[2] += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_Zm] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Zm]] = argument->source[s_index];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Zm]].source_pos[2] -= delta;
        }
        /* XY */
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpYp] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpYp]] = argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpYp]].source_pos[1] += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpYm] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpYm]] = argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpYm]].source_pos[1] -= delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYp] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYp]] = argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm]];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYp]].source_pos[1] += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYm] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYm]] = argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm]];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYm]].source_pos[1] -= delta;
        }
        /* XZ */
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpZp] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpZp]] = argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpZp]].source_pos[2] += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpZm] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpZm]] = argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpZm]].source_pos[2] -= delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmZp] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmZp]] = argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm]];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYp]].source_pos[2] += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmZm] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmZm]] = argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm]];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmZm]].source_pos[2] -= delta;
        }
        /* YZ */
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_YpZp] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YpZp]] = argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Yp]];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YpZp]].source_pos[2] += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_YpZm] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YpZm]] = argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Yp]];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YpZm]].source_pos[2] -= delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_YmZp] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YmZp]] = argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Ym]];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YmZp]].source_pos[2] += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_YmZm] >= 0)
        {
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YmZm]] = argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Ym]];
            argument->source[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YmZm]].source_pos[2] -= delta;
        }
    }

    if(spacecraftId < 0)
    {
        for(i=1; i < num_lmn+num_xyz; ++i)
        {
            double poi[DIFXCALC_3D_VEC_SIZE];
            poi[0] = -argument->source[s_index+i].source_pos[0];
            poi[1] = -argument->source[s_index+i].source_pos[1];
            poi[2] = -argument->source[s_index+i].source_pos[2];
            difx_normalize_to_unit_vector(poi, poi);
            argument->source[s_index+i].source_pointing_dir[0] = poi[0];
            argument->source[s_index+i].source_pointing_dir[1] = poi[1];
            argument->source[s_index+i].source_pointing_dir[2] = poi[2];
        }
    }

	/* Now convert the Cartesian coordinates into (Ra,Dec) for those
	   delay servers that do not understand Cartesian coordinates
	*/
	for(i=1; i < num_lmn+num_xyz; ++i)
	{
        double pos[DIFXCALC_3D_VEC_SIZE];
		double ra, dec, r, parallax;
		pos[0] = argument->source[s_index+i].source_pos[0];
		pos[1] = argument->source[s_index+i].source_pos[1];
		pos[2] = argument->source[s_index+i].source_pos[2];
		difx_Cartesian_to_RADec(pos, &ra, &dec, &r);
		argument->source[s_index+i].ra = ra;
		argument->source[s_index+i].dec = dec;
		if(argument->source[s_index+i].parallax != 0.0)
		{
			parallax = ASTR_UNIT_IAU_2012 / (r*DIFX_PI) * 180.0*3600.0;
			argument->source[s_index+i].parallax = parallax;
		}
	}

    return num_lmn + num_xyz;
}













/* Set up single standard ("star") source */
static unsigned int scanCalcSetupSingleSource(unsigned int s_index, struct SERVER_MODEL_DELAY_ARGUMENT* const argument, const DifxJob* const job, const DifxScan* const scan, DifxSource* const source, double scan_MJD_midpoint, const DifxInput* const D, const CalcParams* const p, const int verbose)
{
    int spacecraftId = -1;
    int num_source_entries;
    /* source direction, velocity, and reference direction */
    double pos[DIFXCALC_3D_VEC_SIZE], vel[DIFXCALC_3D_VEC_SIZE], ref_dir[DIFXCALC_3D_VEC_SIZE], point_dir[DIFXCALC_3D_VEC_SIZE];
    int pos_changed;

    /* Reset estimated delay offset to source for station 0 at the start
       of each new scan
    */
    source->station0PropDelay = 0.0;
    spacecraftId = source->spacecraftId;

    /* Fill in the 0th source, the unaltered source direction */
    difx_strlcpy(argument->source[s_index].source_name, source->name, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    /* TODO: get IAU Name from somewhere for calc11 server */
    argument->source[s_index].IAU_name[0] = 0;
    difx_strlcpy(argument->source[s_index].source_type, (spacecraftId < 0) ? "star": "ephemeris", DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
    if(spacecraftId < 0)
    {
        /* Not an ephemeris object.  Put in position information here.
           Ephemeris objects will be handled elsewhere.
         */
	    argument->source[s_index].coord_frame = int32_t(source->coord_frame);
        pos_changed = convert_RA_Dec_PM_to_vector(source->ra, source->dec, source->parallax, source->pmRA, source->pmDec, 0.0, source->pmEpoch, scan_MJD_midpoint, pos, vel, ref_dir);
        if((pos_changed))
        {
            double ra, dec, r, parallax;
            difx_Cartesian_to_RADec(pos, &ra, &dec, &r);
            parallax = ASTR_UNIT_IAU_2012 / (r*DIFX_PI) * 180.0*3600.0;
            argument->source[s_index].ra = ra;
            argument->source[s_index].dec = dec;
            argument->source[s_index].dra = 0.0;
            argument->source[s_index].ddec = 0.0;
            argument->source[s_index].depoch = 0.0;
            argument->source[s_index].parallax = parallax;
        }
        else
        {
            argument->source[s_index].ra = source->ra;
            argument->source[s_index].dec = source->dec;
            argument->source[s_index].dra = 0.0;
            argument->source[s_index].ddec = 0.0;
            argument->source[s_index].depoch = 0.0;
            argument->source[s_index].parallax = source->parallax;
        }
        for(uint_fast8_t k=0; k < 3; ++k)
        {
	        argument->source[s_index].source_pos[k] = pos[k];
	        argument->source[s_index].source_vel[k] = vel[k];
	        argument->source[s_index].source_acc[k] = 0.0;
	        argument->source[s_index].source_pointing_reference_dir[k] = ref_dir[k];
	        point_dir[k] = -pos[k];
        }
        difx_normalize_to_unit_vector(point_dir, point_dir);
        argument->source[s_index].source_pointing_dir[0] = point_dir[0];
        argument->source[s_index].source_pointing_dir[1] = point_dir[1];
        argument->source[s_index].source_pointing_dir[2] = point_dir[2];

        num_source_entries = scanCalcSetupSingleSourceDerivs(s_index, argument, job, source, verbose);
    }
    else
    {
        /* Ephemeris object.  Just put in coordinate frame here.
           Ephemeris object details will be handled elsewhere.
         */
        DifxSpacecraft* sc;
        sc = D->spacecraft + spacecraftId;

        argument->source[s_index].coord_frame = int32_t(sc->position_coord_frame);
        num_source_entries = calculateSourceIndexOffset(job, source);
    }

    return num_source_entries;
}










/* Set up single spacecraft source at specific time step*/
static int scanCalcSetupSingleSpacecraftSource(unsigned int s_index, struct SERVER_MODEL_DELAY_ARGUMENT* const argument, const DifxJob* const job, const DifxScan* const scan, DifxSource* const source, DifxInput* const D, const CalcParams* const p, const int verbose)
{
    int spacecraftId;
    unsigned int num_source_entries;
    int r;
    DifxSpacecraft* sc;
        
    spacecraftId = source->spacecraftId;
    if(spacecraftId < 0)
    {
        fprintf(stderr, "Programmer error: non spacecraft source given to scanCalcSetupSingleSpacecraftSource\n");
        return -1;
    }
    sc = D->spacecraft + spacecraftId;

    /* call with the delay to station 0 (the Earth center) from the
       previous call to the delay server as the starting guess for the
       delay to the source.  This is stored insource->station0PropDelay.
    */
    r = calcSpacecraftSourcePosition(D, sc, source, argument, spacecraftId, s_index, source->station0PropDelay);
    if(r < 0)
    {
        printf("Error: scanCalcSetupSingleSpacecraftSource: Spacecraft %d table out of time range with code %d\n", spacecraftId, r);
        return -2;
    }

    num_source_entries = scanCalcSetupSingleSourceDerivs(s_index, argument, job, source, verbose);
    return (int)num_source_entries;
}




/* Set up all standard ("star") sources */
static int scanCalcSetupSources(const DifxScan* const scan, const DifxInput* const D, CalcParams* const p, double scan_MJD_midpoint, const int verbose)
{
	const DifxJob* const job(D->job);
    struct SERVER_MODEL_DELAY_ARGUMENT* const argument(&(p->argument));
    struct SERVER_MODEL_DELAY_RESPONSE* const response(&(p->response));
    int k;
    DifxSource *source;
    int sourceId;
    uint32_t Num_Argument_Sources;
    unsigned int s_index;

    /* Figure out how many argument sources need to be present in the argument. */
    Num_Argument_Sources = 0;
    for(k = 0; k < scan->nPhaseCentres +1; ++k)
    {
        if(k == 0) // this is the pointing centre
        {
            sourceId = scan->pointingCentreSrc;
        }
        else
        {
            sourceId = scan->phsCentreSrcs[k-1];
        }
        source = D->source + sourceId;
        Num_Argument_Sources += calculateSourceIndexOffset(job, source);
    }
    /* check memory allocation */
    if(Num_Argument_Sources > p->Num_Allocated_Sources)
    {
	    size_t size;
	    void* mem;
        p->Num_Allocated_Sources = Num_Argument_Sources;
        size = sizeof(struct SERVER_MODEL_DELAY_ARGUMENT_SOURCE)*p->Num_Allocated_Sources;
        if((mem = realloc(argument->source, size)) == NULL)
        {
            fprintf(stderr, "Could not realloc source memory for %"PRIu32" sources\n", p->Num_Allocated_Sources);
            exit(EXIT_FAILURE);
        }
        argument->source = reinterpret_cast<struct SERVER_MODEL_DELAY_ARGUMENT_SOURCE*>(mem);
        size = sizeof(struct SERVER_MODEL_DELAY_RESPONSE_DATA) * p->Num_Allocated_Sources * p->Num_Allocated_Stations;
        if((mem = realloc(response->result, size)) == NULL)
        {
            fprintf(stderr, "Could not realloc result memory for %"PRIu32" sources*stations\n", p->Num_Allocated_Sources*p->Num_Allocated_Stations);
            exit(EXIT_FAILURE);
        }
        response->result = reinterpret_cast<struct SERVER_MODEL_DELAY_RESPONSE_DATA*>(mem);
    }
    argument->Num_Sources = Num_Argument_Sources;
    response->Num_Sources = Num_Argument_Sources;


    s_index = 0;
    for(k = 0; k < scan->nPhaseCentres +1; ++k)
    {
        unsigned int num_source_entries;
        if(k == 0) // this is the pointing centre
        {
            sourceId = scan->pointingCentreSrc;
        }
        else
        {
            sourceId = scan->phsCentreSrcs[k-1];
        }
        source = D->source + sourceId;
        num_source_entries = scanCalcSetupSingleSource(s_index, argument, job, scan, source, scan_MJD_midpoint, D, p, verbose);
        s_index += num_source_entries;
    }

    return 0;
}







/* Set up all spacecraft sources at each time step*/
static int scanCalcSetupSpacecraftSources(const DifxScan* const scan, const DifxJob* const job, DifxInput* const D, CalcParams* const p, const int verbose)
{
	struct SERVER_MODEL_DELAY_ARGUMENT* const argument(&(p->argument));
    int k;
    DifxSource *source;
    int spacecraftId = -1;
    int sourceId;
    unsigned int s_index;

    s_index = 0;
    for(k = 0; k < scan->nPhaseCentres +1; ++k)
    {
        unsigned int num_source_entries;
        if(k == 0) // this is the pointing centre
        {
            sourceId = scan->pointingCentreSrc;
        }
        else
        {
            sourceId = scan->phsCentreSrcs[k-1];
        }
        source = D->source + sourceId;
        spacecraftId = source->spacecraftId;
        if(spacecraftId < 0)
        {
            s_index += calculateSourceIndexOffset(job, source);
        }
        else
        {
            if(k == 0)
            {
                if(p->warnSpacecraftPointingSource)
                {
                    fprintf(stderr, "WARNING: found spacecraft as pointing source!  Is this intentional, or did one of the programmers mess up?\n");
                }
            }
            num_source_entries = scanCalcSetupSingleSpacecraftSource(s_index, argument, job, scan, source, D, p, verbose);
            if(num_source_entries < 0)
            {
                fprintf(stderr, "Error: unable to set up spacecraft source %d in scanCalcSetupSpacecraftSources\n", k);
                return -1;
            }
            s_index += num_source_entries;
        }
    }
    return 0;
}



/* Setup the delay server argument area for the stations */
static int scanCalcSetupStations(const DifxScan* const scan, double scan_MJD_midpoint, const DifxInput* const D, const char *prefix, CalcParams* const p, const int verbose)
{
	struct SERVER_MODEL_DELAY_ARGUMENT* const argument(&(p->argument));
	struct SERVER_MODEL_DELAY_RESPONSE* const response(&(p->response));
    DifxSource* pointing_center;
    DifxAntenna *antenna;
    int antId;
    int spacecraftId;
    /* source direction, velocity, and reference direction */
    double ps_pos[DIFXCALC_3D_VEC_SIZE], ps_vel[DIFXCALC_3D_VEC_SIZE], ps_ref[DIFXCALC_3D_VEC_SIZE];
    

    /* check station allocation size */
    if(uint32_t(scan->nAntenna + 1) > p->Num_Allocated_Stations)
    {
	    size_t size;
	    void* mem;
        p->Num_Allocated_Stations = scan->nAntenna + 1;
        size = sizeof(struct SERVER_MODEL_DELAY_ARGUMENT_STATION)*p->Num_Allocated_Stations;
        if((mem = realloc(argument->station, size)) == NULL)
        {
            fprintf(stderr, "Could not realloc initial station memory for %"PRIu32" stations\n", p->Num_Allocated_Stations);
            exit(EXIT_FAILURE);
        }
        argument->station = reinterpret_cast<struct SERVER_MODEL_DELAY_ARGUMENT_STATION*>(mem);
        size = sizeof(struct SERVER_MODEL_DELAY_RESPONSE_DATA) * p->Num_Allocated_Sources * p->Num_Allocated_Stations;
        if((mem = realloc(response->result, size)) == NULL)
        {
            fprintf(stderr, "Could not realloc result memory for %"PRIu32" sources*stations\n", p->Num_Allocated_Sources*p->Num_Allocated_Stations);
            exit(EXIT_FAILURE);
        }
        response->result = reinterpret_cast<struct SERVER_MODEL_DELAY_RESPONSE_DATA*>(mem);
    }
    argument->Num_Stations = scan->nAntenna + 1;
    response->Num_Stations = scan->nAntenna + 1;

    /* Where are the antennas pointed? */
    pointing_center = D->source + scan->pointingCentreSrc;

    convert_RA_Dec_PM_to_vector(pointing_center->ra, pointing_center->dec, pointing_center->parallax, pointing_center->pmRA, pointing_center->pmDec, 0.0, pointing_center->pmEpoch, scan_MJD_midpoint, ps_pos, ps_vel, ps_ref);



    
    for(antId = 0; antId < scan->nAntenna; ++antId)
    {
        uint16_t station_ID;
        antenna = D->antenna + antId;
        spacecraftId = antenna->spacecraftId;

        difx_strlcpy(argument->station[1+antId].station_name, antenna->calcname, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
        difx_strlcpy(argument->station[1+antId].antenna_name, antenna->calcname, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
        difx_strlcpy(argument->station[1+antId].site_name, antenna->calcname, DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
        station_ID = (uint16_t)(antenna->calcname[0]) | ((uint16_t)(antenna->calcname[1]) << 8);
        argument->station[1+antId].site_ID = station_ID;
        difx_strlcpy(argument->station[1+antId].site_type, antennaSiteTypeNames[antenna->sitetype], DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);
        difx_strlcpy(argument->station[1+antId].axis_type, antennaMountTypeNames[antenna->mount], DIFX_DELAYHANDLERDISTRIBUTOR_STATION_STRING_SIZE);


        argument->station[1+antId].station_pos[0] = antenna->X;
        argument->station[1+antId].station_pos[1] = antenna->Y;
        argument->station[1+antId].station_pos[2] = antenna->Z;
        /* for ground-based antennas, ground drift already*/
        /* taken into account, so set the station */
        /* velocities to 0 */
        argument->station[1+antId].station_vel[0] =  0.0;
        argument->station[1+antId].station_vel[1] =  0.0;
        argument->station[1+antId].station_vel[2] =  0.0;
        /* for ground-based antennas, acceleration */
        /* calculated from Earth rotation, so set the */
        /* accelerations to 0 */
        argument->station[1+antId].station_acc[0] =  0.0;
        argument->station[1+antId].station_acc[1] =  0.0;
        argument->station[1+antId].station_acc[2] =  0.0;

        argument->station[1+antId].station_pointing_dir[0] = ps_pos[0];
        argument->station[1+antId].station_pointing_dir[1] = ps_pos[1];
        argument->station[1+antId].station_pointing_dir[2] = ps_pos[2];

        argument->station[1+antId].station_reference_dir[0] =  0.0;
        argument->station[1+antId].station_reference_dir[1] =  0.0;
        argument->station[1+antId].station_reference_dir[2] =  0.0;
        if(spacecraftId < 0)
        {
	        argument->station[1+antId].station_coord_frame = int32_t(antenna->site_coord_frame);
            if(pointing_center->spacecraftId < 0)
            {
	            argument->station[1+antId].pointing_coord_frame = int32_t(pointing_center->coord_frame);
            }
            else
            {
                const DifxSpacecraft* sc;
                sc = D->spacecraft + pointing_center->spacecraftId;
                argument->station[1+antId].pointing_coord_frame = int32_t(sc->position_coord_frame);
            }
            argument->station[1+antId].pointing_corrections_applied = 2;
        }
        else
        {
            const DifxSpacecraft* sc;
            sc = D->spacecraft + spacecraftId;

            argument->station[1+antId].station_coord_frame = int32_t(sc->position_coord_frame);
            argument->station[1+antId].pointing_coord_frame = int32_t(sc->pointing_coord_frame);
            argument->station[1+antId].pointing_corrections_applied = 1;
        }
        argument->station[1+antId].station_position_delay_offset = 0.0;
        argument->station[1+antId].axis_off = antenna->offset[0];
        /* TODO: get wrap, pressure, temperature information from
           VEX/log files?
        */
        argument->station[1+antId].primary_axis_wrap = 0;
        argument->station[1+antId].secondary_axis_wrap = 0;
        argument->station[1+antId].receiver_name[0] = 0;
        argument->station[1+antId].pressure = 0.0;
        argument->station[1+antId].antenna_pressure = 0.0;
        argument->station[1+antId].temperature = 0.0;
        argument->station[1+antId].wind_speed = fitsnan.d;
        argument->station[1+antId].wind_direction = fitsnan.d;
        argument->station[1+antId].antenna_phys_temperature = 0.0;
    } /* for antId over antennas */
        

    return 0;
}

/* Setup the delay server argument area for the spacecraft stations. */
/* The evaluation time information should already be inside p->argument. */
static int scanCalcSetupSpacecraftStations(const DifxScan* const scan, const DifxInput* const D, const char *prefix, CalcParams* const p, const int verbose)
{
	struct SERVER_MODEL_DELAY_ARGUMENT* const argument(&(p->argument));
    DifxAntenna *antenna;
    int antId;
    int r;
    int spacecraftId;

    for(antId = 0; antId < scan->nAntenna; ++antId)
    {
        antenna = D->antenna + antId;
        spacecraftId = antenna->spacecraftId;
        {
            /* TODO: implement weather information updates for all stations
               here
            */
        }
        if(spacecraftId >= 0)
        {
            r = calcSpacecraftAntennaPosition(D, argument, spacecraftId, antId+1, 0.0);
            if(r < 0)
            {
                printf("Error: scanCalcSetupSpacecraftStations: Antenna spacecraft %d table out of time range, or not yet supported (code=%d)\n", spacecraftId, r);
                
                return -1;
            }
        }
    }
    return 0;
}







static void scanCalcSourcePropDelayFill(const DifxScan* const scan, DifxInput* const D, const struct SERVER_MODEL_DELAY_RESPONSE* const response, const int verbose)
{
    int k;
    int sourceId;
    DifxSource* source;
    unsigned int s_index;
    unsigned int N;
    s_index = 0;
    for(k = 0; k < scan->nPhaseCentres + 1; ++k)
    {
        if(k == 0)
        {
            sourceId = scan->pointingCentreSrc;
        }
        else
        {
            sourceId = scan->phsCentreSrcs[k-1];
        }
        source = D->source + sourceId;
        source->station0PropDelay = response->result[s_index].delay;
        N = calculateSourceIndexOffset(D->job, source);
        s_index += N;
    }
    return;
}
static void scanCalcSourceLastDelaysInitialize(const struct SERVER_MODEL_DELAY_RESPONSE* const response, double* const last_delays, const int verbose)
{
    uint32_t N;
    uint32_t i;
    N = response->Num_Stations*response->Num_Sources;
    for(i=0; i < N; ++i)
    {
        last_delays[i] = response->result[i].delay;
    }
    return;
}
static double scanCalcSourceLastDelaysCompare(const DifxScan* const scan, const DifxInput* const D, const CalcParams* const p, const struct SERVER_MODEL_DELAY_RESPONSE* const response, double* const last_delay, const int verbose)
{
    int k;
    int antId;
    int sourceId;
    const DifxSource* source;
    unsigned int a_index;
    unsigned int s_index;
    unsigned int l;
    unsigned int N;
    double max_error = -DBL_MAX;
    for(antId=0; antId < scan->nAntenna; ++antId)
    {
        a_index = (antId+1)*response->Num_Sources;
        source = D->source + scan->pointingCentreSrc;
        if(source->spacecraftId < 0)
        {
            s_index = 0;
            /* non-spacecraft pointing direction */
            for(k = 0; k < scan->nPhaseCentres + 1; ++k)
            {
                if(k == 0)
                {
                    sourceId = scan->pointingCentreSrc;
                }
                else
                {
                    sourceId = scan->phsCentreSrcs[k-1];
                }
                source = D->source + sourceId;
                N = calculateSourceIndexOffset(D->job, source);
                for(l=0; l < N; ++l)
                {
                    double diff = response->result[a_index+s_index+l].delay - last_delay[a_index+s_index+l];
                    diff = fabs(diff);
                    last_delay[a_index+s_index+l] = response->result[a_index+s_index+l].delay;
                    if(diff > max_error)
                    {
                        max_error = diff;
                    }
                }
                s_index += N;
            }
        }
        else
        {
            /* pointing direction is a spacecraft --- do all sources */
            for(l=0; l < response->Num_Sources; ++l)
            {
                double diff = response->result[a_index+l].delay - last_delay[a_index+l];
                diff = fabs(diff);
                last_delay[a_index+l] = response->result[a_index+l].delay;
                if(diff > max_error)
                {
                    max_error = diff;
                }
            }
        }
    }
    return max_error;
}







/* Setup the delay server argument area for the stations */
static int scanCalcGetDelays(const DifxScan* const scan, const int haveSpacecraftAntenna, const int haveSpacecraftSource, struct modelTemp** model, struct modelTempLMN** modelLMN, struct modelTempXYZ** modelXYZ, DifxInput* const D, const char *prefix, CalcParams* const p, const int verbose)
{
	struct SERVER_MODEL_DELAY_ARGUMENT* const argument(&(p->argument));
	struct SERVER_MODEL_DELAY_RESPONSE* const response(&(p->response));
    int i, j, v;
    int_fast32_t r32;
    int antId;
    int k;
    double sec, subInc;
    double lastsec = -1000;
    double* last_delay = NULL;
    DifxSource *source;
    DifxAntenna *antenna;
    const DifxJob* const job(D->job);
    int sourceId;
    int nError = 0;
    double max_error;
    int loop_count;
    const int MAX_LOOP_COUNT = 10;
    char externalDelayFilename[DIFXIO_FILENAME_LENGTH];
    ExternalDelay ***ed = NULL;

    subInc = D->job->polyInterval/(double)(D->job->polyOrder*p->oversamp);

	
    if((haveSpacecraftSource))
    {
        last_delay = (double*)malloc(sizeof(double)*argument->Num_Stations*argument->Num_Sources);
    }

    if(p->useExtraExternalDelay == 1)
    {
        if(verbose >= 2)
        {
            fprintf(stderr, "Loading Sekido delay models...\n");
        }
        ed = (ExternalDelay ***)calloc(scan->nAntenna, sizeof(ExternalDelay **));
        for(antId=0; antId < scan->nAntenna; ++antId)
        {
            antenna = D->antenna + antId;
            ed[antId] = (ExternalDelay **)calloc(scan->nPhaseCentres + 1, sizeof(ExternalDelay *));
            for(k = 0; k < scan->nPhaseCentres + 1; ++k)
            {
                if(k == 0)
                {
                    sourceId = scan->pointingCentreSrc;
                }
                else
                {
                    sourceId = scan->phsCentreSrcs[k-1];
                }
                source = D->source + sourceId;
                snprintf(externalDelayFilename, DIFXIO_FILENAME_LENGTH, "%s.%s.%s.delay", prefix, antenna->name, source->name);
                if(verbose >= 3)
                {
                    fprintf(stderr, "Loading Sekido delay model from '%s'\n", externalDelayFilename);
                }
                ed[antId][k] = newExternalDelay(externalDelayFilename);
                if(!ed[antId][k])
                {
                    snprintf(externalDelayFilename, DIFXIO_FILENAME_LENGTH, "%s_%s.delay", antenna->name, source->name);
                    ed[antId][k] = newExternalDelay(externalDelayFilename);
                    if((ed[antId][k]))
                    {
                        fprintf(stderr, "Warning: using %s to drive delays.  This filename designator is obsolete.\n", externalDelayFilename);
                    }
                }
                if((verbose >= 3) && (!ed[antId][k]))
                {
                    fprintf(stderr, "Unable to load Sekido delay model from '%s'\n", externalDelayFilename);
                }
            }
        }
        if(verbose >= 2)
        {
            fprintf(stderr, "Loading Sekido delay models...Done!\n");
        }
    } /* if(p->useExtraExternalDelay == 1) Sekido delay model */


    
    for(i = 0; i < scan->nPoly; ++i)
    {
        argument->date = scan->im[0][0][i].mjd;
        sec = scan->im[0][0][i].sec;
        for(j = 0; j <= D->job->polyOrder*p->oversamp; ++j)
        {
            argument->time = sec/SEC_DAY_DBL;
            {
	            DiFX::Delay::Time::DelayTimestamp t(argument->date,argument->time,DiFX::Delay::Time::DIFX_TIME_SYSTEM_MJD);
	            argument->utc_second = t.i();
	            argument->utc_second_fraction = t.f();
	            DiFX::Delay::Time::DelayTimestamp t_TAI = t.UTC_to_TAI();
	            argument->tai_second = t_TAI.i();
	            argument->tai_second_fraction = t_TAI.f();
            }

            /* call calc if we didn't just for this time */
            if(fabs(lastsec - sec) > 1.0e-6)
            {
                /* Update all station data for the argument time */
                v = scanCalcSetupSpacecraftStations(scan, D, prefix, p, verbose);
                if(v < 0)
                {
                    fprintf(stderr, "Error: got return code %d from scanCalcSetupSpacecraftStations in scanCalcGetDelays\n", v);
                    nError = -1;
                    goto end_of_scanCalcGetDelays;
                }
                /* Update all sources for the argument time */
                v = scanCalcSetupSpacecraftSources(scan, job, D, p, verbose);
                if(v < 0)
                {
                    fprintf(stderr, "Error: got return code %d from scanCalcSetupSpacecraftSources in scanCalcGetDelays\n", v);
                    nError = -2;
                    goto end_of_scanCalcGetDelays;
                }
                r32 = p->delayDistributor->process_delay_service(D, job, scan, argument, response, int_fast32_t(verbose));
                if((r32))
                {
                    printf("Error: scanCalcGetDelays: process_delay_service = %"PRIdFAST32"\n", r32);
                                        
                    nError = -3;
                    goto end_of_scanCalcGetDelays;
                }
                scanCalcSourcePropDelayFill(scan, D, response, verbose);
                if(((haveSpacecraftSource)) && ((D->job->calculate_own_retarded_position)))
                {
                    scanCalcSourceLastDelaysInitialize(response, last_delay, verbose);
                    max_error = DBL_MAX;
                    for(loop_count = 0; (max_error > D->job->delayModelPrecision) && (loop_count < MAX_LOOP_COUNT); ++loop_count)
                    {
                        /* Update all sources for the argument time */
                        v = scanCalcSetupSpacecraftSources(scan, job, D, p, verbose);
                        if(v < 0)
                        {
                            fprintf(stderr, "Error: got return code %d from scanCalcSetupSpacecraftSources in scanCalcGetDelays\n", v);
                            nError = -4;
                            goto end_of_scanCalcGetDelays;
                        }
                        r32 = p->delayDistributor->process_delay_service(D, job, scan, argument, response, int_fast32_t(verbose));
                        if((r32))
                        {
                            printf("Error: scanCalcGetDelays: process_delay_service = %"PRIdFAST32"\n", r32);
                                        
                            nError = -5;
                            goto end_of_scanCalcGetDelays;
                        }
                        scanCalcSourcePropDelayFill(scan, D, response, verbose);
                        max_error = scanCalcSourceLastDelaysCompare(scan, D, p, response, last_delay, verbose);
                        if(verbose >= 2)
                        {
                            fprintf(stderr, "Info: after loop %d, maximum delay error for spacecraft sources is %E (max allowed is %E)\n", loop_count, max_error, D->job->delayModelPrecision);
                        }
                    }
                    if(loop_count >= MAX_LOOP_COUNT)
                    {
                        if(verbose >= 1)
                        {
                            fprintf(stderr, "Warning: maximum loop count exceeded in spacecraft source retarded position loop\n");
                        }
                    }
                }
                if((haveSpacecraftAntenna))
                {
                    v = adjustSpacecraftAntennaCalcResults(scan, D, p, argument, response, verbose);
                    if(v < 0)
                    {
                        printf("Error: scanCalcGetDelays: Antenna spacecraft problem %d calculating recording offset\n", v);
                        nError = -6;
                        goto end_of_scanCalcGetDelays;
                    }
                }
            }

            /* use result to populate tabulated values */
            nError += extractCalcResults(scan, D, p, j, model, modelLMN, modelXYZ, response, verbose);

            /* override delay and atmosphere values */
            if(p->useExtraExternalDelay == 1)
            {
                for(antId=0; antId < scan->nAntenna; ++antId)
                {
                    antenna = D->antenna + antId;
                    for(k = 0; k < scan->nPhaseCentres + 1; ++k)
                    {
                        if(k == 0)
                        {
                            sourceId = scan->pointingCentreSrc;
                        }
                        else
                        {
                            sourceId = scan->phsCentreSrcs[k-1];
                        }
                        source = D->source + sourceId;
                        if((ed[antId][k]))
                        {
                            int v;
                            double exDelay, exDry, exWet;

                            v = getExternalDelay(ed[antId][k], argument->date+argument->time, &exDelay, &exDry, &exWet);
                            if(v < 0)
                            {
                                fprintf(stderr, "Error: scanCalcGetDelays: argument for external delay from stn %s source %s at time %14.8f failed with error code %d\n", antenna->name, source->name, argument->date+argument->time, v);
                                nError = -7;
                                goto end_of_scanCalcGetDelays;
                            }

                            model[antId][k].delay[j] = -(exDelay+exDry+exWet)*MICROSECONDS_PER_SECOND;
                            model[antId][k].dry[j] = exDry*MICROSECONDS_PER_SECOND;
                            model[antId][k].wet[j] = exWet*MICROSECONDS_PER_SECOND;
                        }

                    }
                }
            } /* if(p->useExtraExternalDelay == 1) Sekido delay model */

            lastsec = sec;
            sec += subInc;
            if(sec >= SEC_DAY_DBL)
            {
                sec -= SEC_DAY_DBL;
                argument->date += 1;
            }
        }
        computePolyModel(scan, D, i, model, modelLMN, modelXYZ, subInc, p->oversamp, p->interpol, scan->im, scan->imLMN, scan->imXYZ, verbose);
    }


end_of_scanCalcGetDelays:
    free(last_delay);
    last_delay = 0;
    if(p->useExtraExternalDelay == 1)
    {
        if(verbose >= 2)
        {
            fprintf(stderr, "Freeing Sekido delay model memory...\n");
        }
        for(antId=0; antId < scan->nAntenna; ++antId)
        {
            for(k = 0; k < scan->nPhaseCentres + 1; ++k)
            {
                free(ed[antId][k]);
            }
            free(ed[antId]);
        }
        free(ed);
        ed = NULL;
    }

    if(nError > 0)
    {
        fprintf(stderr, "Error: scanCalcGetDelays: had %d invalid delays\n", nError);
    }
    else if(nError < 0)
    {
        fprintf(stderr, "Error: scanCalcGetDelays: had severe error %d\n", nError);
    }

    return nError;
}







static int scanCalc(const int scanId, DifxInput* const D, const char *prefix, CalcParams* const p, const int isLast, const int verbose)
{
    enum PerformDirectionDerivativeType scan_perform_lmn;
    enum PerformDirectionDerivativeType scan_perform_xyz;
    struct modelTemp    **model = NULL;    /* model[antId][sourceId] */
    struct modelTempLMN **modelLMN = NULL; /* modelLMN[antId][sourceId] */
    struct modelTempXYZ **modelXYZ = NULL; /* modelXYZ[antId][sourceId] */
    int antId;
    int mjd, sec;
    int sec1, sec2;
    int jobStart;   /* seconds since last midnight */
    int int1, int2; /* polynomial intervals */
    int nInt;
    int i, v, k;
    double scan_MJD_midpoint;
    DifxJob* const job(D->job);
    DifxScan* const scan(D->scan + scanId);
    int haveSpacecraftAntenna = 0;
    int haveSpacecraftSource = 0;

    scan->nAntenna = D->nAntenna;

    /* Check whether or not the extra delay derivative models need to be computed */
    scan_perform_lmn = job->perform_lmn_deriv;
    if(scan_perform_lmn == PerformDirectionDerivativeNone)
    {
        int sourceId;
        const DifxSource* source;
        for(k = 0; k < scan->nPhaseCentres+1; ++k)
        {
            if(k==0)
            {
                sourceId = scan->pointingCentreSrc;
                source = D->source + sourceId;
            }
            else
            {
                sourceId = scan->phsCentreSrcs[k-1];
                source = D->source + sourceId;
            }
            if(source->perform_lmn_deriv >= PerformDirectionDerivativeFirstDerivative)
            {
                scan_perform_lmn = PerformDirectionDerivativeFirstDerivative;
                break;
            }
        }
    }
    scan_perform_xyz = job->perform_xyz_deriv;
    if(scan_perform_xyz == PerformDirectionDerivativeNone)
    {
        int sourceId;
        const DifxSource* source;
        for(k = 0; k < scan->nPhaseCentres+1; ++k)
        {
            if(k==0)
            {
                sourceId = scan->pointingCentreSrc;
                source = D->source + sourceId;
            }
            else
            {
                sourceId = scan->phsCentreSrcs[k-1];
                source = D->source + sourceId;
            }
            if(source->perform_xyz_deriv >= PerformDirectionDerivativeFirstDerivative)
            {
                scan_perform_xyz = PerformDirectionDerivativeFirstDerivative;
                break;
            }
        }
    }

    scan->im = (DifxPolyModel ***)calloc(scan->nAntenna, sizeof(DifxPolyModel **));
    model = (struct modelTemp**)calloc(scan->nAntenna, sizeof(struct modelTemp**));
    if((scan_perform_lmn))
    {
        scan->imLMN = (DifxPolyModelLMNExtension ***)calloc(scan->nAntenna, sizeof(DifxPolyModelLMNExtension **));
        modelLMN = (struct modelTempLMN**)calloc(scan->nAntenna, sizeof(struct modelTempLMN*));
    }
    if((scan_perform_xyz))
    {
        scan->imXYZ = (DifxPolyModelXYZExtension ***)calloc(scan->nAntenna, sizeof(DifxPolyModelXYZExtension **));
        modelXYZ = (struct modelTempXYZ**)calloc(scan->nAntenna, sizeof(struct modelTempXYZ*));
    }

    mjd = (int)(job->mjdStart);
    jobStart = (int)(SEC_DAY_DBL*(job->mjdStart - mjd) + 0.5);

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
    scan_MJD_midpoint = mjd + (sec1+sec2)*0.5/SEC_DAY_DBL;

    /* Allocate memory for the PolyModels */

    for(antId = 0; antId < scan->nAntenna; ++antId)
    {
        if(D->antenna[antId].spacecraftId >= 0)
        {
            haveSpacecraftAntenna = 1;
        }
        scan->im[antId] = (DifxPolyModel **)calloc(scan->nPhaseCentres+1, sizeof(DifxPolyModel*));
        model[antId] = (struct modelTemp*)calloc(scan->nPhaseCentres+1, sizeof(struct modelTemp));

        if((scan_perform_lmn))
        {
            scan->imLMN[antId] = (DifxPolyModelLMNExtension **)calloc(scan->nPhaseCentres+1, sizeof(DifxPolyModelLMNExtension*));
            modelLMN[antId] = (struct modelTempLMN*)calloc(scan->nPhaseCentres+1, sizeof(struct modelTempLMN));
        }
        if((scan_perform_xyz))
        {
            scan->imXYZ[antId] = (DifxPolyModelXYZExtension **)calloc(scan->nPhaseCentres+1, sizeof(DifxPolyModelXYZExtension*));
            modelXYZ[antId] = (struct modelTempXYZ*)calloc(scan->nPhaseCentres+1, sizeof(struct modelTempXYZ));
        }
        
        for(k = 0; k < scan->nPhaseCentres + 1; ++k)
        {
            int sourceId;
            DifxSource *source;
			enum PerformDirectionDerivativeType perform_lmn_deriv;
			enum PerformDirectionDerivativeType perform_xyz_deriv;
            if(k == 0)
            {
                sourceId = scan->pointingCentreSrc;
            }
            else
            {
                sourceId = scan->phsCentreSrcs[k-1];
            }
            source = D->source + sourceId;
            if(source->spacecraftId >= 0)
            {
                haveSpacecraftSource = 1;
            }
            
            scan->im[antId][k] = (DifxPolyModel *)calloc(nInt, sizeof(DifxPolyModel));
			perform_lmn_deriv = (source->perform_lmn_deriv == PerformDirectionDerivativeDefault) ? job->perform_lmn_deriv: source->perform_lmn_deriv;
            if(perform_lmn_deriv >= PerformDirectionDerivativeFirstDerivative)
            {
                scan->imLMN[antId][k] = (DifxPolyModelLMNExtension *)calloc(nInt, sizeof(DifxPolyModelLMNExtension));
            }
			perform_xyz_deriv = (source->perform_xyz_deriv == PerformDirectionDerivativeDefault) ? job->perform_xyz_deriv: source->perform_xyz_deriv;
            if(perform_xyz_deriv >= PerformDirectionDerivativeFirstDerivative)
            {
                scan->imXYZ[antId][k] = (DifxPolyModelXYZExtension *)calloc(nInt, sizeof(DifxPolyModelXYZExtension));
            }
            sec = int1*D->job->polyInterval;
            mjd = (int)(job->mjdStart);
        
            for(i = 0; i < nInt; ++i)
            {
                if(sec >= SEC_DAY)
                {
                    sec -= SEC_DAY;
                    ++mjd;
                }
    
                /* set up the intervals to calc polys over */
                scan->im[antId][k][i].mjd = mjd;
                scan->im[antId][k][i].sec = sec;
                scan->im[antId][k][i].order = D->job->polyOrder;
                scan->im[antId][k][i].validDuration = D->job->polyInterval;
                if(source->perform_uvw_deriv != PerformDirectionDerivativeDefault)
                {
                    scan->im[antId][k][i].delta = source->delta_lmn;
                }
                else
                {
                    scan->im[antId][k][i].delta = job->delta_lmn;
                }
				if((scan->imLMN))
				{
					if((scan->imLMN[antId]))
					{
						if((scan->imLMN[antId][k]))
						{
							if(source->perform_lmn_deriv != PerformDirectionDerivativeDefault)
							{
								scan->imLMN[antId][k][i].delta = source->delta_lmn;
							}
							else
							{
								scan->imLMN[antId][k][i].delta = job->delta_lmn;
							}
						}
					}
                }
				if((scan->imXYZ))
				{
					if((scan->imXYZ[antId]))
					{
						if((scan->imXYZ[antId][k]))
						{
							if(source->perform_xyz_deriv != PerformDirectionDerivativeDefault)
							{
								scan->imXYZ[antId][k][i].delta = source->delta_xyz;
							}
							else
							{
								scan->imXYZ[antId][k][i].delta = job->delta_xyz;
							}
						}
					}
				}
                sec += D->job->polyInterval;
            }
        } /* for k over scan->nPhaseCentres + 1 */
    } /* for antId over scan->nAntenna */

    v = scanCalcSetupStations(scan, scan_MJD_midpoint, D, prefix, p, verbose);
    if(v < 0)
    {
        fprintf(stderr, "Error: received code %d from scanCalcSetupStations in scanCalc\n", v);
        return -1;
    }
    v = scanCalcSetupSources(scan, D, p, scan_MJD_midpoint, verbose);
    if(v < 0)
    {
        fprintf(stderr, "Error: received code %d from scanCalcSetupSources in scanCalc\n", v);
        return -2;
    }
    /* call delay server to derive delay, etc... polys */
    v = scanCalcGetDelays(scan, haveSpacecraftAntenna, haveSpacecraftSource, model, modelLMN, modelXYZ, D, prefix, p, verbose);
    if(v < 0)
    {
        fprintf(stderr, "Error: received code %d from scanCalcGetDelays in scanCalc\n", v);
        return -3;
    }

    /* deallocate memory */
    if((model))
	{
        for(antId = 0; antId < scan->nAntenna; ++antId)
		{
            free(model[antId]);
        }
    }
    free(model);
    if((modelLMN))
	{
        for(antId = 0; antId < scan->nAntenna; ++antId)
		{
            free(modelLMN[antId]);
        }
    }
    free(modelLMN);
    if((modelXYZ))
	{
        for(antId = 0; antId < scan->nAntenna; ++antId)
		{
            free(modelXYZ[antId]);
        }
    }
    return 0;
}

int difxCalc(DifxInput* const D, CalcParams* const p, const char *prefix, const int verbose)
{
    int scanId;
    int v;
    int isLast;
    DifxScan *scan;

    if(!D)
    {
        return -1;
    }

    for(scanId = 0; scanId < D->nScan; ++scanId)
    {
        scan = D->scan + scanId;

        if(scan->im)
        {
            fprintf(stderr, "Error: difxCalc: scan %d: model already exists\n", scanId);

            return -2;
        }
        if(scan->imLMN)
        {
            fprintf(stderr, "Error: difxCalc: scan %d: LMNmodel already exists\n", scanId);

            return -3;
        }
        if(scan->imXYZ)
        {
            fprintf(stderr, "Error: difxCalc: scan %d: XYZmodel already exists\n", scanId);

            return -4;
        }
        if(scanId == D->nScan - 1)
        {
            isLast = 1;
        }
        else
        {
            isLast = 0;
        }

		if(verbose >= 2)
		{
			fprintf(stderr, "difxCalc subroutine starting scanId=%d\n", scanId);
		}
        v = scanCalc(scanId, D, prefix, p, isLast, verbose);
        if(v < 0)
        {
            return -1;
        }
    }

    
    return 0;
}
