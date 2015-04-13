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
// $Id: difxcalcrotation.h 6002 2014-03-20 13:57:41Z JamesAnderson $
// $HeadURL: $
// $LastChangedRevision: 6002 $
// $Author: JamesAnderson $
// $LastChangedDate: 2014-03-20 14:57:41 +0100 (Thu, 20 Mar 2014) $
//
//============================================================================




/* The rotations in this toolbox work on column vectors, by
   premultiplying the column vectors.  The rotations rotate the
   *vector* (not the coordinate system) about an angle \theta in
   a counterclockwise manner about the specified axis as seen by
   an observer with the specified axis pointing at the observer.

   The rotation matrices R are declared as single dimension arrays,
   and work as R[row*VEC_SIZE +column].

   Vectors are single dimension arrays and work as v[row].


*/




#ifndef DIFXCALC_ROTATION_H
#define DIFXCALC_ROTATION_H 1

#ifdef __GNUC__
#  ifdef restrict
/*   Someone else has already defined it.  Hope they got it right. */
#  elif !defined(__GNUG__) && (__STDC_VERSION__ >= 199901L)
/*   Restrict already available */
#  elif !defined(__GNUG__) && (__GNUC__ > 2) || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#    define restrict __restrict
#  elif (__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
#    define restrict __restrict
#  else
#    define restrict
#  endif
#else
#  ifndef restrict
#    define restrict
#  endif
#endif




#ifdef __cplusplus
extern "C" {
#endif


#define DIFXCALC_3D_VEC_SIZE 3
#define DIFXCALC_3D_MAT_SIZE 9

/* Try   echo "scale=100; 4*a(1)" | bc -l -q    */
#define DIFX_PI 3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170676
#define DIFX_2PI 6.2831853071795864769252867665590057683943387987502116419498891846156328125724179972560696506842341352
#define DIFX_PI_2 1.5707963267948966192313216916397514420985846996875529104874722961539082031431044993140174126710585338
#define DIFX_ROOT_2 1.4142135623730950488016887242096980785696718753769480731766797379907324784621070388503875343276415727




void difx_Cartesian_to_RADec(const double* const restrict v, double* const restrict RA, double* const restrict Dec, double* const restrict r);
void difx_RADec_to_Cartesian(const double RA, const double Dec, const double r, double* const restrict v);
void difx_RADec_proper_motion_to_Cartesian(const double RA, const double Dec, const double r, const double pmRA, const double pmDec, const double pmr, double* const restrict pos, double* const restrict vel, double* const restrict ref_dir);
void difx_Cartesian_to_RADec_proper_motion(const double* const restrict pos, const double* const restrict vel, double* RA, double* Dec, double* r, double* pmRA, double* pmDec, double* pmr);
void difx_normalize_to_unit_vector(const double* const restrict v_in, double* const restrict v_out);
void difxcalc_R_x(double theta, double* const restrict R_x);
void difxcalc_R_y(double theta, double* const restrict R_y);
void difxcalc_R_z(double theta, double* const restrict R_z);
void difxcalc_R_u(const double* const restrict u, double theta, double* const restrict R);
double difxcalc_dot_product_v_v(const double* const restrict v0, const double* const restrict v1);
void difxcalc_cross_product_v_v(const double* const restrict v0, const double* const restrict v1, double* const restrict v_out);
void difxcalc_multiply_R_v(const double* const restrict R, const double* const restrict v_in, double* const restrict v_out);
void difxcalc_multiply_R_R(const double* const restrict R_A, const double* const restrict R_B, double* const restrict R_C);
void difxcalc_add_R_R(const double* const restrict R_A, const double* const restrict R_B, double* const restrict R_C);
void difxcalc_subtract_R_R(const double* const restrict R_A, const double* const restrict R_B, double* const restrict R_C);
void difxcalc_add_v_v(const double* const restrict v_A, const double* const restrict v_B, double* const restrict v_C);
void difxcalc_subtract_v_v(const double* const restrict v_A, const double* const restrict v_B, double* const restrict v_C);
void difxcalc_R_Rotate_Dec(const double* const restrict v, double theta, double* const restrict R);
void difxcalc_R_Rotate_Dec_alternate(const double* const restrict v, double theta, double* const restrict R);
void difxcalc_R_Rotate_RA(const double* const restrict v, double theta, double* const restrict R);
void difxcalc_R_Rotate_RADec(const double* const restrict v, double theta, double beta, double* const restrict R);








#ifdef __cplusplus
}
#endif




#endif /* DIFXCALC_ROTATION_H */

