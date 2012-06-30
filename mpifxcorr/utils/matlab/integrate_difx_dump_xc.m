% Multiplies and integrates data in the two files accross full time range, reading 
% data in chunks of 128 FFTs x Nch channels.
% Returns: d = cross fn1 x fn2, d1 = auto-corr fn1, d2 = auto-corr fn2
%          d1c = integrated fn1, d2c = integrated fn2
%          N = total number of samples integrated
function [d,d1,d2,d1c,d2c,N]=integrate_difx_dump_xc(fn1, fn2, Nch, Nint)

Nfft = 128;
if (nargin==3),
    Nint = -1;
    Nfft = 128;
else
    Nfft = min(abs(Nint), Nfft);
end

fd1 = fopen(fn1);
fd2 = fopen(fn2);
if 0,
    % or checking for by-chance effects
    % open 2nd as copy of 1st and add read offset
    fd2 = fopen(fn1);
    fprintf(1, 'integrate_difx_dump_xc: Warning: DEBUG code changed data sources\n');
    [dd1, deof] = read_difx_dump(fd1, Nfft, Nch);
    %if (fseek(fd2, 2*Nfft*Nch*2*4, 'bof')<0),
    %    fprintf(1, 'Seek failed\n');
    %end
end

d = zeros(1, Nch);
d1 = zeros(1, Nch);
d2 = zeros(1, Nch);
d1c = zeros(1, Nch);
d2c = zeros(1, Nch);

filtermethod = 0;

Nsamp = 0;
Niter = 0;
while (Nfft>0),

    % get data
    [dd1, deof] = read_difx_dump(fd1, Nfft, Nch);
    if deof, break; end
    [dd2, deof] = read_difx_dump(fd2, Nfft, Nch);
    if deof, break; end
    
    % future read() cropping
    Nsamp = Nsamp + size(dd1, 1);
    if (Nint>0) && (Nsamp>(Nint-Nfft)),
        Nfft = Nint - Nsamp;
    end

    % sub-sum (iteration) counter
    if (mod(Niter,10)==0),
        fprintf(1, 'Read iteration %d\n', Niter);
    end
    Niter = Niter + 1;
    
    % auto and cross correlations
    dd1ac = dd1 .* conj(dd1);
    dd2ac = dd2 .* conj(dd2);
    ddX = dd1 .* conj(dd2);
  
    % alternatively, form cross with a priori mean(dd1),mean(dd2)
    % save 'apriori.mat' self1 self2;
    %load 'apriori.mat';
    %self1 = repmat(self1, [Nfft 1]);
    %self2 = repmat(self2, [Nfft 1]);
    %ddX = (dd1 - self1) .* conj(dd2 - self2);
    % result: above value (E<(x-Ex)(y-Ey)>) is the same as E<xy>-E<x>E<y> as expected
    
    % filtering: reduce the Nfft sample vectors to just one filtered output
    switch (filtermethod)
      case 0     % mean
        subint_dd1 = sum(dd1ac, 1) / size(dd1ac, 1);
        subint_dd2 = sum(dd2ac, 1) / size(dd2ac, 1);
        subint_dd1c = sum(dd1, 1) / size(dd1, 1);
        subint_dd2c = sum(dd2, 1) / size(dd2, 1);
        subint_ddX = sum(ddX, 1) / size(ddX, 1);
      otherwise % median
        subint_dd1 = median(dd1ac, 1);
        subint_dd2 = median(dd2ac, 1);
        subint_dd1c = median(dd1, 1);
        subint_dd2c = median(dd2, 1);
        subint_ddX = median(ddX, 1);
    end
    
    % time integrate filter output vectors
    d1 = d1 + subint_dd1;
    d2 = d2 + subint_dd2;
    d1c = d1c + subint_dd1c;
    d2c = d2c + subint_dd2c;
    d = d + subint_ddX;
    
end
fclose(fd1);
fclose(fd2);

%% scale the data (which is a sum of sub-integrations)
d = d / Niter;
d1 = d1 / Niter;
d2 = d2 / Niter;
d1c = d1c / Niter;
d2c = d2c / Niter;
N = Nsamp;

Ldata_MB = 2*4 * Nsamp * Nch / 2^20;
fprintf(1, 'Integrated %u samples/channel and %u MB of data\n', Nsamp, Ldata_MB);
    