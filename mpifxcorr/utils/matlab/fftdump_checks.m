function fftdump_checks(fn1, fn2, Nch)

Nfft = 1024;

%fbase = 'C:\MatlabData\difx\fftdump_094_';
%flist = {'Ef', 'Jb', 'On', 'Wb' };

%ss = integrate_difx_dump([fbase flist{2} '.bin'], Nch); 
%figure(2), plot(abs(ss));

fnr=1;
fs = 32e6;
chfs = fs/(2*Nch);

if 0,
    % Spectrograms
    [xc,ac1,ac2] = specgram_difx_dump_xc(fn1, fn2, Nch); 
end

if 0,
    % Cross and auto plots
    [xc,ac1,ac2] = integrate_difx_dump_xc(fn1, fn2, Nch); 
    figure(fnr), fnr=fnr+1; clf,
    subplot(3,1,1), plot(abs(xc)),  title(['Cross AxB(RR)']);
    subplot(3,1,2), plot(abs(ac1)), title(['Auto A(RR)']);
    subplot(3,1,3), plot(abs(ac2)), title(['Auto B(RR)']);
end

if 0,
    % Cross-corr plot only (+ phase + lags)
    % With true covariance plot added
    [xc,ac1,ac2,self1,self2] = integrate_difx_dump_xc(fn1, fn2, Nch); 
    lags = circshift(fft(xc), [0 round(Nch/2)]);
    xcPostAvg = self1.*conj(self2);
    xc2 = xc - xcPostAvg;
    %xc2 = xc ./ sqrt(self1.*conj(self2));
    lags2 = circshift(fft(xc2), [0 round(Nch/2)]);
    figure(fnr), fnr=fnr+1; clf,
    subplot(6,1,1), plot(abs(xc)),  title(['Spectral Cross-Correlation E<fft( A )*(fft( B )'')> (RR)']);
    subplot(6,1,2), plot(unwrap(phase(xc))), title('Phase (rad)');
    subplot(6,1,3), plot(abs(lags)), title('Lag');
    subplot(6,1,4), plot(abs(xc2)),  title(['Spectral Cross-Covariance = E<fft( A )*(fft( B )'')> - E<fft( A )> x E<fft( B )> (RR) ']);
    subplot(6,1,5), plot(unwrap(phase(xc2))), title('Phase (rad)');
    subplot(6,1,6), plot(abs(lags2)), title('Lag');
end

if 1,
    % Cross-corr plot without phase, lag
    prtfunc = @plot;
    prtfunc = @semilogy;
    [xc,ac1,ac2,self1,self2,Nint] = integrate_difx_dump_xc(fn1, fn2, Nch); 
    xcPostAvg = self1.*conj(self2);
    xc2 = xc - xcPostAvg;
    ymax = max([max(abs(xc)) max(abs(xc2))]);
    figure(fnr), fnr=fnr+1; clf,
    subplot(3,2,1), prtfunc(abs(xc)),  title(['Spectral Cross-Correlation E<fft( A )*(fft( B )'')> (RR)']);
    axis tight; ax=axis(); ax(4)=ymax;
    axis(ax);
    subplot(3,2,2), prtfunc(abs(xc2)), title(['Spectral Covariance = E<fft( A )*(fft( B )'')> - E<fft( A )> x E<fft( B )> (RR) ']);
    axis(ax);    
    subplot(3,2,3), prtfunc(abs(ac1)), title(['Spectral Variance = E<fft( A )*conj(self)> (RR) ']);
    subplot(3,2,4), prtfunc(abs(ac2)), title(['Spectral Variance = E<fft( B )*conj(self)> (RR) ']);
    subplot(3,2,5), prtfunc(abs(xcPostAvg)), title(['Spectral residual = E<fft( A )> x E<fft( B )> (RR) ']);
    xcAuto1 = (ac1.*ac2);
    xcAuto2 = (self1 .* (conj(self1).*self2) .* conj(self2)) * (Nint^2);
    xcAutoO2 = xcAuto1 ./ xcAuto2;
    subplot(3,2,6), prtfunc(abs(xcAutoO2)), title('2nd order residual = E<|s1|^2>*E<|s2|^2> / (Nint*E<s1>E<s1>''*Nint*E<s2>E<s2>'') (RR) ');
    % Report the mean correction
    mnXC = abs(mean(xc));
    mnXC_corr = abs(mean(xcPostAvg));
    fprintf(1, 'Mean correction E<s1*s2>-E<s1>*E<s2> in terms of E<s1>*E<s2>/E<s1*s2> is: %f%%\n', 100*mnXC_corr/mnXC);
end

if 0,
    % Development of cross-corr and covariance phases
    fd1 = fopen(fn1);
    fd2 = fopen(fn2);
    Nint = 128;
    T_xc = zeros(1, Nch);
    T_xc_cov = zeros(1, Nch);
    for ii=1:16,
        [xc,ac1,ac2,self1,self2] = read_difx_dump_xc(fd1, fd2, Nch, Nint);
        if (size(xc,1)<0), break; end
        xc2 = xc - self1.*conj(self2);
        T_xc(ii,:) = xc;
        T_xc_cov(ii,:) = xc2;
    end
    for ii=1:size(T_xc,1),
        T_xc_p(ii,:) = (phase(T_xc(ii,:)));
        T_xc_cov_p(ii,:) = (phase(T_xc_cov(ii,:)));
    end
    figure(3), 
    subplot(2,1,1), imagesc(T_xc_p),  title(['Phase: Cross AxB(RR)']);
    subplot(2,1,2), imagesc(T_xc_cov_p), title(['Phase: Covariance = Cross AxB(RR) - Self1*conj(Self2)']);
    figure(4), 
    subplot(2,1,1), imagesc(abs(T_xc)),  title(['Mag: Cross AxB(RR)']);
    subplot(2,1,2), imagesc(abs(T_xc_cov)), title(['Mag: Covariance = Cross AxB(RR) - Self1*conj(Self2)']);
end
