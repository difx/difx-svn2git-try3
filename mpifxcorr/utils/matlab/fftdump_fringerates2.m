% Loads DiFX fringe-stopped 'Nch'-channel raw FFT'ed station data 
% from the file with filename 'fn'.
% Two station version.
%
% Extracts channels listed in the 'channels' vector.
% Reads 'Nsamp' samples for each channel.
% Applies low-pass filtering and integration to the conj cross product.
% Finally, composes several plots showing filter performance
% and residual fringe frequency spectra.
%
% Examples:
%   fn1='C:\MatlabData\syntheticvlbi\difxdump\fstopped_s0_gpsToneNoNoise_ext.bin';
%   fn2='C:\MatlabData\syntheticvlbi\difxdump\fstopped_s1_gpsToneNoNoise_ext.bin';
%   fftdump_fringerates2(fn1,fn2,128, 65536, 48:50);
%
%   fn1='C:\MatlabData\syntheticvlbi\difxdump\fstopped_s0_gpsCAwithNoise.bin';
%   fn2='C:\MatlabData\syntheticvlbi\difxdump\fstopped_s1_gpsCAwithNoise.bin';
%   fftdump_fringerates2(fn1,fn2,128, 65536*8, 47:49);
%
%   fn1='C:\MatlabData\syntheticvlbi\difxdump\fstopped_ew057_s0.bin';
%   fn2='C:\MatlabData\syntheticvlbi\difxdump\fstopped_ew057_s1.bin';
%   fftdump_fringerates2(fn1,fn2,128, 65536*4, 50);
%
%   fn1='C:\MatlabData\difx\fftdump_094_Ef.bin';
%   fn2='C:\MatlabData\difx\fftdump_094_Jb.bin';
%   fftdump_fringerates2(fn1, fn2, 512, 3000, [1 16 64 250 380]);
function fftdump_fringerates2(fn1, fn2, Nch, Nsamp, channels)

%% Fixed parameters
fs = 32e6;
add_xcorr_DC = 0;       % non-0 to add DC offset to xcorr
add_xcorr_noise = 0;    % non-0 to add (correlated)noise xcorr
upsampling_on = 0;      % double the sample rate before lowpass filter
cumsum_do = 1;          % do cumulative sum after lowpass filtering
replace_by_noise = 0;   % non-0 to replace original data by un- and by correlated noise
add_source_tone_do = 0; % non-0 to further add a fake near-0Hz "astro source" tone to xcorr data
f_source_tone = 22;     % frequency of "astro source" tone to add
filterstate_init = 0;   % non-0 to initialize filter states
Nfft_zoomed = 100; % number of FFT bins to plot in "near 0 Hz" plots
maxPlotLchans = 10;

%% Filter setting
fcut = 10; % absolute [Hz]

%% Derived (1st part)
chfs = fs/(2*Nch);

%% Load data    
fftdata1 = load_difx_dump(fn1, Nsamp, Nch, channels);
fftdata2 = load_difx_dump(fn2, Nsamp, Nch, channels);

%% Replace stations by noise and correlated noise
if replace_by_noise,
    fftdata1 = randn(size(fftdata1)) + i*randn(size(fftdata1));
    fftdata2 = randn(size(fftdata2)) + i*randn(size(fftdata2));
    xcnoise = (randn(size(fftdata1)) + i*randn(size(fftdata1))) * 10;
    fftdata1 = fftdata1 + xcnoise;
    fftdata2 = fftdata2 + xcnoise;
    clear xcnoise;
end

%% Form cross product
crossfft = fftdata1 .* conj(fftdata2);
clear fftdata1 fftdata2

%% Add DC offset and noise to cross product
if add_xcorr_DC,
    DC = 5+i*5;
    cmax = 10 ^ floor(log10(max(max(abs(crossfft)))));
    crossfft = crossfft + cmax*DC;
    fprintf('Additional DC offset added to xcorr data: (%e,i*%e)\n', real(cmax*DC), imag(cmax*DC));
end
if add_xcorr_noise,
    cmax = 10 ^ floor(log10(max(max(abs(crossfft)))));
    % cxnoise = cmax * exp(-i*randn(size(crossfft))); % phase noise on unit circle
    % cxnoise = cmax*(rand(size(crossfft)) + i*rand(size(crossfft)) - (0.5+i*0.5)); % re/im noise in box
    cxnoise = cmax*(randn(size(crossfft)) + i*randn(size(crossfft))); % re/im Gaussian noise
    save 'cxnoise.mat' cxnoise;
    load 'cxnoise.mat';  % for doing comparative plot series, with same noise input
    crossfft = crossfft + cxnoise;
    fprintf('Additional uniform noise added to xcorr data: A=(%e,i*%e)\n', cmax, cmax);
    cxnoise = cxnoise(:,1); % for filter state init
else
    cmax = 10 ^ floor(log10(max(max(abs(crossfft)))));
    cxnoise = cmax*(randn(size(crossfft,1),1) + i*randn(size(crossfft,1),1)); % for filter state init
    save 'cxnoise1ch.mat' cxnoise;
    load 'cxnoise1ch.mat';
end

%% Add tone at the source fringe rate
if add_source_tone_do,
    cmax = 0.5 * (10 ^ floor(log10(max(max(abs(crossfft))))));
    xctone = (1:size(crossfft,1));
    xctone = cmax*sin(2*pi*(f_source_tone/chfs) .* xctone);
    if 1, % add to one channel only, others are noise
        ch = min([2  size(crossfft,2)]);
        crossfft(:,ch) = crossfft(:,ch) + xctone';
    else % add same residual fringe freq tone to all channels
        for ch=1:size(crossfft, 2),
            crossfft(:,ch) = crossfft(:,ch) + xctone';
        end
    end
    fprintf('Additional tone with amplitude %e and freq %e added to xcorr data\n', cmax, f_source_tone);
end

%% Copies
%crossfft = repmat(crossfft, 2, 1); %% DEBUG
crossfft_unfiltered = crossfft;
crossfft_integrated = crossfft;

%% Derived (2nd part)
fnr = 1;
Lchans = size(crossfft, 2);
Nsamp = size(crossfft, 1);
Tsamp = Nsamp/chfs;
chfft_freqs = linspace(0,chfs,Nsamp);
chfft_times = linspace(0,Tsamp,Nsamp);
if cumsum_do,
    label_integrated = 'Mean-filtered';
    label_filtered = 'Low-pass then mean filtered';
else
    label_integrated = 'Mean-filtered';
    label_filtered = 'Low-pass filtered';
end
fprintf(1, 'Data time: %e\n', Tsamp);


%% Filter one channel at a time
hd = fftdump_makefilter(fcut, chfs);
fprintf(1, 'Filter: isstable()=%d\n', isstable(hd));
fprintf(1, 'Filter: fcut = %e Hz at fs = %e Hz\n', fcut, chfs);
for ii=1:Lchans,
    
    %% Upsample prior to low-pass filtering?
    if upsampling_on,
        din = dyadup(crossfft(:,ii),0);
    else
        din = crossfft(:,ii);
    end
    
    %% Reduce to Real-valued only
    %din = real(din); % => the DC part of filtered data spectra still rise
    %above raw spectrum with many windowing funcs (except with Kaiser a=2..3,
    %rectangular, others(?))
    
    %% Time-Integrated
    Ldin = size(din,1);
    cumsum_norm = Ldin; % fixed length window
    %cumsum_norm = linspace(1, Ldin, Ldin)'; % growing window
    integrated = cumsum(din) ./ cumsum_norm;
    
    %% Low-pass filtered
    if filterstate_init,
        hd.PersistentMemory = true;
        hd.reset();
        %filter(hd, cxnoise);
        filter(hd, din); 
        % other method filter(hd,din(end:-1:1)) with inverse corrupts spec
    end
    lowpassed = filter(hd, din);

    %% Plot full filtered data spectrum before integrator/cumsum
    if 0 && (Lchans < maxPlotLchans),
        figure(fnr), clf; fnr=fnr+1;
        xtimes = linspace(0,(size(tmp_lp,1)/chfs),size(tmp_lp,1));
        subplot(Lchans,1,ii), 
        [ax,h1,h2]=plotyy(xtimes, real(lowpassed), xtimes, real(din));
        set(get(ax(1),'Ylabel'),'String','Filtered');
        set(get(ax(2),'Ylabel'),'String','Original');
        title(['Full input sequence, channel ' num2str(channels(ii))]);
        L = 20*log10(max(real(lowpassed)) / max(real(din)));
        fprintf(1, 'Attenuation channel %d is L=20log10(M/m)=%.2f dB\n', ii, -L);
    end
    
    %% Low-pass filtered data: apply cumulative sum?
    if cumsum_do,
        lowpassed = cumsum(lowpassed) ./ cumsum_norm;
    end
    
    %% Finally, downsample again if upsampling was enabled
    if upsampling_on,
        lowpassed = [dyaddown(lowpassed,0); 0]; % [x0 x1 x2 x3 ...]
        integrated = [dyaddown(integrated,0); 0];
    end
    crossfft(:,ii) = lowpassed;
    crossfft_integrated(:,ii) = integrated;
    
    clear lowpassed integrated;
end
   
%% Real/Image Separate Handling -- Plot at most 6 channels in subplot
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
        chdata = squeeze(crossfft_integrated(:, ich));
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
        chdata = squeeze(crossfft(:, ich));
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
        chint = squeeze(crossfft_integrated(:, ich));
        chflt = squeeze(crossfft(:, ich));            
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
                plot(chfft_times, abs(chflt), 'r-'), axis tight, grid on;
                ylabel('|xcorr|');
                legend(label_integrated, label_filtered);
        end
        xlabel('Time [s]');
        title(['Channel ' num2str(channels(ich)) ' Cross-Power']);
    end
end

%% Real-Part Handling -- Plot spectrum of filtered time series, overlaid
if (Lchans < maxPlotLchans),
    figure(fnr), clf; fnr=fnr+1;
    for ich = 1:Lchans,
        NP = min(Nfft_zoomed, floor(max(size(chfft))/2));
        datafunc = @real; % abs(), angle(), real()
        plotfunc = @semilogy;
        %plotfunc = @loglog;
        wf = bartlett(Nsamp); % kaiser(Nsamp, 3.5), rectwin, hann, hanning, bartlett
        chdataU  = fft(wf .* datafunc(squeeze(crossfft_unfiltered(:, ich))));
        chdataLP = fft(wf .* datafunc(squeeze(crossfft(:, ich))));
        chdataI  = fft(wf .* datafunc(squeeze(crossfft_integrated(:, ich))));
        subplot(Lchans, 1, ich);
        plotfunc(chfft_freqs(1:NP), abs(chdataU(1:NP)), 'b-'), grid on, hold on;
        plotfunc(chfft_freqs(1:NP), abs(chdataI(1:NP)), 'g-'), grid on, hold on;
        plotfunc(chfft_freqs(1:NP), abs(chdataLP(1:NP)), 'r-.'), grid on, hold off;
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
        chdata = crossfft(:, ich);
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
    chfft = fft(crossfft,[],1);  % dim#1, since crossfft=Nsamp x Nch
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
xcmax = max(abs(crossfft),[],1);
%fprintf(1, 'Maximum values in all channels:\n');
%xcmax

%% Side by side full "spectrograms"
if 0 && (Lchans>1),
    figure(fnr), clf; fnr=fnr+1;
    subplot(2,1,1), surf(real(crossfft_unfiltered)), shading('interp'), title('|unfiltered(t)|');
    subplot(2,1,2), surf(real(crossfft)), shading('interp'), title('|lowpass(t)|');
end

%% Final sum of data
if 1 && (Lchans>1),
        % Side by side: Filtered last output and Non-Filtered sum
        xcsum = mean(crossfft_unfiltered,1); % filter by summing and scaling (==mean())
        xcflt = crossfft(end,:);
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
