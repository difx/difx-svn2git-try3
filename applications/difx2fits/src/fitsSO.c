/***************************************************************************
 *   Copyright (C) 2008-2012, 2014, 2015 by Walter Brisken                             *
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
// $HeadURL$
// $LastChangedRevision$
// $Author$
// $LastChangedDate$
//
//============================================================================
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include <strings.h>
#include "config.h"
#include "difx2fits.h"

const DifxInput *DifxInput2FitsSO(const DifxInput *D, struct fits_keywords *p_fits_keys, struct fitsPrivate *out)
{
	struct fitsBinTableColumn columns[] =
		{
			{"SPACECR",    "16A", "spacecraft name", 0},
			{"ANTENNA_NO", "1J",  "antenna number", 0},
			{"SUBARRAY",   "1J",  "subarray number", 0},
			{"TIME",       "1D",  "UT time", "DAYS"},
			{"ORBXYZ",     "3D",  "geocentric coordinates", "METERS"},
			{"VELXYZ",     "3D",  "velcity vector", "METERS/SEC"},
			{"ORBOFFXYZ",  "3D",  "model position offset applied", "METERS"},
			{"VELOFFXYZ",  "3D",  "model velocity offset applied", "METERS/SEC"},
			{"SUN_ANGLE",  "3E",  "angle between direction to the Sun and obs", "DEGREES"},
			{"ECLIPSE",    "4E",  "time since entering and leaving Earth's shadow", "DAYS"},
			{"ISANTENNA",  "1L",  "spacecraft is antenna", 0}
		};

	int nColumn;
	int nRowBytes;
	char *fitsbuf;
	char *p_fitsbuf;
	const DifxAntenna* antenna;
	int s;
	int a;
	int32_t antId=-1;
	int32_t subarray=-1;
	float sunAngle[3];
	float eclipse[4];
	
	int nRec = 0;
	static const union
	{
		uint64_t u64;
		double d;
		float f;
	} fitsnan = {UINT64_C(0xFFFFFFFFFFFFFFFF)};
	
	if(D == 0)
	{
		return 0;
	}

	if(D->nSpacecraft == 0 || !D->spacecraft)
	{
		return D;
	}

	subarray = D->job->subarrayId;
	sunAngle[0] = sunAngle[1] = sunAngle[2] = fitsnan.f;
	eclipse[0] = eclipse[1] = eclipse[2] = eclipse[3] = fitsnan.f;

	nColumn = NELEMENTS(columns);
	nRowBytes = FitsBinTableSize(columns, nColumn);

	fitsbuf = (char *)calloc(nRowBytes, 1);
	if(fitsbuf == 0)
	{
		return 0;
	}
	
	for(s = 0; s < D->nSpacecraft; ++s)
	{
		int p;
		char name[16];
		
		strcpypad(name, D->spacecraft[s].name, 16);

		antId = -1;
		for(a = 0; a < D->nAntenna; ++a)
		{				
			antenna = D->antenna + a;
			if(antenna->spacecraftId == s)
			{
				/* Convert difx antId to FITS antennaId */
				antId = a + 1;
				break;
			}
		}

		for(p = 0; p < D->spacecraft[s].nPoint; ++p)
		{
			double xyz[3], vel[3];
			const sixVector *pos;
			nineVector offset;
			double time;
			char isantenna;
			pos = D->spacecraft[s].pos + p;

			time = pos->mjd + pos->fracDay - (int)(D->mjdStart);
			xyz[0] = pos->X;
			xyz[1] = pos->Y;
			xyz[2] = pos->Z;
			vel[0] = pos->dX;
			vel[1] = pos->dY;
			vel[2] = pos->dZ;
			isantenna = (D->spacecraft[s].is_antenna) ? 'T':'F';
			evaluateDifxSpacecraftAntennaOffset(D->spacecraft+s,
			                                    pos->mjd,
			                                    pos->fracDay,
			                                   &offset);

			p_fitsbuf = fitsbuf;

			if(nRec == 0)
			{
				fitsWriteBinTable(out, nColumn, columns, nRowBytes, "SPACECRAFT_ORBIT");
				arrayWriteKeys(p_fits_keys, out);
				fitsWriteInteger(out, "TABREV", 2, "");
				fitsWriteEnd(out);
			}

			FITS_WRITE_ARRAY (name, p_fitsbuf, 16);
			FITS_WRITE_ITEM  (antId, p_fitsbuf);
			FITS_WRITE_ITEM  (subarray, p_fitsbuf);
			FITS_WRITE_ITEM  (time, p_fitsbuf);
			FITS_WRITE_ARRAY (xyz, p_fitsbuf, 3);
			FITS_WRITE_ARRAY (vel, p_fitsbuf, 3);
			FITS_WRITE_ARRAY (&(offset.X), p_fitsbuf, 3);
			FITS_WRITE_ARRAY (&(offset.dX), p_fitsbuf, 3);
			FITS_WRITE_ARRAY (&(sunAngle[0]), p_fitsbuf, 3);
			FITS_WRITE_ARRAY (&(eclipse[0]), p_fitsbuf, 4);
			FITS_WRITE_ITEM  (isantenna, p_fitsbuf);

			testFitsBufBytes(p_fitsbuf - fitsbuf, nRowBytes, "SO");

#ifndef WORDS_BIGENDIAN
			FitsBinRowByteSwap(columns, nColumn, fitsbuf);
#endif
			fitsWriteBinRow(out, fitsbuf);

			++nRec;
		}
	}

	free(fitsbuf);

	return D;
}	
