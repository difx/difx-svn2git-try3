/***************************************************************************
 *   Copyright (C) 2008, 2009 by Walter Brisken                            *
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
 * $Id:$
 * $HeadURL:$
 * $LastChangedRevision:$
 * $Author:$
 * $LastChangedDate:$
 *
 *==========================================================================*/


#ifndef __REPLACED_H__
#define __REPLACED_H__

#ifdef WORDS_BIGENDIAN
#define MARK5_FILL_WORD32 0x44332211UL
#define MARK5_FILL_WORD64 0x4433221144332211ULL
#else
#define MARK5_FILL_WORD32 0x11223344UL
#define MARK5_FILL_WORD64 0x1122334411223344ULL
#endif

#ifdef __cplusplus
extern "C" {
#endif

void countReplaced(const unsigned long *data, int len, long long *wGood, long long *wBad);

#ifdef __cplusplus
}
#endif

#endif
