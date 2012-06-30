% Integrates all data in the file accross full time range, reading 
% data in chunks of 128 FFTs x Nch channels.
function d=integrate_difx_dump(fn, Nch)

fd = fopen(fn);
d = zeros(1, Nch);

Nfft = 128;
Nsamp = 0;
while 1,
    [dd, deof] = read_difx_dump(fd, Nfft, Nch);
    if deof, break; end

    % auto-correlation
    dd = dd .* conj(dd);

    % filtering
    subint_d = sum(dd, 1);
    %subint_d = median(dd, 1);

    % integration
    Nsamp = Nsamp + size(dd, 1);
    d = d + subint_d;

end
fclose(fd);

Ldata_MB = 2*4 * Nsamp * Nch / 2^20;
fprintf(1, 'Integrated %u samples/channel and %u MB of data\n', Nsamp, Ldata_MB);
    