/***************************************************************************
 *   Copyright (C) 2008, 2009 by Walter Brisken                            *
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
// $Id: fitsUV.h 2232 2010-06-15 07:10:54Z JohnMorgan $
// $HeadURL: https://svn.atnf.csiro.au/difx/applications/difx2fits/branches/pcal/src/fitsUV.h $
// $LastChangedRevision: 2232 $
// $Author: JohnMorgan $
// $LastChangedDate: 2010-06-15 15:10:54 +0800 (Tue, 15 Jun 2010) $
//
//============================================================================
#ifndef __FITS_PH_H__
#define __FITS_PH_H__

#include "difxio/parsedifx.h"
#include "difx2fits.h"
typedef struct
{
	/*info taken directly from difxio*/
	int dsId, nRecFreq;
	/*Describes the pcals extracted by DiFX. Calculated as with corresponding values in mpifxcorr configuration.cpp*/
	int *nRecFreqPcal;    /*num of recorded pcals in each sub-band*/
	int **recFreqPcalFreq;/*frequency of each pcal indexed by subband and number within subband*/
	//int *recFreqPcalOffsetHz; // shouldn't be needed for difx2fits
	int maxRecPcal;

	/*The following describe the pcals which are to be output*/
	int **freqPcalOut;/*list of tones which will be written out. vex convention*/
	int *nFreqPcalOut;/*length of above list*/
	int maxPcalOut;/*maximum number of pcals in a single subband, maximum over all datastreams used for FITS header*/

	/*bool which says whether each recorded tone will be written out*/
	int **recFreqPcalOut;
} DifxPCal;

DifxPCal *newDifxPCal(const DifxInput *D, int antId);
#endif
