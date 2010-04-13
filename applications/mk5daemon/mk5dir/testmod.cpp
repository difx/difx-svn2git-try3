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
#include "watchdog.h"
#include "mark5dir.h"
#include "../config.h"

const char program[] = "testmod";
const char author[]  = "Walter Brisken";
const char version[] = "0.1";
const char verdate[] = "20100127";

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
	printf("  --force\n");
	printf("  -f         Don't ask before proceeding\n\n");
	printf("  --help\n");
	printf("  -h         Print help info and quit\n\n");
	printf("  --readonly\n");
	printf("  -r         Perform read-only test\n\n");
	printf("  --realtime\n");
	printf("  -R         Enable real-time mode (sometimes needed for bad packs)\n\n");
	printf("  --skipdircheck\n");
	printf("  -d         Disable directory checking (sometimes needed for bad packs)\n\n");
	printf("  --nrep <n>\n");
	printf("  -n <n>     Perform the test <n> times\n\n");
	printf("  --blocksize <s>\n");
	printf("  -s <s>     Use a read/write block of <s> MB\n\n");
	printf("  --nblock <b>\n");
	printf("  -b <b>     Read/write <b> blocks per test\n\n");
	printf("  --dirfile <f>\n");
	printf("  -o <f>     Write the entire module directory to file <f>\n\n");
	printf("  --position <p>\n");
	printf("  -p <p>     Start read tests from pointer position <p>\n\n");
	printf("and <bank> should be either A or B\n\n");

	return 0;
}

int roundsize(int s)
{
	long long a;

	a = (long long)s*512LL;
	a /= 1000000000;
	a = (a+2)/5;

	return a*5;
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

long long readblock(SSHANDLE xlrDevice, int num, char *buffer1, char *buffer2, int size, int nRep, long long ptr)
{
	XLR_RETURN_CODE xlrRC;
	int r, v;
	long long pos, L = 0;
	unsigned int a, b;
	
	printf("Reading ");
	for(r = 0; r < nRep; r++)
	{
		pos = (long long)size*(num*nRep + r) + ptr;
		a = pos>>32;
		b = pos & 0xFFFFFFFF;
		WATCHDOG( xlrRC = XLRReadData(xlrDevice, (streamstordatatype *)buffer2, a, b, size) );
		if(xlrRC != XLR_SUCCESS)
		{
			fprintf(stderr, "XLRReadData error pos=%Ld a=%u b=%u size=%d\n", pos, a, b, size);
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

int isLegalVSN(const char *vsn)
{
	int i;
	int ns=0, nd=0;

	for(i = 0; vsn[i] > ' '; i++)
	{
		if(vsn[i] == '-' || vsn[i] == '+')
		{
			if(i > 7)
			{
				return 0;
			}
			nd++;
		}
		if(vsn[i] == '/')
		{
			ns++;
		}
	}

	if(ns != 2 || nd != 1)
	{
		return 0;
	}

	return 1;
}

int getLabels(SSHANDLE xlrDevice, DifxMessageMk5Status *mk5status)
{
	S_BANKSTATUS bankStat;

	mk5status->activeBank = ' ';

	WATCHDOGTEST( XLRGetBankStatus(xlrDevice, BANK_A, &bankStat) );
	if(isLegalVSN(bankStat.Label))
	{
		strncpy(mk5status->vsnA, bankStat.Label, 8);
		mk5status->vsnA[8] = 0;
	}
	else
	{
		mk5status->vsnA[0] = 0;
	}
	if(bankStat.Selected)
	{
		mk5status->activeBank = 'A';
	}
	WATCHDOGTEST( XLRGetBankStatus(xlrDevice, BANK_B, &bankStat) );
	if(isLegalVSN(bankStat.Label))
	{
		strncpy(mk5status->vsnB, bankStat.Label, 8);
		mk5status->vsnB[8] = 0;
	}
	else
	{
		mk5status->vsnB[0] = 0;
	}
	if(bankStat.Selected)
	{
		mk5status->activeBank = 'B';
	}

	return 0;
}

int testModule(int bank, int readOnly, int nWrite, int bufferSize, int nRep, int options, int force, const char *dirFile, long long ptr)
{
	SSHANDLE xlrDevice;
	XLR_RETURN_CODE xlrRC;
	S_DRIVESTATS dstats[XLR_MAXBINS];
	S_DRIVEINFO dinfo;
	S_BANKSTATUS bankStat;
	S_DIR dir;
	char label[XLR_LABEL_LENGTH+1];
	char oldLabel[XLR_LABEL_LENGTH+1];
	int labelLength = 0, rs = 0;
	int badLabel = 0;
	int dirLen;
	int nSlash=0, nDash=0, nSep=0;
	int size;
	int ndisk;
	int n, d, v, i;
	long long L, totalError=0, totalBytes=0;
	char drivename[XLR_MAX_DRIVENAME+1];
	char driveserial[XLR_MAX_DRIVESERIAL+1];
	char driverev[XLR_MAX_DRIVEREV+1];
	char *buffer1, *buffer2;
	DifxMessageMk5Status mk5status;
	char label8[10], message[1000];
	char resp[12] = "Y";
	char *rv;
	char *buffer;
	FILE *out;

	buffer1 = (char *)malloc(bufferSize);
	buffer2 = (char *)malloc(bufferSize);
	memset(&mk5status, 0, sizeof(mk5status));

	WATCHDOGTEST( XLROpen(1, &xlrDevice) );
	WATCHDOGTEST( XLRSetBankMode(xlrDevice, SS_BANKMODE_NORMAL) );
	WATCHDOGTEST( XLRSetOption(xlrDevice, options) );
	WATCHDOGTEST( XLRGetBankStatus(xlrDevice, bank, &bankStat) );
	if(bankStat.State != STATE_READY)
	{
		fprintf(stderr, "Bank %c not ready\n", 'A'+bank);
		WATCHDOG( XLRClose(xlrDevice) );
		return -1;
	}
	if(!bankStat.Selected)
	{
		WATCHDOGTEST( xlrRC = XLRSelectBank(xlrDevice, bank) );
	}

	/* the following line is essential to work around an apparent streamstor bug */
	WATCHDOGTEST( XLRGetDirectory(xlrDevice, &dir) );

	WATCHDOGTEST( XLRGetLabel(xlrDevice, label) );

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

	strcpy(oldLabel, label);
	for(i = 0; oldLabel[i]; i++)
	{
		if(oldLabel[i] == '/')
		{
			nSlash++;
		}
		if(oldLabel[i] == '+' || oldLabel[i] == '-')
		{
			nDash++;
		}
		if(oldLabel[i] == 30)
		{
			nSep++;
		}
		if(oldLabel[i] < ' ')
		{
		
			oldLabel[i] = 0;
			break;
		}
	}
	if(nSlash == 2 && nDash == 1 && nSep == 1)
	{
		printf("\nCurrent extended VSN is %s\n", oldLabel);
		if(nSep == 1)
		{
			printf("Current disk module state is %s\n", oldLabel+1+i);
		}
	}
	else
	{
		printf("\nNo VSN currently set on module\n");
		badLabel = 1;
	}

	ndisk = 0;
	printf("\n");
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
		size = roundsize(dinfo.Capacity);
		printf(" %d  %s %s %s  %d %d %d", d, drivename, driveserial, driverev, 
			size, dinfo.SMARTCapable, dinfo.SMARTState);
		if(dinfo.SMARTCapable && !dinfo.SMARTState)
		{
			printf("  FAILED\n");
		}
		else
		{
			printf("\n");
		}
		ndisk++;
	}

	if(!readOnly)
	{
		if(!force && dir.Length > 0)
		{
			printf("\nAbout to perform destructive write/read test.\n");
			printf("This module contains %lld bytes of recorded data\n", dir.Length);
			printf("This test will erase all data on this module!\n");
			printf("Do you wish to continue? [y|n]\n");
			rv = fgets(resp, 10, stdin);
		}
		else
		{
			printf("This module appears empty.\n");
		}
		if(force || resp[0] == 'Y' || resp[0] == 'y')
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
				buffer = (char *)malloc(dirLen);
				WATCHDOG( xlrRC = XLRGetUserDir(xlrDevice, dirLen, 0, buffer) );
				if(xlrRC != XLR_SUCCESS)
				{
					fprintf(stderr, "XLRGetUserDir Failed\n");
					free(buffer);
					return -1;
				}

				if(dirFile)
				{
					n = 0;
					out = fopen(dirFile, "w");
					if(out)
					{
						n = fwrite(buffer, dirLen, 1, out);
						fclose(out);
					}
					if(n != 1)
					{
						fprintf(stderr, "Cannot write module directory to file %s\n", dirFile);
					}
				}

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

			printf("\n");

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

			if(badLabel)
			{
				fprintf(stderr, "Warning: Not updating module label.\n");
				fprintf(stderr, "Please reset the label manually using the vsn program\n\n");
			}
			else
			{
				WATCHDOGTEST( XLRSetLabel(xlrDevice, label, strlen(label)) );
				printf("New disk module state is %s\n", "Erased");
			}

			for(n = 0; n < nWrite; n++)
			{
				mk5status.position = (long long)bufferSize*nRep*n;
				mk5status.scanNumber = n+1;

				if(getLabels(xlrDevice, &mk5status) < 0)
				{
					fprintf(stderr, "Error getting bank status\n");
					return -1;
				}

				mk5status.state = MARK5_STATE_TESTWRITE;
				difxMessageSendMark5Status(&mk5status);

				printf("\nTest %d/%d\n", n+1, nWrite);
				setbuffer(n, buffer1, bufferSize);

				v = writeblock(xlrDevice, n, buffer1, bufferSize, nRep);
				if(v < 0)
				{
					return -1;
				}
				
				mk5status.state = MARK5_STATE_TESTREAD;
				difxMessageSendMark5Status(&mk5status);

				L = readblock(xlrDevice, n, buffer1, buffer2, bufferSize, nRep, 0);
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

			printf("\n");

			for(d = 0; d < 8; d++)
			{
				WATCHDOG( xlrRC = XLRGetDriveStats(xlrDevice, d/2, d%2, dstats) );
				if(xlrRC != XLR_SUCCESS)
				{
					fprintf(stderr, "XLRGetDriveStats failed for drive %d\n", d);
					continue;
				}
				printf("Stats[%d] = %u %u %u %u %u %u %u %u\n", d,
					(unsigned int)(dstats[0].count), 
					(unsigned int)(dstats[1].count),
					(unsigned int)(dstats[2].count), 
					(unsigned int)(dstats[3].count),
					(unsigned int)(dstats[4].count), 
					(unsigned int)(dstats[5].count),
					(unsigned int)(dstats[6].count), 
					(unsigned int)(dstats[7].count));
			}

			if(die)
			{
				printf("User interrupt: Stopping test early!\n");
			}
			printf("\nTotal: %Ld/%Ld bytes differ\n", totalError, totalBytes);

			strncpy(label8, label, 8);
			label8[8] = 0;
			sprintf(message, "%8s test complete: %Ld/%Ld bytes read incorrectly", label8, totalError, totalBytes);
			difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_INFO);

			WATCHDOGTEST( XLRErase(xlrDevice, SS_OVERWRITE_NONE) );

			mk5status.state = MARK5_STATE_CLOSE;
			mk5status.activeBank = ' ';
			mk5status.position = 0;
			mk5status.scanNumber = 0;
			difxMessageSendMark5Status(&mk5status);
		}
	}
	else /* Read-only test here */
	{
		for(n = 0; n < nWrite; n++)
		{
			mk5status.position = (long long)bufferSize*nRep*n + ptr;
			mk5status.scanNumber = n+1;

			if(getLabels(xlrDevice, &mk5status) < 0)
			{
				fprintf(stderr, "Error getting bank status\n");
				return -1;
			}


			printf("\nTest %d/%d\n", n+1, nWrite);
			setbuffer(n, buffer1, bufferSize);

			
			mk5status.state = MARK5_STATE_TESTREAD;
			difxMessageSendMark5Status(&mk5status);

			L = readblock(xlrDevice, n, buffer1, buffer2, bufferSize, nRep, ptr);
			if(L < 0)
			{
				return -1;
			}

			totalError += L;
			totalBytes += (long long)bufferSize*nRep;
			if(die)
			{
				break;
			}
		}

		printf("\n");

		for(d = 0; d < 8; d++)
		{
			WATCHDOG( xlrRC = XLRGetDriveStats(xlrDevice, d/2, d%2, dstats) );
			if(xlrRC != XLR_SUCCESS)
			{
				fprintf(stderr, "XLRGetDriveStats failed for drive %d\n", d);
				continue;
			}
			printf("Stats[%d] = %u %u %u %u %u %u %u %u\n", d,
				(unsigned int)(dstats[0].count), 
				(unsigned int)(dstats[1].count),
				(unsigned int)(dstats[2].count), 
				(unsigned int)(dstats[3].count),
				(unsigned int)(dstats[4].count), 
				(unsigned int)(dstats[5].count),
				(unsigned int)(dstats[6].count), 
				(unsigned int)(dstats[7].count));
		}

		if(die)
		{
			printf("User interrupt: Stopping test early!\n");
		}

		strncpy(label8, label, 8);
		label8[8] = 0;
		sprintf(message, "%8s read test complete", label8);
		difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_INFO);

		mk5status.state = MARK5_STATE_CLOSE;
		mk5status.activeBank = ' ';
		mk5status.position = 0;
		mk5status.scanNumber = 0;
		difxMessageSendMark5Status(&mk5status);
	}

	WATCHDOG( XLRClose(xlrDevice) );

	free(buffer1);
	free(buffer2);

	return 0;
}

int main(int argc, char **argv)
{
	int a, v;
	int bank = -1;
	int nRep = 2;
	int blockSize = 10000000;
	int nBlock = 50;
	int readOnly = 0;
	int verbose = 0;
	int force = 0;
	int options = SS_OPT_DRVSTATS;
	char *dirFile = 0;
	long long ptr = 0;

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
			else if(strcmp(argv[a], "-f") == 0 ||
			   strcmp(argv[a], "--force") == 0)
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
				if(strcmp(argv[a], "-n") == 0 ||
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
				else if(strcmp(argv[a], "-o") == 0 ||
					strcmp(argv[a], "--dirfile") == 0)
				{
					dirFile = argv[a+1];
				}
				else if(strcmp(argv[a], "-p") == 0 ||
					strcmp(argv[a], "--pointer") == 0)
				{
					ptr = atoll(argv[a+1]);
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

	if(bank < 0)
	{
		fprintf(stderr, "No bank specified\n");
		fprintf(stderr, "Run with -h for help info\n");
		return -1;
	}

	oldsiginthand = signal(SIGINT, siginthand);

	setWatchdogVerbosity(verbose);

	v = initWatchdog();
	if(v < 0)
	{
		return 0;
	}

	/* 40 seconds should be enough to complete any XLR command */
	setWatchdogTimeout(40);

	difxMessageInit(-1, program);

	/* *********** */

	testModule(bank, readOnly, nRep, blockSize, nBlock, options, force, dirFile, ptr);

	/* *********** */

	stopWatchdog();

	return 0;
}
