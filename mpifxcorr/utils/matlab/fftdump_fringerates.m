% Loads DiFX fringe-stopped 'Nch'-channel raw FFT'ed station data 
% from the file with filename 'fn'.
% One station version.
%
% Extracts channels listed in the 'channels' vector.
% Reads 'Nsamp' samples for each channel.
%
% Plots FFT in time direction for these channels for
% a fringe frequency plot. The reasoning is that 
% after fringe stopping, the astronomical point source
% is pretty much in the phase center. 
%
% This means every FFT bin value f(i) is a sum 
%  f(i) = abs(astro_point)*phase(0) + abs(astro_extended)*phase(linear,small_variation)
%         + abs(noise)*phase(random) + abs(RFI)*phase(random/linear,large_variation)
%
% Thus Fourier{f(i=i0,t=T0...T1)} will give a spectrum with
% amplitudes versus residual-vs-stopped fringe frequencies.
% Note: the abs() amplitudes in f(i) are assumed to remain constant!
%
%  fftdump_fringerates('C:\MatlabData\difx\fftdump_094_Ef.bin', 512, 3000, [1 16 64 250 380]);
%  fftdump_fringerates('C:\MatlabData\difx\fftdump_094_Jb.bin', 512, 3000, [1 16 64 250 380]);
%
function fftdump_fringerates(fn, Nch, Nsamp, channels)

%% Fixed parameters
% Nsamp = 2048;
fs = 32e6;

%% Derived parameters
chfs = fs/Nch;
Lchan = max(size(channels));
fnr = 1;    

%% Load data    
fftdata = load_difx_dump(fn, Nsamp, Nch, channels);
Nsamp = size(fftdata, 1);
chfft_freqs = (((1:Nsamp)-1)/Nsamp) * chfs;

%% Plot at most 6 channels in subplot
if (Lchan < 6),
    figure(fnr), clf; fnr=fnr+1;
    for ich = 1:Lchan,
        chdata = fftdata(:, ich);
        chfft = fft(chdata); % since chdata is complex, lets keep all bins, not just 1...N/2+1
        subplot(Lchan, 1, ich);
        semilogy(chfft_freqs, abs(chfft));
        title(['Channel ' num2str(channels(ich))]);
        xlabel('Residual fringe frequency [Hz]');
        ylabel('Amplitude');
    end
end

%% Plot all channels together, if too many for individual plots
if ~(Lchan < 6),
    fftdata2 = fft(fftdata,[],1);  % dim#1, since fftdata=Nsamp x Nch

    %figure(fnr), clf; fnr=fnr+1;
    %surfc(abs(fftdata2)), shading('interp'), title('FFT(t=0..N|ch)');

    figure(fnr), clf; fnr=fnr+1;
    surf(real(fftdata)), title('Real(Raw(t=0..N|ch))');
end

