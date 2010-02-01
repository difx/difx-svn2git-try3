#include <iostream>
#include <cstdio>
#include <cstdlib>
#include "transient.h"

bool operator< (Event &t1, Event &t2)
{
	return t1.priority < t2.priority;
}

void EventQueue::addEvent(const DifxMessageTransient *dt)
{
	events.push_back(Event(dt->startMJD, dt->stopMJD, dt->priority));
	while(events.size() > maxSize)
	{
		// Need to delete an element -- delete the lowest priority one
		events.sort();
		events.pop_front();
	}
	destDir = dt->destDir;
}

void EventQueue::copy()
{
	const int CommandSize = 512;
	list<Event>::const_iterator e;
	list<string>::const_iterator m;
	char command[CommandSize];
	int v;

	if(events.empty())
	{
		return;
	}

	/* put the events in decreasing priority and generate the copy command */
	events.sort();
	events.reverse();

	// FIXME -- this should be parallelized across units!
	for(e = events.begin(); e != events.end(); e++)
	{
		for(m = units.begin(); m != units.end(); m++)
		{
			snprintf(command, CommandSize, 
				"su - %s -c 'ssh -x %s \"mk5cp Active %14.8f_%14.8f %s\"'",
				user.c_str(),
				m->c_str(),
				e->startMJD, e->stopMJD, destDir.c_str());
			printf("Executing %s\n", command);

			v = system(command);
		}
	}
}

void EventQueue::addMark5Unit(const char *unit)
{
	units.push_back(string(unit));
}

void EventQueue::setUser(const char *u)
{
	user = u;
}

void EventQueue::print() const
{
	list<Event>::const_iterator e;

	cout << "  Job [" << jobId << "]:" << endl;

	for(e = events.begin(); e != events.end(); e++)
	{
		cout << "    " << "pri=" << e->priority << " start=" << e->startMJD << " stop=" << e->stopMJD << endl;
	}
}

EventManager::EventManager()
{
	pthread_mutex_init(&lock, 0);
}

EventManager::~EventManager()
{
	pthread_mutex_destroy(&lock);
}

EventQueue &EventManager::startJob(const char *jobId)
{
	list<EventQueue>::iterator q;
	bool existed=false;

	pthread_mutex_lock(&lock);

	for(q = queues.begin(); q != queues.end(); q++)
	{
		if(q->jobId == jobId)
		{
			queues.erase(q);
			existed = true;
		}
	}

	if(!existed)
	{
		queues.push_back(EventQueue(jobId));
	}

	pthread_mutex_unlock(&lock);

	return queues.back();
}

void EventManager::stopJob(const char *jobId)
{
	list<EventQueue>::iterator q;

	pthread_mutex_lock(&lock);

	for(q = queues.begin(); q != queues.end(); q++)
	{
		if(q->jobId == jobId)
		{
			q->copy();
			queues.erase(q);
			break;
		}
	}

	pthread_mutex_unlock(&lock);
}

bool EventManager::addEvent(const DifxMessageTransient *dt)
{
	list<EventQueue>::iterator q;
	bool queued = false;

	pthread_mutex_lock(&lock);

	for(q = queues.begin(); q != queues.end(); q++)
	{
		if(q->jobId == dt->jobId)
		{
			q->addEvent(dt);
			queued = true;
			break;
		}
	}

	pthread_mutex_unlock(&lock);

	return queued;
}

void EventManager::print()
{
	list<EventQueue>::const_iterator q;
	
	pthread_mutex_lock(&lock);
	
	cout << "Transient queue [" << queues.size() << " jobs running]" << endl;
	
	for(q = queues.begin(); q != queues.end(); q++)
	{
		q->print();
	}

	pthread_mutex_unlock(&lock);
}
