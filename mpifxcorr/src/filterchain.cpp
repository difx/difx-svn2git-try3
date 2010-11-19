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

#define DEBUG_V 0
#define F_PREC  16  // number of float printout decimals

#include <iostream>
#include <iomanip>
using std::cout;
using std::endl;

/** C'stor clear the chain */
FilterChain::FilterChain() { 
    fchain.clear();
    rchain.clear();
    countchain.clear();
}

/** D'stor free interal allocs */
FilterChain::~FilterChain() {
    std::vector<int*>::iterator citr = countchain.begin();
    std::vector<Filter*>::iterator fitr = fchain.begin();
    int len = countchain.size();
    while ((len--)>0) {
        delete (*citr);
        delete (*fitr);
        *citr = 0;
        *fitr = 0;
        citr++;
        fitr++;
    }
    countchain.clear();
    rchain.clear();
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
    if (Nfloats < (1 + 3)) { // Nstages, {type, decim, order}
        cout << "Error: Too few values in " << cfgfile << endl;
        return;
    }    
    int Nstages = vec[0];
    int idx = 1;
    std::streamsize old_prec = cout.precision();
    for (int stage=0; stage<Nstages; stage++) {
        FltType ftype = static_cast<FltType>((int)(vec[idx++]));
        int R = vec[idx++];
        int N_M = vec[idx++];
        if (DEBUG_V) { cout << "Stage " << (stage+1) << " type=" << ftype << " decim=" << R << " order=" << N_M << endl; }
        switch (ftype) {
            case FLT_AVERAGING:
            {
                int c_idx = 0;
                IntFilter* flt = new IntFilter();
                flt->init((size_t)N_M, (size_t)parallelism);
                this->appendFilter(flt, R);
            } break;
            case FLT_IIR_SOS: 
            {
                int c_idx = 0;
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
                this->appendFilter(flt, R);
            } break;
            case FLT_FIR:
            {
                IIRSOSFilter* flt = new IIRSOSFilter(); // TODO
                flt->init((size_t)N_M, (size_t)parallelism);
                flt->set_prescaling(vec[idx++]);
                for (int n=0; n<N_M; n++) {
                    double c = vec[idx++];
                    if (DEBUG_V) { cout << std::setprecision(F_PREC) << " " << c; }
                }
                if (DEBUG_V) { cout << endl; }
                this->appendFilter(flt, R);
            } break;
            case FLT_DSVF:
            {
                DSVFFilter* flt = new DSVFFilter();
                flt->init((size_t)N_M, (size_t)parallelism); // filter oder is ignored
                flt->set_prescaling(vec[idx++]);
                flt->set_coeff(0, vec[idx++]); // f = 2*sin(pi*fc/fs)
                flt->set_coeff(1, vec[idx++]); // q = 1/Q
                this->appendFilter(flt, R);
            } break;
            default:
                cout << " filterchain: unknown or not yet implemented type " << ftype << endl;
                break;
        } //switch
    }//for(stages)
    cout.precision(old_prec);
}


/**
 * Append filter to end of chain.
 * @arg fp Pointer to filter object to add
 * @arg R  Decimation factor to apply to filter input (R=0,R=1: no decimation)
 */
void FilterChain::appendFilter(Filter* fp, int R) {
    int* Rp = new int(0);
    fchain.push_back(fp);
    rchain.push_back(R);
    countchain.push_back(Rp);
}

/**
 * Clear filters in the chain.
 */
void FilterChain::clear() {
    std::vector<Filter*>::iterator fitr = fchain.begin();
    std::vector<int*>::iterator citr = countchain.begin();
    int len = fchain.size();
    while ((len--)>0) {
        (*fitr)->clear();
        *(*citr) = 0;
        fitr++;
        citr++;
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

void FilterChain::set_coeff(int, double) {
   // TODO
}

double FilterChain::get_coeff(int) {
   // TODO
   return 0.0f;
}

int FilterChain::get_num_coeffs() {
   // TODO
   return 0;
}

/**
 * Add one sample per channel and process data along the filter chain,
 * taking into account also the decimation factors.
 */
void FilterChain::filter(Ipp32fc* in) {
    std::vector<Filter*>::iterator fitr = fchain.begin();
    std::vector<int>::iterator ritr = rchain.begin();
    std::vector<int*>::iterator citr = countchain.begin();
    int len = fchain.size();
    for (int i=0; i<len; i++) {
        (**citr)++;
        if ((**citr) >= *ritr) {
            // Target R reached, take output of prev filter as input of curr filter
            Ipp32fc *xin = (i==0) ? in : (*(fitr-1))->y();
            (*fitr)->filter(xin);
            (**citr) = 0;
        }
        fitr++;
        citr++;
        ritr++;
    }
}

/**
 * Return pointer to output of last filter.
 */
Ipp32fc* FilterChain::y() {
    std::vector<Filter*>::iterator fitr = fchain.end() - 1;
    return (*fitr)->y();
}

/**
 * Allow user to specify own result buffer.
 */
void FilterChain::setUserOutbuffer(Ipp32fc* userY) {
    fchain[fchain.size()-1]->setUserOutbuffer(userY);
}

/**
 * Print summary of filter
 */
void FilterChain::summary(std::ostream& o) {
    std::vector<Filter*>::iterator fitr = fchain.begin();
    std::vector<int>::iterator ritr = rchain.begin();
    std::vector<int*>::iterator citr = countchain.begin();
    std::streamsize old_prec = o.precision();
    int len = fchain.size();
    o << "Summary of FilterChain" << endl;
    o << "Number of stages: " << len << endl;
    for (int i=0; i<len; i++) {
        Filter* f = (*fitr);
        int R_goal = (*ritr);
        int R_count = *(*citr);
        int N_coeffs = f->get_num_coeffs();
        o << "Stage " << (i+1) << " " << f->name() << " filter, C=" << R_count << "/R=" << R_goal << endl;
        o << "  coeffs: ";
        o << std::setprecision(F_PREC);
        for (int ci=0; ci<N_coeffs; ci++) {
            double c = f->get_coeff(ci);
            o << c << " ";
        }
        o << endl;
        fitr++;
        citr++;
        ritr++;
    }
    o.precision(old_prec);
}

