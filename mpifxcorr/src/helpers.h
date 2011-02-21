#ifndef HELPERS_H
#define HELPERS_H

#include <string>
#include <iostream>
#include <sys/time.h>
#include "alert.h"

class Timing {
  public:
    explicit Timing(Alert& os, std::string& msg);
    explicit Timing(Alert& os, const char* msg);
    ~Timing();

  private:
    std::string _msg;
    Alert& _os;
    struct timeval start;
    struct timeval stop;
};

#endif
// vim: shiftwidth=2:softtabstop=2:expandtab

