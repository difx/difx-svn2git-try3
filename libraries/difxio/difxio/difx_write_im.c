/***************************************************************************
 *	 Copyright (C) 2008-2015 by Walter Brisken							   *
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

#include <stdlib.h>
#include <string.h>
#include <float.h>
#include "difxio/difx_write.h"

int writeDifxIM(const DifxInput* const D)
{
	FILE *out;
	const DifxScan* scan;
	const DifxAntenna* antenna;
	int a, s, p, i;
	int refAnt, order;

	if(!D)
	{
		return -1;
	}

	if(D->nJob != 1)
	{
		fprintf(stderr, "writeDifxIM: nJob = %d (not 1)\n", D->nJob);

		return -1;
	}

	if(D->nAntenna < 1 || !D->job || D->nScan < 0)
	{
		return 0;
	}

	out = fopen(D->job->imFile, "w");
	if(!out)
	{
		fprintf(stderr, "Cannot open %s for write\n", D->job->imFile);

		return -1;
	}

	if(D->job->delayServerHost != 0)
	{
		writeDifxLine(out, "DELAY SERVER HOST", D->job->delayServerHost);		
	}
	writeDifxLine(out, "DELAY SERVER TYPE", delayServerTypeNames[D->job->delayServerType]);
	writeDifxLineULong(out, "DELAY VERSION", D->job->delayVersion);
	writeDifxLineULong(out, "DELAY PROGRAM", D->job->delayProgram);
	writeDifxLineULong(out, "DELAY HANDLER", D->job->delayHandler);
	writeDifxLineULong(out, "DELAY DETAILED VERSION", D->job->delayProgramDetailedVersion);

	if(D->fracSecondStartTime > 0)
	{
		writeDifxDateLines(out, truncSeconds(D->job->mjdStart));
	}
	else
	{
		writeDifxDateLines(out, roundSeconds(D->job->mjdStart));
	}
	
	writeDifxLineInt(out, "POLYNOMIAL ORDER", D->job->polyOrder);
	writeDifxLineInt(out, "INTERVAL (SECS)", D->job->polyInterval);
	writeDifxLine(out,    "ABERRATION CORR", aberCorrStrings[D->job->aberCorr]);
	writeDifxLineBoolean(out, "CALC_OWN_RETARDED_POSITION", D->job->calculate_own_retarded_position);
	if((D->job->calcParamTable))
	{
		writeDifxLineDouble(out,"CALCPARAM ACCELGRV",  "%24.16e", D->job->calcParamTable->accelgrv);
		writeDifxLineDouble(out,"CALCPARAM E-FLAT",    "%24.16e", D->job->calcParamTable->e_flat);
		writeDifxLineDouble(out,"CALCPARAM EARTHRAD",  "%24.16e", D->job->calcParamTable->earthrad);
		writeDifxLineDouble(out,"CALCPARAM MMSEMS",    "%24.16e", D->job->calcParamTable->mmsems);
		writeDifxLineDouble(out,"CALCPARAM EPHEPOC",   "%24.16e", D->job->calcParamTable->ephepoc);
		writeDifxLineDouble(out,"CALCPARAM GAUSS",     "%24.16e", D->job->calcParamTable->gauss);
		writeDifxLineDouble(out,"CALCPARAM U-GRV-CN",  "%24.16e", D->job->calcParamTable->u_grv_cn);
		writeDifxLineDouble(out,"CALCPARAM GMSUN",     "%24.16e", D->job->calcParamTable->gmsun);
		writeDifxLineDouble(out,"CALCPARAM GMMERCURY", "%24.16e", D->job->calcParamTable->gmmercury);
		writeDifxLineDouble(out,"CALCPARAM GMVENUS",   "%24.16e", D->job->calcParamTable->gmvenus);
		writeDifxLineDouble(out,"CALCPARAM GMEARTH",   "%24.16e", D->job->calcParamTable->gmearth);
		writeDifxLineDouble(out,"CALCPARAM GMMOON",    "%24.16e", D->job->calcParamTable->gmmoon);
		writeDifxLineDouble(out,"CALCPARAM GMMARS",    "%24.16e", D->job->calcParamTable->gmmars);
		writeDifxLineDouble(out,"CALCPARAM GMJUPITER", "%24.16e", D->job->calcParamTable->gmjupiter);
		writeDifxLineDouble(out,"CALCPARAM GMSATURN",  "%24.16e", D->job->calcParamTable->gmsaturn);
		writeDifxLineDouble(out,"CALCPARAM GMURANUS",  "%24.16e", D->job->calcParamTable->gmuranus);
		writeDifxLineDouble(out,"CALCPARAM GMNEPTUNE", "%24.16e", D->job->calcParamTable->gmneptune);
		writeDifxLineDouble(out,"CALCPARAM ETIDELAG",  "%24.16e", D->job->calcParamTable->etidelag);
		writeDifxLineDouble(out,"CALCPARAM LOVE_H",    "%24.16e", D->job->calcParamTable->love_h);
		writeDifxLineDouble(out,"CALCPARAM LOVE_L",    "%24.16e", D->job->calcParamTable->love_l);
		writeDifxLineDouble(out,"CALCPARAM PRE_DATA",  "%24.16e", D->job->calcParamTable->pre_data);
		writeDifxLineDouble(out,"CALCPARAM REL_DATA",  "%24.16e", D->job->calcParamTable->rel_data);
		writeDifxLineDouble(out,"CALCPARAM TIDALUT1",  "%24.16e", D->job->calcParamTable->tidalut1);
		writeDifxLineDouble(out,"CALCPARAM AU",        "%24.16e", D->job->calcParamTable->au);
		writeDifxLineDouble(out,"CALCPARAM TSECAU",    "%24.16e", D->job->calcParamTable->tsecau);
		writeDifxLineDouble(out,"CALCPARAM VLIGHT",    "%24.16e", D->job->calcParamTable->vlight);
	}

	writeDifxAntennaArray(out, D->nAntenna, D->antenna, 0, 0, 0, 0, 0, 0);

	writeDifxLineInt(out, "NUM SCANS", D->nScan);

	for(s = 0; s < D->nScan; ++s)
	{
		scan = D->scan + s;

		writeDifxLine1(out, "SCAN %d POINTING SRC", s, D->source[scan->pointingCentreSrc].name);
		writeDifxLineInt1(out, "SCAN %d NUM PHS CTRS", s, scan->nPhaseCentres);
		for(i = 0; i < scan->nPhaseCentres; ++i)
		{
			writeDifxLine2(out, "SCAN %d PHS CTR %d SRC", s, i, D->source[scan->phsCentreSrcs[i]].name);
		}

		if(!scan->im)
		{
			writeDifxLineInt1(out, "SCAN %d NUM POLY", s, 0);
			fprintf(stderr, "Warning: Scan %d being written with no delay model information\n", s);
			continue;
		}
		
		for(refAnt = 0; refAnt < scan->nAntenna; ++refAnt)
		{
			if(scan->im[refAnt])
			{
				break;
			}
		}
		if(refAnt == scan->nAntenna)
		{
			writeDifxLineInt1(out, "SCAN %d NUM POLY", s, 0);
			continue;
		}

		writeDifxLineInt1(out,     "SCAN %d NUM POLY", s, scan->nPoly);
		writeDifxLineBoolean1(out, "SCAN %d LMN EXT EXISTS", s, scan->imLMN != NULL);
		writeDifxLineBoolean1(out, "SCAN %d XYZ EXT EXISTS", s, scan->imXYZ != NULL);
		
		for(p = 0; p < scan->nPoly; ++p)
		{
			writeDifxLineInt2(out, "SCAN %d POLY %d MJD", s, p, scan->im[refAnt][0][p].mjd);
			writeDifxLineInt2(out, "SCAN %d POLY %d SEC", s, p, scan->im[refAnt][0][p].sec);
			for(i = 0; i < scan->nPhaseCentres+1; ++i)
			{
				for(a = 0; a < scan->nAntenna; ++a)
				{
					antenna = D->antenna + a;
					if(scan->im[a] == 0)
					{
						continue;
					}
					if(scan->im[a][i] == NULL)
					{
						continue;
					}
					order = scan->im[a][i][p].order;
					writeDifxLineDouble2(out, "SRC %d ANT %d DELTA UVW", i, a, "%.16e", scan->im[a][i][0].delta);
					writeDifxLineArray2(out,  "SRC %d ANT %d DELAY (us)", i, a, scan->im[a][i][p].delay, order+1);
					writeDifxLineArray2(out,  "SRC %d ANT %d DRY (us)", i, a, scan->im[a][i][p].dry, order+1);
					writeDifxLineArray2(out,  "SRC %d ANT %d WET (us)", i, a, scan->im[a][i][p].wet, order+1);
					writeDifxLineArray2(out,  "SRC %d ANT %d IONO (us at 1 GHz)", i, a, scan->im[a][i][p].iono, order+1);
					if(antenna->spacecraftId >= 0)
					{
						writeDifxLineArray2(out, "SRC %d ANT %d SC_GS_DELAY (us)", i, a, scan->im[a][i][p].sc_gs_delay, order+1);
						writeDifxLineArray2(out, "SRC %d ANT %d GS_SC_DELAY (us)", i, a, scan->im[a][i][p].gs_sc_delay, order+1);
						writeDifxLineArray2(out, "SRC %d ANT %d GS_CLOCK_DELAY (us)", i, a, scan->im[a][i][p].gs_clock_delay, order+1);
					}
					writeDifxLineArray2(out,  "SRC %d ANT %d AZ", i, a, scan->im[a][i][p].az, order+1);
					writeDifxLineArray2(out,  "SRC %d ANT %d EL CORR", i, a, scan->im[a][i][p].elcorr, order+1);
					writeDifxLineArray2(out,  "SRC %d ANT %d EL GEOM", i, a, scan->im[a][i][p].elgeom, order+1);
					writeDifxLineArray2(out,  "SRC %d ANT %d PAR ANGLE", i, a, scan->im[a][i][p].parangle, order+1);
					writeDifxLineArray2(out,  "SRC %d ANT %d MSA (rad)", i, a, scan->im[a][i][p].msa, order+1);
					writeDifxLineArray2(out,  "SRC %d ANT %d U (m)", i, a, scan->im[a][i][p].u, order+1);
					writeDifxLineArray2(out,  "SRC %d ANT %d V (m)", i, a, scan->im[a][i][p].v, order+1);
					writeDifxLineArray2(out,  "SRC %d ANT %d W (m)", i, a, scan->im[a][i][p].w, order+1);
					if((scan->imLMN))
					{
						if((scan->imLMN[a]))
						{
							if((scan->imLMN[a][i]))
							{
								writeDifxLineDouble2(out, "SRC %d ANT %d DELTA LMN", i, a, "%.16e", scan->imLMN[a][i][0].delta);
								writeDifxLineArray2(out,  "SRC %d ANT %d dDELAYdL", i, a, scan->imLMN[a][i][p].dDelay_dl, order+1);
								writeDifxLineArray2(out,  "SRC %d ANT %d dDELAYdM", i, a, scan->imLMN[a][i][p].dDelay_dm, order+1);
								writeDifxLineArray2(out,  "SRC %d ANT %d dDELAYdN", i, a, scan->imLMN[a][i][p].dDelay_dn, order+1);
								if(!isnan(scan->imLMN[a][i][p].d2Delay_dldl[0]))
								{
									writeDifxLineArray2(out, "SRC %d ANT %d d2DELAYdLdL", i, a, scan->imLMN[a][i][p].d2Delay_dldl, order+1);
									writeDifxLineArray2(out, "SRC %d ANT %d d2DELAYdLdM", i, a, scan->imLMN[a][i][p].d2Delay_dldm, order+1);
									writeDifxLineArray2(out, "SRC %d ANT %d d2DELAYdLdN", i, a, scan->imLMN[a][i][p].d2Delay_dldn, order+1);
									writeDifxLineArray2(out, "SRC %d ANT %d d2DELAYdMdM", i, a, scan->imLMN[a][i][p].d2Delay_dmdm, order+1);
									writeDifxLineArray2(out, "SRC %d ANT %d d2DELAYdMdN", i, a, scan->imLMN[a][i][p].d2Delay_dmdn, order+1);
									writeDifxLineArray2(out, "SRC %d ANT %d d2DELAYdNdN", i, a, scan->imLMN[a][i][p].d2Delay_dndn, order+1);
								}
							}
						}
					}
					if((scan->imXYZ))
					{
						if((scan->imXYZ[a]))
						{
							if((scan->imXYZ[a][i]))
							{
								writeDifxLineDouble2(out, "SRC %d ANT %d DELTA LMN", i, a, "%.16e", scan->imXYZ[a][i][0].delta);
								writeDifxLineArray2(out,  "SRC %d ANT %d dDELAYdX", i, a, scan->imXYZ[a][i][p].dDelay_dX, order+1);
								writeDifxLineArray2(out,  "SRC %d ANT %d dDELAYdY", i, a, scan->imXYZ[a][i][p].dDelay_dY, order+1);
								writeDifxLineArray2(out,  "SRC %d ANT %d dDELAYdZ", i, a, scan->imXYZ[a][i][p].dDelay_dZ, order+1);
								if(!isnan(scan->imXYZ[a][i][p].d2Delay_dXdX[0]))
								{
									writeDifxLineArray2(out, "SRC %d ANT %d d2DELAYdXdX", i, a, scan->imXYZ[a][i][p].d2Delay_dXdX, order+1);
									writeDifxLineArray2(out, "SRC %d ANT %d d2DELAYdXdY", i, a, scan->imXYZ[a][i][p].d2Delay_dXdY, order+1);
									writeDifxLineArray2(out, "SRC %d ANT %d d2DELAYdXdZ", i, a, scan->imXYZ[a][i][p].d2Delay_dXdZ, order+1);
									writeDifxLineArray2(out, "SRC %d ANT %d d2DELAYdYdY", i, a, scan->imXYZ[a][i][p].d2Delay_dYdY, order+1);
									writeDifxLineArray2(out, "SRC %d ANT %d d2DELAYdYdZ", i, a, scan->imXYZ[a][i][p].d2Delay_dYdZ, order+1);
									writeDifxLineArray2(out, "SRC %d ANT %d d2DELAYdZdZ", i, a, scan->imXYZ[a][i][p].d2Delay_dZdZ, order+1);
								}
							}
						}
					}
				}
			}
		}
	}

	fclose(out);

	return 0;
}
