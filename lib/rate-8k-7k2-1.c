/* $Id$
******************************************************************************

   Fax program for ISDN.
   A filter for use when rateconverting from 8000 Hz to 7200 Hz before
   the demodulator.  The upsample factor is 9, downsample is 10.
   This particular filter bandpasses the signal to 500-2900 Hz,
   with -4.5dB at the limits.  Attenuation is -60dB at 3600Hz.
   Filter is 247 coefs long, linear phase, with 5 added zeros to
   get 252 coefs total (divisible by upsample factor of 9).

   Copyright (C) 1999 Morten Rolland [Morten.Rolland@asker.mail.telia.com]
  
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


signed short rate_8k_7k2_1[252] = {
  0,0,-2,-3,-6,-8,-11,-13,-14,-15,-14,-11,-6,0,9,19,31,44,56,68,77,84,87,
  84,76,63,44,20,-8,-39,-70,-100,-126,-145,-156,-157,-147,-126,-93,-50,
  -1,52,106,154,194,219,225,211,172,109,22,-86,-211,-346,-484,-618,-739,
  -839,-910,-947,-946,-906,-828,-718,-582,-432,-278,-135,-17,62,91,59,-41,
  -209,-445,-740,-1083,-1458,-1844,-2221,-2564,-2851,-3062,-3180,-3193,
  -3095,-2890,-2587,-2203,-1762,-1297,-841,-432,-107,98,154,38,-262,-750,
  -1413,-2229,-3162,-4165,-5181,-6147,-6996,-7661,-8078,-8190,-7952,-7332,
  -6317,-4908,-3130,-1024,1350,3918,6591,9274,11865,14264,16376,18116,19412,
  20213,20483,20213,19412,18116,16376,14264,11865,9274,6591,3918,1350,-1024,
  -3130,-4908,-6317,-7332,-7952,-8190,-8078,-7661,-6996,-6147,-5181,-4165,
  -3162,-2229,-1413,-750,-262,38,154,98,-107,-432,-841,-1297,-1762,-2203,
  -2587,-2890,-3095,-3193,-3180,-3062,-2851,-2564,-2221,-1844,-1458,-1083,
  -740,-445,-209,-41,59,91,62,-17,-135,-278,-432,-582,-718,-828,-906,-946,
  -947,-910,-839,-739,-618,-484,-346,-211,-86,22,109,172,211,225,219,194,
  154,106,52,-1,-50,-93,-126,-147,-157,-156,-145,-126,-100,-70,-39,-8,20,
  44,63,76,84,87,84,77,68,56,44,31,19,9,0,-6,-11,-14,-15,-14,-13,-11,-8,
  -6,-3,-2,0,0,0
};
