bin_PROGRAMS = \
	CalcServer \
	checkCalcServer \
	CALC_9_1_RA_Server \
	checkCALC_9_1_RA_Server \
	DiFX_Delay_Server \
	checkDiFX_Delay_Server
CalcServer_SOURCES = \
	CALC5Server.c \
	CALC_xdr.c \
	cvrtuc.c \
	ingdec.c \
	ingra.c \
	iword.c \
	mjd2str.c \
	sindex.c \
	time2str.c
CALC_9_1_RA_Server_SOURCES = \
	CALC_9_1_RA_Server.c \
	CALC_9_1_RA_xdr.c \
	cvrtuc.c \
	ingdec.c \
	ingra.c \
	iword.c \
	mjd2str.c \
	sindex.c \
	time2str.c
DiFX_Delay_Server_SOURCES = \
	CALC5Server_Params.c \
	CALC_9_1_RA_Server_Params.c \
	CALC_9_1_RA_xdr.c \
	CALC_xdr.c \
	DiFX_Delay_Server.c \
	DiFX_Delay_Server_Params.c \
	DiFX_Delay_Server_xdr.c 
CalcServer_LDADD = \
	$(top_builddir)/calc9.1/libCalc.la
CALC_9_1_RA_Server_LDADD = \
	$(top_builddir)/calc9.1.RA/libCalc_9_1_RA.la
checkCalcServer_SOURCES = \
	CALC_xdr.c \
	checkcalc.c
checkCALC_9_1_RA_Server_SOURCES = \
	CALC_9_1_RA_xdr.c \
	checkCALC_9_1_RA_Server.c
checkDiFX_Delay_Server_SOURCES = \
	DiFX_Delay_Server_xdr.c \
	checkDiFX_Delay_Server.c
EXTRA_DIST = \
	CALCServer.h \
	CALC_9_1_RA_Server.h \
	DiFX_Delay_Server.h \
	MATHCNST.H \
	ocean.h \
	STDDEFS.H \
	vlb.h

