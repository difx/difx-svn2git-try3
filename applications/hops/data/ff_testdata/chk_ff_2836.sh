#!/bin/sh
#
# $Id: chk_ff_2836.sh 870 2013-10-07 18:21:50Z rjc $
#
# canonical test suite for fourfit
#

verb=false
[ -n "$testverb" ] && verb=true

[ -d "$srcdir" ] || { echo srcdir not set; exit 1; }
${HOPS_SETUP-'false'} || . $srcdir/chk_env.sh
export DATADIR=`cd $srcdir/testdata; pwd`

[ -n "$DISPLAY" ] || { echo Skipping test--DISPLAY is undefined; exit 0; }

os=`uname -s` || os=idunno
grep -v $os $DATADIR/2836/cf2836 > ./cf2836

$verb && echo \
fourfit -pt -b AE:X \\ && echo \
    -c ./cf2836 \\ && echo \
    $DATADIR/2836/scan001/2145+067.olomfh

( echo sff-2836.ps; echo q ) | (
    fourfit -pt -b AE:X \
	-c ./cf2836 \
	$DATADIR/2836/scan001/2145+067.olomfh
) 2>/dev/null 1>&2

# pluck out line containing the snr and parse it
line=$(grep '7570 9653' ./ff-2836.ps)

IFS='()'
read a snr b <<<"$line"

# snr bounds
low=139.1
high=140.1
aok=$(echo "$snr>$low && $snr<$high" | bc)

[ $aok -gt 0 ]

#
# eof
#
