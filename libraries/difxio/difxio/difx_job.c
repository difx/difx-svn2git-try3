/***************************************************************************
 *   Copyright (C) 2007-2015 by Walter Brisken                             *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "difxio/difx_input.h"
#include "difxio/parsedifx.h"

const char aberCorrStrings[][MAX_ABER_CORR_STRING_LENGTH] = 
{
	"UNCORRECTED",
	"APPROXIMATE",
	"EXACT",
	"NO ATMOS",
	"MIXED"
};


enum AberCorr stringToAberCorr(const char* str)
{
	enum AberCorr ac;
	for(ac = 0; ac < NumAberCorrOptions; ++ac)
	{
		if(strcmp(aberCorrStrings[ac], str) == 0)
		{
			return ac;
		}
	}
	return AberCorrUncorrected;
}


const char performDirectionDerivativeTypeNames[][MAX_PERFORM_DIRECTION_DERIVATIVE_STRING_LENGTH] = 
{
	"NONE",
	"UNKNOWN",
	"DEFAULT",
	"FIRST",
	"FIRST2",
	"SECOND",
	"SECOND2"
};
enum PerformDirectionDerivativeType stringToPerformDirectionDerivativeType(const char *str)
{
	if(strcasecmp(str, "NONE") == 0)
	{
		return PerformDirectionDerivativeNone;
	}
	if(strcasecmp(str, "DEFAULT") == 0)
	{
		return PerformDirectionDerivativeDefault;
	}
	if(strcasecmp(str, "FIRST") == 0)
	{
		return PerformDirectionDerivativeFirstDerivative;
	}
	if(strcasecmp(str, "FIRST2") == 0)
	{
		return PerformDirectionDerivativeFirstDerivative2;
	}
	if(strcasecmp(str, "SECOND") == 0)
	{
		return PerformDirectionDerivativeSecondDerivative;
	}
	if(strcasecmp(str, "SECOND2") == 0)
	{
		return PerformDirectionDerivativeSecondDerivative2;
	}
	return PerformDirectionDerivativeUnknown;
}

const char delayServerTypeNames[][MAX_DELAY_SERVER_NAME_LENGTH] =
{
	"UnknownServer",
	"CALCServer",
	"CALC_9_1_RA_Server",
	"calcderiv",
	"unknown"
};
const unsigned long delayServerTypeIds[] =
{
	0,            /* Default to not knowning anything */
	0x20000340,   /* CALCServer                  */
	0x20000341,   /* CALC_9_1_RA_Server          */
	2,            /* CALCDERIV                   */
	0
};

enum DelayServerType stringToDelayServerType(const char *str)
{
	enum DelayServerType ds;

	for(ds = 0; ds < NumDelayServerTypes; ++ds)
	{
		if(strcasecmp(str, delayServerTypeNames[ds]) == 0)
		{
			break;
		}
	}

	return ds;
}




const char delayServerHandlerTypeNames[][MAX_DELAY_SERVER_NAME_LENGTH] =
{
	"UnknownServerHandler",
	"DiFX_Delay_Server",
	"Pipe",
	"unknown"
};
const unsigned long delayServerHandlerTypeIds[] =
{
	0,            /* Default to not knowning anything */
	0x20000342,   /* DiFX_Delay_Server                */
	1,            /* Pipe                             */
	0
};

enum DelayServerHandlerType stringToDelayServerHandlerType(const char *str)
{
	enum DelayServerHandlerType ds;

	for(ds = 0; ds < NumDelayServerHandlerTypes; ++ds)
	{
		if(strcasecmp(str, delayServerHandlerTypeNames[ds]) == 0)
		{
			break;
		}
	}

	return ds;
}


const char taperFunctionNames[][MAX_TAPER_FUNCTION_STRING_LENGTH] =
{
	"UNIFORM",
	"UNKNOWN"
};

enum TaperFunction stringToTaperFunction(const char *str)
{
	enum TaperFunction f;

	for(f = 0; f < NumTaperFunctions; ++f)
	{
		if(strcasecmp(str, taperFunctionNames[f]) == 0)
		{
			break;
		}
	}

	return f;
}

DifxJob *newDifxJobArray(int nJob)
{
	DifxJob *dj;
	int j;

	dj = (DifxJob *)calloc(nJob, sizeof(DifxJob));
	for(j = 0; j < nJob; j++)
	{
		snprintf(dj[j].obsCode,        DIFXIO_OBSCODE_LENGTH,  "%s", "DIFX");
		dj[j].taperFunction = TaperFunctionUniform;
		dj[j].polyOrder = DIFXIO_DEFAULT_POLY_ORDER;
		dj[j].polyInterval = DIFXIO_DEFAULT_POLY_INTERVAL;
		dj[j].delayModelPrecision = DIFXIO_DEFAULT_DELAY_MODEL_PRECISION;
		dj[j].delayServerType = DIFXIO_DEFAULT_DELAY_SERVER_TYPE;
		dj[j].delayServerHandlerType = DIFXIO_DEFAULT_DELAY_SERVER_HANDLER_TYPE;
		dj[j].delayVersion = DIFXIO_DEFAULT_DELAY_SERVER_VERSION;
		dj[j].delayProgram = delayServerTypeIds[dj[j].delayServerType];
		dj[j].delayHandler = delayServerHandlerTypeIds[dj[j].delayServerHandlerType];
		dj[j].perform_uvw_deriv = PerformDirectionDerivativeFirstDerivative;
		dj[j].perform_lmn_deriv = PerformDirectionDerivativeNone;
		dj[j].perform_xyz_deriv = PerformDirectionDerivativeNone;
		dj[j].delta_lmn = DIFXIO_DEFAULT_DELTA_LMN;
		dj[j].delta_xyz = DIFXIO_DEFAULT_DELTA_XYZ;
		dj[j].aberCorr = DIFXIO_DEFAULT_ABER_CORR_TYPE;
	}

	return dj;
}

static void deleteDifxJobInternals(DifxJob *dj)
{
	deleteDifxAntennaFlagArray(dj->flag);
	dj->flag = 0;
	free(dj->calcParamTable);
	dj->calcParamTable = 0;
	deleteRemap(dj->jobIdRemap);
	dj->jobIdRemap = 0;
	deleteRemap(dj->freqIdRemap);
	dj->freqIdRemap = 0;
	deleteRemap(dj->antennaIdRemap);
	dj->antennaIdRemap = 0;
	deleteRemap(dj->datastreamIdRemap);
	dj->datastreamIdRemap = 0;
	deleteRemap(dj->baselineIdRemap);
	dj->baselineIdRemap = 0;
	deleteRemap(dj->pulsarIdRemap);
	dj->pulsarIdRemap = 0;
	deleteRemap(dj->configIdRemap);
	dj->configIdRemap = 0;
	deleteRemap(dj->sourceIdRemap);
	dj->sourceIdRemap = 0;
	deleteRemap(dj->spacecraftIdRemap);
	dj->spacecraftIdRemap = 0;
}

void deleteDifxJobArray(DifxJob *djarray, int nJob)
{
	int j;
	DifxJob *dj;

	if(djarray)
	{
		for(j = 0; j < nJob; ++j)
		{
			dj = djarray + j;
			deleteDifxJobInternals(dj);
		}
		free(djarray);
	}
}

void fprintDifxJob(FILE *fp, const DifxJob *dj)
{
	fprintf(fp, "  DifxJob : %p\n", dj);
	fprintf(fp, "    job ID = %d\n", dj->jobId);
	fprintf(fp, "    project = %s\n", dj->obsCode);
	if(dj->obsSession[0])
	{
		fprintf(fp, "    session = %s\n", dj->obsSession);
	}
	fprintf(fp, "    start = MJD %12.6f\n", dj->mjdStart);
	fprintf(fp, "    duration = %f sec\n", dj->duration);
	fprintf(fp, "    vex file = %s\n", dj->vexFile);
	fprintf(fp, "    input file = %s\n", dj->inputFile);
	fprintf(fp, "    threads (core conf) file = %s\n", dj->threadsFile);
	fprintf(fp, "    calc file = %s\n", dj->calcFile);
	fprintf(fp, "    im (model) file = %s\n", dj->imFile);
	fprintf(fp, "    flag file = %s\n", dj->flagFile);
	fprintf(fp, "    taperFunction = %s\n", taperFunctionNames[dj->taperFunction]);
	fprintf(fp, "    output file = %s\n", dj->outputFile);
	fprintf(fp, "    delay server host = %s\n", dj->delayServerHost);
	fprintf(fp, "    delay server type = %s\n", delayServerTypeNames[dj->delayServerType]);
	fprintf(fp, "    delay server handler type = %s\n", delayServerHandlerTypeNames[dj->delayServerHandlerType]);
	fprintf(fp, "    delay version = 0x%lX\n", dj->delayVersion);
	fprintf(fp, "    delay program = 0x%lX\n", dj->delayProgram);
	fprintf(fp, "    delay handler = 0x%lX\n", dj->delayHandler);
	fprintf(fp, "    delay detailed version = 0x%lX\n", dj->delayProgramDetailedVersion);
	fprintf(fp, "    perform uvw = %s\n", performDirectionDerivativeTypeNames[dj->perform_uvw_deriv]);
	fprintf(fp, "    perform lmn = %s\n", performDirectionDerivativeTypeNames[dj->perform_lmn_deriv]);
	fprintf(fp, "    perform xyz = %s\n", performDirectionDerivativeTypeNames[dj->perform_xyz_deriv]);
	fprintf(fp, "    delta lmn = %E\n", dj->delta_lmn);
	fprintf(fp, "    delta xyz = %E\n", dj->delta_xyz);
	fprintf(fp, "    aber corr = %s\n", aberCorrStrings[dj->aberCorr]);
	fprintRemap(fp, "  jobId", dj->jobIdRemap);
	fprintRemap(fp, "  freqId", dj->freqIdRemap);
	fprintRemap(fp, "  antennaId", dj->antennaIdRemap);
	fprintRemap(fp, "  datastreamId", dj->datastreamIdRemap);
	fprintRemap(fp, "  baselineId", dj->baselineIdRemap);
	fprintRemap(fp, "  pulsarId", dj->pulsarIdRemap);
	fprintRemap(fp, "  configId", dj->configIdRemap);
	fprintRemap(fp, "  sourceId", dj->sourceIdRemap);
	fprintRemap(fp, "  spacecraftId", dj->spacecraftIdRemap);
}

void printDifxJob(const DifxJob *dj)
{
	fprintDifxJob(stdout, dj);
}

void copyDifxJob(DifxJob *dest, const DifxJob *src, int *antennaIdRemap)
{
	int f;

	if(dest != src)
	{
		deleteDifxJobInternals(dest);
		*dest = *src;
		/* memcpy(dest, src, sizeof(DifxJob)); */

		if(src->nFlag > 0)
		{
			dest->flag = newDifxAntennaFlagArray(src->nFlag);
			for(f = 0; f < dest->nFlag; f++)
			{
				copyDifxAntennaFlag(dest->flag + f,
				                    src->flag + f, antennaIdRemap);
			}
		}
		if((src->calcParamTable))
		{
			dest->calcParamTable = (DifxCalcParamTable*)malloc(sizeof(DifxCalcParamTable));
			*(dest->calcParamTable) = *(src->calcParamTable);
		}

		dest->jobIdRemap = dupRemap(src->jobIdRemap);
		dest->freqIdRemap = dupRemap(src->freqIdRemap);
		dest->antennaIdRemap = dupRemap(src->antennaIdRemap);
		dest->datastreamIdRemap = dupRemap(src->datastreamIdRemap);
		dest->baselineIdRemap = dupRemap(src->baselineIdRemap);
		dest->pulsarIdRemap = dupRemap(src->pulsarIdRemap);
		dest->configIdRemap = dupRemap(src->configIdRemap);
		dest->sourceIdRemap = dupRemap(src->sourceIdRemap);
		dest->spacecraftIdRemap = dupRemap(src->spacecraftIdRemap);
	}
	else if(antennaIdRemap)
	{
		if(src->nFlag > 0)
		{
			for(f = 0; f < dest->nFlag; f++)
			{
				copyDifxAntennaFlag(dest->flag + f,
				                    src->flag + f, antennaIdRemap);
			}
		}
	}
}

/* simply append dj2 after dj1 return new size on call stack : ndj */
DifxJob *mergeDifxJobArrays(const DifxJob *dj1, int ndj1,
	const DifxJob *dj2, int ndj2, int *jobIdRemap, 
	int *antennaIdRemap, int *ndj)
{
	DifxJob *dj;
	int i;

	for(i = 0; i < ndj2; i++)
	{
		jobIdRemap[i] = ndj1 + i;
	}
	
	*ndj = ndj1 + ndj2;
	dj = newDifxJobArray(*ndj);

	for(i = 0; i < ndj1; i++)
	{
		copyDifxJob(dj + i, dj1 + i, 0);
	}
	for(i = 0; i < ndj2; i++)
	{
		copyDifxJob(dj + ndj1 + i, dj2 + i, antennaIdRemap);
	}

	return dj;
}

/* Warning: fileBase should be at least DIFXIO_FILENAME_LENGTH long */
void generateDifxJobFileBase(DifxJob *dj, char *fileBase)
{
	int i, p;

	fileBase[0] = 0;

	if(!dj)
	{
		return;
	}

	for(i = p = 0; dj->outputFile[i]; i++)
	{
		if(dj->outputFile[i] == '/')
		{
			if(dj->outputFile[i+1] != 0)
			{
				p = i + 1;
			}
		}
	}

	strcpy(fileBase, dj->outputFile + p);

	for(i = p = 0; fileBase[i]; i++)
	{
		if(fileBase[i] == '.')
		{
			if(fileBase[i+1] != 0)
			{
				p = i;
			}
		}
	}

	if(p > 0)
	{
		fileBase[p] = 0;
	}
}
