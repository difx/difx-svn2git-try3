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
//===========================================================================
// SVN properties (DO NOT CHANGE)
//
// $Id: difxcalcrotation.c 6002 2014-03-20 13:57:41Z JamesAnderson $
// $HeadURL: $
// $LastChangedRevision: 6002 $
// $Author: JamesAnderson $
// $LastChangedDate: 2014-03-20 14:57:41 +0100 (Thu, 20 Mar 2014) $
//
//============================================================================


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "difxcalcrotation.h"









void difx_Cartesian_to_RADec(const double* const restrict v, double* const restrict RA, double* const restrict Dec, double* const restrict r)
{
    double rho2;
    rho2 = v[0]*v[0]+v[1]*v[1];
    *r = sqrt(rho2+v[2]*v[2]);
    *RA = atan2(v[1],v[0]);
    *Dec = atan2(v[2],sqrt(rho2));
    return;
}

void difx_RADec_to_Cartesian(const double RA, const double Dec, const double r, double* const restrict v)
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
    v[0] = r*ca*cd;
    v[1] = r*sa*cd;
    v[2] = r*sd;
    return;
}




void difx_RADec_proper_motion_to_Cartesian(const double RA, const double Dec, const double r, const double pmRA, const double pmDec, const double pmr, double* const restrict pos, double* const restrict vel, double* const restrict ref_dir)
{
    /* RA and Dec are in radians
       r is the distance to the source in meters
       pmRA and pmDec are the proper motions of the source, in radians per second
           with pmRA needing to still be corrected for the declination of the
           source
       pmr is the radial velocity of the source (positive away) in m/s
       pos is the Cartesian position of the source in meters
       vel is the velocity of the source, in metres per second
       ref_dir is the unit vector in the direction of celestial North
           at the refrence epoch
     */
    double ca, sa;
    double cd, sd;
    double pmRAa;
    double posu[DIFXCALC_3D_VEC_SIZE];
#ifdef _GNU_SOURCE
    sincos(RA, &sa, &ca);
    sincos(Dec, &sd, &cd);
#else
    sa = sin(RA);
    ca = cos(RA);
    sd = sin(Dec);
    cd = cos(Dec);
#endif
    pos[0] = r*ca*cd;
    pos[1] = r*sa*cd;
    pos[2] = r*sd;
    difx_normalize_to_unit_vector(pos, posu);
    pmRAa = pmRA*cd;
    /* reference direction is related to the declination part of proper motion */
    ref_dir[0] = -ca*sd;
    ref_dir[1] = -sa*sd;
    ref_dir[2] = cd;
    /* declination part of proper motion vector */
    vel[0] = -r*ca*sd*pmDec;
    vel[1] = -r*sa*sd*pmDec;
    vel[2] = r*cd*pmDec;
    /* right ascension part of proper motion vector */
    vel[0] += r*pmRAa*(-sa);
    vel[1] += r*pmRAa*ca;
    /* radial velocity part */
    vel[0] += pmr*posu[0];
    vel[1] += pmr*posu[1];
    vel[2] += pmr*posu[2];
    return;
}




void difx_Cartesian_to_RADec_proper_motion(const double* const restrict pos, const double* const restrict vel, double* RA, double* Dec, double* r, double* pmRA, double* pmDec, double* pmr)
{
    /* Input:
       pos is the Cartesian position of the source in meters
       vel is the velocity of the source, in metres per second
       Output:
       RA and Dec are in radians
       r is the distance to the source in meters
       pmRA and pmDec are the proper motions of the source, in radians per second
           with pmRA corrected for the declination of the
           source (real angular motion is pmRA \times \cos\delta)
       pmr is the radial velocity of the source (positive away) in m/s
     */
    double r_, r2, rho, rho2;
    rho2 = pos[0]*pos[0]+pos[1]*pos[1];
    r2 = rho2 + pos[2]*pos[2];
    *r = r_ = sqrt(r2);
    rho = sqrt(rho2);
    *RA = atan2(pos[1],pos[0]);
    *Dec = atan2(pos[2],rho);
    if(r_ > 0.0)
    {
        *pmr = (pos[0]*vel[0]+pos[1]*vel[1]+pos[2]*vel[2])/r_;
        if(rho2 > 0.0)
        {
            *pmRA = (pos[0]*vel[1] - pos[1]*vel[0])/rho2;
            *pmDec = (rho2*vel[2] - vel[0]*pos[0]*pos[2] - vel[1]*pos[1]*pos[2]) / (r2*rho);
        }
        else
        {
            *pmRA = 0.0;
            *pmDec = 0.0;
        }
    }
    else
    {
        *pmr = 0.0;
        *pmRA = 0.0;
        *pmDec = 0.0;
    }
    return;
}


/* normalize a vector to create a unit vector.  v_in and v_out are
   allowed to overlap
*/
void difx_normalize_to_unit_vector(const double* const restrict v_in, double* const restrict v_out)
{
    double r;
    r = v_in[0]*v_in[0]+v_in[1]*v_in[1]+v_in[2]*v_in[2];
    v_out[0] = v_in[0];
    v_out[1] = v_in[1];
    v_out[2] = v_in[2];
    if(r != 1.0)
    {
    if(r < (double)(FLT_MAX)*FLT_MAX)
        {
            r = 1.0/sqrt(r);
            v_out[0] *= r;
            v_out[1] *= r;
            v_out[2] *= r;
        }
        else
        {
            /* take care of potential overflow issues */
            r = hypot(v_in[0],v_in[1]);
            r = hypot(r,v_in[2]);
            v_out[0] /= r;
            v_out[1] /= r;
            v_out[2] /= r;
        }
            
    }
    return;
}







/* Rotate a column vector an angle \theta counterclockwise about the x axis
   as seen by an observer with the x axis pointing at the observer
*/
void difxcalc_R_x(double theta, double* const restrict R_x)
{
    double ctheta, stheta;
#ifdef _GNU_SOURCE
    sincos(theta, &stheta, &ctheta);
#else
    stheta = sin(theta);
    ctheta = cos(theta);
#endif
    R_x[0] = 1.0;
    R_x[1] = 0.0;
    R_x[2] = 0.0;
    R_x[3] = 0.0;
    R_x[4] = ctheta;
    R_x[5] = -stheta;
    R_x[6] = 0.0;
    R_x[7] = stheta;
    R_x[8] = ctheta;
    return;
}

/* Rotate a column vector an angle \theta counterclockwise about the y axis
   as seen by an observer with the y axis pointing at the observer
*/
void difxcalc_R_y(double theta, double* const restrict R_y)
{
    double ctheta, stheta;
#ifdef _GNU_SOURCE
    sincos(theta, &stheta, &ctheta);
#else
    stheta = sin(theta);
    ctheta = cos(theta);
#endif
    R_y[0] = ctheta;
    R_y[1] = 0.0;
    R_y[2] = stheta;
    R_y[3] = 0.0;
    R_y[4] = 1.0;
    R_y[5] = 0.0;
    R_y[6] = -stheta;
    R_y[7] = 0.0;
    R_y[8] = ctheta;
    return;
}

/* Rotate a column vector an angle \theta counterclockwise about the z axis
   as seen by an observer with the z axis pointing at the observer
*/
void difxcalc_R_z(double theta, double* const restrict R_z)
{
    double ctheta, stheta;
#ifdef _GNU_SOURCE
    sincos(theta, &stheta, &ctheta);
#else
    stheta = sin(theta);
    ctheta = cos(theta);
#endif
    R_z[0] = ctheta;
    R_z[1] = -stheta;
    R_z[2] = 0.0;
    R_z[3] = stheta;
    R_z[4] = ctheta;
    R_z[5] = 0.0;
    R_z[6] = 0.0;
    R_z[7] = 0.0;
    R_z[8] = 1.0;
    return;
}

/* Create a rotation matrix that will rotate a vector about an
   arbitrary axis defined by the unit vector u counterclockwise by
   an angle \theta
*/
void difxcalc_R_u(const double* const restrict u, double theta, double* const restrict R)
{
    double ctheta, stheta, omct;
#ifdef _GNU_SOURCE
    sincos(theta, &stheta, &ctheta);
#else
    stheta = sin(theta);
    ctheta = cos(theta);
#endif
    omct = 1.0 - ctheta;
    R[0] = ctheta + u[0]*u[0]*omct;
    R[1] = u[0]*u[1]*omct -u[2]*stheta;
    R[2] = u[0]*u[2]*omct +u[1]*stheta;
    R[3] = u[1]*u[0]*omct + u[2]*stheta;
    R[4] = ctheta + u[1]*u[1]*omct;
    R[5] = u[1]*u[2]*omct -u[0]*stheta;
    R[6] = u[2]*u[0]*omct -u[1]*stheta;
    R[7] = u[2]*u[1]*omct + u[0]*stheta;
    R[8] = ctheta + u[2]*u[2]*omct;
    return;
}



/* Dot product of two vectors
 */
double difxcalc_dot_product_v_v(const double* const restrict v0, const double* const restrict v1)
{
    return v0[0]*v1[0] + v0[1]*v1[1] + v0[2]*v1[2];
}



/* cross product of two vectors.
   The output vector must not overlap the input vectors.
   
 */
void difxcalc_cross_product_v_v(const double* const restrict v0, const double* const restrict v1, double* const restrict v_out)
{
    v_out[0] = v0[1]*v1[2] - v0[2]*v1[1];
    v_out[1] = v0[2]*v1[0] - v0[0]*v1[2];
    v_out[2] = v0[0]*v1[1] - v0[1]*v1[0];
    return;
}



/* Multiply a rotation matrix and a vector, v_out = R v_in.
   The output vector must not overlap the input vector.
 */
void difxcalc_multiply_R_v(const double* const restrict R, const double* const restrict v_in, double* const restrict v_out)
{
    v_out[0] = R[0]*v_in[0] + R[1]*v_in[1] + R[2]*v_in[2];
    v_out[1] = R[3]*v_in[0] + R[4]*v_in[1] + R[5]*v_in[2];
    v_out[2] = R[6]*v_in[0] + R[7]*v_in[1] + R[8]*v_in[2];
    return;
}



/* Multiply a rotation matrix by a rotation matrix, R_C = R_A R_B
   The output matrix must not overlap the input matrices.
 */
void difxcalc_multiply_R_R(const double* const restrict R_A, const double* const restrict R_B, double* const restrict R_C)
{
    int row, col, krow, kcol;
    for(row=0; row < DIFXCALC_3D_MAT_SIZE; row += DIFXCALC_3D_VEC_SIZE)
    {
        for(col=0; col < DIFXCALC_3D_VEC_SIZE; ++col)
        {
            double s=0.0;
            for(krow=0, kcol=0; kcol < DIFXCALC_3D_VEC_SIZE; ++kcol, krow += DIFXCALC_3D_VEC_SIZE)
            {
                s += R_A[row+kcol] * R_B[col+krow];
            }
            R_C[row+col] = s;
        }
    }
    return;
}






/* Add a rotation matrix to a rotation matrix, R_C = R_A + R_B.
   The output matrix R_C must not overlap the input matrices R_A or R_B.
 */
void difxcalc_add_R_R(const double* const restrict R_A, const double* const restrict R_B, double* const restrict R_C)
{
    int row, col;
    for(row=0; row < DIFXCALC_3D_MAT_SIZE; row+=DIFXCALC_3D_VEC_SIZE)
    {
        for(col=0; col < DIFXCALC_3D_VEC_SIZE; ++col)
        {
            R_C[row+col] = R_A[row+col] + R_B[row+col];
        }
    }
    return;
}



/* Subtract a rotation matrix to a rotation matrix, R_C = R_A - R_B.
   The output matrix R_C must not overlap the input matrices R_A or R_B.
 */
void difxcalc_subtract_R_R(const double* const restrict R_A, const double* const restrict R_B, double* const restrict R_C)
{
    int row, col;
    for(row=0; row < DIFXCALC_3D_MAT_SIZE; row+=DIFXCALC_3D_VEC_SIZE)
    {
        for(col=0; col < DIFXCALC_3D_VEC_SIZE; ++col)
        {
            R_C[row+col] = R_A[row+col] - R_B[row+col];
        }
    }
    return;
}



/* Add a vector to a vector, v_C = v_A + v_B.
   The output matrix v_C must not overlap the input matrices v_A or v_B.
 */
void difxcalc_add_v_v(const double* const restrict v_A, const double* const restrict v_B, double* const restrict v_C)
{
    int row;
    for(row=0; row < DIFXCALC_3D_VEC_SIZE; ++row)
    {
        v_C[row] = v_A[row] + v_B[row];
    }
    return;
}
/* Subtract a vector to a vector, v_C = v_A - v_B.
   The output matrix v_C must not overlap the input matrices v_A or v_B.
 */
void difxcalc_subtract_v_v(const double* const restrict v_A, const double* const restrict v_B, double* const restrict v_C)
{
    int row;
    for(row=0; row < DIFXCALC_3D_VEC_SIZE; ++row)
    {
        v_C[row] = v_A[row] - v_B[row];
    }
    return;
}







/* Create a rotation matrix to rotate a vector up by some angle \theta
   in increasing declination.  Note that v_out
   may not overlap with v (v_in).

   First, rotate about the z axis so that the vector is
   in the y--z plane.  Then rotate about the x axis so that the
   vector goes up an angle \theta in declination.  Then
   rotate back along the z axis to return to the original RA.
*/
void difxcalc_R_Rotate_Dec(const double* const restrict v, double theta, double* const restrict R)
{
    double ct, st;
    double cb, sb;  /* \beta = 90\degr - \alpha */
    double rho;
#ifdef _GNU_SOURCE
    sincos(theta, &st, &ct);
#else
    st = sin(theta);
    ct = cos(theta);
#endif
    rho = hypot(v[0],v[1]);
    if(rho != 0.0)
    {
        sb = v[0]/rho;
        cb = v[1]/rho;
    }
    else
    {
        /* At pole!  \alpha is undefined.  Arbitrarily choose \alpha = 0.0 */
        sb = 1.0;
        cb = 0.0;
    }
    
    R[0] = cb*cb+sb*sb*ct;
    R[1] = -(cb*sb)+cb*sb*ct;
    R[2] = -(sb*st);
    R[3] =-(cb*sb)+cb*sb*ct;
    R[4] = sb*sb+cb*cb*ct;
    R[5] = -(cb*st);
    R[6] = sb*st;
    R[7] = cb*st;
    R[8] = ct;

    return;
}


/* Create a rotation matrix to rotate a vector up by some angle \theta
   in increasing declination.  Note that v_out
   may not overlap with v (v_in).
*/
void difxcalc_R_Rotate_Dec_alternate(const double* const restrict v, double theta, double* const restrict R)
{
    double u[DIFXCALC_3D_VEC_SIZE];
    double rho;
    rho = hypot(v[0],v[1]);
    if(rho != 0.0)
    {
        u[0] =  v[1] / rho;
        u[1] = -v[0] / rho;
    }
    else
    {
        /* At pole!  \alpha is undefined.  Arbitrarily choose \alpha = 0.0 */
        u[0] = 0.0;
        u[1] = -1.0;
    }
    u[2] = 0.0;
    difxcalc_R_u(u,theta,R);
    return;
}



/* Create a rotation matrix to rotate a vector up by some angle \theta
   in the -l direction (approximately increasing RA).  Note that v_out
   may not overlap with v (v_in).

   Create a unit vector perpendicular to v that is in the same plane
   as contains the vector v and the z axis.  Create a rotation matrix about
   this unit vector u that will rotate a vector by theta.

   This should be equivalent to the following rotation matrix series

   R_z(-\beta) R_x(\delta) R_z(\theta) R_x(-\delta) R_z(\beta)

   where \beta = 90\degr - \alpha
*/
void difxcalc_R_Rotate_RA(const double* const restrict v, double theta, double* const restrict R)
{
    double u[DIFXCALC_3D_VEC_SIZE];
    double t;
    t = v[0]*v[0]+v[1]*v[1];
    if(v[2] > 0.0)
    {
        u[0] = -v[0];
        u[1] = -v[1];
        u[2] = t/v[2];
        if(u[2] < DBL_MAX)
        {
            difx_normalize_to_unit_vector(u,u);
        }
        else
        {
            u[0] = 0.0;
            u[1] = 0.0;
            u[2] = 1.0;
        }
    }
    else if(v[2] < 0.0)
    {
        u[0] = v[0];
        u[1] = v[1];
        u[2] = -t/v[2];
        if(u[2] < DBL_MAX)
        {
            difx_normalize_to_unit_vector(u,u);
        }
        else
        {
            u[0] = 0.0;
            u[1] = 0.0;
            u[2] = 1.0;
        }
    }
    else
    {
        u[0] = 0.0;
        u[1] = 0.0;
        u[2] = 1.0;
    }

    difxcalc_R_u(u,theta,R);
    return;
}


/* Create a rotation matrix to rotate a vector up by some angle \theta
   in a direction \beta from the direction of increasing RA through
   increasing declination

   Note that v_out
   may not overlap with v (v_in).
*/
void difxcalc_R_Rotate_RADec(const double* const restrict v, double theta, double beta, double* const restrict R)
{
    double ra_u[DIFXCALC_3D_VEC_SIZE], dec_u[DIFXCALC_3D_VEC_SIZE], u[DIFXCALC_3D_VEC_SIZE];
    double rho;
    double t;
    double cb, sb;
#ifdef _GNU_SOURCE
    sincos(beta, &sb, &cb);
#else
    sb = sin(beta);
    cb = cos(beta);
#endif
    /* Richt Ascension rotation unit vector */
    t = v[0]*v[0]+v[1]*v[1];
    if(v[2] > 0.0)
    {
        ra_u[0] = -v[0];
        ra_u[1] = -v[1];
        ra_u[2] = t/v[2];
        if(ra_u[2] < DBL_MAX)
        {
            difx_normalize_to_unit_vector(ra_u,ra_u);
        }
        else
        {
            ra_u[0] = 0.0;
            ra_u[1] = 0.0;
            ra_u[2] = 1.0;
        }
    }
    else if(v[2] < 0.0)
    {
        ra_u[0] = v[0];
        ra_u[1] = v[1];
        ra_u[2] = -t/v[2];
        if(ra_u[2] < DBL_MAX)
        {
            difx_normalize_to_unit_vector(ra_u,ra_u);
        }
        else
        {
            ra_u[0] = 0.0;
            ra_u[1] = 0.0;
            ra_u[2] = 1.0;
        }
    }
    else
    {
        ra_u[0] = 0.0;
        ra_u[1] = 0.0;
        ra_u[2] = 1.0;
    }
    /* Declination rotation unit vector */
    rho = sqrt(t);
    if(rho != 0.0)
    {
        dec_u[0] =  v[1] / rho;
        dec_u[1] = -v[0] / rho;
    }
    else
    {
        /* At pole!  \alpha is undefined.  Arbitrarily choose \alpha = 0.0 */
        dec_u[0] = 0.0;
        dec_u[1] = -1.0;
    }
    dec_u[2] = 0.0;
    printf("dot product of dec and dec is %E\n", difxcalc_dot_product_v_v(dec_u, dec_u));
    printf("dot product of ra and dec is %E\n", difxcalc_dot_product_v_v(ra_u, dec_u));
    u[0] = cb*ra_u[0] + sb*dec_u[0];
    u[1] = cb*ra_u[1] + sb*dec_u[1];
    u[2] = cb*ra_u[2] + sb*dec_u[2];
    printf("dot product of u and u is %E\n", difxcalc_dot_product_v_v(u, u));
    difx_normalize_to_unit_vector(u,u);
    difxcalc_R_u(u,theta,R);
    return;
}
