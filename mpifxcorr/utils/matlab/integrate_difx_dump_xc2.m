% Loads DiFX fringe-stopped 'Nch'-channel raw FFT'ed station data 
% from the file with filename 'fn'. Processes all channels via
% two methods: 1) integration, 2) filtering followed by integration.
%
function [xcint,xcflt]=integrate_difx_dump_xc2(fn1, fn2, Nch, Nsamp)

%% Fixed parameters
fs = 32e6;
chstep = 32;
fcut = 10;   % filter cut-off, absolute [Hz]
Ttrim = 0.0; % amount of seconds to discard from early filtered output
do_cumsum = 1; % non-0 to make cumulative sum after filtering
do_filterstate = 1; % non-0 to initialize filter state by pre-pass on data

%% Derived
chfs = fs/(2*Nch);
hd = fftdump_makefilter(fcut, chfs);
if ~isstable(hd),
    fprintf(1, 'Filter is not stable! fcut=%e, fs=%e\n', fcut, chfs);
    xcint=0; xcflt=0;
    return;
end
Ntrim = 1 + floor(Ttrim*chfs);

%% Vars
chstart = 1;
fnr = 1;
xcint = zeros(1, Nch);
xcflt = zeros(1, Nch);

%% To reduce memory load, process channels as subsets
while chstart<Nch,
    
    %% Load data
    chstop = min(Nch, chstart+chstep-1);
    channels = (chstart:chstop);
    fprintf(1, 'Loading channels %d to %d\n', chstart, chstop);
    fftdata1 = load_difx_dump(fn1, Nsamp, Nch, channels);
    fftdata2 = load_difx_dump(fn2, Nsamp, Nch, channels);

    %% Cross product
    crossfft = fftdata1 .* conj(fftdata2);
    clear fftdata1 fftdata2

    %% Generate noise?
    if chstart<chstep,
        if 1,
            % on first run only, later load from file always
            cmax = 10 ^ floor(log10(max(max(abs(crossfft))))); % noise power to match closest decade of peak power
            L = max(size(crossfft));
            cxnoise = cmax*(randn(L,1) + i*randn(L,1));
            save 'integrate_difx_dump_xc2_noise.mat' cxnoise;
            fprintf(1, 'Noise generated and written to file\n');
        else
            load 'integrate_difx_dump_xc2_noise.mat';
        end
    end    
    
    %% Derived
    Lchans = size(crossfft, 2);
    Nsactual = size(crossfft, 1);
    Tsamp = Nsactual/chfs;
    %wf = hanning(Nsactual);

    %% Filtering (could filter as 2D array along one dimension as well)
    fprintf(1, 'Filtering channels %d to %d\n', chstart, chstop);
    for ch=1:Lchans,
        fprintf(1, '%d ', chstart+ch-1);
        din = crossfft(:,ch);
        
        %% Add noise
        %din = din + cxnoise;
        
        %% Integrating filter
        xcint(chstart+ch-1) = mean(din);
        
        %% Low-pass filter
        if do_filterstate,
            hd.PersistentMemory = true;
            hd.reset();
            filter(hd, cxnoise(1:max(size(din)))); %din
        end
        flt = filter(hd, din); % filter
        flt = flt(Ntrim:end);  % crop
        if do_cumsum,
            xcflt(chstart+ch-1) = mean(flt); % integrate
        else
            xcflt(chstart+ch-1) = flt(end);
        end
    end
    fprintf(1, '\n');
    
    chstart = chstart + chstep;
end

%% Plot for comparison
if 1,
    figure(fnr), clf; fnr=fnr+1;
    chs = 5;
    che = Nch;
    channels = (chs:che);
    subplot(2,1,1),
        plot(channels, real(xcint(channels)), 'g-'), hold on,
        plot(channels, real(xcflt(channels)), 'r-.');
        xlabel('Channel'), ylabel('Real(XC)'), axis tight, grid on,
        title('Real part of integrated cross-correlation'),
        if do_cumsum,
            legend('Time-integrated', 'Low-pass filtered, integrated');
        else
            legend('Time-integrated', 'Low-pass filtered');
        end
    subplot(2,1,2),
        plot(channels, imag(xcint(channels)), 'g-'), hold on,
        plot(channels, imag(xcflt(channels)), 'r-.');
        xlabel('Channel'), ylabel('Imag(XC)'), axis tight, grid on,
        title('Imaginary part of integrated cross-correlation'),
        if do_cumsum,
            legend('Time-integrated', 'Low-pass filtered, integrated');
        else
            legend('Time-integrated', 'Low-pass filtered');
        end
    figure(fnr), clf; fnr=fnr+1;
    subplot(3,1,1),
        plot(channels, abs(xcint(channels)), 'g-'), hold on,
        plot(channels, abs(xcflt(channels)), 'r-.');
        xlabel('Channel'), ylabel('|XC|'), axis tight, grid on,
        title('Magnitude of integrated cross-correlation'),
        if do_cumsum,
            legend('Time-integrated', 'Low-pass filtered, integrated');
        else
            legend('Time-integrated', 'Low-pass filtered');
        end
    subplot(3,1,2),
        plot(channels, abs(xcint(channels) - xcflt(channels)), 'b'),
        xlabel('Channel'), ylabel('|IntXC-FltXC|'), axis tight, grid on,
        title('Delta of time integrated minus low-pass filtered versions');
    relative = abs(xcint(channels) - xcflt(channels)) ./ abs(xcint(channels));
    subplot(3,1,3),
        plot(channels, relative, 'b'),
        xlabel('Channel'), ylabel('|InXC-FltXC|/|IntXC|'), axis tight, grid on,
        title('Relative delta of time integrated and low-pass filtered versions');
end    
