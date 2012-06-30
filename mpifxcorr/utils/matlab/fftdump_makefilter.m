% Generate some filter
% fcut : cut-off frequency in Hz
% chfs : sampling rate of one signal channel
function [hd]=fftdump_makefilter(fcut, chfs)
wcut = fcut/(chfs/2);

rip_pp = 3;
att_stop = 70;
[b,a] = ellip(4, rip_pp, att_stop, wcut, 'low');
% quantize:
bQ = ( b ./ (2^ceil(log2(max(abs([a b]))))));
aQ = ( a ./ (2^ceil(log2(max(abs([a b]))))));
bQ = single(bQ);
aQ = single(aQ);
%b=double(bQ); a=double(aQ);
%figure(fnr), clf; fnr=fnr+1;
%freqz(b,a);
%crossfft = filter(b,a, crossfft, [], 1); % dim#1, since crossfft=Nsamp x Nch
        
% Elliptical
%d = fdesign.lowpass('n,fp,ap,ast',6,20,wcut,60,80);
%hd = design(d,'ellip');  % ends up with wrong cut-off, much too high!?

% Cheby I, 1dB passband ripple
d = fdesign.lowpass('N,Fp,Ap', 12, fcut, 1, chfs);
hd = design(d, 'cheby1');
%set(hd,'arithmetic','single');

% Butterworth
%wcut = fcut/(chfs/2); % unclear: design('butter'), w3db scaling, 1.0==fs or 1.0==fs/2
%d = fdesign.lowpass('n,f3db', 12, wcut);
%hd = design(d,'butter');
