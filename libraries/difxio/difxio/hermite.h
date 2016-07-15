/***************************************************************************
 *   Copyright (C) 2012 by James M Anderson                                *
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



#ifndef HERMITE_H
#define HERMITE_H

/* restrict
   This is a really useful modifier, but it is not supported by
   all compilers.  Furthermore, the different ways to specify it
   (double * restrict dp0, double dp1[restrict]) are not available
   in the same release of a compiler.  If you are still using an old
   compiler, your performace is going to suck anyway, so this code
   will only give you restrict when it is fully available.
*/
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
/* C99 has restrict */
#  if (__STDC_VERSION__ >= 199901L)
#  else 
#    ifndef restrict
#      define restrict
#    endif
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

double hermite_interpolation(const unsigned int NUM_POINTS,
                             const double * const restrict x,
                             const double * const restrict y,
                             const double * const restrict dydx,
                             const double x_eval);

void hermite_interpolation_pos_vel(const unsigned int NUM_POINTS,
                                   const double * const restrict x,
                                   const double * const restrict y,
                                   const double * const restrict dydx,
                                   const double x_eval,
                                   double * const restrict y_eval,
                                   double * const restrict dydx_eval);

void hermite_interpolation_pva(const unsigned int NUM_POINTS,
                               const double * const restrict x,
                               const double * const restrict y,
                               const double * const restrict dydx,
                               const double x_eval,
                               double * const restrict y_eval,
                               double * const restrict dydx_eval,
                               double * const restrict ddydx2_eval);

void Neville_interpolation_pos_vel(const unsigned int NUM_POINTS,
                                   const double * const restrict x,
                                   const double * const restrict y,
                                   const double x_eval,
                                   double * const restrict y_eval,
                                   double * const restrict dydx_eval);



#ifdef __cplusplus
}
#endif

#endif /* HERMITE_H */
