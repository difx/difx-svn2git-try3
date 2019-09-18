/***************************************************************************
 *   Copyright (C) 2009-2017 by Walter Brisken & Adam Deller               *
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
 * $Id: $
 * $HeadURL: $
 * $LastChangedRevision: $
 * $Author: $
 * $LastChangedDate: $
 *
 *==========================================================================*/

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <vector>
#include <set>

#include "autobands.h"
#include "freq.h"       // class freq
#include "zoomfreq.h"	// class ZoomFreq

////////////////////////////////////////////////////////////////////////////////////////////////////////////

double AutoBands::Band::bandwidth() const
{
	return fhigh - flow;
}

double AutoBands::Span::bandwidth() const
{
	return fhigh - flow;
}

bool AutoBands::Band::operator==(const freq& rhs) const
{
	return (rhs.fq == flow) && (rhs.bw == (fhigh-flow)) && (rhs.sideBand == 'U'); 
}

bool AutoBands::Outputband::operator==(const freq& rhs) const
{
	return (rhs.fq == fbandstart) && (rhs.bw == bandwidth) && (rhs.sideBand == 'U');
}

void AutoBands::Outputband::extend(double fstart, double bw)
{
	constituents.push_back(AutoBands::Band(fstart, fstart+bw, 0));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

AutoBands::AutoBands(double outputbandwidth_Hz, int verbosity, bool permitgaps)
{
	this->outputbandwidth = outputbandwidth_Hz;
	this->verbosity = verbosity;
	this->permitgaps = permitgaps;
	this->Nant = 0;
}

AutoBands::~AutoBands()
{
}

/**
 * Set the bandwidth (Hz) of the desired outputbands.
 */
void AutoBands::setBandwidth(double outputbandwidth_Hz)
{
	outputbandwidth = outputbandwidth_Hz;
	if(outputbandwidth < 1e6)
	{
		outputbandwidth  *= 1e6;
	}
}

/**
 * Add information about the recorded bands of an antenna.
 */
void AutoBands::addRecbands(const std::vector<double>& fstart, const std::vector<double>& fstop, int antId)
{
	assert(fstart.size() == fstop.size());
	if(antId == -1)
	{
		antId = Nant;
	}
	for(unsigned i = 0; i < fstart.size(); i++)
	{
		double lo = fstart[i], hi = fstop[i];
		if(lo > hi)
		{
			std::swap(lo, hi);
		}
		bands.push_back(AutoBands::Band(lo, hi, antId));
	}
	Nant++;
}

/**
 * Add information about the recorded bands of an antenna.
 */
void AutoBands::addRecbands(const std::vector<freq>& freqs, int antId)
{
	if(antId == -1)
	{
		antId = Nant;
	}
	for(unsigned i = 0; i < freqs.size(); i++)
	{
		double lo = freqs[i].fq;
		double hi = freqs[i].fq + ((freqs[i].sideBand == 'U') ? freqs[i].bw : -freqs[i].bw);
		if(lo > hi)
		{
			std::swap(lo, hi);
		}
		bands.push_back(AutoBands::Band(lo, hi, antId));
	}
	Nant++;
}

/**
 * Return the greatest-common-divisor for list of frequencies
 */
double AutoBands::getGranularity(const std::vector<double>& args) const
{
	if(args.size() < 1)
	{
		return 1.0;
	}

	double curr_gcd = args[0];
	for(unsigned i = 1; i < args.size(); i++)
	{
		double arg = args[i];
		if(arg < curr_gcd)
		{
			std::swap(arg, curr_gcd);
		}
		while (1)
		{
			if(std::fabs(curr_gcd) < 0.001)
			{
				curr_gcd = arg;
				break;
			}
			double rem = arg - std::floor(arg / curr_gcd) * curr_gcd;
			arg = curr_gcd;
			curr_gcd = rem;
		}

	}

	// TODO: perhaps there is a tidier way for GCD in C++ than the above

	return curr_gcd;
}

/**
 * Return automatically determined best-fit common bandwidth, or 0 on failure
 */
double AutoBands::autoBandwidth()
{
	if(spans.empty())
	{
		this->analyze();
	}

	if(spans.empty())
	{
		if(verbosity > 1)
		{
			std::cout << "AutoBands::autoBandwidth(): Warning: Not enough data to determine the common bandwidth.\n";
		}
		return 0.0;
	}

	const AutoBands::Span& m = (*std::min_element(spans.begin(), spans.end(), AutoBands::Span::compare_bandwidths));
	const AutoBands::Span& M = (*std::max_element(spans.begin(), spans.end(), AutoBands::Span::compare_bandwidths));
	if(m.bandwidth() == M.bandwidth())
	{
		return m.bandwidth();
	}
	return 0.0;
}

/**
 * Check if freq range falls in its entirety inside any of the recorded bands, at *all* antennas
 */
bool AutoBands::covered(double f0, double f1) const
{
	std::set<int> coverers;
	for(std::vector<AutoBands::Band>::const_iterator b = bands.begin(); b != bands.end(); ++b)
	{
		if(((*b).flow <= f0) && (f1 <= (*b).fhigh))
		{
			coverers.insert((*b).antenna);
		}
	}
	return (coverers.size() >= Nant);
}

/**
 * Simplify an outputband definition by merging its list of input zooms as far as possible i.e. when zooms are from overlapped recorded bands.
 */
void AutoBands::simplify(Outputband& mergeable)
{
	assert(!mergeable.constituents.empty());

	// Start from blank outputband
	Outputband merged(mergeable.fbandstart, mergeable.bandwidth);
	double f0 = mergeable.fbandstart;
	double f1 = f0;

	// Simplify bands in 'mergeable' into hopefully fewer bands in 'merged'
	for(std::vector<AutoBands::Band>::const_iterator bnext = (mergeable.constituents.begin()) + 1; bnext != mergeable.constituents.end(); ++bnext)
	{
		f1 = (*bnext).fhigh;
		if(covered(f0, f1))
		{
			if(verbosity > 4)
			{
				printf("Autobands::simplify() does cover %.6f--%.6f, continue and try merge the next band constituent\n", f0*1e-6, f1*1e-6);
			}
		}
		else
		{
			if(verbosity > 4)
			{
				printf("Autobands::outputbands_merge() no   cover %.6f--%.6f\n", f0*1e-6, f1*1e-6);
			}
			merged.extend(f0, (*bnext).flow - f0);
			f0 = (*bnext).flow;
			if((bnext + 1) != mergeable.constituents.end())
			{
				// set f1 ahead of exiting loop next
				f1 = (*(bnext + 1)).fhigh;
			}
		}
	}

	// Handle leftover
	if(f1 > f0)
	{
		merged.extend(f0, f1-f0);
	}

	// Print the details if something was merged
	if((verbosity > 1) && (merged.constituents.size() < mergeable.constituents.size()))
	{
		std::cout << "Autobands::outputbands()    merged " << mergeable.constituents.size()
			<< " sub-spans into " << merged.constituents.size() << "\n";
	}

	// Store the result
	mergeable = merged;

}

/**
 * Determine frequency spans where at least Nant antennas overlap
 * Spans are the frequency axis split at the band edges of recorded bands of every antenna.
 * Spans without freq gaps to the next span are marked as continued.
 */
void AutoBands::analyze(int Nant)
{
	// Init
	if(Nant <= 0)
	{
		Nant = this->Nant;
	}
	spans.clear();

	// Sorted set of all recorded band edges
	std::set<double> bandedges;
	for(unsigned i = 0; i < bands.size(); i++)
	{
		bandedges.insert(bands[i].flow);
		bandedges.insert(bands[i].fhigh);
	}

	// Convert band edge set into a list, to be able to index it
	std::vector<double> fedges;
	std::copy(bandedges.begin(), bandedges.end(), std::back_inserter(fedges));

	// Split the freq axis into spans (smallest band slices between any two rec band edges)
	for(unsigned n = 1; n < fedges.size(); n++)
	{
		double span_lo = fedges[n-1], span_hi = fedges[n];

		// Count how many antennas have this span
		int antcount = 0, bandcount = 0;
		std::set<int> antennasInSpan;
		for(unsigned k = 0; k < bands.size(); k++)
		{
			if(span_lo >= bands[k].flow && span_hi <= bands[k].fhigh)
			{
				antennasInSpan.insert(bands[k].antenna);
				bandcount++;
			}
		}
		antcount = antennasInSpan.size();

		// Keep the span if enough antennas provide data for it
		if(antcount >= Nant && bandcount >= Nant)
		{
			spans.push_back(AutoBands::Span(span_lo, span_hi, antcount, bandcount));
			if(verbosity > 2)
			{
				printf ("Autobands::analyze() retain  %.6f--%.6f MHz bw %.6f MHz with %d rec bands, %d antennas\n", span_lo*1e-6,span_hi*1e-6, (span_hi-span_lo)*1e-6, bandcount,antcount);
			}
		}
		else if(verbosity > 2)
		{
			printf ("Autobands::analyze() discard %.6f--%.6f MHz bw %.6f MHz with %d rec bands, %d antennas < %d antennas\n", span_lo*1e-6,span_hi*1e-6,(span_hi-span_lo)*1e-6,bandcount,antcount,Nant);
		}
	}

	// Mark directly adjecent spans as 'continued'; last entry defaults to .continued=False
	for(unsigned n = 1; n < spans.size(); n++)
	{
		spans[n-1].continued = (spans[n-1].fhigh == spans[n].flow);
	}
}

/**
 * Produce a set of output bands and a list of their input bands.
 * Utilizes internally stored previously added information of antenna recorded frequencies.
 * Output bands have the requested bandwidth. They can be direct matches to recorded bands, band slices (zoom) of recorded bands,
 * or pieces of several band slices (zoom sets) taken from neighbouring recorded bands.
 */
int AutoBands::generateOutputbands(int Nant, double fstart_Hz)
{
	// Clear old results
	outputbands.clear();

	// Make sure that spans have been detected
	if(spans.empty())
	{
		analyze(Nant);
	}
	if(spans.empty())
	{
		return 0;
	}

	// Parameters
	assert (outputbandwidth > 0);
	const unsigned Nspans = spans.size();
	const double minspanfreq = spans[0].flow;
	unsigned span = 0;
	double foutstart;
	if(Nant <= 0)
	{
		Nant = this->Nant;
	}
	if(fstart_Hz > 0)
	{
		foutstart = std::max(fstart_Hz, minspanfreq);
	}
	else
	{
		foutstart = minspanfreq;
	}

	// Assemble output bands using recorded bands in full or in pieces
	while (span < Nspans)
	{
		double f0 = spans[span].flow;
		double f1 = spans[span].fhigh;

		// Catch when large gaps between spans, e.g., geodetic/DDC mode
		if(foutstart < f0)
		{
			foutstart = f0;
		}
		else if(f1 < foutstart)
		{
			span++;
			continue;
		}

		// Shift start to fall on a 'more integer' MHz if possible
		// TODO

		// Generate band : insert as many bands into current span as possible
		while ((foutstart + outputbandwidth) <= f1)
		{
			if(verbosity > 1)
			{
				printf ("Autobands::outputbands()    case 1 adding %10.6f MHz bw from span %3d @ %10.6f MHz to out#%d %10.6f MHz\n",
					outputbandwidth*1e-6, span, (foutstart-f0)*1e-6, (int)outputbands.size(), foutstart*1e-6);
			}

			AutoBands::Outputband ob(foutstart, outputbandwidth);
			ob.extend(foutstart, outputbandwidth);

			outputbands.push_back(ob);
			foutstart += outputbandwidth;
		}

		// Generate band : insert one band by combining smaller pieces of spans and patch over to next span(s) if needed
		double span_bw_remain = f1 - foutstart;
		if((span_bw_remain > 0) && spans[span].continued)
		{

			// If overlap in rec bands at least at one antenna, may
			// try a slight shift to begin at a more 'integer' MHz
			// TODO

			// Now piece together 'self.outputbw' amout of band from consecutive spans
			AutoBands::Outputband ob(foutstart, outputbandwidth);
			double bw_needed = outputbandwidth;
			double slicestartfreq = foutstart;
			while ((span < Nspans) && (bw_needed > 0))
			{
				double bw_utilized;
				if((slicestartfreq - f0) < 0)
				{
					break;
				}
				bw_utilized = std::min(bw_needed, span_bw_remain);
				bw_needed -= bw_utilized;
				if(verbosity > 1)
				{
					printf ("Autobands::outputbands()    case 2 adding %10.6f MHz bw from span %3d @ %10.6f MHz to out#%d %10.6f MHz, remain %10.6f MHz\n",
						bw_utilized*1e-6, span, (slicestartfreq-f0)*1e-6, (int)outputbands.size(), foutstart*1e-6, bw_needed*1e-6);
				}

				// Add bandwidth to outputband
				ob.extend(slicestartfreq, bw_utilized);
				slicestartfreq += bw_utilized;
				span_bw_remain = f1 - slicestartfreq;

				// If out of remaining bw in this span, overflow into the next span
				if(span_bw_remain <= 0)
				{
					if(!spans[span].continued)
					{
						break;
					}
					span++;
					if(span < Nspans)
					{
						f0 = spans[span].flow;
						f1 = spans[span].fhigh;
						span_bw_remain = f1 - slicestartfreq;
						assert(span_bw_remain >= 0);
					}
				}

			}

			// Store the details of the completed outputband
			if(bw_needed <= 0)
			{
				simplify(ob);
				outputbands.push_back(ob);
				foutstart += outputbandwidth;
			}
			else
			{
				if(verbosity > 1)
				{
					printf ("Autobands::outputbands()    dropping incomplete out fq %.6f MHz\n", foutstart*1e-6);
				}
				foutstart += bw_needed;
				// span++; // perhaps?
			}


		}
		else
		{
			// No band remains in current span
			span++;
		}

	}

	return true;
}

/**
 * Look through the internal list of output bands and search for the first
 * output band that contains 'freq' as one of its constituents.
 *
 * Once the output band of 'freq' has been determined, looks through
 * the list of frequencies 'allfreqs' and locates a match for that output
 * band. Returns the index of that match is returned.
 *
 * If any of the two search stages fails to locate a frequency, the search
 * is repeated with a sideband flipped band having the same sky coverage.
 */
int AutoBands::lookupDestinationFreq(const freq& inputfreq, const std::vector<freq>& allfreqs) const
{
	int outputband_index = -1;

	// Find 'inputfreq' in constituents
	for(unsigned n = 0; n < outputbands.size() && outputband_index < 0; n++)
	{
		const AutoBands::Outputband& ob = outputbands[n];
		for(unsigned m = 0; m < ob.constituents.size(); m++)
		{
			if (ob.constituents[m] == inputfreq)
			{
				outputband_index = n;
				break;
			}
		}
	}

	// Or, find band-flipped 'inputfreq' in constituents
	if (outputband_index < 0)
	{
		freq flipped = inputfreq;
		flipped.flip();
		for(unsigned n = 0; n < outputbands.size() && outputband_index < 0; n++)
		{
			const AutoBands::Outputband& ob = outputbands[n];
			for(unsigned m = 0; m < ob.constituents.size(); m++)
			{
				if (ob.constituents[m] == flipped)
				{
					outputband_index = n;
					break;
				}
			}
		}
	}

	if (outputband_index < 0)
	{
		return -1;
	}

	const AutoBands::Outputband& destination = outputbands[outputband_index];
	for(unsigned n = 0; n < allfreqs.size(); n++)
	{
		if (destination == allfreqs[n])
		{
			return n;
		}
	}

	for(unsigned n = 0; n < allfreqs.size(); n++)
	{
		freq flipped = allfreqs[n];
		flipped.flip();
		if (destination == flipped)
		{
			return n;
		}
	}

	return -1;
}

void AutoBands::barchart(
	std::ostream& os,
	const std::vector<double>& start, const std::vector<double>& stop,
	const std::vector<int>& idtag, const int startmarker,
	double xmin, double xmax) const
{
	const int screenwidth = 110;

	if(std::isnan(xmin))
	{
		xmin = *std::min_element(start.begin(), start.end());
	}
	if(std::isnan(xmax))
	{
		xmax = *std::max_element(stop.begin(), stop.end());
	}

	const double fstep = (xmax - xmin) / screenwidth;
	for(unsigned k = 0; k < start.size(); k++)
	{
		double bw = stop[k] - start[k];
		int L0 = std::ceil((start[k] - xmin)/fstep);
		int L1 = std::ceil(bw/fstep);
		int L2 = screenwidth - L1 - L0;
		if(L2 < 0)
		{
			L1 += L2;
			L2 = 0;
		}
		int id = startmarker + idtag[k]%26;
		os << std::string(L0, '.') << std::string(L1, id) << std::string(L2, '.') << "\n";
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Function for std::min_element and others for sorting Span's
bool AutoBands::Span::compare_bandwidths(const AutoBands::Span& lhs, const AutoBands::Span& rhs)
{
	return (lhs.bandwidth() < rhs.bandwidth());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::ostream& operator << (std::ostream& os, const AutoBands& x)
{
	// Show recorded bands as a bar chart
	os << "Recorded bands:\n";
	std::vector<double> xmin, xmax;
	std::vector<int> id;
	for(unsigned k = 0; k < x.bands.size(); k++)
	{
		xmin.push_back(x.bands[k].flow);
		xmax.push_back(x.bands[k].fhigh);
		id.push_back(x.bands[k].antenna);
	}
	x.barchart(os, xmin, xmax, id);

	// Show auto-detected usable spans as a bar chart
	os << "Detected spans with array-wide common cover:\n";
	xmin.clear();
	xmax.clear();
	id.clear();
	for(unsigned k = 0; k < x.spans.size(); k++)
	{
		xmin.push_back(x.spans[k].flow);
		xmax.push_back(x.spans[k].fhigh);
		id.push_back(k);
	}
	x.barchart(os, xmin, xmax, id, 'A');

	// Show auto-detected usable spans numerically / listed
	std::copy(x.spans.begin(), x.spans.end(), std::ostream_iterator<AutoBands::Span>(os, "\n"));

	// Show output bands and how they are assembled
	os << "Generated " << x.outputbands.size() << " output bands\n";
	std::copy(x.outputbands.begin(), x.outputbands.end(), std::ostream_iterator<AutoBands::Outputband>(os, ""));

	return os;
}

std::ostream& operator << (std::ostream& os, const AutoBands::Band& x)
{
	os << std::fixed
		<< "start at " << std::setw(15) << std::setprecision(8) << (x.flow*1e-6) << " MHz with bw "
		<< std::setw(11) << std::setprecision(7) << (x.bandwidth()*1e-6) << " MHz";
	return os;
}

std::ostream& operator << (std::ostream& os, const AutoBands::Span& x)
{
	os << "Span " << std::fixed
		<< std::setw(15) << std::setprecision(8) << (x.flow*1e-6)  << " -- "
		<< std::setw(15) << std::setprecision(8) << (x.fhigh*1e-6) << " MHz, "
		<< std::setw(11) << std::setprecision(7) << (x.bandwidth()*1e-6) << " MHz bw, "
		<< x.bandcount << " rec.bands, " << (x.continued ? "contig" : "gap");
	return os;
}

std::ostream& operator << (std::ostream& os, const AutoBands::Outputband& x)
{
	os << "Output band at " << std::fixed
		<< std::setw(15) << std::setprecision(8) << (x.fbandstart*1e-6)  << "\n";
	for(unsigned n = 0; n < x.constituents.size(); n++)
	{
		os << "   input " << std::setw(2) << n << " " << x.constituents[n] << "\n";
	}
	return os;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
