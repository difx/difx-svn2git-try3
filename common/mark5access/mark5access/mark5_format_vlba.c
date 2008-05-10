/***************************************************************************
 *   Copyright (C) 2006, 2007 by Walter Brisken                            *
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "mark5access/mark5_stream.h"

#define PAYLOADSIZE 20000

/* the high mag value for 2-bit reconstruction */
static const float HiMag = 3.3359;

static uint32_t *modulate = 0;

static float lut1bit[2][256][8];  /* For all 1-bit modes */
static float lut2bit1[2][256][4]; /* fanout 1 @ 8/16t, fanout 4 @ 32/64t ! */
static float lut2bit2[2][256][4]; /* fanout 2 @ 8/16t, fanout 1 @ 32/64t   */
static float lut2bit3[2][256][4]; /* fanout 4 @ 8/16t, fanout 2 @ 32/64t   */
static float zeros[8];

/* ! 2bit/fanout4 use the following in decoding 32 and 64 track data: */

#ifdef WORDS_BIGENDIAN

#define reorder32(x) ((((x) & 0x55AA55AAUL)) | \
                      (((x) & 0xAA00AA00UL) >> 9) | \
		      (((x) & 0x00550055UL) << 9))
#define reorder64(x) ((((x) & 0x55AA55AA55AA55AAULL)) | \
                      (((x) & 0xAA00AA00AA00AA00ULL) >> 9) | \
		      (((x) & 0x0055005500550055ULL) << 9))

#else

#define reorder32(x) ((((x) & 0xAA55AA55UL)) | \
                      (((x) & 0x55005500UL) >> 7) | \
		      (((x) & 0x00AA00AAUL) << 7))
#define reorder64(x) ((((x) & 0xAA55AA55AA55AA55ULL)) | \
                      (((x) & 0x5500550055005500ULL) >> 7) | \
		      (((x) & 0x00AA00AA00AA00AAULL) << 7))

#endif

struct mark5_format_vlba
{
	int ntrack;
	int fanout;
	int kday;	/* kilo-mjd days.  51000, 52000, ... */
};

static void initluts()
{
	int b, i, s, m, l;
	const float lut2level[2] = {1.0, -1.0};
	const float lut4level[4] = {-HiMag, 1.0, -1.0, HiMag};

	for(i = 0; i < 8; i++)
	{
		zeros[i] = 0.0;
	}

	for(b = 0; b < 256; b++)
	{
		/* lut1bit */
		for(i = 0; i < 8; i++)
		{
			l = (b>>i)&1;
			lut1bit[0][b][i] =  lut2level[l];
			lut1bit[1][b][i] = -lut2level[l];
		}

		/* lut2bit1 */
		for(i = 0; i < 4; i++)
		{
			s = i*2;	/* 0, 2, 4, 6 */
			m = s+1;	/* 1, 3, 5, 7 */
			l = ((b>>s)&1) + (((b>>m)&1)<<1);
			lut2bit1[0][b][i] =  lut4level[l];
			lut2bit1[1][b][i] = -lut4level[l];
		}

		/* lut2bit2 */
		for(i = 0; i < 4; i++)
		{
			s = i+(i/2)*2;	/* 0, 1, 4, 5 */
			m = s+2;	/* 2, 3, 6, 7 */
			l = ((b>>s)&1) + (((b>>m)&1)<<1);
			lut2bit2[0][b][i] =  lut4level[l];
			lut2bit2[1][b][i] = -lut4level[l];
		}

		/* lut2bit3 */
		for(i = 0; i < 4; i++)
		{
			s = i;		/* 0, 1, 2, 3 */
			m = s+4;	/* 4, 5, 6, 7 */
			l = ((b>>s)&1) + (((b>>m)&1)<<1);
			lut2bit3[0][b][i] =  lut4level[l];
			lut2bit3[1][b][i] = -lut4level[l];
		}
	}
}

static void initmodulate()
{
	int i, n, k;
	unsigned int ff[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
	
	if(!modulate) 
	{
		modulate = (uint32_t *)calloc(PAYLOADSIZE, sizeof(uint32_t));
	}
	
	for(i = 0; i < PAYLOADSIZE; i++)
	{
		k = ff[10] ^ ff[12] ^ ff[13] ^ ff[15];
		for(n = 15; n > 0; n--) ff[n] = ff[n-1];
		ff[0] = k;
		modulate[i] = k;
		if(i % 8 == 7) /* Skip the parity bit */
		{
			k = ff[10] ^ ff[12] ^ ff[13] ^ ff[15];
			for(n = 15; n > 0; n--) ff[n] = ff[n-1];
			ff[0] = k;
		}
	}
}

int countbits(uint8_t v)
{
	unsigned int c; // c accumulates the total bits set in v
	for (c = 0; v; c++)
	{
		v &= v - 1; // clear the least significant bit set
	}
	return c;
}

/* Look for the first occurance (lowest offset >= 0) of the following pattern:
 *
 * 32*tracks bits set at offset bytes
 * 32*tracks bits set at offset+2520*tracks bytes
 * 2*tracks bits unset at offset+2518*tracks bytes
 *
 * With up to tol bits not set correctly
 *
 * return offset;
 */
static int findfirstframe(const uint8_t *data, int bytes, int tracks, int tol)
{
	int offset;
	int wrong = 0;
	int i, a, b;

	if(bytes < 2600*tracks)
	{
		return -1;
	}
	
	bytes -= 2600*tracks;

	b = tracks*2520;
	a = b - tracks/4;

	for(i = 0; i < 4*tracks; i++)
	{
		wrong += countbits(~data[i]);
		wrong += countbits(~data[i+b]);
	}
	for(i = 0; i < tracks/4; i++)
	{
		wrong += countbits(data[i+a]);
	}

	for(offset = 0; offset < bytes; offset++)
	{
		if(wrong < tol)
		{
			return offset;
		}
		wrong -= countbits(~data[offset]);
		wrong += countbits(~data[offset+4*tracks]);
		wrong -= countbits(~data[offset+b]);
		wrong += countbits(~data[offset+b+4*tracks]);
		wrong -= countbits(data[offset+a]);
		wrong += countbits(data[offset+a+tracks/4]);
	}

	return -1;
}

/* look at encoded nibbles.  Count bits in each track, assume set if
 * more than half are
 */
static void extractnibbles(const uint8_t *data, int ntracks, int numnibbles, 
	char *nibbles)
{
	int i, j, b, n, c;

	n = ntracks/8;

	for(i = 0; i < numnibbles; i++)
	{
		nibbles[i] = 0;
		for(b = 0; b < 4; b++)
		{
			c = 0;
			for(j = 0; j < n; j++)
			{
				c += countbits(data[n*(4*i+3-b)+j]);
			}
			nibbles[i] += (c > n/2) ? (1 << b) : 0;
		}
	}
}

static int mark5_stream_frame_time_vlba(const struct mark5_stream *ms, 
	int *mjd, int *sec, double *ns)
{
	char nibs[12];
	struct mark5_format_vlba *v;
	int nRealTrack;

	if(!ms)
	{
		return -1;
	}
	v = (struct mark5_format_vlba *)(ms->formatdata);

	nRealTrack = v->ntrack;
	if(nRealTrack < 8)
	{
		nRealTrack = 8;
	}

	extractnibbles(ms->frame + 4*nRealTrack, nRealTrack, 12, nibs);

	if(mjd)
	{
		*mjd = v->kday + nibs[0]*100 + nibs[1]*10 + nibs[2];
	}
	if(sec) 
	{
		*sec = nibs[3]*10000 + nibs[4]*1000 + nibs[5]*100 
			+ nibs[6]*10 + nibs[7];
	}
	if(ns)
	{
		*ns = nibs[8]*100000000 + nibs[9]*10000000 
			+ nibs[10]*1000000 + nibs[11]*100000;
	}

	return 0;
}

static int mark5_format_vlba_fixmjd(struct mark5_stream *ms, int refmjd)
{
	struct mark5_format_vlba *v;
	int n;

	if(!ms)
	{
		return -1;
	}

	v = (struct mark5_format_vlba *)(ms->formatdata);
	if(v->kday == 0)
	{
		n = (refmjd - ms->mjd + 500) / 1000;
		ms->mjd += n*1000;
		v->kday = n*1000;

		return 1;
	}

	return 0;
}

/*********************** data unpack routines **********************/

static int vlba_decode_1bit_1track_fanout1_oversamp1(struct mark5_stream *ms,
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_1track_fanout1_oversamp2(struct mark5_stream *ms,
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i+=2;

		data[0][o] = fp[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_1track_fanout1_oversamp4(struct mark5_stream *ms,
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i+=4;

		data[0][o] = fp[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_2track_fanout1_oversamp1(struct mark5_stream *ms,
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[1];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_2track_fanout1_oversamp2(struct mark5_stream *ms,
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i+=2;

		data[0][o] = fp[0];
		data[1][o] = fp[1];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_2track_fanout1_oversamp4(struct mark5_stream *ms,
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i+=4;

		data[0][o] = fp[0];
		data[1][o] = fp[1];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_2track_fanout2_oversamp1(struct mark5_stream *ms,
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		o++;
		data[0][o] = fp[1];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_2track_fanout2_oversamp2(struct mark5_stream *ms,
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_2track_fanout2_oversamp4(struct mark5_stream *ms,
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i+=2;

		data[0][o] = fp[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_4track_fanout1_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[1];
		data[2][o] = fp[2];
		data[3][o] = fp[3];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_4track_fanout1_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i+=2;

		data[0][o] = fp[0];
		data[1][o] = fp[1];
		data[2][o] = fp[2];
		data[3][o] = fp[3];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_4track_fanout1_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i+=4;

		data[0][o] = fp[0];
		data[1][o] = fp[1];
		data[2][o] = fp[2];
		data[3][o] = fp[3];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_4track_fanout2_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[2];
		o++;
		data[0][o] = fp[1];
		data[1][o] = fp[3];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_4track_fanout2_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[2];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_4track_fanout2_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i+=2;

		data[0][o] = fp[0];
		data[1][o] = fp[2];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_4track_fanout4_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		o++;
		data[0][o] = fp[1];
		o++;
		data[0][o] = fp[2];
		o++;
		data[0][o] = fp[3];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_4track_fanout4_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		o++;
		data[0][o] = fp[2];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_4track_fanout4_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_8track_fanout1_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[1];
		data[2][o] = fp[2];
		data[3][o] = fp[3];
		data[4][o] = fp[4];
		data[5][o] = fp[5];
		data[6][o] = fp[6];
		data[7][o] = fp[7];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_8track_fanout1_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i+=2;

		data[0][o] = fp[0];
		data[1][o] = fp[1];
		data[2][o] = fp[2];
		data[3][o] = fp[3];
		data[4][o] = fp[4];
		data[5][o] = fp[5];
		data[6][o] = fp[6];
		data[7][o] = fp[7];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_8track_fanout1_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i+=4;

		data[0][o] = fp[0];
		data[1][o] = fp[1];
		data[2][o] = fp[2];
		data[3][o] = fp[3];
		data[4][o] = fp[4];
		data[5][o] = fp[5];
		data[6][o] = fp[6];
		data[7][o] = fp[7];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_8track_fanout2_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[2];
		data[2][o] = fp[4];
		data[3][o] = fp[6];
		o++;
		data[0][o] = fp[1];
		data[1][o] = fp[3];
		data[2][o] = fp[5];
		data[3][o] = fp[7];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_8track_fanout2_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[2];
		data[2][o] = fp[4];
		data[3][o] = fp[6];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_8track_fanout2_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i+=2;

		data[0][o] = fp[0];
		data[1][o] = fp[2];
		data[2][o] = fp[4];
		data[3][o] = fp[6];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_8track_fanout4_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[4];
		o++;
		data[0][o] = fp[1];
		data[1][o] = fp[5];
		o++;
		data[0][o] = fp[2];
		data[1][o] = fp[6];
		o++;
		data[0][o] = fp[3];
		data[1][o] = fp[7];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_8track_fanout4_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[4];
		o++;
		data[0][o] = fp[2];
		data[1][o] = fp[6];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_8track_fanout4_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut1bit[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[4];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_16track_fanout1_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 2;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[1];
		data[2][o]  = fp0[2];
		data[3][o]  = fp0[3];
		data[4][o]  = fp0[4];
		data[5][o]  = fp0[5];
		data[6][o]  = fp0[6];
		data[7][o]  = fp0[7];
		data[8][o]  = fp1[0];
		data[9][o]  = fp1[1];
		data[10][o] = fp1[2];
		data[11][o] = fp1[3];
		data[12][o] = fp1[4];
		data[13][o] = fp1[5];
		data[14][o] = fp1[6];
		data[15][o] = fp1[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_16track_fanout1_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 4;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i+=3;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[1];
		data[2][o]  = fp0[2];
		data[3][o]  = fp0[3];
		data[4][o]  = fp0[4];
		data[5][o]  = fp0[5];
		data[6][o]  = fp0[6];
		data[7][o]  = fp0[7];
		data[8][o]  = fp1[0];
		data[9][o]  = fp1[1];
		data[10][o] = fp1[2];
		data[11][o] = fp1[3];
		data[12][o] = fp1[4];
		data[13][o] = fp1[5];
		data[14][o] = fp1[6];
		data[15][o] = fp1[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_16track_fanout1_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i+=7;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[1];
		data[2][o]  = fp0[2];
		data[3][o]  = fp0[3];
		data[4][o]  = fp0[4];
		data[5][o]  = fp0[5];
		data[6][o]  = fp0[6];
		data[7][o]  = fp0[7];
		data[8][o]  = fp1[0];
		data[9][o]  = fp1[1];
		data[10][o] = fp1[2];
		data[11][o] = fp1[3];
		data[12][o] = fp1[4];
		data[13][o] = fp1[5];
		data[14][o] = fp1[6];
		data[15][o] = fp1[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_16track_fanout2_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 2;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;
		
		data[0][o] = fp0[0];
		data[1][o] = fp0[2];
		data[2][o] = fp0[4];
		data[3][o] = fp0[6];
		data[4][o] = fp1[0];
		data[5][o] = fp1[2];
		data[6][o] = fp1[4];
		data[7][o] = fp1[6];
		o++;
		data[0][o] = fp0[1];
		data[1][o] = fp0[3];
		data[2][o] = fp0[5];
		data[3][o] = fp0[7];
		data[4][o] = fp1[1];
		data[5][o] = fp1[3];
		data[6][o] = fp1[5];
		data[7][o] = fp1[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_16track_fanout2_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 2;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;
		
		data[0][o] = fp0[0];
		data[1][o] = fp0[2];
		data[2][o] = fp0[4];
		data[3][o] = fp0[6];
		data[4][o] = fp1[0];
		data[5][o] = fp1[2];
		data[6][o] = fp1[4];
		data[7][o] = fp1[6];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_16track_fanout2_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 4;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i+=3;
		}
		m++;
		
		data[0][o] = fp0[0];
		data[1][o] = fp0[2];
		data[2][o] = fp0[4];
		data[3][o] = fp0[6];
		data[4][o] = fp1[0];
		data[5][o] = fp1[2];
		data[6][o] = fp1[4];
		data[7][o] = fp1[6];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_16track_fanout4_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 2;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o] = fp0[0];
		data[1][o] = fp0[4];
		data[2][o] = fp1[0];
		data[3][o] = fp1[4];
		o++;
		data[0][o] = fp0[1];
		data[1][o] = fp0[5];
		data[2][o] = fp1[1];
		data[3][o] = fp1[5];
		o++;
		data[0][o] = fp0[2];
		data[1][o] = fp0[6];
		data[2][o] = fp1[2];
		data[3][o] = fp1[6];
		o++;
		data[0][o] = fp0[3];
		data[1][o] = fp0[7];
		data[2][o] = fp1[3];
		data[3][o] = fp1[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_16track_fanout4_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 2;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o] = fp0[0];
		data[1][o] = fp0[4];
		data[2][o] = fp1[0];
		data[3][o] = fp1[4];
		o++;
		data[0][o] = fp0[2];
		data[1][o] = fp0[6];
		data[2][o] = fp1[2];
		data[3][o] = fp1[6];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_16track_fanout4_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 2;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o] = fp0[0];
		data[1][o] = fp0[4];
		data[2][o] = fp1[0];
		data[3][o] = fp1[4];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_32track_fanout1_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 4;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[2];
		data[2][o]  = fp0[4];
		data[3][o]  = fp0[6];
		data[4][o]  = fp1[0];
		data[5][o]  = fp1[2];
		data[6][o]  = fp1[4];
		data[7][o]  = fp1[6];
		data[8][o]  = fp2[0];
		data[9][o]  = fp2[2];
		data[10][o] = fp2[4];
		data[11][o] = fp2[6];
		data[12][o] = fp3[0];
		data[13][o] = fp3[2];
		data[14][o] = fp3[4];
		data[15][o] = fp3[6];
		data[16][o] = fp0[1];
		data[17][o] = fp0[3];
		data[18][o] = fp0[5];
		data[19][o] = fp0[7];
		data[20][o] = fp1[1];
		data[21][o] = fp1[3];
		data[22][o] = fp1[5];
		data[23][o] = fp1[7];
		data[24][o] = fp2[1];
		data[25][o] = fp2[3];
		data[26][o] = fp2[5];
		data[27][o] = fp2[7];
		data[28][o] = fp3[1];
		data[29][o] = fp3[3];
		data[30][o] = fp3[5];
		data[31][o] = fp3[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_32track_fanout1_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i+=5;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[2];
		data[2][o]  = fp0[4];
		data[3][o]  = fp0[6];
		data[4][o]  = fp1[0];
		data[5][o]  = fp1[2];
		data[6][o]  = fp1[4];
		data[7][o]  = fp1[6];
		data[8][o]  = fp2[0];
		data[9][o]  = fp2[2];
		data[10][o] = fp2[4];
		data[11][o] = fp2[6];
		data[12][o] = fp3[0];
		data[13][o] = fp3[2];
		data[14][o] = fp3[4];
		data[15][o] = fp3[6];
		data[16][o] = fp0[1];
		data[17][o] = fp0[3];
		data[18][o] = fp0[5];
		data[19][o] = fp0[7];
		data[20][o] = fp1[1];
		data[21][o] = fp1[3];
		data[22][o] = fp1[5];
		data[23][o] = fp1[7];
		data[24][o] = fp2[1];
		data[25][o] = fp2[3];
		data[26][o] = fp2[5];
		data[27][o] = fp2[7];
		data[28][o] = fp3[1];
		data[29][o] = fp3[3];
		data[30][o] = fp3[5];
		data[31][o] = fp3[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_32track_fanout1_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 16;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i+=13;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[2];
		data[2][o]  = fp0[4];
		data[3][o]  = fp0[6];
		data[4][o]  = fp1[0];
		data[5][o]  = fp1[2];
		data[6][o]  = fp1[4];
		data[7][o]  = fp1[6];
		data[8][o]  = fp2[0];
		data[9][o]  = fp2[2];
		data[10][o] = fp2[4];
		data[11][o] = fp2[6];
		data[12][o] = fp3[0];
		data[13][o] = fp3[2];
		data[14][o] = fp3[4];
		data[15][o] = fp3[6];
		data[16][o] = fp0[1];
		data[17][o] = fp0[3];
		data[18][o] = fp0[5];
		data[19][o] = fp0[7];
		data[20][o] = fp1[1];
		data[21][o] = fp1[3];
		data[22][o] = fp1[5];
		data[23][o] = fp1[7];
		data[24][o] = fp2[1];
		data[25][o] = fp2[3];
		data[26][o] = fp2[5];
		data[27][o] = fp2[7];
		data[28][o] = fp3[1];
		data[29][o] = fp3[3];
		data[30][o] = fp3[5];
		data[31][o] = fp3[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_32track_fanout2_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 4;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[4];
		data[2][o]  = fp1[0];
		data[3][o]  = fp1[4];
		data[4][o]  = fp2[0];
		data[5][o]  = fp2[4];
		data[6][o]  = fp3[0];
		data[7][o]  = fp3[4];
		data[8][o]  = fp0[1];
		data[9][o]  = fp0[5];
		data[10][o] = fp1[1];
		data[11][o] = fp1[5];
		data[12][o] = fp2[1];
		data[13][o] = fp2[5];
		data[14][o] = fp3[1];
		data[15][o] = fp3[5];
		o++;
		data[0][o]  = fp0[2];
		data[1][o]  = fp0[6];
		data[2][o]  = fp1[2];
		data[3][o]  = fp1[6];
		data[4][o]  = fp2[2];
		data[5][o]  = fp2[6];
		data[6][o]  = fp3[2];
		data[7][o]  = fp3[6];
		data[8][o]  = fp0[3];
		data[9][o]  = fp0[7];
		data[10][o] = fp1[3];
		data[11][o] = fp1[7];
		data[12][o] = fp2[3];
		data[13][o] = fp2[7];
		data[14][o] = fp3[3];
		data[15][o] = fp3[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_32track_fanout2_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 4;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[4];
		data[2][o]  = fp1[0];
		data[3][o]  = fp1[4];
		data[4][o]  = fp2[0];
		data[5][o]  = fp2[4];
		data[6][o]  = fp3[0];
		data[7][o]  = fp3[4];
		data[8][o]  = fp0[1];
		data[9][o]  = fp0[5];
		data[10][o] = fp1[1];
		data[11][o] = fp1[5];
		data[12][o] = fp2[1];
		data[13][o] = fp2[5];
		data[14][o] = fp3[1];
		data[15][o] = fp3[5];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_32track_fanout2_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i+=5;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[4];
		data[2][o]  = fp1[0];
		data[3][o]  = fp1[4];
		data[4][o]  = fp2[0];
		data[5][o]  = fp2[4];
		data[6][o]  = fp3[0];
		data[7][o]  = fp3[4];
		data[8][o]  = fp0[1];
		data[9][o]  = fp0[5];
		data[10][o] = fp1[1];
		data[11][o] = fp1[5];
		data[12][o] = fp2[1];
		data[13][o] = fp2[5];
		data[14][o] = fp3[1];
		data[15][o] = fp3[5];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_32track_fanout4_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 4;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o] = fp0[0];
		data[1][o] = fp1[0];
		data[2][o] = fp2[0];
		data[3][o] = fp3[0];
		data[4][o] = fp0[1];
		data[5][o] = fp1[1];
		data[6][o] = fp2[1];
		data[7][o] = fp3[1];
		o++;
		data[0][o] = fp0[2];
		data[1][o] = fp1[2];
		data[2][o] = fp2[2];
		data[3][o] = fp3[2];
		data[4][o] = fp0[3];
		data[5][o] = fp1[3];
		data[6][o] = fp2[3];
		data[7][o] = fp3[3];
		o++;
		data[0][o] = fp0[4];
		data[1][o] = fp1[4];
		data[2][o] = fp2[4];
		data[3][o] = fp3[4];
		data[4][o] = fp0[5];
		data[5][o] = fp1[5];
		data[6][o] = fp2[5];
		data[7][o] = fp3[5];
		o++;
		data[0][o] = fp0[6];
		data[1][o] = fp1[6];
		data[2][o] = fp2[6];
		data[3][o] = fp3[6];
		data[4][o] = fp0[7];
		data[5][o] = fp1[7];
		data[6][o] = fp2[7];
		data[7][o] = fp3[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_32track_fanout4_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 4;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o] = fp0[0];
		data[1][o] = fp1[0];
		data[2][o] = fp2[0];
		data[3][o] = fp3[0];
		data[4][o] = fp0[1];
		data[5][o] = fp1[1];
		data[6][o] = fp2[1];
		data[7][o] = fp3[1];
		o++;
		data[0][o] = fp0[4];
		data[1][o] = fp1[4];
		data[2][o] = fp2[4];
		data[3][o] = fp3[4];
		data[4][o] = fp0[5];
		data[5][o] = fp1[5];
		data[6][o] = fp2[5];
		data[7][o] = fp3[5];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_32track_fanout4_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 4;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o] = fp0[0];
		data[1][o] = fp1[0];
		data[2][o] = fp2[0];
		data[3][o] = fp3[0];
		data[4][o] = fp0[1];
		data[5][o] = fp1[1];
		data[6][o] = fp2[1];
		data[7][o] = fp3[1];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_64track_fanout1_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp4 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp5 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp6 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp7 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[2];
		data[2][o]  = fp0[4];
		data[3][o]  = fp0[6];
		data[4][o]  = fp1[0];
		data[5][o]  = fp1[2];
		data[6][o]  = fp1[4];
		data[7][o]  = fp1[6];
		data[8][o]  = fp2[0];
		data[9][o]  = fp2[2];
		data[10][o] = fp2[4];
		data[11][o] = fp2[6];
		data[12][o] = fp3[0];
		data[13][o] = fp3[2];
		data[14][o] = fp3[4];
		data[15][o] = fp3[6];
		data[16][o] = fp0[1];
		data[17][o] = fp0[3];
		data[18][o] = fp0[5];
		data[19][o] = fp0[7];
		data[20][o] = fp1[1];
		data[21][o] = fp1[3];
		data[22][o] = fp1[5];
		data[23][o] = fp1[7];
		data[24][o] = fp2[1];
		data[25][o] = fp2[3];
		data[26][o] = fp2[5];
		data[27][o] = fp2[7];
		data[28][o] = fp3[1];
		data[29][o] = fp3[3];
		data[30][o] = fp3[5];
		data[31][o] = fp3[7];
		data[32][o] = fp4[0];
		data[33][o] = fp4[2];
		data[34][o] = fp4[4];
		data[35][o] = fp4[6];
		data[36][o] = fp5[0];
		data[37][o] = fp5[2];
		data[38][o] = fp5[4];
		data[39][o] = fp5[6];
		data[40][o] = fp6[0];
		data[41][o] = fp6[2];
		data[42][o] = fp6[4];
		data[43][o] = fp6[6];
		data[44][o] = fp7[0];
		data[45][o] = fp7[2];
		data[46][o] = fp7[4];
		data[47][o] = fp7[6];
		data[48][o] = fp4[1];
		data[49][o] = fp4[3];
		data[50][o] = fp4[5];
		data[51][o] = fp4[7];
		data[52][o] = fp5[1];
		data[53][o] = fp5[3];
		data[54][o] = fp5[5];
		data[55][o] = fp5[7];
		data[56][o] = fp6[1];
		data[57][o] = fp6[3];
		data[58][o] = fp6[5];
		data[59][o] = fp6[7];
		data[60][o] = fp7[1];
		data[61][o] = fp7[3];
		data[62][o] = fp7[5];
		data[63][o] = fp7[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_64track_fanout1_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 16;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp4 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp5 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp6 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp7 = lut1bit[modulate[m]][buf[i]];
			i+=9;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[2];
		data[2][o]  = fp0[4];
		data[3][o]  = fp0[6];
		data[4][o]  = fp1[0];
		data[5][o]  = fp1[2];
		data[6][o]  = fp1[4];
		data[7][o]  = fp1[6];
		data[8][o]  = fp2[0];
		data[9][o]  = fp2[2];
		data[10][o] = fp2[4];
		data[11][o] = fp2[6];
		data[12][o] = fp3[0];
		data[13][o] = fp3[2];
		data[14][o] = fp3[4];
		data[15][o] = fp3[6];
		data[16][o] = fp0[1];
		data[17][o] = fp0[3];
		data[18][o] = fp0[5];
		data[19][o] = fp0[7];
		data[20][o] = fp1[1];
		data[21][o] = fp1[3];
		data[22][o] = fp1[5];
		data[23][o] = fp1[7];
		data[24][o] = fp2[1];
		data[25][o] = fp2[3];
		data[26][o] = fp2[5];
		data[27][o] = fp2[7];
		data[28][o] = fp3[1];
		data[29][o] = fp3[3];
		data[30][o] = fp3[5];
		data[31][o] = fp3[7];
		data[32][o] = fp4[0];
		data[33][o] = fp4[2];
		data[34][o] = fp4[4];
		data[35][o] = fp4[6];
		data[36][o] = fp5[0];
		data[37][o] = fp5[2];
		data[38][o] = fp5[4];
		data[39][o] = fp5[6];
		data[40][o] = fp6[0];
		data[41][o] = fp6[2];
		data[42][o] = fp6[4];
		data[43][o] = fp6[6];
		data[44][o] = fp7[0];
		data[45][o] = fp7[2];
		data[46][o] = fp7[4];
		data[47][o] = fp7[6];
		data[48][o] = fp4[1];
		data[49][o] = fp4[3];
		data[50][o] = fp4[5];
		data[51][o] = fp4[7];
		data[52][o] = fp5[1];
		data[53][o] = fp5[3];
		data[54][o] = fp5[5];
		data[55][o] = fp5[7];
		data[56][o] = fp6[1];
		data[57][o] = fp6[3];
		data[58][o] = fp6[5];
		data[59][o] = fp6[7];
		data[60][o] = fp7[1];
		data[61][o] = fp7[3];
		data[62][o] = fp7[5];
		data[63][o] = fp7[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_64track_fanout1_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 32;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp4 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp5 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp6 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp7 = lut1bit[modulate[m]][buf[i]];
			i+=25;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[2];
		data[2][o]  = fp0[4];
		data[3][o]  = fp0[6];
		data[4][o]  = fp1[0];
		data[5][o]  = fp1[2];
		data[6][o]  = fp1[4];
		data[7][o]  = fp1[6];
		data[8][o]  = fp2[0];
		data[9][o]  = fp2[2];
		data[10][o] = fp2[4];
		data[11][o] = fp2[6];
		data[12][o] = fp3[0];
		data[13][o] = fp3[2];
		data[14][o] = fp3[4];
		data[15][o] = fp3[6];
		data[16][o] = fp0[1];
		data[17][o] = fp0[3];
		data[18][o] = fp0[5];
		data[19][o] = fp0[7];
		data[20][o] = fp1[1];
		data[21][o] = fp1[3];
		data[22][o] = fp1[5];
		data[23][o] = fp1[7];
		data[24][o] = fp2[1];
		data[25][o] = fp2[3];
		data[26][o] = fp2[5];
		data[27][o] = fp2[7];
		data[28][o] = fp3[1];
		data[29][o] = fp3[3];
		data[30][o] = fp3[5];
		data[31][o] = fp3[7];
		data[32][o] = fp4[0];
		data[33][o] = fp4[2];
		data[34][o] = fp4[4];
		data[35][o] = fp4[6];
		data[36][o] = fp5[0];
		data[37][o] = fp5[2];
		data[38][o] = fp5[4];
		data[39][o] = fp5[6];
		data[40][o] = fp6[0];
		data[41][o] = fp6[2];
		data[42][o] = fp6[4];
		data[43][o] = fp6[6];
		data[44][o] = fp7[0];
		data[45][o] = fp7[2];
		data[46][o] = fp7[4];
		data[47][o] = fp7[6];
		data[48][o] = fp4[1];
		data[49][o] = fp4[3];
		data[50][o] = fp4[5];
		data[51][o] = fp4[7];
		data[52][o] = fp5[1];
		data[53][o] = fp5[3];
		data[54][o] = fp5[5];
		data[55][o] = fp5[7];
		data[56][o] = fp6[1];
		data[57][o] = fp6[3];
		data[58][o] = fp6[5];
		data[59][o] = fp6[7];
		data[60][o] = fp7[1];
		data[61][o] = fp7[3];
		data[62][o] = fp7[5];
		data[63][o] = fp7[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_1bit_64track_fanout2_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp4 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp5 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp6 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp7 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[4];
		data[2][o]  = fp1[0];
		data[3][o]  = fp1[4];
		data[4][o]  = fp2[0];
		data[5][o]  = fp2[4];
		data[6][o]  = fp3[0];
		data[7][o]  = fp3[4];
		data[8][o]  = fp0[1];
		data[9][o]  = fp0[5];
		data[10][o] = fp1[1];
		data[11][o] = fp1[5];
		data[12][o] = fp2[1];
		data[13][o] = fp2[5];
		data[14][o] = fp3[1];
		data[15][o] = fp3[5];
		data[16][o] = fp4[0];
		data[17][o] = fp4[4];
		data[18][o] = fp5[0];
		data[19][o] = fp5[4];
		data[20][o] = fp6[0];
		data[21][o] = fp6[4];
		data[22][o] = fp7[0];
		data[23][o] = fp7[4];
		data[24][o] = fp4[1];
		data[25][o] = fp4[5];
		data[26][o] = fp5[1];
		data[27][o] = fp5[5];
		data[28][o] = fp6[1];
		data[29][o] = fp6[5];
		data[30][o] = fp7[1];
		data[31][o] = fp7[5];
		o++;
		data[0][o]  = fp0[2];
		data[1][o]  = fp0[6];
		data[2][o]  = fp1[2];
		data[3][o]  = fp1[6];
		data[4][o]  = fp2[2];
		data[5][o]  = fp2[6];
		data[6][o]  = fp3[2];
		data[7][o]  = fp3[6];
		data[8][o]  = fp0[3];
		data[9][o]  = fp0[7];
		data[10][o] = fp1[3];
		data[11][o] = fp1[7];
		data[12][o] = fp2[3];
		data[13][o] = fp2[7];
		data[14][o] = fp3[3];
		data[15][o] = fp3[7];
		data[16][o] = fp4[2];
		data[17][o] = fp4[6];
		data[18][o] = fp5[2];
		data[19][o] = fp5[6];
		data[20][o] = fp6[2];
		data[21][o] = fp6[6];
		data[22][o] = fp7[2];
		data[23][o] = fp7[6];
		data[24][o] = fp4[3];
		data[25][o] = fp4[7];
		data[26][o] = fp5[3];
		data[27][o] = fp5[7];
		data[28][o] = fp6[3];
		data[29][o] = fp6[7];
		data[30][o] = fp7[3];
		data[31][o] = fp7[7];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_64track_fanout2_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp4 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp5 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp6 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp7 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[4];
		data[2][o]  = fp1[0];
		data[3][o]  = fp1[4];
		data[4][o]  = fp2[0];
		data[5][o]  = fp2[4];
		data[6][o]  = fp3[0];
		data[7][o]  = fp3[4];
		data[8][o]  = fp0[1];
		data[9][o]  = fp0[5];
		data[10][o] = fp1[1];
		data[11][o] = fp1[5];
		data[12][o] = fp2[1];
		data[13][o] = fp2[5];
		data[14][o] = fp3[1];
		data[15][o] = fp3[5];
		data[16][o] = fp4[0];
		data[17][o] = fp4[4];
		data[18][o] = fp5[0];
		data[19][o] = fp5[4];
		data[20][o] = fp6[0];
		data[21][o] = fp6[4];
		data[22][o] = fp7[0];
		data[23][o] = fp7[4];
		data[24][o] = fp4[1];
		data[25][o] = fp4[5];
		data[26][o] = fp5[1];
		data[27][o] = fp5[5];
		data[28][o] = fp6[1];
		data[29][o] = fp6[5];
		data[30][o] = fp7[1];
		data[31][o] = fp7[5];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_64track_fanout2_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 16;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp4 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp5 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp6 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp7 = lut1bit[modulate[m]][buf[i]];
			i+=9;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[4];
		data[2][o]  = fp1[0];
		data[3][o]  = fp1[4];
		data[4][o]  = fp2[0];
		data[5][o]  = fp2[4];
		data[6][o]  = fp3[0];
		data[7][o]  = fp3[4];
		data[8][o]  = fp0[1];
		data[9][o]  = fp0[5];
		data[10][o] = fp1[1];
		data[11][o] = fp1[5];
		data[12][o] = fp2[1];
		data[13][o] = fp2[5];
		data[14][o] = fp3[1];
		data[15][o] = fp3[5];
		data[16][o] = fp4[0];
		data[17][o] = fp4[4];
		data[18][o] = fp5[0];
		data[19][o] = fp5[4];
		data[20][o] = fp6[0];
		data[21][o] = fp6[4];
		data[22][o] = fp7[0];
		data[23][o] = fp7[4];
		data[24][o] = fp4[1];
		data[25][o] = fp4[5];
		data[26][o] = fp5[1];
		data[27][o] = fp5[5];
		data[28][o] = fp6[1];
		data[29][o] = fp6[5];
		data[30][o] = fp7[1];
		data[31][o] = fp7[5];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_1bit_64track_fanout4_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp4 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp5 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp6 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp7 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;
		
		data[0][o]  = fp0[0];
		data[1][o]  = fp1[0];
		data[2][o]  = fp2[0];
		data[3][o]  = fp3[0];
		data[4][o]  = fp0[1];
		data[5][o]  = fp1[1];
		data[6][o]  = fp2[1];
		data[7][o]  = fp3[1];
		data[8][o]  = fp4[0];
		data[9][o]  = fp5[0];
		data[10][o] = fp6[0];
		data[11][o] = fp7[0];
		data[12][o] = fp4[1];
		data[13][o] = fp5[1];
		data[14][o] = fp6[1];
		data[15][o] = fp7[1];
		o++;
		data[0][o]  = fp0[2];
		data[1][o]  = fp1[2];
		data[2][o]  = fp2[2];
		data[3][o]  = fp3[2];
		data[4][o]  = fp0[3];
		data[5][o]  = fp1[3];
		data[6][o]  = fp2[3];
		data[7][o]  = fp3[3];
		data[8][o]  = fp4[2];
		data[9][o]  = fp5[2];
		data[10][o] = fp6[2];
		data[11][o] = fp7[2];
		data[12][o] = fp4[3];
		data[13][o] = fp5[3];
		data[14][o] = fp6[3];
		data[15][o] = fp7[3];
		o++;
		data[0][o]  = fp0[4];
		data[1][o]  = fp1[4];
		data[2][o]  = fp2[4];
		data[3][o]  = fp3[4];
		data[4][o]  = fp0[5];
		data[5][o]  = fp1[5];
		data[6][o]  = fp2[5];
		data[7][o]  = fp3[5];
		data[8][o]  = fp4[4];
		data[9][o]  = fp5[4];
		data[10][o] = fp6[4];
		data[11][o] = fp7[4];
		data[12][o] = fp4[5];
		data[13][o] = fp5[5];
		data[14][o] = fp6[5];
		data[15][o] = fp7[5];
		o++;
		data[0][o]  = fp0[6];
		data[1][o]  = fp1[6];
		data[2][o]  = fp2[6];
		data[3][o]  = fp3[6];
		data[4][o]  = fp0[7];
		data[5][o]  = fp1[7];
		data[6][o]  = fp2[7];
		data[7][o]  = fp3[7];
		data[8][o]  = fp4[6];
		data[9][o]  = fp5[6];
		data[10][o] = fp6[6];
		data[11][o] = fp7[6];
		data[12][o] = fp4[7];
		data[13][o] = fp5[7];
		data[14][o] = fp6[7];
		data[15][o] = fp7[7];


		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_64track_fanout4_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp4 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp5 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp6 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp7 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;
		
		data[0][o]  = fp0[0];
		data[1][o]  = fp1[0];
		data[2][o]  = fp2[0];
		data[3][o]  = fp3[0];
		data[4][o]  = fp0[1];
		data[5][o]  = fp1[1];
		data[6][o]  = fp2[1];
		data[7][o]  = fp3[1];
		data[8][o]  = fp4[0];
		data[9][o]  = fp5[0];
		data[10][o] = fp6[0];
		data[11][o] = fp7[0];
		data[12][o] = fp4[1];
		data[13][o] = fp5[1];
		data[14][o] = fp6[1];
		data[15][o] = fp7[1];
		o++;
		data[0][o]  = fp0[4];
		data[1][o]  = fp1[4];
		data[2][o]  = fp2[4];
		data[3][o]  = fp3[4];
		data[4][o]  = fp0[5];
		data[5][o]  = fp1[5];
		data[6][o]  = fp2[5];
		data[7][o]  = fp3[5];
		data[8][o]  = fp4[4];
		data[9][o]  = fp5[4];
		data[10][o] = fp6[4];
		data[11][o] = fp7[4];
		data[12][o] = fp4[5];
		data[13][o] = fp5[5];
		data[14][o] = fp6[5];
		data[15][o] = fp7[5];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_1bit_64track_fanout4_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp1 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp2 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp3 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp4 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp5 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp6 = lut1bit[modulate[m]][buf[i]];
			i++;
			fp7 = lut1bit[modulate[m]][buf[i]];
			i++;
		}
		m++;
		
		data[0][o]  = fp0[0];
		data[1][o]  = fp1[0];
		data[2][o]  = fp2[0];
		data[3][o]  = fp3[0];
		data[4][o]  = fp0[1];
		data[5][o]  = fp1[1];
		data[6][o]  = fp2[1];
		data[7][o]  = fp3[1];
		data[8][o]  = fp4[0];
		data[9][o]  = fp5[0];
		data[10][o] = fp6[0];
		data[11][o] = fp7[0];
		data[12][o] = fp4[1];
		data[13][o] = fp5[1];
		data[14][o] = fp6[1];
		data[15][o] = fp7[1];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

/************************ 2-bit decoders *********************/

static int vlba_decode_2bit_2track_fanout1_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit1[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_2track_fanout1_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit1[modulate[i]][buf[i]];
		}
		i+=2;

		data[0][o] = fp[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_2track_fanout1_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit1[modulate[i]][buf[i]];
		}
		i+=4;

		data[0][o] = fp[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_4track_fanout1_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit1[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[1];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_4track_fanout1_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit1[modulate[i]][buf[i]];
		}
		i+=2;

		data[0][o] = fp[0];
		data[1][o] = fp[1];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_4track_fanout1_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit1[modulate[i]][buf[i]];
		}
		i+=4;

		data[0][o] = fp[0];
		data[1][o] = fp[1];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_4track_fanout2_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit2[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		o++;
		data[0][o] = fp[1];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_4track_fanout2_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit2[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_4track_fanout2_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit2[modulate[i]][buf[i]];
		}
		i+=2;

		data[0][o] = fp[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_8track_fanout1_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit1[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[1];
		data[2][o] = fp[2];
		data[3][o] = fp[3];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}


static int vlba_decode_2bit_8track_fanout1_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit1[modulate[i]][buf[i]];
		}
		i+=2;

		data[0][o] = fp[0];
		data[1][o] = fp[1];
		data[2][o] = fp[2];
		data[3][o] = fp[3];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}


static int vlba_decode_2bit_8track_fanout1_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit1[modulate[i]][buf[i]];
		}
		i+=4;

		data[0][o] = fp[0];
		data[1][o] = fp[1];
		data[2][o] = fp[2];
		data[3][o] = fp[3];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_8track_fanout2_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit2[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[2];
		o++;
		data[0][o] = fp[1];
		data[1][o] = fp[3];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_8track_fanout2_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit2[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		data[1][o] = fp[2];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_8track_fanout2_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit2[modulate[i]][buf[i]];
		}
		i+=2;

		data[0][o] = fp[0];
		data[1][o] = fp[2];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_8track_fanout4_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit3[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		o++;
		data[0][o] = fp[1];
		o++;
		data[0][o] = fp[2];
		o++;
		data[0][o] = fp[3];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_2bit_8track_fanout4_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit3[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];
		o++;
		data[0][o] = fp[2];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_2bit_8track_fanout4_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp;
	int o, i;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp = zeros;
			nblank++;
		}
		else
		{
			fp = lut2bit3[modulate[i]][buf[i]];
		}
		i++;

		data[0][o] = fp[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_2bit_16track_fanout1_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 2;
			nblank++;
		}
		else
		{
			fp0 = lut2bit1[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit1[modulate[m]][buf[i]];
			i++;
		}
		m++;
	
		data[0][o]  = fp0[0];
		data[1][o]  = fp0[1];
		data[2][o]  = fp0[2];
		data[3][o]  = fp0[3];
		data[4][o]  = fp1[0];
		data[5][o]  = fp1[1];
		data[6][o]  = fp1[2];
		data[7][o]  = fp1[3];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_16track_fanout1_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 4;
			nblank++;
		}
		else
		{
			fp0 = lut2bit1[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit1[modulate[m]][buf[i]];
			i+=3;
		}
		m++;
	
		data[0][o]  = fp0[0];
		data[1][o]  = fp0[1];
		data[2][o]  = fp0[2];
		data[3][o]  = fp0[3];
		data[4][o]  = fp1[0];
		data[5][o]  = fp1[1];
		data[6][o]  = fp1[2];
		data[7][o]  = fp1[3];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_16track_fanout1_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut2bit1[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit1[modulate[m]][buf[i]];
			i+=7;
		}
		m++;
	
		data[0][o]  = fp0[0];
		data[1][o]  = fp0[1];
		data[2][o]  = fp0[2];
		data[3][o]  = fp0[3];
		data[4][o]  = fp1[0];
		data[5][o]  = fp1[1];
		data[6][o]  = fp1[2];
		data[7][o]  = fp1[3];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_16track_fanout2_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 2;
			nblank++;
		}
		else
		{
			fp0 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit2[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o] = fp0[0];
		data[1][o] = fp0[2];
		data[2][o] = fp1[0];
		data[3][o] = fp1[2];
		o++;
		data[0][o] = fp0[1];
		data[1][o] = fp0[3];
		data[2][o] = fp1[1];
		data[3][o] = fp1[3];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_16track_fanout2_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 2;
			nblank++;
		}
		else
		{
			fp0 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit2[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o] = fp0[0];
		data[1][o] = fp0[2];
		data[2][o] = fp1[0];
		data[3][o] = fp1[2];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_16track_fanout2_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 4;
			nblank++;
		}
		else
		{
			fp0 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit2[modulate[m]][buf[i]];
			i+=3;
		}
		m++;

		data[0][o] = fp0[0];
		data[1][o] = fp0[2];
		data[2][o] = fp1[0];
		data[3][o] = fp1[2];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_16track_fanout4_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 2;
			nblank++;
		}
		else
		{
			fp0 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit3[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o] = fp0[0];
		data[1][o] = fp1[0];
		o++;
		data[0][o] = fp0[1];
		data[1][o] = fp1[1];
		o++;
		data[0][o] = fp0[2];
		data[1][o] = fp1[2];
		o++;
		data[0][o] = fp0[3];
		data[1][o] = fp1[3];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_2bit_16track_fanout4_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 2;
			nblank++;
		}
		else
		{
			fp0 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit3[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o] = fp0[0];
		data[1][o] = fp1[0];
		o++;
		data[0][o] = fp0[2];
		data[1][o] = fp1[2];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_2bit_16track_fanout4_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/2;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = zeros;
			i += 2;
			nblank++;
		}
		else
		{
			fp0 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit3[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o] = fp0[0];
		data[1][o] = fp1[0];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 4*nblank;
}

static int vlba_decode_2bit_32track_fanout1_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 4;
			nblank++;
		}
		else
		{
			fp0 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp2 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp3 = lut2bit2[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[2];
		data[2][o]  = fp1[0];
		data[3][o]  = fp1[2];
		data[4][o]  = fp2[0];
		data[5][o]  = fp2[2];
		data[6][o]  = fp3[0];
		data[7][o]  = fp3[2];
		data[8][o]  = fp0[1];
		data[9][o]  = fp0[3];
		data[10][o] = fp1[1];
		data[11][o] = fp1[3];
		data[12][o] = fp2[1];
		data[13][o] = fp2[3];
		data[14][o] = fp3[1];
		data[15][o] = fp3[3];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_32track_fanout1_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp2 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp3 = lut2bit2[modulate[m]][buf[i]];
			i+=5;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[2];
		data[2][o]  = fp1[0];
		data[3][o]  = fp1[2];
		data[4][o]  = fp2[0];
		data[5][o]  = fp2[2];
		data[6][o]  = fp3[0];
		data[7][o]  = fp3[2];
		data[8][o]  = fp0[1];
		data[9][o]  = fp0[3];
		data[10][o] = fp1[1];
		data[11][o] = fp1[3];
		data[12][o] = fp2[1];
		data[13][o] = fp2[3];
		data[14][o] = fp3[1];
		data[15][o] = fp3[3];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_32track_fanout1_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 16;
			nblank++;
		}
		else
		{
			fp0 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp2 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp3 = lut2bit2[modulate[m]][buf[i]];
			i+=13;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[2];
		data[2][o]  = fp1[0];
		data[3][o]  = fp1[2];
		data[4][o]  = fp2[0];
		data[5][o]  = fp2[2];
		data[6][o]  = fp3[0];
		data[7][o]  = fp3[2];
		data[8][o]  = fp0[1];
		data[9][o]  = fp0[3];
		data[10][o] = fp1[1];
		data[11][o] = fp1[3];
		data[12][o] = fp2[1];
		data[13][o] = fp2[3];
		data[14][o] = fp3[1];
		data[15][o] = fp3[3];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_32track_fanout2_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 4;
			nblank++;
		}
		else
		{
			fp0 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp2 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp3 = lut2bit3[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp1[0];
		data[2][o]  = fp2[0];
		data[3][o]  = fp3[0];
		data[4][o]  = fp0[1];
		data[5][o]  = fp1[1];
		data[6][o]  = fp2[1];
		data[7][o]  = fp3[1];
		o++;
		data[0][o]  = fp0[2];
		data[1][o]  = fp1[2];
		data[2][o]  = fp2[2];
		data[3][o]  = fp3[2];
		data[4][o]  = fp0[3];
		data[5][o]  = fp1[3];
		data[6][o]  = fp2[3];
		data[7][o]  = fp3[3];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_32track_fanout2_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 4;
			nblank++;
		}
		else
		{
			fp0 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp2 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp3 = lut2bit3[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp1[0];
		data[2][o]  = fp2[0];
		data[3][o]  = fp3[0];
		data[4][o]  = fp0[1];
		data[5][o]  = fp1[1];
		data[6][o]  = fp2[1];
		data[7][o]  = fp3[1];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_32track_fanout2_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/4;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp2 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp3 = lut2bit3[modulate[m]][buf[i]];
			i+=5;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp1[0];
		data[2][o]  = fp2[0];
		data[3][o]  = fp3[0];
		data[4][o]  = fp0[1];
		data[5][o]  = fp1[1];
		data[6][o]  = fp2[1];
		data[7][o]  = fp3[1];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_32track_fanout4_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint32_t *buf, bits;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i;
	uint8_t *bytes;
	int zone, l2;
	int nblank = 0;

	buf = (uint32_t *)(ms->payload);
	i = ms->readposition >> 2;  /* note here that i counts 32-bit words */
	l2 = ms->log2blankzonesize - 2;

	bytes = (uint8_t *)(& bits);

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> l2;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			nblank++;
		}
		else
		{
			bits = reorder32(buf[i]);
			fp0 = lut2bit1[modulate[i]][bytes[0]];
			fp1 = lut2bit1[modulate[i]][bytes[1]];
			fp2 = lut2bit1[modulate[i]][bytes[2]];
			fp3 = lut2bit1[modulate[i]][bytes[3]];
		}
		i++;

		data[0][o] = fp0[0];
		data[1][o] = fp2[0];
		data[2][o] = fp1[0];
		data[3][o] = fp3[0];
		o++;
		data[0][o] = fp0[1];
		data[1][o] = fp2[1];
		data[2][o] = fp1[1];
		data[3][o] = fp3[1];
		o++;
		data[0][o] = fp0[2];
		data[1][o] = fp2[2];
		data[2][o] = fp1[2];
		data[3][o] = fp3[2];
		o++;
		data[0][o] = fp0[3];
		data[1][o] = fp2[3];
		data[2][o] = fp1[3];
		data[3][o] = fp3[3];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = (uint32_t *)(ms->payload);
			i = 0;
		}
	}

	ms->readposition = 4*i;

	return nsamp - 4*nblank;
}

static int vlba_decode_2bit_32track_fanout4_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint32_t *buf, bits;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i;
	uint8_t *bytes;
	int zone, l2;
	int nblank = 0;

	buf = (uint32_t *)(ms->payload);
	i = ms->readposition >> 2;  /* note here that i counts 32-bit words */
	l2 = ms->log2blankzonesize - 2;

	bytes = (uint8_t *)(& bits);

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> l2;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			nblank++;
		}
		else
		{
			bits = reorder32(buf[i]);
			fp0 = lut2bit1[modulate[i]][bytes[0]];
			fp1 = lut2bit1[modulate[i]][bytes[1]];
			fp2 = lut2bit1[modulate[i]][bytes[2]];
			fp3 = lut2bit1[modulate[i]][bytes[3]];
		}
		i++;

		data[0][o] = fp0[0];
		data[1][o] = fp2[0];
		data[2][o] = fp1[0];
		data[3][o] = fp3[0];
		o++;
		data[0][o] = fp0[2];
		data[1][o] = fp2[2];
		data[2][o] = fp1[2];
		data[3][o] = fp3[2];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = (uint32_t *)(ms->payload);
			i = 0;
		}
	}

	ms->readposition = 4*i;

	return nsamp - 4*nblank;
}

static int vlba_decode_2bit_32track_fanout4_oversamp4(struct mark5_stream *ms, int nsamp,
	float **data)
{
	uint32_t *buf, bits;
	float *fp0, *fp1, *fp2, *fp3;
	int o, i;
	uint8_t *bytes;
	int zone, l2;
	int nblank = 0;

	buf = (uint32_t *)(ms->payload);
	i = ms->readposition >> 2;  /* note here that i counts 32-bit words */
	l2 = ms->log2blankzonesize - 2;

	bytes = (uint8_t *)(& bits);

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> l2;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			nblank++;
		}
		else
		{
			bits = reorder32(buf[i]);
			fp0 = lut2bit1[modulate[i]][bytes[0]];
			fp1 = lut2bit1[modulate[i]][bytes[1]];
			fp2 = lut2bit1[modulate[i]][bytes[2]];
			fp3 = lut2bit1[modulate[i]][bytes[3]];
		}
		i++;

		data[0][o] = fp0[0];
		data[1][o] = fp2[0];
		data[2][o] = fp1[0];
		data[3][o] = fp3[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = (uint32_t *)(ms->payload);
			i = 0;
		}
	}

	ms->readposition = 4*i;

	return nsamp - 4*nblank;
}

static int vlba_decode_2bit_64track_fanout1_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp2 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp3 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp4 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp5 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp6 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp7 = lut2bit2[modulate[m]][buf[i]];
			i++;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[2];
		data[2][o]  = fp1[0];
		data[3][o]  = fp1[2];
		data[4][o]  = fp2[0];
		data[5][o]  = fp2[2];
		data[6][o]  = fp3[0];
		data[7][o]  = fp3[2];
		data[8][o]  = fp0[1];
		data[9][o]  = fp0[3];
		data[10][o] = fp1[1];
		data[11][o] = fp1[3];
		data[12][o] = fp2[1];
		data[13][o] = fp2[3];
		data[14][o] = fp3[1];
		data[15][o] = fp3[3];
		data[16][o] = fp4[0];
		data[17][o] = fp4[2];
		data[18][o] = fp5[0];
		data[19][o] = fp5[2];
		data[20][o] = fp6[0];
		data[21][o] = fp6[2];
		data[22][o] = fp7[0];
		data[23][o] = fp7[2];
		data[24][o] = fp4[1];
		data[25][o] = fp4[3];
		data[26][o] = fp5[1];
		data[27][o] = fp5[3];
		data[28][o] = fp6[1];
		data[29][o] = fp6[3];
		data[30][o] = fp7[1];
		data[31][o] = fp7[3];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_64track_fanout1_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 16;
			nblank++;
		}
		else
		{
			fp0 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp2 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp3 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp4 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp5 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp6 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp7 = lut2bit2[modulate[m]][buf[i]];
			i+=16;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[2];
		data[2][o]  = fp1[0];
		data[3][o]  = fp1[2];
		data[4][o]  = fp2[0];
		data[5][o]  = fp2[2];
		data[6][o]  = fp3[0];
		data[7][o]  = fp3[2];
		data[8][o]  = fp0[1];
		data[9][o]  = fp0[3];
		data[10][o] = fp1[1];
		data[11][o] = fp1[3];
		data[12][o] = fp2[1];
		data[13][o] = fp2[3];
		data[14][o] = fp3[1];
		data[15][o] = fp3[3];
		data[16][o] = fp4[0];
		data[17][o] = fp4[2];
		data[18][o] = fp5[0];
		data[19][o] = fp5[2];
		data[20][o] = fp6[0];
		data[21][o] = fp6[2];
		data[22][o] = fp7[0];
		data[23][o] = fp7[2];
		data[24][o] = fp4[1];
		data[25][o] = fp4[3];
		data[26][o] = fp5[1];
		data[27][o] = fp5[3];
		data[28][o] = fp6[1];
		data[29][o] = fp6[3];
		data[30][o] = fp7[1];
		data[31][o] = fp7[3];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_64track_fanout1_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 32;
			nblank++;
		}
		else
		{
			fp0 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp2 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp3 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp4 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp5 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp6 = lut2bit2[modulate[m]][buf[i]];
			i++;
			fp7 = lut2bit2[modulate[m]][buf[i]];
			i+=25;
		}
		m++;

		data[0][o]  = fp0[0];
		data[1][o]  = fp0[2];
		data[2][o]  = fp1[0];
		data[3][o]  = fp1[2];
		data[4][o]  = fp2[0];
		data[5][o]  = fp2[2];
		data[6][o]  = fp3[0];
		data[7][o]  = fp3[2];
		data[8][o]  = fp0[1];
		data[9][o]  = fp0[3];
		data[10][o] = fp1[1];
		data[11][o] = fp1[3];
		data[12][o] = fp2[1];
		data[13][o] = fp2[3];
		data[14][o] = fp3[1];
		data[15][o] = fp3[3];
		data[16][o] = fp4[0];
		data[17][o] = fp4[2];
		data[18][o] = fp5[0];
		data[19][o] = fp5[2];
		data[20][o] = fp6[0];
		data[21][o] = fp6[2];
		data[22][o] = fp7[0];
		data[23][o] = fp7[2];
		data[24][o] = fp4[1];
		data[25][o] = fp4[3];
		data[26][o] = fp5[1];
		data[27][o] = fp5[3];
		data[28][o] = fp6[1];
		data[29][o] = fp6[3];
		data[30][o] = fp7[1];
		data[31][o] = fp7[3];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - nblank;
}

static int vlba_decode_2bit_64track_fanout2_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp2 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp3 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp4 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp5 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp6 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp7 = lut2bit3[modulate[m]][buf[i]];
			i++;
		}
		m++;
		
		data[0][o]  = fp0[0];
		data[1][o]  = fp1[0];
		data[2][o]  = fp2[0];
		data[3][o]  = fp3[0];
		data[4][o]  = fp0[1];
		data[5][o]  = fp1[1];
		data[6][o]  = fp2[1];
		data[7][o]  = fp3[1];
		data[8][o]  = fp4[0];
		data[9][o]  = fp5[0];
		data[10][o] = fp6[0];
		data[11][o] = fp7[0];
		data[12][o] = fp4[1];
		data[13][o] = fp5[1];
		data[14][o] = fp6[1];
		data[15][o] = fp7[1];
		o++;
		data[0][o]  = fp0[2];
		data[1][o]  = fp1[2];
		data[2][o]  = fp2[2];
		data[3][o]  = fp3[2];
		data[4][o]  = fp0[3];
		data[5][o]  = fp1[3];
		data[6][o]  = fp2[3];
		data[7][o]  = fp3[3];
		data[8][o]  = fp4[2];
		data[9][o]  = fp5[2];
		data[10][o] = fp6[2];
		data[11][o] = fp7[2];
		data[12][o] = fp4[3];
		data[13][o] = fp5[3];
		data[14][o] = fp6[3];
		data[15][o] = fp7[3];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_64track_fanout2_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 8;
			nblank++;
		}
		else
		{
			fp0 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp2 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp3 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp4 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp5 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp6 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp7 = lut2bit3[modulate[m]][buf[i]];
			i++;
		}
		m++;
		
		data[0][o]  = fp0[0];
		data[1][o]  = fp1[0];
		data[2][o]  = fp2[0];
		data[3][o]  = fp3[0];
		data[4][o]  = fp0[1];
		data[5][o]  = fp1[1];
		data[6][o]  = fp2[1];
		data[7][o]  = fp3[1];
		data[8][o]  = fp4[0];
		data[9][o]  = fp5[0];
		data[10][o] = fp6[0];
		data[11][o] = fp7[0];
		data[12][o] = fp4[1];
		data[13][o] = fp5[1];
		data[14][o] = fp6[1];
		data[15][o] = fp7[1];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_64track_fanout2_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint8_t *buf;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i, m;
	int zone;
	int nblank = 0;

	buf = ms->payload;
	i = ms->readposition;
	m = i/8;

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> ms->log2blankzonesize;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			i += 16;
			nblank++;
		}
		else
		{
			fp0 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp1 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp2 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp3 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp4 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp5 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp6 = lut2bit3[modulate[m]][buf[i]];
			i++;
			fp7 = lut2bit3[modulate[m]][buf[i]];
			i+=9;
		}
		m++;
		
		data[0][o]  = fp0[0];
		data[1][o]  = fp1[0];
		data[2][o]  = fp2[0];
		data[3][o]  = fp3[0];
		data[4][o]  = fp0[1];
		data[5][o]  = fp1[1];
		data[6][o]  = fp2[1];
		data[7][o]  = fp3[1];
		data[8][o]  = fp4[0];
		data[9][o]  = fp5[0];
		data[10][o] = fp6[0];
		data[11][o] = fp7[0];
		data[12][o] = fp4[1];
		data[13][o] = fp5[1];
		data[14][o] = fp6[1];
		data[15][o] = fp7[1];

		if(m >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = ms->payload;
			i = 0;
			m = 0;
		}
	}

	ms->readposition = i;

	return nsamp - 2*nblank;
}

static int vlba_decode_2bit_64track_fanout4_oversamp1(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint64_t *buf, bits;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i;
	uint8_t *bytes;
	int zone, l2;
	int nblank = 0;

	buf = (uint64_t *)(ms->payload);
	i = ms->readposition >> 3;  /* note that i here counts 64-bit words */
	l2 = ms->log2blankzonesize - 3;

	bytes = (uint8_t *)(& bits);

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> l2;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			nblank++;
		}
		else
		{
			bits = reorder64(buf[i]);
			fp0 = lut2bit1[modulate[i]][bytes[0]];
			fp1 = lut2bit1[modulate[i]][bytes[1]];
			fp2 = lut2bit1[modulate[i]][bytes[2]];
			fp3 = lut2bit1[modulate[i]][bytes[3]];
			fp4 = lut2bit1[modulate[i]][bytes[4]];
			fp5 = lut2bit1[modulate[i]][bytes[5]];
			fp6 = lut2bit1[modulate[i]][bytes[6]];
			fp7 = lut2bit1[modulate[i]][bytes[7]];
		}
		i++;
		
		data[0][o] = fp0[0];
		data[1][o] = fp2[0];
		data[2][o] = fp1[0];
		data[3][o] = fp3[0];
		data[4][o] = fp4[0];
		data[5][o] = fp6[0];
		data[6][o] = fp5[0];
		data[7][o] = fp7[0];
		o++;
		data[0][o] = fp0[1];
		data[1][o] = fp2[1];
		data[2][o] = fp1[1];
		data[3][o] = fp3[1];
		data[4][o] = fp4[1];
		data[5][o] = fp6[1];
		data[6][o] = fp5[1];
		data[7][o] = fp7[1];
		o++;
		data[0][o] = fp0[2];
		data[1][o] = fp2[2];
		data[2][o] = fp1[2];
		data[3][o] = fp3[2];
		data[4][o] = fp4[2];
		data[5][o] = fp6[2];
		data[6][o] = fp5[2];
		data[7][o] = fp7[2];
		o++;
		data[0][o] = fp0[3];
		data[1][o] = fp2[3];
		data[2][o] = fp1[3];
		data[3][o] = fp3[3];
		data[4][o] = fp4[3];
		data[5][o] = fp6[3];
		data[6][o] = fp5[3];
		data[7][o] = fp7[3];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = (uint64_t *)(ms->payload);
			i = 0;
		}
	}

	ms->readposition = 8*i;

	return nsamp - 4*nblank;
}

static int vlba_decode_2bit_64track_fanout4_oversamp2(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint64_t *buf, bits;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i;
	uint8_t *bytes;
	int zone, l2;
	int nblank = 0;

	buf = (uint64_t *)(ms->payload);
	i = ms->readposition >> 3;  /* note that i here counts 64-bit words */
	l2 = ms->log2blankzonesize - 3;

	bytes = (uint8_t *)(& bits);

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> l2;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			nblank++;
		}
		else
		{
			bits = reorder64(buf[i]);
			fp0 = lut2bit1[modulate[i]][bytes[0]];
			fp1 = lut2bit1[modulate[i]][bytes[1]];
			fp2 = lut2bit1[modulate[i]][bytes[2]];
			fp3 = lut2bit1[modulate[i]][bytes[3]];
			fp4 = lut2bit1[modulate[i]][bytes[4]];
			fp5 = lut2bit1[modulate[i]][bytes[5]];
			fp6 = lut2bit1[modulate[i]][bytes[6]];
			fp7 = lut2bit1[modulate[i]][bytes[7]];
		}
		i++;
		
		data[0][o] = fp0[0];
		data[1][o] = fp2[0];
		data[2][o] = fp1[0];
		data[3][o] = fp3[0];
		data[4][o] = fp4[0];
		data[5][o] = fp6[0];
		data[6][o] = fp5[0];
		data[7][o] = fp7[0];
		o++;
		data[0][o] = fp0[2];
		data[1][o] = fp2[2];
		data[2][o] = fp1[2];
		data[3][o] = fp3[2];
		data[4][o] = fp4[2];
		data[5][o] = fp6[2];
		data[6][o] = fp5[2];
		data[7][o] = fp7[2];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = (uint64_t *)(ms->payload);
			i = 0;
		}
	}

	ms->readposition = 8*i;

	return nsamp - 4*nblank;
}

static int vlba_decode_2bit_64track_fanout4_oversamp4(struct mark5_stream *ms, 
	int nsamp, float **data)
{
	uint64_t *buf, bits;
	float *fp0, *fp1, *fp2, *fp3, *fp4, *fp5, *fp6, *fp7;
	int o, i;
	uint8_t *bytes;
	int zone, l2;
	int nblank = 0;

	buf = (uint64_t *)(ms->payload);
	i = ms->readposition >> 3;  /* note that i here counts 64-bit words */
	l2 = ms->log2blankzonesize - 3;

	bytes = (uint8_t *)(& bits);

	for(o = 0; o < nsamp; o++)
	{
		zone = i >> l2;

		if(i <  ms->blankzonestartvalid[zone] || 
		   i >= ms->blankzoneendvalid[zone])
		{
			fp0 = fp1 = fp2 = fp3 = zeros;
			fp4 = fp5 = fp6 = fp7 = zeros;
			nblank++;
		}
		else
		{
			bits = reorder64(buf[i]);
			fp0 = lut2bit1[modulate[i]][bytes[0]];
			fp1 = lut2bit1[modulate[i]][bytes[1]];
			fp2 = lut2bit1[modulate[i]][bytes[2]];
			fp3 = lut2bit1[modulate[i]][bytes[3]];
			fp4 = lut2bit1[modulate[i]][bytes[4]];
			fp5 = lut2bit1[modulate[i]][bytes[5]];
			fp6 = lut2bit1[modulate[i]][bytes[6]];
			fp7 = lut2bit1[modulate[i]][bytes[7]];
		}
		i++;
		
		data[0][o] = fp0[0];
		data[1][o] = fp2[0];
		data[2][o] = fp1[0];
		data[3][o] = fp3[0];
		data[4][o] = fp4[0];
		data[5][o] = fp6[0];
		data[6][o] = fp5[0];
		data[7][o] = fp7[0];

		if(i >= PAYLOADSIZE)
		{
			if(mark5_stream_next_frame(ms) < 0)
			{
				return -1;
			}
			buf = (uint64_t *)(ms->payload);
			i = 0;
		}
	}

	ms->readposition = 8*i;

	return nsamp - 4*nblank;
}

/******************************************************************/

static int mark5_format_vlba_make_formatname(struct mark5_stream *ms)
{
	struct mark5_format_vlba *f;

	f = (struct mark5_format_vlba *)(ms->formatdata);
	
	sprintf(ms->formatname, "VLBA%d_%d-%d-%d-%d", ms->oversamp,
		f->fanout, ms->Mbps, ms->nchan, ms->nbit);

	return 0;
}

static int mark5_format_vlba_init(struct mark5_stream *ms)
{
	struct mark5_format_vlba *f;
	int bytes;
	int tol=5;
	int mjd1, sec1, ns1;
	double dns1, dns;
	int datarate;
	int nRealTrack;

	if(!ms)
	{
		fprintf(stderr, "mark5_format_vlba_init: ms = 0\n");
		return -1;
	}

	f = (struct mark5_format_vlba *)(ms->formatdata);

	nRealTrack = f->ntrack;
	if(nRealTrack < 8)
	{
		nRealTrack = 8;
	}
	ms->samplegranularity = f->fanout/ms->oversamp;
	ms->framebytes = 20160*nRealTrack/8;
	ms->databytes = 20000*nRealTrack/8;
	ms->payloadoffset = 12*nRealTrack;
	ms->framesamples = 20000*f->fanout/ms->oversamp;
	ms->format = MK5_FORMAT_VLBA;
	ms->framegranularity = 1;

	ms->blanker = blanker_mark5;

	if(ms->datawindow)
	{
		if(ms->datawindowsize < ms->framebytes)
		{
			return -1;
		}

		/* look through entire data window, up to 1Mibytes */
		bytes = ms->datawindowsize < (1<<20) ?
			ms->datawindowsize : (1<<20);
		ms->frameoffset = findfirstframe(ms->datawindow, bytes, 
			nRealTrack, tol);
		if(ms->frameoffset < 0)
		{
			return -1;
		}

		ms->frame = ms->datawindow + ms->frameoffset;
		ms->payload = ms->frame + ms->payloadoffset;

		ms->gettime(ms, &ms->mjd, &ms->sec, &dns);
		ms->ns = (int)(dns+0.5);
		ms->frame += ms->framebytes;
		ms->gettime(ms, &mjd1, &sec1, &dns1);
		ns1 = (int)(dns1 + 0.5);
		ms->frame -= ms->framebytes;

		/* assume frame time less than 1 second, integer number of
		 * frames per second
		 */
		if(ns1 != ms->ns)
		{
			ms->framens = ns1 - ms->ns;
			if(ms->framens <= 0)
			{
				ms->framens += 1000000000;
			}
			ms->samprate = ms->framesamples*
				(1000000000/ms->framens);
			datarate = ms->samprate*ms->nbit*ms->nchan/1000000;
			if(datarate != ms->Mbps)
			{
				if(ms->Mbps > 0)
				{
					fprintf(stderr, "Warning -- data rate "
						"disagrees : %d != %d\n",
						datarate, ms->Mbps);
				}
				ms->Mbps = datarate;
			}
		}
		else
		{
			fprintf(stderr, "Warning -- rate calc. suspect\n");
		}
	}

	ms->gframens = (int)(ms->framegranularity*ms->framens + 0.5);

	mark5_format_vlba_make_formatname(ms);

	return 0;
}

static int mark5_format_vlba_final(struct mark5_stream *ms)
{
	if(!ms)
	{
		return -1;
	}

	if(ms->formatdata)
	{
		free(ms->formatdata);
	}

	return 0;
}

static int one(const struct mark5_stream *ms)
{
	return 1;
}

struct mark5_format_generic *new_mark5_format_vlba(int Mbps, int nchan,
	int nbit, int fanout, int oversamp)
{
	struct mark5_format_generic *f;
	struct mark5_format_vlba *v;
	int decoderindex=0;
	int ntrack;

	ntrack = nchan*fanout*nbit;

	if(!modulate)
	{
		initmodulate();
		initluts();
	}

	if(oversamp == 1)
	{
		decoderindex += 0;
	}
	else if(oversamp == 2)
	{
		decoderindex += 42;
	}
	else if(oversamp == 4)
	{
		decoderindex += 84;
	}
	else
	{
		fprintf(stderr, "oversamp must be 1, 2 or 4\n");
	}

	if(nbit == 1)
	{
		decoderindex += 0;
	}
	else if(nbit == 2)
	{
		decoderindex += 21;
	}
	else
	{
		fprintf(stderr, "nbit must be 1 or 2\n");
		return 0;
	}

	if(ntrack == 1)
	{
		decoderindex += 0;
	}
	else if(ntrack == 2)
	{
		decoderindex += 3;
	}
	else if(ntrack == 4)
	{
		decoderindex += 6;
	}
	else if(ntrack == 8)
	{
		decoderindex += 9;
	}
	else if(ntrack == 16)
	{
		decoderindex += 12;
	}
	else if(ntrack == 32)
	{
		decoderindex += 15;
	}
	else if(ntrack == 64)
	{
		decoderindex += 18;
	}
	else
	{
		fprintf(stderr, "ntrack must be 2^n : n = 0..6\n");
		return 0;
	}

	if(fanout == 1)
	{
		decoderindex += 0;
	}
	else if(fanout == 2)
	{
		decoderindex += 1;
	}
	else if(fanout == 4)
	{
		decoderindex += 2;
	}
	else
	{
		fprintf(stderr, "fanout must be 1, 2 or 4\n");
		return 0;
	}

	v = (struct mark5_format_vlba *)calloc(1,
		sizeof(struct mark5_format_vlba));
	f = (struct mark5_format_generic *)calloc(1,
		sizeof(struct mark5_format_generic));

	v->ntrack = ntrack;
	v->fanout = fanout;
	v->kday = 0;

	f->Mbps = Mbps;
	f->nchan = nchan;
	f->nbit = nbit;
	f->formatdata = v;
	f->gettime = mark5_stream_frame_time_vlba;
	f->fixmjd = mark5_format_vlba_fixmjd;
	f->init_format = mark5_format_vlba_init;
	f->final_format = mark5_format_vlba_final;
	f->validate = one;
	f->oversamp = oversamp;
	switch(decoderindex)
	{
		case 0  : f->decode = vlba_decode_1bit_1track_fanout1_oversamp1; break;


		case 3  : f->decode = vlba_decode_1bit_2track_fanout1_oversamp1; break;
		case 4  : f->decode = vlba_decode_1bit_2track_fanout2_oversamp1; break;
		
		case 6  : f->decode = vlba_decode_1bit_4track_fanout1_oversamp1; break;
		case 7  : f->decode = vlba_decode_1bit_4track_fanout2_oversamp1; break;
		case 8  : f->decode = vlba_decode_1bit_4track_fanout4_oversamp1; break;
		case 9  : f->decode = vlba_decode_1bit_8track_fanout1_oversamp1; break;
		case 10 : f->decode = vlba_decode_1bit_8track_fanout2_oversamp1; break;
		case 11 : f->decode = vlba_decode_1bit_8track_fanout4_oversamp1; break;
		case 12 : f->decode = vlba_decode_1bit_16track_fanout1_oversamp1; break;
		case 13 : f->decode = vlba_decode_1bit_16track_fanout2_oversamp1; break;
		case 14 : f->decode = vlba_decode_1bit_16track_fanout4_oversamp1; break;
		case 15 : f->decode = vlba_decode_1bit_32track_fanout1_oversamp1; break;
		case 16 : f->decode = vlba_decode_1bit_32track_fanout2_oversamp1; break;
		case 17 : f->decode = vlba_decode_1bit_32track_fanout4_oversamp1; break;
		case 18 : f->decode = vlba_decode_1bit_64track_fanout1_oversamp1; break;
		case 19 : f->decode = vlba_decode_1bit_64track_fanout2_oversamp1; break;
		case 20 : f->decode = vlba_decode_1bit_64track_fanout4_oversamp1; break;
		
		
		
		case 24 : f->decode = vlba_decode_2bit_2track_fanout1_oversamp1; break;
		
		
		case 27 : f->decode = vlba_decode_2bit_4track_fanout1_oversamp1; break;
		case 28 : f->decode = vlba_decode_2bit_4track_fanout2_oversamp1; break;
		
		case 30 : f->decode = vlba_decode_2bit_8track_fanout1_oversamp1; break;
		case 31 : f->decode = vlba_decode_2bit_8track_fanout2_oversamp1; break;
		case 32 : f->decode = vlba_decode_2bit_8track_fanout4_oversamp1; break;
		case 33 : f->decode = vlba_decode_2bit_16track_fanout1_oversamp1; break;
		case 34 : f->decode = vlba_decode_2bit_16track_fanout2_oversamp1; break;
		case 35 : f->decode = vlba_decode_2bit_16track_fanout4_oversamp1; break;
		case 36 : f->decode = vlba_decode_2bit_32track_fanout1_oversamp1; break;
		case 37 : f->decode = vlba_decode_2bit_32track_fanout2_oversamp1; break;
		case 38 : f->decode = vlba_decode_2bit_32track_fanout4_oversamp1; break;
		case 39 : f->decode = vlba_decode_2bit_64track_fanout1_oversamp1; break;
		case 40 : f->decode = vlba_decode_2bit_64track_fanout2_oversamp1; break;
		case 41 : f->decode = vlba_decode_2bit_64track_fanout4_oversamp1; break;
		case 42 : f->decode = vlba_decode_1bit_1track_fanout1_oversamp1; break;
		
		
		case 45 : f->decode = vlba_decode_1bit_2track_fanout1_oversamp1; break;
		case 46 : f->decode = vlba_decode_1bit_2track_fanout2_oversamp1; break;
		
		case 48 : f->decode = vlba_decode_1bit_4track_fanout1_oversamp1; break;
		case 49 : f->decode = vlba_decode_1bit_4track_fanout2_oversamp1; break;
		case 50 : f->decode = vlba_decode_1bit_4track_fanout4_oversamp1; break;
		case 51 : f->decode = vlba_decode_1bit_8track_fanout1_oversamp1; break;
		case 52 : f->decode = vlba_decode_1bit_8track_fanout2_oversamp1; break;
		case 53 : f->decode = vlba_decode_1bit_8track_fanout4_oversamp1; break;
		case 54 : f->decode = vlba_decode_1bit_16track_fanout1_oversamp1; break;
		case 55 : f->decode = vlba_decode_1bit_16track_fanout2_oversamp1; break;
		case 56 : f->decode = vlba_decode_1bit_16track_fanout4_oversamp1; break;
		case 57 : f->decode = vlba_decode_1bit_32track_fanout1_oversamp1; break;
		case 58 : f->decode = vlba_decode_1bit_32track_fanout2_oversamp1; break;
		case 59 : f->decode = vlba_decode_1bit_32track_fanout4_oversamp1; break;
		case 60 : f->decode = vlba_decode_1bit_64track_fanout1_oversamp1; break;
		case 61 : f->decode = vlba_decode_1bit_64track_fanout2_oversamp1; break;
		case 62 : f->decode = vlba_decode_1bit_64track_fanout4_oversamp1; break;
		
		
		
		case 66 : f->decode = vlba_decode_2bit_2track_fanout1_oversamp1; break;
		
		
		case 69 : f->decode = vlba_decode_2bit_4track_fanout1_oversamp1; break;
		case 70 : f->decode = vlba_decode_2bit_4track_fanout2_oversamp1; break;
		
		case 72 : f->decode = vlba_decode_2bit_8track_fanout1_oversamp1; break;
		case 73 : f->decode = vlba_decode_2bit_8track_fanout2_oversamp1; break;
		case 74 : f->decode = vlba_decode_2bit_8track_fanout4_oversamp1; break;
		case 75 : f->decode = vlba_decode_2bit_16track_fanout1_oversamp1; break;
		case 76 : f->decode = vlba_decode_2bit_16track_fanout2_oversamp1; break;
		case 77 : f->decode = vlba_decode_2bit_16track_fanout4_oversamp1; break;
		case 78 : f->decode = vlba_decode_2bit_32track_fanout1_oversamp1; break;
		case 79 : f->decode = vlba_decode_2bit_32track_fanout2_oversamp1; break;
		case 80 : f->decode = vlba_decode_2bit_32track_fanout4_oversamp1; break;
		case 81 : f->decode = vlba_decode_2bit_64track_fanout1_oversamp1; break;
		case 82 : f->decode = vlba_decode_2bit_64track_fanout2_oversamp1; break;
		case 83 : f->decode = vlba_decode_2bit_64track_fanout4_oversamp1; break;
		case 84 : f->decode = vlba_decode_1bit_1track_fanout1_oversamp1; break;
		
		
		case 87 : f->decode = vlba_decode_1bit_2track_fanout1_oversamp1; break;
		case 88 : f->decode = vlba_decode_1bit_2track_fanout2_oversamp1; break;
		
		case 90 : f->decode = vlba_decode_1bit_4track_fanout1_oversamp1; break;
		case 91 : f->decode = vlba_decode_1bit_4track_fanout2_oversamp1; break;
		case 92 : f->decode = vlba_decode_1bit_4track_fanout4_oversamp1; break;
		case 93 : f->decode = vlba_decode_1bit_8track_fanout1_oversamp1; break;
		case 94 : f->decode = vlba_decode_1bit_8track_fanout2_oversamp1; break;
		case 95 : f->decode = vlba_decode_1bit_8track_fanout4_oversamp1; break;
		case 96 : f->decode = vlba_decode_1bit_16track_fanout1_oversamp1; break;
		case 97 : f->decode = vlba_decode_1bit_16track_fanout2_oversamp1; break;
		case 98 : f->decode = vlba_decode_1bit_16track_fanout4_oversamp1; break;
		case 99 : f->decode = vlba_decode_1bit_32track_fanout1_oversamp1; break;
		case 100: f->decode = vlba_decode_1bit_32track_fanout2_oversamp1; break;
		case 101: f->decode = vlba_decode_1bit_32track_fanout4_oversamp1; break;
		case 102: f->decode = vlba_decode_1bit_64track_fanout1_oversamp1; break;
		case 103: f->decode = vlba_decode_1bit_64track_fanout2_oversamp1; break;
		case 104: f->decode = vlba_decode_1bit_64track_fanout4_oversamp1; break;
		
		
		
		case 108: f->decode = vlba_decode_2bit_2track_fanout1_oversamp1; break;
		
		
		case 111: f->decode = vlba_decode_2bit_4track_fanout1_oversamp1; break;
		case 112: f->decode = vlba_decode_2bit_4track_fanout2_oversamp1; break;
		
		case 114: f->decode = vlba_decode_2bit_8track_fanout1_oversamp1; break;
		case 115: f->decode = vlba_decode_2bit_8track_fanout2_oversamp1; break;
		case 116: f->decode = vlba_decode_2bit_8track_fanout4_oversamp1; break;
		case 117: f->decode = vlba_decode_2bit_16track_fanout1_oversamp1; break;
		case 118: f->decode = vlba_decode_2bit_16track_fanout2_oversamp1; break;
		case 119: f->decode = vlba_decode_2bit_16track_fanout4_oversamp1; break;
		case 120: f->decode = vlba_decode_2bit_32track_fanout1_oversamp1; break;
		case 121: f->decode = vlba_decode_2bit_32track_fanout2_oversamp1; break;
		case 122: f->decode = vlba_decode_2bit_32track_fanout4_oversamp1; break;
		case 123: f->decode = vlba_decode_2bit_64track_fanout1_oversamp1; break;
		case 124: f->decode = vlba_decode_2bit_64track_fanout2_oversamp1; break;
		case 125: f->decode = vlba_decode_2bit_64track_fanout4_oversamp1; break;
		default:  f->decode = 0;
	}

	if(f->decode == 0)
	{
		fprintf(stderr, "Illegal combination of fanout, tracks and bits\n");
		return 0;
	}

	return f;
}
