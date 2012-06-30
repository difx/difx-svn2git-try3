% function [data,samplestats]=read_mk5b(fid, Nframes, Nch, chlist)
% Read and decode 2-bit data frames from a file descriptor.
% Data must be in Mark5B format with any power-of-2 number
% of channels. The current read offset of the file descriptor
% must point to the start of a Mark5B frame (with header).
%
% fid: file descriptor
% Nframes: number of frames to read
% Nch: number of channels in the data
% chlist: list of channels for which data should be extracted ([1:Nch])
%
% Returns:
%   channeldata([chlist],1:numsamples) array with floating point samples
%   samplestats(1:Nch,4) array of sample state counts
%
% Note: the file should be opened with little-endian format ('ieee-le'),
% for example fopen('thefile.m5b','rb','ieee-le')

function [data,samplestats]=read_mk5b(fid, Nframes, Nch, chlist)

 %% derived parameters
 Nbitpersample = 2;
 NsamplesPerFrame = 10e3 * (8/Nbitpersample);
 NChSamplesPerFrame = NsamplesPerFrame / Nch;
 Nbins = Nframes * NChSamplesPerFrame;
 samplestats = zeros([Nch 4]);

 %% 2-bit to float conversion table
 Vsigma1 = 0.9816; Vsigma2 = 3.3359;
 % -- actual VLBA
 %vlbamap = [Vsigma1 Vsigma2 -Vsigma1 -Vsigma2]; % sign,mag (00, 01, 10, 11)
 %vlbamap = [Vsigma1 -Vsigma1 Vsigma2 -Vsigma2]; % mag,sign (00, 01, 10, 11)
 % -- for DBBC it seems the distribution is
 % 00:2924(14.62%) 01:7347(36.73%) 10:7260(36.30%) 11:2469(12.35%)
 % and hence the mapping is a rather wrong one as follows
 vlbamap = [-Vsigma2 -Vsigma1 Vsigma1 Vsigma2]; % could be this
 vlbamap = [-Vsigma2 Vsigma1  -Vsigma1 Vsigma2]; % but might be this one too
     
 %% read data from file, Mark5B format (16 byte header, 10.000 byte data)
 data = zeros([max(size(chlist)) Nbins]);
 for ii=1:Nframes,
    hdr = fread(fid, [16], 'ubit8'); % header
    if (~(hdr(1)==hex2dec('ed') && hdr(2)==hex2dec('de') && hdr(3)==hex2dec('ad') && hdr(4)==hex2dec('ab'))),
        fprintf(1, 'Header %x%x%x%x\n', hdr(1), hdr(2), hdr(3), hdr(4));
    end
    tmp = fread(fid, [Nch NChSamplesPerFrame], 'ubit2'); 
    % accumulate 2-bit value distribution into statistics
    for bitval=0:3,
        samplestats(:,bitval+1) = samplestats(:,bitval+1) + sum((tmp(:,:)==bitval),2);
    end
    % convert 2-bit values into floating point
    tmp = vlbamap(tmp+1);
    % append data
    ostart = (ii-1)*NChSamplesPerFrame + 1;
    oend = ostart + NChSamplesPerFrame - 1;
    data(:,ostart:oend) = tmp(chlist,:);
 end

 if 0,
     for ii=1:max(size(chlist)),
        chnr = chlist(ii);
        fprintf(1, 'Channel #%02d mean %+4.3f ', chnr, mean(data(ii,:)));
        totalpwr = sum(samplestats(chnr,:));
        estmean = (samplestats(chnr,:) * vlbamap') / totalpwr;
        fprintf(' 00:%d(%.2f%%) 01:%d(%.2f%%) 10:%d(%.2f%%) 11:%d(%.2f%%) => %f\n', ...
            samplestats(ii,1), 100*samplestats(chnr,1)/totalpwr, ...
            samplestats(ii,2), 100*samplestats(chnr,2)/totalpwr, ...
            samplestats(ii,3), 100*samplestats(chnr,3)/totalpwr, ...
            samplestats(ii,4), 100*samplestats(chnr,4)/totalpwr, ...
            estmean);
     end
 end 