% Load data from DiFX dump file (FFT output of fringe stopped station data)
%   d = load_difx_dump(fn, Nfft, Nch, [Lch])
%
% Nfft  : number of FFTs to read
% Nch   : number of channels in the data
% Lch   : optional, list
% d : output, Nfft x Nch matrix
function d=load_difx_dump(fn, Nfft, Nch, Lch)

fd = fopen(fn);
if (nargin==3),
    d = read_difx_dump(fd, Nfft, Nch);
else
    Nsub = max(size(Lch));
    d = zeros(Nfft, Nsub);
    
    ddone = 0;
    while (ddone<Nfft),
        [dtmp,deof] = read_difx_dump(fd, 128, Nch);
        if (deof>0), break; end
        
        Npts = size(dtmp,1);
        if (Npts<1), break; end

        dtmp = dtmp(:,Lch);
        d((ddone+1):(ddone+Npts),:) = dtmp;
        ddone = ddone + Npts;
    end
end
fclose(fd);
