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
// $Id$
// $HeadURL$
// $LastChangedRevision$
// $Author$
// $LastChangedDate$
//
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <difxmessage.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include "config.h"
#include "mark5dir.h"
#include "replaced.h"
#include "watchdog.h"
#include "../config.h"

const char program[] = "testmod";
const char author[]  = "Walter Brisken";
const char version[] = "0.1";
const char verdate[] = "20100121";

int die = 0;
typedef void (*sighandler_t)(int);
sighandler_t oldsiginthand;

void siginthand(int j)
{
	fprintf(stderr, "<Being killed>");
	die = 1;
	signal(SIGHUP, oldsiginthand);
}

int usage(const char *pgm)
{
	printf("\n%s ver. %s   %s %s\n\n", program, version, author, verdate);
	printf("A program to test Mark5 modules\n\n");
	printf("Usage: %s <options> <bank>\n\n", pgm);
	printf("Where <options> can include:\n\n");
	printf("  --verbose\n");
	printf("  -v         Increase the verbosity\n\n");
	printf("  --help\n");
	printf("  -h         Print help info and quit\n\n");
	printf("  --readonly\n");
	printf("  -r         Perform read-only test\n\n");
	printf("  --readwrite\n");
	printf("  -w         Perform destructive read/write test\n\n");
	printf("  --realtime\n");
	printf("  -R         Enable real-time playback mode (for partial packs)\n\n");
	printf("  --skipdircheck\n");
	printf("  -d         Disable directory checking (sometimes needed for bad packs)\n\n");
	printf("  --nrep <n>\n");
	printf("  -n <n>     Perform the test <n> times\n\n");
	printf("  --blocksize <s>\n");
	printf("  -s <s>     Use a read/write block of <s> MB\n\n");
	printf("  --nblock <b>\n");
	printf("  -b <b>     Read/write <b> blocks per test\n\n");
	printf("  --VSN <v>\n");
	printf("  -V <v>     Change the module VSN to <v>\n\n");
	printf("and <bank> should be either A or B\n\n");

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

void setbuffer(int num, char *buffer, int size)
{
	int i;

	for(i = 0; i < size; i++) buffer[i] = (num+i) & 0xFF;
}

int comparebuffers(const char *buf1, const char *buf2, int size)
{
	int n = 0;
	int i;

	for(i = 0; i < size; i++)
	{
		if(buf1[i] != buf2[i])
		{
			n++;
		}
	}

	return n;
}

int writeblock(SSHANDLE xlrDevice, int num, char *buffer, int size, int nRep)
{
	int r;

	if(num == 0)
	{
		WATCHDOGTEST( XLRRecord(xlrDevice, 0, 1) );
	}
	else
	{
		WATCHDOGTEST( XLRAppend(xlrDevice) );
	}

	printf("Writing ");
	for(r = 0; r < nRep; r++)
	{
		WATCHDOGTEST( XLRWriteData(xlrDevice, buffer, size) );
		printf("."); fflush(stdout);
		if(die)
		{
			break;
		}
	}
	printf("\n");

	WATCHDOGTEST( XLRStop(xlrDevice) );

	return 0;
}

long long readblock(SSHANDLE xlrDevice, int num, char *buffer1, char *buffer2, int size, int nRep)
{
	XLR_RETURN_CODE xlrRC;
	int r, v;
	long long pos, L = 0;
	unsigned long a, b;
	
	printf("Reading ");
	for(r = 0; r < nRep; r++)
	{
		pos = (long long)size*(num*nRep + r);
		a = pos>>32;
		b = pos & 0xFFFFFFFF;
		WATCHDOG( xlrRC = XLRReadData(xlrDevice, (PULONG)buffer2, a, b, size) );
		if(xlrRC != XLR_SUCCESS)
		{
			fprintf(stderr, "XLRReadData error pos=%Ld a=%d b=%d size=%d\n", pos, a, b, size);
			return -1;
		}
		v = comparebuffers(buffer1, buffer2, size);
		L += v;
		printf("."); fflush(stdout);
		if(die)
		{
			break;
		}
	}
	printf("\n");

	return L;
}

int testModule(int bank, const char *newLabel, int readOnly, int nWrite, int bufferSize, int nRep, int options)
{
	SSHANDLE xlrDevice;
	XLR_RETURN_CODE xlrRC;
	S_DRIVESTATS dstats[XLR_MAXBINS];
	S_DRIVEINFO dinfo;
	S_BANKSTATUS bankStat;
	char label[XLR_LABEL_LENGTH];
	int labelLength = 0, rs = 0;
	int badLabel = 0;
	int dirLen;
	int size, capacity;
	int ndisk, rate;
	int n, d, v;
	long long L, totalError=0, totalBytes=0;
	char drivename[XLR_MAX_DRIVENAME+1];
	char driveserial[XLR_MAX_DRIVESERIAL+1];
	char driverev[XLR_MAX_DRIVEREV+1];
	char *buffer1, *buffer2;

	buffer1 = (char *)malloc(bufferSize);
	buffer2 = (char *)malloc(bufferSize);

	WATCHDOGTEST( XLROpen(1, &xlrDevice) );
	WATCHDOGTEST( XLRSetBankMode(xlrDevice, SS_BANKMODE_NORMAL) );
	WATCHDOGTEST( XLRSetOption(xlrDevice, options) );
	WATCHDOGTEST( XLRGetBankStatus(xlrDevice, bank, &bankStat) );
	if(bankStat.State != STATE_READY)
	{
		fprintf(stderr, "Bank %d not ready\n", bank);
		WATCHDOG( XLRClose(xlrDevice) );
		return -1;
	}
	if(!bankStat.Selected)
	{
		printf("Hold on a few seconds while switching banks...\n");
		WATCHDOGTEST( xlrRC = XLRSelectBank(xlrDevice, bank) );
		sleep(5);
	}

	WATCHDOGTEST( XLRGetLabel(xlrDevice, label) );

	capacity = 0;
	ndisk = 0;
	for(d = 0; d < 8; d++)
	{
		WATCHDOG( xlrRC = XLRGetDriveInfo(xlrDevice, d/2, d%2, &dinfo) );
		if(xlrRC != XLR_SUCCESS)
		{
			fprintf(stderr, "XLRGetDriveInfo failed for disk %d\n", d);
			continue;
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
		WATCHDOGTEST( XLRClearWriteProtect(xlrDevice) );

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

		WATCHDOGTEST( XLRGetLabel(xlrDevice, label) );

		if(newLabel[0])
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

			printf("Original module label = '%s'\n", label);

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
			WATCHDOGTEST( XLRSetLabel(xlrDevice, label, strlen(label)) );
		}

		for(n = 0; n < nWrite; n++)
		{
			printf("Test %d/%d\n", n+1, nWrite);
			setbuffer(n, buffer1, bufferSize);

			v = writeblock(xlrDevice, n, buffer1, bufferSize, nRep);
			if(v < 0)
			{
				return -1;
			}
			L = readblock(xlrDevice, n, buffer1, buffer2, bufferSize, nRep);
			if(L < 0)
			{
				return -1;
			}

			printf("%Ld/%Ld differ\n", L, (long long)bufferSize*nRep);

			totalError += L;
			totalBytes += (long long)bufferSize*nRep;
			if(die)
			{
				break;
			}
		}

		for(d = 0; d < 8; d++)
		{
			WATCHDOG( xlrRC = XLRGetDriveStats(xlrDevice, d/2, d%2, dstats) );
			if(xlrRC != XLR_SUCCESS)
			{
				fprintf(stderr, "XLRGetDriveStats failed for drive %d\n", d);
				continue;
			}
			printf("Stats[%d] = %d %d %d %d %d %d %d %d\n", d,
				dstats[0].count, dstats[1].count,
				dstats[2].count, dstats[3].count,
				dstats[4].count, dstats[5].count,
				dstats[6].count, dstats[7].count);
		}

		if(die)
		{
			printf("User interrupt: Stopping test early!\n");
		}
		printf("Total: %Ld/%Ld bytes differ\n", totalError, totalBytes);

		WATCHDOGTEST( XLRErase(xlrDevice, SS_OVERWRITE_NONE) );
	}
	else
	{
		printf("Label is %s\n", label);
	}

	WATCHDOG( XLRClose(xlrDevice) );

	free(buffer1);
	free(buffer2);

	return 0;
}

int main(int argc, char **argv)
{
	int a, v, i;
	char newVSN[10] = "";
	int bank = -1;
	int nRep=1;
	int blockSize=10000000;
	int nBlock=10;
	int readOnly = 1;
	int verbose = 0;
	int options = SS_OPT_DRVSTATS;

	oldsiginthand = signal(SIGINT, siginthand);

	v = initWatchdog();
	if(v < 0)
	{
		return 0;
	}

	/* 20 seconds should be enough to complete any XLR command */
	setWatchdogTimeout(20);

	for(a = 1; a < argc; a++)
	{
		if(argv[a][0] == 'A' || argv[a][0] == 'a')
		{
			bank = BANK_A;
		}
		else if(argv[a][0] == 'B' || argv[a][0] == 'b')
		{
			bank = BANK_B;
		}
		else if(argv[a][0] == '-')
		{
			if(strcmp(argv[a], "-v") == 0 ||
			   strcmp(argv[a], "--verbose") == 0)
			{
				verbose++;
			}
			else if(strcmp(argv[a], "-h") == 0 ||
			   strcmp(argv[a], "--help") == 0)
			{
				return usage(argv[0]);
			}
			else if(strcmp(argv[a], "-r") == 0 ||
			        strcmp(argv[a], "--readonly") == 0)
			{
				readOnly = 1;
			}
			else if(strcmp(argv[a], "-w") == 0 ||
			        strcmp(argv[a], "--readwrite") == 0)
			{
				readOnly = 0;
			}
			else if(strcmp(argv[a], "-R") == 0 ||
			        strcmp(argv[a], "--realtime") == 0)
			{
				options |= SS_OPT_REALTIMEPLAYBACK;
			}
			else if(strcmp(argv[a], "-d") == 0 ||
			        strcmp(argv[a], "--skipdircheck") == 0)
			{
				options |= SS_OPT_SKIPCHECKDIR;
			}
			else if(a+1 < argc)
			{
				if(strcmp(argv[a], "-V") == 0 ||
				   strcmp(argv[a], "--VSN") == 0)
				{
					if(strlen(argv[a+1]) != 8)
					{
						fprintf(stderr, "VSN length must be 8 characters\n");
						fprintf(stderr, "Run with -h for help info\n");
						return -1;
					}
					strcpy(newVSN, argv[a+1]);
					newVSN[8] = 0;
					for(i = 0; 8; i++)
					{
						newVSN[i] = toupper(newVSN[i]);
					}
				}
				else if(strcmp(argv[a], "-n") == 0 ||
				        strcmp(argv[a], "--nrep") == 0)
				{
					nRep = atoi(argv[a+1]);
				}
				else if(strcmp(argv[a], "-b") == 0 ||
				        strcmp(argv[a], "--nblock") == 0)
				{
					nBlock = atoi(argv[a+1]);
				}
				else if(strcmp(argv[a], "-s") == 0 ||
				        strcmp(argv[a], "--blocksize") == 0)
				{
					blockSize = 1000000*atoi(argv[a+1]);
				}
				else
				{
					fprintf(stderr, "Unknown option %s\n", argv[a]);
					fprintf(stderr, "Run with -h for help info\n");
					return -1;
				}
				a++;
			}
		}
		else
		{
			fprintf(stderr, "Unexpected parameter: %s\n", argv[a]);
			fprintf(stderr, "Run with -h for help info\n");
			return -1;
		}
	}

	setWatchdogVerbosity(verbose);

	if(bank < 0)
	{
		fprintf(stderr, "No bank specified\n");
		fprintf(stderr, "Run with -h for help info\n");
		return -1;
	}

	/* *********** */

	testModule(bank, newVSN, readOnly, nRep, blockSize, nBlock, options);

	/* *********** */

	stopWatchdog();

	return 0;
}
