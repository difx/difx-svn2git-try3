% test_iirsos(coeffile,L) 
%    Loads coefficients from .coeff file and filters a fixed DC 
%    signal (1.5+i2.0) using that filter on 'L' identical samples.
function [flt,rflt,uflt] = test_iirsos(coeffile,L)

verbose = 0;
Nt = L; %32e3; % number of samples per channel
Nchannels = 4; % number of channels
filtered = zeros(Nchannels, Nt); % time series
unfiltered = ones(Nt, 1) .* (1.5 + i*2.0);
noiseF = 0;
unfiltered = unfiltered + noiseF*(randn(size(unfiltered)) + i*randn(size(unfiltered)));

%% load Nsections, Giir, Ciir from file
load(coeffile, 'Giir', 'Ciir', 'flt_hds_single');
Nsections=size(Ciir,1);

%% direct form II filter (more efficient)
% http://cnx.org/content/m11919/latest/
% http://www.earlevel.com/Digital%20Audio/Biquads.html -- transposed form
% http://www.earlevel.com/Digital%20Audio/StateVar.html -- state var osc
if 1,
  %% allocate IIR SOS scratch space for TF type   filters
  input = zeros(1, Nchannels);
  output = zeros(1, Nchannels);
  storagemid = zeros(Nchannels, Nsections, 2);
  storageout = zeros(Nchannels, Nsections);
    
  %% apply filtering
  Nt_eff = Nt;
  %Nt_eff = 50;
  for ii=1:Nt_eff,

      % move new data in
      input = Giir * unfiltered(ii);
      % Ciir:
      %        b0(1)     b1(1)     b2(1)     a0(1)     a1(1)     a2(1)
      % =  Ciir(:,1) Ciir(:,2) Ciir(:,3) Ciir(:,4) Ciir(:,5) Ciir(:,6)

      % apply filter
      if (verbose), 
          fprintf(1, 'filter() in[0]=%f\n', input(1));
      end
      for jj=1:Nsections,
          b0 = Ciir(jj,1); % == 1
          b1 = Ciir(jj,2);
          b2 = Ciir(jj,3); % often 1
          a0 = Ciir(jj,4); % == 1
          a1 = Ciir(jj,5);
          a2 = Ciir(jj,6);
          
          % DF II direct -- least storage requirements
        if 0,
          presum = input - a1*storagemid(:,jj,1) - a2*storagemid(:,jj,2); % 1xcMADD, 1xcMADD
          output = presum + b1*storagemid(:,jj,1) + b2*storagemid(:,jj,2); % 1xcMADD, 1xcMADD
          % b0==1, was: output = b0*presum + b1*storagemid(:,jj,1) + b2*storagemid(:,jj,2); % 
          if (verbose), 
              fprintf(1, 'jj=%d in=%e out=%e sum=%e s[0]=%e s[1]=%e\n', jj-1, input(1), output(1), presum(1), storagemid(1,jj,1), storagemid(1,jj,2));
          end
          storagemid(:,jj,2) = storagemid(:,jj,1); % 1xMEMCPY
          storagemid(:,jj,1) = presum; % 1xMEMCPY
          input = output;
        else
          % DF II transposed -- least storage
          output = input + storagemid(:,jj,1); % 1*ADD
          storagemid(:,jj,1) = storagemid(:,jj,2) + b1*input - a1*output; % 1xcMADD, 1xcMADD
          storagemid(:,jj,2) = b2*input - a2*output; % 1xcMUL, 1xcMADD
          input = output;
        end
      end
      
      % get output from last section
      filtered(:, ii) = output;
   end
end

flt = filtered(1,:);
uflt = unfiltered;
rflt = filter(flt_hds_single,unfiltered)';

if 1,
  axv=[0 Nt -2 3];
  figure(1), clf,
    subplot(3,2,1), plot(real(uflt)), axis(axv), title('Real() unfiltered'),
    subplot(3,2,2), semilogy(abs(fft(real(uflt)))),
    subplot(3,2,3), plot(real(rflt)), axis(axv), title('Real() ref filtered'),
    subplot(3,2,4), semilogy(abs(fft(real(rflt)))),
    subplot(3,2,5), plot(real(flt)), axis(axv), title('Real() usr filtered'),
    subplot(3,2,6), semilogy(abs(fft(real(flt)))),
  if 0,
      figure(2), clf,
        subplot(3,2,1), plot(imag(uflt)), axis(axv), title('Imag() unfiltered'),
        subplot(3,2,2), semilogy(abs(fft(imag(uflt)))),
        subplot(3,2,3), plot(imag(rflt)), axis(axv), title('Imag() ref filtered'),
        subplot(3,2,4), semilogy(abs(fft(imag(rflt)))),
        subplot(3,2,5), plot(imag(flt)), axis(axv), title('Imag() usr filtered'),
        subplot(3,2,6), semilogy(abs(fft(imag(flt)))),
  end
%  figure(3),
%    plot(real(rflt)-real(flt)), title('Ref vs own');
end
rflt(end),
flt(end)

