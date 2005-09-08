%
% PostScript encapulator prolog file of the BLT "eps" canvas item.
%
% Copyright 1991-1997 Bell Labs Innovations for Lucent Technologies.
%
% Permission to use, copy, modify, and distribute this software and its
% documentation for any purpose and without fee is hereby granted, provided
% that the above copyright notice appear in all copies and that both that the
% copyright notice and warranty disclaimer appear in supporting documentation,
% and that the names of Lucent Technologies any of their entities not be used
% in advertising or publicity pertaining to distribution of the software
% without specific, written prior permission.
%
% Lucent Technologies disclaims all warranties with regard to this software,
% including all implied warranties of merchantability and fitness.  In no event
% shall Lucent Technologies be liable for any special, indirect or
% consequential damages or any damages whatsoever resulting from loss of use,
% data or profits, whether in an action of contract, negligence or other
% tortuous action, arising out of or in connection with the use or performance
% of this software.
%

%
% The definitions of the next two macros are from Appendix H of 
% Adobe's "PostScript Language Reference Manual" pp. 709-736.
% 

% Prepare for EPS file

/BeginEPSF {				
  /beforeInclusionState save def
  /dictCount countdictstack def		% Save the # objects in the dictionary
  /opCount count 1 sub def		% Count object on operator stack
  userdict begin			% Make "userdict" the current 
					% dictionary
    /showpage {} def			% Redefine showpage to be null
    0 setgray 
    0 setlinecap
    1 setlinewidth
    0 setlinejoin
    10 setmiterlimit
    [] 0 setdash
    newpath
    /languagellevel where {
      pop languagelevel 
      1 ne {
	false setstrokeadjust false setoverprint
      } if
    } if
    % note: no "end"
} bind def

/EndEPSF { %def
  count opCount sub {
    pop
  } repeat
  countdictstack dictCount sub { 
  end					% Clean up dictionary stack
  } repeat
  beforeInclusionState restore
} bind def


%
% Set up a clip region based upon a bounding box (x1, y1, x2, y2).
%
/SetClipRegion {
  % Stack: x1 y1 x2 y2
  newpath
  4 2 roll moveto
  1 index 0 rlineto
  0 exch rlineto
  neg 0 rlineto
  closepath
  clip
  newpath
} def

