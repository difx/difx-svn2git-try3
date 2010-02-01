#ifndef __TRANSIENT_H__
#define __TRANSIENT_H__

#include <string>
#include <list>
#include <pthread.h>
#include <difxmessage.h>

using namespace std;

class Event
{
public:
	Event(double start, double stop, double pri) : 
		startMJD(start), stopMJD(stop), priority(pri) {}
	double startMJD, stopMJD, priority;

	friend bool operator< (Event &t1, Event &t2);
};

class EventQueue
{
public:
	EventQueue(string id) : jobId(id), maxSize(5) {}
	string jobId;
	string destDir;
	string user;
	unsigned int maxSize;
	list<Event> events;
	list<string> units;

	void addMark5Unit(const char *unit);
	void addEvent(const DifxMessageTransient *dt);
	void setUser(const char *u);
	void copy();
	void print() const;
};

class EventManager
{
public:
	list<EventQueue> queues;
	pthread_mutex_t lock;

	EventManager();
	~EventManager();
	EventQueue &startJob(const char *jobId);
	void stopJob(const char *jobId);
	bool addEvent(const DifxMessageTransient *dt);
	void print();
};

#endif
