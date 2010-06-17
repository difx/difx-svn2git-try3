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
#include <glob.h>
#include "config.h"
#include "difx2fits.h"
#include "other.h"

//FIXME replace with difxio
const int nBandTone = 2;
const int bandTone[] = {1, -1};
const int pcalIntTime = 10.0;

/* go through pcal file, determine maximum number of tones.
 */
static int getNTone(const char *filename, double t1, double t2)
{
	const int MaxLineLength=1000;
	FILE *in;
	char line[MaxLineLength+1];
	int n, nTone, maxnTone=0;
	double t;
	char *rv;

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
		n = sscanf(line, "%*s%lf%*f%*f%*d%*d%d", &t, &nTone);
		if(n != 2)
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
	int antId,
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

	n = sscanf(line, "%s%lf%f%lf%d%d%d%d%d%n", antName, time, timeInt, 
		cableCal, &np, &nb, &nt, &ns, &nRecChan, &p);
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
					&recChan, &A, &B, &C, &p);
				if(n < 4)
				{
					return -4;
				}
				line += p;
				if(recChan < 0 || recChan >= nRecChan)
				{
					continue;
				}
				v = DifxConfigRecChan2IFPol(D, *configId,
					antId, recChan, &bandId, &polId);
				if(v >= 0)
				{
					if(bandId < 0 || polId < 0)
					{
						fprintf(stderr, "Error: derived "
							"bandId and polId (%d,%d) are "
							"not legit.  From "
							"recChan=%d.\n",
							bandId, polId, recChan);
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
				n = sscanf(line, "%d%n", &recChan, &p);
				line += p;
				v = DifxConfigRecChan2IFPol(D, *configId,
					antId, recChan, &bandId, &polId);
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
	const int MaxLineLength=1000;
	char line[MaxLineLength+1];
	int nBand, nPol;
	int nTone=0;
	int *antnTone;
	double time;
	float timeInt;
	double cableCal;
	double freqs[2][array_MAX_TONES];
	float pulseCalRe[2][array_MAX_TONES];
	float pulseCalIm[2][array_MAX_TONES];
	float stateCount[2][array_MAX_TONES];
	float pulseCalRate[2][array_MAX_TONES];
	int configId;
	int antId, sourceId;
	int refDay;
	int i, j, k, n, p, v, t;
	int nPcalDatastreams=0;
	double start, stop;
	FILE *in;
	char *rv;
	/* The following are 1-based indices for FITS format */
	int32_t antId1, arrayId1, sourceId1, freqId1;
	int outTones;
	char antName[20];

	//members of pcalinfo should be moved into difxio datastream
	typedef struct
	{
		/*Describes the pcals extracted by DiFX. Calculated as with corresponding values in mpifxcorr configuration.cpp*/
		int *nRecFreqPcal;    /*num of recorded pcals in each sub-band*/
		int **recFreqPcalFreq;/*frequency of each pcal indexed by subband and number within subband*/
		//int *recFreqPcalOffsetHz; // shouldn't be needed for difx2fits
		int maxRecPcal;/*maximum number of pcals in a single subband*/

		/*The following describe the pcals which are to be output*/
		int **freqPcalOut;/*list of tones which will be written out. VEX (1-based) convention*/
		int *nFreqPcalOut;/*length of above list*/
		int nPcalOut;

		/*bool which says whether each recorded tone will be written out to FITS*/
		int **recFreqPcalOut;
		/*total number of pcals out across all subbands*/
	} pcalinfostruct;

	pcalinfostruct *pcalinfo;

	if(D == 0)
	{
		return D;
	}

	nBand = D->nIF;
	nPol = D->nPol;

	mjd2dayno((int)(D->mjdStart), &refDay);

	start = D->mjdStart - (int)(D->mjdStart);
	stop  = D->mjdStop  - (int)(D->mjdStart);

	/*Is DiFX phase cal extraction switched on?*/
	//first check all datastreams and all configs for pcal
	antnTone = calloc(sizeof(int)*D->nAntenna, 1);
	nTone = 0;

	for(i = 0; i < D->nDatastream; i++)
	{
		if(D->datastream[i].phaseCalIntervalMHz != 0)
		{
			nPcalDatastreams++;
			antnTone[D->datastream[i].antennaId] += nBandTone*D->datastream[i].nRecBand;
			if(antnTone[D->datastream[i].antennaId] > nTone)
			{
				nTone += antnTone[D->datastream[i].antennaId];
			}
		}
	}

	if(nPcalDatastreams == 0)
	{
		/* No difx-extracted pcals */
		/* get the maximum dimensions possibly needed */
		nTone = getNTone("pcal", refDay + start, refDay + stop);
		if(nTone < 0)
		{
			/* neither difx no observation pcals */
			return D;
		}

		in = fopen("pcal", "r");
		if(!in)
		{
			return D;
		}
		
		rv = fgets(line, MaxLineLength, in);
		if(!rv)
		{
			fclose(in);
			return D;
		}
	}
	else
	{
		//open first pcal file
		//FIXME cut and paste job from mpifxcorr configuration.cpp
		pcalinfo = (pcalinfostruct *)calloc(D->nDatastream, sizeof(pcalinfo));
		int freqindex, lofreq, tonefreq;
		for(i = 0; i < D->nDatastream; i++)
		{
			pcalinfo[i].nRecFreqPcal = (int *)calloc(D->datastream[i].nRecFreq, sizeof(int));
			pcalinfo[i].recFreqPcalFreq = (int **)calloc(D->datastream[i].nRecFreq, sizeof(int*));
			pcalinfo[i].maxRecPcal = 0;
			pcalinfo[i].freqPcalOut = (int **)calloc(D->datastream[i].nRecFreq, sizeof(int*));
			pcalinfo[i].nFreqPcalOut = (int *)calloc(D->datastream[i].nRecFreq, sizeof(int));
			pcalinfo[i].nPcalOut = 0;

			for(j=0; D->datastream[i].nRecFreq; j++)
			{
				/*FIXME replace with values from vex file/.v2d override*/
				pcalinfo[i].nFreqPcalOut[j] = nBandTone;
				pcalinfo[i].recFreqPcalOut[j] = (int *)calloc(pcalinfo[i].nFreqPcalOut[j], sizeof(int));
				for(t = 0; t < pcalinfo[i].nFreqPcalOut[j]; t++)
				{
					pcalinfo[i].freqPcalOut[j][t] = bandTone[t];
					//FIXME die or ignore if 0? According to vex definition this means state counts rather than pcals(??)
				}

				pcalinfo[i].nRecFreqPcal[j] = 0;
				freqindex = D->datastream[i].recFreqId[j];
				lofreq = D->freq[freqindex].freq;

				if(D->freq[freqindex].sideband == 'L')
				{
					lofreq -= D->freq[freqindex].bw;
				}
				tonefreq = ((int) lofreq/D->datastream[i].phaseCalIntervalMHz)*D->datastream[i].phaseCalIntervalMHz;

				if(tonefreq <= lofreq)
				{
					tonefreq += D->datastream[i].phaseCalIntervalMHz;
				}

				while(tonefreq + pcalinfo[i].nRecFreqPcal[j]*D->datastream[i].phaseCalIntervalMHz < lofreq + D->freq[freqindex].bw)
				{
					pcalinfo[i].nRecFreqPcal[j]++;
				}
				if(pcalinfo[i].nRecFreqPcal[j] > pcalinfo[i].maxRecPcal)
				{
					pcalinfo[i].maxRecPcal = pcalinfo[i].nRecFreqPcal[j];
				}
				pcalinfo[i].recFreqPcalFreq[j] = (int *)calloc(pcalinfo[i].nRecFreqPcal[j], sizeof(int));
				tonefreq = ((int) lofreq / D->datastream[i].phaseCalIntervalMHz) * D->datastream[i].phaseCalIntervalMHz;
				if(tonefreq < lofreq)
				{
					tonefreq += D->datastream[i].phaseCalIntervalMHz;
				}
				for(k = 0; k < pcalinfo[i].nRecFreqPcal[j]; k++)
				{
					pcalinfo[i].recFreqPcalFreq[j][k] = tonefreq + k * D->datastream[i].phaseCalIntervalMHz;
					for(t = 0; t < pcalinfo[i].nFreqPcalOut[j]; t++)
					{
						if(pcalinfo[i].freqPcalOut[j][t] > 0)
						{
							if(pcalinfo[i].freqPcalOut[j][t] == k + 1)
							{
								if(!pcalinfo[i].recFreqPcalOut[j][k])
								{
									pcalinfo[i].nPcalOut++;
								}
								pcalinfo[i].recFreqPcalOut[j][k] = 1;
								break;
							}
						}
						if(pcalinfo[i].freqPcalOut[j][t] < 0)
						{
							if(pcalinfo[i].freqPcalOut[j][t] + pcalinfo[i].nRecFreqPcal[j] == k)
							{
								if(!pcalinfo[i].recFreqPcalOut[j][k])
								{
									pcalinfo[i].nPcalOut++;
								}
								pcalinfo[i].recFreqPcalOut[j][k] = 1;
								break;
							}
						}
					}
				}
				//pcalinfo[i].recFreqPcalOffsetHz[j] = long(1e6*pcalinfo[i].recFreqPcalFreq[j][0]) - long(1e6*lofreq);
			}
		}
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

	/* set defaults */
	for(i = 0; i < array_MAX_TONES; i++)
	{
		pulseCalRate[0][i] = 0.0;
		pulseCalRate[1][i] = 0.0;
	}
	antId = 0;
	arrayId1 = 1;

	for(i = 0; i < D->nDatastream; i++)
	{
		rv = fgets(line, MaxLineLength, in);
		if(!rv)
		{
			if(nPcalDatastreams != 0)
			{
				//FIXME move onto next file
			}
			else
			{
				break;
			}
		}
			
		/* ignore possible comment lines */
		if(line[0] == '#')
		{
			continue;
		}
		else 
		{
			if(nPcalDatastreams != 0)
			{
				n = sscanf(line, "%s%n", antName, &p);
				if(n != 1 || !strcmp(antName, D->antenna[i].name))
				{
					continue;	
				}
				v = parsePulseCal(line, i, &sourceId, &time, &timeInt, 
					&cableCal, freqs, pulseCalRe, pulseCalIm,
					stateCount, refDay, D, &configId, phasecentre);
			}
			if(v < 0)
			{
				continue;
			}

			freqId1 = D->config[configId].freqId + 1;
			sourceId1 = D->source[sourceId].fitsSourceIds[configId] + 1;
			antId1 = antId + 1;

			p_fitsbuf = fitsbuf;
		
			FITS_WRITE_ITEM (time, p_fitsbuf);
			FITS_WRITE_ITEM (timeInt, p_fitsbuf);
			FITS_WRITE_ITEM (sourceId1, p_fitsbuf);
			FITS_WRITE_ITEM (antId1, p_fitsbuf);
			FITS_WRITE_ITEM (arrayId1, p_fitsbuf);
			FITS_WRITE_ITEM (freqId1, p_fitsbuf);
			FITS_WRITE_ITEM (cableCal, p_fitsbuf);

			for(i = 0; i < nPol; i++)
			{
				FITS_WRITE_ARRAY(stateCount[i], p_fitsbuf,
					4*nBand);
				if(nTone > 0)
				{
					FITS_WRITE_ARRAY(freqs[i],
						p_fitsbuf, nTone*nBand);
					FITS_WRITE_ARRAY(pulseCalRe[i], 
						p_fitsbuf, nTone*nBand);
					FITS_WRITE_ARRAY(pulseCalIm[i], 
						p_fitsbuf, nTone*nBand);
					FITS_WRITE_ARRAY(pulseCalRate[i], 
						p_fitsbuf, nTone*nBand);
				}
			}

			testFitsBufBytes(p_fitsbuf - fitsbuf, nRowBytes, "PH");
			
#ifndef WORDS_BIGENDIAN
			FitsBinRowByteSwap(columns, nColumn, fitsbuf);
#endif
			fitsWriteBinRow(out, fitsbuf);
		}
	}

	if(nPcalDatastreams == 0)
	{
		/* close the file, free memory, and return */
		fclose(in);
	}


	free(fitsbuf);
	//FIXME free pcalinfo

	return D;
}
