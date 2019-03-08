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
#include <time.h>
#include <string.h>
#include <error.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>



#include "JMA_math.h"




static const int_fast32_t ZERO_PAD_FACTOR = 2;
static const int_fast32_t NUM_PEAKS = 100;
static const int_fast32_t DELTA_DELAY = 2;
static const int_fast32_t DELTA_RATE  = 2;


struct peak_pos_struct {
    int_fast32_t freq_pos;
    int_fast32_t time_pos;
    Real32_t     value;
};



void get_RMS_peaks_components_slots(const int_fast32_t FREQ_POS,
                                    const int_fast32_t NUM_SAMPLES,
                                    const int_fast32_t SKIP,
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
    for(int_fast32_t i=SKIP; i < NUM_SAMPLES-SKIP; i++) {
        Real64_t d = data[i];
        sum += d;
        sumsqr += d*d;
    }
    *num_samples += NUM_SAMPLES;
    *sum_tot += sum;
    *sumsqr_tot += sumsqr;
    for(int_fast32_t i=SKIP; i < NUM_SAMPLES-SKIP; i++) {
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






int_fast32_t join_data(FILE* const restrict fp_out,
                       const int_fast32_t NUM_FILES,
                       const Real32_t* const restrict * const restrict datapp,
                       const Real32_t Channel_Bandwidth,
                       const Real32_t Integration_Time,
                       const int_fast32_t NUM_FFT_CHANNELS,
                       const int_fast32_t NUM_FFT_SLOTS_AT_ONCE,
                       const int_fast32_t NUM_STAGGER) throw()
{
    // allocate memory.
    // Extend-pad by a factor of ZERO_PAD_FACTOR for more resolution
    Real32_t* restrict data_out;
    Real32_t* restrict data_out2;
    void* data_out_void;
    void* data_out_void2;
    const size_t NUM_PIXELS_BLOCK = size_t(NUM_FFT_SLOTS_AT_ONCE)*NUM_FFT_CHANNELS;
    const size_t NUM_BYTES_SLOT  = size_t(NUM_FFT_SLOTS_AT_ONCE)*sizeof(Real32_t);
    if(posix_memalign(&data_out_void,16, NUM_BYTES_SLOT) != 0)
    {
        fprintf(stderr, "Error: could not allocate %llu bytes for fringe slot\n", (unsigned long long)(NUM_BYTES_SLOT));
        exit(3);
    }
    data_out = reinterpret_cast<Real32_t* restrict>(data_out_void);
    if(posix_memalign(&data_out_void2,16, NUM_BYTES_SLOT) != 0)
    {
        fprintf(stderr, "Error: could not allocate %llu bytes for fringe slot2\n", (unsigned long long)(NUM_BYTES_SLOT));
        exit(3);
    }
    data_out2 = reinterpret_cast<Real32_t* restrict>(data_out_void2);

    // Now run through the staggers
    size_t start = 0;
    for(int_fast32_t st = 0; st < NUM_STAGGER; st++, start += NUM_PIXELS_BLOCK) {
        struct peak_pos_struct peak[NUM_PEAKS];
        for(int_fast32_t p=0; p < NUM_PEAKS; p++) {
            peak[p].freq_pos = -1;
            peak[p].time_pos = -1;
            peak[p].value = -1E20f;
        }
        int_fast64_t num_pixels = 0;
        Real64_t sum=0.0;
        Real64_t sumsqr = 0.0;
        memset(reinterpret_cast<void*>(data_out2), 0, NUM_BYTES_SLOT);
        for(int_fast32_t ch=0; ch < DELTA_DELAY; ch++) {
            if(fwrite(reinterpret_cast<void*>(data_out2), NUM_BYTES_SLOT, 1, fp_out) != 1)
            {
                fprintf(stderr, "Error: could not write channel %llu to final output\n", (unsigned long long)(ch));
                exit(2);
            }
        }
        for(int_fast32_t ch=DELTA_DELAY; ch < NUM_FFT_CHANNELS-DELTA_DELAY; ch++) {
            memset(reinterpret_cast<void*>(data_out), 0, NUM_BYTES_SLOT);
            memset(reinterpret_cast<void*>(data_out2), 0, NUM_BYTES_SLOT);
            size_t start_slot = start + (ch-DELTA_DELAY)*NUM_FFT_SLOTS_AT_ONCE;
            for(int_fast32_t dd = -DELTA_DELAY; dd <= DELTA_DELAY; dd++, start_slot += NUM_FFT_SLOTS_AT_ONCE) {
                // add the slots from the various input files together
                for(int_fast32_t f=0; f < NUM_FILES; f++) {
                    for(int_fast32_t s=0; s < NUM_FFT_SLOTS_AT_ONCE; s++) {
                        data_out[s] += datapp[f][start_slot+s];
                    }
                }
            }
            // smooth in slot direction
            for(int_fast32_t s=DELTA_RATE; s < NUM_FFT_SLOTS_AT_ONCE-DELTA_RATE; s++) {
                Real32_t tot = 0.0f;
                for(int_fast32_t dr = -DELTA_RATE; dr <= DELTA_RATE; dr++) {
                    tot += data_out[s+dr];
                }
                data_out2[s] = tot;
//                 if(s==(59+NUM_FFT_SLOTS_AT_ONCE/2)) {
//                     printf("ch %6d s %6d val %12.5E\n", int(ch-NUM_FFT_CHANNELS/2), int(s-NUM_FFT_SLOTS_AT_ONCE/2), tot);
//                 }
//                 if(ch==(79+NUM_FFT_CHANNELS/2)) {
//                     printf("pch %6d s %6d val %12.5E\n", int(ch-NUM_FFT_CHANNELS/2), int(s-NUM_FFT_SLOTS_AT_ONCE/2), tot);
//                 }
            }

            // statistics
            get_RMS_peaks_components_slots(ch, NUM_FFT_SLOTS_AT_ONCE, DELTA_RATE,
                                           data_out2,
                                           NUM_PEAKS, peak,
                                          &num_pixels, &sum, &sumsqr);
            // write out the data
            if(fwrite(reinterpret_cast<void*>(data_out2), NUM_BYTES_SLOT, 1, fp_out) != 1)
            {
                fprintf(stderr, "Error: could not write channel %llu to final output\n", (unsigned long long)(ch));
                exit(2);
            }
        }
        memset(reinterpret_cast<void*>(data_out2), 0, NUM_BYTES_SLOT);
        for(int_fast32_t ch=0; ch < DELTA_DELAY; ch++) {
            if(fwrite(reinterpret_cast<void*>(data_out2), NUM_BYTES_SLOT, 1, fp_out) != 1)
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
        for(int_fast32_t p=0; p < NUM_PEAKS; p++) {
            if((peak_not_neighbor(p, peak))) {
                printf("Stagger %3d Peak %2d Delay_Pos %6d %11.3E [s] Rate_Pos %6d %11.3E [Hz] Value %15.7E\n",
                       int(st), int(p), int(peak[p].freq_pos-NUM_FFT_CHANNELS/2),
                       Real64_t(peak[p].freq_pos-NUM_FFT_CHANNELS/2)/NUM_FFT_CHANNELS/Channel_Bandwidth,
                       int(peak[p].time_pos-NUM_FFT_SLOTS_AT_ONCE/2),
                       Real64_t(peak[p].time_pos-NUM_FFT_SLOTS_AT_ONCE/2)/NUM_FFT_SLOTS_AT_ONCE/Integration_Time,
                       (peak[p].value-mean)/stddev);
                int_fast64_t dist_f(peak[p].freq_pos-NUM_FFT_CHANNELS/2);
                int_fast64_t dist_t(peak[p].time_pos-NUM_FFT_SLOTS_AT_ONCE/2);
                int_fast64_t dist = dist_f*dist_f+dist_t*dist_t;
                if(dist < min_dist) {
                    min_dist = dist;
                    pos = p;
                }
            }
        }
        printf("Peak closest to 0,0\n");
        printf("Stagger %3d Peak %2d Delay_Pos %6d %11.3E [s] Rate_Pos %6d %11.3E [Hz] Value %15.7E\n",
               int(st), int(pos), int(peak[pos].freq_pos-NUM_FFT_CHANNELS/2),
               Real64_t(peak[pos].freq_pos-NUM_FFT_CHANNELS/2)/NUM_FFT_CHANNELS/Channel_Bandwidth,
               int(peak[pos].time_pos-NUM_FFT_SLOTS_AT_ONCE/2),
               Real64_t(peak[pos].time_pos-NUM_FFT_SLOTS_AT_ONCE/2)/NUM_FFT_SLOTS_AT_ONCE/Integration_Time,
               (peak[pos].value-mean)/stddev);
    }
    free(data_out_void); data_out_void = NULL; data_out = NULL;
    free(data_out_void2); data_out_void2 = NULL; data_out2 = NULL;
    return 0;
}







int main(int argc, char* argv[]) {
    if(argc < 10) {
        fprintf(stderr, "Error: correct usage is\n%s Channel_Bandwidth[Hz] Integration_Time[s] NUM_CHANNELS STAGGER_TIMESLOTS STAGGER_START_TIMESLOTS_OFFSET STAGGER_TIMESLOTS_OFFSET NUM_STAGGER filename_out filename_in0 [filename_in1 ...]\n", argv[0]);
        exit(2);
    }
    const Real32_t Channel_Bandwidth(atof(argv[1]));
    const Real32_t Integration_Time(atof(argv[2]));
    const int_fast32_t NUM_RAW_CHANNELS = atoi(argv[3]);
    const int_fast32_t NUM_CHANNELS_AFTER_FFT = NUM_RAW_CHANNELS*ZERO_PAD_FACTOR;
    const int_fast32_t STAGGER_TIMESLOTS = atoi(argv[4]);
    const int_fast32_t NUM_TIMESLOTS_AFTER_FFT = STAGGER_TIMESLOTS*ZERO_PAD_FACTOR;
    const int_fast32_t NUM_STAGGER = atoi(argv[7]);
    const char* const restrict filename_out = argv[8];
    FILE* fp_out = fopen(filename_out, "wb");
    if(fp_out == NULL) {
        fprintf(stderr, "Error: cannot open filename_out '%s' for output\n", filename_out);
        exit(2);
    }



    // mmap size information
    const size_t NUM_BYTES_IN = size_t(NUM_TIMESLOTS_AFTER_FFT)*NUM_CHANNELS_AFTER_FFT*NUM_STAGGER*sizeof(Real32_t);

    const int_fast32_t NUM_FILES = argc - 9;
    int* fdp = reinterpret_cast<int*>(malloc(sizeof(int)*NUM_FILES));
    void** datapp= reinterpret_cast<void**>(malloc(sizeof(void*)*NUM_FILES));
    if((fdp==NULL) || (datapp==NULL)) {
        fprintf(stderr, "Error: could not malloc array holders\n");
        exit(3);
    }
    for(int_fast32_t f=0; f < NUM_FILES; f++) {
        fdp[f] = open(argv[9+f], O_RDONLY|O_NOATIME);
        if(fdp[f] == -1) {
            perror("error opening file for read");
            fprintf(stderr, "tried to open file '%s'\n", argv[9+f]);
            exit(2);
        }
        datapp[f] = mmap(NULL, NUM_BYTES_IN, PROT_READ,
                         MAP_SHARED|MAP_NORESERVE,
                         fdp[f], 0);
        if(datapp[f] == MAP_FAILED) {
            perror("error memory mapping channel FFT file");
            exit(2);
        }
    }
                                            
    join_data(fp_out, NUM_FILES,
              reinterpret_cast<const Real32_t* const restrict * const restrict>(datapp),
              Channel_Bandwidth, Integration_Time,
              NUM_CHANNELS_AFTER_FFT,
              NUM_TIMESLOTS_AFTER_FFT, NUM_STAGGER);

    for(int_fast32_t f=0; f < NUM_FILES; f++) {
        int retval = munmap(datapp[f], NUM_BYTES_IN);
        if((retval)) {
            perror("error unmemory mapping FFT file");
            exit(2);
        }
        datapp[f]=NULL;
        retval = close(fdp[f]);
        if((retval)) {
            perror("error closing input file from read");
            exit(2);
        }
        fdp[f]=0;
    }
    fclose(fp_out); fp_out=NULL;

    free(fdp); fdp=NULL;
    free(datapp); datapp=NULL;

    printf("Fringe-fitting has %6d frequency channels (center at %6d)\n",
           int(NUM_CHANNELS_AFTER_FFT), int(NUM_CHANNELS_AFTER_FFT/2));
    printf("Fringe-fitting has %6d time slots (center at %6d)\n",
           int(NUM_TIMESLOTS_AFTER_FFT), int(NUM_TIMESLOTS_AFTER_FFT/2));
    
    
    return 0;
}
