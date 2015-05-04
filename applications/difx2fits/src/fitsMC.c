/***************************************************************************
 *   Copyright (C) 2008-2015 by Walter Brisken & Adam Deller               *
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
#include <strings.h>
#include <stdint.h>
#include "config.h"
#include "difx2fits.h"



/* Note: This is a particular NaN variant the FITS-IDI format/convention 
 * wants, namely 0xFFFFFFFFFFFFFFFF */
static const union
{
	uint64_t u64;
	double d;
	float f;
} fitsnan = {UINT64_C(0xFFFFFFFFFFFFFFFF)};

/* static void copyPolyModelArray(const int order, const double* const in, double* const out) */
/* { */
/* 	int j; */
/* 	for(j=0; j <= order; j++) */
/* 	{ */
/* 		out[j] = in[j]; */
/* 	} */
/* 	return; */
/* } */
/* In the following function, there should be no problem if
   out==in0 or out==in1
*/
static void addPolyModelArrays(const int order, const double* const in0, const double* in1, double* const out)
{
	int j;
	for(j=0; j <= order; j++)
	{
		out[j] = in0[j] + in1[j];
	}
	return;
}
/* /\* In the following function, there should be no problem if */
/*    out==in0 or out==in1 */
/* *\/ */
/* static void subtractPolyModelArrays(const int order, const double* const in0, const double* in1, double* const out) */
/* { */
/* 	int j; */
/* 	for(j=0; j <= order; j++) */
/* 	{ */
/* 		out[j] = in0[j] - in1[j]; */
/* 	} */
/* 	return; */
/* } */
/* Prepare two arrays to hold multiplication factors to calculate the
   polynomical value (0) at a time offset x into the polynomial, and
   the first derivative with time (1) at the time offset x into the
   polynomial.  The resulting multiplication factor arrays (factors_0 and
   factors_1) can be used to evaluate many polynomials with the same
   timeranges.
*/
static void preparePolyModelFactors(const int order, const double x, double* const factors_0, double* const factors_1)
{
	double x_j, x_j_m1;
	int j;
	x_j = 1.0;
	x_j_m1 = 0.0;
	for(j=0; j <= order; ++j)
	{
		factors_0[j] = x_j;
		factors_1[j] = j * x_j_m1;
		x_j_m1 = x_j;
		x_j *= x;
	}
	return;
}
/* Apply the polynomial multiplication factors to get the polynomial
   value (value_0) and the first derivative of the polynomial value
   (value_1) using the factor arrays computed from the function above.
   This function assumes that higher order terms are small, and so their
   contribution is calculated first to preserve accuracy.
*/
static void applyPolyModelFactors(const int order, const double scale, const double* const poly, const double* const factors_0, double* const factors_1, double* const value_0, double* const value_1)
{
	double v0=0.0;
	double v1=0.0;
	int j;
	for(j = order; j >= 0; --j)
	{
		v0 += poly[j] * factors_0[j];
		v1 += poly[j] * factors_1[j];
	}
	v0 *= scale;
	v1 *= scale;
	if(isfinite(v0) && isfinite(v1))
	{
		*value_0 = v0;
		*value_1 = v1;
	}
	else
	{
		*value_0 = fitsnan.d;
		*value_1 = fitsnan.d;
	}
	return;
}
                         











const DifxInput *DifxInput2FitsMC(const DifxInput *D,
                                  struct fits_keywords *p_fits_keys, struct fitsPrivate *out, int phaseCentre)
{
	char bandFormFloat[8];

	struct fitsBinTableColumn base_columns[] =
		{
			{"TIME",       "1D", "Time of center of interval",   "DAYS"},
			{"SOURCE_ID",  "1J", "source id from sources tbl",   0},
			{"ANTENNA_NO", "1J", "antenna id from antennas tbl", 0},
			{"ARRAY",      "1J", "array id number",              0},
			{"FREQID",     "1J", "freq id from frequency tbl",   0},
			{"ATMOS",      "1D", "atmospheric group delay",      "SECONDS"},
			{"DATMOS",     "1D", "atmospheric group delay rate", "SEC/SEC"},
			{"GDELAY",     "1D", "CALC geometric delay",         "SECONDS"},
			{"GRATE",      "1D", "CALC geometric delay rate",    "SEC/SEC"}
		};
	static const size_t base_columns_size = NELEMENTS(base_columns);
	struct fitsBinTableColumn pol1_columns[] =
		{
			{"CLOCK_1",    "1D", "electronic delay",             "SECONDS"},
			{"DCLOCK_1",   "1D", "electronic delay rate",        "SEC/SEC"},
			{"LO_OFFSET_1", bandFormFloat, "station lo_offset for polar. 1", "HZ"},
			{"DLO_OFFSET_1",bandFormFloat, "station lo_offset rate for polar. 1", "HZ/SEC"},
			{"DISP_1",     "1E", "dispersive delay",             "SEC/M/M"},
			{"DDISP_1",    "1E", "dispersive delay rate",        "SEC/M/M/SEC"}
		};
	static const size_t pol1_columns_size = NELEMENTS(pol1_columns);
	struct fitsBinTableColumn pol2_columns[] =
		{
			{"CLOCK_2",    "1D", "electronic delay",             "SECONDS"},
			{"DCLOCK_2",   "1D", "electronic delay rate",        "SEC/SEC"},
			{"LO_OFFSET_2", bandFormFloat, "station lo_offset for polar. 2", "HZ"},
			{"DLO_OFFSET_2",bandFormFloat, "station lo_offset rate for polar. 2", "HZ/SEC"},
			{"DISP_2",     "1E", "dispersive delay for polar 2", "SEC/M/M"},
			{"DDISP_2",    "1E", "dispersive delay rate for polar 2", "SEC/M/M/SEC"}
		};
	static const size_t pol2_columns_size = NELEMENTS(pol2_columns);
	struct fitsBinTableColumn extra_columns[] =
		{
			{"DRY",        "1D", "mount source angle",           "SECONDS"},
			{"DDRY",       "1D", "mount source angle rate",      "SEC/SEC"},
			{"WET",        "1D", "mount source angle",           "SECONDS"},
			{"DWET",       "1D", "mount source angle rate",      "SEC/SEC"},
			{"AZ",         "1D", "mount source angle",           "DEGREES"},
			{"DAZ",        "1D", "mount source angle rate",      "DEG/SEC"},
			{"ELCORR",     "1D", "mount source angle",           "DEGREES"},
			{"DELCORR",    "1D", "mount source angle rate",      "DEG/SEC"},
			{"ELGEOM",     "1D", "mount source angle",           "DEGREES"},
			{"DELGEOM",    "1D", "mount source angle rate",      "DEG/SEC"},
			{"PARA",       "1D", "mount source angle",           "DEGREES"},
			{"DPARA",      "1D", "mount source angle rate",      "DEG/SEC"},
			{"MSANGLE",    "1D", "mount source angle",           "DEGREES"},
			{"DMSANGLE",   "1D", "mount source angle rate",      "DEG/SEC"}
		};
	static const size_t extra_columns_size = NELEMENTS(extra_columns);
	struct fitsBinTableColumn spacecraft_columns[] =
		{
			{"SCGSDELAY",  "1D", "spacecraft to ground station delay",       "SECONDS"},
			{"DSCGSDELAY", "1D", "spacecraft to ground station delay rate",  "SEC/SEC"},
			{"GSSCDELAY",  "1D", "ground station to spacecraft delay",       "SECONDS"},
			{"DGSSCDELAY", "1D", "ground station to spacecraft delay rate",  "SEC/SEC"},
			{"GSCLOCK",    "1D", "ground station electronic delay",          "SECONDS"},
			{"DGSCLOCK",   "1D", "ground station electronic delay rate",     "SEC/SEC"}
		};
	static const size_t spacecraft_columns_size = NELEMENTS(spacecraft_columns);
	struct fitsBinTableColumn LMN_columns[] =
		{
			{"DTAUDL",     "1D", "derivative of delay wrt l",                "SEC/RAD"},
			{"DDTAUDL",    "1D", "derivative of delay wrt l rate",           "SEC/RAD/SEC"},
			{"DTAUDM",     "1D", "derivative of delay wrt m",                "SEC/RAD"},
			{"DDTAUDM",    "1D", "derivative of delay wrt m rate",           "SEC/RAD/SEC"},
			{"DTAUDN",     "1D", "derivative of delay wrt n",                "SEC"},
			{"DDTAUDM",    "1D", "derivative of delay wrt n rate",           "SEC/SEC"},
			{"DDTAUDLDL",  "1D", "dbl derivative of delay wrt l and l",      "SEC/RAD/RAD"},
			{"DDDTAUDLDL", "1D", "dbl derivative of delay wrt l and l rate", "SEC/RAD/RAD/SEC"},
			{"DDTAUDLDM",  "1D", "dbl derivative of delay wrt l and m",      "SEC/RAD/RAD"},
			{"DDDTAUDLDM", "1D", "dbl derivative of delay wrt l and m rate", "SEC/RAD/RAD/SEC"},
			{"DDTAUDLDN",  "1D", "dbl derivative of delay wrt l and n",      "SEC/RAD"},
			{"DDDTAUDLDN", "1D", "dbl derivative of delay wrt l and n rate", "SEC/RAD/SEC"},
			{"DDTAUDMDM",  "1D", "dbl derivative of delay wrt m and m",      "SEC/RAD/RAD"},
			{"DDDTAUDMDM", "1D", "dbl derivative of delay wrt m and m rate", "SEC/RAD/RAD/SEC"},
			{"DDTAUDMDN",  "1D", "dbl derivative of delay wrt m and n",      "SEC/RAD"},
			{"DDDTAUDMDN", "1D", "dbl derivative of delay wrt m and n rate", "SEC/RAD/SEC"},
			{"DDTAUDNDN",  "1D", "dbl derivative of delay wrt n and n",      "SEC"},
			{"DDDTAUDNDN", "1D", "dbl derivative of delay wrt n and n rate", "SEC/SEC"},
		};
	static const size_t LMN_columns_size = NELEMENTS(LMN_columns);
	struct fitsBinTableColumn XYZ_columns[] =
		{
			{"DTAUDX",     "1D", "derivative of delay wrt x",                "SEC/M"},
			{"DDTAUDX",    "1D", "derivative of delay wrt x rate",           "SEC/M/SEC"},
			{"DTAUDY",     "1D", "derivative of delay wrt y",                "SEC/M"},
			{"DDTAUDY",    "1D", "derivative of delay wrt y rate",           "SEC/M/SEC"},
			{"DTAUDZ",     "1D", "derivative of delay wrt z",                "SEC/M"},
			{"DDTAUDZ",    "1D", "derivative of delay wrt z rate",           "SEC/M/SEC"},
			{"DDTAUDXDX",  "1D", "dbl derivative of delay wrt x and x",      "SEC/M/M"},
			{"DDDTAUDXDX", "1D", "dbl derivative of delay wrt x and x rate", "SEC/M/M/SEC"},
			{"DDTAUDXDY",  "1D", "dbl derivative of delay wrt x and y",      "SEC/M/M"},
			{"DDDTAUDXDY", "1D", "dbl derivative of delay wrt x and y rate", "SEC/M/M/SEC"},
			{"DDTAUDXDZ",  "1D", "dbl derivative of delay wrt x and z",      "SEC/M/M"},
			{"DDDTAUDXDZ", "1D", "dbl derivative of delay wrt x and z rate", "SEC/M/M/SEC"},
			{"DDTAUDYDY",  "1D", "dbl derivative of delay wrt y and y",      "SEC/M/M"},
			{"DDDTAUDYDY", "1D", "dbl derivative of delay wrt y and y rate", "SEC/M/M/SEC"},
			{"DDTAUDYDZ",  "1D", "dbl derivative of delay wrt y and z",      "SEC/M/M"},
			{"DDDTAUDYDZ", "1D", "dbl derivative of delay wrt y and z rate", "SEC/M/M/SEC"},
			{"DDTAUDZDZ",  "1D", "dbl derivative of delay wrt z and z",      "SEC/M/M"},
			{"DDDTAUDZDZ", "1D", "dbl derivative of delay wrt z and z rate", "SEC/M/MSEC"},
		};
	static const size_t XYZ_columns_size = NELEMENTS(XYZ_columns);
	// static const size_t total_columns_size = base_columns_size + pol1_columns_size + pol2_columns_size + extra_columns_size + spacecraft_columns_size + LMN_columns_size + XYZ_columns_size;
	static const size_t total_columns_size =  NELEMENTS(base_columns) + NELEMENTS(pol1_columns) + NELEMENTS(pol2_columns) + NELEMENTS(extra_columns) + NELEMENTS(spacecraft_columns) + NELEMENTS(LMN_columns) + NELEMENTS(XYZ_columns);

	struct fitsBinTableColumn columns[total_columns_size];
	size_t nCol;
	size_t n;
	int nColumn;
	int nRowBytes;
	char *p_fitsbuf, *fitsbuf;
	int nBand, nPol;
	int b, j, s, p, np, a;
	int found_MC_interval, MC_interval;
	float LOOffset[array_MAX_BANDS];
	float LORate[array_MAX_BANDS];
	const DifxConfig *config;
	const DifxScan *scan;
	const DifxPolyModel* P;
	const DifxPolyModelLMNExtension* PLMN;
	const DifxPolyModelXYZExtension* PXYZ;
	double time, deltat;
	double x, x_j, x_j_m1;
	double delay, delayRate;
	double atmosDelay, atmosRate;
	double clock, clockRate;
	double sc_gs_delay, sc_gs_rate;
	double gs_clock_delay, gs_clock_rate;
	double msa, msa_rate;
	int configId, dsId, antId;
	int *skip;
	int skipped=0;
	int printed=0;
	int has_spacecraft=0;
	int has_LMN=0;
	int has_XYZ=0;
	/* 1-based indices for FITS file */
	int32_t antId1, arrayId1, sourceId1, freqId1;
	int polyDuration = 0;
	double DELTAT = 0.0;

	if(D == 0)
	{
		return 0;
	}

	/* set up the standard rows */
	nCol = 0;
	for(n=0; n < base_columns_size; ++n, ++nCol)
	{
		columns[nCol] = base_columns[n];
	}

	nBand = p_fits_keys->no_band;
	sprintf (bandFormFloat, "%1dE", nBand);  
  
	for(n=0; n < pol1_columns_size; ++n, ++nCol)
	{
		columns[nCol] = pol1_columns[n];
	}
	nPol = D->nPol;
	if(nPol == 2)
	{
		for(n=0; n < pol2_columns_size; ++n, ++nCol)
		{
			columns[nCol] = pol2_columns[n];
		}
	}
	for(n=0; n < extra_columns_size; ++n, ++nCol)
	{
		columns[nCol] = extra_columns[n];
	}

	/* Find out whether or not there are spacecraft antennas. */
	/* Because of the way that AIPS loads in FITS files, if */
	/* any job of an experiment has a spacecraft antenna, then */
	/* all jobs of the experiment must write out information */
	/* for the spacecraft antenna mode. */
	for(s = 0; s < D->nSpacecraft; s++)
	{
		if((D->spacecraft[s].is_antenna))
		{
			has_spacecraft = 1;
			break;
		}
	}
	if((has_spacecraft))
	{
		for(n=0; n < spacecraft_columns_size; ++n, ++nCol)
		{
			columns[nCol] = spacecraft_columns[n];
		}
	}
	/* Check for LMN and XYZ */
	for(s = 0; s < D->nScan; ++s)
	{
		scan = D->scan + s;
		if(scan->im == NULL)
		{
			continue;
		}
		configId = scan->configId;
		if(configId < 0)
		{
			continue;
		}
		config = D->config + configId;
		for(a = 0; a < config->nAntenna; ++a)
		{
			if((scan->imLMN))
			{
				has_LMN = 1;
			}
			if((scan->imXYZ))
			{
				has_XYZ = 1;
			}
		}
	}
	if((has_LMN))
	{
		for(n=0; n < LMN_columns_size; ++n, ++nCol)
		{
			columns[nCol] = LMN_columns[n];
		}
	}
	if((has_XYZ))
	{
		for(n=0; n < XYZ_columns_size; ++n, ++nCol)
		{
			columns[nCol] = XYZ_columns[n];
		}
	}

	nColumn = nCol;
	nRowBytes = FitsBinTableSize(columns, nColumn);

	/* calloc space for storing table in FITS order */
	fitsbuf = (char *)calloc(nRowBytes, 1);
	if(fitsbuf == 0)
	{
		return 0;
	}
	/* Determine the output table interval */
	for(s = 0; s < D->nScan; ++s)
	{
		scan = D->scan + s;
		configId = scan->configId;
		if(configId < 0)
		{
			continue;
		}
		config = D->config + configId;
		if(s > 0)
		{
			if(DELTAT != config->MC_table_output_interval)
			{
				fprintf(stderr, "Table output interval for scan %d does not match the value for other scans.  This is not supported in FITS-IDI.  Taking the largest value;\n", s);
				if(DELTAT < config->MC_table_output_interval)
				{
					DELTAT = config->MC_table_output_interval;
				}
			}
		}
		else
		{
			DELTAT = config->MC_table_output_interval; /* in seconds */
		}
		/* Now check the polynomial durations */
		for(a = 0; a < config->nAntenna; ++a)
		{
			if(scan->im[a])
			{
				if(polyDuration != 0)
				{
					if(polyDuration != scan->im[a][0][0].validDuration)
					{
						fprintf(stderr, "Polynomial validDuration for scan %d antenna %d does not match the value for other scans.  Different table validity times are not supported in FITS-IDI.  Taking the largest value;\n", s, a);
						if(polyDuration < scan->im[a][0][0].validDuration)
						{
							polyDuration = scan->im[a][0][0].validDuration;
						}
					}
				}
				else
				{
					polyDuration = scan->im[a][0][0].validDuration;
				}
			}
		}
	}
	if((DELTAT == 0.0) || (DELTAT > polyDuration))
	{
		DELTAT = polyDuration;
	}

	fitsWriteBinTable(out, nColumn, columns, nRowBytes, "MODEL_COMPS");
	arrayWriteKeys(p_fits_keys, out);
	fitsWriteInteger(out, "NO_POL",   nPol, "");
	fitsWriteInteger(out, "FFT_SIZE", D->nInChan*2, "");
	fitsWriteInteger(out, "OVERSAMP", 0, "");
	fitsWriteInteger(out, "ZERO_PAD", 0, "");
	fitsWriteInteger(out, "FFT_TWID", 1, "Version of FFT twiddle table used");
	fitsWriteString(out,  "TAPER_FN", D->job->taperFunction, "");
	fitsWriteFloat(out,   "DELTAT",   DELTAT/SEC_DAY_DBL, "DAYS");
	fitsWriteInteger(out, "TABREV",   1, "");
	
	fitsWriteEnd(out);

	arrayId1 = 1;

	/* some values that are always zero */
	for(b = 0; b < nBand; ++b)
	{
		LOOffset[b] = 0.0;
		LORate[b] = 0.0;
	}

	skip = (int *)calloc(D->nAntenna, sizeof(int));

	for(s = 0; s < D->nScan; ++s)
	{
		scan = D->scan + s;
		if(scan->im == NULL)
		{
			continue;
		}
		configId = scan->configId;
		if(configId < 0)
		{
			continue;
		}
		if(phaseCentre >= scan->nPhaseCentres)
		{
			continue;
		}
		config = D->config + configId;
		freqId1 = config->fitsFreqId + 1;
		sourceId1 = D->source[scan->phsCentreSrcs[phaseCentre]].fitsSourceIds[configId] + 1;

		if(scan->im)
		{
			np = scan->nPoly;
		}
		else
		{
			fprintf(stderr, "No im table available for scan %d; aborting MC file creation\n", s);
			continue;
		}

		for(p = 0; p < np; ++p)
		{
			found_MC_interval = 1;
			MC_interval = -1;
			while((found_MC_interval))
			{
				MC_interval++;
				/* set loop condition to false --- it will be set true as
				   needed later on */
				found_MC_interval = 0;
				x = MC_interval * config->MC_table_output_interval;
				/* loop over original .input file antenna list */
				for(a = 0; a < config->nAntenna; ++a)
				{
					double factors_0[MAX_MODEL_ORDER+1];
					double factors_1[MAX_MODEL_ORDER+1];
					double clockfactors_0[MAX_MODEL_ORDER+1];
					double clockfactors_1[MAX_MODEL_ORDER+1];
					double poly[MAX_MODEL_ORDER+1];
					double v0, v1;
					double c0, c1;
					dsId = config->ant2dsId[a];
					if(dsId < 0 || dsId >= D->nDatastream)
					{
						continue;
					}
					/* convert to D->antenna[] index ... */
					antId = D->datastream[dsId].antennaId;
                                        
					if(antId < 0 || antId >= scan->nAntenna)
					{
						continue;
					}
                                        
					/* ... and to FITS antennaId */
					antId1 = antId + 1;

					if(scan->im[antId] == 0)
					{
						if(skip[antId] == 0)
						{
							printf("\n    Polynomial model error : skipping antId %d = %s", 
							       antId, D->antenna[antId].name);
							++skip[antId];
							++printed;
							++skipped;
						}
						continue;
					}

					P = scan->im[antId][phaseCentre] + p;
					if((x >= P->validDuration) && (MC_interval > 0))
					{
						continue;
					}
					else {
						found_MC_interval = 1;
					}
                                        
					time = P->mjd - (int)(D->mjdStart) + (P->sec +x)/86400.0;
					deltat = (P->mjd - D->antenna[antId].clockrefmjd)*86400.0 + P->sec +x;
                                        
					/* evaluate the model polynomials and their first
					   derivatives for time offset x */
					x_j = 1.0;
					x_j_m1 = 0.0;
					atmosDelay = atmosRate = delay = delayRate = 0.0;
					sc_gs_delay = sc_gs_rate = gs_clock_delay = gs_clock_rate = 0.0;
					msa = msa_rate = 0.0;
					for(j=0; j <= P->order; j++)
					{
						atmosDelay += (P->dry[j] + P->wet[j]) * x_j;
						atmosRate  += (P->dry[j] + P->wet[j]) * (j * x_j_m1);
						/* here correct the sign of delay,
						   and remove atmospheric portion of it. */
						delay      += (-P->delay[j] - (P->dry[j] + P->wet[j])) * x_j;
						delayRate  += (-P->delay[j] - (P->dry[j] + P->wet[j])) * (j * x_j_m1);
						if((has_spacecraft))
						{
							sc_gs_delay += P->sc_gs_delay[j] * x_j;
							sc_gs_rate  += P->sc_gs_delay[j] * (j * x_j_m1);
							gs_clock_delay += P->gs_clock_delay[j] * x_j;
							gs_clock_rate  += P->gs_clock_delay[j] * (j * x_j_m1);
							msa      += P->msa[j] * x_j;
							msa_rate += P->msa[j] * (j * x_j_m1);
						}
						/* update x_j values for the next order */
						x_j_m1 = x_j;
						x_j *= x;
					}
					

					/* evaluate the clock polynomials and their first
					   derivatives for time offset x */
					x_j = 1.0;
					x_j_m1 = 0.0;
					clock = clockRate = 0.0;
					for(j = 0; j <= D->antenna[antId].clockorder; j++)
					{
						clock     += D->antenna[antId].clockcoeff[j] * x_j;
						clockRate += D->antenna[antId].clockcoeff[j] * (j * x_j_m1);
						/* update x_j values for the next order */
						x_j_m1 = x_j;
						x_j *= deltat;
					}
					
					preparePolyModelFactors(P->order, x, factors_0, factors_1);
					preparePolyModelFactors(D->antenna[antId].clockorder, deltat, clockfactors_0, clockfactors_1);
                                        
					/* in general, convert from (us) to (sec) */
					atmosDelay     *= SECONDS_PER_MICROSECOND;
					atmosRate      *= SECONDS_PER_MICROSECOND;
					delay          *= SECONDS_PER_MICROSECOND;
					delayRate      *= SECONDS_PER_MICROSECOND;
					sc_gs_delay    *= SECONDS_PER_MICROSECOND;
					sc_gs_rate     *= SECONDS_PER_MICROSECOND;
					gs_clock_delay *= SECONDS_PER_MICROSECOND;
					gs_clock_rate  *= SECONDS_PER_MICROSECOND;
					clock          *= SECONDS_PER_MICROSECOND;
					clockRate      *= SECONDS_PER_MICROSECOND;
					/* convert from radians to degrees */
					msa            *= 180.0/M_PI;
					msa_rate       *= 180.0/M_PI;
                                        
					/* get MSA in range -180 to +180 degrees */
					msa = fmod(msa,360.0);
					if(msa > 180.0)
					{
						msa -= 360.0;
					}
					else if(msa <= -180.0)
					{
						msa += 360.0;
					}
					if((has_spacecraft))
					{
						if(!config->doMSAcalibration)
						{
							msa=0.0;
							msa_rate=0.0;
						}
					}
                                        
					p_fitsbuf = fitsbuf;

					/* Write the basic data */
					FITS_WRITE_ITEM (time, p_fitsbuf);
					FITS_WRITE_ITEM (sourceId1, p_fitsbuf);
					FITS_WRITE_ITEM (antId1, p_fitsbuf);
					FITS_WRITE_ITEM (arrayId1, p_fitsbuf);
					FITS_WRITE_ITEM (freqId1, p_fitsbuf);
					    /* Atmospheric Delay */
					addPolyModelArrays(P->order, P->dry, P->wet, poly);
					applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, poly, factors_0, factors_1, &v0, &v1);
					if((fabs(v0-atmosDelay) > 1E-12) || (fabs(v1-atmosRate) > 1E-14))
					{
						fprintf(stderr, "Error: different computations of atmospheric delay differ --- %E %E %E %E\n", v0, atmosDelay, v1, atmosRate);
						exit(EXIT_FAILURE);
					}
					FITS_WRITE_ITEM (v0, p_fitsbuf);
					FITS_WRITE_ITEM (v1, p_fitsbuf);
					    /* Total delay --- correct the sign and remove
					       atmospheric portion of the delay */
					addPolyModelArrays(P->order, P->delay, poly, poly);
					applyPolyModelFactors(P->order, -SECONDS_PER_MICROSECOND, poly, factors_0, factors_1, &v0, &v1);
					if((fabs(v0-delay) > 1E-12) || (fabs(v1-delayRate) > 1E-14))
					{
						fprintf(stderr, "Error: different computations of delay differ --- %E %E %E %E\n", v0, delay, v1, delayRate);
						exit(EXIT_FAILURE);
					}
					FITS_WRITE_ITEM (v0, p_fitsbuf);
					FITS_WRITE_ITEM (v1, p_fitsbuf);
					/* Write the polarization data */
					applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, D->antenna[antId].clockcoeff, clockfactors_0, clockfactors_1, &c0, &c1);
					if((fabs(c0-clock) > 1E-12) || (fabs(c1-clockRate) > 1E-14))
					{
						fprintf(stderr, "Error: different computations of clock differ --- %E %E %E %E\n", c0, clock, c1, clockRate);
						exit(EXIT_FAILURE);
					}
					/* Dispersive delay (such as ionospherid delay) is
					   supposed to be provided in units of seconds per square
					   meter in FITS-IDI.  This is the delay at a wavelength
					   of 1 meter, whereas the difxio value is the delay
					   in microseconds at a frequency of 1 GHz.  Convert.
					*/
					applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND * SPEED_LIGHT / 1E9, P->iono, factors_0, factors_1, &v0, &v1);
					for(j = 0; j < nPol; ++j)
					{
						FITS_WRITE_ITEM (c0, p_fitsbuf);
						FITS_WRITE_ITEM (c1, p_fitsbuf);
						FITS_WRITE_ARRAY(LOOffset, p_fitsbuf, nBand);
						FITS_WRITE_ARRAY(LORate, p_fitsbuf, nBand);
						FITS_WRITE_ITEM (v0, p_fitsbuf);
						FITS_WRITE_ITEM (v1, p_fitsbuf);
					} /* Polarization loop */
					/* Write the extra data */
					applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, P->dry, factors_0, factors_1, &v0, &v1);
					FITS_WRITE_ITEM (v0, p_fitsbuf);
					FITS_WRITE_ITEM (v1, p_fitsbuf);
					applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, P->wet, factors_0, factors_1, &v0, &v1);
					FITS_WRITE_ITEM (v0, p_fitsbuf);
					FITS_WRITE_ITEM (v1, p_fitsbuf);
					applyPolyModelFactors(P->order, 1.0, P->az, factors_0, factors_1, &v0, &v1);
					FITS_WRITE_ITEM (v0, p_fitsbuf);
					FITS_WRITE_ITEM (v1, p_fitsbuf);
					applyPolyModelFactors(P->order, 1.0, P->elcorr, factors_0, factors_1, &v0, &v1);
					FITS_WRITE_ITEM (v0, p_fitsbuf);
					FITS_WRITE_ITEM (v1, p_fitsbuf);
					applyPolyModelFactors(P->order, 1.0, P->elgeom, factors_0, factors_1, &v0, &v1);
					FITS_WRITE_ITEM (v0, p_fitsbuf);
					FITS_WRITE_ITEM (v1, p_fitsbuf);
					applyPolyModelFactors(P->order, 1.0, P->parangle, factors_0, factors_1, &v0, &v1);
					FITS_WRITE_ITEM (v0, p_fitsbuf);
					FITS_WRITE_ITEM (v1, p_fitsbuf);
					if(!config->doMSAcalibration)
					{
						v0=fitsnan.d;
						v1=fitsnan.d;
					}
					else
					{
						applyPolyModelFactors(P->order, 180.0/M_PI, P->msa, factors_0, factors_1, &v0, &v1);
					}
					FITS_WRITE_ITEM (v0, p_fitsbuf);
					FITS_WRITE_ITEM (v1, p_fitsbuf);
					if((has_spacecraft))
					{
						applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, P->sc_gs_delay, factors_0, factors_1, &v0, &v1);
						FITS_WRITE_ITEM (v0, p_fitsbuf);
						FITS_WRITE_ITEM (v1, p_fitsbuf);
						applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, P->gs_sc_delay, factors_0, factors_1, &v0, &v1);
						FITS_WRITE_ITEM (v0, p_fitsbuf);
						FITS_WRITE_ITEM (v1, p_fitsbuf);
						applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, P->gs_clock_delay, factors_0, factors_1, &v0, &v1);
						FITS_WRITE_ITEM (v0, p_fitsbuf);
						FITS_WRITE_ITEM (v1, p_fitsbuf);
					}
					if((has_LMN))
					{
						int have_data = 0;
						if((scan->imLMN))
						{
							if((scan->imLMN[antId]))
							{
								if((scan->imLMN[antId][phaseCentre]))
								{
									have_data = 1;
									PLMN = scan->imLMN[antId][phaseCentre] + p;
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PLMN->dDelay_dl, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PLMN->dDelay_dm, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PLMN->dDelay_dn, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PLMN->d2Delay_dldl, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PLMN->d2Delay_dldm, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PLMN->d2Delay_dldn, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PLMN->d2Delay_dmdm, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PLMN->d2Delay_dmdn, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PLMN->d2Delay_dndn, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
								}
							}
						}
						if(!have_data)
						{
							v0 = fitsnan.d;
							v1 = fitsnan.d;
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
						}
					}
					if((has_XYZ))
					{
						int have_data = 0;
						if((scan->imXYZ))
						{
							if((scan->imXYZ[antId]))
							{
								if((scan->imXYZ[antId][phaseCentre]))
								{
									have_data = 1;
									PXYZ = scan->imXYZ[antId][phaseCentre] + p;
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PXYZ->dDelay_dX, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PXYZ->dDelay_dY, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PXYZ->dDelay_dZ, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PXYZ->d2Delay_dXdX, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PXYZ->d2Delay_dXdY, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PXYZ->d2Delay_dXdZ, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PXYZ->d2Delay_dYdY, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PXYZ->d2Delay_dYdZ, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
									applyPolyModelFactors(P->order, SECONDS_PER_MICROSECOND, PXYZ->d2Delay_dZdZ, factors_0, factors_1, &v0, &v1);
									FITS_WRITE_ITEM (v0, p_fitsbuf);
									FITS_WRITE_ITEM (v1, p_fitsbuf);
								}
							}
						}
						if(!have_data)
						{
							v0 = fitsnan.d;
							v1 = fitsnan.d;
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
							FITS_WRITE_ITEM (v0, p_fitsbuf);
							FITS_WRITE_ITEM (v1, p_fitsbuf);
						}
					}

					testFitsBufBytes(p_fitsbuf - fitsbuf, nRowBytes, "MC");
                                        
#ifndef WORDS_BIGENDIAN
					FitsBinRowByteSwap(columns, nColumn, fitsbuf);
#endif
					fitsWriteBinRow(out, fitsbuf);
				} /* Antenna loop */
				if(config->MC_table_output_interval == 0.0)
				{
					found_MC_interval = 0;
				}
			} /* found_MC_interval loop */
		} /* Intervals in scan loop */
	} /* Scan loop */

	if(printed)
	{
		printf("\n                            ");
	}
  
	/* release buffer space */
	free(fitsbuf);
	free(skip);

	return D;
}
