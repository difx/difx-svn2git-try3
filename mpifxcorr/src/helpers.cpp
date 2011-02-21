#include <string>
#include <iostream>
#include <sys/time.h>
#include "helpers.h"

Timing::Timing(Alert& os, std::string& msg) : _os(os), _msg(msg) 
{
  gettimeofday(&start, NULL);
}

Timing::Timing(Alert& os, const char* msg) : _os(os), _msg(std::string(msg)) 
{
  gettimeofday(&start, NULL);
}

Timing::~Timing() 
{
  gettimeofday(&stop, NULL);
  double dT = (stop.tv_sec - start.tv_sec)*1e6 + (stop.tv_usec - start.tv_usec);
  _os << startl << _msg << dT << endl;
}

// vim: shiftwidth=2:softtabstop=2:expandtab

