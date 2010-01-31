#include "transient.h"

bool operator< (Transient &t1, Transient &t2)
{
	return t1.priority < t2.priority;
}
