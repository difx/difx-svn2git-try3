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

#ifndef __AUTOBANDS_H__
#define __AUTOBANDS_H__

#include <string>
#include <vector>
#include <iostream>
#include <limits>

class ZoomFreq; // declared in corrparams.h
class freq;     // declared in freq.h

class AutoBands
{
public:
	//constructors
	AutoBands(double outputbandwidth_Hz, int verbosity=0, bool permitgaps=false);
	AutoBands() : outputbandwidth(32e6), verbosity(0), permitgaps(false) { }
	~AutoBands();

	//types
	class Band {
	public:
		double flow;
		double fhigh;
		int antenna;

		Band(double flow_, double fhigh_, int antenna_)
			: flow(flow_),fhigh(fhigh_),antenna(antenna_) { }
		double bandwidth() const { return fhigh-flow; }
		friend std::ostream& operator << (std::ostream &os, const AutoBands::Band &x);
	};

	class Span {
	public:
		double flow;
		double fhigh;
		int antennacount;
		int bandcount;
		bool continued;

		Span(double flow_, double fhigh_, int antcount_, int bandcount_)
			: flow(flow_),fhigh(fhigh_),antennacount(antcount_),bandcount(bandcount_),continued(false) { }
		static bool compare_bandwidths(const AutoBands::Span& lhs, const AutoBands::Span& rhs);
		double bandwidth() const { return fhigh-flow; }
		friend std::ostream& operator << (std::ostream &os, const AutoBands::Span &x);
	};

	class Outputband {
	public:
		double fbandstart;
		std::vector<Band> constituents;

		Outputband(double fbandstart_) : fbandstart(fbandstart_)
		{
			this->constituents.clear();
		}
		void extend(double fstart, double bw)
		{
			this->constituents.push_back(Band(fstart, fstart+bw, 0));
		}
		friend std::ostream& operator << (std::ostream &os, const AutoBands::Outputband &x);
	};

public:
	//methods
	double autoBandwidth();
	void setBandwidth(double outputbandwidth_Hz);

	void addRecbands(const std::vector<double>& fstart, const std::vector<double>& fstop, int antId = -1);
	void addRecbands(const std::vector<freq>& freqs, int antId = -1);

	int generate(int Nant=0, double fstart_Hz=0.0);
	double granularity(const std::vector<double>& freqs) const;

	//variables
	std::vector<Band> bands;
	std::vector<Span> spans;
	std::vector<Outputband> outputbands;
	std::vector<ZoomFreq> outputzooms;
	double minrecfreq, maxrecfreq;
	unsigned int Nant;
	double outputbandwidth;
	int verbosity;
	bool permitgaps;

private:
	//methods
	bool covered(double f0, double f1) const;
	void analyze(int Nant=0);
	void simplify(Outputband& mergeable);

	double adjustStartFreq(double f0, double f1, double finitial, double df);

	void barchart(std::ostream& os,
		const std::vector<double>& start,
		const std::vector<double>& stop,
		const std::vector<int>& idtag,
		const int startmarker='a',
		double xmin=std::numeric_limits<double>::quiet_NaN(),
		double xmax=std::numeric_limits<double>::quiet_NaN()) const;

private:
	friend std::ostream& operator << (std::ostream &os, const AutoBands &x);

};

std::ostream& operator << (std::ostream& os, const AutoBands& x);
std::ostream& operator << (std::ostream& os, const AutoBands::Band& x);
std::ostream& operator << (std::ostream& os, const AutoBands::Span& x);
std::ostream& operator << (std::ostream& os, const AutoBands::Outputband& x);

#endif
