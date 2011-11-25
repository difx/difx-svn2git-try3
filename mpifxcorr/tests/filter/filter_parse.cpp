/********************************************************************
 * Ultra-narrow filters for VLBI software correlation
 * (C) 2010 Jan Wagner MPIfR
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *********************************************************************/

#include "filterchain.h"
#include "filters.h"
#include "filterhelpers.h"
#include <iostream>
using std::endl;
using std::cout;

int main(int argc, char** argv)
{
    if (argc < 2) {
        cout << endl
             << "Usage: filter_parser <filterfile.coeff>" << endl << endl
             << "Attempts to load and parse the specified filter file." << endl
             << "Can be used for a validity check for that file." << endl << endl;
        return -1;
    }

    FilterChain fc;
    fc.buildFromFile(argv[1], 16);
    fc.summary(std::cout);

    return 0;
}
