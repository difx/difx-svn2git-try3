/***************************************************************************
 *   Copyright (C) 2013, 2014 by Walter Brisken                                  *
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
 * $HeadURL: $
 * $LastChangedRevision$
 * $Author$
 * $LastChangedDate$
 *
 *==========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include "difxio/difx_input.h"

const char program[] = "testephem";
const char author[]  = "Walter Brisken <wbrisken@nrao.edu>";
const char version[] = "0.2";
const char verdate[] = "20140319";

int main(int argc, char **argv)
{
	DifxSpacecraft *ds;

	if(argc != 11)
	{
		printf("%s  ver. %s  %s  %s\n\n", program, version, author, verdate);
		printf("Usage: %s <mjd0> <deltat[s]> <n> <obj> <naif file> <ephem file> <0-Source, 1-Antenna> <ephem type> <orientation file> <JPL planetary ephemeris>\n", argv[0]);

		return 0;
	}

	ds = newDifxSpacecraftArray(1);
        int computeDifxSpacecraftSourceEphemeris(DifxSpacecraft *ds, double mjd0, double deltat, int nPoint, const char *objectName, const char *ephemType, const char *naifFile, const char *ephemFile, const char* orientationFile, const char* JPLplanetaryephem, double ephemStellarAber, double ephemClockError);
        int computeDifxSpacecraftAntennaEphemeris(DifxSpacecraft *ds, double mjd0, double deltat, int nPoint, const char *objectName, const char *ephemType, const char *naifFile, const char *ephemFile, const char* orientationFile, const char* JPLplanetaryephem, double ephemClockError);

        if(argv[7] == '0')
        {
            computeDifxSpacecraftSourceEphemeris(
                   ds, 
                   atof(argv[1]),		/* mjd0 [days] */
                   atof(argv[2])/86400.0,	/* deltat [sec] */
                   atoi(argv[3]),		/* nPoint */
                   argv[4],			/* obj name */
                   argv[8],                     /* ephemType */
                   argv[5],			/* naifFile */
                   argv[6],			/* ephemFile */
                   argv[9],                     /* orientationFile */
                   argv[10],                    /* JPLplanetaryephem */
                   0.0, 0.0);
        }
        else
        {
            computeDifxSpacecraftAntennaEphemeris(
                   ds, 
                   atof(argv[1]),		/* mjd0 [days] */
                   atof(argv[2])/86400.0,	/* deltat [sec] */
                   atoi(argv[3]),		/* nPoint */
                   argv[4],			/* obj name */
                   argv[8],                     /* ephemType */
                   argv[5],			/* naifFile */
                   argv[6],			/* ephemFile */
                   argv[9],                     /* orientationFile */
                   argv[10],                    /* JPLplanetaryephem */
                   0.0);
        }
            
        writeDifxSpacecraftArray(stdout, 1, ds);
            
	deleteDifxSpacecraftArray(ds, 1);

	return 0;
}
