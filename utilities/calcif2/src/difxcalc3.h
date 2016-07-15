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
// $Id: difxcalc.h 6697 2015-06-05 13:26:42Z JamesAnderson $
// $HeadURL: $
// $LastChangedRevision: 6697 $
// $Author: JamesAnderson $
// $LastChangedDate: 2015-06-05 15:26:42 +0200 (Fri, 05 Jun 2015) $
//
//============================================================================

#ifndef __DIFX_CALC_3_H__
#define __DIFX_CALC_3_H__

#include <difxio.h>
#include "DelayHandlerDistributorInterface.h"
#include "DelayHandlerDistributor.h"

#ifdef __cplusplus
using namespace DiFX::Delay::Handler;
#endif


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
	uint_fast32_t Num_CALC_Threads; /* 0 means use environment variable
	                                   DIFX_NUMBERCALCSERVERTHREADS
	                                 */
    uint32_t Num_Allocated_Stations;
    uint32_t Num_Allocated_Sources;
    struct SERVER_MODEL_DELAY_ARGUMENT argument;
    struct SERVER_MODEL_DELAY_ARGUMENT sc_argument;
	struct SERVER_MODEL_DELAY_ARGUMENT_STATION sc_station[3];
	struct SERVER_MODEL_DELAY_ARGUMENT_SOURCE sc_source[1];
    struct SERVER_MODEL_DELAY_RESPONSE response;
    struct SERVER_MODEL_DELAY_RESPONSE sc_response;
	struct SERVER_MODEL_DELAY_RESPONSE_DATA sc_result[3];
	DelayHandlerDistributor* delayDistributor;
	int64_t argument_id;
} CalcParams;

int difxCalcInit(DifxInput *D, CalcParams *p, int verbose);
int CheckInputForSpacecraft(const DifxInput *D, CalcParams *p);
int difxCalc(DifxInput *D, CalcParams *p, const char *prefix, int verbose);

#endif
