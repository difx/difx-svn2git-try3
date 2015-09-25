/***************************************************************************
 *   Copyright (C) 2014-2015 by Chris Phillips                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
/*===========================================================================
 * SVN properties (DO NOT CHANGE)
 *
 * $Id$
 * $HeadURL: https://svn.atnf.csiro.au/difx/applications/vex2difx/branches/multidatastream/src/vex2difx.cpp $
 * $LastChangedRevision$
 * $Author$
 * $LastChangedDate$
 *
 *==========================================================================*/

#ifndef __PARSERHELP_H__
#define __PARSERHELP_H__

#include <string>
#include <vector>

enum charType
{
	SIGN,
	DIGIT,
	DOT,
	E,
	SPACE,
	CHARERROR
};

enum charType whatChar(const char a);

int getDouble(std::string &value, double &x);
  
int getOp(std::string &value, int &plus);

// Read a string consisting of a series of additions and subtrations (only) and return a double
double parseDouble(const std::string &value);

// Turns a string into MJD
// The following formats are allowd:
// 1. decimal mjd:  55345.113521
// 2. ISO 8601 dateTtime strings:  2009-03-08T12:34:56.121
// 3. VLBA-like time:   2009MAR08-12:34:56.121
// 4. vex time: 2009y245d08h12m24s"
double parseTime(const std::string &timeStr);
// Turns a string into an MJD with a integer and fractional day parts
// The following formats are allowed:
// 1. decimal mjd:				   54345.341944
// 2. ISO 8601 dateTtime strings:  2009-03-08T12:34:56.121
// 3. VLBA-like time			   2009MAR08-12:34:56.121
// 4. vex time					   2009y061d12h34m56.121s
void parseTimeFractional(const char* const str,
						 int& mjd,
						 double& dayfraction,
                         char** endptr);

// Turns a SpacecraftGroundClockBreak string into two MJDs,
// a clock offset fudge, and a possible clock offset polynomial fudge
// start@MJD/sync@MJD/clockfudge0@sec/clockfudge1@sec_per_sec_1/clockfudge2@sec_per_sec_2/clockfudge3@sec_per_sec_3/clockfudge4@sec_per_sec_4/clockfudge5@sec_per_sec_5
// The clock fudge terms in the class have units of seconds per second^{N}.
// In the .v2d file, the fudge terms have units of
// microseconds per second^{N}.
// The terms may be provided in any order within the .../.../... construct.
// Zero or more of the clock terms may be present.  The start MJD indicates
// the time to create the clock break, and the sync MJD indicates the
// instant at which the recorder syncs the
// time between the ground station and the spacecraft.
// The following formats are allowed for the MJDs:
// 1. decimal mjd:				   54345.341944
// 2. ISO 8601 dateTtime strings:  2009-03-08T12:34:56.121
// 3. VLBA-like time			   2009MAR08-12:34:56.121
// 4. vex time					   2009y061d12h34m56.121s
SpacecraftGroundClockBreak parseSpacecraftGroundClockBreak(const std::string &timeStr, int* nWarn);
simple3Vector parseSpacecraftsimple3Vector(const std::string &vecStr):


double parseCoord(const char *str, char type);

void split(const std::string &str, std::vector<std::string> &tokens, const std::string &delimiters = " ");

bool parseBoolean(const std::string &str);

#endif
