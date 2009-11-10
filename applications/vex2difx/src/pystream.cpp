#include "pystream.h"

void pystream::open(string antennaName)
{
	ofstream::open(antennaName.c_str());
	ant = antennaName;
	lastValid = 0.0;
	lastSourceId = -1;
	obsCode = string("VLBI");
}

void pystream::close()
{
	int p;

	p = precision();
	precision(14);

	if(lastValid != 0.0)
	{
		*this << "array.wait(" << (lastValid + 1.0/86400.0) << ")" << endl;
	}

	precision(p);

	ofstream::close();
}

int pystream::writeHeader(const VexData *V)
{
	obsCode = V->getExper()->name;

	*this << "obsCode = \"" << obsCode << "\"" << endl;
	*this << "stnCode = \"" << ant << "\"" << endl;
	*this << endl;

	return 0;
}

int pystream::writeRecorderInit(const VexData *V)
{
	// For now, set up single recorder in Mark5B mode

	*this << "recorder0 = Mark5C(-1)" << endl;
	*this << "recorder0.setMode(\"Mark5B\")" << endl;
	*this << "subarray.setRecorder(recorder0)" << endl;
	*this << endl;

	return 1;
}

int pystream::writeSourceTable(const VexData *V)
{
	int nSource;
	int p;
	const VexSource *S;

	nSource = V->nSource();

	p = precision();
	precision(15);

	for(int s = 0; s < nSource; s++)
	{
		S = V->getSource(s);
		*this << "source" << s << " = Source(" << S->ra << ", " << S->dec << ")" << endl;
		*this << "source" << s << ".setName(\"" << S->name << "\")" << endl;
		*this << endl;
	}

	precision(p);

	return nSource;
}

int pystream::writeScans(const VexData *V)
{
	int p;
	int n = 0;
	int nScan;
	const VexInterval *arange;
	const VexScan *scan;
	const VexMode *mode;
	int ifmap[4];
	int nif;
	const string VLBAIFName[4] = {"A", "B", "C", "D"};
	vector<VexSubband>::const_iterator it;

	nScan = V->nScan();

	p = precision();
	precision(14);

	for(int s = 0; s < nScan; s++)
	{
		scan = V->getScan(s);
		*this << "# Scan " << s << " = " << scan->name << endl;
		if(scan->stations.count(ant) == 0)
		{
			*this << "# Antenna " << ant << " not in scan " << scan->name << endl;
		}
		else
		{
			arange = &scan->stations.find(ant)->second;

			mode = V->getMode(scan->modeName);
			for(int i = 0; i < 4; i++)
			{
				ifmap[i] = -1;
			}
			nif = 0;
			for(it = mode->subbands.begin(); it != mode->subbands.end(); it++)
			{
				for(int i = 0; i < 4; i++)
				{
					if(it->ifname == VLBAIFName[i])
					{
						if(ifmap[i] < 0)
						{
							ifmap[i] = nif;
							nif++;
						}
					}
				}
			}
			for(int i = 0; i < 4; i++)
			{
				if(ifmap[i] >= 0)
				{
					if(sw[ifmap[i]] != VLBAIFName[i])
					{
						sw[ifmap[i]] = VLBAIFName[i];
						*this << "subarray.set4x4Switch(" << ifmap[i] << ", \"" << VLBAIFName[i] << "\")" << endl;
					}
				}
			}

			int sourceId = V->getSourceId(scan->sourceName);
			if(sourceId != lastSourceId)
			{
				*this << "subarray.setSource(source" << sourceId << ")" << endl;
				lastSourceId = sourceId;
			}

			*this << "subarray.setRecord(" << arange->mjdStart << ", " << arange->mjdStop << ", '\%s_\%s_\%s' % (obsCode, stnCode, \'" << scan->name << "\') )" << endl;
			*this << "subarray.execute(" << scan->mjdVex << ")" << endl;

			lastValid = arange->mjdStop;
		}

		*this << endl;
	}

	precision(p);

	return n;
}
