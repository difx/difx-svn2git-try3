/* dumpRadioAstronBaseline.c */
/* dump out a single baseline from a FITS file */
/* 2013 Apr 18  JMA  ----  MPIfR  copy from dump.c */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fitsio.h>
#include <math.h>

int main(int argc, char *argv[])
{
    fitsfile *fptr;         /* FITS file pointer, defined in fitsio.h */
    char card[FLEN_CARD];   /* Standard string lengths defined in fitsio.h */
    int status = 0;   /* CFITSIO status value MUST be initialized to zero! */
    int single = 0, hdupos, nkeys, ii;
    //int retval;
    int anynull;
    int is_uv_data = 0;
    int is_anntenna_table;
    int antenna_1 = 1;
    int antenna_2 = 2;
    int NUM_IF=0;
    int NUM_CHAN=0;
    float CHANNEL_BANDWIDTH = 0.0f;
    int NUM_STOKES=0;
    const int NUM_COMPLEX=2;

    if(argc == 4) {
        antenna_1 = atoi(argv[2]);
        antenna_2 = atoi(argv[3]);
    }

    else if (argc != 2) {
      printf("Usage:  listhead filename[ext] \n");
      printf("\n");
      printf("List the FITS header keywords in a single extension, or, if \n");
      printf("ext is not given, list the keywords in all the extensions. \n");
      printf("\n");
      printf("Examples: \n");
      printf("   listhead file.fits      - list every header in the file \n");
      printf("   listhead file.fits[0]   - list primary array header \n");
      printf("   listhead file.fits[2]   - list header of 2nd extension \n");
      printf("   listhead file.fits+2    - same as above \n");
      printf("   listhead file.fits[GTI] - list header of GTI extension\n");
      printf("\n");
      printf("Note that it may be necessary to enclose the input file\n");
      printf("name in single quote characters on the Unix command line.\n");
      return(0);
    }

    if (!fits_open_file(&fptr, argv[1], READONLY, &status))
    {
      fits_get_hdu_num(fptr, &hdupos);  /* Get the current HDU position */

      /* List only a single header if a specific extension was given */ 
      if (hdupos != 1 || strchr(argv[1], '[')) single = 1;

      for (; !status; hdupos++)  /* Main loop through each extension */
      {
          is_uv_data=0;
          is_anntenna_table = 0;
        fits_get_hdrspace(fptr, &nkeys, NULL, &status); /* get # of keywords */

        printf("Header listing for HDU #%d:\n", hdupos);

        for (ii = 1; ii <= nkeys; ii++) { /* Read and print each keywords */

           if (fits_read_record(fptr, ii, card, &status))break;
           printf("%s\n", card);
           if(strncmp(card, "EXTNAME = 'UV_DATA '", strlen("EXTNAME = 'UV_DATA '"))==0) {
               is_uv_data=1;
           }
           else if(strncmp(card, "EXTNAME = 'ANTENNA '", strlen("EXTNAME = 'ANTENNA '"))==0) {
               is_anntenna_table=1;
           }
           if((is_uv_data)) {
               if(strncmp(card, "NO_CHAN =", strlen("NO_CHAN ="))==0) {
                   NUM_CHAN = atoi(card+strlen("NO_CHAN ="));
               }
               else if(strncmp(card, "NO_BAND =", strlen("NO_BAND ="))==0) {
                   NUM_IF = atoi(card+strlen("NO_BAND ="));
               }
               else if(strncmp(card, "NO_STKD =", strlen("NO_STKD ="))==0) {
                   NUM_STOKES = atoi(card+strlen("NO_STKD ="));
               }
               else if(strncmp(card, "CHAN_BW =", strlen("CHAN_BW ="))==0) {
                   CHANNEL_BANDWIDTH = atof(card+strlen("CHAN_BW ="));
               }
           }
        }
        printf("END\n\n");  /* terminate listing with END */

        if (single) break;  /* quit if only listing a single header */



        if((is_anntenna_table)) {
            fprintf(stdout, "Checking ANTENNA table\n");
            LONGLONG row;
            LONGLONG firstelement=1;
            long nrows;

            int ant1;
            char antname[32];
            char* antp[2];
            antp[0] = antname;
            antp[1] = NULL;
            
            fits_get_num_rows(fptr, &nrows, &status);
            for(row=1; row <= nrows; row++) {
                fits_read_col(fptr, TSTRING, 3, row, firstelement, firstelement, NULL, antp, &anynull, &status);
                fits_read_col(fptr, TINT, 4, row, firstelement, firstelement, NULL, &ant1, &anynull, &status);
                if((status)) {
                    char err_msg[128];
                    fits_get_errstatus(status, err_msg);
                    fprintf(stderr, "cfitsio error %d '%s'\n", status, err_msg);
                    exit(1);
                }
                fprintf(stdout, "Antenna %2d is '%8s'\n", ant1, antname);
            }
            fprintf(stdout, "\n\n");
        }
        
        if((is_uv_data)) {
            // visibility data???
            LONGLONG row;
            LONGLONG firstelement=1;
            LONGLONG nelements;
            long nrows;
            int IF=0;
            int ch=0;
            //int chd = NUM_CHAN/2;
            int st=0;
            //int std=0;
            int baseline;
            int ant1, ant2;
            double date, date_last=-1E20;
            double iat, iat_last=-1E20;
            float inttime;
            int count=0;
            const int NUM_TOTAL=NUM_IF*NUM_CHAN*NUM_STOKES*NUM_COMPLEX;
            const int NUM_STOKES_COMPLEX = NUM_STOKES*NUM_COMPLEX;
            const int NUM_CHAN_STOKES_COMPLEX = NUM_CHAN*NUM_STOKES*NUM_COMPLEX;
            const int NUM_CHAN_COMPLEX = NUM_CHAN*NUM_COMPLEX;
            float* data;
            float* tmp_data;
            FILE** fp_out;
            nelements=NUM_TOTAL;
            data = (float*)malloc(sizeof(float)*NUM_TOTAL);
            tmp_data = (float*)malloc(sizeof(float)*NUM_CHAN_COMPLEX);
            fp_out = (FILE**)malloc(sizeof(FILE*)*NUM_IF*NUM_STOKES);
            if((data == NULL) || (tmp_data == NULL) || (fp_out == NULL)) {
                fprintf(stderr, "Error: could not malloc memory\n");
                exit(1);
            }
            for(IF=0; IF < NUM_IF; IF++) {
                for(st=0; st < NUM_STOKES; st++) {
                    char filename[128];
                    sprintf(filename, "vis_if%d_st%d.dat", IF, st);
                    fp_out[IF*NUM_STOKES+st] = fopen(filename, "wb");
                    if(fp_out[IF*NUM_STOKES+st] == NULL) {
                        fprintf(stderr, "Error: cannot open '%s'\n", filename);
                        exit(1);
                    }
                }
            }
            fits_get_num_rows(fptr, &nrows, &status);
            for(row=1; row <= nrows; row++) {
                fits_read_col(fptr, TDOUBLE, 4, row, firstelement, firstelement, NULL, &date, &anynull, &status);
                fits_read_col(fptr, TDOUBLE, 5, row, firstelement, firstelement, NULL, &iat, &anynull, &status);
                fits_read_col(fptr, TINT32BIT, 6, row, firstelement, firstelement, NULL, &baseline, &anynull, &status);
                fits_read_col(fptr, TFLOAT, 10, row, firstelement, firstelement, NULL, &inttime, &anynull, &status);
                fits_read_col(fptr, TFLOAT, 13, row, firstelement, nelements, NULL, data, &anynull, &status);
                if((status)) {
                    char err_msg[128];
                    fits_get_errstatus(status, err_msg);
                    fprintf(stderr, "cfitsio error %d '%s'\n", status, err_msg);
                    exit(1);
                }
                ant1 = baseline>>8;
                ant2 = baseline&0xFF;
                if((ant1==antenna_1) && (ant2==antenna_2)) {
                    /* if((is_uv_data)) { */
                    /*     fprintf(stdout, "row=%lld retval=%d status=%d ants %3d %3d date %15E %15E inttime %12E data[0][%d][%d]=%15E %15E  amp=%15E phas=%15E\n", (long long)row, retval, status, */
                    /*             ant1,ant2, */
                    /*             date, iat, inttime, */
                    /*             chd, std, */
                    /*             data[0][chd][std][0], data[0][chd][std][1], */
                    /*             sqrt(data[0][chd][std][0]*data[0][chd][std][0]+data[0][ch][st][1]*data[0][ch][st][1]), */
                    /*             atan2(data[0][chd][std][1],data[0][chd][std][0])); */
                    /*     is_uv_data=1; */
                    /* } */
                    if(count==0) {
                        printf("Integration time was %E seconds\n", inttime);
                    }
                    if(date < date_last) {
                        fprintf(stderr, "Bad date order\n");
                        exit(1);
                    }
                    date_last = date;
                    if(iat < iat_last) {
                        fprintf(stderr, "Bad iat order\n");
                        exit(1);
                    }
                    iat_last = iat;
                    for(IF=0; IF < NUM_IF; IF++) {
                        for(st=0; st < NUM_STOKES; st++) {
                            for(ch=0; ch < NUM_CHAN; ch++) {
                                tmp_data[ch*NUM_COMPLEX+0] = data[IF*NUM_CHAN_STOKES_COMPLEX+ch*NUM_STOKES_COMPLEX+st*NUM_COMPLEX+0];
                                tmp_data[ch*NUM_COMPLEX+1] = data[IF*NUM_CHAN_STOKES_COMPLEX+ch*NUM_STOKES_COMPLEX+st*NUM_COMPLEX+1];
                            }
                            fwrite(tmp_data, sizeof(float)*NUM_CHAN_COMPLEX, 1, fp_out[IF*NUM_STOKES+st]);
                        }
                    }
                    count++;
                }
            }
            fprintf(stdout, "Wrote %d %d %d %d %E %E\n", NUM_IF, count, NUM_CHAN, NUM_STOKES, inttime, CHANNEL_BANDWIDTH);
            for(IF=0; IF < NUM_IF; IF++) {
                for(st=0; st < NUM_STOKES; st++) {
                    fclose(fp_out[IF*NUM_STOKES+st]);
                }
            }
            free(data);
            free(tmp_data);
            free(fp_out);
        }
            

        fits_movrel_hdu(fptr, 1, NULL, &status);  /* try to move to next HDU */
      }

      if (status == END_OF_FILE)  status = 0; /* Reset after normal error */

      fits_close_file(fptr, &status);
    }

    if (status) fits_report_error(stderr, status); /* print any error message */
    return(status);
}

