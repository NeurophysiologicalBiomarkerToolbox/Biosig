function [ht,m,r] = ECTBcorr(QRStime,ectb_times);
% Correction of ectopic beat Presence Effect
%
% This Algorithm corrects the effect of ectopic beats, annotated
%    by the function QRScorr, by calculating the corrected Heart Timing Signal,
%    according to the IPFM - Modell
% With the Heart Timing Signal the HRV PSD estimation can be calculated by the 
%    FHTIS - method. (see [1])
% 
% INPUT:
%  QRStime:      Time values of the detected QRS-Complexes
%  ectb_times:   Time values of the ectopic beats generated by
%                   QRScorr(output-variable: ANNOT.mov)
%
% OUTPUT:
%  ht:           ht.data: Heart Timing Signal
%                ht.time: Dedicated Time Series
%  m:            Derivative of the heart timing signal,
%                  dedicated time series is "ht.time"
%  r:            Instantaneous Heart Rate
%                  dedicated time series is "ht.time"
%
%
% Example:
%   [ht,m,r] = ECTBcorr(QRStime_corr,ectb_times);
%
%
% Filename: ECTBcorr.m
% Last modified: 2003/09/15
% Copyright (c) 2003 by Johannes Peham
%
% Reference:
% [1] J. Mateo, P. Laguna, Analysis of Heart Rate Variability in Presence
%      of Ectopic Beats Using the Heart Timing Signal
%     IEEE Transactions on biomedical engineering,
%      Vol.50, No.3, March 2003
%
%
% This library is free software; you can redistribute it and/or
% modify it under the terms of the GNU Library General Public
% License as published by the Free Software Foundation; either
% Version 2 of the License, or (at your option) any later version.
%
% This library is distributed in the hope that it will be useful,
% but WITHOUT ANY WARRANTY; without even the implied warranty of
% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
% Library General Public License for more details.
%
% You should have received a copy of the GNU Library General Public
% License along with this library; if not, write to the
% Free Software Foundation, Inc., 59 Temple Place - Suite 330,
% Boston, MA  02111-1307, USA.


%Input Arguments
   if length(QRStime)<2, error('beat sequence too short'); end;
   if length(ectb_times)<1, error('no ectopic beats to correct'); end;
   
%Conversion into row vectors
   QRStime=QRStime(:)';
   ectb_times=ectb_times(:)';
   
%Time Series
   t=QRStime;

%Indices of the ectopic beats in ke_all
%  ke_all relates to the time series t
   for i=1:length(ectb_times)
      ke_all(i) = find(t==ectb_times(i));
   end;

% Instantaneous heart period hp
   for k=2:length(t)
      hp(k)=(t(k)-t(k-1));
   end
   hp(1)=NaN;
   hp_mean=mean(hp)

%heart period without ectopic beats
   hpD=hp;
   hpD(ke_all)=[];

%Making a time series ti
% without the ectopic beat occurence times
   ti=t;
   ti(ke_all)=[];


%**************************************************%    
% Are ectopic beats consecutive?
%**************************************************%    
   j=1;
   n_all=ones(1,length(ke_all)); %number of consecutive ectopic beats
   while j<length(ke_all)
      while ((j<=length(ke_all)-1)&((ke_all(j+1)-ke_all(j))==1))
         ke_all(j)=[]; %deletion of consecutive indices
         n_all(j)=n_all(j)+1;
      end;
      j=j+1;
   end;
   n_all(length(ke_all)+1:end)=[];

%Initialising the structure for truncation
   trunc.t=[];
   trunc.hp=[];
   trunc.ke_all=[];
   trunc.n_all=[];
 
%if ect. beats are at the end they will be truncated
   if ke_all(end)==length(t)
      trunc.t=(ke_all(end)-n_all(end)+1:ke_all(end));
      trunc.hp=(ke_all(end)-n_all(end)+1:ke_all(end));
      trunc.ke_all=length(ke_all);
      trunc.n_all=length(n_all);
   end;
 
% if ectopic beats are at the beginning they will be truncated
   if (ke_all(1)+1-n_all(1))==1
      trunc.t=[trunc.t (ke_all(1)+1-n_all(1):ke_all(1))];
      trunc.hp=[trunc.hp (ke_all(1)+1-n_all(1):ke_all(1))];
      trunc.ke_all=[trunc.ke_all (1)];
      trunc.n_all=[trunc.n_all (1)];
      ke_all=ke_all - n_all(1);    
   end;

%Truncation
   if length(trunc.t)~=0, t(trunc.t)=[];end;
   hp(trunc.hp)=[];
   ke_all(trunc.ke_all)=[];
   n_all(trunc.n_all)=[];

%**************************************************%
% Spline interpolation of the heart period hp
%**************************************************%
   N=1*length(t);
   ts=(t(end)-t(2))/(N-1);
   t_hpD=t(2):ts:t(end);
   hp(1)=[];
   hpD(1)=[];
   hpD=spline(ti(2:end),hpD,t_hpD);
 
%Initialisation of the forward an backward extended
% virtual beat times (cell-array)
   tb={};
   tf={};

%**************************************************%
% Calculating the real offset sD 
%    referring to the indices after every ectopic beat
%**************************************************%
   for j=1:length(ke_all)
      ke=ke_all(j);

   %**************************************************%
   % virtual beat times
   %**************************************************%
   %forward extended
      tf{j}(1)=t(ke-n_all(j)) + hpD(ke-n_all(j));
      n=1;
      while ((tf{j}(n) < t(ke+1))&((ke + (n+1) -n_all(j)) <= length(hpD)))
         n=n+1;
         tf{j}(n) = tf{j}(n-1) + hpD(ke + n -n_all(j));
      end;    

   %backward extended
      tb{j}(1)=t(ke+1) - hpD(ke+1);
      n=1;
      while ((tb{j}(n) > t(ke-n_all(j)))&((ke - (n+1) + 1) <= length(hpD)))
         n=n+1;
         tb{j}(n) = tb{j}(n-1) - hpD(ke - n + 1); 
      end;    
 
   %**************************************************%
   % calculating sD
   %**************************************************%
   % sD % see Documentation for details
      bn=t(ke-n_all(j));
      an=t(ke+1);
      if (length(tf{j})>=2), b=tf{j}(end-1);
      else b=tf{j}(end); end;
      if (length(tb{j})>=2), a=tb{j}(end-1);
      else a=tb{j}(end); end;

      if length(tf{j})<= 2
         tf_1 = bn;
      else 
         tf_1 = tf{j}(end-2);
      end;

      if length(tb{j}) <= 2
         tb_1 = an;
      else 
         tb_1 = tb{j}(end-2);
      end;
 
      xf = (b-a)/(b-tf_1);
      xb = (b-a)/(tb_1-a);
 
      rectangle = ((b-a)*((length(tf{j})-2) + 1 + (length(tb{j})-2)));
      triangle1 = ((b-a)*xf/2);
      triangle2 = ((b-a)*xb/2);
      A = rectangle - triangle1 - triangle2;
 
      sD(j) = (1/(b - a)) * A
 
   end; % all ect. beats calculated (while)

%**************************************************%
% Calculating T 
%**************************************************%
   T = (t(end)-t(1))/(length(t)-1-sum(n_all)+sum(sD))

%**************************************************%
% Calculating ht
%**************************************************%
   k_ht=[];
%Calculating ht before the first ectopic beat
   for k=1:ke_all(1)-n_all(1)
      ht(k) = t(1) + k*T-t(k);
   end;

%Calculating the sections after the first ectopic beat
   for j=1:length(ke_all)
      ke=ke_all(j);

      for k=ke:length(t)
         ht(k) = t(1) + (k-sum(n_all(1:j))+sum(sD(1:j)))*T - t(k);
      end;
      k_ht=[k_ht (ke-n_all(j)+1:ke)]; %identify the positions of the ectopic beats

   end;
   ht(k_ht)=[];%deleting the ectopic beats, because formula for ht is only valid 
               %  for not ectopic positions (k<ke)&(k>ke) see [1]
   
%**************************************************%
% spline interpolation of ht, resampling
%**************************************************%
   ht_non_resampled=ht;
   N=1*length(t);
   ts=(t(end)-t(1))/(N-1)
   t_hti=t(1):ts:t(end);
   hti=spline(ti,ht,t_hti);
   ht.data=hti;
   ht.time=t_hti; %after resampling ht has an own time series
   
%**************************************************%
% Calculating the derivative of the heart timing signal
% m = d(ht)/dt
%**************************************************%
   m = diff(hti);

%**************************************************%
% Calculating the instantaneous heart rate
%**************************************************%
   r = (1 + m) / T;

return;