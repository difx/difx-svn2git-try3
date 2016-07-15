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
#include <rpc/pmap_clnt.h>
#include "DiFX_Delay_Server.h"


#ifndef DIFX_DELAY_SERVER_PARAMS_H
#define DIFX_DELAY_SERVER_PARAMS_H 1

extern int process_DiFX_Delay_Server_Parameters_1(FILE* flog, const unsigned int count, struct svc_req *pRequest, SVCXPRT *pTransport);

extern int process_DiFX_Delay_Server_Parameters_1_CALCPROG(const struct getDIFX_DELAY_SERVER_PARAMETERS_1_arg* const argument, struct DIFX_DELAY_SERVER_PARAMETERS_1_res* const res);
extern int process_DiFX_Delay_Server_Parameters_1_CALC_9_1_RAPROG(const struct getDIFX_DELAY_SERVER_PARAMETERS_1_arg* const argument, struct DIFX_DELAY_SERVER_PARAMETERS_1_res* const res);



#endif
