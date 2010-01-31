#ifndef __TRANSIENT_H__
#define __TRANSIENT_H__

#include <string>
#include <list>
#include <difxmessage.h>

using namespace std;

class Transient
{
public:
	Transient(double start, double stop, double pri) : 
		startMJD(start), stopMJD(stop), priority(pri) {}
	double startMJD, stopMJD, priority;

	friend bool operator< (Transient &t1, Transient &t2);

};

class TransientQueue
{
public:
	TransientQueue(string id) : jobId(id) {}
	string jobId;
	string destDir;
	list<Transient> transient;
};

class TransientManager
{
public:
	list<TransientQueue> queues;

	void startJob(string jobId);
	void stopJob(string jobId);
	void addEvent(const DifxMessageTransient *dt);
};

#endif
