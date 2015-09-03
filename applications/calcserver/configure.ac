AC_INIT([CalcServer], [2.0], [Walter Brisken <wbrisken@nrao.edu>])

AM_INIT_AUTOMAKE

AC_PROG_CC
AM_SANITY_CHECK

AC_LANG_C

AC_PROG_F77
AM_PROG_LIBTOOL

AC_CHECK_LIB(m, sin,,)

AC_OUTPUT(Makefile \
	setupCALCservers \
	startCalcServer \
	src/Makefile \
	calc9.1/Makefile \
	calc9.1.RA/Makefile \
	init.d/calcserver \
	data/Makefile
)
