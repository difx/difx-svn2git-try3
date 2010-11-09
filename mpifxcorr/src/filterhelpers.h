#ifndef _HELPERS_H
#define _HELPERS_H

#include <vector>

class Helpers {
   private:
     Helpers();
   public:
     static void parse_numerics_file(std::vector<double>& vec, const char* filename); 
};

#endif // _HELPERS_H
