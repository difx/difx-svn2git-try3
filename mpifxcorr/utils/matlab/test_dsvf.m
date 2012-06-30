% test_dsvf(fc,fs,Q,L) 
%    Uses Digital Stage Variable filter to filter a fixed DC 
%    signal (1.5+i2.0) using that filter on 'L' identical samples.
function [flt,uflt] = test_dsvf(fc,fs,Q,L)

Nt = L; %32e3; % number of samples per channel
Nchannels = 4; % number of channels
noiseF = 1;

goal = (1.5 + i*2.0);
% goal = 1.5;
unfiltered = ones(Nt, 1, 'single') .* goal;
%unfiltered = single(unfiltered + noiseF*(randn(size(unfiltered))));
unfiltered = single(unfiltered + noiseF*(randn(size(unfiltered)) + i*randn(size(unfiltered))));
filtered = zeros(Nchannels, Nt, 'single');

%% derive DSVF coefficients
f = single(2*sin(pi*fc/fs));
q = single(1/Q);
fprintf(1, 'DSVF coeff f=%f q=%d\n', f, q);
gain = single(1.0);

%% digital state variable filter
% http://www.fpga.synth.net/pmwiki/pmwiki.php?n=FPGASynth.SVF
fb1 = zeros(1, Nchannels, 'single');
fb2 = zeros(1, Nchannels, 'single');
output = zeros(1, Nchannels, 'single');

Nt_eff = Nt;
%Nt_eff = 50;
for ii=1:Nt_eff,

  % move new data in
  input = single(gain * unfiltered(ii));

  sum1 = input + (-fb1 * q) + (-output);
  sum2 = f*sum1 + fb1;
  sum3 = f*fb1 + fb2;
  output = sum3;
  fb1 = sum2;
  fb2 = sum3;
  % whos fb1 fb2 %should say 16 bytes and 'single'

  filtered(:, ii) = output;
end

flt = filtered(1,:);
uflt = unfiltered;

if 1,
  axv=[0 Nt -2 3];
  uflt_mag = abs(fft(real(uflt)));
  flt_mag = abs(fft(real(flt)));
  freqvals = linspace(0, fs, max(size(flt_mag)));
  figure(1), clf,
    subplot(3,2,1), plot(real(uflt)), axis(axv), title('Real() unfiltered'),
    subplot(3,2,2), semilogy(freqvals,uflt_mag),
    subplot(3,2,3), plot(real(flt)), axis(axv), title('Real() usr filtered'),
    subplot(3,2,4), semilogy(freqvals,flt_mag),
    subplot(3,2,5), plot(real(flt)./real(goal)), axis(axv), title('Real() usr filtered, normalized by unfiltered goal'),
    subplot(3,2,6), semilogy(freqvals,flt_mag./max(uflt_mag')),
  if 0,
      figure(2), clf,
        subplot(2,2,1), plot(imag(uflt)), axis(axv), title('Imag() unfiltered'),
        subplot(2,2,2), semilogy(freqvals,abs(fft(imag(uflt)))),
        subplot(2,2,3), plot(imag(flt)), axis(axv), title('Imag() usr filtered'),
        subplot(2,2,4), semilogy(freqvals,abs(fft(imag(flt)))),
  end
%  figure(3),
%    plot(real(rflt)-real(flt)), title('Ref vs own');
end
goal,
flt(end)
