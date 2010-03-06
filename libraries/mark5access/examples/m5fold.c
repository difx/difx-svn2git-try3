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
// $Id: m5spec.c 1989 2010-02-26 17:37:16Z WalterBrisken $
// $HeadURL: https://svn.atnf.csiro.au/difx/libraries/mark5access/trunk/mark5access/mark5_stream.c $
// $LastChangedRevision: 1989 $
// $Author: WalterBrisken $
// $LastChangedDate: 2010-02-26 10:37:16 -0700 (Fri, 26 Feb 2010) $
//
//============================================================================
#include <complex.h>
#include <stdlib.h>
#include <complex.h>
#include <math.h>
#include "../mark5access/mark5_stream.h"

const char program[] = "m5fold";
const char author[]  = "Walter Brisken";
const char version[] = "0.1";
const char verdate[] = "2010 Mar 6";

int usage(const char *pgm)
{
	printf("\n");

	printf("%s ver. %s   %s  %s\n\n", program, version, author, verdate);
	printf("A Mark5 power folder.  Can use VLBA, Mark3/4, and Mark5B "
		"formats using the\nmark5access library.\n\n");
	printf("Usage : %s <infile> <dataformat> <nbin> <nint> <freq> <outfile> [<offset>]\n\n", program);
	printf("  <infile> is the name of the input file\n\n");
	printf("  <dataformat> should be of the form: "
		"<FORMAT>-<Mbps>-<nchan>-<nbit>, e.g.:\n");
	printf("    VLBA1_2-256-8-2\n");
	printf("    MKIV1_4-128-2-1\n");
	printf("    Mark5B-512-16-2\n\n");
	printf("  <nchan> is the number of channels to make per IF\n\n");
	printf("  <nint> is the number of FFT frames to spectrometize\n\n");
	printf("  <outfile> is the name of the output file\n\n");
	printf("  <offset> is number of bytes into file to start decoding\n\n");

	return 0;
}

int fold(const char *filename, const char *formatname, int nbin, int nint,
	double freq, const char *outfile, long long offset)
{
	struct mark5_stream *ms;
	double **data, **bins;
	int **weight;
	int c, i, j, k, status;
	int chunk, nif, bin;
	long long total, unpacked;
	FILE *out;
	double R;
	long long sampnum = 0;

	chunk = 10000;

	total = unpacked = 0;

	ms = new_mark5_stream(
		new_mark5_stream_file(filename, offset),
		new_mark5_format_generic_from_string(formatname) );

	if(!ms)
	{
		printf("problem opening %s\n", filename);
		return 0;
	}

	mark5_stream_print(ms);

	out = fopen(outfile, "w");
	if(!out)
	{
		fprintf(stderr, "Error -- cannot open %s for write\n", outfile);
		delete_mark5_stream(ms);

		return 0;
	}

	R = nbin*freq/ms->samprate;

	nif = ms->nchan;

	data = (double **)malloc(nif*sizeof(double *));
	bins = (double **)malloc(nif*sizeof(double *));
	weight = (int **)malloc(nif*sizeof(double *));
	for(i = 0; i < nif; i++)
	{
		data[i] = (double *)malloc(chunk*sizeof(double));
		bins[i] = (double *)malloc(nbin*sizeof(double));
		weight[i] = (int *)malloc(nbin*sizeof(int));
	}

	for(j = 0; j < nint; j++)
	{
		status = mark5_stream_decode_double(ms, chunk, data);
		
		if(status < 0)
		{
			break;
		}
		else
		{
			total += chunk;
			unpacked += status;
		}

		if(ms->consecutivefails > 5)
		{
			printf("Too many failures.  consecutive, total fails = %d %d\n", ms->consecutivefails, ms->nvalidatefail);
			break;
		}

		for(k = 0; k < chunk; k++)
		{
			bin = (int)(sampnum*R) % nbin;
			for(i = 0; i < nif; i++)
			{
				bins[i][bin] += data[i][k]*data[i][k];
				weight[i][bin]++;
			}
			sampnum++;
		}
	}

	fprintf(stderr, "%Ld / %Ld samples unpacked\n", unpacked, total);

	/* normalize */
	for(k = 0; k < nbin; k++)
	{
		for(i = 0; i < nif; i++)
		{
			if(weight[i][k]) bins[i][k] /= weight[i][k];
		}
	}

	for(c = 0; c < nbin; c++)
	{
		fprintf(out, "%d ", c);
		for(i = 0; i < nif; i++)
		{
			fprintf(out, " %f", bins[i][c]);
		}
		fprintf(out, "\n");
	}

	fclose(out);

	for(i = 0; i < nif; i++)
	{
		free(data[i]);
		free(bins[i]);
		free(weight[i]);
	}
	free(data);
	free(bins);
	free(weight);

	delete_mark5_stream(ms);

	return 0;
}

int main(int argc, char **argv)
{
	long long offset = 0;
	int nbin, nint;
	double freq;
	int v;

	if(argc == 2)
	{
		struct mark5_format *mf;
		int bufferlen = 1<<11;
		char *buffer;
		FILE *in;

		buffer = malloc(bufferlen);
		
		in = fopen(argv[1], "r");
		v = fread(buffer, bufferlen, 1, in);
		if(v == 0)
		{
			fprintf(stderr, "Not enough data in file\n");
			free(buffer);

			return 0;
		}

		mf = new_mark5_format_from_stream(
			new_mark5_stream_memory(buffer, bufferlen/2));

		print_mark5_format(mf);
		delete_mark5_format(mf);

		mf = new_mark5_format_from_stream(
			new_mark5_stream_memory(buffer, bufferlen/2));

		print_mark5_format(mf);
		delete_mark5_format(mf);

		free(buffer);

		fclose(in);

		return 0;
	}

	else if(argc < 6)
	{
		return usage(argv[0]);
	}

	nbin = atol(argv[3]);
	nint = atol(argv[4]);
	freq = atof(argv[5]);
	if(nint <= 0)
	{
		nint = 2000000000L;
	}

	if(argc > 7)
	{
		offset=atoll(argv[7]);
	}

	fold(argv[1], argv[2], nbin, nint, freq, argv[6], offset);

	return 0;
}

