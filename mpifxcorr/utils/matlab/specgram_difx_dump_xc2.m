% Multiplies data in the two files accross full time range, reading 
% data in chunks of Lch*k FFTs x {N_sub_ch} channels. Shows unintegrated
% result values over time range.
function [d,d1,d2]=specgram_difx_dump_xc2(fn1, fn2, Nch, Lch, st1_str, st2_str, IfftCh)

fd1 = fopen(fn1);
fd2 = fopen(fn2);
d = zeros(1, Nch);
d1 = zeros(1, Nch);
d2 = zeros(1, Nch);

Nsampint = 2; % number of time points to average for one plot point
Nfft = Lch * Nsampint;
Nsamp = 0;

if nargin < 5,
    st1_str = '1';
end
if nargin < 6,
    st2_str = '2';
end
if nargin < 7,
    IfftCh = floor(Nch/2)+1;
end
fprintf(1, 'Averaging %d time samples for one plot point\n', Nsampint);

while 1,
    [dd1, deof] = read_difx_dump(fd1, Nfft, Nch);
    if deof, break; end
    [dd2, deof] = read_difx_dump(fd2, Nfft, Nch);
    if deof, break; end

    % auto and cross correlations
    dd1ac = dd1 .* conj(dd1);
    dd2ac = dd2 .* conj(dd2);
    ddX = dd1 .* conj(dd2);
    
    % sub-integrate
    if Nsampint>1,
        for ii=1:(Nfft/Nsampint),
            istart = Nsampint*(ii-1) + 1;
            iend = istart + (Nsampint-1);
            dd1ac(ii,:) = sum(dd1ac(istart:iend,:));
            dd2ac(ii,:) = sum(dd2ac(istart:iend,:));
            ddX(ii,:) = sum(ddX(istart:iend,:));
        end
        dd1ac = dd1ac(1:(Nfft/Nsampint),:);
        dd2ac = dd2ac(1:(Nfft/Nsampint),:);
        ddX = ddX(1:(Nfft/Nsampint),:);
        %ddX = ddX ./ (sqrt(dd1ac) .* sqrt(dd2ac));
    end
    
    figure(1),
    subplot(4,1,1), imagesc(abs(dd1ac)), title(['Autocorr ' st1_str ' (average of ' num2str(Nsampint) ' time samples)']), xlabel('channel'), ylabel('time');
    subplot(4,1,2), imagesc(abs(dd2ac)), title(['Autocorr ' st2_str]);
    subplot(4,1,3), imagesc(abs(ddX)), title('Xcorr');
    subplot(4,1,4), plot(abs(fft(ddX(:,IfftCh)))), title(['Channel ' num2str(IfftCh) ' Xcorr FFT']);
    ins = input('press key to continue','s');
    if (ins == 'q'), break; end
end
fclose(fd1);
fclose(fd2);

Ldata_MB = 2*4 * Nsamp * Nch / 2^20;
fprintf(1, 'Integrated %u samples/channel and %u MB of data\n', Nsamp, Ldata_MB);
    