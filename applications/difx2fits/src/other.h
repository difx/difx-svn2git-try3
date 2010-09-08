/***************************************************************************
 *   Copyright (C) 2008-2010 by Walter Brisken                             *
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
 * $HeadURL$
 * $LastChangedRevision$
 * $Author$
 * $LastChangedDate$
 *
 *==========================================================================*/

#ifndef __OTHER_H__
#define __OTHER_H__

#ifndef NELEMENTS
#define NELEMENTS(array)    /* number of elements in an array */ \
              (sizeof (array) / sizeof ((array) [0]))
#endif

char *mjd2str(long, char *);
int mjd2dayno(long, int *);
int mjd2date(long, int*, int*, int*);
char *time2str(double, char *, char *);
char *rad2str(double, char *, char *);
char *rad2strg(double, char *, char *, int);
char *timeMjd2str(double, char *);
char *srvMjd2str(double inTime, char *pOutStr);

/* textutils.c */
void copyQuotedString(char *dest, const char *src, int n);

/* ymd2mjd.c */
int ymd2mjd(int yr, int mo, int day);
int ymd2doy(int yr, int mo, int day);

#endif
