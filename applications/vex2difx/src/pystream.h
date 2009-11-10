#ifndef __PY_STREAM_H__
#define __PY_STREAM_H__

#include <fstream>
#include <string>
#include "vextables.h"

class pystream : public ofstream
{
private:
	string ant;
	string sw[4];	// 4x4 switch state
	string obsCode;
	double lastValid;
	int lastSourceId;
public:
	void open(string antennaName);
	void close();
	int writeHeader(const VexData *V);
	int writeRecorderInit(const VexData *V);
	int writeSourceTable(const VexData *V);
	int writeScans(const VexData *V);
};

#endif
