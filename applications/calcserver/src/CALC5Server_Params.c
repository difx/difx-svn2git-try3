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
// $Id: fitsCT.c 6619 2015-04-22 14:26:47Z JamesAnderson $
// $HeadURL: https://svn.atnf.csiro.au/difx/applications/difx2fits/branches/radioastron20140201/src/fitsCT.c $
// $LastChangedRevision: 6619 $
// $Author: JamesAnderson $
// $LastChangedDate: 2015-04-22 16:26:47 +0200 (Wed, 22 Apr 2015) $
//
//============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <rpc/rpc.h>
#include "DiFX_Delay_Server.h"
#include "DiFX_Delay_Server_Params.h"
#include "MATHCNST.H"
#include "STDDEFS.H"




/******************************************************************************/
int
process_DiFX_Delay_Server_Parameters_1_CALCPROG(const struct getDIFX_DELAY_SERVER_PARAMETERS_1_arg* const argument, struct DIFX_DELAY_SERVER_PARAMETERS_1_res* const res)
{
    res->server_error =   0;
    res->model_error =    0;
    res->request_id =     argument->request_id;
    res->server_struct_setup_code = argument->server_struct_setup_code;
    res->server_version = 0x90100;
    res->accelgrv =       ACCEL_GRV;
    res->e_flat =         E_FLAT_FCTR;
    res->earthrad =       E_FLAT_FCTR;
    res->mmsems =         LMASS_RATIO;
    res->ephepoc =        2000.0;
    res->gauss =          GAUSS_GRAV;
    res->u_grv_cn =       GRAV_CONST;
    res->gmsun =          GRAV_HELIO;
    res->gmmercury =      GRAV_HELIO/6023600.0;
    res->gmvenus =        GRAV_HELIO/408523.71;
    res->gmearth =        GRAV_GEO;
    res->gmmoon =         GRAV_MOON;
    res->gmmars =         GRAV_HELIO/3098708.0;
    res->gmjupiter =      GRAV_HELIO/1047.3486;
    res->gmsaturn =       GRAV_HELIO/3497.90;
    res->gmuranus =       GRAV_HELIO/22902.94;
    res->gmneptune =      GRAV_HELIO/19412.24;
    res->etidelag =       ETIDE_LAG;
    res->love_h =         LOVE_H;
    res->love_l =         LOVE_L;
    res->pre_data =       PRECESS;
    res->rel_data =       1.0;
    res->tidalut1 =       0.0;
    res->au =             ASTR_UNIT;
    res->tsecau =         TAU_A;
    res->vlight =         C_LIGHT;
    return 0;
}
