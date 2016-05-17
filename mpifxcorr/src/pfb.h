/***************************************************************************
 *   Copyright (C) 2006-2016 by Jan Wagner                                 *
 *                                                                         *
 *   This program is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/
//===========================================================================
// SVN properties (DO NOT CHANGE)
//
// $Id: $
// $HeadURL: $
// $LastChangedRevision: $
// $Author: $
// $LastChangedDate: $
//
//============================================================================

#ifndef PFB_H
#define PFB_H

#include "architecture.h"
#include <iostream>
#include <fstream>
#include <cstdlib>

using namespace std;

#ifndef f32
#define f32 float
#endif


/**
@class PFB
@brief Polyphase filter bank class
@author Jan Wagner
*/

class PFB {

public:

	/**
	 * Constructor, loads filter bank coefficients shared by all instances of the class.
	 * @param scoeff An std::istream input text stream containing the PFB description.
         * @param maximize_usage Output direct FFT result during leading/tailing time where PFB has no valid output
	 */
	PFB(istream& scoeff, bool maximize_usage = false);

	/** Copy constructor. Allocates memory and copies PFB state. Shared coefficients are not altered. */
	PFB(const PFB*);

	/** Copy constructor. Allocates memory and copies PFB state. Shared coefficients are not altered. */
	PFB(const PFB&);

	/** Destructor */
	~PFB() { PFB::dealloc(); }

public:

	/** @return true if PFB configuration is usable and valid */
	bool isValid() const { return valid; }

	/** @return number of output channels of the PFB */
	int getNch() const { return Nch; }

	/** @return number of FIR taps in the PFB */
	int getNtaps() const { return Ntap; }

	/** @return total number of FIR coefficients (channels x taps) in the PFB */
	int getNcoeff() const { return Ncoeff; }

	/** Channelize a real-valued sample sequence that yields multiple output samples per channel.
	 * Previous PFB filter states are not remembered between calls to this function.
         * Output data start with the first valid sample, and trailing Ntap-1 samples assume the data
         * past the end of 'x' are all zero.
	 * @param x input sample data, real-valued
	 * @param Y output sample data, complex-valued, a sequence of blocks of [ch0 ... ch_Nch-1] samples
	 * @param N number of input samples, must be a multiple of PFB Nch
	 */
	void channelize(f32 const *x, cf32 *Y, const int N) const;

	/** Channelize a complex-valued sample sequence that yields multiple output samples per channel.
	 * Previous PFB filter states are not remembered between calls to this function.
         * Output data start with the first valid sample, and trailing Ntap-1 samples assume the data
         * past the end of 'x' are all zero.
	 * @param x input sample data, complex-valued
	 * @param Y output sample data, complex-valued, a sequence of blocks of [ch0 ... ch_Nch-1] samples
	 * @param N number of input samples, must be a multiple of PFB Nch
	 */
	void channelize(cf32 const *x, cf32 *Y, const int N) const; // TODO

	/** Channelize a short Nch-long chunk of samples, yielding one sample per output channel.
	 * The previous PFB filter state is remembered. This function can be called repeatedly to
	 * feed more input data to the PFB filter. Call reset() to discard filter states.
	 * The leading Ntap-1 outputs assume data before the first calls to this function were zero.
	 * @param x input sample data, real-valued
	 * @param Y output sample data, complex-valued, a sequence of blocks of [ch0 ... ch_Nch-1] samples
	 * @param N number of input samples, must equal PFB Nch
	 */
	void channelize_single(f32 const *x, cf32 *Y, const int N);

	/** Channelize a short Nch-long chunk of samples, yielding one sample per output channel.
	 * The previous PFB filter state is remembered. This function can be called repeatedly to
	 * feed more input data to the PFB filter. Call reset() to discard filter states.
	 * The leading Ntap-1 outputs assume data before the first calls to this function were zero.
	 * @param x input sample data, complex-valued
	 * @param Y output sample data, complex-valued, a sequence of blocks of [ch0 ... ch_Nch-1] samples
	 * @param N number of input samples, must equal PFB Nch
	 */
	void channelize_single(cf32 const *x, cf32 *Y, const int N); // TODO

	/** Reset the internal state of the PFB.
	 * Call this function when channelizing with channelize_single() and you changed
	 * the input signal source.
	 **/
	void reset();

private:
	static f32* coeffs_contiguous;
	static cf32* coeffscplx_contiguous; // identical values to coeffs_contiguous but complex ({re,0.0f})
	static f32** coeffs; // "coeffs[Ntap][Nch]" pointers into coeffs_contiguous
	static cf32** coeffscplx; // "coeffscplx[Ntap][Nch]" pointers into coeffscplx_contiguous
	static int Ncoeff, Nch, Ntap;
	bool valid;
	bool use_fft;
	bool maximize_data_use;
	f32* lags_contiguous;
	f32** lags; // "lags[Ntap][Nch]"
	cf32* lagscplx_contiguous;
	cf32** lagscplx; // "lags[Ntap][Nch]"
	int* lag_idcs;
	int lag_curr;
	int iter;
private:
	vecFFTSpecR_f32  *planFFTr2c;
	vecFFTSpecC_cf32 *planFFTc2c;
	vecDFTSpecR_f32  *planDFTr2c;
	vecDFTSpecC_cf32 *planDFTc2c;
	f32* y;
	cf32* yc;
	u8 * ftbuffer;
	u8 * ftbuffercplx;
private:
	friend ostream& operator<<(ostream&, const PFB*);
	friend ostream& operator<<(ostream&, const PFB&);
	void alloc(void);
	void dealloc(void);
	static void dealloc_static(void);
	void fill_complex_coeffs(void);
	void autoscale_coeffs(void);
};

extern ostream& operator<<(ostream &os, const PFB *p);
extern ostream& operator<<(ostream &os, const PFB &p);

#endif
