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



#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "hermite.h"

double hermite_interpolation(const unsigned int NUM_POINTS,
                             const double * const restrict x,
                             const double * const restrict y,
                             const double * const restrict dydx,
                             const double x_eval)
{
    // Hermite interpolation, with input position and velocity,
    // returning the position estimate
    // for the time x_eval
    
    // INPUTS:
    // NUM_POINTS number of data points in the input arrays
    // x[NUM_POINTS] x points for function f(x)
    // y[NUM_POINTS] function values for f(x)
    // dydx[NUM_POINTS] first derivative values, f'(x)
    // x_eval x value at which to evaluate the interpolation
    //
    // OUTPUTS:
    // f(x_eval)
    
    // Requirements:
    // x[i] is monotonically increasing.
    // Because dydx is the derivative of y with respect to x, the
    // x, y, and dydx arrays must have matching units.  That is, the
    // units of dydx must be the uits of y over the units of x.  So
    // if y is m and dydx is m/s, give x in seconds, not days.

    // see http://en.wikipedia.org/wiki/Hermite_interpolation
    static unsigned int NUM_POINTS_MAX = 0;
    static double * restrict poly_array = NULL;
    static double * restrict xmult_array = NULL;
    static double * restrict work_array = NULL;

    const unsigned int NUM_POINTS_2 = 2*NUM_POINTS;
    if(NUM_POINTS <= NUM_POINTS_MAX) {
        // do nothing
    }
    else {
        NUM_POINTS_MAX = NUM_POINTS;
        poly_array = (double * restrict)realloc(poly_array,
                                                sizeof(double)*NUM_POINTS_2*3);
        if(!poly_array) {
            fprintf(stderr, "Error: cannot malloc enough space for %u points in hermite_interpolation\n", NUM_POINTS);
            exit(1);
        }
        xmult_array = poly_array+NUM_POINTS_2*1;
        work_array = poly_array+NUM_POINTS_2*2;
    }
    // Convert the initial x values into the range [-1,1],
    // and fill in initial work array values
    double x_eval_mult = 0.0;
    unsigned int UPPER_BOUND = NUM_POINTS;
    double xterm = 1.0;
    double product = xterm;
    if(NUM_POINTS > 1) {
        unsigned int j=0;
        const double min = x[0];
        const double max = x[NUM_POINTS-1];
        const double midpoint = (min+max)*0.5;
        double mult = 2.0/(max-min);
        x_eval_mult = (x_eval - midpoint)*mult;
        for(unsigned int i=0; i < UPPER_BOUND; i++) {
            double xm = (x[i]-midpoint)*mult;
            xmult_array[j] = xm;
            work_array[j] = y[i];
            ++j;
            xmult_array[j] = xm;
            work_array[j] = y[i];
            ++j;
        }
        mult = 1.0/mult;
        poly_array[0] = work_array[0] * product;
        xterm = x_eval_mult - xmult_array[0];
        product = xterm;
        j=0;
        --UPPER_BOUND;
        for(unsigned int i=0; i < UPPER_BOUND; i++) {
            work_array[j++] = dydx[i]*mult;
            work_array[j] = (work_array[j+1] - work_array[j])
                / (xmult_array[j+1] - xmult_array[j]);
            j++;
        }
        work_array[j] = dydx[UPPER_BOUND]*mult;
        poly_array[1] = work_array[0] * product;
        xterm = x_eval_mult - xmult_array[1];
        product *= xterm;
        j=0;
        UPPER_BOUND = NUM_POINTS_2-2;
    }
    else {
        poly_array[0] = y[0];
        product = x_eval - x[0];
        poly_array[1] = dydx[0] * product;
        return poly_array[0] + poly_array[1];
    }
    // now work the way down the divided difference table
    unsigned int k=2;
    while((UPPER_BOUND)) {
        for(unsigned int i=0; i < UPPER_BOUND; i++) {
            work_array[i] = (work_array[i+1] - work_array[i])
                / (xmult_array[i+k]-xmult_array[i]);
        }
        --UPPER_BOUND;
        poly_array[k] = work_array[0] * product;
        xterm = x_eval_mult - xmult_array[k++];
        product *= xterm;
    }
    double result = 0.0;
    for(k=NUM_POINTS_2; k > 0;) {
        result += poly_array[--k];
    }
    return result;
}



void hermite_interpolation_pos_vel(const unsigned int NUM_POINTS,
                                   const double * const restrict x,
                                   const double * const restrict y,
                                   const double * const restrict dydx,
                                   const double x_eval,
                                   double * const restrict y_eval,
                                   double * const restrict dydx_eval)
{
    // Hermite interpolation, with input position and velocity,
    // returning the position and velocity estimates
    // for the time x_eval
    
    // INPUTS:
    // NUM_POINTS number of data points in the input arrays
    // x[NUM_POINTS] x points for function f(x)
    // y[NUM_POINTS] function values for f(x)
    // dydx[NUM_POINTS] first derivative values, f'(x)
    // x_eval x value at which to evaluate the interpolation
    //
    // OUTPUTS:
    // *y_eval = f(x_eval)
    // *dydx_eval = f'(x_eval)
    
    // Requirements:
    // x[i] is monotonically increasing

    // see http://en.wikipedia.org/wiki/Hermite_interpolation
    static unsigned int NUM_POINTS_MAX = 0;
    static double * restrict poly_array = NULL;
    static double * restrict poly_vel_array = NULL;
    static double * restrict xmult_array = NULL;
    static double * restrict work_array = NULL;

    const unsigned int NUM_POINTS_2 = 2*NUM_POINTS;
    if(NUM_POINTS <= NUM_POINTS_MAX) {
        // do nothing
    }
    else {
        NUM_POINTS_MAX = NUM_POINTS;
        poly_array = (double * restrict)realloc(poly_array,
                                                sizeof(double)*NUM_POINTS_2*4);
        if(!poly_array) {
            fprintf(stderr, "Error: cannot malloc enough space for %u points in hermite_interpolation\n", NUM_POINTS);
            exit(1);
        }
        poly_vel_array = poly_array+NUM_POINTS_2*1;
        xmult_array = poly_array+NUM_POINTS_2*2;
        work_array = poly_array+NUM_POINTS_2*3;
    }
    // Convert the initial x values into the range [-1,1],
    // and fill in initial work array values
    double x_eval_mult = 0.0;
    unsigned int UPPER_BOUND = NUM_POINTS;
    double xterm = 1.0; // (x_eval_mult - xmult_array[-1])^0
    double pos_product = xterm; 
    double vel_product = 0.0; // derivative of xterm with respect to x_eval_mult
    double vel_mult = 1.0;
    if(NUM_POINTS > 1) {
        unsigned int j=0;
        const double min = x[0];
        const double max = x[NUM_POINTS-1];
        const double midpoint = (min+max)*0.5;
        double mult = 2.0/(max-min);
        x_eval_mult = (x_eval - midpoint)*mult;
        for(unsigned int i=0; i < UPPER_BOUND; i++) {
            double xm = (x[i]-midpoint)*mult;
            xmult_array[j] = xm;
            work_array[j] = y[i];
            ++j;
            xmult_array[j] = xm;
            work_array[j] = y[i];
            ++j;
        }
        vel_mult = mult;
        mult = 1.0/mult;
        poly_array[0] = work_array[0] * pos_product;
        poly_vel_array[0] = 0.0; // vel_product is 0.0, so just set to 0 here
        xterm = x_eval_mult - xmult_array[0];
        vel_product = pos_product + xterm*vel_product;
        pos_product *= xterm;
        j=0;
        --UPPER_BOUND;
        for(unsigned int i=0; i < UPPER_BOUND; i++) {
            work_array[j++] = dydx[i]*mult;
            work_array[j] = (work_array[j+1] - work_array[j])
                / (xmult_array[j+1] - xmult_array[j]);
            j++;
        }
        work_array[j] = dydx[UPPER_BOUND]*mult;
        poly_array[1] = work_array[0] * pos_product;
        poly_vel_array[1] = work_array[0] * vel_product;
        xterm = x_eval_mult - xmult_array[1];
        vel_product = pos_product + xterm*vel_product;
        pos_product *= xterm;
        j=0;
        UPPER_BOUND = NUM_POINTS_2-2;
    }
    else {
        poly_array[0] = y[0];
        pos_product = x_eval - x[0];
        poly_array[1] = dydx[0] * pos_product;
        *y_eval = poly_array[0] + poly_array[1];
        *dydx_eval = dydx[0];
        return;
    }
    // now work the way down the divided difference table
    unsigned int k=2;
    while((UPPER_BOUND)) {
        for(unsigned int i=0; i < UPPER_BOUND; i++) {
            work_array[i] = (work_array[i+1] - work_array[i])
                / (xmult_array[i+k]-xmult_array[i]);
        }
        --UPPER_BOUND;
        poly_array[k] = work_array[0] * pos_product;
        poly_vel_array[k] = work_array[0] * vel_product;
        xterm = x_eval_mult - xmult_array[k];
        vel_product = pos_product + xterm*vel_product;
        pos_product *= xterm;
        k++;
    }
    double pos = 0.0;
    double vel = 0.0;
    for(k=NUM_POINTS_2; k > 0;) {
        pos += poly_array[--k];
        vel += poly_vel_array[k];
    }
    *y_eval = pos;
    *dydx_eval = vel * vel_mult;
    return;
}


void hermite_interpolation_pva(const unsigned int NUM_POINTS,
                               const double * const restrict x,
                               const double * const restrict y,
                               const double * const restrict dydx,
                               const double x_eval,
                               double * const restrict y_eval,
                               double * const restrict dydx_eval,
                               double * const restrict ddydx2_eval)
{
    // Hermite interpolation, with input position and velocity,
    // returning the position, velocity, and acceleration estimates
    // for the time x_eval
    
    // INPUTS:
    // NUM_POINTS number of data points in the input arrays
    // x[NUM_POINTS] x points for function f(x)
    // y[NUM_POINTS] function values for f(x)
    // dydx[NUM_POINTS] first derivative values, f'(x)
    // x_eval x value at which to evaluate the interpolation
    //
    // OUTPUTS:
    // *y_eval = f(x_eval)
    // *dydx_eval = f'(x_eval)
    // *ddydx2_eval = f''(x_eval)
    
    // Requirements:
    // x[i] is monotonically increasing

    // see http://en.wikipedia.org/wiki/Hermite_interpolation
    static unsigned int NUM_POINTS_MAX = 0;
    static double * restrict poly_array = NULL;
    static double * restrict poly_vel_array = NULL;
    static double * restrict poly_acc_array = NULL;
    static double * restrict xmult_array = NULL;
    static double * restrict work_array = NULL;

    const unsigned int NUM_POINTS_2 = 2*NUM_POINTS;
    if(NUM_POINTS <= NUM_POINTS_MAX) {
        // do nothing
    }
    else {
        NUM_POINTS_MAX = NUM_POINTS;
        poly_array = (double * restrict)realloc(poly_array,
                                                sizeof(double)*NUM_POINTS_2*5);
        if(!poly_array) {
            fprintf(stderr, "Error: cannot malloc enough space for %u points in hermite_interpolation\n", NUM_POINTS);
            exit(1);
        }
        poly_vel_array = poly_array+NUM_POINTS_2*1;
        poly_acc_array = poly_array+NUM_POINTS_2*2;
        xmult_array = poly_array+NUM_POINTS_2*3;
        work_array = poly_array+NUM_POINTS_2*4;
    }
    // Convert the initial x values into the range [-1,1],
    // and fill in initial work array values
    double x_eval_mult = 0.0;
    unsigned int UPPER_BOUND = NUM_POINTS;
    double xterm = 1.0; // (x_eval_mult - xmult_array[-1])^0
    double pos_product = xterm; 
    double vel_product = 0.0; // derivative of xterm with respect to x_eval_mult
    double acc_product = 0.0; // derivative of vel_product with respect to x_eval_mult
    if(NUM_POINTS > 1) {
        unsigned int j=0;
//         const double min = x[0];
//         const double max = x[NUM_POINTS-1];
//         const double midpoint = (min+max)*0.5;
//         double mult = 2.0/(max-min);
        const double midpoint = 0.0;
        double mult = 1.0;
        x_eval_mult = (x_eval - midpoint)*mult;
        for(unsigned int i=0; i < UPPER_BOUND; i++) {
            double xm = (x[i]-midpoint)*mult;
            xmult_array[j] = xm;
            //printf("z[%d] = %E\n", j,xmult_array[j]);
            work_array[j] = y[i];
            ++j;
            xmult_array[j] = xm;
            //printf("z[%d] = %E\n", j,xmult_array[j]);
            work_array[j] = y[i];
            ++j;
        }
//         for(unsigned int i=0; i < NUM_POINTS_2; i++) {
//             printf("f(%d)[%d] = %E\n", 0,i,work_array[i]);
//         }
        mult = 1.0/mult;
        poly_array[0] = work_array[0] * pos_product;
        poly_vel_array[0] = 0.0; // vel_product is 0.0, so just set to 0 here
        poly_acc_array[0] = 0.0; // acc_product is 0.0, so just set to 0 here
        //printf("coeff %E  prod %E     term %E   vprod %E   vterm %E\n", work_array[0], pos_product, poly_array[0], vel_product, poly_vel_array[0]);
        xterm = x_eval_mult - xmult_array[0];
        acc_product = vel_product + xterm*acc_product;
        vel_product = pos_product + xterm*vel_product;
        pos_product *= xterm;
        j=0;
        --UPPER_BOUND;
        for(unsigned int i=0; i < UPPER_BOUND; i++) {
            work_array[j++] = dydx[i]*mult;
            work_array[j] = (work_array[j+1] - work_array[j])
                / (xmult_array[j+1] - xmult_array[j]);
            j++;
        }
        work_array[j] = dydx[UPPER_BOUND]*mult;
//         for(unsigned int i=0; i < NUM_POINTS_2-1; i++) {
//             printf("f(%d)[%d] = %E\n", 1,i,work_array[i]);
//         }
        poly_array[1] = work_array[0] * pos_product;
        poly_vel_array[1] = work_array[0] * vel_product;
        poly_acc_array[1] = work_array[0] * acc_product;
        //printf("coeff %E  prod %E     term %E   vprod %E   vterm %E\n", work_array[0], pos_product, poly_array[0], vel_product, poly_vel_array[1]);
        xterm = x_eval_mult - xmult_array[1];
        acc_product = vel_product + xterm*acc_product;
        vel_product = pos_product + xterm*vel_product;
        pos_product *= xterm;
        j=0;
        UPPER_BOUND = NUM_POINTS_2-2;
    }
    else {
        poly_array[0] = y[0];
        pos_product = x_eval - x[0];
        poly_array[1] = dydx[0] * pos_product;
        *y_eval = poly_array[0] + poly_array[1];
        *dydx_eval = dydx[0];
        *ddydx2_eval = 0.0;
        return;
    }
    // now work the way down the divided difference table
    unsigned int k=2;
    while((UPPER_BOUND)) {
        for(unsigned int i=0; i < UPPER_BOUND; i++) {
            work_array[i] = (work_array[i+1] - work_array[i])
                / (xmult_array[i+k]-xmult_array[i]);
        }
//         for(unsigned int i=0; i < UPPER_BOUND; i++) {
//             printf("f(%d)[%d] = %E\n", k,i,work_array[i]);
//         }
        --UPPER_BOUND;
        poly_array[k] = work_array[0] * pos_product;
        poly_vel_array[k] = work_array[0] * vel_product;
        poly_acc_array[k] = work_array[0] * acc_product;
        //printf("coeff %E  prod %E     term %E   vprod %E   vterm %E\n", work_array[0], pos_product, poly_array[k], vel_product, poly_vel_array[k]);
        xterm = x_eval_mult - xmult_array[k];
        acc_product = vel_product + xterm*acc_product;
        vel_product = pos_product + xterm*vel_product;
        pos_product *= xterm;
        k++;
    }
    double pos = 0.0;
    double vel = 0.0;
    double acc = 0.0;
    for(k=NUM_POINTS_2; k > 0;) {
        pos += poly_array[--k];
        vel += poly_vel_array[k];
        acc += poly_acc_array[k];
    }
    *y_eval = pos;
    *dydx_eval = vel;
    *ddydx2_eval = acc * 2.0;
    return;
}







void Neville_interpolation_pos_vel(const unsigned int NUM_POINTS,
                                   const double * const restrict x,
                                   const double * const restrict y,
                                   const double x_eval,
                                   double * const restrict y_eval,
                                   double * const restrict dydx_eval)
{
    // INPUTS:
    // NUM_POINTS number of data points in the input arrays
    // x[NUM_POINTS] x points for function f(x)
    // y[NUM_POINTS] function values for f(x)
    // x_eval x value at which to evaluate the interpolation
    //
    // OUTPUTS:
    // *y_eval = f(x_eval)
    // *dydx_eval = f'(x_eval)
    // Requirements:
    // x[i] is monotonically increasing

    // see http://en.wikipedia.org/wiki/Neville%27s_algorithm
    static unsigned int NUM_POINTS_MAX = 0;
    static double * restrict poly_array = NULL;
    static double * restrict poly_vel_array = NULL;
    static double * restrict xmult_array = NULL;
    static double * restrict work_array = NULL;

    if(NUM_POINTS <= NUM_POINTS_MAX) {
        // do nothing
    }
    else {
        NUM_POINTS_MAX = NUM_POINTS;
        poly_array = (double * restrict)realloc(poly_array,
                                                sizeof(double)*NUM_POINTS*4);
        if(!poly_array) {
            fprintf(stderr, "Error: cannot malloc enough space for %u points in Neville_interpolation_pos_vel\n", NUM_POINTS);
            exit(1);
        }
        poly_vel_array = poly_array+NUM_POINTS*1;
        xmult_array = poly_array+NUM_POINTS*2;
        work_array = poly_array+NUM_POINTS*3;
    }
    // Convert the initial x values into the range [-1,1],
    // and fill in initial work array values
    double x_eval_mult = 0.0;
    unsigned int UPPER_BOUND = NUM_POINTS;
    double xterm = 1.0; // (x_eval_mult - xmult_array[-1])^0
    double pos_product = xterm; 
    double vel_product = 0.0; // derivative of xterm with respect to x_eval_mult
    double vel_mult = 1.0;
    if(NUM_POINTS > 2) {
        const double min = x[0];
        const double max = x[NUM_POINTS-1];
        const double midpoint = (min+max)*0.5;
        double mult = 2.0/(max-min);
        x_eval_mult = (x_eval - midpoint)*mult;
        for(unsigned int i=0; i < UPPER_BOUND; i++) {
            double xm = (x[i]-midpoint)*mult;
            xmult_array[i] = xm;
            work_array[i] = y[i];
        }
        vel_mult = mult;
        mult = 1.0/mult;
        poly_array[0] = work_array[0] * pos_product;
        poly_vel_array[0] = 0.0; // vel_product is 0.0, so just set to 0 here
        xterm = x_eval_mult - xmult_array[0];
        vel_product = pos_product + xterm*vel_product;
        pos_product *= xterm;
        --UPPER_BOUND;
        for(unsigned int i=0; i < UPPER_BOUND; i++) {
            work_array[i] = (work_array[i+1] - work_array[i])
                / (xmult_array[i+1] - xmult_array[i]);
        }
        poly_array[1] = work_array[0] * pos_product;
        poly_vel_array[1] = work_array[0] * vel_product;
        xterm = x_eval_mult - xmult_array[1];
        vel_product = pos_product + xterm*vel_product;
        pos_product *= xterm;
        UPPER_BOUND = NUM_POINTS-2;
    }
    else {
        poly_array[0] = y[0];
        pos_product = x_eval - x[0];
        vel_product = (y[1]-y[0])/(x[1]-x[0]);
        *y_eval = poly_array[0] + vel_product * pos_product;
        *dydx_eval = vel_product;
        return;
    }
    // now work the way down the divided difference table
    unsigned int k=2;
    while((UPPER_BOUND)) {
        for(unsigned int i=0; i < UPPER_BOUND; i++) {
            work_array[i] = (work_array[i+1] - work_array[i])
                / (xmult_array[i+k]-xmult_array[i]);
        }
        --UPPER_BOUND;
        poly_array[k] = work_array[0] * pos_product;
        poly_vel_array[k] = work_array[0] * vel_product;
        xterm = x_eval_mult - xmult_array[k];
        vel_product = pos_product + xterm*vel_product;
        pos_product *= xterm;
        k++;
    }
    double pos = 0.0;
    double vel = 0.0;
    for(k=NUM_POINTS; k > 0;) {
        pos += poly_array[--k];
        vel += poly_vel_array[k];
    }
    *y_eval = pos;
    *dydx_eval = vel * vel_mult;
    return;
}
