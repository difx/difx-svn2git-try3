% Loads DiFX fringe-stopped 'Nch'-channel raw FFT'ed station data 
% from the file with filename 'fn'.
%
% Two station version.
% Extracts channels listed in the 'channels' vector.
% Reads 'Nsamp' samples for each channel.
%
% Filtering each station data separate before forming their conjugate
% product, and does not filter the product, unlike fftdump_fringerates2().
%
% Finally, composes several plots showing filter performance
% and residual fringe frequency spectra.
%
% Examples:
%   fn1='C:\MatlabData\syntheticvlbi\difxdump\fstopped_s0_gpsToneNoNoise_ext.bin';
%   fn2='C:\MatlabData\syntheticvlbi\difxdump\fstopped_s1_gpsToneNoNoise_ext.bin';
%   fftdump_fringerates2_prexc(fn1,fn2,128, 65536, 48:50);
%
%   fn1='C:\MatlabData\syntheticvlbi\difxdump\fstopped_s0_gpsCAwithNoise.bin';
%   fn2='C:\MatlabData\syntheticvlbi\difxdump\fstopped_s1_gpsCAwithNoise.bin';
%   fftdump_fringerates2_prexc(fn1,fn2,128, 65536*8, 47:49);
%
%   fn1='C:\MatlabData\difx\fftdump_094_Ef.bin';
%   fn2='C:\MatlabData\difx\fftdump_094_Jb.bin';
%   fftdump_fringerates2_prexc(fn1, fn2, 512, 3000, [1 16 64 250 380]);
function fftdump_fringerates2_prexc(fn1, fn2, Nch, Nsamp, channels)

%% Fixed parameters
fs = 32e6;
add_xcorr_noise = 0;    % non-0 to add (correlated)noise xcorr
add_source_tone_do = 0; % non-0 to add a fake near-0Hz "astro source" tone
f_source_tone = 4;      % frequency of "astro source" tone to add
filterstate_init = 0;   % non-0 to initialize filter states with reverse signal
Nfft_zoomed = 100; % number of FFT bins to plot in "near 0 Hz" plots
maxPlotLchans = 10;

%% Filter setting
fcut = 16; % absolute [Hz]
%fcut = 16/Tsamp; % relative to data [Hz]

%% Derived (1st part)
chfs = fs/(2*Nch);

%% Load data    
fftdata1 = load_difx_dump(fn1, Nsamp, Nch, channels);
fftdata2 = load_difx_dump(fn2, Nsamp, Nch, channels);

%% Replace stations by noise and correlated noise
if 0,
    fftdata1 = randn(size(fftdata1)) + i*randn(size(fftdata1));
    fftdata2 = randn(size(fftdata2)) + i*randn(size(fftdata2));
    xcnoise = randn(size(fftdata1)) + i*randn(size(fftdata1));
    fftdata1 = fftdata1 + xcnoise;
    fftdata2 = fftdata2 + xcnoise;
    clear xcnoise;
end

%% Derived (2nd part)
fnr = 1;
Lchans = size(fftdata1, 2);
Nsamp = size(fftdata1, 1);
Tsamp = Nsamp/chfs;
chfft_freqs = linspace(0,chfs,Nsamp);
chfft_times = linspace(0,Tsamp,Nsamp);
label_integrated = 'Mean-filtered';
label_filtered = 'Low-pass filtered, integrated';
fprintf(1, 'Data time: %e\n', Tsamp);

%% Filter both stations, one channel at a time
hd = fftdump_makefilter(fcut, chfs);
if filterstate_init,
    hd.PersistentMemory = true;
end
fprintf(1, 'Filter: isstable()=%d\n', isstable(hd));
fprintf(1, 'Filter: fcut = %e Hz at fs = %e Hz\n', fcut, chfs);
fftdata1_lp = fftdata1;
fftdata2_lp = fftdata2;
for ii=1:Lchans,
    if filterstate_init,
        hd.reset();
        filter(hd, fftdata1_lp(:,ii));
        fftdata1_lp(:,ii) = filter(hd, fftdata1_lp(:,ii));
        hd.reset();
        filter(hd, fftdata2_lp(:,ii));
        fftdata2_lp(:,ii) = filter(hd, fftdata2_lp(:,ii));
    else
        fftdata1_lp(:,ii) = filter(hd, fftdata1_lp(:,ii));
        fftdata2_lp(:,ii) = filter(hd, fftdata2_lp(:,ii));
    end
    if 0 && (Lchans < maxPlotLchans),
        figure(fnr), clf; fnr=fnr+1;
        subplot(2,1,1),
            semilogy(chfft_freqs, abs(fft(fftdata1(:,ii))), 'b-'), hold on,
            semilogy(chfft_freqs, abs(fft(fftdata1_lp(:,ii))), 'r-.'),
            title(['Spectrum of Station 1 data channel ' num2str(channels(ii))]),
            legend('Raw', 'Low-pass filtered');
        subplot(2,1,2),
            semilogy(chfft_freqs, abs(fft(fftdata2(:,ii))), 'b-'), hold on,
            semilogy(chfft_freqs, abs(fft(fftdata2_lp(:,ii))), 'r-.'),
            title(['Spectrum of Station 2 data channel ' num2str(channels(ii))]),
            legend('Raw', 'Low-pass filtered');
    end
end

%% Form cross products; time integrate both later
crossfft_int = fftdata1 .* conj(fftdata2);
crossfft_lp = fftdata1_lp .* conj(fftdata2_lp);
crossfft_unfiltered = crossfft_int;
clear fftdata1 fftdata2 fftdata1_lp fftdata2_lp
fprintf(1, 'Filtering done and cross-correlations formed\n');

%% Add DC offset and noise to cross product
if add_xcorr_noise,
    cmax = 10 ^ floor(log10(max(max(abs(crossfft_int))))); % noise power to match closest decade of peak power
    cxnoise = cmax*(randn(size(crossfft_int)) + i*randn(size(crossfft_int))); % re/im Gaussian noise
    %save 'cxnoise_prexc1.mat' cxnoise;
    %load 'cxnoise_prexc1.mat';  % for doing comparative plot series, with same noise input
    crossfft_int = crossfft_int + cxnoise;
    crossfft_lp = crossfft_lp + cxnoise;
    crossfft_unfiltered = crossfft_unfiltered + cxnoise;    
    fprintf('Additional uniform noise added to xcorr data: A=(%e,i*%e)\n', cmax, cmax);
end

%% Add tone at the source fringe rate
if add_source_tone_do,
    cmax = 0.5 * (10 ^ floor(log10(max(max(abs(crossfft_int))))));
    xctone = cmax * cos(2*pi*f_source_tone .* chfft_times);
    for ch=1:size(crossfft_int, 2),
        crossfft_int(:,ch) = crossfft_int(:,ch) + xctone';
        crossfft_lp(:,ch) = crossfft_lp(:,ch) + xctone';
        crossfft_unfiltered(:,ch) = crossfft_unfiltered(:,ch) + xctone';
    end
    fprintf('Additional tone with amplitude %e and freq %e added to xcorr data\n', cmax, f_source_tone);
end

%% Time integrate raw and low-passed station data, one channel at a time
for ii=1:Lchans,    
    Ldin = size(crossfft_int(:,ii),1);
    crossfft_int(:,ii) = cumsum(crossfft_int(:,ii)) ./ linspace(1, Ldin, Ldin)';
    crossfft_lp(:,ii)  = cumsum(crossfft_lp(:,ii)) ./ linspace(1, Ldin, Ldin)'; 
end
   
%% Real-part Handling -- Plot at most 6 channels in subplot
if (Lchans < maxPlotLchans),
    wf = hanning(size(crossfft_unfiltered,1));
    %wf = ones(size(crossfft_unfiltered,1), 1);
    % Non-Filtered
    figure(fnr), clf; fnr=fnr+1;
    for ich = 1:Lchans,
        chdata = squeeze(crossfft_unfiltered(:, ich));
        subplot(Lchans, 2, 2*ich-1);
            plot(chfft_times, real(chdata)), axis tight, grid on;
            title(['Raw Channel ' num2str(channels(ich)) ' Cross-Power']);
            xlabel('Time [s]');
            ylabel('Real(XC))');
        chfft = fft(wf .* real(chdata)); % r2c fft
        LP = max(size(chfft)); %min(Nfft_zoomed, max(size(chfft)));
        subplot(Lchans, 2, 2*ich-0);
            semilogy(chfft_freqs(1:LP), abs(chfft(1:LP))), grid on;
            title(['Channel ' num2str(channels(ich))]);
            xlabel('Freq [Hz]');
            ylabel('|FFT(Real(XC)))|');
    end
    % Integrated
    figure(fnr), clf; fnr=fnr+1;
    for ich = 1:Lchans,
        chdata = squeeze(crossfft_int(:, ich));
        subplot(Lchans, 2, 2*ich-1);
            plot(chfft_times, real(chdata)), axis tight, grid on;
            title([label_integrated ' Channel ' num2str(channels(ich)) ' Cross-Power']);
            xlabel('Time [s]');
            ylabel('Real(XC))');
        chfft = fft(wf .* real(chdata)); % r2c fft
        LP = min(Nfft_zoomed, max(size(chfft)));
        subplot(Lchans, 2, 2*ich-0);
            semilogy(chfft_freqs(1:LP), abs(chfft(1:LP))), grid on;
            title(['Channel ' num2str(channels(ich))]);
            xlabel('Freq [Hz]');
            ylabel('|FFT(Real(XC)))|');
    end
    % Filtered
    figure(fnr), clf; fnr=fnr+1;
    for ich = 1:Lchans,
        chdata = squeeze(crossfft_lp(:, ich));
        subplot(Lchans, 2, 2*ich-1);
            plot(chfft_times, real(chdata)), axis tight, grid on;
            title([label_filtered ' Channel ' num2str(channels(ich)) ' Cross-Power']);
            xlabel('Time [s]');
            ylabel('Real(XC))');
        chfft = fft(wf .* real(chdata)); % r2c fft
        LP = min(Nfft_zoomed, max(size(chfft)));
        subplot(Lchans, 2, 2*ich-0);
            semilogy(chfft_freqs(1:LP), abs(chfft(1:LP))), grid on;
            title(['Channel ' num2str(channels(ich))]);
            xlabel('Freq [Hz]');
            ylabel('|FFT(Real(XC)))|');
    end
    % Both filtered and integrated, time domain plot
    figure(fnr), clf; fnr=fnr+1;
    for ich = 1:Lchans,
        chint = squeeze(crossfft_int(:, ich));
        chflt = squeeze(crossfft_lp(:, ich));            
        subplot(Lchans, 1, ich);
            switch 'abs',
                case 'reim',
                    base = 0;  % base=min(real(chint);
                    plot(chfft_times, real(chint)-base, 'g-'), axis tight, grid on, hold on,
                    plot(chfft_times, imag(chint)-base, 'g-.'), axis tight, grid on, hold on,
                    plot(chfft_times, real(chflt)-base, 'r-'), axis tight, grid on;
                    plot(chfft_times, imag(chflt)-base, 'r-.'), axis tight, grid on;
                    ylabel('Real(xcorr), Imag(xcorr)');
                    legend([label_integrated ' Real'], [label_integrated ' Imag'], [label_filtered ' Real'], [label_filtered ' Imag']);
                case 'abs',
                    plot(chfft_times, abs(chint), 'g-'), axis tight, grid on, hold on,
                    plot(chfft_times, abs(chflt), 'r-.'), axis tight, grid on;
                    ylabel('|xcorr|');
                    legend(label_integrated, label_filtered);
            end
            title(['Channel ' num2str(channels(ich)) ' Cross-Power']);
            xlabel('Time [s]');
    end
    
end

%% Real Part Handling -- Plot spectrum of filtered time series, overlaid
if (Lchans < maxPlotLchans),
    figure(fnr), clf; fnr=fnr+1;
    for ich = 1:Lchans,
        NP = min(Nfft_zoomed, max(size(chfft)));
        wf = hanning(size(crossfft_unfiltered,1));
        %wf = ones(size(crossfft_unfiltered,1),1);
        chdataU = fft(wf .* real(squeeze(crossfft_unfiltered(:, ich))));
        chdataLP = fft(wf .* real(squeeze(crossfft_lp(:, ich))));
        chdataI = fft(wf .* real(squeeze(crossfft_int(:, ich))));
        subplot(Lchans, 1, ich);
        semilogy(chfft_freqs(1:NP), abs(chdataU(1:NP)), 'b-'), grid on, hold on;
        semilogy(chfft_freqs(1:NP), abs(chdataI(1:NP)), 'g-'), grid on, hold on;
        semilogy(chfft_freqs(1:NP), abs(chdataLP(1:NP)), 'r-.'), grid on, hold off;
        %axis tight;
        legend('Raw data', label_integrated, label_filtered);
        xlabel('Frequency [Hz]'), ylabel('Magnitude');
        title(['Spectrum of cross-power time series for channel ' num2str(channels(ich))]);
    end
end

%% Full Complex Handling -- Plot at most 6 channels in subplot
if 0 && (Lchans < maxPlotLchans),
    % Non-Filtered
    figure(fnr), clf; fnr=fnr+1;
    for ich = 1:Lchans,
        chdata = crossfft_unfiltered(:, ich);
        chfft = fft(chdata); % since chdata is complex, lets keep all bins, not just 1...N/2+1
        subplot(Lchans, 1, ich);
        semilogy(chfft_freqs, abs(chfft));
        title(['Raw: Station 1 x 2 : Channel ' num2str(channels(ich))]);
        xlabel('Residual fringe frequency [Hz]');
        ylabel('Cross Amplitude');
    end
    % Filtered
    figure(fnr), clf; fnr=fnr+1;
    for ich = 1:Lchans,
        chdata = crossfft_lp(:, ich);
        chfft = fft(chdata); % since chdata is complex, lets keep all bins, not just 1...N/2+1
        subplot(Lchans, 1, ich);
        semilogy(chfft_freqs, abs(chfft));
        title([label_filtered ': Station 1 x 2 : Channel ' num2str(channels(ich))]);
        xlabel('Residual fringe frequency [Hz]');
        ylabel('Cross Amplitude');
    end
end

%% Full Complex Handling -- Plot all channels together, if too many for individual plots
if 0 && ~(Lchans < maxPlotLchans),

    % Non-Filtered
    figure(fnr), clf; fnr=fnr+1;
    chfft = fft(crossfft_unfiltered,[],1);  % dim#1, since crossfft=Nsamp x Nch
    chfft = abs(chfft);
    %chfft = 10*log10(chfft);            % log-plot
    %chfft = chfft - max(max(chfft));    % normalized
    [x,y]=meshgrid(channels,chfft_freqs);
    surf(x,y,chfft), shading('interp');
    xlabel('Channel number'), ylabel('Fringe frequency [Hz]'), zlabel('Cross power');
    title(['Raw: Station 1 x 2 : multiple channels : residual fringe freq and noise']);
    zmax = max(max(chfft));
    axis([ 0 max(channels) 0 max(chfft_freqs) 0 zmax]);

    % Filtered
    figure(fnr), clf; fnr=fnr+1;
    chfft = fft(crossfft_lp,[],1);  % dim#1, since crossfft=Nsamp x Nch
    chfft = abs(chfft);
    %chfft = 10*log10(chfft);            % log-plot
    %chfft = chfft - max(max(chfft));    % normalized
    [x,y]=meshgrid(channels,chfft_freqs);
    surf(x,y,chfft), shading('interp');
    xlabel('Channel number'), ylabel('Fringe frequency [Hz]'), zlabel('Cross power');
    title([label_filtered ': Station 1 x 2 : multiple channels : residual fringe freq and noise']);
    axis([ 0 max(channels) 0 max(chfft_freqs) 0 zmax]);
end

%% Data statistics
xcmax = max(abs(crossfft_lp),[],1);
%fprintf(1, 'Maximum values in all channels:\n');
%xcmax

%% Side by side full "spectrograms"
if 0 && (Lchans>1),
    figure(fnr), clf; fnr=fnr+1;
    subplot(2,1,1), surf(real(crossfft_unfiltered)), shading('interp'), title('|unfiltered(t)|');
    subplot(2,1,2), surf(real(crossfft_lp)), shading('interp'), title('|lowpass(t)|');
end

%% Final sum of data
if 1 && (Lchans>1),
    % Side by side: Filtered last output and Non-Filtered mean
    xcsum = crossfft_int(end,:);
    xcflt = crossfft_lp(end,:);
    figure(fnr), clf; fnr=fnr+1;
    subplot(2,2,1), 
        plot(channels, abs(xcsum), 'r-'), hold on,
        plot(channels, abs(xcflt), 'g-.'),            
        xlabel('Channel'), ylabel('Magnitude(Y)'),
        grid on,
        legend(label_integrated, label_filtered),
        title('Magnitude of Cross-Power at end of Averaging Period');
    subplot(2,2,2), 
        plot(channels, (180/pi)*phase(xcsum), 'r-'), hold on,
        plot(channels, (180/pi)*phase(xcflt), 'g-.'), 
        xlabel('Channel'), ylabel('Phase(Y) [deg]'),
        legend(label_integrated, label_filtered),
        title('Phase');
        grid on;
    subplot(2,2,3), 
        plot(channels, real(xcsum), 'r-'), hold on,
        plot(channels, real(xcflt), 'g-.'),
        xlabel('Channel'), ylabel('Real(Y)'),
        legend(label_integrated, label_filtered),
        title('Real part'),
        grid on;
    subplot(2,2,4), 
        plot(channels, imag(xcsum), 'r-'), hold on,
        plot(channels, imag(xcflt), 'g-.'), 
        xlabel('Channel'), ylabel('Imag(Y)'),
        legend(label_integrated, label_filtered),
        title('Imag part'),
        grid on;
end
