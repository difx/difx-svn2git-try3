/***************************************************************************
 *   Copyright (C) 2008-2010 by Walter Brisken                             *
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
#include "difx2fits.h"
#include "fitsPH.h"
#include "other.h"

//phase values from mpifxcorr currently have to be inverted to match FITS conversion
#define PCAL_INVERT

//FIXME replace with difxio
const int nBandTone = 2;
const int bandTone[] = {1, -1};
const int pcalIntTime = 20.0;
const int pcaltiny = 1e-10; //FIXME review once we've got pcal amplitudes sorted


int nextfile(const DifxInput *D, int antId, int *jobId, FILE **file)
{
	char filename[50];
	//FIXME must/should be a maxfilebase in difxio
	const char suffix[] = ".difx/PCAL_";
	if(*file)
	{
		fclose(*file);
		*file = 0;
	}

	while(*file == 0)
	{
		*jobId += 1;
		if(*jobId >= D->nJob)
		{

			//printf("Job %d nJob %d\n", *jobId, D->nJob);
			return 1;
		}
		//FIXME add check to see if antenna is present in this job
		
		sprintf(filename, "%s%s%s", D->job[*jobId].fileBase,
				 suffix, D->antenna[antId].name);
		//printf("Opening File %s\n", filename);
		*file = fopen(filename, "r");
		if(!*file)
		{
			fprintf(stderr, "Error opening file : %s\n", filename);
		}
	}
	printf("Opened File %s\n", filename);
	return 0;
}
	
/* go through pcal file, determine maximum number of tones.*/
static int getNTone(const char *filename, double t1, double t2, const char *antName)
{
	const int MaxLineLength=1000;
	FILE *in;
	char line[MaxLineLength+1];
	int n, nTone, maxnTone=0;
	double t;
	char *rv;
	char antName1[20];


	//FIXME also check that nBand and nPol don't exceed D->nIF and D->nPol
	in = fopen(filename, "r");
	if(!in)
	{
		return -1;
	}
	
	for(;;)
	{
		rv = fgets(line, MaxLineLength, in);
		if(!rv)
		{
			break;
		}
		n = sscanf(line, "%s%lf%*f%*f%*d%*d%d", antName1, &t, &nTone);
		if(n != 3 || strcmp(antName, antName1))
		{
			continue;
		}
		if(t >= t1 && t <= t2)
		{
			if(nTone > maxnTone)
			{
				maxnTone = nTone;
			}
		}
		
	}
	fclose(in);

	return maxnTone;
}

static int parsePulseCal(const char *line, 
	int antId, int nBand, int nTone,
	int *sourceId,
	double *time, float *timeInt, double *cableCal,
	double freqs[2][array_MAX_TONES], 
	float pulseCalRe[2][array_MAX_TONES], 
	float pulseCalIm[2][array_MAX_TONES], 
	float stateCount[2][array_MAX_TONES], 
	int refDay, const DifxInput *D, int *configId, 
	int phasecentre)
{
	int np, nb, nt, ns;
	int nRecBand, recBand;
	int n, p, i, v;
	int freqId, polId, bandId, tone, state;
	int pol, band;
	int scanId;
	double A;
	float B, C;
	double mjd;
	char antName[20];
	
	union
	{
		int32_t i32;
		float f;
	} nan;
	nan.i32 = -1;

	n = sscanf(line, "%s%lf%f%lf%d%d%d%d%d%n", antName, time, timeInt, 
		cableCal, &np, &nb, &nt, &ns, &nRecBand, &p);
	if(n != 9)
	{
		return -1;
	}
	line += p;

	if(*cableCal > 999.89 && *cableCal < 999.91)
	{
		*cableCal = nan.f;
	}

	*time -= refDay;
	mjd = *time + (int)(D->mjdStart);

	if(mjd < D->mjdStart || mjd > D->mjdStop)
	{
		return -1;
	}

	scanId = DifxInputGetScanIdByAntennaId(D, mjd, antId);
	if(scanId < 0)
	{	
		return -3;
	}

        if(phasecentre >= D->scan[scanId].nPhaseCentres)
        {
          printf("Skipping scan %d as the requested phase centre was not used\n", scanId);
          return -3;
        }

	*sourceId = D->scan[scanId].phsCentreSrcs[phasecentre];
	*configId = D->scan[scanId].configId;
	if(*sourceId < 0 || *configId < 0)	/* not in scan */
	{
		return -3;
	}
	
	for(pol = 0; pol < 2; pol++)
	{
		for(i = 0; i < array_MAX_TONES; i++)
		{
			freqs[pol][i] = 0.0;
			pulseCalRe[pol][i] = 0.0;
			pulseCalIm[pol][i] = 0.0;
			stateCount[pol][i] = 0.0;
		}
	}

	*cableCal *= 1e-12;

	/* Read in pulse cal information */
	for(pol = 0; pol < np; pol++)
	{
		for(band = 0; band < nb; band++)
		{
			for(tone = 0; tone < nt; tone++)
			{
				n = sscanf(line, "%d%lf%f%f%n", 
					&recBand, &A, &B, &C, &p);
				if(n < 4)
				{
					return -4;
				}
				line += p;
				if(recBand < 0 || recBand >= nRecBand)
				{
					continue;
				}
				v = DifxConfigRecChan2IFPol(D, *configId,
					antId, recBand, &bandId, &polId);
				if(v >= 0)
				{
					if(freqId < 0 || polId < 0)
					{
						fprintf(stderr, "Error: derived "
							"freqId and polId (%d,%d) are "
							"not legit.  From "
							"recBand=%d.\n",
							freqId, polId, recBand);
						continue;
					}
					else if(freqId >= D->nFreq)
					{
						fprintf(stderr, "parsePulseCal(1): Developer error: freqId=%d, nFreq=%d\n",
							freqId, D->nFreq);
						continue;
					}
					bandId = D->config[*configId].freqId2IF[freqId];
					if(bandId < 0)
					{
						/* This sub-band is not to go to the FITS file */
						continue;
					}
					freqs[polId][tone + bandId*nt] = A*1.0e6;
					if((B >= 999.89 && B < 999.91) ||
					   (C >= 999.89 && C < 999.91))
					{
					  pulseCalRe[polId][tone + bandId*nt] = 
						nan.f;
					  pulseCalIm[polId][tone + bandId*nt] = 
						nan.f;
					}
					else
					{
					  pulseCalRe[polId][tone + bandId*nt] = 
						B*cos(C*M_PI/180.0);
					  pulseCalIm[polId][tone + bandId*nt] = 
						B*sin(C*M_PI/180.0);
					}
				}
			}
		}
	}
	
	if(ns > 0)
	{
		/* Read in state count information */
		for(pol = 0; pol < np; pol++)
		{
			for(band = 0; band < nb; band++)
			{
				n = sscanf(line, "%d%n", &recBand, &p);
				line += p;
				v = DifxConfigRecChan2IFPol(D, *configId,
					antId, recBand, &bandId, &polId);
				for(state = 0; state < 4; state++)
				{
					if(freqId < 0 || polId < 0)
					{
						fprintf(stderr, "parsePulseCal(2): Developer error: derived "
							"freqId and polId (%d,%d) are "
							"not legit.  From "
							"recBand=%d.\n",
							freqId, polId, recBand);
						continue;
					}
					else if(freqId >= D->nFreq)
					{
						fprintf(stderr, "parsePulseCal(2): Developer error: freqId=%d, nFreq=%d\n",
							freqId, D->nFreq);
						continue;
					}
					bandId = D->config[*configId].freqId2IF[freqId];
					if(bandId < 0)
					{
						/* This sub-band is not to go to the FITS file */
						continue;
					}
					for(state = 0; state < 4; state++)
					{
						if(state < ns)
						{
							n = sscanf(line, "%f%n", &B, &p);
							if(n < 1)
							{
								return -5;
							}
							line += p;
						}
						else
						{
							B = 0.0;
						}
						stateCount[polId][state + bandId*4] = B;
					}
				}
			}
		}
	}

	return 0;
}
static int parsePulseCalCableCal(const char *line, 
	int antId,
	int *sourceId,
	double *time, float *timeInt, double *cableCal,
	int refDay, const DifxInput *D, int *configId, 
	int phasecentre)
{
	int np, nb, nt, ns;
	int nRecChan, recChan;
	int n, p, i, v;
	int polId, bandId, tone, state;
	int pol, band;
	int scanId;
	double A;
	float B, C;
	double mjd;
	char antName[20];

	union
	{
		int32_t i32;
		float f;
	} nan;
	nan.i32 = -1;
	
	n = sscanf(line, "%s%lf%f%lf%n", antName, time, timeInt, 
		cableCal, &p);
	printf("\n n%d %f\n", n, *cableCal);
	if(n != 4)
	{
		return -1;
	}

	if(*cableCal > 999.89 && *cableCal < 999.91)
	{
		*cableCal = nan.f;
	}

	*time -= refDay;
	mjd = *time + (int)(D->mjdStart);

	if(mjd < D->mjdStart || mjd > D->mjdStop)
	{
		return -1;
	}

	scanId = DifxInputGetScanIdByAntennaId(D, mjd, antId);
	if(scanId < 0)
	{	
		return -3;
	}

        if(phasecentre >= D->scan[scanId].nPhaseCentres)
        {
          printf("Skipping scan %d as the requested phase centre was not used\n", scanId);
          return -3;
        }

	*sourceId = D->scan[scanId].phsCentreSrcs[phasecentre];
	*configId = D->scan[scanId].configId;
	if(*sourceId < 0 || *configId < 0)	/* not in scan */
	{
		return -3;
	}
	
	*cableCal *= 1e-12;
}
static int parseDifxPulseCal(const char *line, 
	int antId, int nBand, int nTone,
	const DifxPCal *pcal,
	int *sourceId, double *time,
	double freqs[2][array_MAX_TONES], 
	float pulseCalRe[2][array_MAX_TONES], 
	float pulseCalIm[2][array_MAX_TONES], 
	float stateCount[2][array_MAX_TONES],
	float pulseCalRate[2][array_MAX_TONES],
	int refDay, const DifxInput *D, int *configId, 
	int phasecentre)
{
	int np, nb, nt, ns;
	int nRecChan, recChan;
	int n, p, i, j, k;
	int polId, bandId, tone;
	int pol, band;
	int scanId;
	double A;
	float B, C;
	double mjd;
	char antName[20];
	double cableCal;
	float timeInt;

	union
	{
		int32_t i32;
		float f;
	} nan;
	nan.i32 = -1;

	/* This is the same as parsePulseCal except that parameters 
	 * are taken from D where possible, and cable cal is 0*/

	n = sscanf(line, "%s%lf%f%lf%d%d%d%d%d%n", antName, time, &timeInt, 
		&cableCal, &np, &nb, &nt, &ns, &nRecChan, &p);
	if(n != 9)
	{
		fprintf(stderr, "Error scanning header\n");
		return -1;
	}
	line += p;

	*time -= refDay;
	mjd = *time + (int)(D->mjdStart);

	if(mjd < D->mjdStart || mjd > D->mjdStop)
	{
		//printf("out of mjd range\n");
		return -1;
	}

	scanId = DifxInputGetScanIdByAntennaId(D, mjd, antId);
	if(scanId < 0)
	{	
		return -3;
	}

        if(phasecentre >= D->scan[scanId].nPhaseCentres)
        {
          printf("Skipping scan %d as the requested phase centre was not used\n", scanId);
          return -3;
        }

	*sourceId = D->scan[scanId].phsCentreSrcs[phasecentre];
	*configId = D->scan[scanId].configId;
	if(*sourceId < 0 || *configId < 0)	/* not in scan */
	{
		return -3;
	}
	
	for(pol = 0; pol < 2; pol++)
	{
		for(i = 0; i < nBand*nTone; i++)
		{
			//FIXME should these be set to nan instead?
			freqs[pol][i] = 0.0;
			pulseCalRe[pol][i] = 0.0;
			pulseCalIm[pol][i] = 0.0;
			pulseCalRate[pol][i] = 0.0;
		}
		for(i = 0; i < nBand*4; i++)
		{
			stateCount[pol][i] = 0.0;
		}
	}

	/* Read in pulse cal information */
	for(pol = 0; pol < D->nPol; pol++)
	{
		//FIXME double-check there's no possibility of pols getting swapped
		j = 0;/*band index within freqs, pulseCalRe and pulseCalIm*/
		for(band = 0; band < pcal->nRecFreq; band++)
		{
			if(j >= nBand)
			{
				printf("Warning: trying to too many bands in pol %d band %d\n", pol, band);
				break;
			}
			k = 0;/*tone index within freqs, pulseCalRe and pulseCalIm*/
			for(tone = 0; tone < pcal->nRecFreqPcal[band]; tone++)
			{

				n = sscanf(line, "%d%lf%f%f%n", 
					&recChan, &A, &B, &C, &p);
				if(n < 4)
				{
					printf("Warning: Error scanning line\n");
					return -4;
				}
				line += p;
				/*Only write out specified frequencies*/
				if(!pcal->recFreqPcalOut[band][tone])
				{
					continue;
				}

				if(k >= nTone)
				{
					printf("Warning: trying to extract too many tones in pol %d band %d\n", pol, band);
					break;
				}
				
				freqs[pol][j*nTone + k] = (double) pcal->recFreqPcalFreq[band][tone]*1.0e6;
				if(B < pcaltiny && B > -pcaltiny && C < pcaltiny && C > -pcaltiny)
				{
				  pulseCalRe[pol][j*nTone + k] = 
					nan.f;
				  pulseCalIm[pol][j*nTone + k] = 
					nan.f;
				}
				else
				{
					pulseCalRe[pol][j*nTone + k] = B;
#ifdef PCAL_INVERT
					C *= -1;
#endif
					pulseCalIm[pol][j*nTone + k] = C;
				}
				k++;
			}
			while(k < nTone)
			{
				pulseCalRe[pol][j*nTone + k] = 
				nan.f;
				pulseCalIm[pol][j*nTone + k] = 
				nan.f;
				k++;
			}
			j++;
		}
		//n.b. unused bands at the end will be set to zero rather than NaN
	}
	return 0;
}

const DifxInput *DifxInput2FitsPH(const DifxInput *D,
	struct fits_keywords *p_fits_keys, struct fitsPrivate *out,
	int phasecentre)
{
	char stateFormFloat[4];
	char toneFormDouble[4];
	char toneFormFloat[4];
	
	/*  define the flag FITS table columns */
	struct fitsBinTableColumn columns[] =
	{
		{"TIME", "1D", "time of center of interval", "DAYS"},
		{"TIME_INTERVAL", "1E", "time span of datum", "DAYS"},
		{"SOURCE_ID", "1J", "source id number from source tbl", 0},
		{"ANTENNA_NO", "1J", "antenna id from array geom. tbl", 0},
		{"ARRAY", "1J", "????", 0},
		{"FREQID", "1J", "freq id from frequency tbl", 0},
		{"CABLE_CAL", "1D", "cable length calibration", "SECONDS"},
		{"STATE_1", stateFormFloat, "state counts (4 per baseband)", 0},
		{"PC_FREQ_1", toneFormDouble, "Pcal recorded frequency", "Hz"},
		{"PC_REAL_1", toneFormFloat, "Pcal real", 0},
		{"PC_IMAG_1", toneFormFloat, "Pcal imag", 0},
		{"PC_RATE_1", toneFormFloat, "Pcal rate", 0},
		{"STATE_2", stateFormFloat, "state counts (4 per baseband)", 0},
		{"PC_FREQ_2", toneFormDouble, "Pcal recorded frequency", "Hz"},
		{"PC_REAL_2", toneFormFloat, "Pcal real", 0},
		{"PC_IMAG_2", toneFormFloat, "Pcal imag", 0},
		{"PC_RATE_2", toneFormFloat, "Pcal rate", 0}
	};

	int nColumn;
	int nRowBytes;
	char *fitsbuf, *p_fitsbuf;
	const int MaxLineLength=10000;//FIXME even this could be too small!!
	char line[MaxLineLength+1];
	int nBand, nPol;
	int nTone=-2;
	int nStationTone = -2;
	double time;
	float timeInt;
	double cableCal, cableCalOut;
	double cableTime;
	float cableTimeInt;
	double freqs[2][array_MAX_TONES];
	float pulseCalRe[2][array_MAX_TONES];
	float pulseCalIm[2][array_MAX_TONES];
	float stateCount[2][array_MAX_TONES];
	float pulseCalRate[2][array_MAX_TONES];
	int configId;
	int antId, sourceId;
	int refDay;
	int i, j, n, v;
	double start, stop;
	FILE *in, *in2;
	char *rv;
	/* The following are 1-based indices for FITS format */
	int32_t antId1, arrayId1, sourceId1, freqId1;

	int stationpcal = 0;
	int jobId;
	DifxPCal **pcalinfo;
	char antName[20];

	union
	{
		int32_t i32;
		float f;
	} nan;
	nan.i32 = -1;


	if(D == 0)
	{
		return D;
	}

	nBand = D->nIF;
	nPol = D->nPol;

	mjd2dayno((int)(D->mjdStart), &refDay);

	start = D->mjdStart - (int)(D->mjdStart);
	stop  = D->mjdStop  - (int)(D->mjdStart);

	//check for existence of pcal file
	in = fopen("pcal", "r");
	if(in)
	{
		printf("Station-extracted pcals available\n");
		stationpcal = 1;
		fclose(in);
	}
	printf("\n");

	if(!pcalinfo)
	{
		fprintf(stderr, "Error allocating pcalinfo\n");
		return 0;
	}
	
	//check all antennas for pcal and work out maxtones per IF (nTones)
	for(i = 0; i < D->nAntenna; i++)
	{
		pcalinfo[i] = newDifxPCal(D, i);
		//assert(pcalinfo[i]);
		if(pcalinfo[i])
		{
			printf("Using DiFX-extracted pcals for antenna %s\n", D->antenna[i].name);
			printDifxPCal(pcalinfo[i]);
			//printf("Phase Cal interval %d\n", D->datastream[i].phaseCalIntervalMHz);
			if(pcalinfo[i]->maxPcalOut > nTone)
			{
				nTone = pcalinfo[i]->maxPcalOut;
			}
		}
		else if(stationpcal)
		{
			nStationTone = getNTone("pcal", refDay + start, refDay + stop, D->antenna[i].name);
			printf("Using Station-extracted pcals for antenna %s\n", D->antenna[i].name, nStationTone);
			if(nStationTone > nTone)
			{
				nTone = nStationTone;
			}
		}
	}

	if(nTone < 1)
	{
		/* neither difx nor observation pcals */
		printf("    No pcals found\n");
		free(pcalinfo);
		return D;
	}
	if(nTone < 2)
	{
		printf("    Warning, less than 2 tones per sub-band\n");
	}

	sprintf(stateFormFloat, "%dE", 4*nBand);
	sprintf(toneFormFloat,  "%dE", nTone*nBand);
	sprintf(toneFormDouble, "%dD", nTone*nBand);
	
	if(nPol == 2)
	{
		nColumn = NELEMENTS(columns);
	}
	else
	{
		nColumn = NELEMENTS(columns) - 5;
	}
	
	nRowBytes = FitsBinTableSize(columns, nColumn);

	/* calloc space for storing table in FITS format */
	fitsbuf = (char *)calloc(nRowBytes, 1);
	if(fitsbuf == 0)
	{
	        fclose(in);
		fprintf(stderr, "Error: DifxInput2FitsPH: Memory allocation failure\n");

		exit(0);
	}

	fitsWriteBinTable(out, nColumn, columns, nRowBytes, "PHASE-CAL");
	arrayWriteKeys (p_fits_keys, out);
	fitsWriteInteger(out, "NO_POL", nPol, "");
	fitsWriteInteger(out, "NO_TONES", nTone, "");
	fitsWriteInteger(out, "TABREV", 2, "");
	fitsWriteEnd(out);

	antId = 0;
	arrayId1 = 1;

	in = fopen("pcal", "r");

	for(i = 0; i < D->nAntenna; i++)
	{
		printf("Processing %s\n", D->antenna[i].name);
		/* set defaults */

		jobId = -1;
		in2 = 0;
		cableCal = 0.0;
		cableTime = 0.0;
		cableTimeInt = -1.0;
		/*rewind(in) below this loop*/
		while(1)
		{
			if(!pcalinfo[i])/*try reading pcal file*/
			{
				rv = fgets(line, MaxLineLength, in);
				if(!rv)
				{
					break;/*to next datastream*/
				}
					
				/* ignore possible comment lines */
				if(line[0] == '#')
				{
					continue;/*to next line in file*/
				}
				else 
				{
					n = sscanf(line, "%s", antName);
					if(n != 1 || strcmp(antName, D->antenna[i].name))
					{
						continue;/*to next line in file*/	
					}
					v = parsePulseCal(line, i, nBand, nTone, &sourceId, &time, &timeInt, 
						&cableCal, freqs, pulseCalRe, pulseCalIm,
						stateCount, refDay, D, &configId, phasecentre);
					if(v < 0)
					{
						continue;/*to next line in file*/
					}
				}
			}
			else/*reading difx-extracted pcals*/
			{
				if(in2 == 0)
				{
					//printf("no open file\n");
					v = nextfile(D, i, &jobId, &in2);
					if(v == 0)
					{
						continue;
					}
					break;/*to next antenna*/
				}
				rv = fgets(line, MaxLineLength, in2);
				if(!rv)
				{
					//printf("couldn't read from file\n");
					v = nextfile(D, i, &jobId, &in2);
					if(v == 0)
					{
						continue;
					}
					break;/*to next antenna*/
				}
					
				/* ignore possible comment lines */
				if(line[0] == '#')
				{
					continue;/*to next line in file*/
				}
				else 
				{
					v = parseDifxPulseCal(line, i, nBand, nTone, pcalinfo[i], &sourceId, &time,
							      freqs, pulseCalRe, pulseCalIm, stateCount, pulseCalRate,
							      refDay, D, &configId, phasecentre);
					if(v < 0)
					{
						continue;/*to next line in file*/
					}
					timeInt = D->config[configId].tInt/86400;
				}

				/*get cable cal from pcal file */
				while(cableTimeInt < 0 || cableTime + cableTimeInt / 2 < time)
				{
					rv = fgets(line, MaxLineLength, in);
					if(!rv)
					{
						break;/*to out of pcal search*/
					}
					else
					{
						//printf("%s", line);
					}
						
					/* ignore possible comment lines */
					if(line[0] == '#')
					{
						continue;/*to next line in file*/
					}
					else 
					{
						n = sscanf(line, "%s", antName);
						if(n != 1 || strcmp(antName, D->antenna[i].name))
						{
							continue;/*to next line in file*/	
						}
						v = parsePulseCalCableCal(line, i, &sourceId, &cableTime, &cableTimeInt, 
							&cableCalOut, refDay, D, &configId, phasecentre);
						printf("\n%f %f %e", cableTime, cableTimeInt, cableCalOut);
						if(v < 0)
						{
							continue;/*to next line in file*/
						}
					}
				}
				if(cableTimeInt > 0 && cableTime - cableTimeInt/2 < time && cableTime + cableTimeInt/2 > time)
				{
					cableCal = cableCalOut;
				}
				else
				{
					cableCal = nan.f;
				}

			}

			freqId1 = D->config[configId].fitsFreqId + 1;
			sourceId1 = D->source[sourceId].fitsSourceIds[configId] + 1;
			antId1 = i + 1;

			p_fitsbuf = fitsbuf;
		
			FITS_WRITE_ITEM (time, p_fitsbuf);
			FITS_WRITE_ITEM (timeInt, p_fitsbuf);
			FITS_WRITE_ITEM (sourceId1, p_fitsbuf);
			FITS_WRITE_ITEM (antId1, p_fitsbuf);
			FITS_WRITE_ITEM (arrayId1, p_fitsbuf);
			FITS_WRITE_ITEM (freqId1, p_fitsbuf);
			FITS_WRITE_ITEM (cableCal, p_fitsbuf);

			for(j = 0; j < nPol; j++)
			{
				FITS_WRITE_ARRAY(stateCount[j], p_fitsbuf,
					4*nBand);
				if(nTone > 0)
				{
					FITS_WRITE_ARRAY(freqs[j],
						p_fitsbuf, nTone*nBand);
					FITS_WRITE_ARRAY(pulseCalRe[j], 
						p_fitsbuf, nTone*nBand);
					FITS_WRITE_ARRAY(pulseCalIm[j], 
						p_fitsbuf, nTone*nBand);
					FITS_WRITE_ARRAY(pulseCalRate[j], 
						p_fitsbuf, nTone*nBand);
				}
			}

			testFitsBufBytes(p_fitsbuf - fitsbuf, nRowBytes, "PH");
			
#ifndef WORDS_BIGENDIAN
			FitsBinRowByteSwap(columns, nColumn, fitsbuf);
#endif
			fitsWriteBinRow(out, fitsbuf);
			//printf("Entry Written\n");
		}
		rewind(in);
	}
	fclose(in);

	for(i = 0; i < D->nDatastream; i++)
	{
		deleteDifxPCal(pcalinfo[i]);
	}

	free(pcalinfo);
	free(fitsbuf);

	return D;
}
