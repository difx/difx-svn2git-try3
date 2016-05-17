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

//============================================================================
//
// This C++ class implements a polyphase filter bank (PFB),
// with externally supplied values for the filter coefficients.
//
// Format of the PFB coefficient text file:
//
//  1st line      : number of channels (Nch)
//  2nd line      : number of FIR taps per channel (Ntap)
//  3rd..Nth line : coefficients, one per line, Nch*Ntap coefficients in total
//
// Usually a low-pass filter design (Nch*Ntap -long FIR filter) is used for
// the coefficients, possibly derived using the "windowed sinc" method.
//
//============================================================================
//
// This pfb.cpp can be compiled into a test program by defining UNIT_TEST.
// Usage:  ./pfb [<coeff file>] [<num iterations>]
//
//============================================================================

//============================================================================
// Principle of Polyphase Filter Baks (PFBs)
//
// Useful background infomation:
//  [1] https://en.wikipedia.org/wiki/Filter_bank
//  [2] https://casper.berkeley.edu/wiki/The_Polyphase_Filter_Bank_Technique
//  [3] C. Harris and K. Haines, "A Mathematical Review of Polyphase Filterbank
//      Implementations" for Radio Astronomy" 2011PASA...28..317H
//
// In general, a PFB consists of a bank of identical finite impulse response
// filters (FIR filters). They split a wideband signal into several channels.
//
// Input to the PFB  : one real/complex time domain signal
// Output of the PFB : complex time domain signals in individual channels
//
// Output channels are uniformly spaced and nonoverlapping in frequency.
// The discrete Fourier transform (DFT/FFT) and the PFB are similar; given
// a wideband input signal both produce narrowband complex output signals
// in uniformly spaced nonoverlapping subbands channels.
//
// Comparison      direct FFT              PFB
// Input         1 ch x N samples    1 ch x k*N samples
// Weights       1 weight/channel    k>1 weights/channel
// Output        N ch x 1 sample     Nch x 1 sample
// Resolution    low                 high
// Leakage       high                low
// Amplitude     low accuracy        high accuracy
// Response      sinc(), Hann, ...   arbitrary
//
// A "PFB" with 1 tap (k=1) and weights of unity reduces to a direct FFT.
//
// A "PFB" with 1 tap (k=1) and e.g. Hamming weights reduces to a windowed FFT.
//
// More advanced FFT schemes are possible (windowed overlapped FFT with
// spectral averaging of output bins; FFT filter bank) and can achieve
// a spectral performance similar to that of a simple PFB. Computational
// requirements of the PFB and of the advanced FFT schemes are similar.
// The PFB is however typically much more flexible.
//
// Example calculations in a PFB:
//
//   4 channels out, 4 taps per channel,
//
//   x is 16-sample input data  (4 ch * 4 taps = 16 samples),
//   h are the PFB coefficients (4 ch * 4 taps = 16 coefficients i.e. weights),
//   y is 16 samples of input data weighted by 16 coefficients, folded in time into 4 bins
//   Y is the DFT of those 4 bins, yielding finally the complex amplitudes in 4 frequency channels
//
//   x:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15  (example data; '0' oldest, '15' newest)
//   h: -1 -1 -1 -1 +1 +1 +1 +1 -2 -2 -2 -2 +2 +2 +2 +2  (example coeffs, nonsensical but easiest to follow)
//   y = x .* h
//     =   [ 0  1  2  3] .* [-1 -1 -1 -1]    lag 0
//       + [ 4  5  6  7] .* [+1 +1 +1 +1]    lag 1 ...
//       + [ 8  9 10 11] .* [-2 -2 -2 -2]    lag N-2
//       + [12 13 14 15] .* [+2 +2 +2 +2]    lag N-1
//     =   [12 12 12 12]
//   Y = FFT(y) = FFT([12 12 12 12]) = [48 0+0i 0+0i 0+0i]
//
//   The above is the 4-channel output Y[n] at a single time. The next in time
//   output Y[n+1] is calculated as above, except with the input data x are
//   advanced by 4 samples (by as many samples as there are output channels).
//
// In interferometric FX-type correlators, PFB yields lower amplitude loss
// against delay than FFT due to the PFB's wider input time range per output sample.
// Leakage in spectral channels is lower in PFB, and the per-channel phase corrections
// are largely applied to the correct signal and "noise", i.e., leaked signals from
// other channels multiplied by wrong phase correction, is reduced.
//
//============================================================================


#include "architecture.h"
#include "pfb.h"

#include <assert.h>
#include <malloc.h>
#include <sys/time.h>

#define PFB_MAX_PRTARRAY 8 // length limit related to debug printout of arrays


// STATIC DATA SHARED BETWEEN PFB INSTANCES
// TODO: better method to achieve the same, without 'static'; perhaps coeffs outside class
f32*   PFB::coeffs_contiguous = NULL;
cf32*  PFB::coeffscplx_contiguous = NULL;
f32**  PFB::coeffs = NULL;
cf32** PFB::coeffscplx = NULL;
int    PFB::Nch = 0;
int    PFB::Ntap = 0;
int    PFB::Ncoeff = 0;


/**
 * Constructor, loads filter bank coefficients shared by all instances of the class.
 * @param scoeff An std::istream input text stream containing the PFB description.
 * @param maximize_usage Output direct FFT result during leading/tailing time where PFB has no valid output
 */
PFB::PFB(istream& scoeff, bool maximize_usage)
{
	std::string line;
	int n = 0;

	valid = false;

	if (!std::getline(scoeff, line)) return;
	Nch = ::atoi(line.c_str());

	if (!std::getline(scoeff, line)) return;
	Ntap = ::atoi(line.c_str());

	Ncoeff =  Nch*Ntap;
	if ((Ncoeff < 0) || ((Nch % 4) != 0) || (Ntap <=1)) return;

	PFB::alloc();
	while (std::getline(scoeff, line) && (n < Ncoeff)) {
		coeffs_contiguous[n++] = ::atof(line.c_str());
	}
	PFB::autoscale_coeffs();
	PFB::fill_complex_coeffs();

	valid = (n == Ncoeff);
	use_fft = !(Nch & (Nch-1)); // bitwise test to check for power of 2
	maximize_data_use = maximize_usage;
	iter = 0;
}

/** Copy constructor. Allocates memory and copies PFB state. Shared coefficients are not altered. */
PFB::PFB(const PFB *p)
{
	f32 *tmpcoeffs;
	Nch = p->Nch;
	Ntap = p->Ntap;
	Ncoeff = p->Ncoeff;
	valid = p->valid;
	use_fft = p->use_fft;
	maximize_data_use = p->maximize_data_use;
	iter = p->iter;
	tmpcoeffs = vectorAlloc_f32(p->Ncoeff);
	vectorCopy_f32(p->coeffs_contiguous, tmpcoeffs, p->Ncoeff); // back up 'static' data
	alloc();
	for (int n=0; n<p->Ntap; n++) {
		vectorCopy_f32(p->lags[n], lags[n], p->Nch);
		vectorCopy_cf32(p->lagscplx[n], lagscplx[n], p->Nch);
	}
	vectorCopy_f32(tmpcoeffs, coeffs_contiguous, p->Ncoeff);
	vectorFree(tmpcoeffs);
	fill_complex_coeffs();
}

/** Copy constructor. Allocates memory and copies PFB state. Shared coefficients are not altered. */
PFB::PFB(const PFB &p)
{
	f32 *tmpcoeffs;
	Nch = p.Nch;
	Ntap = p.Ntap;
	Ncoeff = p.Ncoeff;
	valid = p.valid;
	use_fft = p.use_fft;
	maximize_data_use = p.maximize_data_use;
	iter = p.iter;
	tmpcoeffs = vectorAlloc_f32(p.Ncoeff);
	vectorCopy_f32(p.coeffs_contiguous, tmpcoeffs, p.Ncoeff); // back up 'static' data
	alloc();
	for (int n=0; n<p.Ntap; n++) {
		vectorCopy_f32(p.lags[n], lags[n], p.Nch);
	}
	vectorCopy_f32(tmpcoeffs, coeffs_contiguous, p.Ncoeff);
	vectorFree(tmpcoeffs);
	fill_complex_coeffs();
}

/** Allocate internal arrays */
void PFB::alloc()
{
	int ftbuffersize = 0, ftbuffersizec = 0, order = 0;

	assert(Ntap >= 2);
	assert(Nch  >= 32); // 128 byte cache line = 32 floats
	assert((Nch % 4) == 0);

	while ((Nch >> order) != 1) { order++; }

	PFB::dealloc_static(); // ensure static vars are free'd beforehand

	coeffs_contiguous = (f32*)memalign(4096, sizeof(f32)*Ntap*Nch);
	lags_contiguous = (f32*)memalign(4096, sizeof(f32)*Ntap*Nch);
	coeffs = (f32**)malloc(sizeof(f32*)*Ntap);
	lags = (f32**)malloc(sizeof(f32*)*Ntap);
	for (int n=0; n<Ntap; n++) {
		lags[n] = lags_contiguous + n*Nch;
		coeffs[n] = coeffs_contiguous + n*Nch;
	}
	y = vectorAlloc_f32(Nch);

	coeffscplx_contiguous = (cf32*)memalign(4096, sizeof(cf32)*Ntap*Nch);
	lagscplx_contiguous = (cf32*)memalign(4096, sizeof(cf32)*Ntap*Nch);
	coeffscplx = (cf32**)malloc(sizeof(cf32*)*Ntap);
	lagscplx = (cf32**)malloc(sizeof(cf32*)*Ntap);
	for (int n=0; n<Ntap; n++) {
		lagscplx[n] = lagscplx_contiguous + n*Nch;
		coeffscplx[n] = coeffscplx_contiguous + n*Nch;
	}
	yc = vectorAlloc_cf32(Nch);

	lag_idcs = new int[Ntap]();

	if (use_fft) {
		vectorInitFFTR_f32(&planFFTr2c, order, vecFFT_NoReNorm, vecAlgHintFast);
		vectorInitFFTC_cf32(&planFFTc2c, order, vecFFT_NoReNorm, vecAlgHintFast);
		vectorGetFFTBufSizeR_f32(planFFTr2c, &ftbuffersize);
		vectorGetFFTBufSizeC_cf32(planFFTc2c, &ftbuffersizec);
	} else {
		vectorInitDFTR_f32(&planDFTr2c, Nch, vecFFT_NoReNorm, vecAlgHintFast);
		vectorInitDFTC_cf32(&planDFTc2c, Nch, vecFFT_NoReNorm, vecAlgHintFast);
		vectorGetDFTBufSizeR_f32(planDFTr2c, &ftbuffersize);
		vectorGetFFTBufSizeC_cf32(planFFTc2c, &ftbuffersizec);
	}
	ftbuffer = vectorAlloc_u8(ftbuffersize);
	ftbuffercplx = vectorAlloc_u8(ftbuffersizec);

	PFB::reset();
}

/** Free up memory */
void PFB::dealloc(void)
{
	delete[] lag_idcs;
	free(lags_contiguous);
	free(lagscplx_contiguous);
	free(lags);
	free(lagscplx);
	vectorFree(y);
	vectorFree(yc);
	vectorFree(ftbuffer);
	vectorFree(ftbuffercplx);
	if (use_fft) {
		vectorFreeFFTR_f32(planFFTr2c);
		vectorFreeFFTC_cf32(planFFTc2c);
	} else {
		vectorFreeDFTR_f32(planDFTr2c);
		vectorFreeDFTC_cf32(planDFTc2c);
	}

	// PFB::dealloc_static(); // dangerous here since other PFB instances might exist that need the data!
}

/** Free up static allocations */
void PFB::dealloc_static(void)
{
	if (coeffs_contiguous != NULL) {
		free(coeffs_contiguous);
		coeffs_contiguous = NULL;
	}
	if (coeffscplx_contiguous != NULL) {
		free(coeffscplx_contiguous);
		coeffscplx_contiguous = NULL;
	}
	if (coeffs != NULL) {
		free(coeffs);
		coeffs = NULL;
	}
	if (coeffscplx != NULL) {
		free(coeffscplx);
		coeffscplx = NULL;
	}
}

/** Copy real-valued coefficients into the complex coeffs array. */
void PFB::fill_complex_coeffs(void)
{
	for (int n=0; n<Ntap; n++) {
		for (int c=0; c<Nch; c++) {
			coeffscplx[n][c].re = coeffs[n][c];
			coeffscplx[n][c].im = 0.0f;
		}
	}
}

/** Scale the PFB coefficients to get amplitudes equivalent to direct FFT. */
void PFB::autoscale_coeffs(void)
{
	double sum = 0.0, scale;
	for (int n=0; n<Ntap; n++) {
		for (int c=0; c<Nch; c++) {
			sum += coeffs[n][c];
		}
	}
	scale = ((double)Nch) / sum;
	for (int n=0; n<Ntap; n++) {
		for (int c=0; c<Nch; c++) {
			coeffs[n][c] *= scale;
		}
	}
}


/** Reset the internal state of the PFB.
 * Call this function when channelizing with channelize_single() and you changed
 * the input signal source.
 **/
void PFB::reset()
{
	lag_curr = 0;
	iter = 0;
	for (int n=0; n<Ntap; n++) {
		vectorZero_f32(lags[n], Nch);
		vectorZero_cf32(lagscplx[n], Nch);
		lag_idcs[n] = (lag_curr + n) % Ntap;
	}
}

/** Channelize a short Nch-long chunk of samples, yielding one sample per output channel.
 * The previous PFB filter state is remembered. This function can be called repeatedly to
 * feed more input data to the PFB filter. Call reset() to discard filter states.
 * The leading Ntap-1 outputs assume data before the first calls to this function were zero.
 * @param x input sample data, real-valued
 * @param Y output sample data, complex-valued, a sequence of blocks of [ch0 ... ch_Nch-1] samples
 * @param N number of input samples, must equal PFB Nch
 */
void PFB::channelize_single(f32 const *x, cf32 *Y, const int len)
{
	assert(len == Nch);

	// Copy in the new data (need it also for history in future call to channelize_single())
	vectorCopy_f32(x, lags[lag_idcs[Ntap-1]], Nch);

	// Weighted sum of all lags
	vectorMul_f32(lags[lag_idcs[0]], coeffs[0], y, Nch);
	for (int n=1; n<Ntap; n++) {
		vectorAddProduct_f32(lags[lag_idcs[n]], coeffs[n], y, Nch);
	}

	// Transform for final output
	const f32* ft_in  = y;
	if (maximize_data_use && (iter < (Ntap-1))) {
		// use orignal data as FFT input until PFB has had enough input for valid output
		ft_in = x;
		iter++;
	}
	if (use_fft) {
		vectorFFT_RtoC_f32(ft_in, (f32*)Y, planFFTr2c, ftbuffer);
	} else {
		vectorDFT_RtoC_f32(ft_in, (f32*)Y, planDFTr2c, ftbuffer);
	}

	// Move forward in circular (pointer)buffer
	lag_curr = (lag_curr + 1) % Ntap;
	for (int n=0; n<Ntap; n++) {
		lag_idcs[n] = (lag_curr + n) % Ntap;
	}
}

/** Channelize a short Nch-long chunk of samples, yielding one sample per output channel.
 * The previous PFB filter state is remembered. This function can be called repeatedly to
 * feed more input data to the PFB filter. Call reset() to discard filter states.
 * The leading Ntap-1 outputs assume data before the first calls to this function were zero.
 * @param x input sample data, complex-valued
 * @param Y output sample data, complex-valued, a sequence of blocks of [ch0 ... ch_Nch-1] samples
 * @param N number of input samples, must equal PFB Nch
 */
void PFB::channelize_single(cf32 const *x, cf32 *Y, const int len)
{
	assert(len == Nch);

	// Copy in the new data (need it also for history in future call to channelize_single())
	vectorCopy_cf32(x, lagscplx[lag_idcs[Ntap-1]], Nch);

	// Weighted sum of all lags
	vectorMul_cf32(lagscplx[lag_idcs[0]], coeffscplx[0], yc, Nch);
	for (int n=1; n<Ntap; n++) {
		vectorAddProduct_cf32(lagscplx[lag_idcs[n]], coeffscplx[n], yc, Nch);
	}

	// Transform for final output
	const cf32* ft_in = yc;
	if (maximize_data_use && (iter < (Ntap-1))) {
		// use orignal data as FFT input until PFB has had enough input for valid output
		ft_in = x;
		iter++;
	}
	if (use_fft) {
		vectorFFT_CtoC_cf32(ft_in, Y, planFFTc2c, ftbuffercplx);
	} else {
		vectorDFT_CtoC_cf32(ft_in, Y, planDFTc2c, ftbuffercplx);
	}

	// Move forward in circular (pointer)buffer
	lag_curr = (lag_curr + 1) % Ntap;
	for (int n=0; n<Ntap; n++) {
		lag_idcs[n] = (lag_curr + n) % Ntap;
	}
}

/** Channelize a real-valued sample sequence that yields multiple output samples per channel.
 * Previous PFB filter states are not remembered between calls to this function.
 * Output data start with the first valid sample, and trailing Ntap-1 samples assume the data
 * past the end of 'x' are all zero.
 * @param x input sample data, real-valued
 * @param Y output sample data, complex-valued, a sequence of blocks of [ch0 ... ch_Nch-1] samples
 * @param N number of input samples, must be a multiple of PFB Nch
 */
void PFB::channelize(f32 const *x, cf32 *Y, const int len) const
{
	assert((len >= Nch) && ((len % Nch) == 0));
	f32 const* x_end = x + len - (Ntap-1)*Nch;
	f32* Y_f32 = (f32*)Y;

	while (x < x_end) {

		// Weighted sum of all lags
		vectorMul_f32(x, coeffs[0], y, Nch);
		for (int n=1; n<Ntap; n++) {
			vectorAddProduct_f32(x + n*Nch, coeffs[n], y, Nch);
		}

		// Transform for final output
		if (use_fft) {
			vectorFFT_RtoC_f32(y, Y_f32, planFFTr2c, ftbuffer);
		} else {
			vectorDFT_RtoC_f32(y, Y_f32, planDFTr2c, ftbuffer);
		}

		// Next input & output
		x += Nch;
		Y_f32 += Nch;
	}
}

/** Stream printout of PFB settings ("toString()") */
ostream& operator<<(ostream &os, const PFB *p) {
        os << "PFB(valid=" << p->valid
		<< ", max_use=" << p->maximize_data_use
		<< ", FFT=" << p->use_fft
                << ", Nch=" << p->Nch
                << " x Ntap=" << p->Ntap
                << ", Ncoeff=" << p->Ncoeff;
	if (!p->valid) {
		return os << ")";
	}
        os << "; [";
        if (p->Ncoeff < 32) {
                for (int i=0; i<p->Ncoeff; i++) {
                        os << p->coeffs_contiguous[i] << " ";
                }
        } else {
                for (int i=0; i<8; i++) {
                        os << p->coeffs_contiguous[i] << " ";
                }
                os << "... ";
                for (int i=p->Ncoeff-8; i<p->Ncoeff; i++) {
                        os << p->coeffs_contiguous[i] << " ";
                }
        }
        os << "])";
        return os;
}

/** Stream printout of PFB settings ("toString()") */
ostream& operator<<(ostream &os, const PFB &p) {
	return os << &p;
}



//===========================================================================
// Embedded test program
//===========================================================================

#ifdef UNIT_TEST

class Timing {
public:
	Timing(const int nsamp) : Nsamp(nsamp) {
		gettimeofday(&tv0, NULL);
	}
	~Timing() {
		gettimeofday(&tv1, NULL);
		double dt = (tv1.tv_sec - tv0.tv_sec) + 1e-6*(tv1.tv_usec - tv0.tv_usec);
		cout << " time=" << (dt*1e3) << " msec, " << (Nsamp/dt)*1e-6 << " Ms/s" << endl;
	}
private:
	const int Nsamp;
	struct timeval tv0, tv1;
};

int main(int argc, char** argv)
{
	int n;
	bool maximize = true;

	// Mandatory for Intel Integr.Perf.Prim
	ippInit();

	// Test: bad input coeffs
	if (0) {
		ifstream dummyfile("/tmp/pfb-none");
		PFB *p = new PFB(dummyfile);
		cout << p << endl;
		delete p;
	}

	// Load actual coeffs
	const char *cfile;
	if (argc >= 2) {
		cfile = argv[1];
	} else {
		cfile = "test_1.pfb";
	}
	ifstream coeffile(cfile);

	PFB pfb(coeffile, maximize);
	cout << pfb << endl;

	int Nch = pfb.getNch();
	int Nblocks = 4*pfb.getNtaps();

	if (argc >= 3) {
		Nblocks = atoi(argv[2]);
		Nblocks = (Nblocks > 0 && Nblocks < 1e6) ? Nblocks : 4*pfb.getNtaps();
	}

	f32 *in = vectorAlloc_f32(Nch*Nblocks);
	cf32 *incplx = vectorAlloc_cf32(Nch*Nblocks);
	cf32 *out = vectorAlloc_cf32(Nch*Nblocks);
	f32 *out_f32 = (f32*)out;
	vectorSet_f32(1.0f, in, Nch*Nblocks);
	vectorSet_f32(1.0f, (f32*)incplx, 2*Nch*Nblocks);
	vectorSet_f32(0.0f, out_f32, 2*Nch*Nblocks);

	// Test: continous chunk of input data
	if (1) {

		cout << "Running PFB::channelize() : ";
		Timing *t = new Timing(Nch*Nblocks);
		pfb.channelize(in, out, Nch*Nblocks);
		delete t;
		pfb.reset();

		// Report the results
		for (int blk=0; blk<Nblocks; blk++) {
			cout << "t[" << blk << "] out : ";
			for (n=0; n<PFB_MAX_PRTARRAY && n<Nch; n++) {
				cout << out_f32[n + blk*Nch] << " ";
			}
			if (n < Nch) {
				cout << "... ";
			}
			cout << endl;
		}
		cout << " ... " << endl;
	}

	// Test: pieces of Nch-long input data
	if (1) {

		cout << "Running PFB::channelize_single() : ";
		Timing *t = new Timing(Nch*Nblocks);
		for (int blk=0; blk<Nblocks; blk++) {
			pfb.channelize_single(in + Nch*blk, out, Nch);
		}
		delete t;
		pfb.reset();

		// Re-run for printouts, without timing
		PFB pfb2(pfb);
		cout << pfb2 << endl;
		for (int blk=0; blk<Nblocks; blk++) {
			pfb2.channelize_single(in + Nch*blk, out, Nch);
			cout << "t[" << blk << "] out : ";
			for (n=0; n<PFB_MAX_PRTARRAY && n<Nch; n++) {
				cout << out_f32[n] << " ";
			}
			if (n < Nch) {
				cout << "... ";
			}
			cout << endl;
		}
	}

	// Test: pieces of complex input data
	if (1) {

		cout << "Running PFB::channelize_single(complex) : ";
		Timing *t = new Timing(Nch*Nblocks);
		for (int blk=0; blk<Nblocks; blk++) {
			pfb.channelize_single(incplx + Nch*blk, out, Nch);
		}
		delete t;
		pfb.reset();

		// Re-run for printouts, without timing
		PFB pfb2(pfb);
		cout << pfb2 << endl;
		for (int blk=0; blk<Nblocks; blk++) {
			pfb2.channelize_single(incplx + Nch*blk, out, Nch);
			cout << "t[" << blk << "] out : ";
			for (n=0; n<PFB_MAX_PRTARRAY && n<Nch; n++) {
				cout << out_f32[n] << " ";
			}
			if (n < Nch) {
				cout << "... ";
			}
			cout << endl;
		}
	}

	vectorFree(in);
	vectorFree(incplx);
	vectorFree(out);

	return 0;
}
#endif

