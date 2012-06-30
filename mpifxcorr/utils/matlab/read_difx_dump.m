function [d,deof]=read_difx_dump(fd, Nfft, Nch)

mode = 1;

d = zeros(Nfft, Nch);
deof = 0;

if mode==0,
    Ld = [Nch*Nfft 2];
else
    Ld = [2 Nch*Nfft];
end;

%% Read large Nfft*Nch chunk instead of several Nch-sized pieces
% fread(fd, [Nch timesteps], ...)
x = fread(fd, Ld, 'float32');
if (not(size(x) == Ld)), 
    fprintf(1, 'Out of data at %d/%d\n', size(x,2-mode)/Nch, Nfft);
    deof = 1;
    return
end

%% Convert real,real pairs into complex 
if mode==0,
    x = x(:,1) + i*x(:,2);
else
    x = x(1,:) + i*x(2,:);
end

%% Reformat chunk into Nch-sized pieces
% reshape() always fills columns first, so each column is 1x512 ch of 1FFT
x = reshape(x, [Nch Nfft]); 

%% Output data
d = transpose(x); % raw data
% d = transpose(x .* conj(x)); % autocorrelation

