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
// $Id: checkdifxcalcrotation.c 6002 2014-03-20 13:57:41Z JamesAnderson $
// $HeadURL: $
// $LastChangedRevision: 6002 $
// $Author: JamesAnderson $
// $LastChangedDate: 2014-03-20 14:57:41 +0100 (Thu, 20 Mar 2014) $
//
//============================================================================


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "difxcalcrotation.h"





void print_spherical(double* const restrict v)
{
    double r, RA, Dec;

    r = sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    RA = atan2(v[1],v[0]);
    Dec = asin(v[2]/r);
    printf("r=%14E RA=%14E Dec=%14E        in degrees:  RA=%10f Dec=%10f\n", r, RA, Dec, RA*180./DIFX_PI, Dec*180./DIFX_PI);
    return;
}

void print_cartesian(double* const restrict v)
{
    printf("%14E %14E %14E\n", v[0], v[1], v[2]);
    return;
}




int main(void)
{
    double R0[DIFXCALC_3D_MAT_SIZE];
    double R1[DIFXCALC_3D_MAT_SIZE];
    double R2[DIFXCALC_3D_MAT_SIZE];
    double R3[DIFXCALC_3D_MAT_SIZE];
    double R4[DIFXCALC_3D_MAT_SIZE];
    double R5[DIFXCALC_3D_MAT_SIZE];
    double v0[DIFXCALC_3D_VEC_SIZE];
    double v1[DIFXCALC_3D_VEC_SIZE];
    double v2[DIFXCALC_3D_VEC_SIZE];
    double v3[DIFXCALC_3D_VEC_SIZE];
    double v4[DIFXCALC_3D_VEC_SIZE];
    double v5[DIFXCALC_3D_VEC_SIZE];
    int i,j;
    double RA, Dec, theta, beta;

    /* for(i=0; i < DIFXCALC_3D_MAT_SIZE; i++) */
    /* { */
    /*     R0[i] = R1[i] = 0.0; */
    /* } */
    /* R0[0] = R0[4] = R0[8] = 1.0; */
    /* R1[0] = R1[4] = R1[8] = 1.0; */

    /* for(i=0; i < 100000000; i++) */
    /* { */
    /*     difxcalc_multiply_R_R(R0,R1,R2); */
    /*     difxcalc_multiply_R_R(R2,R1,R0); */
    /* } */

    /* for(i=0; i < DIFXCALC_3D_VEC_SIZE; ++i) */
    /* { */
    /*     for(j=0; j < DIFXCALC_3D_VEC_SIZE; ++j) */
    /*     { */
    /*         printf("%5.0f   ", R2[i*DIFXCALC_3D_VEC_SIZE+j]); */
    /*     } */
    /*     printf("\n"); */
    /* } */
    RA = 45.0/180.*DIFX_PI;
    Dec = -45.0/180.*DIFX_PI;
    theta = 5.0/180.*DIFX_PI;
    v0[0] = cos(RA)*cos(Dec);
    v0[1] = sin(RA)*cos(Dec);
    v0[2] = sin(Dec);
    beta = DIFX_PI_2 - RA;

    difxcalc_R_z(beta, R0);
    difxcalc_R_x(theta, R1);
    difxcalc_R_z(-beta, R2);

    difxcalc_multiply_R_v(R0,v0,v1);
    difxcalc_multiply_R_v(R1,v1,v2);
    difxcalc_multiply_R_v(R2,v2,v3);

    difxcalc_R_Rotate_Dec(v0, theta, R3);
    difxcalc_multiply_R_v(R3, v0, v4);

    difxcalc_subtract_v_v(v3,v4,v5);

    print_spherical(v0);
    print_spherical(v3);
    print_spherical(v4);
    print_cartesian(v5);

    difxcalc_R_Rotate_Dec_alternate(v0, theta, R3);
    difxcalc_multiply_R_v(R3, v0, v1);

    difxcalc_subtract_v_v(v1,v4,v5);

    print_spherical(v1);
    print_cartesian(v5);

    /* for(j=-6;j<=-1;++j) */
    {
        /* theta = pow(10.0,j); */
        /* for(i=-180;i <= 180; ++i) */
        {

            RA = 0.0/180.*DIFX_PI;
            /* Dec = i/2.0/180.*DIFX_PI; */
            Dec = 45.0/180.*DIFX_PI;
            theta = 1E-4;/*5.0/180.*DIFX_PI;*/
            v0[0] = cos(RA)*cos(Dec);
            v0[1] = sin(RA)*cos(Dec);
            v0[2] = sin(Dec);
            beta = DIFX_PI_2 - RA;


            difxcalc_R_z(beta,   R0);
            difxcalc_R_x(-Dec,   R1);
            difxcalc_R_z(theta, R2);
            difxcalc_R_x(Dec,    R3);
            difxcalc_R_z(-beta,  R4);

            difxcalc_multiply_R_v(R0,v0,v1);
            difxcalc_multiply_R_v(R1,v1,v2);
            difxcalc_multiply_R_v(R2,v2,v1);
            difxcalc_multiply_R_v(R3,v1,v2);
            difxcalc_multiply_R_v(R4,v2,v1);

            difxcalc_R_Rotate_RA(v0, theta, R5);
            difxcalc_multiply_R_v(R5, v0, v4);

            difxcalc_subtract_v_v(v1,v4,v5);

    
            print_spherical(v0);
            print_spherical(v1);
            print_spherical(v4);
            print_cartesian(v5);
            printf("guestimate is %f\n", (RA - theta/cos(Dec))*180./DIFX_PI);
            /* { */
            /*     double RA2, Dec2, r2, dRA; */
            /*     difx_Cartesian_to_RADec(v4,&RA2, &Dec2, &r2); */
            /*     dRA = (RA2 - (RA - theta/cos(Dec))); */
            /*     printf("Difference Dec= %5.1f theta= %7.1E  RAoff= %10.3E  %10.3E dRA= %10.3E %10.3E frac= %10.3E\n", i/2.0, theta, -RA2, -RA2*180./DIFX_PI, dRA, dRA*180./DIFX_PI, dRA/-RA2); */
            /* } */
        }    
    }    

    RA = 0.0/180.*DIFX_PI;
    Dec = 0.0/180.*DIFX_PI;
    theta = 5.0/180.*DIFX_PI;
    v0[0] = cos(RA)*cos(Dec);
    v0[1] = sin(RA)*cos(Dec);
    v0[2] = sin(Dec);
    beta = 45.0/180.*DIFX_PI;

    difxcalc_R_Rotate_RA(v0, theta, R0);
    difxcalc_multiply_R_v(R0, v0, v1);
    difxcalc_R_Rotate_Dec(v1, theta, R1);
    difxcalc_multiply_R_v(R1, v1, v2);

    
    difxcalc_R_Rotate_RADec(v0, theta*sqrt(2.0), beta, R2);
    difxcalc_multiply_R_v(R2, v0, v3);

    print_spherical(v0);
    print_spherical(v1);
    print_spherical(v2);
    print_spherical(v3);

    difx_RADec_proper_motion_to_Cartesian(45.0/180.*DIFX_PI, -45.0/180.*DIFX_PI, 1.0,
                                          0.0/180.*DIFX_PI, 1.0/180.*DIFX_PI, 0.0,
                                          v0, v1, v2);
    print_cartesian(v0);
    print_cartesian(v1);
    print_cartesian(v2);
    
   
    return 0;
}



