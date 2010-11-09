#include "filterhelpers.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <limits>

/**
 * Read floating-point numbers from a file and append them to a vector.
 * Comments in the file (any text after and including '#') are ignored.
 */
void Helpers::parse_numerics_file(std::vector<double>& vec, const char* filename)
{
    vec.clear();
    std::ifstream fin(filename, std::ios::in);
    while (true) {
        std::string wstr;
        getline(fin, wstr, '#');
        fin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::istringstream ssconverter(wstr);
        while (true) {
            double x;
            ssconverter >> std::dec >> x;
            if (!ssconverter) { break; }
            else { vec.push_back(x); }
        }
        if (!fin) break;
    }
}
