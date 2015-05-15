/***************************************************************************
 *	 Copyright (C) 2007-2011, 2014, 2015 by Walter Brisken & Adam Deller			   *
 *																		   *
 *	 This program is free software; you can redistribute it and/or modify  *
 *	 it under the terms of the GNU General Public License as published by  *
 *	 the Free Software Foundation; either version 3 of the License, or	   *
 *	 (at your option) any later version.								   *
 *																		   *
 *	 This program is distributed in the hope that it will be useful,	   *
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of		   *
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the		   *
 *	 GNU General Public License for more details.						   *
 *																		   *
 *	 You should have received a copy of the GNU General Public License	   *
 *	 along with this program; if not, write to the						   *
 *	 Free Software Foundation, Inc.,									   *
 *	 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.			   *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "difxio/difx_input.h"
#include "difxio/parsedifx.h"
#include "difxio/difx_write.h"

/* These names must match what delay server expects */
const char sourceCoordinateFrameTypeNames[][MAX_SOURCE_COORDINATE_FRAME_STRING_LENGTH] =
	{
		"UNKNOWN",
		"",
		"J2000",
		"J2000_CMB",
		"J2000_CMB_1",
		"J2000_MWB",
		"J2000_MWB_1",
		"J2000_SSB",
		"J2000_Earth",
		"ITRF2008",
		"OTHER"		/* don't expect the right parallactic angle or delay model! */
	};
enum SourceCoordinateFrameType stringToSourceCoordinateFrameType(const char* str)
{
	enum SourceCoordinateFrameType s;
	for(s=SourceCoordinateFrameUnknown; s < NumSourceCoordinateFrames; ++s)
	{
		if(s == SourceCoordinateFrameDefault)
		{
			continue;
		}
		if(strcmp(str, sourceCoordinateFrameTypeNames[s]) == 0)
		{
			return s;
		}
	}
	return SourceCoordinateFrameUnknown;
}



DifxSource *newDifxSourceArray(int nSource)
{
	DifxSource *ds;
	int s;

	ds = (DifxSource *)calloc(nSource, sizeof(DifxSource));
	for(s = 0; s < nSource; s++)
	{
		ds[s].spacecraftId = -1;
		ds[s].sc_epoch = 0.0;
		ds[s].numFitsSourceIds = 0;
		ds[s].fitsSourceIds = 0;
		ds[s].pmRA = 0.0;
		ds[s].pmDec = 0.0;
		ds[s].parallax = 0.0;
		ds[s].pmEpoch = 0.0;
		ds[s].qual = 0;
		ds[s].calCode[0] = 0;
		ds[s].name[0] = 0;
		ds[s].coord_frame = SourceCoordinateFrameUnknown;
		ds[s].perform_uvw_deriv = PerformDirectionDerivativeDefault;
		ds[s].perform_lmn_deriv = PerformDirectionDerivativeDefault;
		ds[s].perform_xyz_deriv = PerformDirectionDerivativeDefault;
		ds[s].delta_lmn = 0.0;
		ds[s].delta_xyz = 0.0;
	}
	
	return ds;
}

void deleteDifxSourceArray(DifxSource *ds, int nSource)
{
	int i;
	if(ds)
	{
		for(i=0;i<nSource;i++)
		{
			free(ds[i].fitsSourceIds);
		}
		free(ds);
	}
}

void fprintDifxSource(FILE *fp, const DifxSource *ds)
{
	int i;
	fprintf(fp, "  DifxSource [%s] : %p\n", ds->name, ds);
	fprintf(fp, "	 RA	 =	%10.7f\n", ds->ra);
	fprintf(fp, "	 Dec = %+11.7f\n", ds->dec);
	fprintf(fp, "	 Calcode = %s\n", ds->calCode);
	fprintf(fp, "	 coord_frame = %s\n", sourceCoordinateFrameTypeNames[ds->coord_frame]);
	fprintf(fp, "	 perform_uvw_deriv = %s\n", performDirectionDerivativeTypeNames[ds->perform_uvw_deriv]);
	fprintf(fp, "	 perform_lmn_deriv = %s\n", performDirectionDerivativeTypeNames[ds->perform_lmn_deriv]);
	fprintf(fp, "	 perform_xyz_deriv = %s\n", performDirectionDerivativeTypeNames[ds->perform_xyz_deriv]);
	fprintf(fp, "	 delta_lmn = %E\n", ds->delta_lmn);
	fprintf(fp, "	 delta_xyz = %E\n", ds->delta_xyz);
	fprintf(fp, "	 delta_xyz_used = %E\n", ds->delta_lmn_used);
	fprintf(fp, "	 delta_xyz_used = %E\n", ds->delta_xyz_used);
	fprintf(fp, "	 Qualifier = %d\n", ds->qual);
	fprintf(fp, "	 SpacecraftId = %d\n", ds->spacecraftId);
	fprintf(fp, "    SC_Name = %s\n", ds->sc_name);
	fprintf(fp, "	 SC_Epoch = %11.7f\n", ds->sc_epoch);
	fprintf(fp, "	 Num FITS SourceIds = %d\n", ds->numFitsSourceIds);
	for(i=0;i<ds->numFitsSourceIds;i++)
	{
		fprintf(fp, "	 FITS SourceId[%d] = %d\n", i, ds->fitsSourceIds[i]);
	}
	fprintf(fp, "	 pmRA = %E\n", ds->pmRA);
	fprintf(fp, "	 pmDec = %E\n", ds->pmDec);
	fprintf(fp, "	 parallax = %E\n", ds->parallax);
	fprintf(fp, "	 pmEpoch = %f\n", ds->pmEpoch);
	fprintf(fp, "	 station0PropDelay = %E\n", ds->station0PropDelay);
}

void printDifxSource(const DifxSource *ds)
{
	fprintDifxSource(stdout, ds);
}

void fprintDifxSourceSummary(FILE *fp, const DifxSource *ds)
{
	fprintf(fp, "  %s\n", ds->name);
	fprintf(fp, "	 RA	 =	%10.7f\n", ds->ra);
	fprintf(fp, "	 Dec = %+11.7f\n", ds->dec);
	fprintf(fp, "	 Calcode = %s\n", ds->calCode);
	fprintf(fp, "	 Qualifier = %d\n", ds->qual);
	if(ds->spacecraftId >= 0)
	{
		fprintf(fp, "	 SpacecraftId = %d\n", ds->spacecraftId);
		fprintf(fp, "	 SC_Name = %s\n", ds->sc_name);
		fprintf(fp, "	 SC_Epoch = %11.7f\n", ds->sc_epoch);
	}
}

void printDifxSourceSummary(const DifxSource *ds)
{
	fprintDifxSourceSummary(stdout, ds);
}

int isSameDifxSourceBasic(const DifxSource *ds1, const DifxSource *ds2)
{
	if(strcmp(ds1->name, ds2->name) == 0		  &&
	   ds1->ra				 == ds2->ra			  &&
	   ds1->dec				 == ds2->dec		  &&
	   strcmp(ds1->calCode,ds2->calCode) == 0	  &&
	   ds1->coord_frame		 == ds2->coord_frame  &&
	   ds1->perform_uvw_deriv == ds2->perform_uvw_deriv  &&
	   ds1->perform_lmn_deriv == ds2->perform_lmn_deriv  &&
	   ds1->perform_xyz_deriv == ds2->perform_xyz_deriv  &&
	   ds1->delta_lmn		 == ds2->delta_lmn  &&
	   ds1->delta_xyz		 == ds2->delta_xyz  &&
	   ds1->qual			 == ds2->qual		  &&
	   ds1->spacecraftId	 == ds2->spacecraftId &&
	   strcmp(ds1->sc_name,ds2->sc_name) == 0	  &&
	   ds1->sc_epoch		 == ds2->sc_epoch	  &&
	   ds1->numFitsSourceIds == ds2->numFitsSourceIds &&
	   ds1->pmRA			 == ds2->pmRA		  &&
	   ds1->pmDec			 == ds2->pmDec		  &&
	   ds1->parallax		 == ds2->parallax	  &&
	   ds1->pmEpoch			 == ds2->pmEpoch)
	{
		return 1;
	}
	return 0;
}

int isSameDifxSource(const DifxSource *ds1, const DifxSource *ds2)
{
	int i;
	//printf("About to do the source compare on two sources with values %p, %p\n", ds1, ds2);
	//printf("Comparing source called %s with %s\n", ds1->name, ds2->name);
	if(isSameDifxSourceBasic(ds1, ds2))
	{
		for(i=0;i<ds1->numFitsSourceIds;i++)
		{
			if(ds1->fitsSourceIds[i] != ds2->fitsSourceIds[i])
			{
				return 0;
			}
		}
		return 1;
	}
	else
	{
		return 0;
	}
}

void copyDifxSource(DifxSource *dest, const DifxSource *src)
{
	int i;
	if(dest != src)
	{
		free(dest->fitsSourceIds);
		*dest = *src;
		/* dest->ra		   = src->ra; */
		/* dest->dec		   = src->dec; */
		/* dest->qual		   = src->qual; */
		/* dest->spacecraftId = src->spacecraftId; */
		/* dest->sc_epoch	   = src->sc_epoch; */
		/* dest->numFitsSourceIds = src->numFitsSourceIds; */
		/* dest->pmRA		   = src->pmRA; */
		/* dest->pmDec		   = src->pmDec; */
		/* dest->parallax	   = src->parallax; */
		/* dest->pmEpoch	   = src->pmEpoch; */
		if(src->fitsSourceIds)
		{
			dest->fitsSourceIds = (int*)malloc(src->numFitsSourceIds * sizeof(int));
			for(i=0;i<src->numFitsSourceIds;i++)
			{
				dest->fitsSourceIds[i] = src->fitsSourceIds[i];
			}
		}
		/* snprintf(dest->calCode, DIFXIO_CALCODE_LENGTH, "%s", src->calCode); */
		/* snprintf(dest->name, DIFXIO_NAME_LENGTH, "%s", src->name); */
	}
}

/* merge two DifxSource tables into an new one.	 sourceIdRemap will contain the
 * mapping from ds2's old source entries to that of the merged set
 */
DifxSource *mergeDifxSourceArrays(const DifxSource *ds1, int nds1,
								  const DifxSource *ds2, int nds2, int *sourceIdRemap,
								  int *nds)
{
	DifxSource *ds;
	int i, j;

	*nds = nds1;

	/* first identify entries that differ and assign new sourceIds to them */
	for(j = 0; j < nds2; j++)
	{
		for(i = 0; i < nds1; i++)
		{
			if(isSameDifxSource(ds1 + i, ds2 + j))
			{
				sourceIdRemap[j] = i;
				break;
			}
		}
		if(i == nds1)
		{
			sourceIdRemap[j] = *nds;
			(*nds)++;
		}
	}

	/* Allocate and copy */
	ds = newDifxSourceArray(*nds);
	for(i = 0; i < nds1; i++)
	{
		copyDifxSource(ds + i, ds1 + i);
	}
	for(j = 0; j < nds2; j++)
	{
		i = sourceIdRemap[j];
		if(i >= nds1)
		{
			copyDifxSource(ds + i, ds2 + j);
		}
	}

	return ds;
}


int writeDifxSourceArray(FILE *out, int nSource, const DifxSource *ds,
						 int doCalcode, int doQual, int doSpacecraftID)
{
	int n;	/* number of lines written */
	int i;

	writeDifxLineInt(out, "NUM SOURCES", nSource);
	n = 1;

	for(i = 0; i < nSource; i++)
	{
		writeDifxLine1(out,        "SOURCE %d NAME", i, ds[i].name);
		++n;
		writeDifxLineDouble1(out,  "SOURCE %d RA", i, "%19.16f", ds[i].ra);
		++n;
		writeDifxLineDouble1(out,  "SOURCE %d DEC", i, "%19.16f",  ds[i].dec);
		++n;
		if(doCalcode) 
		{
			writeDifxLine1(out,    "SOURCE %d CALCODE", i, ds[i].calCode);
			++n;
		}
		writeDifxLine1(out,        "SOURCE %d FRAME", i, sourceCoordinateFrameTypeNames[ds[i].coord_frame]);
		++n;
		writeDifxLine1(out,        "SOURCE %d PERFORM UVW", i, performDirectionDerivativeTypeNames[ds[i].perform_uvw_deriv]);
		++n;
		writeDifxLine1(out,        "SOURCE %d PERFORM LMN", i, performDirectionDerivativeTypeNames[ds[i].perform_lmn_deriv]);
		++n;
		writeDifxLine1(out,        "SOURCE %d PERFORM XYZ", i, performDirectionDerivativeTypeNames[ds[i].perform_xyz_deriv]);
		++n;
		writeDifxLineDouble1(out,  "SOURCE %d DELTA LMN", i, "%24.16e",  ds[i].delta_lmn);
		++n;
		writeDifxLineDouble1(out,  "SOURCE %d DELTA XYZ", i, "%24.16e",  ds[i].delta_xyz);
		++n;
		if(doQual)
		{
			writeDifxLineInt1(out, "SOURCE %d QUAL", i, ds[i].qual);
			++n;
		}
		writeDifxLineDouble1(out,  "SOURCE %d PM RA (ARCSEC/YR)", i, "%24.16e",  ds[i].pmRA);
		++n;
		writeDifxLineDouble1(out,  "SOURCE %d PM DEC (ARCSEC/YR)", i, "%24.16e",  ds[i].pmDec);
		++n;
		writeDifxLineDouble1(out,  "SOURCE %d PARALLAX (ARCSEC)", i, "%24.16e",  ds[i].parallax);
		++n;
		if(ds[i].pmEpoch==0.0)
		{
			writeDifxLineDouble1(out,  "SOURCE %d PM EPOCH (MJD)", i, "%3.1f",  ds[i].pmEpoch);
		}
		else
		{
			writeDifxLineDouble1(out,  "SOURCE %d PM EPOCH (MJD)", i, "%19.16f",  ds[i].pmEpoch);
		}
		++n;
		if(doSpacecraftID)
		{
			writeDifxLineInt1(out, "SOURCE %d S/CRAFT ID", i, ds[i].spacecraftId);
			++n;
			if(ds[i].sc_name[0] != 0)
			{
				writeDifxLine1(out, "SOURCE %d S/CRAFT NAME", i,	 ds[i].sc_name);
				++n;
			}
			if(ds[i].sc_epoch==0.0)
			{
				writeDifxLineDouble1(out, "SOURCE %d S/CRAFT EPOCH", i, "%3.1f",	 ds[i].sc_epoch);
			}
			else
			{
				writeDifxLineDouble1(out, "SOURCE %d S/CRAFT EPOCH", i, "%18.12f",  ds[i].sc_epoch);
			}
			++n;
		}
	}

	return n;
}
