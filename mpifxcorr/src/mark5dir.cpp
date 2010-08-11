/***************************************************************************
 *   Copyright (C) 2009, 2010 by Walter Brisken                            *
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
/*===========================================================================
 * SVN properties (DO NOT CHANGE)
 *
 * $Id$
 * $HeadURL$
 * $LastChangedRevision$
 * $Author$
 * $LastChangedDate$
 *
 *==========================================================================*/

#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mark5access.h>
#include "mark5dir.h"
#include "../config.h"

#ifdef WORDS_BIGENDIAN
#define MARK5_FILL_WORD32 0x44332211UL
#define MARK5_FILL_WORD64 0x4433221144332211ULL
#else
#define MARK5_FILL_WORD32 0x11223344UL
#define MARK5_FILL_WORD64 0x1122334411223344ULL
#endif


using namespace std;

char Mark5DirDescription[][20] =
{
	"Short scan",
	"XLR Read error",
	"Decode error",
	"Decoded",
	"Decoded WR"
};

void countReplaced(const streamstordatatype *data, int len, 
	long long *wGood, long long *wBad)
{
	int nBad=0;

	for(int i = 0; i < len; i++)
	{
		if(data[i] == MARK5_FILL_WORD32)
		{
			nBad++;
		}
	}

	*wGood += (len-nBad);
	*wBad += nBad;
}

/* returns active bank, or -1 if none */
int Mark5BankGet(SSHANDLE *xlrDevice)
{
	S_BANKSTATUS bank_stat;
	XLR_RETURN_CODE xlrRC;
	int b = -1;

	xlrRC = XLRGetBankStatus(*xlrDevice, BANK_A, &bank_stat);
	if(xlrRC == XLR_SUCCESS)
	{
		if(bank_stat.Selected)
		{
			b = 0;
		}
	}
	if(b == -1)
	{
		xlrRC = XLRGetBankStatus(*xlrDevice, BANK_B, &bank_stat);
		if(xlrRC == XLR_SUCCESS)
		{
			if(bank_stat.Selected)
			{
				b = 1;
			}
		}
	}

	return b;
}

/* returns 0 or 1 for bank A or B, or < 0 if module not found or on error */
int Mark5BankSetByVSN(SSHANDLE *xlrDevice, const char *vsn)
{
	S_BANKSTATUS bank_stat;
	XLR_RETURN_CODE xlrRC;
	S_DIR dir;
	int b = -1;
	int bank=-1;

	xlrRC = XLRGetBankStatus(*xlrDevice, BANK_A, &bank_stat);
	if(xlrRC == XLR_SUCCESS)
	{
		if(strncasecmp(bank_stat.Label, vsn, 8) == 0)
		{
			b = 0;
			bank = BANK_A;
		}
	}

	if(b == -1)
	{
		xlrRC = XLRGetBankStatus(*xlrDevice, BANK_B, &bank_stat);
		if(xlrRC == XLR_SUCCESS)
		{
			if(strncasecmp(bank_stat.Label, vsn, 8) == 0)
			{
				b = 1;
				bank = BANK_B;
			}
		}
	}

	if(bank < 0)
	{
		return -1;
	}

	xlrRC = XLRGetBankStatus(*xlrDevice, bank, &bank_stat);
	if(xlrRC != XLR_SUCCESS)
	{
		return -4;
	}
	if(!bank_stat.Selected) // need to change banks
	{
		xlrRC = XLRSelectBank(*xlrDevice, bank);
		if(xlrRC != XLR_SUCCESS)
		{
			b = -2 - b;
		}
		else
		{
			for(int i = 0; i < 100; i++)
			{
				xlrRC = XLRGetBankStatus(*xlrDevice, bank, &bank_stat);
				if(xlrRC != XLR_SUCCESS)
				{
					return -4;
				}
				if(bank_stat.State == STATE_READY && bank_stat.Selected)
				{
					break;
				}
				usleep(100000);
			}

			if(bank_stat.State != STATE_READY || !bank_stat.Selected)
			{
				b = -4;
			}
		}
	}

	xlrRC = XLRGetDirectory(*xlrDevice, &dir);
	if(xlrRC != XLR_SUCCESS)
	{
		return -6;
	}

	return b;
}

static int uniquifyScanNames(struct Mark5Module *module)
{
	char scanNames[MAXSCANS][MAXLENGTH];
	int nameCount[MAXSCANS];
	int origIndex[MAXSCANS];
	int i, j, n=0;
	char tmpStr[MAXLENGTH+5];

	if(!module)
	{
		return -1;
	}

	if(module->nscans < 2)
	{
		return 0;
	}

	strcpy(scanNames[0], module->scans[0].name);
	nameCount[0] = 1;
	origIndex[0] = 0;
	n = 1;

	for(i = 1; i < module->nscans; i++)
	{
		for(j = 0; j < n; j++)
		{
			if(strcmp(scanNames[j], module->scans[i].name) == 0)
			{
				nameCount[j]++;
				sprintf(tmpStr, "%s_%04d", scanNames[j], nameCount[j]);
				strncpy(module->scans[i].name, tmpStr, MAXLENGTH-1);
				module->scans[i].name[MAXLENGTH-1] = 0;
				break;
			}
		}
		if(j == n)
		{
			strcpy(scanNames[n], module->scans[i].name);
			nameCount[n] = 1;
			origIndex[n] = i;
			n++;
		}
	}

	/* rename those that would have had name extension _0001 */
	for(j = 0; j < n; j++)
	{
		if(nameCount[j] > 1)
		{
			i = origIndex[j];
			sprintf(tmpStr, "%s_%04d", scanNames[j], 1);
			strncpy(module->scans[i].name, tmpStr, MAXLENGTH-1);
			module->scans[i].name[MAXLENGTH-1] = 0;
		}
	}

	return 0;
}

static int getMark5Module(struct Mark5Module *module, SSHANDLE *xlrDevice, int mjdref, 
	int (*callback)(int, int, int, void *), void *data, float *replacedFrac)
{
	XLR_RETURN_CODE xlrRC;
	Mark5Directory *m5dir;
	unsigned char *dirData;
	int len, n;
	int j;
	struct mark5_format *mf;
	Mark5Scan *scan;
	char label[XLR_LABEL_LENGTH];
	int bank;
	unsigned long a, b;
	int bufferlen;
	unsigned int x, signature;
	int die = 0;
	long long wGood=0, wBad=0;
	long long wGoodSum=0, wBadSum=0;
	int dirVersion;		/* == 0 for old style (pre-mark5-memo 81) */
				/* == version number for mark5-memo 81 */
	int oldLen1, oldLen2, oldLen3;
	int start, stop;

	streamstordatatype *buffer;

	/* allocate a bit more than the minimum needed */
	bufferlen = 20160*8*10;

	bank = Mark5BankGet(xlrDevice);
	if(bank < 0)
	{
		return -1;
	}

	xlrRC = XLRGetLabel(*xlrDevice, label);
	if(xlrRC != XLR_SUCCESS)
	{
		return -2;
	}
	label[8] = 0;

	len = XLRGetUserDirLength(*xlrDevice);
	/* The historic directories written by Mark5A could come in three sizes.
	 * See readdir() in Mark5A.c.  If one of these matches the actual dir size,
	 * then assume it is old style, which we declare to be directory version
	 * 0.  Otherwise check for divisibility by 128.  If so, then it is considered 
	 * new style, and the version number can be extracted from the header.
	 */
	oldLen1 = (int)sizeof(struct Mark5Directory);
	oldLen2 = oldLen1 + 64 + 8*88;	/* 88 = sizeof(S_DRIVEINFO) */
	oldLen3 = oldLen1 + 64 + 16*88;
	if(len == oldLen1 || len == oldLen2 || len == oldLen3)
	{
		dirVersion = 0;
	}
	else if(len % 128 == 0)
	{
		dirVersion = -1;  /* signal to get version number later */
	}
	else
	{
		printf("size=%d  len=%d\n", static_cast<int>(sizeof(struct Mark5Directory)), len);

		return -3;
	}

	dirData = (unsigned char *)calloc(len, sizeof(int));
	if(dirData == 0)
	{
		return -4;
	}
	m5dir = (struct Mark5Directory *)dirData;

	xlrRC = XLRGetUserDir(*xlrDevice, len, 0, dirData);
	if(xlrRC != XLR_SUCCESS)
	{
		free(dirData);
		return -5;
	}
	if(dirVersion == -1)
	{
		dirVersion = ((int *)dirData)[0];
	}
	
	if(dirVersion < 0 || dirVersion > 1)
	{
		return -6;
	}

	/* the adventurous would use md5 here */
	if(dirVersion == 0)
	{
		start = 0;
		stop = 81952;
	}
	else
	{
		/* Don't base directory on header material as that can change */
		start = sizeof(struct Mark5DirectoryHeaderVer1);
		stop = len;
	}
	signature = 1;

	if(start < stop)
	{
		for(j = start/4; j < stop/4; j++)
		{
			x = ((unsigned int *)dirData)[j] + 1;
			signature = signature ^ x;
		}

		/* prevent a zero signature */
		if(signature == 0)
		{
			signature = 0x55555555;
		}
	}

	if(module->signature == signature && module->nscans > 0)
	{
		module->bank = bank;
		free(dirData);
		return 0;
	}
	else
	{
		return -1;
	}
#if 0
	buffer = (streamstordatatype *)malloc(bufferlen);
	
	memset(module, 0, sizeof(struct Mark5Module));
	if(dirVersion == 0)
	{
		module->nscans = m5dir->nscans;
	}
	else
	{
		module->nscans = len/128 - 1;
	}
	module->bank = bank;
	strcpy(module->label, label);
	module->signature = signature;
	module->dirVersion = dirVersion;
	module->mode = MARK5_READ_MODE_NORMAL;

	for(int i = 0; i < module->nscans; i++)
	{
		wGood = wBad = 0;
		scan = module->scans + i;

		if(dirVersion == 0)
		{
			strncpy(scan->name, m5dir->scanName[i], MAXLENGTH);
			scan->start  = m5dir->start[i];
			scan->length = m5dir->length[i];
		}
		else if(dirVersion == 1)
		{
			const struct Mark5DirectoryScanHeaderVer1 *sh;
			sh = (struct Mark5DirectoryScanHeaderVer1 *)(dirData + 128*(i+1));
			strncpy(scan->name, sh->scanName, 32);
			scan->name[31] = 0;
			scan->start  = sh->startByte;
			scan->length = sh->stopByte - sh->startByte;
		}
		if(scan->length < bufferlen)
		{
			if(callback)
			{
				die = callback(i, module->nscans, MARK5_DIR_SHORT_SCAN, data);
			}
			continue;
		}

		if(die)
		{
			break;
		}

		if(scan->start & 4)
		{
			scan->start -= 4;
			scan->length -= 4;
		}

		a = scan->start>>32;
		b = scan->start % (1LL<<32);

		xlrRC = XLRReadData(*xlrDevice, buffer, a, b, bufferlen);

		if(xlrRC == XLR_FAIL)
		{
			if(callback)
			{
				die = callback(i, module->nscans, MARK5_DIR_READ_ERROR, data);
			}
			scan->format = -2;

			continue;
		}

		countReplaced(buffer, bufferlen/4, &wGood, &wBad);

		if(die)
		{
			break;
		}

		mf = new_mark5_format_from_stream(new_mark5_stream_memory(buffer, bufferlen));
	
		if(!mf)
		{
			if(callback)
			{
				die = callback(i, module->nscans, MARK5_DIR_DECODE_ERROR, data);
			}
			scan->format = -1;
			continue;
		}
		
		if(die)
		{
			break;
		}

		/* Fix mjd.  FIXME: this should be done in mark5access */
		if(mf->format == 0 || mf->format == 2)  /* VLBA or Mark5B format */
		{
			n = (mjdref - mf->mjd + 500) / 1000;
			mf->mjd += n*1000;
		}
		else if(mf->format == 1)	/* Mark4 format */
		{
			n = (int)((mjdref - mf->mjd + 1826)/3652.4);
			mf->mjd = addDecades(mf->mjd, n);
		}
		
		scan->mjd = mf->mjd;
		scan->sec = mf->sec;
		scan->format      = mf->format;
		scan->frameoffset = mf->frameoffset;
		scan->tracks      = mf->ntrack;
		scan->framespersecond = int(1000000000.0/mf->framens + 0.5);
		scan->framenuminsecond = int(mf->ns/mf->framens + 0.5);
		scan->framebytes  = mf->framebytes;
		scan->duration    = (int)((scan->length - scan->frameoffset)
			/ scan->framebytes)/(double)(scan->framespersecond);
		
		delete_mark5_format(mf);

		if(callback)
		{
			enum Mark5DirStatus s;

			if(wBad > 8)
			{
				s = MARK5_DIR_DECODE_WITH_REPLACEMENTS;
			}
			else
			{
				s = MARK5_DIR_DECODE_SUCCESS;
			}
			die = callback(i, module->nscans, s, data);
		}

		wGoodSum += wGood;
		wBadSum += wBad;

		if(die)
		{
			break;
		}
	}

	if(replacedFrac)
	{
		*replacedFrac = (double)wBad/(double)wGood;
	}

	free(buffer);
	free(dirData);

	uniquifyScanNames(module);

	return -die;
#endif
}

void printMark5Module(const struct Mark5Module *module)
{
	int n;
	const Mark5Scan *scan;

	if(!module)
	{
		return;
	}
	if(module->bank < 0)
	{
		return;
	}
	
	printf("Module Name = %s   Nscans = %d   Bank = %c  Sig = %u\n", 
		module->label, module->nscans, module->bank+'A', 
		module->signature);

	n = module->nscans;
	for(int i = 0; i < n; i++)
	{
		scan = module->scans + i;
	
		printf("%3d %1d %-32s %13Ld %13Ld %5d %2d %5d %5d+%d/%d %6.4f\n",
			i+1,
			scan->format,
			scan->name,
			scan->start,
			scan->start+scan->length,
			scan->frameoffset,
			scan->tracks,
			scan->mjd,
			scan->sec,
			scan->framenuminsecond,
			scan->framespersecond,
			scan->duration);
	}
}

int loadMark5Module(struct Mark5Module *module, const char *filename)
{
	FILE *in;
	struct Mark5Scan *scan;
	char line[256];
	int nscans, n;
	char bank;
	char label[XLR_LABEL_LENGTH];
	unsigned int signature;
	char extra1[12], extra2[12];

	if(!module)
	{
		return -1;
	}

	module->label[0] = 0;
	module->nscans = 0;
	module->bank = -1;
	module->needRealtimeMode = false;

	in = fopen(filename, "r");
	if(!in)
	{
		return -1;
	}

	fgets(line, 255, in);
	if(feof(in))
	{
		fclose(in);
		return -1;
	}

	n = sscanf(line, "%8s %d %c %u %10s %10s", label, &nscans, &bank, &signature, extra1, extra2);
	if(n < 3)
	{
		fclose(in);
		return -1;
	}
	else if(n == 3)
	{
		signature = ~0;
	}
	if(n == 5)
	{
		if(strcmp(extra1, "RT") == 0)
		{
			module->needRealtimeMode = true;
		}
	}
	else if(n == 6)
	{
		if(strcmp(extra2, "RT") == 0)
		{
			module->needRealtimeMode = true;
		}
	}

	if(nscans > MAXSCANS || nscans < 0)
	{
		fclose(in);
		return -1;
	}

	strcpy(module->label, label);
	module->nscans = nscans;
	module->bank = bank-'A';
	module->signature = signature;

	for(int i = 0; i < nscans; i++)
	{
		scan = module->scans + i;

		fgets(line, 255, in);
		if(feof(in))
		{
			module->nscans = i;
			fclose(in);
			return -1;
		}
		
		sscanf(line, "%Ld%Ld%d%d%d%d%lf%d%d%d%d%63s",
			&scan->start, 
			&scan->length,
			&scan->mjd,
			&scan->sec,
			&scan->framenuminsecond,
			&scan->framespersecond,
			&scan->duration,
			&scan->framebytes,
			&scan->frameoffset,
			&scan->tracks,
			&scan->format,
			scan->name);
	}

	fclose(in);
	
	return 0;
}

int saveMark5Module(struct Mark5Module *module, const char *filename)
{
	FILE *out;
	struct Mark5Scan *scan;
	char mode[12] = "";
	
	if(!module)
	{
		return -1;
	}

	out = fopen(filename, "w");
	if(!out)
	{
		return -1;
	}

	if(module->needRealtimeMode)
	{
		strcpy(mode, "RT");
	}

	fprintf(out, "%8s %d %c %u %s\n",
		module->label,
		module->nscans,
		module->bank+'A',
		module->signature,
		mode);
	for(int i = 0; i < module->nscans; i++)
	{
		scan = module->scans + i;
		
		fprintf(out, "%14Ld %14Ld %5d %d %d %d %12.6f %6d %6d %2d %1d %s\n",
			scan->start, 
			scan->length,
			scan->mjd,
			scan->sec,
			scan->framenuminsecond,
			scan->framespersecond,
			scan->duration,
			scan->framebytes,
			scan->frameoffset,
			scan->tracks,
			scan->format,
			scan->name);
	}

	fclose(out);

	return 0;
}

/* retrieves directory (either from cache or module) and makes sure
 * desired module is the active one.  On any failure return < 0 
 */
int getCachedMark5Module(struct Mark5Module *module, SSHANDLE *xlrDevice, 
	int mjdref, const char *vsn, const char *dir,
	int (*callback)(int, int, int, void *), void *data,
	float *replacedFrac)
{
	char filename[256];
	int v, curbank;

	curbank = Mark5BankSetByVSN(xlrDevice, vsn);
	if(curbank < 0)
	{
		return -1;
	}
	
	sprintf(filename, "%s/%s.dir", dir, vsn);
	
	v = loadMark5Module(module, filename);

	v = getMark5Module(module, xlrDevice, mjdref, callback, data, replacedFrac);

	if(v >= 0)
	{
		saveMark5Module(module, filename);
	}

	if(module->needRealtimeMode)
	{
		XLRSetOption(*xlrDevice, SS_OPT_REALTIMEPLAYBACK | SS_OPT_SKIPCHECKDIR);
	}

	return v;
}

int sanityCheckModule(const struct Mark5Module *module)
{
	for(int i = 0; i < module->nscans; i++)
	{
		if(module->scans[i].format < 0)
		{
			return -1;
		}
	}

	return 0;
}
