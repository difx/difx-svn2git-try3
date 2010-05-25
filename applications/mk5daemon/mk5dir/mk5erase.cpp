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
#include <string.h> 
#include <unistd.h>
#include <sys/timeb.h>
#include <signal.h>
#include "xlrapi.h"
#include "watchdog.h"
#include "difxmessage.h"

/* Note: this program is largely based on Haystack's SSErase.  Thanks
 * to John Ball and Dan Smythe for providing a nice template.
 * Improvements to the original include:
 *  1. Integration to the difxmessage system
 *  2. Watchdog around all XLR function calls
 */

const char program[] = "mk5erase";
const char author[]  = "Walter Brisken";
const char version[] = "0.1";
const char verdate[] = "20100525";


#define MJD_UNIX0       40587.0
#define SEC_DAY         86400.0
#define MSEC_DAY        86400000.0 


int die = 0;
const char RecordSeparator = 30;

typedef void (*sighandler_t)(int);

sighandler_t oldsiginthand;

void siginthand(int j)
{
	printf("Being killed\n");
	die = 1;
}

void trim(char *out, const char *in)
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
}

int roundSize(long long a)
{
	a /= 1000000000;
	a = (a+2)/5;

	return a*5;
}

int mk5erase(const char *vsn, int cond, int verbose, int dirVersion)
{
	const int printInterval = 10;
	SSHANDLE xlrDevice;
	XLR_RETURN_CODE xlrRC;
	S_BANKSTATUS bankStatus;
	S_DEVINFO devInfo;
	S_DRIVESTATS driveStats[XLR_MAXBINS];
	S_DRIVEINFO driveInfo;
	S_DEVSTATUS devStatus;
	char label[XLR_LABEL_LENGTH+1];
	char driveModel[8][XLR_MAX_DRIVENAME+1];
	char driveSerial[8][XLR_MAX_DRIVESERIAL+1];
	char driveRev[8][XLR_MAX_DRIVEREV+1];
	int driveFailed[8];
	int nDrive;
	long long driveCapacity[8];	/* in bytes */
	long long minCapacity;		/* in bytes */
	int totalCapacity;		/* in GB (approx) */
	double lowestRate;
	int rate = 1024;		/* Mbps, for label */
	DifxMessageCondition condMessage;
	DifxMessageMk5Status mk5status;
	int dirLength;
	char *dirData;
	char message[DIFX_MESSAGE_LENGTH];

	memset((char *)(&mk5status), 0, sizeof(mk5status));

	WATCHDOGTEST( XLROpen(1, &xlrDevice) );
	WATCHDOGTEST( XLRSetBankMode(xlrDevice, SS_BANKMODE_NORMAL) );
	WATCHDOGTEST( XLRGetBankStatus(xlrDevice, BANK_A, &bankStatus) );
	if(bankStatus.State != STATE_READY)
	{
		snprintf(message, DIFX_MESSAGE_LENGTH, "Bank A is not ready");
		fprintf(stderr,  "%s\n", message);
		difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_ERROR);
		WATCHDOG( XLRClose(xlrDevice) );
		return -1;
	}
	if(!bankStatus.Selected)
	{
		snprintf(message, DIFX_MESSAGE_LENGTH, 
			"To continue, the target module needs to be alone in bank A.");
		fprintf(stderr,  "%s\n", message);
		difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_ERROR);
		WATCHDOG( XLRClose(xlrDevice) );
		return -1;
	}
	
	WATCHDOGTEST( XLRGetLabel(xlrDevice, label) );
	if(strncasecmp(label, vsn, 8) != 0)
	{
		snprintf(message, DIFX_MESSAGE_LENGTH, 
			"VSN mismatch: %s is not %s", vsn, label);
		fprintf(stderr,  "%s\n", message);
		difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_ERROR);
		WATCHDOG( XLRClose(xlrDevice) );
		return -1;
		
	}

	WATCHDOGTEST( XLRSetMode(xlrDevice, SS_MODE_PCI) );
	WATCHDOGTEST( XLRGetDeviceInfo(xlrDevice, &devInfo) );
	WATCHDOGTEST( XLRGetDeviceStatus(xlrDevice, &devStatus) );
	WATCHDOGTEST( XLRClearOption(xlrDevice, SS_OPT_SKIPCHECKDIR) );
	WATCHDOGTEST( XLRClearWriteProtect(xlrDevice) );

	nDrive = 0;
	minCapacity = 0LL;
	for(int d = 0; d < 8; d++)
	{
		WATCHDOG( xlrRC = XLRGetDriveInfo(xlrDevice, d/2, d%2, &driveInfo) );
		if(xlrRC != XLR_SUCCESS)
		{
			snprintf(message, DIFX_MESSAGE_LENGTH,
				"XLRGetDriveInfo failed for disk %d", d);
			printf("message\n");
			difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_WARNING);

			driveModel[d][0] = 0;
			driveSerial[d][0] = 0;
			driveRev[d][0] = 0;
			driveFailed[d] = 0;
		
			continue;
		}

		trim(driveModel[d], driveInfo.Model);
		trim(driveSerial[d], driveInfo.Serial);
		trim(driveRev[d], driveInfo.Revision);
		driveCapacity[d] = driveInfo.Capacity * 512LL;
		if(driveCapacity[d] > 0)
		{
			nDrive++;
		}
		if(minCapacity == 0 || 
		   (driveCapacity[d] > 0 && driveCapacity[d] < minCapacity))
		{
			minCapacity = driveCapacity[d];
		}
		if(driveInfo.SMARTCapable && !driveInfo.SMARTState)
		{
			driveFailed[d] = 1;
		}
		else
		{
			driveFailed[d] = 0;
		}
	}
	totalCapacity = (nDrive*minCapacity/10000000000LL)*10;

	printf("> Module %s consists of %d drives totalling about %d GB:\n",
		vsn, nDrive, totalCapacity);
	for(int d = 0; d < 8; d++)
	{
		if(driveModel[d][0] == 0)
		{
			continue;
		}
		printf("> Disk %d info : %s : %s : %s : %d : %s\n",
			d, driveModel[d], driveSerial[d], driveRev[d], 
			roundSize(driveCapacity[d]),
			driveFailed[d] ? "FAILED" : "OK");
	}

	strcpy(mk5status.vsnA, vsn);
	mk5status.activeBank = 'A';
	/* note: there is no module in bank B */

	WATCHDOGTEST( XLRSetOption(xlrDevice, SS_OPT_DRVSTATS) );

	if(cond == 0)
	{
		/* here just erasing */
		WATCHDOGTEST( XLRErase(xlrDevice, SS_OVERWRITE_NONE) );
	}
	else
	{
		/* here doing the full condition */

		long long len, lenLast=0;
		long long lenFirst=0;
		struct timeb time1, time2;
		double dt;

		lowestRate = 1e9;	/* Unphysically fast! */
		mk5status.state = MARK5_STATE_CONDITION;
		difxMessageSendMark5Status(&mk5status);
		
		WATCHDOGTEST( XLRErase(xlrDevice, SS_OVERWRITE_RW_PATTERN) );
	
		ftime(&time1);

		for(int n = 0; ; n++)
		{
			WATCHDOG( len = XLRGetLength(xlrDevice) );
			if(lenLast == 0)
			{
				lenFirst = lenLast = len;
			}
			WATCHDOGTEST( XLRGetDeviceStatus(xlrDevice, &devStatus) );
			if(!devStatus.Recording)
			{
				break;	/* must be done */
			}
			if(die)
			{
				snprintf(message, DIFX_MESSAGE_LENGTH, "Conditioning aborted.");
				fprintf(stderr, "%s\n", message);
				difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_WARNING);

				break;
			}
			if(n == printInterval)
			{
				long long bytes;
				double done;

				n = 0;

				bytes = -(len-lenLast);	// It is counting down
				done = 100.0*(double)(lenFirst-len)/(double)len;
				mk5status.position = devInfo.NumBuses*len;
				mk5status.rate = 8*devInfo.NumBuses*bytes/(printInterval*1000000.0);
				sprintf(mk5status.scanName, "[%4.2f%%]", done);
				if(mk5status.rate < lowestRate)
				{
					lowestRate = mk5status.rate;
				}
				ftime(&time2);
				dt = time2.time + time2.millitm/1000.0 
				   - time1.time - time1.millitm/1000.0;
				
				if(len != lenLast)
				{
					mk5status.state = MARK5_STATE_CONDITION;
				}
				else
				{
					mk5status.state = MARK5_STATE_COND_ERROR;
				}
				difxMessageSendMark5Status(&mk5status);

				printf(". Time = %7.2f  Position = %Ld  Rate = %f  Done = %5.2f %%\n",
					dt, mk5status.position, mk5status.rate, done);

				lenLast = len;
			}
			sleep(1);
		}

		ftime(&time2);
		dt = time2.time + time2.millitm/1000.0 
		   - time1.time - time1.millitm/1000.0;

		if(die)
		{
			WATCHDOG( XLRClose(xlrDevice) );
			printf("! Conditioning cancelled.  Reopening device\n");
			WATCHDOGTEST( XLROpen(1, &xlrDevice) );
			WATCHDOGTEST( XLRSetBankMode(xlrDevice, SS_BANKMODE_NORMAL) );
		}
		else
		{
			printf("> Conditioning %s took %7.2f seconds", vsn, dt);

			rate = 128*(int)(lowestRate/128.0);

			condMessage.startMJD = MJD_UNIX0 + time1.time/SEC_DAY + time1.millitm/MSEC_DAY;
			condMessage.stopMJD = MJD_UNIX0 + time2.time/SEC_DAY + time2.millitm/MSEC_DAY;
			strncpy(condMessage.moduleVSN, vsn, DIFX_MESSAGE_MARK5_VSN_LENGTH);
			condMessage.moduleVSN[DIFX_MESSAGE_MARK5_VSN_LENGTH] = 0;

			for(int d = 0; d < 8; d++)
			{
				WATCHDOG( xlrRC = XLRGetDriveInfo(xlrDevice, d/2, d%2, &driveInfo) );
				if(xlrRC == XLR_SUCCESS)
				{
					condMessage.moduleSlot = d;
					strncpy(condMessage.serialNumber, 
						driveSerial[d], 
						DIFX_MESSAGE_DISC_SERIAL_LENGTH);
					strncpy(condMessage.modelNumber,
						driveModel[d],
						DIFX_MESSAGE_DISC_MODEL_LENGTH);
					condMessage.diskSize = driveCapacity[d]/1000000000LL;
					printf("> Disk %d stats : %s", d, driveSerial[d]);
					for(int i = 0; i < DIFX_MESSAGE_N_CONDITION_BINS; i++)
					{
						condMessage.bin[i] = -1;
					}
					WATCHDOG( xlrRC = XLRGetDriveStats(xlrDevice, d/2, d%2, driveStats) );
					if(xlrRC == XLR_SUCCESS)
					{
						for(int i = 0; i < XLR_MAXBINS; i++)
						{
							if(i < DIFX_MESSAGE_N_CONDITION_BINS)
							{
								condMessage.bin[i] = driveStats[i].count;
								printf(" : %d", condMessage.bin[i]);
							}
						}
					}
					printf("\n");
				}
				else
				{
					printf("! Disk %d stats : Not found!\n", d);
				}
			}
		}
	}

	/* clear the user directory */
	if(dirVersion < 0)
	{
		WATCHDOG( dirLength = XLRGetUserDirLength(xlrDevice) );
		if(dirLength < 80000 || dirLength % 128 == 0)
		{
			dirLength = 128;
		}
	}
	else if(dirVersion == 0)
	{
		dirLength = 83424;
	}
	else
	{
		dirLength = 128;
	}

	printf("> Dir Size = %d\n", dirLength);

	dirData = (char *)calloc(dirLength, 1);
	WATCHDOGTEST( XLRSetUserDir(xlrDevice, dirData, dirLength) );
	free(dirData);

	/* reset the label */
	snprintf(label, XLR_LABEL_LENGTH, "%8s/%d/%d%c%s", 
		vsn, totalCapacity, rate, RecordSeparator,
		die ? "Erased" : "Error");
	printf("> New label = %s\n", label);
	WATCHDOGTEST( XLRSetLabel(xlrDevice, label, strlen(label)) );


	/* finally close the device */
	WATCHDOG( XLRClose(xlrDevice) );

	mk5status.state = MARK5_STATE_IDLE;
	mk5status.rate = 0;
	mk5status.position = 0;
	difxMessageSendMark5Status(&mk5status);

	return 0;
}

int usage(const char *pgm)
{
	printf("\n%s ver. %s   %s %s\n\n", program, version, author, verdate);
	printf("A program to erase or condition a Mark5 module.\n");
	printf("\nUsage : %s [<options>] <vsn>\n\n", pgm);
	printf("options can include:\n");
	printf("  --help\n");
	printf("  -h             Print this help message\n\n");
	printf("  --verbose\n");
	printf("  -v             Be more verbose\n\n");
	printf("  --force\n");
	printf("  -f             Don't ask to continue\n\n");
	printf("  --condition\n");
	printf("  -c             Do full conditioning, not just erasing\n");
	printf("<vsn> is a valid module VSN (8 characters)\n\n");
	printf("Note: A single Mark5 unit needs to be installed in bank A for\n");
	printf("proper operation.  If the VSN is not set, use the  vsn  utility\n");
	printf("To assign it prior to erasure or conditioning.\n\n");

	return 0;
}

int main(int argc, char **argv)
{
	int verbose = 0;
	int force = 0;
	int cond = 0;
	char vsn[10] = "";
	char resp[12] = " ";
	char *rv;
	int v;
	int dirVersion = -1;

	if(argc < 2)
	{
		printf("Please run with -h for help\n");
		return 0;
	}

	for(int a = 1; a < argc; a++)
	{
		if(strcmp(argv[a], "-h") == 0 ||
		   strcmp(argv[a], "--help") == 0)
		{
			return usage(argv[0]);
		}
		else if(strcmp(argv[a], "-v") == 0 ||
		        strcmp(argv[a], "--verbose") == 0)
		{
			verbose++;
		}
		else if(strcmp(argv[a], "-c") == 0 ||
		        strcmp(argv[a], "--condition") == 0)
		{
			cond = 1;
		}
		else if(strcmp(argv[a], "-f") == 0 ||
		        strcmp(argv[a], "--force") == 0)
		{
			force = 1;
		}
		else if(strcmp(argv[a], "-l") == 0 ||
		        strcmp(argv[a], "--legacydir") == 0)
		{
			dirVersion = 0;
		}
		else if(strcmp(argv[a], "-n") == 0 ||
		        strcmp(argv[a], "--newdir") == 0)
		{
			dirVersion = 0;
		}
		else if(argv[a][0] == '-')
		{
			fprintf(stderr, "Unknown option %s provided\n", argv[a]);
			return 0;
		}
		else
		{
			if(vsn[0])
			{
				fprintf(stderr, "Error: two VSNs provided : %s and %s\n",
					vsn, argv[a]);
				return 0;
			}
			strncpy(vsn, argv[a], 10);
			vsn[9] = 0;
			if(strlen(vsn) != 8)
			{
				fprintf(stderr, "Error: VSN %s not 8 chars long!\n", argv[a]);
				return 0;
			}
		}
	}

	if(vsn[0] == 0)
	{
		printf("Error: no VSN provided!\n");
		return 0;
	}

	printf("About to proceed.  verbose=%d cond=%d force=%d vsn=%s\n",
		verbose, cond, force, vsn);

	if(!force)
	{
		if(cond)
		{
			printf("\nAbout to condition module %s.  This will destroy all\n", vsn);
			printf("contents of the module and will take a long time.  Are\n");
			printf("you sure you want to continue? [y|n]\n");
		}
		else
		{
			printf("\nAbout to erase module %s.  This will destroy all\n", vsn);
			printf("contents of the module.  Are you sure you want to\n");
			printf("continue? [y|n]\n");
		}
		for(;;)
		{
			rv = fgets(resp, 10, stdin);
			if(strcmp(resp, "Y\n") == 0 || strcmp(resp, "y\n") == 0)
			{
				if(verbose)
				{
					printf("OK -- continuing\n\n");
				}
				break;
			}
			else if(strcmp(resp, "N\n") == 0 || strcmp(resp, "n\n") == 0)
			{
				printf("OK -- I won't continue.\n\n");
				return 0;
			}
			else
			{
				printf("Your response was not understood.\n");
				printf("Continue? [y|n]\n");
			}
		}
	}

	v = initWatchdog();
	if(v < 0)
	{
		fprintf(stderr, "Error: Cannot start watchdog!\n");
		return -1;
	}

	oldsiginthand = signal(SIGINT, siginthand);

	/* 60 seconds should be enough to complete any XLR command */
	setWatchdogTimeout(60);

	difxMessageInit(-1, program);

	/* *********** */

	v = mk5erase(vsn, cond, verbose, dirVersion);
	if(v < 0)
	{
		if(watchdogXLRError[0] != 0)
		{
			char message[DIFX_MESSAGE_LENGTH];
			snprintf(message, DIFX_MESSAGE_LENGTH, 
				"Streamstor error executing: %s : %s",
				watchdogStatement, watchdogXLRError);
			difxMessageSendDifxAlert(message, DIFX_ALERT_LEVEL_ERROR);
		}
	}

	/* *********** */

	stopWatchdog();

	return 0;
}
