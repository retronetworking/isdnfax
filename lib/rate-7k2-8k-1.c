/* $Id$
******************************************************************************

   Fax program for ISDN.
   A filter for use when rateconverting from 7200 Hz to 8000 Hz after
   the modulator.  The upsample factor is 10, downsample is 9.
   This particular filter bandpasses the signal to 500-2900 Hz,
   with -4.5dB at the limits.  Attenuation is -60dB at 3600Hz.
   Filter is 247 coefs long, linear phase, with 3 added zeros to
   get 250 coefs total (divisible by upsample factor of 10).

   Copyright (C) 1998 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
  
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   THE AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

******************************************************************************
*/


signed short rate_7k2_8k_1[250] = {
  0,-2,-4,-6,-9,-12,-14,-16,-16,-15,-12,-7,0,10,22,35,49,63,75,86,93,96,
  94,85,70,49,22,-9,-43,-78,-111,-140,-161,-174,-175,-164,-140,-103,-56,
  -1,58,118,172,215,243,251,234,191,121,24,-96,-234,-384,-538,-687,-821,
  -932,-1011,-1052,-1051,-1007,-920,-798,-647,-480,-309,-150,-19,69,101,
  65,-45,-232,-494,-822,-1203,-1620,-2049,-2468,-2849,-3168,-3403,-3533,
  -3547,-3439,-3211,-2874,-2447,-1958,-1441,-934,-480,-119,109,171,43,-291,
  -833,-1570,-2477,-3513,-4628,-5757,-6830,-7774,-8512,-8975,-9100,-8835,
  -8147,-7019,-5454,-3478,-1138,1500,4353,7324,10304,13183,15848,18195,
  20128,21569,22459,22759,22459,21569,20128,18195,15848,13183,10304,7324,
  4353,1500,-1138,-3478,-5454,-7019,-8147,-8835,-9100,-8975,-8512,-7774,
  -6830,-5757,-4628,-3513,-2477,-1570,-833,-291,43,171,109,-119,-480,-934,
  -1441,-1958,-2447,-2874,-3211,-3439,-3547,-3533,-3403,-3168,-2849,-2468,
  -2049,-1620,-1203,-822,-494,-232,-45,65,101,69,-19,-150,-309,-480,-647,
  -798,-920,-1007,-1051,-1052,-1011,-932,-821,-687,-538,-384,-234,-96,24,
  121,191,234,251,243,215,172,118,58,-1,-56,-103,-140,-164,-175,-174,-161,
  -140,-111,-78,-43,-9,22,49,70,85,94,96,93,86,75,63,49,35,22,10,0,-7,-12,
  -15,-16,-16,-14,-12,-9,-6,-4,-2,0,0
};
