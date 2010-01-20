/***************************************************************************
 *   Copyright (C) 2010 by Walter Brisken                                  *
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
// $Id: mk5cp.cpp 1888 2009-12-31 21:56:22Z WalterBrisken $
// $HeadURL: https://svn.atnf.csiro.au/difx/applications/mk5daemon/branches/difx-1.5/mk5dir/mk5cp.cpp $
// $LastChangedRevision: 1888 $
// $Author: WalterBrisken $
// $LastChangedDate: 2009-12-31 14:56:22 -0700 (Thu, 31 Dec 2009) $
//
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <difxmessage.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include <pthread.h>
#include "config.h"
#include "mark5dir.h"
#include "replaced.h"
#include "../config.h"

int verbose = 0;
int die = 0;
time_t watchdogTime = 0;
char watchdogStatement[256];
pthread_mutex_t watchdogLock;

#define WATCHDOG(statement) \
{ \
	pthread_mutex_lock(&watchdogLock); \
	watchdogTime = time(0); \
	strcpy(watchdogStatement, #statement); \
	if(verbose) printf("Executing (at time %d): %s\n", (int)(watchdogTime), watchdogStatement); \
	pthread_mutex_unlock(&watchdogLock); \
	statement; \
	pthread_mutex_lock(&watchdogLock); \
	if(verbose) printf("Executed (in %d seconds): %s\n", (int)(time(0)-watchdogTime), watchdogStatement); \
	watchdogTime = 0; \
	watchdogStatement[0] = 0; \
	pthread_mutex_unlock(&watchdogLock); \
}

const char program[] = "testmod";
const char author[]  = "Walter Brisken";
const char version[] = "0.1";
const char verdate[] = "20100115";


typedef void (*sighandler_t)(int);

sighandler_t oldsiginthand;

void *watchdogFunction(void *data)
{
	int deltat;
	int lastdeltat = 0;

	for(;;)
	{
		usleep(100000);
		pthread_mutex_lock(&watchdogLock);

		if(strcmp(watchdogStatement, "DIE") == 0)
		{
			pthread_mutex_unlock(&watchdogLock);
			return 0;
		}
		else if(watchdogTime != 0)
		{
			deltat = time(0) - watchdogTime;
			if(deltat > 20)  // Nothing should take 20 seconds to complete!
			{
				fprintf(stderr, "Watchdog caught a hang-up executing: %s\n", watchdogStatement);
				exit(0);
			}
			else if(deltat != lastdeltat)
			{
				fprintf(stderr, "Waiting %d seconds executing: %s\n", deltat, watchdogStatement);
				lastdeltat = deltat;
			}
		}
		else
		{
			lastdeltat = 0;
		}

		pthread_mutex_unlock(&watchdogLock);
	}
}

void siginthand(int j)
{
	if(verbose)
	{
		printf("Being killed\n");
	}
	die = 1;
	signal(SIGHUP, oldsiginthand);
}

int usage(const char *pgm)
{
	printf("\n%s ver. %s   %s %s\n\n", program, version, author, verdate);

	return 0;
}

int trim(char *out, const char *in)
{
	int i, s=-1, e=0;

	for(i = 0; in[i]; i++)
	{
		if(in[i] > ' ')
		{
			if(s == -1) 
			{
				s = e = i;
			}
			else
			{
				e = i;
			}
		}
	}

	if(s == -1)
	{
		out[0] = 0;
	}
	else
	{
		strncpy(out, in+s, e-s+1);
		out[e-s+1] = 0;
	}

	return 0;
}

int testModule(int bank, const char *newLabel, int readOnly)
{
	SSHANDLE xlrDevice;
	XLR_RETURN_CODE xlrRC;
	char label[XLR_LABEL_LENGTH];
	int labelLength = 0, rs = 0;
	int badLabel = 0;
	int dirLen;
	int size, capacity;
	int ndisk, rate;
	int d, v;
	S_DRIVEINFO dinfo;
	S_BANKSTATUS bankStat;
	char drivename[XLR_MAX_DRIVENAME+1];
	char driveserial[XLR_MAX_DRIVESERIAL+1];
	char driverev[XLR_MAX_DRIVEREV+1];

	WATCHDOG( xlrRC = XLROpen(1, &xlrDevice) );
	if(xlrRC != XLR_SUCCESS)
	{
		fprintf(stderr, "XLROpen Failed\n");
		return -1;
	}

	WATCHDOG( xlrRC = XLRSetBankMode(xlrDevice, SS_BANKMODE_NORMAL) );
	if(xlrRC != XLR_SUCCESS)
	{
		fprintf(stderr, "XLRSetBankMode Failed\n");
		return -1;
	}

	WATCHDOG( xlrRC = XLRGetBankStatus(xlrDevice, bank, &bankStat) );
	if(xlrRC != XLR_SUCCESS)
	{
		fprintf(stderr, "XLRGetBankStatus Failed\n");
		return -1;
	}

	if(bankStat.State != STATE_READY)
	{
		fprintf(stderr, "Bank %d not ready\n", bank);
		WATCHDOG( XLRClose(xlrDevice) );
		return -1;
	}
	if(!bankStat.Selected)
	{
		WATCHDOG( xlrRC = XLRSelectBank(xlrDevice, bank) );
		if(xlrRC != XLR_SUCCESS)
		{
			fprintf(stderr, "XLRSelectBank Failed\n");
			return -1;
		}
		sleep(5);
	}

	WATCHDOG( xlrRC = XLRGetLabel(xlrDevice, label) );
	if(xlrRC != XLR_SUCCESS)
	{
		fprintf(stderr, "XLRGetLabel Failed\n");
		return -1;
	}

	capacity = 0;
	ndisk = 0;
	for(d = 0; d < 8; d++)
	{
		WATCHDOG( xlrRC = XLRGetDriveInfo(xlrDevice, d/2, d%2, &dinfo) );
		if(xlrRC != XLR_SUCCESS)
		{
			fprintf(stderr, "XLRGetDriveInfo failed for disk %d\n", d);
			return -1;
		}
		trim(drivename, dinfo.Model);
		trim(driveserial, dinfo.Serial);
		trim(driverev, dinfo.Revision);
		if(drivename[0] == 0)
		{
			printf(" %d  MISSING\n", d);
			continue;
		}
		size = (dinfo.Capacity/1000000)*512/1000;
		printf(" %d  %s %s %s  %d %d %d", d, drivename, driveserial, driverev, (dinfo.Capacity/1000000)*512/1000,
			dinfo.SMARTCapable, dinfo.SMARTState);
		if(dinfo.SMARTCapable && !dinfo.SMARTState)
		{
			printf("  FAILED\n");
		}
		else
		{
			printf("\n");
		}
		capacity += size;
		ndisk++;
	}

	if(!readOnly)
	{
		WATCHDOG( xlrRC = XLRClearWriteProtect(xlrDevice) );
		if(xlrRC != XLR_SUCCESS)
		{
			fprintf(stderr, "XLRClearWriteProtect Failed\n");
			return -1;
		}

		WATCHDOG( xlrRC = XLRErase(xlrDevice, SS_OVERWRITE_NONE) );
		if(xlrRC != XLR_SUCCESS)
		{
			fprintf(stderr, "XLRErase Failed\n");
			return -1;
		}

		WATCHDOG( dirLen = XLRGetUserDirLength(xlrDevice) );
		if(dirLen > 8)
		{
			char *buffer;
			int i;
			FILE *out;
			
			buffer = (char *)malloc(dirLen);
			WATCHDOG( xlrRC = XLRGetUserDir(xlrDevice, dirLen, 0, buffer) );
			if(xlrRC != XLR_SUCCESS)
			{
				fprintf(stderr, "XLRGetUserDir Failed\n");
				free(buffer);
				return -1;
			}

			out = fopen("dir", "w");
			fwrite(buffer, dirLen, 1, out);
			fclose(out);

			for(i = 0; i < 4; i++) buffer[i] = 0;

			WATCHDOG( xlrRC = XLRSetUserDir(xlrDevice, buffer, dirLen) );
			if(xlrRC != XLR_SUCCESS)
			{
				fprintf(stderr, "XLRSetUserDir Failed\n");
				free(buffer);
				return -1;
			}

			free(buffer);
		}

		WATCHDOG( xlrRC = XLRGetLabel(xlrDevice, label) );
		if(xlrRC != XLR_SUCCESS)
		{
			fprintf(stderr, "XLRGetLabel Failed\n");
			return -1;
		}

		if(newLabel)
		{
			rate = ndisk*128;
			sprintf(label, "%8s/%d/%d%cErased", newLabel, capacity, rate, 30);	/* ASCII "RS" == 30 */
		}
		else
		{
			for(labelLength = 0; labelLength < XLR_LABEL_LENGTH; labelLength++)
			{
				if(!label[labelLength])
				{
					break;
				}
			}

			if(labelLength >= XLR_LABEL_LENGTH)
			{
				fprintf(stderr, "Warning: module label is not terminated!");
				label[XLR_LABEL_LENGTH-1] = 0;
				labelLength = XLR_LABEL_LENGTH-1;
				badLabel = 1;
			}

			if(verbose)
			{
				printf("Original module label = '%s'\n", label);
			}

			if(!badLabel)
			{
				for(rs = 0; rs < labelLength; rs++)
				{
					if(label[rs] == 30)	/* ASCII "RS" == "Record separator" */
					{
						break;
					}
				}
				if(rs >= labelLength)
				{
					fprintf(stderr, "No record separator found in label\n");
					badLabel = 1;
				}
				else
				{
					strcpy(label+rs+1, "Erased");
				}
			}
		}

		if(badLabel)
		{
			fprintf(stderr, "Warning: Not updating module label -- please reset the label manually!");
		}
		else
		{
			printf("New module label = '%s'\n", label);
			WATCHDOG( xlrRC = XLRSetLabel(xlrDevice, label, strlen(label)) );
		}
/*
		v = writeblock(xlrDevice, 0);
		if(v < 0)
		{
			return -1;
		}
		v = readblock(xlrDevice, 0);
		if(v < 0)
		{
			return -1;
		}
*/
		WATCHDOG( xlrRC = XLRErase(xlrDevice, SS_OVERWRITE_NONE) );
		if(xlrRC != XLR_SUCCESS)
		{
			fprintf(stderr, "XLRErase Failed\n");
			return -1;
		}
	}
	else
	{
		printf("Label is %s\n", label);
	}

	WATCHDOG( XLRClose(xlrDevice) );

	return 0;
}

int main(int argc, char **argv)
{
	pthread_t watchdogThread;
	int perr, i;
	char newLabel[100];
	int bank;

	perr = pthread_create(&watchdogThread, NULL, watchdogFunction, 0);

	if(perr != 0)
	{
		fprintf(stderr, "Error -- could not launch watchdog thread!\n");
		exit(0);
	}

	verbose = 0;

	if(argc < 2)
	{
		return usage(argv[0]);
	}
	if(argc > 2)
	{
		strcpy(newLabel, argv[2]);
		for(i = 0; newLabel[i]; i++)
		{
			newLabel[i] = toupper(newLabel[i]);
		}
	}

	switch(argv[1][0])
	{
	case 'a':
	case 'A':
		bank = BANK_A;
		break;
	case 'b':
	case 'B':
		bank = BANK_B;
		break;
	default:
		return usage(argv[0]);
	}

	/* *********** */

	testModule(bank, newLabel, 0);

	/* *********** */

	pthread_mutex_lock(&watchdogLock);
	strcpy(watchdogStatement, "DIE");
	pthread_mutex_unlock(&watchdogLock);
	pthread_join(watchdogThread, NULL);

	return 0;
}
