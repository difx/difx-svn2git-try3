/***************************************************************************
 *   Copyright (C) 2009-2012 by Walter Brisken, Adam Deller                *
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

#ifndef __PY_STREAM_H__
#define __PY_STREAM_H__

#include <fstream>
#include <string>
#include "vextables.h"
#include "vlbadefaults.h"

class pystream : public ofstream
{
public:
	enum ScriptType {SCRIPT_VLBA, SCRIPT_EVLA, SCRIPT_GBT};
	enum PersonalityType {RDBE_UNKNOWN, RDBE_NONE, RDBE_PFB, RDBE_DDC};
	enum RecorderType {RECORDER_NONE, RECORDER_MARK5C};

	pystream(): scriptType(SCRIPT_VLBA), personalityType(RDBE_UNKNOWN), recorderType(RECORDER_MARK5C), lastValid(0.0), lastSourceId(-1), lastModeId(-1), lastChannelSet(-1), evlaIntSec(0), evlasbChan(0), evlasbBits(0), evlaVCIVersion(0.0), mjd0(0.0), isMark5A(false) {};
	void open(const string& antennaName, const VexData *V, ScriptType sType);
	void close();
	void addPhasingSource(const string &sourceName);
	void figurePersonality(const VexData *V);
	int maxIFs(const VexData *V) const;
	int writeHeader(const VexData *V);
	int writeComment(const string &commentString);
	int writeRecorderInit(const VexData *V);
	int writeDbeInit(const VexData *V);
	void writeImplicitConversionComment(const vector<unsigned int> &implicitConversions);
	int writeChannelSet(const VexSetup *setup, int modeNum);
	int writeDDCChannelSet(const VexSetup *setup, int modeNum);
	int writeChannelSet5A(const VexSetup *setup, int modeNum);
	int writeLoifTable(const VexData *V);
	int writeDDCLoifTable(const VexData *V);
	int writeSourceTable(const VexData *V);
	int writeDDCSourceTable(const VexData *V);
	int writeScans(const VexData *V);
	int writeDDCScans(const VexData *V);
	int writeScansGBT(const VexData *V);
        void setDBEPersonality(const string &filename);
	void setDBEPersonalityType(PersonalityType pt) { personalityType = pt; }
	PersonalityType getDBEPersonalityType() { return personalityType; }
	void setRecorderType(RecorderType rt) { recorderType = rt; }
	void setMark5A(bool x) { isMark5A = x; }

private:
	ScriptType scriptType;
	PersonalityType personalityType;
	RecorderType recorderType;
	string evlaVCIDir;
	string ant;
	string sw[4];   // 4x4 switch state
	string obsCode;
	string fileName;
	string dbeFilename;
	double lastValid;
	int lastSourceId;
	int lastModeId;
	int lastChannelSet;
	int evlaIntSec, evlasbChan, evlasbBits;
	double evlaVCIVersion;
	double mjd0;
	bool isMark5A;
	vector<map<string,unsigned int> > ifIndex;	// for each scan, map from IF name to number
	vector<string> phasingSources;

	void calcIfIndex(const VexData *V);
	void writeVCI(const VexData *V, int modeIndex, const string &filename);

	// determine how many DBEs we will need
	int numDBE;
	int numDBEChan;
    char dbeAppend[2]; // {'a'|'b'} '\0'
	// which modes need 2DBEs by mode
    vector<bool> need2DBEbyMode;
	// how many channels per IF by mode
	vector<vector<int> > chanPerIF;
	// how many IF in use by mode
	vector<int> IFinUse;
	// IF numbers ordered by how many channels are assigned to each; highest at index 0; by mode
	vector<vector<int> > orderedIFNums;
	// which IF is assigned to which DBE/IF by mode
    vector<vector<vector<int> > > IF2DBEassign;
	// how many IF are in use by DBE and by mode
    vector<vector<int> > IFinUseByDBE;
	// if IF has more than MAX_IF_PER_DBE channels assigned this tells us how to split them across DBEs
	vector<vector<int> > chanByDBE;
	// with which channel distribution are we going?
	vector<int> chanDist;
	// channels assigned to DBEs
	vector<vector<int> > chanAssign;
};

#endif
