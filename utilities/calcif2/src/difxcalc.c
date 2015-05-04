/***************************************************************************
 *   Copyright (C) 2008-2015 by Walter Brisken & Adam Deller               *
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
#include <float.h>
#include <inttypes.h>
#include "MATHCNST.H"
#include "difxcalc.h"
#include "externaldelay.h"
#include "poly.h"
#include "difxcalcrotation.h"
#include "DiFX_Delay_Server.h"

#define MIN_NUM_EOPS 5

static struct timeval TIMEOUT = {25, 0};

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
    if(perform_lmn == PerformDirectionDerivativeNone)
    {
        if(perform_uvw == PerformDirectionDerivativeFirstDerivative)
        {
            *dp = uv;
            return num_uv;
        }
        else if(perform_uvw == PerformDirectionDerivativeFirstDerivative2)
        {
            *dp = uv2;
            return num_uv2;
        }
        *dp = none;
        return num_none;
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
    *dp = none;
    return num_none;
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
    if(perform_xyz == PerformDirectionDerivativeFirstDerivative)
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
    *dp = none;
    return num_none;
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













static int check_delayserver_operation(const DifxInput* const D, const CalcParams* const p, const int verbose)
{

    struct getDIFX_DELAY_SERVER_1_arg request_args, *p_request;
    struct getDIFX_DELAY_SERVER_1_res request_res, *p_result;
    
    double delta_dly;
    char   stnnamea[8];
    char   stnnameb[8];
    unsigned short station_ID;
    int    i, mode_failures;
    size_t station_size, source_size, EOP_size;
    void *station_mem, *source_mem, *EOP_mem;
    unsigned long calcserver_version;
    enum clnt_stat clnt_stat;

    if((verbose))
    {
        printf("Checking delay server program %s\n", delayServerTypeNames[p->delayServerType]);
    }
    mode_failures = 0;
    p_request = &request_args;
    memset(p_request, 0, sizeof(struct getDIFX_DELAY_SERVER_1_arg));
    station_size = sizeof(struct DIFX_DELAY_SERVER_1_station)*2;
    if((station_mem = malloc(station_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc station memory\n");
        exit(EXIT_FAILURE);
    }
    memset(station_mem, 0, station_size);
    p_request->Num_Stations = 2;
    p_request->station.station_len = 2;
    p_request->station.station_val = station_mem;
    source_size = sizeof(struct DIFX_DELAY_SERVER_1_source)*1;
    if((source_mem = malloc(source_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc source memory\n");
        exit(EXIT_FAILURE);
    }
    memset(source_mem, 0, source_size);
    p_request->Num_Sources = 1;
    p_request->source.source_len = 1;
    p_request->source.source_val = source_mem;
    EOP_size = sizeof(struct DIFX_DELAY_SERVER_1_EOP)*5;
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
    p_request->verbosity = verbose;
    p_request->delay_server = delayServerTypeIds[p->delayServerType];
    if(p->delayServerType == CALC_9_1_RA_Server)
    {
        p_request->server_struct_setup_code = 0x0510;
    }
    else
    {
        p_request->server_struct_setup_code = 0;
    }
    p_request->request_id = 0;
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

    if((verbose))
    {
        printf("Checking delay server at host %s type %s handler version %lu\n", p->delayServerHost, delayServerTypeNames[p->delayServerType], p->delayVersion);
    }
    p_result = &request_res;
    memset(p_result, 0, sizeof(struct getDIFX_DELAY_SERVER_1_res));
    clnt_stat = clnt_call(p->clnt, GETDIFX_DELAY_SERVER,
                          (xdrproc_t)xdr_getDIFX_DELAY_SERVER_1_arg, 
                          (caddr_t)p_request,
                          (xdrproc_t)xdr_getDIFX_DELAY_SERVER_1_res, 
                          (caddr_t)(p_result),
                          TIMEOUT);
    if((verbose))
    {
        printf("Return from clnt_call\n");
    }
    if(clnt_stat != RPC_SUCCESS)
    {
        fprintf(stderr, "clnt_call failed!\n");
        free(station_mem);
        free(source_mem);
        free(EOP_mem);
        return -1;
    }
    if(p_result->this_error)
    {
        fprintf(stderr,"Error: callCalc: %s\n", p_result->getDIFX_DELAY_SERVER_1_res_u.errmsg);
        free(station_mem);
        free(source_mem);
        free(EOP_mem);
        return -2;
    }

    calcserver_version = p_result->getDIFX_DELAY_SERVER_1_res_u.response.server_version;
    if(verbose >= 1) {
        printf("Delay Server version 0x%lX detected for type %s\n", calcserver_version, delayServerTypeNames[p->delayServerType]);
    }

    if(verbose >= 2) {
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
            struct DIFX_DELAY_SERVER_vec V;
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
    }
    /**/
    delta_dly = p_result->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[1].delay;
    delta_dly = fabs(delta_dly - (-2.04212341289221e-02));

    if ((delta_dly >= D->job->delayModelPrecision)
       || ((p_result->getDIFX_DELAY_SERVER_1_res_u.response.delay_server_error))
       || ((p_result->getDIFX_DELAY_SERVER_1_res_u.response.server_error))
       || ((p_result->getDIFX_DELAY_SERVER_1_res_u.response.model_error))
        )
    {
        fprintf(stderr, "ERROR : delay server on host %s with type %s with handler version 0x%lX is returning BAD DATA.\n", p->delayServerHost, delayServerTypeNames[p->delayServerType], p->delayVersion);
        fprintf(stderr, "        Restart the delay servers.\n");
        mode_failures++;
    }
    else
    {
        if(verbose >= 2) {
            printf ("Delay server on host %s with type %s with handler version 0x%lX is running normally.\n", p->delayServerHost, delayServerTypeNames[p->delayServerType], p->delayVersion);
        }
    }

    if((mode_failures))
    {
        fprintf (stderr, "ERROR : Delay server is returning BAD DATA.\n");
        fprintf (stderr, "        Restart the delay servers.\n");
        exit(EXIT_FAILURE);
    }

    if(clnt_freeres(p->clnt, (xdrproc_t) xdr_getDIFX_DELAY_SERVER_1_res, (caddr_t) p_result) != 1)
    {
        fprintf(stderr, "Failed to free results buffer\n");
        exit(EXIT_FAILURE);
    }
        
    free(station_mem);
    free(source_mem);
    free(EOP_mem);

    return 0;
};




static int get_Delay_Server_Parameters(DifxInput* const D, const CalcParams* const p, const int verbose)
{
    struct getDIFX_DELAY_SERVER_PARAMETERS_1_arg request_args, *p_request;
    struct getDIFX_DELAY_SERVER_PARAMETERS_1_res request_res,  *p_result;
    struct DIFX_DELAY_SERVER_PARAMETERS_1_res* res;
    int mode_failures;
    enum clnt_stat clnt_stat;

    mode_failures = 0;
    p_request = &request_args;
    memset(p_request, 0, sizeof(struct getDIFX_DELAY_SERVER_PARAMETERS_1_arg));
    p_request->verbosity = verbose;
    p_request->delay_server = delayServerTypeIds[p->delayServerType];
    if(p->delayServerType == CALC_9_1_RA_Server)
    {
        p_request->server_struct_setup_code = 0x0510;
    }
    else
    {
        p_request->server_struct_setup_code = 0;
    }
    p_request->request_id = 0;

    if((verbose))
    {
        printf("getting delay server parameters from host %s type %s handler version %lu\n", p->delayServerHost, delayServerTypeNames[p->delayServerType], p->delayVersion);
    }
    p_result = &request_res;
    memset(p_result, 0, sizeof(struct getDIFX_DELAY_SERVER_PARAMETERS_1_res));
    clnt_stat = clnt_call(p->clnt, GETDIFX_DELAY_SERVER_PARAMETERS,
                          (xdrproc_t)xdr_getDIFX_DELAY_SERVER_PARAMETERS_1_arg, 
                          (caddr_t)p_request,
                          (xdrproc_t)xdr_getDIFX_DELAY_SERVER_PARAMETERS_1_res, 
                          (caddr_t)(p_result),
                          TIMEOUT);
    if((verbose))
    {
        printf("Return from clnt_call\n");
    }
    if(clnt_stat != RPC_SUCCESS)
    {
        fprintf(stderr, "clnt_call failed!\n");
        return -1;
    }
    if(p_result->this_error)
    {
        fprintf(stderr,"Error: callCalc: %s\n", p_result->getDIFX_DELAY_SERVER_PARAMETERS_1_res_u.errmsg);
        return -2;
    }

    if(!p_result->this_error)
    {
        printf ("result: this_error = %d\n", p_result->this_error);
        res = &(p_result->getDIFX_DELAY_SERVER_PARAMETERS_1_res_u.response);
        if(verbose >= 3)
        {
            printf("results: delay_server_error=%d server_error=%d model_error=%d\n", res->delay_server_error, res->server_error, res->model_error);
            printf("results: request_id=%ld delay_server=0x%lX\n", res->request_id, res->delay_server);
            printf("results: server_struct_setup_code=0x%lX server_version=0x%lX\n", res->server_struct_setup_code, res->server_version);
            printf("results: accelgrv=%25.16E\n", res->accelgrv);
            printf("results: e_flat=%25.16E\n", res->e_flat);
            printf("results: earthrad=%25.16E\n", res->earthrad);
            printf("results: mmsems=%25.16E\n", res->mmsems);
            printf("results: ephepoc=%25.16E\n", res->ephepoc);
            printf("results: gauss=%25.16E\n", res->gauss);
            printf("results: u_grv_cn=%25.16E\n", res->u_grv_cn);
            printf("results: gmsun=%25.16E\n", res->gmsun);
            printf("results: gmmercury=%25.16E\n", res->gmmercury);
            printf("results: gmvenus=%25.16E\n", res->gmvenus);
            printf("results: gmearth=%25.16E\n", res->gmearth);
            printf("results: gmmoon=%25.16E\n", res->gmmoon);
            printf("results: gmmars=%25.16E\n", res->gmmars);
            printf("results: gmjupiter=%25.16E\n", res->gmjupiter);
            printf("results: gmsaturn=%25.16E\n", res->gmsaturn);
            printf("results: gmuranus=%25.16E\n", res->gmuranus);
            printf("results: gmneptune=%25.16E\n", res->gmneptune);
            printf("results: etidelag=%25.16E\n", res->etidelag);
            printf("results: love_h=%25.16E\n", res->love_h);
            printf("results: love_l=%25.16E\n", res->love_l);
            printf("results: pre_data=%25.16E\n", res->pre_data);
            printf("results: rel_data=%25.16E\n", res->rel_data);
            printf("results: tidalut1=%25.16E\n", res->tidalut1);
            printf("results: au=%25.16E\n", res->au);
            printf("results: tsecau=%25.16E\n", res->tsecau);
            printf("results: vlight=%25.16E\n", res->vlight);
        }
        if((fabs(res->vlight - 299792458.0) > 1E-3)
           || ((res->delay_server_error))
           || ((res->server_error))
           || ((res->model_error))
            )
        {
            printf ("ERROR : Delay Server is returning BAD PARAMETER DATA.\n");
            printf ("        Restart the Delay Server.\n");
            mode_failures++;
        }
        else
        {
            free(D->job->calcParamTable);
            D->job->calcParamTable = (DifxCalcParamTable*)malloc(sizeof(DifxCalcParamTable));
            
            D->job->calcParamTable->accelgrv    = res->accelgrv     ;
            D->job->calcParamTable->e_flat      = res->e_flat       ;
            D->job->calcParamTable->earthrad    = res->earthrad     ;
            D->job->calcParamTable->mmsems      = res->mmsems       ;
            D->job->calcParamTable->ephepoc     = res->ephepoc      ;
            D->job->calcParamTable->gauss       = res->gauss        ;
            D->job->calcParamTable->u_grv_cn    = res->u_grv_cn     ;
            D->job->calcParamTable->gmsun       = res->gmsun        ;
            D->job->calcParamTable->gmmercury   = res->gmmercury    ;
            D->job->calcParamTable->gmvenus     = res->gmvenus      ;
            D->job->calcParamTable->gmearth     = res->gmearth      ;
            D->job->calcParamTable->gmmoon      = res->gmmoon       ;
            D->job->calcParamTable->gmmars      = res->gmmars       ;
            D->job->calcParamTable->gmjupiter   = res->gmjupiter    ;
            D->job->calcParamTable->gmsaturn    = res->gmsaturn     ;
            D->job->calcParamTable->gmuranus    = res->gmuranus     ;
            D->job->calcParamTable->gmneptune   = res->gmneptune    ;
            D->job->calcParamTable->etidelag    = res->etidelag     ;
            D->job->calcParamTable->love_h      = res->love_h       ;
            D->job->calcParamTable->love_l      = res->love_l       ;
            D->job->calcParamTable->pre_data    = res->pre_data     ;
            D->job->calcParamTable->rel_data    = res->rel_data     ;
            D->job->calcParamTable->tidalut1    = res->tidalut1     ;
            D->job->calcParamTable->au          = res->au           ;
            D->job->calcParamTable->tsecau      = res->tsecau       ;
            D->job->calcParamTable->vlight      = res->vlight       ;
        }
    }
    else {
        printf ("ERROR : Delay Server is returning BAD PARAMETER DATA.\n");
        printf ("        Restart the Delay Server.\n");
        mode_failures++;
    }
        
    if(clnt_freeres(p->clnt, (xdrproc_t) xdr_getDIFX_DELAY_SERVER_PARAMETERS_1_res, (caddr_t) p_result) != 1)
    {
        fprintf(stderr, "Failed to free results buffer\n");
        exit(EXIT_FAILURE);
    }

    return mode_failures;
};





int difxCalcInit(DifxInput* const D, CalcParams* const p, const int verbose)
{
    struct getDIFX_DELAY_SERVER_1_arg *p_request;
    int i;

    static const char Earth_Center[] = "EC";
    unsigned short station_ID;
    size_t station_size, source_size, EOP_size;
    void *station_mem, *source_mem, *EOP_mem;

    i = check_delayserver_operation(D, p, verbose);
    if(i < 0)
    {
        fprintf(stderr, "Error: check_delayserver_operation not working in difxCalcInit, got return code %d\n", i);
        return -3;
    }
    i = get_Delay_Server_Parameters(D, p, verbose);
    if(i < 0)
    {
        fprintf(stderr, "Error: get_Delay_Server_Parameters not working in difxCalcInit, got return code %d\n", i);
        return -4;
    }

    p_request = &(p->request);

    memset(p_request, 0, sizeof(struct getDIFX_DELAY_SERVER_1_arg));
    p->Num_Allocated_Stations = 2;
    station_size = sizeof(struct DIFX_DELAY_SERVER_1_station)*p->Num_Allocated_Stations;
    if((station_mem = malloc(station_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc initial station memory for %u stations\n", p->Num_Allocated_Stations);
        exit(EXIT_FAILURE);
    }
    memset(station_mem, 0, station_size);
    p_request->Num_Stations = p->Num_Allocated_Stations;
    p_request->station.station_len = p->Num_Allocated_Stations;
    p_request->station.station_val = station_mem;
    p->Num_Allocated_Sources = 3;
    source_size = sizeof(struct DIFX_DELAY_SERVER_1_source)*p->Num_Allocated_Sources;
    if((source_mem = malloc(source_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc initial source memory for %u sources\n", p->Num_Allocated_Sources);
        exit(EXIT_FAILURE);
    }
    memset(source_mem, 0, source_size);
    p_request->Num_Sources = p->Num_Allocated_Sources;
    p_request->source.source_len = p->Num_Allocated_Sources;
    p_request->source.source_val = source_mem;
    p_request->Num_EOPs = D->nEOP;
    EOP_size = sizeof(struct DIFX_DELAY_SERVER_1_EOP)*p_request->Num_EOPs;
    if((EOP_mem = malloc(EOP_size)) == NULL)
    {
        fprintf(stderr, "Could not malloc initial EOP memory for %u EOPs\n", p_request->Num_EOPs);
        exit(EXIT_FAILURE);
    }
    memset(EOP_mem, 0, EOP_size);
    p_request->EOP.EOP_len = p_request->Num_EOPs;
    p_request->EOP.EOP_val = EOP_mem;

    p_request->verbosity = verbose;
    p_request->delay_server = delayServerTypeIds[p->delayServerType];
    if(p->delayServerType == CALC_9_1_RA_Server)
    {
        p_request->server_struct_setup_code = 0x0510;
    }
    else
    {
        p_request->server_struct_setup_code = 0;
    }
    p_request->request_id = 0;
    p_request->ref_frame = 0;
    
    for (i = 0; i < NUM_DIFX_DELAY_SERVER_1_KFLAGS; i++)
        p_request->kflags[i] = -1;
    p_request->sky_frequency = 10.E9;
    p_request->Use_Server_Station_Table = 0;
    p_request->Use_Server_Source_Table = 0;
    p_request->Use_Server_EOP_Table = 0;

    station_ID = (unsigned short)(Earth_Center[0]) | ((unsigned short)(Earth_Center[1]) << 8);
    difx_strlcpy(p_request->station.station_val[0].station_name, Earth_Center, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[0].antenna_name, Earth_Center, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(p_request->station.station_val[0].site_name, Earth_Center, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
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

    if(D->nEOP >= MIN_NUM_EOPS)
    {
        for(i = 0; i < D->nEOP; ++i)
        {
            p_request->EOP.EOP_val[i].EOP_time = D->eop[i].mjd;
            p_request->EOP.EOP_val[i].tai_utc  = D->eop[i].tai_utc;
            p_request->EOP.EOP_val[i].ut1_utc  = D->eop[i].ut1_utc;
            p_request->EOP.EOP_val[i].xpole    = D->eop[i].xPole;
            p_request->EOP.EOP_val[i].ypole    = D->eop[i].yPole;
        }
    }
    else
    {
        fprintf(stderr, "Not enough eop values present (%d < %d)\n", D->nEOP, MIN_NUM_EOPS);

        return -1;
    }

    /* check that eops bracket the observation */
    if(D->eop[D->nEOP-1].mjd < D->mjdStart ||
       D->eop[0].mjd          > D->mjdStop)
    {
        fprintf(stderr, "EOPs don't bracket the observation.\n");

        return -2;
    }

    /* copy the primary delay server request data struct information over to the
       special spacecraft handling request area
    */
    p->sc_request = p->request;
    p->sc_request.Num_Stations = 3;
    p->sc_request.station.station_len = 3;
    p->sc_request.station.station_val = p->sc_station;
    p->sc_request.station.station_val[0] = p->request.station.station_val[0];
    p->sc_request.station.station_val[1] = p->request.station.station_val[0];
    p->sc_request.station.station_val[2] = p->request.station.station_val[0];
    p->sc_request.Num_Sources = 1;
    p->sc_request.source.source_len = 1;
    p->sc_request.source.source_val = p->sc_source;

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


static int calcSpacecraftSourcePosition(const DifxInput* const D, DifxSpacecraft* const sc, DifxSource* source, struct getDIFX_DELAY_SERVER_1_arg *request, int spacecraftId, unsigned int sourceIndex, double delay_to_station_0)
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
        int mjd = request->date;
        double frac = request->time;
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

    request->source.source_val[sourceIndex].ra  =  RA;
    request->source.source_val[sourceIndex].dec =  Dec;
    request->source.source_val[sourceIndex].parallax = parallax;
    request->source.source_val[sourceIndex].source_pos.x = pos.X;
    request->source.source_val[sourceIndex].source_pos.y = pos.Y;
    request->source.source_val[sourceIndex].source_pos.z = pos.Z;

    /* TODO: put in calculations to find the source_pointing_dir and
       source_pointing_reference_dir
    */
    request->source.source_val[sourceIndex].source_pointing_dir.x = 0.0;
    request->source.source_val[sourceIndex].source_pointing_dir.y = 0.0;
    request->source.source_val[sourceIndex].source_pointing_dir.z = 0.0;
    request->source.source_val[sourceIndex].source_pointing_reference_dir.x = 0.0;
    request->source.source_val[sourceIndex].source_pointing_reference_dir.y = 0.0;
    request->source.source_val[sourceIndex].source_pointing_reference_dir.z = 0.0;

    if(source->sc_epoch == 0.0)
    {
        /* proper motion comes back as meters per second. */
        request->source.source_val[sourceIndex].source_vel.x = pos.dX;
        request->source.source_val[sourceIndex].source_vel.y = pos.dY;
        request->source.source_val[sourceIndex].source_vel.z = pos.dZ;
        /* TODO: return acceleration of spacecraft for better accuracy
           for some applications? */
        request->source.source_val[sourceIndex].source_acc.x = 0.0;
        request->source.source_val[sourceIndex].source_acc.y = 0.0;
        request->source.source_val[sourceIndex].source_acc.z = 0.0;
        request->source.source_val[sourceIndex].dra  = muRA;
        request->source.source_val[sourceIndex].ddec = muDec;
        request->source.source_val[sourceIndex].depoch = pos.mjd + pos.fracDay;
    }
    else
    {
        /* Fixed epoch means no motion is to be applied */
        request->source.source_val[sourceIndex].source_vel.x = 0.0;
        request->source.source_val[sourceIndex].source_vel.y = 0.0;
        request->source.source_val[sourceIndex].source_vel.z = 0.0;
        request->source.source_val[sourceIndex].source_acc.x = 0.0;
        request->source.source_val[sourceIndex].source_acc.y = 0.0;
        request->source.source_val[sourceIndex].source_acc.z = 0.0;

        request->source.source_val[sourceIndex].dra  = 0.0;
        request->source.source_val[sourceIndex].ddec = 0.0;
        request->source.source_val[sourceIndex].depoch = 0.0;
    }

    return 0;
}

static int calcSpacecraftAsPointingDirection(const DifxInput* const D, DifxSpacecraft* const sc, const double epoch, struct getDIFX_DELAY_SERVER_1_arg* const request, int spacecraftId, unsigned int antennaIndex, int calculate_own_retarded_position, double delay_to_station_0)
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
        int mjd = request->date;
        double frac = request->time;
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

    request->station.station_val[antennaIndex].station_pointing_dir.x = pos.X;
    request->station.station_val[antennaIndex].station_pointing_dir.y = pos.Y;
    request->station.station_val[antennaIndex].station_pointing_dir.z = pos.Z;
    /* TODO: implement the spacecraft reference direction */

    return 0;
}

static int calcSpacecraftAntennaPosition(const DifxInput* const D, struct getDIFX_DELAY_SERVER_1_arg* const request, int spacecraftId, unsigned int stationIndex, double baseline_delay)
{
    DifxSpacecraft *sc;
    nineVector pos;
    spacecraftAxisVectors pointing_direction, pointing_velocity;
    int r;
    int mjd = request->date;
    double frac = request->time;
    
    sc = D->spacecraft + spacecraftId;

    if((!sc->is_antenna))
    {
        fprintf(stderr, "Error: asked to perform ANTENNA position on spacecraft SOURCE\n");
        return -2;
    }
    mjd = request->date;
    frac = request->time;
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
    request->station.station_val[stationIndex].station_pos.x = pos.X;
    request->station.station_val[stationIndex].station_pos.y = pos.Y;
    request->station.station_val[stationIndex].station_pos.z = pos.Z;
    request->station.station_val[stationIndex].station_vel.x = pos.X;
    request->station.station_val[stationIndex].station_vel.y = pos.Y;
    request->station.station_val[stationIndex].station_vel.z = pos.Z;
    request->station.station_val[stationIndex].station_acc.x = pos.X;
    request->station.station_val[stationIndex].station_acc.y = pos.Y;
    request->station.station_val[stationIndex].station_acc.z = pos.Z;
        

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
    request->station.station_val[stationIndex].station_pointing_dir.x = pointing_direction.Z[0];
    request->station.station_val[stationIndex].station_pointing_dir.y = pointing_direction.Z[1];
    request->station.station_val[stationIndex].station_pointing_dir.z = pointing_direction.Z[2];
    request->station.station_val[stationIndex].station_reference_dir.x = pointing_direction.X[0];
    request->station.station_val[stationIndex].station_reference_dir.y = pointing_direction.X[1];
    request->station.station_val[stationIndex].station_reference_dir.z = pointing_direction.X[2];

    return 0;
}


#warning "This function should be removed once testing is finished"
/* /\* The function unwindAzimuth has been replaced by unwrap_deg_array_for_poly */
/*    and unwrap_array_for_poly */
/* *\/ */
/* static int unwindAzimuth(double *az, int order) */
/* { */
/*     int i; */
/*     double azmax, azmin; */

/*     azmax = azmin = az[0]; */

/*     for(i = 1; i < order; ++i) */
/*     { */
/*         if(az[i] > azmax) */
/*         { */
/*             azmax = az[i]; */
/*         } */
/*         if(az[i] < azmin) */
/*         { */
/*             azmin = az[i]; */
/*         } */
/*     } */

/*     if(fabs(azmax-azmin) > 180.0) */
/*     { */
/*         for(i = 0; i < order; ++i) */
/*         { */
/*             if(az[i] < 180.0) */
/*             { */
/*                 az[i] += 360.0; */
/*             } */
/*         } */
/*     } */

/*     return 0; */
/* } */








static int freeCalcResults(struct getDIFX_DELAY_SERVER_1_res* const results, const CalcParams* const p, const int verbose)
{
    if((verbose>=3))
    {
        printf("Calling clnt_freeres\n");
    }
    if(clnt_freeres(p->clnt, (xdrproc_t) xdr_getDIFX_DELAY_SERVER_1_res, (caddr_t) results) != 1)
    {
        fprintf(stderr, "Failed to free results buffer\n");
        return -1;
    }
    if((verbose>=3))
    {
        printf("Return from clnt_freeres\n");
    }
    return 0;
}



static int callCalc(struct getDIFX_DELAY_SERVER_1_arg* const request, struct getDIFX_DELAY_SERVER_1_res* const results, CalcParams* const p, const int verbose)
{
    static long request_id = 0;
    enum clnt_stat clnt_stat;
    request->request_id = request_id;
    if(verbose >= 3)
    {
        unsigned int s, e;
        printf("CALCIF2_REQUEST: request arg: request_id=0x%lX delay_server=0x%lX server_struct_setup_code=0x%lX\n", request->request_id, request->delay_server, request->server_struct_setup_code);
        printf("CALCIF2_REQUEST: request arg: date=%ld time=%16.12f ref_frame=%ld verbosity=%d\n", request->date, request->time, request->ref_frame, request->verbosity);
        if(verbose >= 4) {
            unsigned int k;
            for(k=0; k < NUM_DIFX_DELAY_SERVER_1_KFLAGS; k++) {
                printf("CALCIF2_REQUEST: request arg: kflag[%02u]=%hd\n", k, request->kflags[k]);
            }
        }
        printf("CALCIF2_REQUEST: request arg: sky_frequency = %E\n", request->sky_frequency);
        printf("CALCIF2_REQUEST: Station information\n");
        printf("CALCIF2_REQUEST: request arg: Use_Server_Station_Table=%d Num_Stations=%d\n", request->Use_Server_Station_Table, request->Num_Stations);
        for(s=0; s < request->Num_Stations; s++) {
            char ID0, ID1;
            struct DIFX_DELAY_SERVER_vec v;
            printf("CALCIF2_REQUEST: request arg: station=%02u station_name='%s'\n", s, request->station.station_val[s].station_name);
            printf("CALCIF2_REQUEST: request arg: station=%02u antenna_name='%s'\n", s, request->station.station_val[s].antenna_name);
            printf("CALCIF2_REQUEST: request arg: station=%02u site_name=   '%s'\n", s, request->station.station_val[s].site_name);
            ID0 = (char)(request->station.station_val[s].site_ID&0xFF);
            ID1 = (char)(request->station.station_val[s].site_ID>>8);
            printf("CALCIF2_REQUEST: request arg: station=%02u site_ID=     '%c%c' 0x%04hX\n", s, ID0, ID1, request->station.station_val[s].site_ID);
            printf("CALCIF2_REQUEST: request arg: station=%02u site_type=   '%s'\n", s, request->station.station_val[s].site_type);
            printf("CALCIF2_REQUEST: request arg: station=%02u axis_type=   '%s'\n", s, request->station.station_val[s].axis_type);
            v = request->station.station_val[s].station_pos;
            printf("CALCIF2_REQUEST: request arg: station=%02u station_pos= [%14.4f, %14.4f, %14.4f]\n", s, v.x, v.y, v.z);
            v = request->station.station_val[s].station_vel;
            printf("CALCIF2_REQUEST: request arg: station=%02u station_vel= [%14.3E, %14.3E, %14.3E]\n", s, v.x, v.y, v.z);
            v = request->station.station_val[s].station_acc;
            printf("CALCIF2_REQUEST: request arg: station=%02u station_acc= [%14.3E, %14.3E, %14.3E]\n", s, v.x, v.y, v.z);
            v = request->station.station_val[s].station_pointing_dir;
            printf("CALCIF2_REQUEST: request arg: station=%02u station_pointing_dir= [%14.9f, %14.9f, %14.9f]\n", s, v.x, v.y, v.z);
            v = request->station.station_val[s].station_reference_dir;
            printf("CALCIF2_REQUEST: request arg: station=%02u station_reference_dir= [%14.9f, %14.9f, %14.9f]\n", s, v.x, v.y, v.z);
            printf("CALCIF2_REQUEST: request arg: station=%02u station_coord_frame='%s'\n", s, request->station.station_val[s].station_coord_frame);
            printf("CALCIF2_REQUEST: request arg: station=%02u pointing_coord_frame='%s'\n", s, request->station.station_val[s].pointing_coord_frame);
            printf("CALCIF2_REQUEST: request arg: station=%02u pointing_corrections_applied=%d\n", s, request->station.station_val[s].pointing_corrections_applied);
            printf("CALCIF2_REQUEST: request arg: station=%02u station_position_delay_offset=%E\n", s, request->station.station_val[s].station_position_delay_offset);
            printf("CALCIF2_REQUEST: request arg: station=%02u axis_off=%7.4f primary_axis_wrap=%2d secondary_axis_wrap=%2d\n", s, request->station.station_val[s].axis_off, request->station.station_val[s].primary_axis_wrap, request->station.station_val[s].secondary_axis_wrap);
            printf("CALCIF2_REQUEST: request arg: station=%02u receiver_name='%s'\n", s, request->station.station_val[s].receiver_name);
            printf("CALCIF2_REQUEST: request arg: station=%02u pressure=%12.3E antenna_pressure=%12.3E temperature=%6.1f\n", s, request->station.station_val[s].pressure, request->station.station_val[s].antenna_pressure, request->station.station_val[s].temperature);
            printf("CALCIF2_REQUEST: request arg: station=%02u wind_speed=%6.1f wind_direction=%7.2f antenna_phys_temperature=%6.1f\n", s, request->station.station_val[s].wind_speed, request->station.station_val[s].wind_direction, request->station.station_val[s].antenna_phys_temperature);
        }
        printf("CALCIF2_REQUEST: Source information\n");
        printf("CALCIF2_REQUEST: request arg: Use_Server_Source_Table=%d Num_Sources=%d\n", request->Use_Server_Source_Table, request->Num_Sources);
        for(s=0; s < request->Num_Sources; s++) {
            struct DIFX_DELAY_SERVER_vec v;
            printf("CALCIF2_REQUEST: request arg: source=%02u source_name='%s'\n", s, request->source.source_val[s].source_name);
            printf("CALCIF2_REQUEST: request arg: source=%02u IAU_name=   '%s'\n", s, request->source.source_val[s].IAU_name);
            printf("CALCIF2_REQUEST: request arg: source=%02u source_type='%s'\n", s, request->source.source_val[s].source_type);
            printf("CALCIF2_REQUEST: request arg: source=%02u ra=           %20.16f\n", s, request->source.source_val[s].ra);
            printf("CALCIF2_REQUEST: request arg: source=%02u dec=          %20.16f\n", s, request->source.source_val[s].dec);
            printf("CALCIF2_REQUEST: request arg: source=%02u dra=          %20.10f\n", s, request->source.source_val[s].dra);
            printf("CALCIF2_REQUEST: request arg: source=%02u ddec=         %20.10f\n", s, request->source.source_val[s].ddec);
            printf("CALCIF2_REQUEST: request arg: source=%02u depoch=       %20.16f\n", s, request->source.source_val[s].depoch);
            printf("CALCIF2_REQUEST: request arg: source=%02u parallax=     %20.3f\n", s, request->source.source_val[s].parallax);
            printf("CALCIF2_REQUEST: request arg: source=%02u coord_frame= '%s'\n", s, request->source.source_val[s].coord_frame);
            v = request->source.source_val[s].source_pos;
            printf("CALCIF2_REQUEST: request arg: source=%02u source_pos= [%24.16E, %24.16E, %24.16E]\n", s, v.x, v.y, v.z);
            v = request->source.source_val[s].source_vel;
            printf("CALCIF2_REQUEST: request arg: source=%02u source_vel= [%24.16E, %24.16E, %24.16E]\n", s, v.x, v.y, v.z);
            v = request->source.source_val[s].source_acc;
            printf("CALCIF2_REQUEST: request arg: source=%02u source_acc= [%24.16E, %24.16E, %24.16E]\n", s, v.x, v.y, v.z);
            v = request->source.source_val[s].source_pointing_dir;
            printf("CALCIF2_REQUEST: request arg: source=%02u source_pointing_dir= [%24.16E, %24.16E, %24.16E]\n", s, v.x, v.y, v.z);
            v = request->source.source_val[s].source_pointing_reference_dir;
            printf("CALCIF2_REQUEST: request arg: source=%02u source_pointing_reference_dir= [%24.16E, %24.16E, %24.16E]\n", s, v.x, v.y, v.z);
        }
        printf("CALCIF2_REQUEST: EOP information\n");
        printf("CALCIF2_REQUEST: request arg: Use_Server_EOP_Table=%d Num_EOPs=%d\n", request->Use_Server_EOP_Table, request->Num_EOPs);
        for(e=0; e < request->Num_EOPs; e++) {
            printf("CALCIF2_REQUEST: request arg: EOP=%02u EOP_time=  %20.11f\n", e, request->EOP.EOP_val[e].EOP_time);
            printf("CALCIF2_REQUEST: request arg: EOP=%02u tai_utc=   %20.12f\n", e, request->EOP.EOP_val[e].tai_utc);
            printf("CALCIF2_REQUEST: request arg: EOP=%02u ut1_utc=   %20.12f\n", e, request->EOP.EOP_val[e].ut1_utc);
            printf("CALCIF2_REQUEST: request arg: EOP=%02u xpole=     %10.6f\n", e, request->EOP.EOP_val[e].xpole);
            printf("CALCIF2_REQUEST: request arg: EOP=%02u ypole=     %10.6f\n", e, request->EOP.EOP_val[e].ypole);
        }
    } /* if verbose >= 3 */


    memset(results, 0, sizeof(struct getDIFX_DELAY_SERVER_1_res));
    if((verbose>=2))
    {
        printf("Calling clnt_call\n");
    }
    clnt_stat = clnt_call(p->clnt, GETDIFX_DELAY_SERVER,
                          (xdrproc_t)xdr_getDIFX_DELAY_SERVER_1_arg, 
                          (caddr_t)request,
                          (xdrproc_t)xdr_getDIFX_DELAY_SERVER_1_res, 
                          (caddr_t)(results),
                          TIMEOUT);
    if((verbose>=2))
    {
        printf("Return from clnt_call\n");
    }
    if(clnt_stat != RPC_SUCCESS)
    {
        fprintf(stderr, "clnt_call failed!\n");
        return -1;
    }
    if(results->this_error)
    {
        fprintf(stderr,"Error: callCalc: %s\n", results->getDIFX_DELAY_SERVER_1_res_u.errmsg);
        return -2;
    }
    /* Sanity check */
    if((results->getDIFX_DELAY_SERVER_1_res_u.response.date != request->date)
      || (results->getDIFX_DELAY_SERVER_1_res_u.response.time != request->time))
    {
        fprintf(stderr, "Error: callCalc: response date does not match request\n");
        return -3;
    }
    else if((results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Stations != request->Num_Stations)
      || (results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources != request->Num_Sources))
    {
        fprintf(stderr, "Error: callCalc: response number stations/sources does not match request\n");
        return -4;
    }
    else if(results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_len != request->Num_Stations*request->Num_Sources)
    {
        fprintf(stderr, "Error: callCalc: response number stations/sources does not match data allocation\n");
        return -5;
    }
    else if(results->getDIFX_DELAY_SERVER_1_res_u.response.request_id != request_id)
    {
        fprintf(stderr, "Error: callCalc: response request_id (%ld) does not match expected value (%ld)\n", results->getDIFX_DELAY_SERVER_1_res_u.response.request_id, request_id);
        return -6;
    }
    else if((results->getDIFX_DELAY_SERVER_1_res_u.response.delay_server_error < 0)
           || (results->getDIFX_DELAY_SERVER_1_res_u.response.server_error < 0)
           || (results->getDIFX_DELAY_SERVER_1_res_u.response.model_error < 0)
            )
    {
            fprintf(stderr, "Error: callCalc: fatal error from delay server (%d %d %d)\n", results->getDIFX_DELAY_SERVER_1_res_u.response.delay_server_error, results->getDIFX_DELAY_SERVER_1_res_u.response.server_error, results->getDIFX_DELAY_SERVER_1_res_u.response.model_error);
        return -7;
    }
    else if(results->getDIFX_DELAY_SERVER_1_res_u.response.delay_server != request->delay_server)
    {
        fprintf(stderr, "Error: callCalc: Unknown delay server actually called, wanted 0x%lX got 0x%lX\n", request->delay_server, results->getDIFX_DELAY_SERVER_1_res_u.response.delay_server);
        return -8;
    }
    else if(results->getDIFX_DELAY_SERVER_1_res_u.response.server_struct_setup_code != request->server_struct_setup_code)
    {
        fprintf(stderr, "Error: callCalc: Unknown server_struct_setup_code, wanted 0x%lX got 0x%lX\n", request->server_struct_setup_code, results->getDIFX_DELAY_SERVER_1_res_u.response.server_struct_setup_code);
        return -9;
    }
    p->delayProgramDetailedVersion = results->getDIFX_DELAY_SERVER_1_res_u.response.server_version;
    if((verbose >= 3))
    {
        unsigned int st, so;
        printf("CALCIF2_RESULTS:Results\n");
        printf("CALCIF2_RESULTS:request res: delay_server_error=%d server_error=%d model_error=%d\n", results->getDIFX_DELAY_SERVER_1_res_u.response.delay_server_error, results->getDIFX_DELAY_SERVER_1_res_u.response.server_error, results->getDIFX_DELAY_SERVER_1_res_u.response.model_error);
        printf("CALCIF2_RESULTS:request res: request_id=%ld delay_server=0x%lX server_struct_setup_code=0x%lX\n", results->getDIFX_DELAY_SERVER_1_res_u.response.request_id, results->getDIFX_DELAY_SERVER_1_res_u.response.delay_server, results->getDIFX_DELAY_SERVER_1_res_u.response.server_struct_setup_code);
        printf("CALCIF2_RESULTS:request res: server_version=0x%lX\n", results->getDIFX_DELAY_SERVER_1_res_u.response.server_version);
        printf("CALCIF2_RESULTS:request res: date=%ld time=%.16f\n", results->getDIFX_DELAY_SERVER_1_res_u.response.date, results->getDIFX_DELAY_SERVER_1_res_u.response.time);
        for(st=0; st < results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Stations; ++st)
        {
            for(so=0; so < results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources; ++so)
            {
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u delay=%24.16E\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].delay);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u dry_atmos=%24.16E\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].dry_atmos);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u wet_atmos=%24.16E\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].wet_atmos);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u iono_atmos=%24.16E\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].iono_atmos);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u az_corr=%10.6f\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].az_corr);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u el_corr=%10.6f\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].el_corr);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u az_geom=%10.6f\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].az_geom);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u el_geom=%10.6f\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].el_geom);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u primary_axis_angle=%10.6f\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].primary_axis_angle);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u secondary_axis_angle=%10.6f\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].secondary_axis_angle);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u mount_source_angle=%10.6f\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].mount_source_angle);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u station_antenna_theta=%10.6f\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].station_antenna_theta);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u station_antenna_phi=%10.6f\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].station_antenna_phi);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u source_antenna_theta=%10.6f\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].source_antenna_theta);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u source_antenna_phi=%10.6f\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].source_antenna_phi);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u UVW = [%24.16E, %24.16E, %24.16E]\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].UVW.x, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].UVW.y, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].UVW.z);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u baselineP2000 = [%24.16E, %24.16E, %24.16E]\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineP2000.x, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineP2000.y, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineP2000.z);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u baselineV2000 = [%24.16E, %24.16E, %24.16E]\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineV2000.x, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineV2000.y, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineV2000.z);
                printf("CALCIF2_RESULTS:request res: station=%02u source=%02u baselineA2000 = [%24.16E, %24.16E, %24.16E]\n", st, so, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineA2000.x, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineA2000.y, results->getDIFX_DELAY_SERVER_1_res_u.response.result.result_val[st*results->getDIFX_DELAY_SERVER_1_res_u.response.Num_Sources+so].baselineA2000.z);
            }
        }
    } /* if verbose >= 3 */
    ++request_id;
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
            model->u[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lm] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lp]) * 0.5;
            model->v[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mm]) * 0.5;
            model->w[index] = C_LIGHT * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0];
            if(verbose >= 3)
            {
                printf("extractCalcResultsSingleSourceDerivs results\n");
                printf("    delay server UVW = %14E %14E %14E\n", model->u[index], model->v[index], model->w[index]);
            }
        }
        break;
    case PerformDirectionDerivativeFirstDerivative:
        {
            model->u[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_0] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lp]);
            model->v[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]);
            model->w[index] = C_LIGHT * dlmn[SELECT_LMN_DERIVATIVE_INDICES_0];
            /* model->u[index] = (C_LIGHT/results->delta)*(d-dx); */
            /* model->v[index] = (C_LIGHT/results->delta)*(dy-d); */
            /* model->w[index] = C_LIGHT*d; */
            if(verbose >= 3)
            {
                printf("extractCalcResultsSingleSourceDerivs results\n");
                printf("    delay server UVW = %14E %14E %14E\n", model->u[index], model->v[index], model->w[index]);
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
            modelLMN->d2Delay_dldl[index] = NAN;
            modelLMN->d2Delay_dldm[index] = NAN;
            modelLMN->d2Delay_dldn[index] = NAN;
            modelLMN->d2Delay_dmdm[index] = NAN;
            modelLMN->d2Delay_dmdn[index] = NAN;
            modelLMN->d2Delay_dndn[index] = NAN;
        }
        break;
    case PerformDirectionDerivativeFirstDerivative:
        {
            modelLMN->dDelay_dl[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Lp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]);
            modelLMN->dDelay_dm[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Mp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]);
            modelLMN->dDelay_dn[index] = lmn_mult * (dlmn[SELECT_LMN_DERIVATIVE_INDICES_Np] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]) / DIFXIO_DELTA_LMN_N_FACTOR;
            modelLMN->d2Delay_dldl[index] = NAN;
            modelLMN->d2Delay_dldm[index] = NAN;
            modelLMN->d2Delay_dldn[index] = NAN;
            modelLMN->d2Delay_dmdm[index] = NAN;
            modelLMN->d2Delay_dmdn[index] = NAN;
            modelLMN->d2Delay_dndn[index] = NAN;
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
            modelXYZ->d2Delay_dXdX[index] = NAN;
            modelXYZ->d2Delay_dXdY[index] = NAN;
            modelXYZ->d2Delay_dXdZ[index] = NAN;
            modelXYZ->d2Delay_dYdY[index] = NAN;
            modelXYZ->d2Delay_dYdZ[index] = NAN;
            modelXYZ->d2Delay_dZdZ[index] = NAN;
        }
        break;
    case PerformDirectionDerivativeFirstDerivative:
        {
            modelXYZ->dDelay_dX[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Xp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]);
            modelXYZ->dDelay_dY[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Yp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]);
            modelXYZ->dDelay_dZ[index] = xyz_mult * (dxyz[SELECT_XYZ_DERIVATIVE_INDICES_Zp] - dlmn[SELECT_LMN_DERIVATIVE_INDICES_0]);
            modelXYZ->d2Delay_dXdX[index] = NAN;
            modelXYZ->d2Delay_dXdY[index] = NAN;
            modelXYZ->d2Delay_dXdZ[index] = NAN;
            modelXYZ->d2Delay_dYdY[index] = NAN;
            modelXYZ->d2Delay_dYdZ[index] = NAN;
            modelXYZ->d2Delay_dZdZ[index] = NAN;
        }
        break;
    default:;
    }
    return 0;
}




static unsigned int extractCalcResultsSingleSource(const DifxInput* const D, const CalcParams* const p, const DifxAntenna* const antenna, const DifxSource* const source, const unsigned int antIndex, const unsigned int sourceIndex, const unsigned int timeIndex, struct modelTemp* model, struct modelTempLMN* modelLMN, struct modelTempXYZ* modelXYZ, const struct DIFX_DELAY_SERVER_1_res* const results, unsigned int* num_source_entries, const int verbose)
{
    DifxJob* job;
    DifxSpacecraft* sc;
    const struct DIFX_DELAY_SERVER_1_RESULTS* res;
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
    r_index = antIndex*results->Num_Sources + sourceIndex;
    res = &(results->result.result_val[r_index]);
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
    if(isnan(res[0].iono_atmos)) rv |= 0x40;
    if(isinf(res[0].iono_atmos)) rv |= 0x80;

    model->delay[timeIndex]    = -res[0].delay*MICROSECONDS_PER_SECOND;
    model->dry[timeIndex]      =  res[0].dry_atmos*MICROSECONDS_PER_SECOND;
    model->wet[timeIndex]      =  res[0].wet_atmos*MICROSECONDS_PER_SECOND;
    model->iono[timeIndex]     =  res[0].iono_atmos*MICROSECONDS_PER_SECOND;
    model->az[timeIndex]       =  res[0].az_geom*180.0/DIFX_PI;
    model->elgeom[timeIndex]   =  res[0].el_geom*180.0/DIFX_PI;
    model->msa[timeIndex]      =  res[0].mount_source_angle*180.0/DIFX_PI;
    model->elcorr[timeIndex]   =  res[0].el_geom*180.0/DIFX_PI;
    model->parangle[timeIndex] =  res[0].mount_source_angle*180.0/DIFX_PI;
    model->u[timeIndex]        =  res[0].UVW.x;
    model->v[timeIndex]        =  res[0].UVW.y;
    model->w[timeIndex]        =  res[0].UVW.z;
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
        printf("extractCalcResultsSingleSource results for antenna='%s' source='%s' at time MJD %ld %.16f\n", antenna->name, source->name, results->date, results->time);
        printf("    delay=%14E dry_delay=%14E wet_delay=%14E iono_delay=%14E\n", model->delay[timeIndex], model->dry[timeIndex], model->wet[timeIndex], model->iono[timeIndex]);
        printf("    delay server UVW = %14E %14E %14E\n", res[0].UVW.x, res[0].UVW.y, res[0].UVW.z);
    }

    if(((perform_uvw)) || ((perform_lmn)) || ((perform_xyz)))
    {
        switch(p->aberCorr) {
        case AberCorrExact:
            {
                for(i=0; i < SELECT_LMN_DERIVATIVE_INDICES_MAX; i++)
                {
                    if(lmn_indices[i] >= 0)
                    {
                        dlmn[i] = res[lmn_indices[i]].delay - res[lmn_indices[i]].wet_atmos - res[lmn_indices[i]].dry_atmos - res[lmn_indices[i]].iono_atmos;
                        if(isnan(dlmn[i]))     rv |= 0x100;
                        if(isinf(dlmn[i]))     rv |= 0x200;
    
                    }
                }
                for(i=0; i < SELECT_XYZ_DERIVATIVE_INDICES_MAX; i++)
                {
                    if(xyz_indices[i] >= 0)
                    {
                        dxyz[i] = res[xyz_indices[i]].delay - res[xyz_indices[i]].wet_atmos - res[xyz_indices[i]].dry_atmos - res[xyz_indices[i]].iono_atmos;
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



    

static int extractCalcResults(const DifxScan* const scan, const DifxInput* const D, const CalcParams* const p, const int timeIndex, struct modelTemp** model, struct modelTempLMN** modelLMN, struct modelTempXYZ** modelXYZ, const struct DIFX_DELAY_SERVER_1_res* results, const int verbose)
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
        antenna = D->antenna + antId;
        s_index = 0;
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
            rv = extractCalcResultsSingleSource(D, p, antenna, source, antId+1, s_index, timeIndex, &(model[antId][k]), &(modelLMN[antId][k]), &(modelXYZ[antId][k]),  results, &num_source_entries, verbose);
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

#warning "This function should be removed once testing is finished"
/* static int adjustSingleSpacecraftAntennaCalcResults_orig(struct modelTemp *mod, int index, const DifxInput *D, const struct getCALC_arg *request, int antId, int spacecraftId, const CalcParams *calcparms_orig) */
/* { */
/*     DifxAntenna* antenna; */
/*     DifxSpacecraft *sc; */
/*     int i; */
/*     int mjd_offset; */
/*     int original_date; */
/*     double original_day_fraction; */
/*     int retarded_date; */
/*     double retarded_day_fraction; */
/*     int need_new_gs_recording_delay = 0; */
/*     int store_new_gs_recording_delay = 0; */
/*     double sc_gs_delay = 0.0;          /\* Spacecraft to ground station delay, */
/*                                           in seconds *\/ */
/*     double gs_clock_offset = 0.0;      /\* in seconds *\/ */
/*     double sc_time_frame_delay = 0.0;  /\* in seconds *\/ */

/*     antenna = D->antenna + antId; */
/*     sc = D->spacecraft + spacecraftId; */

/*     if((!sc->is_antenna)) */
/*     { */
/*         fprintf(stderr, "Error: spacecraft is not antenna in adjustSpacecraftAntennaCalcResults\n"); */
/*         return -1; */
/*     } */

/*     if(sc->spacecraft_time_type == SpacecraftTimeLocal) */
/*     { */
/*         need_new_gs_recording_delay = 0; */
/*         store_new_gs_recording_delay = 0; */
/*         sc_gs_delay = sc->GS_recording_delay; */
/*         gs_clock_offset = 0.0; */
/*         original_date = request->date; */
/*         original_day_fraction = request->time; */
/*     } */
/*     else if(sc->spacecraft_time_type == SpacecraftTimeGroundReception) */
/*     { */
/*         if(sc->GS_recording_delay >= -1.0E100) */
/*         { */
/*             /\* constant delay offset already calculated, so use it *\/ */
/* #ifdef CALCIF2_DEBUG         */
/*             fprintf(stderr, "CALCIF2_DEBUG: using exisiting GS_recording_delay=%E, SC_recording_delay=%E, with target delay=%E\n", sc->GS_recording_delay, sc->SC_recording_delay, im->delay[index]); */
/* #endif /\* CALCIF2_DEBUG *\/ */
/*             need_new_gs_recording_delay = 0; */
/*             store_new_gs_recording_delay = 0; */
/*             sc_gs_delay = sc->GS_recording_delay; */
/*             gs_clock_offset = sc->GS_clock_delay; */
/*         } */
/*         else { */
/*             need_new_gs_recording_delay = 1; */
/*             store_new_gs_recording_delay = 1; */
/*         } */
/*         original_date = sc->GS_mjd_sync; */
/*         original_day_fraction = sc->GS_dayfraction_sync; */
/*         /\* processing handled below *\/ */
/*     } */
/*     else if(sc->spacecraft_time_type == SpacecraftTimeGroundClock) */
/*     { */
/*         /\* How do I deal with the spacecraft clock running off of the ground */
/*            maser, correcting for all special and general relativistic effects */
/*            for clock transfer? *\/ */
/*         fprintf(stderr, "Error: delay adjustment for spacecraft_time_type == SpacecraftTimeGroundClock is not yet implemented.  Contact your DiFX developer\n"); */
/*         return -3; */
/*         need_new_gs_recording_delay = 1; */
/*         store_new_gs_recording_delay = 0; */
/*         original_date = request->date; */
/*         original_day_fraction = request->time; */
/*         /\* processing handled below *\/ */
/*     } */
/*     else */
/*     { */
/*         /\* unknown time type *\/ */
/*         fprintf(stderr, "Error: unknown time type\n"); */
/*         return -2; */
/*     } */

/*     if((need_new_gs_recording_delay)) { */
/*         /\* Get the ground station clock offset *\/ */
/*         double x = 1.0; */
/*         double Delta_t = (original_date + original_day_fraction) - sc->GS_clockrefmjd; */
/*         Delta_t *= SEC_DAY_DBL; */
/*         for(i=0; i <= sc->GS_clockorder; i++) { */
/*             gs_clock_offset += x*sc->GS_clockcoeff[i]; */
/*             x *= Delta_t; */
/*         } */
/*         gs_clock_offset *= 1.0E-6; /\* convert from \mu s to s *\/ */
/*         gs_clock_offset += sc->GS_clock_break_fudge_sec; */
/*         /\* correct the indicated station time to the true TT time *\/ */
/*         original_day_fraction += gs_clock_offset / SEC_DAY_DBL; */
/*         mjd_offset = (int)(floor(original_day_fraction)); */
/*         original_date += mjd_offset; */
/*         original_day_fraction -= mjd_offset; */

/*         if((store_new_gs_recording_delay)) { */
/*             sc->GS_clock_delay = gs_clock_offset; */
/*         } */
/*     } */

/*     if((need_new_gs_recording_delay)) { */
/*         CalcParams p; */
/*         nineVector pos; */
/*         nineVector dir; */
/*         int r; */
/*         struct getCALC_arg sc_request; */
/*         struct getCALC_arg gs_request; */
/*         struct CalcResults sc_results; */
/*         struct CalcResults gs_results; */
/*         char sc_mount[MAX_ANTENNA_MOUNT_NAME_LENGTH]; */
/*         char gs_mount[MAX_ANTENNA_MOUNT_NAME_LENGTH]; */
/*         double pos_magnitude; */
/*         double gs_delay = 0.0;             /\* ground station to center of Earth */
/*                                               delay in seconds *\/ */
/*         double last_sc_gs_delay = -2E100;  /\* in seconds *\/ */
/*         int loop_count; */
/*         char s_name[] = "sc"; */
/*         static const int MAX_LOOP_COUNT = 10; */
        
/* #ifdef CALCIF2_DEBUG         */
/*         fprintf(stderr, "CALCIF2_DEBUG: Determining GS_recording_delay for spacecraft '%s' (%d) at %d %.6f\n", sc->name, spacecraftId, request->date, request->time); */
/* #endif /\* CALCIF2_DEBUG *\/ */
        
/*         p = *calcparms_orig; */
/*         p.delta = 0.0; */
    
/*         /\* Set up the CALC control codes. *\/ */
/*         /\* This should be copied from calcinit.f as much as possible, but turn */
/*            on topocentric corrections. *\/ */
/*         sc_request = *request; */
/* /\*     for(i = 0; i < 64; i++) *\/ */
/* /\*     { *\/ */
/* /\*         sc_request.kflags[i] = -1; *\/ */
/* /\*     } *\/ */
/* /\*     sc_request.kflags[ 0] = 1; /\\* KATMC *\\/ *\/ */
/* /\*     sc_request.kflags[52] = 2; /\\* KOCEC *\\/ *\/ */
/* /\*     sc_request.kflags[60] = 1; /\\* KPANC *\\/ *\/ */
/* /\*     sc_request.kflags[23] = 1; /\\* KUT1D *\\/ *\/ */
/* /\*     sc_request.kflags[62] = 1; /\\* KTOPC *\\/ *\/ */
/* /\*     sc_request.kflags[63] = 1; /\\* KTOPD *\\/ *\/ */
/*         gs_request = sc_request; */
    

    
/*         /\* set up the spacecraft request *\/ */
/*         sc_request.date = original_date; */
/*         sc_request.time = original_day_fraction; */
/*         sc_request.station_b = antenna->calcname; */
/*         sc_request.ra  =  0.0; */
/*         sc_request.dec =  0.0; */
/*         sc_request.dra  = 0.0; */
/*         sc_request.ddec = 0.0; */
/*         sc_request.parallax = 0.0; */
/*         sc_request.depoch = 0.0; */
/*         sc_request.pressure_b = 0.0; */
/*         sc_request.source = s_name; */
/*         /\* this is needed to get around xdr_string not coping well with const strings *\/ */
/*         strncpy(sc_mount, antennaMountTypeNames[antenna->mount], MAX_ANTENNA_MOUNT_NAME_LENGTH-1); */
/*         sc_mount[MAX_ANTENNA_MOUNT_NAME_LENGTH-1] = 0; */
/*         sc_request.axis_type_b = sc_mount; */
/*         sc_request.axis_off_b = antenna->offset[0]; */
/*         /\* Now get the spacecraft position at the true ground reception time time *\/ */
/*         r = evaluateDifxSpacecraftAntenna(sc, original_date, original_day_fraction, &pos); */
/*         if(r < 0) */
/*         { */
/*             printf("Error: %s:%d:adjustSpacecraftAntennaCalcResults: evaluateDifxSpacecraftAntenna = %d\n", __FILE__, __LINE__, r); */
/*             return -4; */
/*         } */
/*         sc_request.b_x = pos.X; */
/*         sc_request.b_y = pos.Y; */
/*         sc_request.b_z = pos.Z; */
/*         sc_request.b_dx = pos.dX; */
/*         sc_request.b_dy = pos.dY; */
/*         sc_request.b_dz = pos.dZ; */
/*         sc_request.b_ddx = pos.ddX; */
/*         sc_request.b_ddy = pos.ddY; */
/*         sc_request.b_ddz = pos.ddZ; */



/*         /\* set up the ground station request *\/ */
/*         gs_request.date = original_date; */
/*         gs_request.time = original_day_fraction; */
/*         gs_request.station_b = sc->GS_calcName; */
/*         gs_request.b_x = sc->GS_X; */
/*         gs_request.b_y = sc->GS_Y; */
/*         gs_request.b_z = sc->GS_Z; */
/*         gs_request.b_dx = 0.0; /\* for ground-based antennas, ground drift already*\/ */
/*         gs_request.b_dy = 0.0; /\* taken into account, so set the station *\/ */
/*         gs_request.b_dz = 0.0; /\* velocities to 0 *\/ */
/*         gs_request.b_ddx = 0.0; /\* for ground-based antennas, acceleration *\/ */
/*         gs_request.b_ddy = 0.0; /\* calculated from Earth rotation, so set the *\/ */
/*         gs_request.b_ddz = 0.0; /\* accelerations to 0 *\/ */
/*         gs_request.ra  =  0.0; */
/*         gs_request.dec =  0.0; */
/*         gs_request.dra  = 0.0; */
/*         gs_request.ddec = 0.0; */
/*         gs_request.parallax = 0.0; */
/*         gs_request.depoch = 0.0; */
/*         gs_request.pressure_b = 0.0; */
/*         gs_request.source = s_name; */
/*         /\* this is needed to get around xdr_string not coping well with const strings *\/ */
/*         strncpy(gs_mount, antennaMountTypeNames[sc->GS_mount], MAX_ANTENNA_MOUNT_NAME_LENGTH-1); */
/*         gs_mount[MAX_ANTENNA_MOUNT_NAME_LENGTH-1] = 0; */
/*         gs_request.axis_type_b = gs_mount; */
/*         gs_request.axis_off_b = sc->GS_offset[0]; */

/*         /\* zero out the J2000 position of the ground station in the result area */
/*            as a first order approximation *\/ */
/*         gs_results.res[0].getCALC_res_u.record.baselineP2000[0] = 0.0; */
/*         gs_results.res[0].getCALC_res_u.record.baselineP2000[1] = 0.0; */
/*         gs_results.res[0].getCALC_res_u.record.baselineP2000[2] = 0.0; */
/*         gs_results.res[0].getCALC_res_u.record.baselineV2000[0] = 0.0; */
/*         gs_results.res[0].getCALC_res_u.record.baselineV2000[1] = 0.0; */
/*         gs_results.res[0].getCALC_res_u.record.baselineV2000[2] = 0.0; */
/*         gs_results.res[0].getCALC_res_u.record.baselineA2000[0] = 0.0; */
/*         gs_results.res[0].getCALC_res_u.record.baselineA2000[1] = 0.0; */
/*         gs_results.res[0].getCALC_res_u.record.baselineA2000[2] = 0.0; */

/*         for(loop_count = 0; loop_count < MAX_LOOP_COUNT; loop_count++) */
/*         { */
/*             /\* get the spacecraft position at the instant of */
/*                transmission, at the retarded time corresponding to the */
/*                true recoding time at the ground station. */
/*             *\/ */
/*             retarded_date = original_date; */
/*             retarded_day_fraction = original_day_fraction - sc_gs_delay/SEC_DAY_DBL; */
/*             mjd_offset = (int)(floor(retarded_day_fraction)); */
/*             retarded_date += mjd_offset; */
/*             retarded_day_fraction -= mjd_offset; */
/*             r = evaluateDifxSpacecraftAntenna(sc, retarded_date, retarded_day_fraction, &pos); */
/*             if(r < 0) */
/*             { */
/*                 printf("Error: %s:%d:adjustSpacecraftAntennaCalcResults: evaluateDifxSpacecraftAntenna = %d\n", __FILE__, __LINE__, r); */
/*                 return -5; */
/*             } */
/*             /\* Call CALC with the ground station as station b, with the */
/*                spacecraft direction as the target source.  The 3-D source */
/*                vector is used rather than RA,Dec to indicate that no */
/*                annual abberation should be applied.  Note that the source */
/*                position vector is multiplied to be vary far away, so that */
/*                the direction is the same for the geocenter and the station */
/*                topocentric direction. */
/*             *\/ */
/* #ifdef CALCIF2_DEBUG         */
/*             fprintf(stderr, "CALCIF2_DEBUG: Have Spacecraft    Ground  stations at\n"); */
/*             fprintf(stderr, "CALCIF2_DEBUG: X  %15.3f %15.3f\n", pos.X, -gs_results.res[0].getCALC_res_u.record.baselineP2000[0]); */
/*             fprintf(stderr, "CALCIF2_DEBUG: Y  %15.3f %15.3f\n", pos.Y, -gs_results.res[0].getCALC_res_u.record.baselineP2000[1]); */
/*             fprintf(stderr, "CALCIF2_DEBUG: Z  %15.3f %15.3f\n", pos.Z, -gs_results.res[0].getCALC_res_u.record.baselineP2000[2]); */
/*             fprintf(stderr, "CALCIF2_DEBUG: Have Ground  stations velocities\n"); */
/*             fprintf(stderr, "CALCIF2_DEBUG: X %15.3f\n", -gs_results.res[0].getCALC_res_u.record.baselineV2000[0]); */
/*             fprintf(stderr, "CALCIF2_DEBUG: Y %15.3f\n", -gs_results.res[0].getCALC_res_u.record.baselineV2000[1]); */
/*             fprintf(stderr, "CALCIF2_DEBUG: Z %15.3f\n", -gs_results.res[0].getCALC_res_u.record.baselineV2000[2]); */
/* #endif /\* CALCIF2_DEBUG *\/ */
/*             dir.X = pos.X + gs_results.res[0].getCALC_res_u.record.baselineP2000[0]; */
/*             dir.Y = pos.Y + gs_results.res[0].getCALC_res_u.record.baselineP2000[1]; */
/*             dir.Z = pos.Z + gs_results.res[0].getCALC_res_u.record.baselineP2000[2]; */
/*             { */
/*                 double R = sqrt(dir.X*dir.X + dir.Y*dir.Y + dir.Z*dir.Z); */
/* #ifdef CALCIF2_DEBUG         */
/*                 fprintf(stderr, "CALCIF2_DEBUG: spacecraft--ground station distance is %.3f m, %E s\n", R, R/C_LIGHT); */
/* #endif /\* CALCIF2_DEBUG *\/ */
/*                 if(R > 0.0) { */
/*                     static const double parsec = 3.085678E16; /\* m *\/ */
/*                     const double multiplier = 1E16 * parsec / R; */
/*                     dir.X *= multiplier; */
/*                     dir.Y *= multiplier; */
/*                     dir.Z *= multiplier; */
/*                 } */
/*                 else { */
/*                     fprintf(stderr, "Error: the spacecraft %d ('%s') crashed into ground station ('%s') at MJD %d fracDay %f, and the observation came to an abrupt end.  Please contact your software developer so that this bug is fixed before launching future space missions.\n", spacecraftId, sc->name, sc->GS_Name, retarded_date, retarded_day_fraction); */
/*                     exit(1); */
/*                 } */
/*                 dir.dX = 0.0; */
/*                 dir.dY = 0.0; */
/*                 dir.dZ = 0.0; */
/*             } */
            
/*             gs_request.source_pos[0] = dir.X; */
/*             gs_request.source_pos[1] = dir.Y; */
/*             gs_request.source_pos[2] = dir.Z; */
/*             gs_request.source_vel[0] = dir.dX; */
/*             gs_request.source_vel[1] = dir.dY; */
/*             gs_request.source_vel[2] = dir.dZ; */
/*             gs_request.source_epoch  = original_date + original_day_fraction; */
/*             gs_request.source_parallax = 0.0; */
/*             /\* assume ground station is actually pointing at the spacecraft *\/ */
/*             gs_request.pointing_pos_b_z[0] = gs_request.source_pos[0]; */
/*             gs_request.pointing_pos_b_z[1] = gs_request.source_pos[1]; */
/*             gs_request.pointing_pos_b_z[2] = gs_request.source_pos[2]; */
/*             gs_request.pointing_vel_b_z[0] = gs_request.source_vel[0]; */
/*             gs_request.pointing_vel_b_z[1] = gs_request.source_vel[1]; */
/*             gs_request.pointing_vel_b_z[2] = gs_request.source_vel[2]; */
/*             gs_request.pointing_epoch_b  = gs_request.source_epoch; */
/*             gs_request.pointing_parallax = gs_request.source_parallax; */

/*             r = callCalc(&gs_request, &gs_results, &p); */
/*             if(r < 0) */
/*             { */
/*                 printf("Error: %s:%d:adjustSpacecraftAntennaCalcResults: callCalc = %d\n", __FILE__, __LINE__, r); */
        
/*                 return -5; */
/*             } */
/*             /\* Now call CALC with the direction of the spacecraft as seen */
/*                by the ground station as the target source direction.  CALC */
/*                is called with the retarded time. */
/*             *\/ */
/*             for(i=0; i < 3; i++) { */
/*                 sc_request.source_pos[i] = gs_request.source_pos[i]; */
/*                 sc_request.source_vel[i] = gs_request.source_vel[i]; */
/*                 sc_request.pointing_pos_b_z[i] = gs_request.pointing_pos_b_z[i]; */
/*                 sc_request.pointing_vel_b_z[i] = gs_request.pointing_vel_b_z[i]; */
/*             } */
/*             sc_request.source_epoch = retarded_date + retarded_day_fraction; */
/*             sc_request.source_parallax = 0.0; */
/*             sc_request.pointing_epoch_b = sc_request.source_epoch; */
/*             sc_request.pointing_parallax = sc_request.source_parallax; */

/*             /\* the spacecraft velocity and acceleration is fudged in order */
/*                that the abberation is the same as seen by the */
/*                ground station */
/*             *\/ */
/*             sc_request.b_dx = -gs_results.res[0].getCALC_res_u.record.baselineV2000[0]; */
/*             sc_request.b_dy = -gs_results.res[0].getCALC_res_u.record.baselineV2000[1]; */
/*             sc_request.b_dz = -gs_results.res[0].getCALC_res_u.record.baselineV2000[2]; */
/*             sc_request.b_ddx = -gs_results.res[0].getCALC_res_u.record.baselineA2000[0]; */
/*             sc_request.b_ddy = -gs_results.res[0].getCALC_res_u.record.baselineA2000[1]; */
/*             sc_request.b_ddz = -gs_results.res[0].getCALC_res_u.record.baselineA2000[2]; */
            
/*             r = callCalc(&sc_request, &sc_results, &p); */
/*             if(r < 0) */
/*             { */
/*                 printf("Error: %s:%d:adjustSpacecraftAntennaCalcResults: callCalc = %d\n", __FILE__, __LINE__, r); */
        
/*                 return -6; */
/*             } */

/*             /\* The spacecraft to ground station delay is now calculated *\/ */
/* #ifdef CALCIF2_DEBUG         */
/*             fprintf(stderr, "CALCIF2_DEBUG: for loop %d using start retarded delay=%E, found spacecraft delay=%E, ground station delay=%E\n", loop_count, last_sc_gs_delay, -sc_results.res[0].getCALC_res_u.record.delay[0], -gs_results.res[0].getCALC_res_u.record.delay[0]); */
/* #endif /\* CALCIF2_DEBUG *\/ */
/*             sc_gs_delay = -sc_results.res[0].getCALC_res_u.record.delay[0] - -gs_results.res[0].getCALC_res_u.record.delay[0]; */
/*             gs_delay = -gs_results.res[0].getCALC_res_u.record.delay[0]; */
/* #ifdef CALCIF2_DEBUG */
/*             fprintf(stderr, "CALCIF2_DEBUG: for loop %d sc_gs_delay=%E last_sc_gs_delay=%E\n", loop_count, sc_gs_delay, last_sc_gs_delay); */
/*             fprintf(stderr, "CALCIF2_DEBUG: for loop %d change from last iteration=%E, relative change=%E\n", loop_count, sc_gs_delay - last_sc_gs_delay, (sc_gs_delay - last_sc_gs_delay)/last_sc_gs_delay); */
/* #endif /\* CALCIF2_DEBUG *\/ */
/*             if(fabs(sc_gs_delay - last_sc_gs_delay) < 1E-13) */
/*             { */
/*                 /\* The delay value has chenged by less than 0.1 ps, */
/*                    which is just 0.03 mm of path difference, or */
/*                    33 times smaller than the wavelength at 300 GHz, */
/*                    and so this should be good enough for radio interferometry. */
/*                 *\/ */
/*                 break; */
/*             } */
/*             last_sc_gs_delay = sc_gs_delay; */
/*         } */
/*         if(loop_count == MAX_LOOP_COUNT) */
/*         { */
/*             fprintf(stderr, "Error: ground station delay not converging\n"); */
/*             return -7; */
/*         } */

/*         if((store_new_gs_recording_delay)) { */
/*             sc->GS_recording_delay = sc_gs_delay; */
/*         } */
/*     } */


/*     /\* Adjust the delay to account for the difference between the ground time */
/*        frame (TT) and the spacecraft (general) relativistic time frame. */
/*        The evaluateDifxSpacecraftAntennaTimeFrameOffset function returns the */
/*        differential time offset offset, such that when the center of Earth */
/*        clock reads time t, the spacecraft clock reads time t - offset.  Adjust */
/*        the model delay.  The model delay is defined such that if the wavefront */
/*        from the source arrives at the center of Earth at time t, the wavefront */
/*        should arrive at the telescope at time t - delay according to the */
/*        station clock.  Thus, accounting for the time frame difference, the */
/*        arrival at the spacecraft is seen at time t - delay - offset read */
/*        on the spacecraft clock.  Correct the requested TT time for the */
/*        delay offsets, to get the TT time at which the wavefront reaches the */
/*        spacecraft. */

/*        Note that if there is an error in the spacecraft */
/*        ephemeris clock, that error must be corrected outside of DiFX.  That */
/*        is, the ephemeris table itself should be fixed to give the */
/*        correct timestamp corresponding to the spacecraft position.  Otherwise, */
/*        yet another clock polynomial must be input in the v2d file, */
/*        and the corresponding code propagated to here. */
/*     *\/ */
/*     if((sc->spacecraft_time_type == SpacecraftTimeLocal) */
/*       || (sc->spacecraft_time_type == SpacecraftTimeGroundReception)) */
/*     { */
/*         spacecraftTimeFrameOffset offset; */
/*         int retval; */
/*         retarded_date = request->date; */
/*         retarded_day_fraction = request->time - (mod->delay[index]*1E-6)/SEC_DAY_DBL; */
/*         mjd_offset = (int)(floor(retarded_day_fraction)); */
/*         retarded_date += mjd_offset; */
/*         retarded_day_fraction -= mjd_offset; */
 
/*         retval = evaluateDifxSpacecraftAntennaTimeFrameOffset(sc, */
/*                                                               retarded_date, */
/*                                                               retarded_day_fraction, */
/*                                                              &offset); */
                 
/* #ifdef CALCIF2_DEBUG */
/*         fprintf(stderr, "CALCIF2_DEBUG: at %d %f adjustSpacecraftAntennaCalcResults got evaluateDifxSpacecraftAntennaTimeFrameOffset result %d %E s, original delay is %E s\n", request->date, request->time, retval, offset.Delta_t, im->delay[index]*1E-6); */
/* #endif /\* CALCIF2_DEBUG *\/ */
/*         if(retval < 0) */
/*         { */
/*             fprintf(stderr, "Error: recieved code %d from evaluateDifxSpacecraftAntennaTimeFrameOffset in adjustSpacecraftAntennaCalcResults\n"); */
/*             return -8; */
/*         } */
/*         mod->delay[index] += (offset.Delta_t)*1E6; */
/*     } */
/*     else if(sc->spacecraft_time_type == SpacecraftTimeGroundClock) */
/*     { */
/*         fprintf(stderr, "Error: delay adjustment for spacecraft time frame for spacecraft_time_type == SpacecraftTimeGroundClock is not yet implemented.  Contact your DiFX developer\n"); */
/*         return -9; */
/*     } */
/*     else */
/*     { */
/*         /\* unknown time type *\/ */
/*         fprintf(stderr, "Error: unknown time type (position 2)\n"); */
/*         return -2; */
/*     } */
    
/* #ifdef CALCIF2_DEBUG */
/*     fprintf(stderr, "CALCIF2_DEBUG: Have GS_recording_delay=%E s, SC_recording_delay=%E s, gs_clock_offset=%E s,  with geometric target delay=%E s\n", sc_gs_delay, sc->SC_recording_delay, gs_clock_offset, mod->delay[index]*1E-6); */
/* #endif /\* CALCIF2_DEBUG *\/ */
/*     /\* correct the delay to account for the sc_gs_delay and the */
/*        SC_recording_delay.  The *spacecraft* clock delay will be added in later */
/*        in the actual correlator section, so do not add it in here, but *do* */
/*        add in the ground station clock offset. */
/*     *\/ */
/*     mod->delay[index] -= (sc_gs_delay - gs_clock_offset + sc->SC_recording_delay)*1E6; */
/*     mod->sc_gs_delay[index] = sc_gs_delay*1E+6; */
/*     mod->gs_clock_delay[index] = -gs_clock_offset*1E+6; */

/*     return 0; */
/* } */





static int adjustSingleSpacecraftAntennaGSRecording(const DifxScan* const scan, DifxInput* const D, CalcParams* p, const DifxAntenna* const antenna, DifxSpacecraft* sc, const int spacecraftId, const struct DIFX_DELAY_SERVER_1_res* const response, int original_date, double original_day_fraction, double* const ret_sc_gs_delay, const int verbose)
{
    struct getDIFX_DELAY_SERVER_1_res results;
    struct DIFX_DELAY_SERVER_1_res* res;
    struct getDIFX_DELAY_SERVER_1_arg* sc_request;
    unsigned short station_ID;
    int r;
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
    res = &(results.getDIFX_DELAY_SERVER_1_res_u.response);

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
        fprintf(stderr, "CALCIF2_DEBUG: Determining GS_recording_delay for spacecraft '%s' (%d) at %d %.6f\n", sc->name, spacecraftId, original_date, original_day_fraction);
    }
    sc_request = &(p->sc_request);
    sc_request->date = original_date;
    sc_request->time = original_day_fraction;

    /* Antenna 0 is the Earth Center (needed for some delay servers) */
    /* Antenna 0 information has already been filled in */

    /* Antenna 1 is the spacecraft */
    /* TODO: replace GS information with the spacecraft struct with
       an actual station that gets carried around by difx_input stuff.
    */
    difx_strlcpy(sc_request->station.station_val[1].station_name, antenna->calcname, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(sc_request->station.station_val[1].antenna_name, antenna->calcname, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(sc_request->station.station_val[1].site_name, antenna->calcname, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    station_ID = (unsigned short)(antenna->calcname[0]) | ((unsigned short)(antenna->calcname[1]) << 8);
    sc_request->station.station_val[1].site_ID = station_ID;
    difx_strlcpy(sc_request->station.station_val[1].site_type, antennaSiteTypeNames[antenna->sitetype], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(sc_request->station.station_val[1].axis_type, antennaMountTypeNames[antenna->mount], DIFX_DELAY_SERVER_STATION_STRING_SIZE);

    /* Get the spacecraft position at the true ground reception time time */
    r = calcSpacecraftAntennaPosition(D, sc_request, spacecraftId, 1, 0.0);
    if(r < 0)
    {
        printf("Error: %s:%d:adjustSingleSpacecraftAntennaGSRecording: calcSpacecraftAntennaPosition = %d\n", __FILE__, __LINE__, r);
        return -2;
    }
    difx_strlcpy(sc_request->station.station_val[1].station_coord_frame, sourceCoordinateFrameTypeNames[sc->position_coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(sc_request->station.station_val[1].pointing_coord_frame, sourceCoordinateFrameTypeNames[sc->pointing_coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    sc_request->station.station_val[1].pointing_corrections_applied = 1;

    sc_request->station.station_val[1].station_position_delay_offset = 0.0;
    sc_request->station.station_val[1].axis_off = antenna->offset[0];
    /* TODO: get wrap, pressure, temperature information from
       VEX/log files?
    */
    sc_request->station.station_val[1].primary_axis_wrap = 0;
    sc_request->station.station_val[1].secondary_axis_wrap = 0;
    sc_request->station.station_val[1].receiver_name[0] = 0;
    sc_request->station.station_val[1].pressure = 0.0;
    sc_request->station.station_val[1].antenna_pressure = 0.0;
    sc_request->station.station_val[1].temperature = 0.0;
    sc_request->station.station_val[1].wind_speed = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
    sc_request->station.station_val[1].wind_direction = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
    sc_request->station.station_val[1].antenna_phys_temperature = 0.0;

    /* Antenna 2 is the ground station */
    /* TODO: replace GS information with the spacecraft struct with
       an actual station that gets carried around by difx_input stuff.
    */
    if(!sc->GS_exists)
    {
        fprintf(stderr, "Error: in adjustSingleSpacecraftAntennaGSRecording, spacecraft (id=%d name='%s') has no ground station!\n", spacecraftId, sc->name);
        return -3;
    }
    difx_strlcpy(sc_request->station.station_val[2].station_name, sc->GS_calcName, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(sc_request->station.station_val[2].antenna_name, sc->GS_calcName, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(sc_request->station.station_val[2].site_name, sc->GS_calcName, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    station_ID = (unsigned short)(sc->GS_calcName[0]) | ((unsigned short)(sc->GS_calcName[1]) << 8);
    sc_request->station.station_val[2].site_ID = station_ID;
    difx_strlcpy(sc_request->station.station_val[2].site_type, antennaSiteTypeNames[AntennaSiteFixed], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(sc_request->station.station_val[2].axis_type, antennaMountTypeNames[sc->GS_mount], DIFX_DELAY_SERVER_STATION_STRING_SIZE);


    sc_request->station.station_val[2].station_pos.x = sc->GS_X;
    sc_request->station.station_val[2].station_pos.y = sc->GS_Y;
    sc_request->station.station_val[2].station_pos.z = sc->GS_Z;
    /* for ground-based antennas, ground drift already*/
    /* taken into account, so set the station */
    /* velocities to 0 */
    sc_request->station.station_val[2].station_vel.x =  0.0;
    sc_request->station.station_val[2].station_vel.y =  0.0;
    sc_request->station.station_val[2].station_vel.z =  0.0;
    /* for ground-based antennas, acceleration */
    /* calculated from Earth rotation, so set the */
    /* accelerations to 0 */
    sc_request->station.station_val[2].station_acc.x =  0.0;
    sc_request->station.station_val[2].station_acc.y =  0.0;
    sc_request->station.station_val[2].station_acc.z =  0.0;
    /* the pointing directions will be updated below.  For now just
       blank the pointing */
    sc_request->station.station_val[2].station_pointing_dir.x = 0.0;
    sc_request->station.station_val[2].station_pointing_dir.y = 0.0;
    sc_request->station.station_val[2].station_pointing_dir.z = 0.0;

    sc_request->station.station_val[2].station_reference_dir.x =  0.0;
    sc_request->station.station_val[2].station_reference_dir.y =  0.0;
    sc_request->station.station_val[2].station_reference_dir.z =  0.0;

    /* TODO: implement ground station coordinate frame */
    difx_strlcpy(sc_request->station.station_val[2].station_coord_frame, sourceCoordinateFrameTypeNames[SourceCoordinateFrameITRF2008], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    /* TODO: implement spacecraft source --- not the spacecraft
       antenna itself --- as the source here */
    difx_strlcpy(sc_request->station.station_val[0].pointing_coord_frame, sourceCoordinateFrameTypeNames[sc->position_coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    sc_request->station.station_val[0].pointing_corrections_applied = 2;
    difx_strlcpy(sc_request->station.station_val[2].pointing_coord_frame, sourceCoordinateFrameTypeNames[sc->position_coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    sc_request->station.station_val[2].pointing_corrections_applied = 2;

    sc_request->station.station_val[2].station_position_delay_offset = 0.0;
    sc_request->station.station_val[2].axis_off = sc->GS_offset[0];
    /* TODO: get wrap, pressure, temperature information from
       VEX/log files?
    */
    sc_request->station.station_val[2].primary_axis_wrap = 0;
    sc_request->station.station_val[2].secondary_axis_wrap = 0;
    sc_request->station.station_val[2].receiver_name[0] = 0;
    sc_request->station.station_val[2].pressure = 0.0;
    sc_request->station.station_val[2].antenna_pressure = 0.0;
    sc_request->station.station_val[2].temperature = 0.0;
    sc_request->station.station_val[2].wind_speed = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
    sc_request->station.station_val[2].wind_direction = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
    sc_request->station.station_val[2].antenna_phys_temperature = 0.0;


    /* Setup source information */
    /* TODO: get source as spacecraft soruce instead of spacecraft
       antenna */
    difx_strlcpy(sc_request->source.source_val[0].source_name, sc->name, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    sc_request->source.source_val[0].IAU_name[0] = 0;
    difx_strlcpy(sc_request->source.source_val[0].source_type, "ephemeris", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    difx_strlcpy(sc_request->source.source_val[0].coord_frame, sourceCoordinateFrameTypeNames[sc->position_coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    /* get the spacecraft position at the data/time origin offset by sc_gs_delay
       seconds of offset, and put it into the pointing direction of
       station 0 (Earth center)
    */
    retarded_date = original_date;
    retarded_day_fraction = original_day_fraction - sc_gs_delay/SEC_DAY_DBL;
    offset = (int)(floor(retarded_day_fraction));
    retarded_date += offset;
    retarded_day_fraction -= offset;
    sc_request->date = retarded_date;
    sc_request->time = retarded_day_fraction;
    r = calcSpacecraftAsPointingDirection(D, sc, /* epoch */ 0.0, sc_request, spacecraftId, /* station */ 0, /* do own retardation */ 1, /* delay offset */ sc_gs_delay);
    if(r < 0)
    {
        printf("Error: %s:%d:adjustSingleSpacecraftAntennaGSRecording: calcSpacecraftAsPointingDirection = %d\n", __FILE__, __LINE__, r);
        return -4;
    }
    /* copy pointing direction to the ground station */
    sc_request->station.station_val[2].station_pointing_dir = sc_request->station.station_val[0].station_pointing_dir;
    dpos[0] = sc_request->station.station_val[0].station_pointing_dir.x;
    dpos[1] = sc_request->station.station_val[0].station_pointing_dir.y;
    dpos[2] = sc_request->station.station_val[0].station_pointing_dir.z;
    difx_Cartesian_to_RADec(dpos, &RA, &Dec, &radius);
    if(radius == 0.0)
    {
        radius = 1.0;
    }
    sc_request->source.source_val[0].ra  =  RA;
    sc_request->source.source_val[0].dec =  Dec;
    sc_request->source.source_val[0].parallax = 0.0;
    sc_request->source.source_val[0].source_pos.x = dpos[0] / radius * 1E30;
    sc_request->source.source_val[0].source_pos.y = dpos[1] / radius * 1E30;
    sc_request->source.source_val[0].source_pos.z = dpos[2] / radius * 1E30;

    /* TODO: put in calculations to find the source_pointing_dir and
       source_pointing_reference_dir
    */
    sc_request->source.source_val[0].source_pointing_dir.x = 0.0;
    sc_request->source.source_val[0].source_pointing_dir.y = 0.0;
    sc_request->source.source_val[0].source_pointing_dir.z = 0.0;
    sc_request->source.source_val[0].source_pointing_reference_dir.x = 0.0;
    sc_request->source.source_val[0].source_pointing_reference_dir.y = 0.0;
    sc_request->source.source_val[0].source_pointing_reference_dir.z = 0.0;
            
    sc_request->source.source_val[0].source_vel.x = 0.0;
    sc_request->source.source_val[0].source_vel.y = 0.0;
    sc_request->source.source_val[0].source_vel.z = 0.0;
    sc_request->source.source_val[0].source_acc.x = 0.0;
    sc_request->source.source_val[0].source_acc.y = 0.0;
    sc_request->source.source_val[0].source_acc.z = 0.0;
    sc_request->source.source_val[0].dra  = 0.0;
    sc_request->source.source_val[0].ddec = 0.0;
    sc_request->source.source_val[0].depoch = original_date + original_day_fraction;
    /* reset time to original time */
    sc_request->date = original_date;
    sc_request->time = original_day_fraction;

    r = callCalc(sc_request, &results, p, verbose);
    if(r < 0)
    {
        fprintf(stderr, "Error: fatal error %d from callCalc\n", r);
        return -5;
    }
    
    for(loop_count = 0; loop_count < MAX_LOOP_COUNT; loop_count++)
    {
        /* get the predicted delays */
        sc_delay = res->result.result_val[1].delay;
        gs_delay = res->result.result_val[2].delay;
        sc_gs_delay = sc_delay - gs_delay;
        if(verbose >= 3)
        {
            fprintf(stderr, "CALCIF2_DEBUG: have loop_count=%d delays [s] sc=%E gs=%E sc_gs=%E\n", loop_count, sc_delay, gs_delay, sc_gs_delay);
            fprintf(stderr, "CALCIF2_DEBUG: for loop %d sc_gs_delay=%E last_sc_gs_delay=%E\n", loop_count, sc_gs_delay, last_sc_gs_delay);
            fprintf(stderr, "CALCIF2_DEBUG: for loop %d change from last iteration=%E, relative change=%E\n", loop_count, sc_gs_delay - last_sc_gs_delay, (sc_gs_delay - last_sc_gs_delay)/last_sc_gs_delay);
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
        sc_request->date = retarded_date;
        sc_request->time = retarded_day_fraction;

        r = calcSpacecraftAntennaPosition(D, sc_request, spacecraftId, 1, sc_delay);
        if(r < 0)
        {
            printf("Error: adjustSingleSpacecraftAntennaGSRecording: Antenna spacecraft %d table out of time range, or not yet supported\n", spacecraftId);
            
            return -6;
        }
        /* Get the pointing direction. */
        if((strcmp(sc_request->station.station_val[1].station_coord_frame, "J2000_Earth") == 0)
          && (strcmp(sc_request->station.station_val[2].station_coord_frame, "ITRF2008") == 0))
        {
            /* use the returned baseline positions to get the coordinates of
               the ground station in the J2000 frame */
            DIFX_DELAY_SERVER_vec sc_pos = sc_request->station.station_val[1].station_pos;
            DIFX_DELAY_SERVER_vec bP = res->result.result_val[2].baselineP2000;
            DIFX_DELAY_SERVER_vec bV = res->result.result_val[2].baselineV2000;
            if(verbose >= 3)
            {
                fprintf(stderr, "CALCIF2_DEBUG: Have Spacecraft    Ground  station positions [m] at\n");
                fprintf(stderr, "CALCIF2_DEBUG: X  %15.3f %15.3f\n", sc_pos.x, -bP.x);
                fprintf(stderr, "CALCIF2_DEBUG: Y  %15.3f %15.3f\n", sc_pos.y, -bP.y);
                fprintf(stderr, "CALCIF2_DEBUG: Z  %15.3f %15.3f\n", sc_pos.z, -bP.z);
                fprintf(stderr, "CALCIF2_DEBUG: Have Ground  stations velocity [m/s]\n");
                fprintf(stderr, "CALCIF2_DEBUG: X %15.9f\n", -bV.x);
                fprintf(stderr, "CALCIF2_DEBUG: Y %15.9f\n", -bV.y);
                fprintf(stderr, "CALCIF2_DEBUG: Z %15.9f\n", -bV.z);
            }
            dpos[0] = sc_pos.x + bP.x;
            dpos[1] = sc_pos.y + bP.y;
            dpos[2] = sc_pos.z + bP.z;
            difx_Cartesian_to_RADec(dpos, &RA, &Dec, &radius);
            if(radius == 0.0)
            {
                radius = 1.0;
            }
            sc_request->source.source_val[0].ra  =  RA;
            sc_request->source.source_val[0].dec =  Dec;
            sc_request->source.source_val[0].parallax = 0.0;
            sc_request->source.source_val[0].source_pos.x = dpos[0] / radius * 1E30;
            sc_request->source.source_val[0].source_pos.y = dpos[1] / radius * 1E30;
            sc_request->source.source_val[0].source_pos.z = dpos[2] / radius * 1E30;
            sc_request->station.station_val[0].station_pointing_dir = sc_request->source.source_val[0].source_pos;
            sc_request->station.station_val[0].station_pointing_dir = sc_request->station.station_val[0].station_pointing_dir;
            /* TODO:  Is the aberration correction below actually needed? */
            /* /\* the spacecraft velocity and acceleration is fudged in order */
            /*    that the abberation is the same as seen by the */
            /*    ground station */
            /* *\/ */
            /* sc_request.b_dx = -gs_results.res[0].getCALC_res_u.record.baselineV2000[0]; */
            /* sc_request.b_dy = -gs_results.res[0].getCALC_res_u.record.baselineV2000[1]; */
            /* sc_request.b_dz = -gs_results.res[0].getCALC_res_u.record.baselineV2000[2]; */
            /* sc_request.b_ddx = -gs_results.res[0].getCALC_res_u.record.baselineA2000[0]; */
            /* sc_request.b_ddy = -gs_results.res[0].getCALC_res_u.record.baselineA2000[1]; */
            /* sc_request.b_ddz = -gs_results.res[0].getCALC_res_u.record.baselineA2000[2]; */
        }
        else if(strcmp(sc_request->station.station_val[1].station_coord_frame, sc_request->station.station_val[2].station_coord_frame) == 0)
        {
            /* Calculate directly in the specified frame */
            DIFX_DELAY_SERVER_vec sc_pos = sc_request->station.station_val[1].station_pos;
            DIFX_DELAY_SERVER_vec gs_pos = sc_request->station.station_val[2].station_pos;
            if(verbose >= 3)
            {
                fprintf(stderr, "CALCIF2_DEBUG: Have Spacecraft    Ground  station positions [m] at\n");
                fprintf(stderr, "CALCIF2_DEBUG: X  %15.3f %15.3f\n", sc_pos.x, gs_pos.x);
                fprintf(stderr, "CALCIF2_DEBUG: Y  %15.3f %15.3f\n", sc_pos.y, gs_pos.y);
                fprintf(stderr, "CALCIF2_DEBUG: Z  %15.3f %15.3f\n", sc_pos.z, gs_pos.z);
            }
            dpos[0] = sc_pos.x - gs_pos.x;
            dpos[1] = sc_pos.y - gs_pos.y;
            dpos[2] = sc_pos.z - gs_pos.z;
            difx_Cartesian_to_RADec(dpos, &RA, &Dec, &radius);
            if(radius == 0.0)
            {
                radius = 1.0;
            }
            sc_request->source.source_val[0].ra  =  RA;
            sc_request->source.source_val[0].dec =  Dec;
            sc_request->source.source_val[0].parallax = 0.0;
            sc_request->source.source_val[0].source_pos.x = dpos[0] / radius * 1E30;
            sc_request->source.source_val[0].source_pos.y = dpos[1] / radius * 1E30;
            sc_request->source.source_val[0].source_pos.z = dpos[2] / radius * 1E30;
            sc_request->station.station_val[0].station_pointing_dir = sc_request->source.source_val[0].source_pos;
            sc_request->station.station_val[0].station_pointing_dir = sc_request->station.station_val[0].station_pointing_dir;
        }
        else
        {
            fprintf(stderr, "Error: this software does not currently support the requested spacecraft operations for spacecraft coordinate frame='%s' and ground station coordinate frame='%s'.  Please contact your developer.\n", sc_request->station.station_val[1].station_coord_frame, sc_request->station.station_val[2].station_coord_frame);
            return -7;
        }
        sc_request->source.source_val[0].source_vel.x = 0.0;
        sc_request->source.source_val[0].source_vel.y = 0.0;
        sc_request->source.source_val[0].source_vel.z = 0.0;
        sc_request->source.source_val[0].source_acc.x = 0.0;
        sc_request->source.source_val[0].source_acc.y = 0.0;
        sc_request->source.source_val[0].source_acc.z = 0.0;
        sc_request->station.station_val[1].station_vel.x =  0.0;
        sc_request->station.station_val[1].station_vel.y =  0.0;
        sc_request->station.station_val[1].station_vel.z =  0.0;
        sc_request->station.station_val[1].station_acc.x =  0.0;
        sc_request->station.station_val[1].station_acc.y =  0.0;
        sc_request->station.station_val[1].station_acc.z =  0.0;

        r = freeCalcResults(&results, p, verbose);
        if(r < 0)
        {
            fprintf(stderr, "Error: %s:%d:adjustSingleSpacecraftAntennaGSRecording: freeCalcResults = %d\n", __FILE__, __LINE__, r);
        
            return -8;
        }
        
        
        r = callCalc(sc_request, &results, p, verbose);
        if(r < 0)
        {
            printf("Error: %s:%d:adjustSingleSpacecraftAntennaGSRecording: callCalc = %d\n", __FILE__, __LINE__, r);
        
            return -9;
        }
    }
    r = freeCalcResults(&results, p, verbose);
    if(r < 0)
    {
        fprintf(stderr, "Error: %s:%d:adjustSingleSpacecraftAntennaGSRecording: freeCalcResults = %d\n", __FILE__, __LINE__, r);
        
        return -10;
    }
    if(loop_count == MAX_LOOP_COUNT)
    {
        fprintf(stderr, "Error: ground station delay not converging\n");
        return -11;
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
                fprintf(stderr, "CALCIF2_DEBUG: at %d %f adjustSingleSpacecraftAntennaFrameTime got evaluateDifxSpacecraftAntennaTimeFrameOffset result %d %E s, original delay is %E s\n", original_date, original_day_fraction, r, Toffset.Delta_t, sc_delay);
            }
            if(r < 0)
            {
                fprintf(stderr, "Error: recieved code %d from evaluateDifxSpacecraftAntennaTimeFrameOffset in adjustSingleSpacecraftAntennaFrameTime\n", r);
                return -1;
            }
            *frame_time_offset = -Toffset.Delta_t;
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







static int adjustSingleSpacecraftAntennaCalcResults(const DifxScan* const scan, DifxInput* const D, CalcParams* const p, const DifxAntenna* const antenna, DifxSpacecraft* const sc, const int spacecraftId, const struct getDIFX_DELAY_SERVER_1_arg* const request, const struct DIFX_DELAY_SERVER_1_res* const response, const int verbose)
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
                fprintf(stderr, "CALCIF2_DEBUG: SpacecraftTimeGroundReception using exisiting GS_recording_delay=%E s, GS_clock_delay_sync=%E s\n", sc->GS_recording_delay, sc->GS_clock_delay_sync);
            }
        }
        else {
            sc->GS_transmission_delay = 0.0;
            sc->GS_transmission_delay_sync = 0.0;
            sc->SC_elec_delay = sc->SC_recording_delay + sc->SC_Elec_to_Comm;
            sc->GS_clock_delay = 0.0;
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
                fprintf(stderr, "CALCIF2_DEBUG: SpacecraftTimeGroundReception using new GS_recording_delay=%E s, GS_clock_delay_sync=%E s\n", sc->GS_recording_delay, sc->GS_clock_delay_sync);
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
            sc->GS_recording_delay = 0.0;
            sc->GS_transmission_delay_sync = 0.0;
            sc->SC_elec_delay = sc->SC_recording_delay; /* ??? */
            sc->GS_clock_delay_sync = 0.0;
            /* initialize using last delay offset calculated */
            if(verbose >= 3)
            {
                fprintf(stderr, "CALCIF2_DEBUG: SpacecraftTimeGroundClock using exisiting GS_transmission_delay=%E s\n", sc->GS_transmission_delay);
            }
        }
        fprintf(stderr, "Error: SpacecraftTimeGroundClock not yet implemented\n");
        exit(EXIT_FAILURE);
    }
    else if(sc->spacecraft_time_type == SpacecraftTimeGroundClockReception)
    {
        if(!isnan(sc->GS_recording_delay))
        {
            sc->GS_transmission_delay = 0.0;
            sc->GS_transmission_delay_sync = 0.0;
            sc->SC_elec_delay = sc->SC_recording_delay; /* ??? */
            sc->GS_clock_delay = 0.0;
            /* constant delay offset already calculated, so use it */
            if(verbose >= 3)
            {
                fprintf(stderr, "CALCIF2_DEBUG: SpacecraftTimeGroundClockReception using exisiting GS_recording_delay=%E s\n", sc->GS_recording_delay);
            }
        }
        else {
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








static int adjustSpacecraftAntennaCalcResults(const DifxScan* const scan, DifxInput* const D, CalcParams* const p, const struct getDIFX_DELAY_SERVER_1_arg* const request, struct DIFX_DELAY_SERVER_1_res* const response, const int verbose)
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

        r = adjustSingleSpacecraftAntennaCalcResults(scan, D, p, antenna, sc, spacecraftId, request, response, verbose);
        if(r < 0)
        {
            fprintf(stderr, "Error: received %d from adjustSingleSpacecraftAntennaCalcResults\n", r);
            return -1;
        }
        if(verbose >= 3)
        {
            fprintf(stderr, "CALCIF2_DEBUG: Have GS_recording_delay=%E s, GS_transmission_delay=%E s, GS_transmission_delay_sync=%E s, SC_elec_delay=%E s, GS_clock_delay=%E s, GS_clock_delay_sync=%E s s\n", sc->GS_recording_delay, sc->GS_transmission_delay, sc->GS_transmission_delay_sync, sc->SC_elec_delay, sc->GS_clock_delay, sc->GS_clock_delay_sync);
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
            r = adjustSingleSpacecraftAntennaFrameTime(sc, request->date, request->time, response->result.result_val[a_index+l].delay, &sc_time_frame_delay, verbose);
            if(r < 0)
            {
                fprintf(stderr, "Error: adjustSingleSpacecraftAntennaFrameTime returned %d\n", r);
                return -2;
            }
            response->result.result_val[a_index+l].delay += sc_extra_delay + sc_time_frame_delay;
        }
    }
    return 0;
}








/* Set up individual source delay derivative source information */
static unsigned int scanCalcSetupSingleSourceDerivs(unsigned int s_index, struct getDIFX_DELAY_SERVER_1_arg* const request, const DifxJob* const job, DifxSource* const source, const int verbose)
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
        pos[0] = request->source.source_val[s_index].source_pos.x;
        pos[1] = request->source.source_val[s_index].source_pos.y;
        pos[2] = request->source.source_val[s_index].source_pos.z;
        /* Remember, l goes in the direction of decreasing RA */
        /* Individual indices */
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp]] = request->source.source_val[s_index];
            difxcalc_R_Rotate_RA(pos, -theta, R);
            difxcalc_multiply_R_v(R, pos, npos);
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp]].source_pos.x = npos[0];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp]].source_pos.y = npos[1];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp]].source_pos.z = npos[2];
            
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm]] = request->source.source_val[s_index];
            difxcalc_R_Rotate_RA(pos, +theta, R);
            difxcalc_multiply_R_v(R, pos, npos);
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm]].source_pos.x = npos[0];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm]].source_pos.y = npos[1];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm]].source_pos.z = npos[2];
            
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]] = request->source.source_val[s_index];
            difxcalc_R_Rotate_Dec(pos, theta, R);
            difxcalc_multiply_R_v(R, pos, npos);
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]].source_pos.x = npos[0];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]].source_pos.y = npos[1];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]].source_pos.z = npos[2];
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]] = request->source.source_val[s_index];
            difxcalc_R_Rotate_Dec(pos, -theta, R);
            difxcalc_multiply_R_v(R, pos, npos);
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]].source_pos.x = npos[0];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]].source_pos.y = npos[1];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]].source_pos.z = npos[2];
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Np] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Np]] = request->source.source_val[s_index];
            r_factor = 1.0 + theta * DIFXIO_DELTA_LMN_N_FACTOR;
            npos[0] = pos[0] * r_factor;
            npos[1] = pos[1] * r_factor;
            npos[2] = pos[2] * r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]].source_pos.x = npos[0];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]].source_pos.y = npos[1];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]].source_pos.z = npos[2];
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Nm] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Nm]] = request->source.source_val[s_index];
            r_factor = 1.0 - theta * DIFXIO_DELTA_LMN_N_FACTOR;
            npos[0] = pos[0] * r_factor;
            npos[1] = pos[1] * r_factor;
            npos[2] = pos[2] * r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]].source_pos.x = npos[0];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]].source_pos.y = npos[1];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]].source_pos.z = npos[2];
        }
        /* LM */
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMp] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMp]] = request->source.source_val[s_index];
            difxcalc_R_Rotate_RADec(pos, theta*DIFX_ROOT_2, 135.0*DIFX_PI/180.0, R);
            difxcalc_multiply_R_v(R, pos, npos);
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMp]].source_pos.x = npos[0];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMp]].source_pos.y = npos[1];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMp]].source_pos.z = npos[2];
            
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMm] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMm]] = request->source.source_val[s_index];
            difxcalc_R_Rotate_RADec(pos, theta*DIFX_ROOT_2, -135.0*DIFX_PI/180.0, R);
            difxcalc_multiply_R_v(R, pos, npos);
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMm]].source_pos.x = npos[0];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMm]].source_pos.y = npos[1];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpMm]].source_pos.z = npos[2];
            
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMp] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMp]] = request->source.source_val[s_index];
            difxcalc_R_Rotate_RADec(pos, theta*DIFX_ROOT_2, 45.0*DIFX_PI/180.0, R);
            difxcalc_multiply_R_v(R, pos, npos);
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMp]].source_pos.x = npos[0];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMp]].source_pos.y = npos[1];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMp]].source_pos.z = npos[2];
            
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMm] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMm]] = request->source.source_val[s_index];
            difxcalc_R_Rotate_RADec(pos, theta*DIFX_ROOT_2, -45.0*DIFX_PI/180.0, R);
            difxcalc_multiply_R_v(R, pos, npos);
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMm]].source_pos.x = npos[0];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMm]].source_pos.y = npos[1];
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmMm]].source_pos.z = npos[2];
            
        }
        /* LN */
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNp] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNp]] = request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp]];
            r_factor = 1.0 + theta * DIFXIO_DELTA_LMN_N_FACTOR;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNp]].source_pos.x *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNp]].source_pos.y *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNp]].source_pos.z *= r_factor;
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNm] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNm]] = request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lp]];
            r_factor = 1.0 - theta * DIFXIO_DELTA_LMN_N_FACTOR;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNm]].source_pos.x *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNm]].source_pos.y *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LpNm]].source_pos.z *= r_factor;
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNp] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNp]] = request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm]];
            r_factor = 1.0 + theta * DIFXIO_DELTA_LMN_N_FACTOR;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNp]].source_pos.x *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNp]].source_pos.y *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNp]].source_pos.z *= r_factor;
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNm] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNm]] = request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Lm]];
            r_factor = 1.0 - theta * DIFXIO_DELTA_LMN_N_FACTOR;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNm]].source_pos.x *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNm]].source_pos.y *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_LmNm]].source_pos.z *= r_factor;
        }
        /* MN */
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNp] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNp]] = request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]];
            r_factor = 1.0 + theta * DIFXIO_DELTA_LMN_N_FACTOR;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNp]].source_pos.x *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNp]].source_pos.y *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNp]].source_pos.z *= r_factor;
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNm] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNm]] = request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mp]];
            r_factor = 1.0 - theta * DIFXIO_DELTA_LMN_N_FACTOR;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNm]].source_pos.x *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNm]].source_pos.y *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MpNm]].source_pos.z *= r_factor;
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNp] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNp]] = request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]];
            r_factor = 1.0 + theta * DIFXIO_DELTA_LMN_N_FACTOR;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNp]].source_pos.x *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNp]].source_pos.y *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNp]].source_pos.z *= r_factor;
        }
        if(lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNm] >= 0)
        {
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNm]] = request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_Mm]];
            r_factor = 1.0 - theta * DIFXIO_DELTA_LMN_N_FACTOR;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNm]].source_pos.x *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNm]].source_pos.y *= r_factor;
            request->source.source_val[s_index+lmn_indices[SELECT_LMN_DERIVATIVE_INDICES_MmNm]].source_pos.z *= r_factor;
        }
    }
    if(num_xyz > 0)
    {
        double delta = (source->delta_xyz == 0.0) ? job->delta_xyz : source->delta_xyz;
        if(delta < 0.0)
        {
            const DIFX_DELAY_SERVER_vec* tmp;
            double r;
            delta = -delta;
            tmp = &(request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]].source_pos);
            r = sqrt(tmp->x*tmp->x + tmp->y*tmp->y + tmp->z*tmp->z);
            delta = delta * r;
        }
        source->delta_xyz_used = delta;
        /* Individual indices */
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]] = request->source.source_val[s_index];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]].source_pos.x += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm]] = request->source.source_val[s_index];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm]].source_pos.x -= delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_Yp] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Yp]] = request->source.source_val[s_index];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Yp]].source_pos.y += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_Ym] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Ym]] = request->source.source_val[s_index];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Ym]].source_pos.y -= delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_Zp] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Zp]] = request->source.source_val[s_index];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Zp]].source_pos.z += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_Zm] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Zm]] = request->source.source_val[s_index];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Zm]].source_pos.z -= delta;
        }
        /* XY */
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpYp] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpYp]] = request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpYp]].source_pos.y += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpYm] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpYm]] = request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpYm]].source_pos.y -= delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYp] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYp]] = request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm]];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYp]].source_pos.y += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYm] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYm]] = request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm]];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYm]].source_pos.y -= delta;
        }
        /* XZ */
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpZp] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpZp]] = request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpZp]].source_pos.z += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpZm] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpZm]] = request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xp]];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XpZm]].source_pos.z -= delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmZp] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmZp]] = request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm]];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmYp]].source_pos.z += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmZm] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmZm]] = request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Xm]];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_XmZm]].source_pos.z -= delta;
        }
        /* YZ */
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_YpZp] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YpZp]] = request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Yp]];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YpZp]].source_pos.z += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_YpZm] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YpZm]] = request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Yp]];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YpZm]].source_pos.z -= delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_YmZp] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YmZp]] = request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Ym]];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YmZp]].source_pos.z += delta;
        }
        if(lmn_indices[SELECT_XYZ_DERIVATIVE_INDICES_YmZm] >= 0)
        {
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YmZm]] = request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_Ym]];
            request->source.source_val[s_index+num_lmn+xyz_indices[SELECT_XYZ_DERIVATIVE_INDICES_YmZm]].source_pos.z -= delta;
        }
    }

    if(spacecraftId < 0)
    {
        for(i=1; i < num_lmn+num_xyz; ++i)
        {
            double poi[DIFXCALC_3D_VEC_SIZE];
            poi[0] = -request->source.source_val[s_index+i].source_pos.x;
            poi[1] = -request->source.source_val[s_index+i].source_pos.y;
            poi[2] = -request->source.source_val[s_index+i].source_pos.z;
            difx_normalize_to_unit_vector(poi, poi);
            request->source.source_val[s_index+i].source_pointing_dir.x = poi[0];
            request->source.source_val[s_index+i].source_pointing_dir.y = poi[1];
            request->source.source_val[s_index+i].source_pointing_dir.z = poi[2];
        }
    }

    return num_lmn + num_xyz;
}















#warning "This function should be removed once testing is finished"
/* /\* antenna here is a pointer to a particular antenna object *\/ */
/* static int antennaCalc(int scanId, int antId, const DifxInput* const D, const char *prefix, CalcParams* const p, int phasecentre, int verbose) */
/* { */
/*     struct getCALC_arg *request; */
/*     struct CalcResults results; */
/*     struct modelTemp mod; */
/*     int i, j, v; */
/*     double sec, subInc; */
/*     double lastsec = -1000; */
/*     DifxPolyModel **im; */
/*     DifxScan *scan; */
/*     DifxSource *source; */
/*     DifxSource* pointing_center; */
/*     DifxAntenna *antenna; */
/*     int nInt; */
/*     int spacecraftId = -1; */
/*     int antennaspacecraftId = -1; */
/*     int pointingspacecraftId = -1; */
/*     int sourceId; */
/*     int nError = 0; */
/*     int spacecraft_delay_warning = 0;  */
/*     char mount[MAX_ANTENNA_MOUNT_NAME_LENGTH]; */
/*     char externalDelayFilename[DIFXIO_FILENAME_LENGTH]; */
/*     ExternalDelay *ed; */

/*     antenna = D->antenna + antId; */
/*     antennaspacecraftId = antenna->spacecraftId; */
/*     scan = D->scan + scanId; */
/*     im = scan->im[antId]; */
/*     nInt = scan->nPoly; */
/*     if(phasecentre == 0) // this is the pointing centre */
/*     { */
/*         sourceId = scan->pointingCentreSrc; */
/*     } */
/*     else */
/*     { */
/*         sourceId = scan->phsCentreSrcs[phasecentre-1]; */
/*     } */
/*     source = D->source + sourceId; */
/*     pointing_center = D->source + scan->pointingCentreSrc; */
/*     subInc = p->increment/(double)(D->job->polyOrder*p->oversamp); */
/*     request = &(p->request); */
/*     spacecraftId = source->spacecraftId; */
/*     pointingspacecraftId = pointing_center->spacecraftId; */

/*     /\* this is needed to get around xdr_string not coping well with const strings *\/ */
/*     strncpy(mount, antennaMountTypeNames[antenna->mount], MAX_ANTENNA_MOUNT_NAME_LENGTH-1); */
/*     mount[MAX_ANTENNA_MOUNT_NAME_LENGTH-1] = 0; */

/*     request->station_b = antenna->calcname; */
/*     request->b_x = antenna->X; */
/*     request->b_y = antenna->Y; */
/*     request->b_z = antenna->Z; */
/*     request->b_dx = 0.0; /\* for ground-based antennas, ground drift already*\/ */
/*     request->b_dy = 0.0; /\* taken into account, so set the station *\/ */
/*     request->b_dz = 0.0; /\* velocities to 0 *\/ */
/*     request->b_ddx = 0.0; /\* for ground-based antennas, acceleration *\/ */
/*     request->b_ddy = 0.0; /\* calculated from Earth rotation, so set the *\/ */
/*     request->b_ddz = 0.0; /\* accelerations to 0 *\/ */
/*     request->axis_type_b = mount; */
/*     request->axis_off_b = antenna->offset[0]; */

/*     request->source = source->name; */
/*     if(spacecraftId < 0) */
/*     { */
/*         request->ra       = source->ra; */
/*         request->dec      = source->dec; */
/*         request->dra      = source->pmRA;    */
/*         request->ddec     = source->pmDec; */
/*         request->parallax = source->parallax; */
/*         request->depoch   = source->pmEpoch; */
/*     } */
/*     else */
/*     { */
/*         request->ra       = 0.0; */
/*         request->dec      = 0.0; */
/*         request->dra      = 0.0; */
/*         request->ddec     = 0.0; */
/*         request->parallax = 0.0; */
/*         request->depoch   = 0.0; */
/*     } */
/*     for(i=0; i < 3; ++i) { */
/*         request->source_pos[i] = 0.0; */
/*         request->source_vel[i] = 0.0; */
/*         request->pointing_pos_b_x[i] = 0.0; */
/*         request->pointing_vel_b_x[i] = 0.0; */
/*         request->pointing_pos_b_y[i] = 0.0; */
/*         request->pointing_vel_b_y[i] = 0.0; */
/*         request->pointing_pos_b_z[i] = 0.0; */
/*         request->pointing_vel_b_z[i] = 0.0; */
/*     } */
/*     request->source_epoch = 0.0; */
/*     request->source_parallax = 0.0; */
/*     if(pointingspacecraftId < 0) */
/*     { */
/*         double x, y, z; */
/*         double muRA, muDec; */
/*         x = cos(pointing_center->ra) * cos(pointing_center->dec); */
/*         y = sin(pointing_center->ra) * cos(pointing_center->dec); */
/*         z = sin(pointing_center->dec); */
/*         request->pointing_pos_b_z[0] = x; */
/*         request->pointing_pos_b_z[1] = y; */
/*         request->pointing_pos_b_z[2] = z; */
/*         /\* convert proper motions to rad/s from arcsec/yr *\/ */
/*         muRA  = pointing_center->pmRA  * (M_PI/(180.0*3600.0))/ (JUL_YEAR*SEC_DAY_DBL); */
/*         muDec = pointing_center->pmDec * (M_PI/(180.0*3600.0))/ (JUL_YEAR*SEC_DAY_DBL); */
/*         x = -muRA*sin(pointing_center->ra)*cos(pointing_center->dec) */
/*             - muDec*cos(pointing_center->ra)*sin(pointing_center->dec); */
/*         y = +muRA*cos(pointing_center->ra)*cos(pointing_center->dec) */
/*             - muDec*sin(pointing_center->ra)*sin(pointing_center->dec); */
/*         z = muDec*cos(pointing_center->dec); */
/*         request->pointing_vel_b_z[0] = x; */
/*         request->pointing_vel_b_z[1] = y; */
/*         request->pointing_vel_b_z[2] = z; */
/*         request->pointing_parallax = pointing_center->parallax; */
/*         request->pointing_epoch_b  = pointing_center->pmEpoch; */
/*     } */

/*     snprintf(externalDelayFilename, DIFXIO_FILENAME_LENGTH, "%s.%s.%s.delay", prefix, antenna->name, source->name); */
/*     ed = newExternalDelay(externalDelayFilename); */
/*     if(!ed) */
/*     { */
/*         snprintf(externalDelayFilename, DIFXIO_FILENAME_LENGTH, "%s_%s.delay", antenna->name, source->name); */
/*         ed = newExternalDelay(externalDelayFilename); */
/*         if(ed) */
/*         { */
/*             fprintf(stderr, "Warning: using %s to drive delays.  This filename designator is obsolete.\n", externalDelayFilename); */
/*         } */
/*     } */

/*     for(i = 0; i < nInt; ++i) */
/*     { */
/*         double e; */

/*         request->date = im[phasecentre][i].mjd; */
/*         sec = im[phasecentre][i].sec; */
/*         for(j = 0; j <= D->job->polyOrder*p->oversamp; ++j) */
/*         { */
/*             request->time = sec/SEC_DAY_DBL; */

/*             /\* call calc if we didn't just for this time *\/ */
/*             if(fabs(lastsec - sec) > 1.0e-6) */
/*             { */
/*                 if(spacecraftId >= 0) */
/*                 { */
/*                     v = calcSpacecraftSourcePosition(D, request, spacecraftId, sourceId); */
/*                     if(v < 0) */
/*                     { */
/*                         printf("Error: antennaCalc: Spacecraft %d table out of time range\n", spacecraftId); */

                        
/*                         nError = -1; */
/*                         goto end_of_antennaCalc; */
/*                     } */
/*                 } */
/*                 if(pointingspacecraftId >= 0) */
/*                 { */
/*                     v = calcSpacecraftPhaseCenterPosition(D, request, pointingspacecraftId, scan->pointingCentreSrc); */
/*                     if(v < 0) */
/*                     { */
/*                         printf("Error: antennaCalc: calcSpacecraftPhaseCenterPosition %d table out of time range\n", pointingspacecraftId); */

/*                         nError = -1; */
/*                         goto end_of_antennaCalc; */
/*                     } */
/*                 } */
/*                 if(antennaspacecraftId >= 0) */
/*                 { */
/*                     v = calcSpacecraftAntennaPosition(D, request, antennaspacecraftId+1); */
/*                     if(v < 0) */
/*                     { */
/*                         printf("Error: antennaCalc: Antenna spacecraft %d table out of time range, or not yet supported\n", antennaspacecraftId); */

/*                         nError = -1; */
/*                         goto end_of_antennaCalc; */
/*                     } */
/*                 } */
/*                 v = callCalc(request, &results, p); */
/*                 if(v < 0) */
/*                 { */
/*                     printf("Error: antennaCalc: callCalc = %d\n", v); */
                                        
/*                     nError = -1; */
/*                     goto end_of_antennaCalc; */
/*                 } */
/*             } */

/*             /\* use result to populate tabulated values *\/ */
/*             nError += extractCalcResults(scan, D, p, j, model, modelLMN, modelXYZ, results, verbose); */

/*             if(antennaspacecraftId >= 0) */
/*             { */
/* #error "Fix this function" */
/*                 v = adjustSpacecraftAntennaCalcResults(&mod, j, D, request, antId, antennaspacecraftId, p); */
/*                 if(v < 0) */
/*                 { */
/*                     printf("Error: antennaCalc: Antenna spacecraft %d problem %d calculating recording offset\n", antennaspacecraftId, v); */
/*                     nError = -1; */
/*                     goto end_of_antennaCalc; */
/*                 } */
/*             } */

/*             /\* override delay and atmosphere values *\/ */
/*             if(ed) */
/*             { */
/*                 int v; */
/*                 double exDelay, exDry, exWet; */

/*                 v = getExternalDelay(ed, request->date+request->time, &exDelay, &exDry, &exWet); */
/*                 if(v < 0) */
/*                 { */
/*                     fprintf(stderr, "Error: antennaCalc: request for external delay from stn %s source %s at time %14.8f failed with error code %d\n", antenna->name, source->name, request->date+request->time, v); */

/*                     exit(0); */
/*                 } */

/*                 mod.delay[j] = -(exDelay+exDry+exWet)*MICROSECONDS_PER_SECOND; */
/*                 mod.dry[j] = exDry*MICROSECONDS_PER_SECOND; */
/*                 mod.wet[j] = exWet*MICROSECONDS_PER_SECOND; */
/*             } */

/*             lastsec = sec; */
/*             sec += subInc; */
/*             if(sec >= SEC_DAY_DBL) */
/*             { */
/*                 sec -= SEC_DAY_DBL; */
/*                 request->date += 1; */
/*             } */
/*         } */
/*         e = computePolyModel(&im[phasecentre][i], &mod, subInc, p->oversamp, p->interpol); */
/*         if(verbose > 0 && p->oversamp > 1) */
/*         { */
/*             printf("Scan %d antId %d poly %d : max delay interpolation error = %e us\n", scanId, antId, i, e); */
/*         } */
/*     } */


/* end_of_antennaCalc: */
/*     if(ed) */
/*     { */
/*         deleteExternalDelay(ed); */
/*         ed = 0; */
/*     } */

/*     if(nError > 0) */
/*     { */
/*         fprintf(stderr, "Error: antennaCalc: Antenna %s had %d invalid delays\n", D->antenna[antId].name, nError); */
/*     } */

/*     return nError; */
/* } */





/* Set up single standard ("star") source */
static unsigned int scanCalcSetupSingleSource(unsigned int s_index, struct getDIFX_DELAY_SERVER_1_arg* const request, const DifxJob* const job, const DifxScan* const scan, DifxSource* const source, double scan_MJD_midpoint, const DifxInput* const D, const CalcParams* const p, const int verbose)
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
    difx_strlcpy(request->source.source_val[s_index].source_name, source->name, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    /* TODO: get IAU Name from somewhere for calc11 server */
    request->source.source_val[s_index].IAU_name[0] = 0;
    difx_strlcpy(request->source.source_val[s_index].source_type, (spacecraftId < 0) ? "star": "ephemeris", DIFX_DELAY_SERVER_STATION_STRING_SIZE);
    if(spacecraftId < 0)
    {
        /* Not an ephemeris object.  Put in position information here.
           Ephemeris objects will be handled elsewhere.
         */
        difx_strlcpy(request->source.source_val[s_index].coord_frame, sourceCoordinateFrameTypeNames[source->coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
        pos_changed = convert_RA_Dec_PM_to_vector(source->ra, source->dec, source->parallax, source->pmRA, source->pmDec, 0.0, source->pmEpoch, scan_MJD_midpoint, pos, vel, ref_dir);
        if((pos_changed))
        {
            double ra, dec, r, parallax;
            difx_Cartesian_to_RADec(pos, &ra, &dec, &r);
            parallax = atan(r/ASTR_UNIT_IAU_2012) / DIFX_PI * 180.0*3600.0;
            request->source.source_val[s_index].ra = ra;
            request->source.source_val[s_index].dec = dec;
            request->source.source_val[s_index].dra = 0.0;
            request->source.source_val[s_index].ddec = 0.0;
            request->source.source_val[s_index].depoch = 0.0;
            request->source.source_val[s_index].parallax = parallax;
        }
        else
        {
            request->source.source_val[s_index].ra = source->ra;
            request->source.source_val[s_index].dec = source->dec;
            request->source.source_val[s_index].dra = 0.0;
            request->source.source_val[s_index].ddec = 0.0;
            request->source.source_val[s_index].depoch = 0.0;
            request->source.source_val[s_index].parallax = source->parallax;
        }
        request->source.source_val[s_index].source_pos.x = pos[0];
        request->source.source_val[s_index].source_pos.y = pos[1];
        request->source.source_val[s_index].source_pos.z = pos[2];
        request->source.source_val[s_index].source_vel.x = vel[0];
        request->source.source_val[s_index].source_vel.y = vel[1];
        request->source.source_val[s_index].source_vel.z = vel[2];
        request->source.source_val[s_index].source_acc.x = 0.0;
        request->source.source_val[s_index].source_acc.y = 0.0;
        request->source.source_val[s_index].source_acc.z = 0.0;
        request->source.source_val[s_index].source_pointing_reference_dir.x = ref_dir[0];
        request->source.source_val[s_index].source_pointing_reference_dir.y = ref_dir[1];
        request->source.source_val[s_index].source_pointing_reference_dir.z = ref_dir[2];
        point_dir[0] = -pos[0];
        point_dir[1] = -pos[1];
        point_dir[1] = -pos[2];
        difx_normalize_to_unit_vector(point_dir, point_dir);
        request->source.source_val[s_index].source_pointing_dir.x = point_dir[0];
        request->source.source_val[s_index].source_pointing_dir.y = point_dir[1];
        request->source.source_val[s_index].source_pointing_dir.z = point_dir[2];

        num_source_entries = scanCalcSetupSingleSourceDerivs(s_index, request, job, source, verbose);
    }
    else
    {
        DifxSpacecraft* sc;
        sc = D->spacecraft + spacecraftId;

        difx_strlcpy(request->source.source_val[s_index].coord_frame, sourceCoordinateFrameTypeNames[sc->position_coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
        num_source_entries = calculateSourceIndexOffset(job, source);
    }

    return num_source_entries;
}










/* Set up single spacecraft source at specific time step*/
static int scanCalcSetupSingleSpacecraftSource(unsigned int s_index, struct getDIFX_DELAY_SERVER_1_arg* const request, const DifxJob* const job, const DifxScan* const scan, DifxSource* const source, DifxInput* const D, const CalcParams* const p, const int verbose)
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
    r = calcSpacecraftSourcePosition(D, sc, source, request, spacecraftId, s_index, source->station0PropDelay);
    if(r < 0)
    {
        printf("Error: scanCalcSetupSingleSpacecraftSource: Spacecraft %d table out of time range with code %d\n", spacecraftId, r);
        return -2;
    }

    num_source_entries = scanCalcSetupSingleSourceDerivs(s_index, request, job, source, verbose);
    return (int)num_source_entries;
}




/* Set up all standard ("star") sources */
static int scanCalcSetupSources(const DifxScan* const scan, const DifxInput* const D, CalcParams* const p, double scan_MJD_midpoint, const int verbose)
{
    const DifxJob* job;
    struct getDIFX_DELAY_SERVER_1_arg *request;
    int k;
    DifxSource *source;
    int sourceId;
    unsigned int Num_Request_Sources;
    size_t source_size;
    void* source_mem;
    unsigned int s_index;

    job = D->job;
    request = &(p->request);
    
    /* Figure out how many request sources need to be present in the request. */
    Num_Request_Sources = 0;
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
        Num_Request_Sources += calculateSourceIndexOffset(job, source);
    }
    /* check memory allocation */
    if(Num_Request_Sources > p->Num_Allocated_Sources)
    {
        p->Num_Allocated_Sources = Num_Request_Sources;
        source_size = sizeof(struct DIFX_DELAY_SERVER_1_source)*p->Num_Allocated_Sources;
        if((source_mem = realloc(request->source.source_val, source_size)) == NULL)
        {
            fprintf(stderr, "Could not realloc source memory for %u sources\n", p->Num_Allocated_Sources);
            exit(EXIT_FAILURE);
        }
        request->source.source_val = source_mem;
    }
    request->Num_Sources = Num_Request_Sources;
    request->station.station_len = request->Num_Sources;


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
        num_source_entries = scanCalcSetupSingleSource(s_index, request, job, scan, source, scan_MJD_midpoint, D, p, verbose);
        s_index += num_source_entries;
    }

    return 0;
}







/* Set up all spacecraft sources at each time step*/
static int scanCalcSetupSpacecraftSources(const DifxScan* const scan, const DifxJob* const job, DifxInput* const D, CalcParams* const p, const int verbose)
{
    struct getDIFX_DELAY_SERVER_1_arg *request;
    int k;
    DifxSource *source;
    int spacecraftId = -1;
    int sourceId;
    unsigned int s_index;

    s_index = 0;
    request = &(p->request);
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
            num_source_entries = scanCalcSetupSingleSpacecraftSource(s_index, request, job, scan, source, D, p, verbose);
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
    struct getDIFX_DELAY_SERVER_1_arg *request;
    DifxSource* pointing_center;
    DifxAntenna *antenna;
    int antId;
    int spacecraftId;
    size_t station_size;
    void* station_mem;
    /* source direction, velocity, and reference direction */
    double ps_pos[DIFXCALC_3D_VEC_SIZE], ps_vel[DIFXCALC_3D_VEC_SIZE], ps_ref[DIFXCALC_3D_VEC_SIZE];
            

    request = &(p->request);

    /* check station allocation size */
    if(scan->nAntenna + 1 > p->Num_Allocated_Stations)
    {
        p->Num_Allocated_Stations = scan->nAntenna + 1;
        station_size = sizeof(struct DIFX_DELAY_SERVER_1_station)*p->Num_Allocated_Stations;
        if((station_mem = realloc(request->station.station_val, station_size)) == NULL)
        {
            fprintf(stderr, "Could not realloc initial station memory for %u stations\n", p->Num_Allocated_Stations);
            exit(EXIT_FAILURE);
        }
        request->station.station_val = station_mem;
    }
    request->Num_Stations = scan->nAntenna + 1;
    request->station.station_len = request->Num_Stations;

    /* Where are the antennas pointed? */
    pointing_center = D->source + scan->pointingCentreSrc;

    convert_RA_Dec_PM_to_vector(pointing_center->ra, pointing_center->dec, pointing_center->parallax, pointing_center->pmRA, pointing_center->pmDec, 0.0, pointing_center->pmEpoch, scan_MJD_midpoint, ps_pos, ps_vel, ps_ref);



    
    for(antId = 0; antId < scan->nAntenna; ++antId)
    {
        unsigned short station_ID;
        antenna = D->antenna + antId;
        spacecraftId = antenna->spacecraftId;

        difx_strlcpy(request->station.station_val[1+antId].station_name, antenna->calcname, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
        difx_strlcpy(request->station.station_val[1+antId].antenna_name, antenna->calcname, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
        difx_strlcpy(request->station.station_val[1+antId].site_name, antenna->calcname, DIFX_DELAY_SERVER_STATION_STRING_SIZE);
        station_ID = (unsigned short)(antenna->calcname[0]) | ((unsigned short)(antenna->calcname[1]) << 8);
        request->station.station_val[1+antId].site_ID = station_ID;
        difx_strlcpy(request->station.station_val[1+antId].site_type, antennaSiteTypeNames[antenna->sitetype], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
        difx_strlcpy(request->station.station_val[1+antId].axis_type, antennaMountTypeNames[antenna->mount], DIFX_DELAY_SERVER_STATION_STRING_SIZE);


        request->station.station_val[1+antId].station_pos.x = antenna->X;
        request->station.station_val[1+antId].station_pos.y = antenna->Y;
        request->station.station_val[1+antId].station_pos.z = antenna->Z;
        /* for ground-based antennas, ground drift already*/
        /* taken into account, so set the station */
        /* velocities to 0 */
        request->station.station_val[1+antId].station_vel.x =  0.0;
        request->station.station_val[1+antId].station_vel.y =  0.0;
        request->station.station_val[1+antId].station_vel.z =  0.0;
        /* for ground-based antennas, acceleration */
        /* calculated from Earth rotation, so set the */
        /* accelerations to 0 */
        request->station.station_val[1+antId].station_acc.x =  0.0;
        request->station.station_val[1+antId].station_acc.y =  0.0;
        request->station.station_val[1+antId].station_acc.z =  0.0;

        request->station.station_val[1+antId].station_pointing_dir.x = ps_pos[0];
        request->station.station_val[1+antId].station_pointing_dir.y = ps_pos[1];
        request->station.station_val[1+antId].station_pointing_dir.z = ps_pos[2];

        request->station.station_val[1+antId].station_reference_dir.x =  0.0;
        request->station.station_val[1+antId].station_reference_dir.y =  0.0;
        request->station.station_val[1+antId].station_reference_dir.z =  0.0;
        if(spacecraftId < 0)
        {
            difx_strlcpy(request->station.station_val[1+antId].station_coord_frame, sourceCoordinateFrameTypeNames[antenna->site_coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
            if(pointing_center->spacecraftId < 0)
            {
                difx_strlcpy(request->station.station_val[1+antId].pointing_coord_frame, sourceCoordinateFrameTypeNames[pointing_center->coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
            }
            else
            {
                const DifxSpacecraft* sc;
                sc = D->spacecraft + pointing_center->spacecraftId;
                difx_strlcpy(request->station.station_val[1+antId].pointing_coord_frame, sourceCoordinateFrameTypeNames[sc->position_coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
            }
            request->station.station_val[1+antId].pointing_corrections_applied = 2;
        }
        else
        {
            const DifxSpacecraft* sc;
            sc = D->spacecraft + spacecraftId;

            difx_strlcpy(request->station.station_val[1+antId].station_coord_frame, sourceCoordinateFrameTypeNames[sc->position_coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
            difx_strlcpy(request->station.station_val[1+antId].pointing_coord_frame, sourceCoordinateFrameTypeNames[sc->pointing_coord_frame], DIFX_DELAY_SERVER_STATION_STRING_SIZE);
            request->station.station_val[1+antId].pointing_corrections_applied = 1;
        }
        request->station.station_val[1+antId].station_position_delay_offset = 0.0;
        request->station.station_val[1+antId].axis_off = antenna->offset[0];
        /* TODO: get wrap, pressure, temperature information from
           VEX/log files?
        */
        request->station.station_val[1+antId].primary_axis_wrap = 0;
        request->station.station_val[1+antId].secondary_axis_wrap = 0;
        request->station.station_val[1+antId].receiver_name[0] = 0;
        request->station.station_val[1+antId].pressure = 0.0;
        request->station.station_val[1+antId].antenna_pressure = 0.0;
        request->station.station_val[1+antId].temperature = 0.0;
        request->station.station_val[1+antId].wind_speed = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
        request->station.station_val[1+antId].wind_direction = DIFX_DELAY_SERVER_1_MISSING_GENERAL_DATA;
        request->station.station_val[1+antId].antenna_phys_temperature = 0.0;
    } /* for antId over antennas */
        

    return 0;
}

/* Setup the delay server argument area for the spacecraft stations. */
/* The evaluation time information should already be inside p->request. */
static int scanCalcSetupSpacecraftStations(const DifxScan* const scan, const DifxInput* const D, const char *prefix, CalcParams* const p, const int verbose)
{
    struct getDIFX_DELAY_SERVER_1_arg *request;
    DifxAntenna *antenna;
    int antId;
    int r;
    int spacecraftId;

    request = &(p->request);

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
            r = calcSpacecraftAntennaPosition(D, request, spacecraftId, antId+1, 0.0);
            if(r < 0)
            {
                printf("Error: scanCalcSetupSpacecraftStations: Antenna spacecraft %d table out of time range, or not yet supported (code=%d)\n", spacecraftId, r);
                
                return -1;
            }
        }
    }
    return 0;
}







static void scanCalcSourcePropDelayFill(const DifxScan* const scan, DifxInput* const D, const struct DIFX_DELAY_SERVER_1_res* const response, const int verbose)
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
        source->station0PropDelay = response->result.result_val[s_index].delay;
        N = calculateSourceIndexOffset(D->job, source);
        s_index += N;
    }
    return;
}
static void scanCalcSourceLastDelaysInitialize(const struct DIFX_DELAY_SERVER_1_res* const response, double* const last_delays, const int verbose)
{
    unsigned int N;
    unsigned int i;
    N = response->Num_Stations*response->Num_Sources;
    for(i=0; i < N; ++i)
    {
        last_delays[i] = response->result.result_val[i].delay;
    }
    return;
}
static double scanCalcSourceLastDelaysCompare(const DifxScan* const scan, const DifxInput* const D, const CalcParams* const p, const struct DIFX_DELAY_SERVER_1_res* const response, double* const last_delay, const int verbose)
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
                    double diff = response->result.result_val[a_index+s_index+l].delay - last_delay[a_index+s_index+l];
                    diff = fabs(diff);
                    last_delay[a_index+s_index+l] = response->result.result_val[a_index+s_index+l].delay;
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
                double diff = response->result.result_val[a_index+l].delay - last_delay[a_index+l];
                diff = fabs(diff);
                last_delay[a_index+l] = response->result.result_val[a_index+l].delay;
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
    struct getDIFX_DELAY_SERVER_1_arg* request;
    struct getDIFX_DELAY_SERVER_1_res results;
    struct DIFX_DELAY_SERVER_1_res* response;
    int i, j, v;
    int antId;
    int k;
    double sec, subInc;
    double lastsec = -1000;
    double* last_delay = NULL;
    DifxSource *source;
    DifxAntenna *antenna;
    const DifxJob* job;
    int sourceId;
    int needToFreeResultsCall = 0;
    int nError = 0;
    double max_error;
    int loop_count;
    const int MAX_LOOP_COUNT = 10;
    char externalDelayFilename[DIFXIO_FILENAME_LENGTH];
    ExternalDelay ***ed = NULL;

    job = D->job;
    request = &(p->request);
    response = &(results.getDIFX_DELAY_SERVER_1_res_u.response);
    subInc = p->increment/(double)(D->job->polyOrder*p->oversamp);

    if((haveSpacecraftSource))
    {
        last_delay = (double*)malloc(sizeof(double)*request->Num_Stations*request->Num_Sources);
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
        request->date = scan->im[0][0][i].mjd;
        sec = scan->im[0][0][i].sec;
        for(j = 0; j <= D->job->polyOrder*p->oversamp; ++j)
        {
            request->time = sec/SEC_DAY_DBL;

            /* call calc if we didn't just for this time */
            if(fabs(lastsec - sec) > 1.0e-6)
            {
                /* Update all station data for the request time */
                v = scanCalcSetupSpacecraftStations(scan, D, prefix, p, verbose);
                if(v < 0)
                {
                    fprintf(stderr, "Error: got return code %d from scanCalcSetupSpacecraftStations in scanCalcGetDelays\n", v);
                    nError = -1;
                    goto end_of_scanCalcGetDelays;
                }
                /* Update all sources for the request time */
                v = scanCalcSetupSpacecraftSources(scan, job, D, p, verbose);
                if(v < 0)
                {
                    fprintf(stderr, "Error: got return code %d from scanCalcSetupSpacecraftSources in scanCalcGetDelays\n", v);
                    nError = -2;
                    goto end_of_scanCalcGetDelays;
                }
                if((needToFreeResultsCall))
                {
                    v = freeCalcResults(&results, p, verbose);
                    if(v < 0)
                    {
                        fprintf(stderr, "Error: scanCalcGetDelays: freeCalcResults = %d\n", v);         
                        nError = -3;
                        goto end_of_scanCalcGetDelays;
                    }
                    needToFreeResultsCall = 0;
                }
                v = callCalc(request, &results, p, verbose);
                if(v < 0)
                {
                    printf("Error: scanCalcGetDelays: callCalc = %d\n", v);
                                        
                    nError = -4;
                    goto end_of_scanCalcGetDelays;
                }
                needToFreeResultsCall = 1;
                scanCalcSourcePropDelayFill(scan, D, response, verbose);
                if(((haveSpacecraftSource)) && ((D->job->calculate_own_retarded_position)))
                {
                    scanCalcSourceLastDelaysInitialize(response, last_delay, verbose);
                    max_error = DBL_MAX;
                    for(loop_count = 0; (max_error > D->job->delayModelPrecision) && (loop_count < MAX_LOOP_COUNT); ++loop_count)
                    {
                        /* Update all sources for the request time */
                        v = scanCalcSetupSpacecraftSources(scan, job, D, p, verbose);
                        if(v < 0)
                        {
                            fprintf(stderr, "Error: got return code %d from scanCalcSetupSpacecraftSources in scanCalcGetDelays\n", v);
                            nError = -5;
                            goto end_of_scanCalcGetDelays;
                        }
                        if((needToFreeResultsCall))
                        {
                            v = freeCalcResults(&results, p, verbose);
                            if(v < 0)
                            {
                                fprintf(stderr, "Error: scanCalcGetDelays: freeCalcResults = %d\n", v);         
                                nError = -6;
                                goto end_of_scanCalcGetDelays;
                            }
                            needToFreeResultsCall = 0;
                        }
                        v = callCalc(request, &results, p, verbose);
                        if(v < 0)
                        {
                            printf("Error: scanCalcGetDelays: callCalc = %d\n", v);
                                        
                            nError = -7;
                            goto end_of_scanCalcGetDelays;
                        }
                        needToFreeResultsCall = 1;
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
                    v = adjustSpacecraftAntennaCalcResults(scan, D, p, request, response, verbose);
                    if(v < 0)
                    {
                        printf("Error: scanCalcGetDelays: Antenna spacecraft problem %d calculating recording offset\n", v);
                        nError = -8;
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

                            v = getExternalDelay(ed[antId][k], request->date+request->time, &exDelay, &exDry, &exWet);
                            if(v < 0)
                            {
                                fprintf(stderr, "Error: scanCalcGetDelays: request for external delay from stn %s source %s at time %14.8f failed with error code %d\n", antenna->name, source->name, request->date+request->time, v);
                                nError = -9;
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
                request->date += 1;
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
    if((needToFreeResultsCall))
    {
        v = freeCalcResults(&results, p, verbose);
        if(v < 0)
        {
            fprintf(stderr, "Error: scanCalcGetDelays: freeCalcResults = %d\n", v);
        }
        needToFreeResultsCall = 0;
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
    DifxJob *job;
    DifxScan *scan;
    int haveSpacecraftAntenna = 0;
    int haveSpacecraftSource = 0;

    job = D->job;
    scan = D->scan + scanId;

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
            if(((job->perform_lmn_deriv)) && (source->perform_lmn_deriv != PerformDirectionDerivativeNone))
            {
                scan->imLMN[antId][k] = (DifxPolyModelLMNExtension *)calloc(nInt, sizeof(DifxPolyModelLMNExtension));
            }
            if(((job->perform_xyz_deriv)) && (source->perform_xyz_deriv != PerformDirectionDerivativeNone))
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
                if((source->perform_uvw_deriv))
                {
                    scan->im[antId][k][i].delta = source->delta_lmn;
                }
                else
                {
                    scan->im[antId][k][i].delta = job->delta_lmn;
                }
                if((scan->imLMN[antId][k]))
                {
                    if((source->perform_lmn_deriv))
                    {
                        scan->imLMN[antId][k][i].delta = source->delta_lmn;
                    }
                    else
                    {
                        scan->imLMN[antId][k][i].delta = job->delta_lmn;
                    }
                }
                if((scan->imXYZ[antId][k]))
                {
                    if((source->perform_xyz_deriv))
                    {
                        scan->imXYZ[antId][k][i].delta = source->delta_xyz;
                    }
                    else
                    {
                        scan->imXYZ[antId][k][i].delta = job->delta_xyz;
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
    if((model)) {
        for(antId = 0; antId < scan->nAntenna; ++antId) {
            free(model[antId]);
        }
    }
    free(model);
    if((modelLMN)) {
        for(antId = 0; antId < scan->nAntenna; ++antId) {
            free(modelLMN[antId]);
        }
    }
    free(modelLMN);
    if((modelXYZ)) {
        for(antId = 0; antId < scan->nAntenna; ++antId) {
            free(modelXYZ[antId]);
        }
    }
    free(model);
    return 0;
}

int difxCalc(DifxInput* const D, CalcParams* const p, const char *prefix, const int verbose)
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
        v = scanCalc(scanId, D, prefix, p, isLast, verbose);
        if(v < 0)
        {
            return -1;
        }
    }

    return 0;
}
