/***************************************************************************
 *   Copyright (C) 2014-2015 by Chris Phillips & Walter Brisken            *
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

#include <iostream>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <cmath>
#include <cstdlib>
#include "timeutils.h"
#include "parserhelp.h"

enum charType whatChar(const char a) {
	if (a=='+'||a=='-') 
		return SIGN;
	else if (a=='E'||a=='e')
		return (E);
	else if (a>='0'&&a<='9')
		return DIGIT;
	else if (a==' ')
		return SPACE;
	else if (a=='.')
		return DOT;
	else
		return CHARERROR;
}

int getDouble(std::string &value, double &x)
{
	enum stateType {START, STARTINT, INTEGER, DECIMAL, STARTEXP, EXPONENT, END, ERROR};
	enum stateType state = START;
	enum charType what;

	unsigned int i;
	for (i=0 ; i<value.length(); i++) {
		what = whatChar(value[i]);
	  
		switch (state) {
		case START:
			switch (what) {
			case CHARERROR:
				std::cerr << "Error parsing character in \"" << value << "\" at : '" << value[i] << "':" << i << std::endl;
				value = "";
				return 1; 
				break;
			case SIGN:
				state = STARTINT;
				break;
			case DIGIT:
				state = INTEGER;
				break;
			case SPACE:
				break;
			case E:
				state = ERROR;
				break;
			case DOT:
				state=DECIMAL;
				break;
			}
			break;
	
		case STARTINT:
			switch (what) {
			case CHARERROR:
				std::cerr << "Error parsing character in \"" << value << "\" at : '" << value[i] << "':" << i << std::endl;
				value = "";
				return 1; 
				break;
			case SIGN:
			case E:
				state = ERROR;
				break;
			case DIGIT:
				state = INTEGER;
				break;
			case SPACE:
				break;
			case DOT:
				state = DECIMAL;
			}
			break;

		case INTEGER:
			switch (what) {
			case CHARERROR:
				std::cerr << "Error parsing character in \"" << value << "\" at : '" << value[i] << "':" << i << std::endl;
				value = "";
				return 1; 
				break;
			case DIGIT:
				break;
			case SIGN:
			case SPACE:
				state = END;
				break;
			case E:
				state = STARTEXP;
				break;
			case DOT:
				state = DECIMAL;
			}
			break;
	
		case DECIMAL:
			switch (what) {
			case CHARERROR:
				std::cerr << "Error parsing character in \"" << value << "\" at : '" << value[i] << "':" << i << std::endl;
				value = "";
				return 1; 
				break;
			case DIGIT:
				break;
			case SIGN:
			case SPACE:
				state = END;
				break;
			case E:
				state = STARTEXP;
				break;
			case DOT:
				state = ERROR;
				break;
			}
			break;
	
		case STARTEXP:
			switch (what) {
			case CHARERROR:
				std::cerr << "Error parsing character in \"" << value << "\" at : '" << value[i] << "':" << i << std::endl;
				value = "";
				return 1; 
				break;
			case SIGN:
			case DIGIT:
				state = EXPONENT;
				break;
			case SPACE:
			case E:
			case DOT:
				state = ERROR;
				break;
			}
			break;
	
		case EXPONENT:
			switch (what) {
			case CHARERROR:
				std::cerr << "Error parsing character in \"" << value << "\" at : '" << value[i] << "':" << i << std::endl;
				value = "";
				return 1; 
				break;
			case SPACE:
			case SIGN:
				state = END;
				break;
			case DIGIT:
				break;
			case DOT:
			case E:
				state = ERROR;
				break;
			}
			break;

		case ERROR:
		case END:
			break;
	
		}
	  
		if (state==ERROR) {
			std::cerr << "Error parsing \"" << value << "\" at : '" << value[i] << "':" << i << std::endl;
			value = "";
			return 1; 
		}
		if (state==END) break;
	}

	std::stringstream ss;
	if (state==START) {
		value = "";
		return 1;
	} else if (state==END) {
	} else {
		i = value.length();
	}
	ss << value.substr(0,i);
	ss >> x;
	value  = value.substr(i);

	return 0;
}
  
int getOp(std::string &value, int &plus) {
	enum charType what;

	unsigned int i;
	for (i=0 ; i<value.length(); i++) {
		what = whatChar(value[i]);
	  
		if (what==CHARERROR) {
			std::cerr << "Error parsing character in \"" << value << "\" at : '" << value[i] << "':" << i << std::endl;
			value = "";
			return 1; 
		} else if (what==SPACE) {
			continue;
		} else if (what==SIGN) {
			if (value[i]=='+') {
				plus = 1;
			} else {
				plus = 0;
			} 
			value = value.substr(i+1);
			return(0);
		} else {
			std::cerr << "Unexpected character in \"" << value << "\" at : '" << value[i] << "':" << i << std::endl;
			value = "";
			return 1; 
		}
	}
	return(1); // Did not match anything
}

// Read a string consisting of a series of additions and subtrations (only) and return a double
double parseDouble(const std::string &value) {
	// Read a string consisting of a series of additions and subtrations (only) and return a double

	std::string str = value; // Copy as the procedure destroys the string
  
	int status, number=1, sign=-1;
	double thisvalue, result=0;
	while (str.length()) {
		if (number) {
			status = getdouble(str, thisvalue);
			if (status) break;
			if (sign==-1)
				result = thisvalue;
			else if (sign==1) 
				result += thisvalue;
			else
				result -= thisvalue;
			number = 0;
	
		} else	{
			status = getOp(str, sign);
			if (status) break;
			number = 1;
		}
	}

	return result;

}

// Turns a string into MJD 
// The following formats are allowed:
// 1. decimal mjd:	55345.113521
// 2. ISO 8601 dateTtime strings:  2009-03-08T12:34:56.121
// 3. VLBA-like time:	2009MAR08-12:34:56.121
// 4. vex time: 2009y245d08h12m24s"
double parseTime(const std::string &timeStr)
{
	double mjd;
	const char* const str = timeStr.c_str();
	const char *p;
	double t;
	double f;
	int n;
	struct tm tm;
	bool need_s = false;
	char dummy;
	char* endptr;

	// Test for ISO 8601
	p = strptime(str, "%FT%T", &tm);
	if(!p)
	{
		//Test for VLBA-like
		p = strptime(str, "%Y%b%d-%T", &tm);
	}
	if(!p)
	{
		//Test for Vex
		p = strptime(str, "%Yy%jd%Hh%Mm%S", &tm);
		need_s = true;
	}
	if(p)
	{
		f = 0.0;
		if(p[0] == '.') {
			errno = 0;
			f = strtod(p,&endptr);
			if((endptr != p) && (errno==0)) {
				p = endptr;
			}
			else {
				f = 0.0;
			}
		}
		if(need_s) {
			if(p[0] == 's') {}
			else {
				goto format_error;
			}
		}
		t = mktime(&tm);
				
		mjd = (t+f)/SEC_DAY_DBL_ + MJD_UNIX0;

		return mjd;
	}

	n = sscanf(str, "%lf%c", &mjd, &dummy);
	if(n == 1)
	{
		// Must be straight MJD value
		return mjd;
	}
		
format_error:
	// No match
	std::cerr << std::endl;
	std::cerr << "Error: date not parsable: " << timeStr << std::endl;
	std::cerr << std::endl;
	std::cerr << "Allowable formats are:" << std::endl;
	std::cerr << "1. Straight MJD		 54345.341944" << std::endl;
	std::cerr << "2. Vex formatted date	 2009y245d08h12m24s" << std::endl;
	std::cerr << "3. VLBA-like format	 2009SEP02-08:12:24" << std::endl;
	std::cerr << "4. ISO 8601 format	 2009-09-02T08:12:24" << std::endl;
	std::cerr << std::endl;

	exit(EXIT_FAILURE);
}

// Turns a string into an MJD with a integer and fractional day parts
// The following formats are allowed:
// 1. decimal mjd:				   54345.341944
// 2. ISO 8601 dateTtime strings:  2009-03-08T12:34:56.121
// 3. VLBA-like time			   2009MAR08-12:34:56.121
// 4. vex time					   2009y061d12h34m56.121s
void parseTimeFractional(const char* const str,
						 int& mjd,
						 double& dayfraction,
						 char** endptr)
{
	char *p;
	double f;
	struct tm tm;
	bool need_s = false;
	*endptr = 0;

	// Test for ISO 8601
	p = strptime(str, "%FT%T", &tm);
	if(!p)
	{
		//Test for VLBA-like
		p = strptime(str, "%Y%b%d-%T", &tm);
	}
	if(!p)
	{
		//Test for Vex
		p = strptime(str, "%Yy%jd%Hh%Mm%S", &tm);
		need_s = true;
	}
	if(p)
	{
		f = 0.0;
		if(p[0] == '.') {
			errno = 0;
			f = strtod(p,endptr);
			if((*endptr != p) && (errno==0)) {
				p = *endptr;
			}
			else {
				f = 0.0;
			}
		}
		if(need_s) {
			if(p[0] == 's') {
				*endptr = const_cast<char*>(p+1);
			}
			else {
				goto format_error;
			}
		}
		else {
			*endptr = const_cast<char*>(p);
		}
		time_t tt = mktime(&tm);
				
		mjd = tt/SEC_DAY_INT + MJD_UNIX0_INT;
		dayfraction = (tt%SEC_DAY_INT + f)/SEC_DAY_DBL_;
		return;
	}

	errno = 0;
	mjd = int(strtol(p,endptr,10));
	if((*endptr != p) && (errno==0)) {}
	else {
		goto format_error;
	}
	p=*endptr;
	dayfraction = strtod(p,endptr);
	if((*endptr != p) && (errno==0)) {}
	else {
		goto format_error;
	}
	return;
		
format_error:
	// No match
	std::cerr << std::endl;
	std::cerr << "Error: date not parsable in parseTimeFractional: " << str << std::endl;
	std::cerr << std::endl;
	std::cerr << "Allowable formats are:" << std::endl;
	std::cerr << "1. Straight MJD		 54345.341944" << std::endl;
	std::cerr << "2. Vex formatted date	 2009y245d08h12m24s" << std::endl;
	std::cerr << "3. VLBA-like format	 2009SEP02-08:12:24" << std::endl;
	std::cerr << "4. ISO 8601 format	 2009-09-02T08:12:24" << std::endl;
	std::cerr << std::endl;

	exit(EXIT_FAILURE);
}





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
SpacecraftGroundClockBreak parseSpacecraftGroundClockBreak(const std::string &timeStr, int* nWarn)
{
	bool have_start = false;
	bool have_sync = false;
	bool have_fudge0 = false;
	bool have_fudge1 = false;
	bool have_fudge2 = false;
	bool have_fudge3 = false;
	bool have_fudge4 = false;
	bool have_fudge5 = false;
	bool no_identifiers = false;
	int pos_count = 0;
	std::string::size_type at, last, splitat;
	std::string nestedkeyval;
	std::string key;
	std::string value;
	const char* str;
	char* endptr = 0;
	SpacecraftGroundClockBreak result;
   
	last = 0;
	at = 0;
	while(at != std::string::npos)
	{
		at = timeStr.find_first_of('/', last);
		nestedkeyval = timeStr.substr(last, at-last);
		splitat = nestedkeyval.find_first_of('@');
		if(splitat == std::string::npos)
		{
			if(pos_count == 0)
			{
				std::cerr << "Warning: old style SC_GS_clock_break entry without key@value pairs found.	 Assuming the values come in the correct order" << std::endl;
				++(*nWarn);
				no_identifiers = true;
			}
			else if(!no_identifiers) {
				std::cerr << "Error: mixed old style (vlaues only) and new style (key@value) entries in SC_GS_clock_break entry is not allowed.	 SC_GS_clock_break entry is '" << timeStr << "'" << std::endl;
				goto format_error;
			}
			str = nestedkeyval.c_str();
			if(pos_count == 0) {
				parseTimeFractional(str, result.mjd_start, result.day_fraction_start, &endptr);
				if((endptr == 0) || (*endptr != 0)) {
					goto format_error;
				}
				have_start = true;
			}
			else if(pos_count == 1) {
				parseTimeFractional(str, result.mjd_sync, result.day_fraction_sync, &endptr);
				if((endptr == 0) || (*endptr != 0)) {
					goto format_error;
				}
				have_sync = true;
			}
			else if(pos_count == 2) {
				errno = 0;
				result.clock_break_fudge_seconds_0 = strtod(str,&endptr);
				if((*endptr == 0) && (errno==0)) {}
				else {
					goto format_error;
				}
				if(result.clock_break_fudge_order < 0)
				{
					result.clock_break_fudge_order = 0;
				}
				result.clock_break_fudge_seconds_0 *= 1E-6; // convert from \mu s to s
				have_fudge0 = true;
			}
			else {
				std::cerr << "Error: too many values in old-style SC_GS_clock_break entry '" << timeStr << "'" << std::endl;
				goto format_error;
			}
		}
		else {
			// key@value
			if(no_identifiers) {
				std::cerr << "Error: mixed old style (vlaues only) and new style (key@value) entries in SC_GS_clock_break entry is not allowed.	 SC_GS_clock_break entry is '" << timeStr << "'" << std::endl;
				goto format_error;
			}
			key = nestedkeyval.substr(0,splitat);
			value = nestedkeyval.substr(splitat+1);
			if(key == "start") {
				if(have_start) {
					std::cerr << "Error: multiple 'start' keys in SC_GS_clock_break entry '" << timeStr << "'" << std::endl;
					goto format_error;
				}
				parseTimeFractional(value.c_str(), result.mjd_start, result.day_fraction_start, &endptr);
				if((endptr == 0) || (*endptr != 0)) {
					goto format_error;
				}
				have_start = true;
			}
			else if(key == "sync") {
				if(have_sync) {
					std::cerr << "Error: multiple 'sync' keys in SC_GS_clock_break entry '" << timeStr << "'" << std::endl;
					goto format_error;
				}
				parseTimeFractional(value.c_str(), result.mjd_sync, result.day_fraction_sync, &endptr);
				if((endptr == 0) || (*endptr != 0)) {
					goto format_error;
				}
				have_sync = true;
			}
			else if((key == "clockfudge")||(key == "clockfudge0")) {
				if(have_fudge0) {
					std::cerr << "Error: multiple 'clockfudge0' keys in SC_GS_clock_break entry '" << timeStr << "'" << std::endl;
					goto format_error;
				}
				errno = 0;
				result.clock_break_fudge_seconds_0 = strtod(value.c_str(),&endptr);
				if((*endptr == 0) && (errno==0)) {}
				else {
					goto format_error;
				}
				if(result.clock_break_fudge_order < 0)
				{
					result.clock_break_fudge_order = 0;
				}
				result.clock_break_fudge_seconds_0 *= 1E-6; // convert from \mu s to s
				have_fudge0 = true;
			}
			else if(key == "clockfudge1") {
				if(have_fudge1) {
					std::cerr << "Error: multiple 'clockfudge1' keys in SC_GS_clock_break entry '" << timeStr << "'" << std::endl;
					goto format_error;
				}
				errno = 0;
				result.clock_break_fudge_seconds_1 = strtod(value.c_str(),&endptr);
				if((*endptr == 0) && (errno==0)) {}
				else {
					goto format_error;
				}
				if(result.clock_break_fudge_order < 1)
				{
					result.clock_break_fudge_order = 1;
				}
				result.clock_break_fudge_seconds_1 *= 1E-6; // convert from \mu s to s
				have_fudge1 = true;
			}
			else if(key == "clockfudge2") {
				if(have_fudge2) {
					std::cerr << "Error: multiple 'clockfudge2' keys in SC_GS_clock_break entry '" << timeStr << "'" << std::endl;
					goto format_error;
				}
				errno = 0;
				result.clock_break_fudge_seconds_2 = strtod(value.c_str(),&endptr);
				if((*endptr == 0) && (errno==0)) {}
				else {
					goto format_error;
				}
				if(result.clock_break_fudge_order < 2)
				{
					result.clock_break_fudge_order = 2;
				}
				result.clock_break_fudge_seconds_2 *= 1E-6; // convert from \mu s to s
				have_fudge2 = true;
			}
			else if(key == "clockfudge3") {
				if(have_fudge3) {
					std::cerr << "Error: multiple 'clockfudge3' keys in SC_GS_clock_break entry '" << timeStr << "'" << std::endl;
					goto format_error;
				}
				errno = 0;
				result.clock_break_fudge_seconds_3 = strtod(value.c_str(),&endptr);
				if((*endptr == 0) && (errno==0)) {}
				else {
					goto format_error;
				}
				if(result.clock_break_fudge_order < 3)
				{
					result.clock_break_fudge_order = 3;
				}
				result.clock_break_fudge_seconds_3 *= 1E-6; // convert from \mu s to s
				have_fudge3 = true;
			}
			else if(key == "clockfudge4") {
				if(have_fudge4) {
					std::cerr << "Error: multiple 'clockfudge4' keys in SC_GS_clock_break entry '" << timeStr << "'" << std::endl;
					goto format_error;
				}
				errno = 0;
				result.clock_break_fudge_seconds_4 = strtod(value.c_str(),&endptr);
				if((*endptr == 0) && (errno==0)) {}
				else {
					goto format_error;
				}
				if(result.clock_break_fudge_order < 4)
				{
					result.clock_break_fudge_order = 4;
				}
				result.clock_break_fudge_seconds_4 *= 1E-6; // convert from \mu s to s
				have_fudge4 = true;
			}
			else if(key == "clockfudge5") {
				if(have_fudge5) {
					std::cerr << "Error: multiple 'clockfudge5' keys in SC_GS_clock_break entry '" << timeStr << "'" << std::endl;
					goto format_error;
				}
				errno = 0;
				result.clock_break_fudge_seconds_5 = strtod(value.c_str(),&endptr);
				if((*endptr == 0) && (errno==0)) {}
				else {
					goto format_error;
				}
				if(result.clock_break_fudge_order < 5)
				{
					result.clock_break_fudge_order = 5;
				}
				result.clock_break_fudge_seconds_5 *= 1E-6; // convert from \mu s to s
				have_fudge5 = true;
			}
			else {
				std::cerr << "Error: unrecognized SC_GS_clock_break sub-key '" << key << "' in SC_GS_clock_break entry '" << timeStr << "'" << std::endl;
				goto format_error;
			}
		}
		pos_count++;
		last = at+1;
	}
	// Did we get everything we need?
	if((have_start) && (have_sync)) {
		// all we need
	}
	else {
		std::cerr << "Error: not all required sub-keys were found in SC_GS_clock_break entry '" << timeStr << "'" << std::endl;
		goto format_error;
	}
	return result;

format_error:
	// No match
	std::cerr << std::endl;
	std::cerr << "Error: SC_GS_clock_break entry '" << timeStr << "' not parsable." << std::endl;
	std::cerr << std::endl;
	std::cerr << "SC_GS_clock_break should be given as start@MJD/sync@MJD/clockfudge0@sec/clockfudge1@sec_per_sec_1/clockfudge2@sec_per_sec_2/clockfudge3@sec_per_sec_3/clockfudge4@sec_per_sec_4/clockfudge5@sec_per_sec_5\n"
"In the .v2d file, the clock fudge polynomial terms have units of\n"
"microseconds per second^{N}.\n"
"The SC_GS_clock_break terms may be provided in any order within the .../.../... construct.\n"
"Zero  or more of the clock fudge polynomial terms may be present.\n"
"The start MJD indicates the time to create the clock break,\n"
"and the sync MJD indicates the zero point for the fudge series." << std::endl;
	std::cerr << std::endl;
	std::cerr << "Allowable MJD date formats are:" << std::endl;
	std::cerr << "1. Straight MJD		 54345.341944" << std::endl;
	std::cerr << "2. Vex formatted date	 2009y245d08h12m24s" << std::endl;
	std::cerr << "3. VLBA-like format	 2009SEP02-08:12:24" << std::endl;
	std::cerr << "4. ISO 8601 format	 2009-09-02T08:12:24" << std::endl;
	std::cerr << std::endl;
	std::cerr << "Clock fudge polynomial terms should be specified as a floating point value such as:" << std::endl;
	std::cerr << "1" << std::endl;
	std::cerr << "1.2" << std::endl;
	std::cerr << "1.2E3" << std::endl;
		

	exit(0);
}



simple3Vector parseSpacecraftsimple3Vector(const std::string &vecStr)
{
	const char* const str = vecStr.c_str();
	const char *p;
	char* endptr;
	simple3Vector result;

	p = str;
	endptr = 0;
	errno=0;
	result.X = strtod(p, &endptr);
	if((endptr == 0) || (endptr == p) || (errno != 0) || (*endptr == 0)) {
		goto format_error;
	}
	p = endptr + 1;
	endptr = 0;
	result.Y = strtod(p, &endptr);
	if((endptr == 0) || (endptr == p) || (errno != 0) || (*endptr == 0)) {
		goto format_error;
	}
	p = endptr + 1;
	endptr = 0;
	result.Z = strtod(p, &endptr);
	if((endptr == 0) || (endptr == p) || (errno != 0) || (*endptr != 0)) {
		goto format_error;
	}
	return result;

format_error:
	// No match
	std::cerr << std::endl;
	std::cerr << "Error: simple3Vector values not parsable: " << vecStr << std::endl;
	std::cerr << std::endl;
	std::cerr << "simple3Vector entries should be provided as" << std::endl;
	std::cerr << "NUMBER,NUMBER,NUMBER" << std::endl;
	std::cerr << "with no whitespace between entries" << std::endl;
	std::cerr << std::endl;

	exit(0);
}

double parseCoord(const char *str, char type)
{
	int sign = 1, l, n;
	double a, b, c;
	double v = -999999.0;

	if(type != ' ' && type != 'R' && type != 'D')
	{
		std::cerr << "Programmer error: parseCoord: parameter 'type' has illegal value = " << type << std::endl;
		
		exit(EXIT_FAILURE);
	}

	if(str[0] == '-')
	{
		sign = -1;
		++str;
	}
	else if(str[0] == '+')
	{
		++str;
	}

	l = strlen(str);

	if(sscanf(str, "%lf:%lf:%lf", &a, &b, &c) == 3)
	{
		v = sign*(a + b/60.0 + c/3600.0);
		if(type == 'D')
		{
			v *= M_PI/180.0;
		}
		else
		{
			v *= M_PI/12.0;
		}
	}
	else if(sscanf(str, "%lfh%lfm%lf", &a, &b, &c) == 3 && str[l-1] == 's' && type != 'D')
	{
		v = sign*(a + b/60.0 + c/3600.0);
		v *= M_PI/12.0;
	}
	else if(sscanf(str, "%lfd%lf'%lf\"", &a, &b, &c) == 3 && str[l-1] == '"' && type == 'D')
	{
		v = sign*(a + b/60.0 + c/3600.0);
		v *= M_PI/180.0;
	}
	else if(sscanf(str, "%lf%n", &a, &n) == 1)
	{
		if(n == l)
		{
			v = a;
		}
		else if(strcmp(str+n, "rad") == 0)
		{
			v = a;
		}
		else if(strcmp(str+n, "deg") == 0)
		{
			v = a*M_PI/180.0;
		}
		else
		{
			std::cerr << "Error parsing coordinate value " << str << std::endl;

			exit(EXIT_FAILURE);
		}
		v *= sign;
	}

	return v;
}

// From http://oopweb.com/CPP/Documents/CPPHOWTO/Volume/C++Programming-HOWTO-7.html
void split(const std::string &str, std::vector<std::string> &tokens, const std::string &delimiters = " ")
{
	// Skip delimiters at beginning.
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter".
	std::string::size_type pos	   = str.find_first_of(delimiters, lastPos);

	while(std::string::npos != pos || std::string::npos != lastPos)
	{
		// Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters.	 Note the "not_of"
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"
		pos = str.find_first_of(delimiters, lastPos);
	}
}

bool parseBoolean(const std::string &str)
{
	if(str[0] == '0' || str[0] == 'f' || str[0] == 'F' || str[0] == '-')
	{
		return false;
	}
	else
	{
		return true;
	}
}
