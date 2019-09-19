/***************************************************************************
 *   Copyright (C) 2015 by Walter Brisken                                  *
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
/*===========================================================================
 * SVN properties (DO NOT CHANGE)
 *
 * $Id$
 * $HeadURL: https://svn.atnf.csiro.au/difx/applications/vex2difx/trunk/src/util.h $
 * $LastChangedRevision$
 * $Author$
 * $LastChangedDate$
 *
 *==========================================================================*/

#include "freq.h"
#include <iomanip>

// Returns index of requested (fq, bw, sb, ...) from freqs.
// If not in freqs, it is added first
int getFreqId(std::vector<freq>& freqs, double fq, double bw, char sb, double isr, double osr, int d, int iz, unsigned int t)
{
	freq newfq(fq, bw, sb, isr, osr, d, iz, t);
	return getFreqId(freqs, newfq);
}

// Returns index of requested (fq, bw, sb, ...) from freqs.
// If not in freqs, it is added first
int getFreqId(std::vector<freq>& freqs, const freq& newfq)
{
	for(std::vector<freq>::const_iterator it = freqs.begin(); it != freqs.end(); ++it)
	{
		if(newfq == *it)
		{
			// use iterator math to get index
			return it - freqs.begin();
		}
	}

	// not in list yet, so add
	freqs.push_back(newfq);

	return freqs.size() - 1;
}

void freq::flip()
{
	if (sideBand == 'U')
	{
		sideBand = 'L';
		fq += bw;
	}
	else
	{
		sideBand = 'U';
		fq -= bw;
	}
}

std::ostream& operator << (std::ostream& os, const freq& f)
{
	os << std::setw(15) << std::setprecision(8)
		<< f.fq*1e-6 << " MHz "<< f.bw*1e-6 << " MHz sb:" << f.sideBand
		<< " z:" << f.isZoomFreq;
	return os;
}
