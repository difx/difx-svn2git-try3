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
#include <math.h> //fpclassify()

#ifndef DEBUG_V
#define DEBUG_V 0
#endif

#define F_PREC  16  // number of float printout decimals

#include <iostream>
#include <iomanip>
using std::cout;
using std::endl;

/** C'stor clear the chain */
FilterChain::FilterChain() { 
    fchain.clear();
    numchannels = 0;
    for (int i=0; i<128; i++) fchainQuick[i]=NULL;
}

/** D'stor free interal allocs */
FilterChain::~FilterChain() {
    std::vector<Filter*>::iterator fitr = fchain.begin();
    int len = fchain.size();
    while ((len--)>0) {
        delete (*fitr);
        fitr++;
    }
    fchain.clear();
}

/** Initialize filter chain from config file
 *  Normal averaging filter does not use a chain
 *
 *  @arg cfgfile     Configuration file path and name
 *  @arg type        Filter chain type: FLT_IIR, FLT_FIR
 *  @arg parallelism Number of signal channels that will be processed in parallel
 */
void FilterChain::buildFromFile(const char* cfgfile, int parallelism) {
    std::vector<double> vec;
    std::streamsize Nfloats;
    Helpers::parse_numerics_file(vec, cfgfile);
    Nfloats = vec.size();
    if (Nfloats < (1 + 1)) { // Nstages, type{,order,...}
        cout << "Error: Too few values in " << cfgfile << endl;
        return;
    }    
    int Nstages = vec[0];
    int idx = 1;
    this->numchannels = parallelism;
    std::streamsize old_prec = cout.precision();
    for (int stage=0; stage<Nstages; stage++) {
        FltType ftype = static_cast<FltType>((int)(vec[idx++]));
        if (DEBUG_V) { cout << "Stage " << (stage+1) << " type=" << ftype << endl; }
        switch (ftype) {
            case FLT_DECIMATOR:
            {
                int R = vec[idx++];
                DecFilter* flt = new DecFilter();
                flt->init((size_t)0, (size_t)parallelism); // filter order is ignored
                flt->set_coeff(0, R);
                appendFilter(flt);
            } break;
            case FLT_AVERAGING:
            {
                int c_idx = 0;
                IntFilter* flt = new IntFilter();
                flt->init((size_t)1, (size_t)parallelism); // filter order is ignored
                appendFilter(flt);
            } break;
            case FLT_IIR_SOS: 
            {
                int c_idx = 0;
                int N_M = vec[idx++];
                IIRSOSFilter* flt = new IIRSOSFilter();
                flt->init((size_t)N_M, (size_t)parallelism);
                flt->set_prescaling(vec[idx++]);
                for (int n=0; n<(N_M/2); n++) { // (N/2) stages of 2nd order IIRs
                    for (int j=0; j<6; j++) {
                        double c = vec[idx++];
                        if (DEBUG_V) { cout << std::setprecision(F_PREC) << " " << c; }
                        flt->set_coeff(c_idx++, c);
                    }
                    if (DEBUG_V) { cout << endl; }
                }
                appendFilter(flt);
            } break;
            case FLT_FIR:
            {
                int N_M = vec[idx++];
                IIRSOSFilter* flt = new IIRSOSFilter(); // TODO FLT_FIR!
                flt->init((size_t)N_M, (size_t)parallelism);
                flt->set_prescaling(vec[idx++]);
                for (int n=0; n<N_M; n++) {
                    double c = vec[idx++];
                    if (DEBUG_V) { cout << std::setprecision(F_PREC) << " " << c; }
                }
                if (DEBUG_V) { cout << endl; }
                fchain.push_back(flt);
                fchainQuick[stage] = flt;
            } break;
            case FLT_DSVF:
            {
                DSVFFilter* flt = new DSVFFilter();
                flt->init((size_t)2, (size_t)parallelism); // filter oder is ignored
                flt->set_prescaling(vec[idx++]);
                flt->set_coeff(0, vec[idx++]); // f = 2*sin(pi*fc/fs)
                flt->set_coeff(1, vec[idx++]); // q = 1/Q
                appendFilter(flt);
            } break;
            case FLT_MAVG:
            {
                MAvgFilter* flt = new MAvgFilter();
                size_t windowsize = (size_t)vec[idx++]; // "filter order" is size of moving window
                flt->init(windowsize, (size_t)parallelism);
                flt->set_prescaling(vec[idx++]);
                appendFilter(flt);
            } break;
            default:
                cout << " filterchain: unknown or not yet implemented type " << ftype << endl;
                break;
        } //switch
    }//for(stages)
    cout.precision(old_prec);
}


/**
 * Clear filters in the chain.
 */
void FilterChain::clear() {
    std::vector<Filter*>::iterator fitr = fchain.begin();
    int len = fchain.size();
    while ((len--)>0) {
        (*fitr)->clear();
        fitr++;
    }
}

/**
 * Init filters in the chain to identical settings.
 */
void FilterChain::init(size_t order, size_t N) {
    this->clear();
    std::vector<Filter*>::iterator fitr = fchain.begin();
    int len = fchain.size();
    while ((len--)>0) {
        (*fitr)->init(order, N);
        fitr++;
     }
}

/**
 * Append external filter
 */
void FilterChain::appendFilter(Filter* flt) {
    fchainQuick[fchain.size()] = flt;
    fchain.push_back(flt);
}

void FilterChain::set_coeff(int, double) {
   // TODO
}

double FilterChain::get_coeff(int) {
   // TODO
   return 0.0f;
}

void FilterChain::generate_coeffs(double wcutoff) {
   // TODO(?)
}

int FilterChain::get_num_coeffs() {
   // TODO
   return 0;
}

/**
 * Add one sample per channel and process data along the filter chain,
 * taking into account also the decimation factors.
 */
size_t FilterChain::filter(Ipp32fc* in) {

    size_t newsamps = fchainQuick[0]->filter(in);
    if (!newsamps) return 0;

    Ipp32fc *xin = fchainQuick[0]->y();
    for (int i=1; i<fchain.size(); i++) {
        newsamps = fchainQuick[i]->filter(xin);
        if (!newsamps) break;
        xin = fchainQuick[i]->y();
    }

    return newsamps;

#if 0
    std::vector<Filter*>::iterator fitr = fchain.begin();
    int len = fchain.size();
    size_t newsamps = 0;
    for (int i=0; i<len; i++) {
        // input from chain start or output of prev filter
        Ipp32fc *xin = (i==0) ? in : (*(fitr-1))->y();
        // filter, then quit chain if decimator/filter had no new output data
        newsamps = (*fitr)->filter(xin);
        if (!newsamps)
            break;
        fitr++;
    }
#endif

    // catch errors
#if 0
    Ipp32fc* yy = fchain.back()->y();
    bool found_num_fault = false;
    for (int i=1; i<(this->numchannels-1) && !found_num_fault; i++) {
        int fp_re = fpclassify(yy[i].re);
        int fp_im = fpclassify(yy[i].im);
        if (fp_re != FP_ZERO && fp_re != FP_NORMAL) {
            found_num_fault = true;
            std::cout << "FilterChain numeric problem, ch#" << i << ".re fpclassify=" << fp_re << std::endl;
        }
        if (fp_im != FP_ZERO && fp_im != FP_NORMAL) {
            found_num_fault = true;
            std::cout << "FilterChain numeric problem, ch#" << i << ".im fpclassify=" << fp_im << std::endl;
        }
    }
#endif

    return newsamps;
}

/**
 * Return pointer to output of last filter.
 */
Ipp32fc* FilterChain::y() {
    return fchain.back()->y();
}

/**
 * Allow user to specify own result buffer.
 */
void FilterChain::setUserOutbuffer(Ipp32fc* userY) {
    if (fchain.size() > 0) {
        fchain[fchain.size()-1]->setUserOutbuffer(userY);
    }
}

/**
 * Print summary of filter
 */
void FilterChain::summary(std::ostream& o) {
    std::vector<Filter*>::iterator fitr = fchain.begin();
    std::streamsize old_prec = o.precision();
    int len = fchain.size();
    o << "Summary of FilterChain" << endl;
    o << "Number of stages: " << len << endl;
    for (int i=0; i<len; i++) {
        Filter* f = (*fitr);
        int N_coeffs = f->get_num_coeffs();
        o << "Stage " << (i+1) << " " << f->name() << endl;
        o << "  coeffs: ";
        o << std::setprecision(F_PREC);
        for (int ci=0; ci<N_coeffs; ci++) {
            double c = f->get_coeff(ci);
            o << c << " ";
        }
        o << endl;
        fitr++;
    }
    o.precision(old_prec);
}

