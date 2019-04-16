#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifndef __STDC_CONSTANT_MACROS
#  define __STDC_CONSTANT_MACROS
#endif
#ifndef __STDC_LIMIT_MACROS
#  define __STDC_LIMIT_MACROS
#endif
#ifndef _ISOC99_SOURCE
#  define _ISOC99_SOURCE
#endif
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif
#ifndef __USE_ISOC99
#  define __USE_ISOC99 1
#endif
#ifndef _ISOC99_SOURCE
#  define _ISOC99_SOURCE
#endif
#ifndef __USE_MISC
#  define __USE_MISC 1
#endif
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include <inttypes.h>
#include <limits>
#ifdef __cplusplus
#  include <cstddef>
#else
#  include <stddef.h>
#endif
#include <stdint.h>
// we want to use ISO C9X stuff
// we want to use some GNU stuff
// But this sometimes breaks time.h
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <error.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>



#include "fftw3.h"
#include "JMA_math.h"

#define FFTW_EST_MODE FFTW_MEASURE // FFTW_ESTIMATE or FFTW_MEASURE or FFTW_EXHAUSTIVE

static const char wisdom_file[] = ".fftw3f_wisdom";
static const int_fast32_t ZERO_PAD_FACTOR = 2;
static const int_fast32_t NUM_PEAKS = 100;
static const int_fast32_t NUM_PEAKS_PRINT = 8;


struct peak_pos_struct {
    int_fast32_t freq_pos;
    int_fast32_t time_pos;
    Real32_t     value;
};



void blackman_window(const int_fast32_t N,
                     Real32_t* const restrict data)
{
    const Real64_t alpha = 0.16;
    Real64_t a0 = (1.0-alpha)*0.5;
    Real64_t a1 = 0.5;
    Real64_t a2 = alpha*0.5;
    Real64_t mult1 = M_2PI/(N-1);
    Real64_t mult2 = 2.0*M_2PI/(N-1);
    for(int_fast32_t n=0, c=0; n < N; n++, c+=2) {
        Real32_t w(a0-a1*cos(n*mult1)+a2*cos(n*mult2));
        if((n&0x1)==0x1) {
            // incorporate zero shift term.
            w = -w;
        }
        data[c] = w;
        data[c+1] = w;
    }
    return;
}

void hanning_window_freq(const int_fast32_t N,
                         Real32_t* const restrict data)
{
    if(N<16) {
        fprintf(stderr, "Not enough channels for proper fringe finding\n");
        exit(2);
    }
    int_fast32_t ten_percent(N*0.1f);
    if(ten_percent < 1) {
        ten_percent = 1;
    }
    const int_fast32_t M = N-2*ten_percent;
    Real64_t mult1 = M_2PI/(M-1);
    for(int_fast32_t n=0, c=0; n < N; n++, c+=2) {
        if((n>=ten_percent)&&(n<N-ten_percent)) {
            Real32_t w(0.5*(1.0-cos((n-ten_percent)*mult1)));
            if((n&0x1)==0x1) {
                // incorporate zero shift term.
                w = -w;
            }
            data[c] = w;
            data[c+1] = w;
        }
        else {
            data[c] = 0.0f;
            data[c+1] = 0.0f;
        }
    }
    return;
}


void hanning_window_time(const int_fast32_t N,
                         Real32_t* const restrict data)
{
    Real64_t mult1 = M_2PI/(N-1);
    for(int_fast32_t n=0, c=0; n < N; n++, c+=2) {
        Real32_t w(0.5*(1.0-cos(n*mult1)));
        if((n&0x1)==0x1) {
            // incorporate zero shift term.
            w = -w;
        }
        data[c] = w;
        data[c+1] = w;
    }
    return;
}



void uniform_window_freq(const int_fast32_t N,
                         Real32_t* const restrict data)
{
    if(N<16) {
        fprintf(stderr, "Not enough channels for proper fringe finding\n");
        exit(2);
    }
    // flag the edge channels
    int_fast32_t ten_percent(N*0.1f);
    if(ten_percent < 1) {
        ten_percent = 1;
    }
    for(int_fast32_t n=0, c=0; n < N; n++, c+=2) {
        if((n>=ten_percent)&&(n<N-ten_percent)) {
            Real32_t w = 1.0f;
            if((n&0x1)==0x1) {
                // incorporate zero shift term.
                w = -w;
            }
            data[c] = w;
            data[c+1] = w;
        }
        else {
            data[c] = 0.0f;
            data[c+1] = 0.0f;
        }
    }
    return;
}


void uniform_window_time(const int_fast32_t N,
                         Real32_t* const restrict data)
{
    for(int_fast32_t n=0, c=0; n < N; n++, c+=2) {
        Real32_t w = 1.0f;
        if((n&0x1)==0x1) {
            // incorporate zero shift term.
            w = -w;
        }
        data[c] = w;
        data[c+1] = w;
    }
    return;
}






int_fast32_t fft_in_frequency(FILE* const restrict fp_in,
                              FILE* const restrict fp_out,
                              const int_fast32_t NUM_CHANNELS,
                              const int_fast32_t TIME_SLOT_START,
                              const int_fast32_t NUM_TIME_SLOTS_PROCESS) throw()
{
    // check that we have an even channel size
    if((NUM_CHANNELS&0x1) == 0x1) {
        fprintf(stderr, "Error: even number of channels required\n");
        exit(2);
    }
    // allocate memory.  Extend-pad by a factor of ZERO_PAD_FACTOR for more resolution
    Real32_t* restrict data_in;
    Real32_t* restrict data_out;
    Real32_t* restrict window;
    //Real32_t* restrict channel_sum;
    void* data_in_void;
    void* data_out_void;
    void* window_void;
    //void* channel_sum_void;
    const int_fast32_t NUM_REAL32_T_VALUES = NUM_CHANNELS*2;
    const size_t NUM_BYTES_IN  = size_t(NUM_REAL32_T_VALUES)*sizeof(Real32_t);
    //const size_t NUM_BYTES_SUM  = size_t(NUM_CHANNELS)*sizeof(Real32_t);
    const int_fast32_t NUM_FFT_CHANNELS = NUM_CHANNELS*ZERO_PAD_FACTOR;
    const size_t NUM_BYTES_OUT = size_t(NUM_FFT_CHANNELS*2)*sizeof(Real32_t);
    printf("FFT along the channel direction for many timeslots\n");
    //printf("processing %d channel data\n", int(NUM_CHANNELS));
    //printf("processing %d time slots\n", int(NUM_TIME_SLOTS_PROCESS));
    //printf("processing starting at time slot %d\n", int(TIME_SLOT_START));
    //printf("allocating arrays of %llu bytes\n", (unsigned long long)(NUM_BYTES_OUT));
    if(posix_memalign(&data_in_void,16, NUM_BYTES_OUT) != 0)
    {
        fprintf(stderr, "Error: could not allocate %llu bytes for channel data_in\n", (unsigned long long)(NUM_BYTES_OUT));
        exit(3);
    }
    if(posix_memalign(&data_out_void,16, NUM_BYTES_OUT) != 0)
    {
        fprintf(stderr, "Error: could not allocate %llu bytes for channel data_out\n", (unsigned long long)(NUM_BYTES_OUT));
        exit(3);
    }
    if(posix_memalign(&window_void,16, NUM_BYTES_IN) != 0)
    {
        fprintf(stderr, "Error: could not allocate %llu bytes for window\n", (unsigned long long)(NUM_BYTES_IN));
        exit(3);
    }
//     if(posix_memalign(&channel_sum_void,16, NUM_BYTES_SUM) != 0)
//     {
//         fprintf(stderr, "Error: could not allocate %llu bytes for channel_sum\n", (unsigned long long)(NUM_BYTES_SUM));
//         exit(3);
//     }
    data_in = reinterpret_cast<Real32_t* restrict>(data_in_void);
    data_out = reinterpret_cast<Real32_t* restrict>(data_out_void);
    window = reinterpret_cast<Real32_t* restrict>(window_void);
//    channel_sum = reinterpret_cast<Real32_t* restrict>(channel_sum_void);
    uniform_window_freq(NUM_CHANNELS, window);
//    memset(reinterpret_cast<void*>(channel_sum), 0, NUM_BYTES_SUM);

    // get the plan
    fftwf_plan plan = fftwf_plan_dft_1d(int(NUM_FFT_CHANNELS),
                                        reinterpret_cast<fftwf_complex *>(data_in),
                                        reinterpret_cast<fftwf_complex *>(data_out),
                                        FFTW_FORWARD,
                                        FFTW_DESTROY_INPUT|FFTW_EST_MODE);

//     // seek to the correct location in the file to start
//     rewind(fp_in);
//     if(fseek(fp_in, long(NUM_BYTES_IN*TIME_SLOT_START), SEEK_SET) != 0) {
//         fprintf(stderr, "Error: could not seek to %llu bytes in input file\n", (unsigned long long)(NUM_BYTES_IN*TIME_SLOT_START));
//         exit(2);
//     }
//     // loop through the data to get the mean channel amplitude
//     // now loop through the data
//     for(int_fast32_t s=0; s < NUM_TIME_SLOTS_PROCESS; s++) {
//         if(fread(reinterpret_cast<void*>(data_in), NUM_BYTES_IN, 1, fp_in) != 1)
//         {
//             fprintf(stderr, "Error: could not read data for timeslot %llu from input file\n", (unsigned long long)(s+TIME_SLOT_START));
//             exit(2);
//         }
//         for(int_fast32_t ii=0, ic=0; ii < NUM_CHANNELS; ii++, ic+=2) {
//             channel_sum[ii] += hypotf(data_in[ic],data_in[ic+1]);
//         }
//     }    
//     for(int_fast32_t ii=0, ic=0; ii < NUM_CHANNELS; ii++, ic+=2) {
//         channel_sum[ii] *= 1.0f/NUM_CHANNELS;
//         printf("Channel %6d mean %12.4E\n", int(ii), channel_sum[ii]);
//         window[ic] *= 1.0f/channel_sum[ii];
//         window[ic+1] *= 1.0f/channel_sum[ii];
//     }

    // seek to the correct location in the file to start
    rewind(fp_in);
    if(fseek(fp_in, long(NUM_BYTES_IN*TIME_SLOT_START), SEEK_SET) != 0) {
        fprintf(stderr, "Error: could not seek to %llu bytes in input file\n", (unsigned long long)(NUM_BYTES_IN*TIME_SLOT_START));
        exit(2);
    }
    // now loop through the data
    for(int_fast32_t s=0; s < NUM_TIME_SLOTS_PROCESS; s++) {
        memset(reinterpret_cast<void*>(data_in), 0, NUM_BYTES_OUT);
        if(fread(reinterpret_cast<void*>(data_in), NUM_BYTES_IN, 1, fp_in) != 1)
        {
            fprintf(stderr, "Error: could not read data for timeslot %llu from input file\n", (unsigned long long)(s+TIME_SLOT_START));
            exit(2);
        }
//         for(int_fast32_t ii=0; ii < NUM_CHANNELS; ii++) {
//             int_fast32_t jj=ii*2;
//             Real32_t x(ii*0.25*M_2PI+0.1*s);
//             Real32_t si,co;
//             sincosf(x, &si, &co);
//             data_in[jj]=co;
//             data_in[jj+1]=si;
//         }
        // apply the window function, which also applies the zero-shift
        for(int_fast32_t co=0; co < NUM_REAL32_T_VALUES; co++) {
            data_in[co] *= window[co];
        }
        // do the FFT
        fftwf_execute(plan);
//         for(int_fast32_t ii=0; ii < NUM_FFT_CHANNELS; ii++) {
//             int_fast32_t jj=ii*2;
//             printf("%3d %15.7E\n", int(ii), hypotf(data_out[jj], data_out[jj+1]));
//          }
        // write out the data
        if(fwrite(reinterpret_cast<void*>(data_out), NUM_BYTES_OUT, 1, fp_out) != 1)
        {
            fprintf(stderr, "Error: could not write slot %llu to temp output\n", (unsigned long long)(s));
            exit(2);
        }
    }
    // destroy the plan
    fftwf_destroy_plan(plan);
    //free(channel_sum_void); channel_sum_void = NULL; channel_sum = NULL;
    free(window_void); window_void = NULL; window = NULL;
    free(data_out_void); data_out_void = NULL; data_out = NULL;
    free(data_in_void); data_in_void = NULL; data_in = NULL;
    return 0;
}






inline void complex_to_amplitude(const int_fast32_t NUM_COMPLEX,
                                 Real32_t* const restrict data)
{
    // in-place conversion from complex to amplitude
    for(int_fast32_t a=0, c=0; a < NUM_COMPLEX; a++, c+=2) {
        data[a] = hypotf(data[c],data[c+1]);
    }
    return;
}


void get_RMS_peaks_components_slots(const int_fast32_t FREQ_POS,
                                    const int_fast32_t NUM_SAMPLES,
                                    const Real32_t* const restrict data,
                                    const int_fast32_t NUM_PEAKS,
                                    struct peak_pos_struct* const restrict peak,
                                    int_fast64_t* const restrict num_samples,
                                    Real64_t* const restrict sum_tot,
                                    Real64_t* const restrict sumsqr_tot)
{
    Real64_t sum=0.0;
    Real64_t sumsqr = 0.0;
    Real32_t bottom_peak = peak[NUM_PEAKS-1].value;
    for(int_fast32_t i=0; i < NUM_SAMPLES; i++) {
        Real64_t d = data[i];
        sum += d;
        sumsqr += d*d;
    }
    *num_samples += NUM_SAMPLES;
    *sum_tot += sum;
    *sumsqr_tot += sumsqr;
    for(int_fast32_t i=0; i < NUM_SAMPLES; i++) {
        if(data[i] > bottom_peak) {
            struct peak_pos_struct p;
            p.freq_pos = FREQ_POS;
            p.time_pos = i;
            p.value = data[i];
            struct peak_pos_struct temp;
            for(int_fast32_t j=0; j < NUM_PEAKS; j++) {
                if(p.value > peak[j].value) {
                    // higher, bubble down
                    temp = peak[j];
                    peak[j] = p;
                    p = temp;
                }
            }
            bottom_peak = peak[NUM_PEAKS-1].value;
        }
    }
    return;
}


int_fast32_t peak_not_neighbor(const int_fast32_t PEAK_NUM,
                               struct peak_pos_struct* const restrict peak)
{
    for(int_fast32_t p=0; p < PEAK_NUM; p++) {
        int_fast32_t df = peak[p].freq_pos-peak[PEAK_NUM].freq_pos;
        int_fast32_t dt = peak[p].time_pos-peak[PEAK_NUM].time_pos;
        if( (df>=-1) && (df<=+1) ) {
            if( (dt>=-1) && (dt<=+1) ) {
                return 0;
            }
        }
    }
    return 1;
}






int_fast32_t fft_in_time(const int fd_in,
                         FILE* const restrict fp_out,
                         const Real32_t Channel_Bandwidth,
                         const Real32_t Integration_Time,
                         const int_fast32_t NUM_FFT_CHANNELS,
                         const int_fast32_t NUM_SLOTS_AT_ONCE,
                         const int_fast32_t NUM_SLOTS_STAGGER,
                         const int_fast32_t NUM_STAGGER) throw()
{
    // check that we have an even slot size
    if((NUM_SLOTS_AT_ONCE&0x1) == 0x1) {
        fprintf(stderr, "Error: even number of slots at once required\n");
        exit(2);
    }
    // allocate memory.  Extend-pad by a factor of ZERO_PAD_FACTOR for more resolution
    Real32_t* restrict data_in;
    Real32_t* restrict data_out;
    Real32_t* restrict window;
    void* data_in_void;
    void* data_out_void;
    void* window_void;
    //const int_fast32_t NUM_RAW_CHANNELS = NUM_FFT_CHANNELS/ZERO_PAD_FACTOR;
    const int_fast32_t NUM_REAL32_T_VALUES = NUM_SLOTS_AT_ONCE*2;
    const size_t NUM_BYTES_IN  = size_t(NUM_REAL32_T_VALUES)*sizeof(Real32_t);
    const int_fast32_t NUM_FFT_SLOTS = NUM_SLOTS_AT_ONCE*ZERO_PAD_FACTOR;
    const size_t NUM_BYTES_OUT = size_t(NUM_FFT_SLOTS*2)*sizeof(Real32_t);
    const size_t NUM_BYTES_WRITE = size_t(NUM_FFT_SLOTS)*sizeof(Real32_t);
    printf("FFT along the time direction for many channels, for %d stagger groups\n", int(NUM_STAGGER));
    //printf("processing %d channel data\n", int(NUM_FFT_CHANNELS));
    //printf("processing %d time slots\n", int(NUM_SLOTS_AT_ONCE));
    //printf("processing %d staggers\n", int(NUM_STAGGER));
    //printf("allocating arrays of %llu bytes\n", (unsigned long long)(NUM_BYTES_OUT));
    if(posix_memalign(&data_in_void,16, NUM_BYTES_OUT) != 0)
    {
        fprintf(stderr, "Error: could not allocate %llu bytes for slot data_in\n", (unsigned long long)(NUM_BYTES_OUT));
        exit(3);
    }
    if(posix_memalign(&data_out_void,16, NUM_BYTES_OUT) != 0)
    {
        fprintf(stderr, "Error: could not allocate %llu bytes for slot data_out\n", (unsigned long long)(NUM_BYTES_OUT));
        exit(3);
    }
    if(posix_memalign(&window_void,16, NUM_BYTES_IN) != 0)
    {
        fprintf(stderr, "Error: could not allocate %llu bytes for window\n", (unsigned long long)(NUM_BYTES_IN));
        exit(3);
    }
    data_in = reinterpret_cast<Real32_t* restrict>(data_in_void);
    data_out = reinterpret_cast<Real32_t* restrict>(data_out_void);
    window = reinterpret_cast<Real32_t* restrict>(window_void);
    uniform_window_time(NUM_SLOTS_AT_ONCE, window);

    // get the plan
    fftwf_plan plan = fftwf_plan_dft_1d(int(NUM_FFT_SLOTS),
                                        reinterpret_cast<fftwf_complex *>(data_in),
                                        reinterpret_cast<fftwf_complex *>(data_out),
                                        FFTW_FORWARD,
                                        FFTW_DESTROY_INPUT|FFTW_EST_MODE);

    // mmap the input file
    const size_t SLOT_OFFSET = NUM_FFT_CHANNELS *2;
    const size_t STAGGER_OFFSET = NUM_SLOTS_STAGGER * SLOT_OFFSET;
    size_t MMAP_BYTES = size_t(NUM_SLOTS_AT_ONCE) * SLOT_OFFSET;
    for(int_fast32_t st=1; st < NUM_STAGGER; st++) {
        MMAP_BYTES += STAGGER_OFFSET;
    }
    MMAP_BYTES *= sizeof(Real32_t);
    
    void* memory_map = mmap(NULL, MMAP_BYTES, PROT_READ,
                            MAP_SHARED|MAP_NORESERVE,
                            fd_in, 0);
    if(memory_map == MAP_FAILED) {
        perror("error memory mapping channel FFT file");
        exit(2);
    }
    const Real32_t* restrict file_in =
        reinterpret_cast<const Real32_t* restrict>(memory_map);
    // Now run through the staggers
    size_t pos_stagger_start = 0;
    for(int_fast32_t st = 0; st < NUM_STAGGER; st++, pos_stagger_start += STAGGER_OFFSET) {
        size_t pos_chan_start = pos_stagger_start;
        struct peak_pos_struct peak[NUM_PEAKS];
        for(int_fast32_t p=0; p < NUM_PEAKS; p++) {
            peak[p].freq_pos = -1;
            peak[p].time_pos = -1;
            peak[p].value = -1E20f;
        }
        int_fast64_t num_pixels = 0;
        Real64_t sum=0.0;
        Real64_t sumsqr = 0.0;
        for(int_fast32_t ch=0; ch < NUM_FFT_CHANNELS; ch++, pos_chan_start+=2) {
            size_t pos_slot_start = pos_chan_start;
            memset(reinterpret_cast<void*>(data_in), 0, NUM_BYTES_OUT);
            // apply the window function, which also applies the zero-shift
            for(int_fast32_t s=0; s < NUM_SLOTS_AT_ONCE; s+=2, pos_slot_start+=SLOT_OFFSET) {
                data_in[s] = file_in[pos_slot_start]*window[s];
                data_in[s+1] = file_in[pos_slot_start+1]*window[s+1];
            }
            // do the FFT
            fftwf_execute(plan);
            // convert to amplitude
            complex_to_amplitude(NUM_FFT_SLOTS, data_out);
            // statistics
            get_RMS_peaks_components_slots(ch, NUM_FFT_SLOTS, data_out,
                                           NUM_PEAKS, peak,
                                          &num_pixels, &sum, &sumsqr);
            // write out the data, which is now single Real32_t values, not
            // complex, so the size is NUM_BYTES_WRITE
            if(fwrite(reinterpret_cast<void*>(data_out), NUM_BYTES_WRITE, 1, fp_out) != 1)
            {
                fprintf(stderr, "Error: could not write channel %llu to final output\n", (unsigned long long)(ch));
                exit(2);
            }
        }
        // print statistics
        Real64_t mean = sum / num_pixels;
        Real64_t stddev = (sumsqr - sum*mean)/(num_pixels-1);
        if(stddev < 0.0) {stddev = 0.0;}
        stddev = sqrt(stddev);
        printf("Stagger %3d Mean %15.7E StdDev %15.7E\n", int(st), mean, stddev);
        if(stddev == 0.0) {stddev = 1.0;}
        int_fast64_t min_dist = std::numeric_limits<int_fast64_t>::max();
        int_fast32_t pos = -1;
        for(int_fast32_t p=0,pc=0; p < NUM_PEAKS; p++) {
            if((peak_not_neighbor(p, peak))) {
                printf("Stagger %3d Peak %2d Delay_Pos %6d %11.3E [s] Rate_Pos %6d %11.3E [Hz] Value %15.7E\n",
                       int(st), int(p), int(peak[p].freq_pos-NUM_FFT_CHANNELS/2),
                       Real64_t(peak[p].freq_pos-NUM_FFT_CHANNELS/2)/NUM_FFT_CHANNELS/Channel_Bandwidth,
                       int(peak[p].time_pos-NUM_FFT_SLOTS/2),
                       Real64_t(peak[p].time_pos-NUM_FFT_SLOTS/2)/NUM_FFT_SLOTS/Integration_Time,
                       (peak[p].value-mean)/stddev);
                int_fast64_t dist_f(peak[p].freq_pos-NUM_FFT_CHANNELS/2);
                int_fast64_t dist_t(peak[p].time_pos-NUM_FFT_SLOTS/2);
                int_fast64_t dist = dist_f*dist_f+dist_t*dist_t;
                if(dist < min_dist) {
                    min_dist = dist;
                    pos = p;
                }
                pc++;
                if(pc > NUM_PEAKS_PRINT) {
                    break;
                }
            }
        }
        printf("Peak closest to 0,0\n");
        printf("Stagger %3d Peak %2d Delay_Pos %6d %11.3E [s] Rate_Pos %6d %11.3E [Hz] Value %15.7E\n",
               int(st), int(pos), int(peak[pos].freq_pos-NUM_FFT_CHANNELS/2),
               Real64_t(peak[pos].freq_pos-NUM_FFT_CHANNELS/2)/NUM_FFT_CHANNELS/Channel_Bandwidth,
               int(peak[pos].time_pos-NUM_FFT_SLOTS/2),
               Real64_t(peak[pos].time_pos-NUM_FFT_SLOTS/2)/NUM_FFT_SLOTS/Integration_Time,
               (peak[pos].value-mean)/stddev);
    }
    // destroy the plan
    fftwf_destroy_plan(plan);
    {
        int retval = munmap(memory_map, MMAP_BYTES);
        if((retval)) {
            perror("error unmemory mapping channel FFT file");
            exit(2);
        }
        memory_map=NULL;
        file_in=0;
    }
    free(data_out_void); data_out_void = NULL; data_out = NULL;
    free(data_in_void); data_in_void = NULL; data_in = NULL;
    return 0;
}







int main(int argc, char* argv[]) {
    if(argc != 11) {
        fprintf(stderr, "Error: correct usage is\n%s Channel_Bandwidth[Hz] Integration_Time[s] NUM_CHANNELS STAGGER_TIMESLOTS STAGGER_START_TIMESLOTS_OFFSET STAGGER_TIMESLOTS_OFFSET NUM_STAGGER filename_in filename_temp_chan_out filename_out\n", argv[0]);
        exit(2);
    }
    const Real32_t Channel_Bandwidth(atof(argv[1]));
    const Real32_t Integration_Time(atof(argv[2]));
    const int_fast32_t NUM_RAW_CHANNELS = atoi(argv[3]);
    const int_fast32_t NUM_CHANNELS_AFTER_FFT = NUM_RAW_CHANNELS*ZERO_PAD_FACTOR;
    const int_fast32_t STAGGER_TIMESLOTS = atoi(argv[4]);
    const int_fast32_t NUM_TIMESLOTS_AFTER_FFT = STAGGER_TIMESLOTS*ZERO_PAD_FACTOR;
    const int_fast32_t STAGGER_START_TIMESLOTS_OFFSET = atoi(argv[5]);
    const int_fast32_t STAGGER_TIMESLOTS_OFFSET = atoi(argv[6]);
    const int_fast32_t NUM_STAGGER = atoi(argv[7]);
    const char* const restrict filename_in = argv[8];
    const char* const restrict filename_temp_chan_out = argv[9];
    const char* const restrict filename_out = argv[10];

    if((NUM_RAW_CHANNELS<2) || (STAGGER_TIMESLOTS<2)
      || (STAGGER_START_TIMESLOTS_OFFSET<0) || (STAGGER_TIMESLOTS_OFFSET<1)
      || (NUM_STAGGER<1)) {
        fprintf(stderr, "Error: bad integer input\n");
        exit(2);
    }

    fftwf_import_wisdom_from_filename(wisdom_file);

    FILE* fp_in = fopen(filename_in, "rb");
    if(fp_in == NULL) {
        fprintf(stderr, "Error: cannot open filename_in '%s'\n", filename_in);
        exit(2);
    }
    FILE* fp_temp_chan_out = fopen(filename_temp_chan_out, "wb");
    if(fp_temp_chan_out == NULL) {
        fprintf(stderr, "Error: cannot open filename_temp_chan_out '%s' for output\n", filename_temp_chan_out);
        exit(2);
    }

    int_fast32_t NUM_TIME_SLOTS_PROCESS = STAGGER_TIMESLOTS;
    for(int_fast32_t st=1; st < NUM_STAGGER; st++) {
        NUM_TIME_SLOTS_PROCESS +=STAGGER_TIMESLOTS_OFFSET;
    }

    fft_in_frequency(fp_in, fp_temp_chan_out, NUM_RAW_CHANNELS,
                     STAGGER_START_TIMESLOTS_OFFSET,
                     NUM_TIME_SLOTS_PROCESS);

    fclose(fp_in); fp_in = NULL;
    fclose(fp_temp_chan_out); fp_temp_chan_out = NULL;

    int fd_temp_chan_out = open(filename_temp_chan_out, O_RDONLY|O_NOATIME);
    if(fd_temp_chan_out == -1) {
        perror("error opening filename_temp_chan_out for read");
        exit(2);
    }
    FILE* fp_out = fopen(filename_out, "wb");
    if(fp_out == NULL) {
        fprintf(stderr, "Error: cannot open filename_out '%s' for output\n", filename_out);
        exit(2);
    }


    fft_in_time(fd_temp_chan_out, fp_out, Channel_Bandwidth, Integration_Time,
                NUM_CHANNELS_AFTER_FFT,
                STAGGER_TIMESLOTS, STAGGER_TIMESLOTS_OFFSET, NUM_STAGGER);

    fclose(fp_out); fp_out=NULL;
    {
        int retval = close(fd_temp_chan_out);
        if((retval)) {
            perror("error closing filename_temp_chan_out file from read");
            exit(2);
        }
        fd_temp_chan_out=0;
    }



    printf("Fringe-fitting has %6d frequency channels (center at %6d)\n",
           int(NUM_CHANNELS_AFTER_FFT), int(NUM_CHANNELS_AFTER_FFT/2));
    printf("Fringe-fitting has %6d time slots (center at %6d)\n",
           int(NUM_TIMESLOTS_AFTER_FFT), int(NUM_TIMESLOTS_AFTER_FFT/2));
    

    fftwf_export_wisdom_to_filename(wisdom_file);
    
    return 0;
}
