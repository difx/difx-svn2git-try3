C   cmxsr.i
C
C***The maximum number of sources can be changed in the parameter
C***statement here and is the only change necessary.
C
C Written: B. Archinal, 95.11.13.
C    98.09.08 - D. Gordon: Added variables P_motion, D_psec, Pmotion, and
C               Dpaec for proper motion and parallax corrections, mainly 
C               for correlator usage; added additional documentation of 
C               variables.
C    98.11.25 - D. Gordon: Added variable PRcorr to hold proper motion 
C               RA and Dec corrections for each source. 
C    2012.06.10-James M Anderson:  Added 3-D source vectors for near-field
C               and multi-reference frame purposes
C
C   Maximum number of radio sources per database.
C   (should equal or be greater than MAX_ARC_SRC in
C   /mk3/src/solve/include/gsfcb.i and MAXSTR in
C   /mk3/src/dbedit/sitstr.i).
C
      Integer*4 MAX_ARC_SRC
      Parameter(MAX_ARC_SRC=300)
C
      Real*8     CD, CRA, RADEC(2,MAX_ARC_SRC), SD, SRA, 
     *           P_motion(4,MAX_ARC_SRC), D_psec(MAX_ARC_SRC),
     *           PRcorr(2,MAX_ARC_SRC), SRC_DIR_VEC(3,MAX_ARC_SRC)
      Integer*4  Pmotion, Dpsec, SRC_REF_FRAME(MAX_ARC_SRC)
      Integer*2  NUMSTR, LNSTAR(4,MAX_ARC_SRC), i1dum
      COMMON / STRCM / CD, CRA, RADEC, SD, SRA, P_motion, D_psec,
     *                 PRcorr, SRC_DIR_VEC,
     *                 Pmotion, Dpsec, SRC_REF_FRAME,
     *                 LNSTAR, NUMSTR, i1dum
C
C   Variables:
C       1. CD                    - Cosine of Declination of source in the 
C                                  current observation.
C       2. CRA                   - Cosine of Right Ascension of source in the 
C                                  current observation.
C       3. RADEC(2,MAX_ARC_SRC)  - THE RIGHT ASCENSIONS AND DECLINATIONS OF THE
C                                  STARS IN THE STAR CATALOG. (RAD, RAD)
C       4. SD                    - Sine of Declination of source in the 
C                                  current observation.
C       5. SRA                   - Sine of Right Ascension of source in the 
C                                  current observation.
C       6. P_motion(4,Max_arc_src)-Proper motion array. 1) Proper motion in RA,
C                                  2) Proper motion in Dec, and 3) Epoch for
C                                  which RADEC( ) above is correct, for each
C                                  source. (arc-sec/year, arc-sec/year, 
C                                  year (1995.0, etc.))
C                                  If P_motion(4,?) > 10000 then
C                                  P_motion(1--3,?) are the vector quantities
C                                  of the proper motion, in units of m/s,
C                                  and P_motion(4,?) is the MJD of the zero epoch
C       7. D_psec(Max_arc_src)   - The estimated distances to each source.
C                                  (Parsecs)
C       8. PRcorr(2,Max_arc_src) - Proper motion corrections in RA and 
C                                  Declination. (radians)
C       9. SRC_DIR_VEC(3,MAX_ARC_SRC)-Source directions as 3-D vectors.  
C                                  The units are meters
C                                  If all 3 components of the vector are
C                                  0.D0, then use the RADEC array.  If
C                                  the magnitude of the vector is ~ 1.0,
C                                  then treat this as a unit vector.  Otherwise,
C                                  the vector is the actual position of
C                                  the source with respect to the center
C                                  of the reference frame.
C      10. Pmotion               - Proper motion indicator. If greater than 
C                                  zero, compute proper motion contributions.
C      11. Dpsec                 - Parallax indicator. If greater than zero, 
C                                  compute parallax contributions.
C      12. SRC_REF_FRAME(MAX_ARC_SRC)- Refrence frame of the source positions.
C                                  0 Solar barycenter position and velocity, 
C                                    J2000
C                                  1 Earth center position and velocity, J2000
C      13. NUMSTR                - THE NUMBER OF STARS IN THE STAR CATALOG.
C      14. LNSTAR(4,MAX_ARC_SRC) - THE EIGHT ALPHANUMERIC CHARACTER NAMES OF
C                                  THE STARS IN THE STAR CATALOG.
C
