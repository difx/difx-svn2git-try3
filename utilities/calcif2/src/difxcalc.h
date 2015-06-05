/***************************************************************************
 *   Copyright (C) 2008-2012, 2015 by Walter Brisken & Adam Deller               *
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

#ifndef __DIFX_CALC_H__
#define __DIFX_CALC_H__

#include <difxio.h>
#include "DiFX_Delay_Server.h"

#define MAX_MODEL_OVERSAMP 5

typedef struct
{
    int oversamp;
    int interpol;
    int allowNegDelay;
	int warnSpacecraftPointingSource;
	int useExtraExternalDelay; /* Flag
								  0: Do not use some extra delay software
								  1: Use the calc_Sekido software
							   */
    unsigned int Num_Allocated_Stations;
    unsigned int Num_Allocated_Sources;
    struct getDIFX_DELAY_SERVER_1_arg request;
    struct getDIFX_DELAY_SERVER_1_arg sc_request;
	struct DIFX_DELAY_SERVER_1_station sc_station[3];
	struct DIFX_DELAY_SERVER_1_source sc_source[1];
    CLIENT *clnt;
} CalcParams;

int difxCalcInit(DifxInput *D, CalcParams *p, int verbose);
int CheckInputForSpacecraft(const DifxInput *D, CalcParams *p);
int difxCalc(DifxInput *D, CalcParams *p, const char *prefix, int verbose);

#endif
