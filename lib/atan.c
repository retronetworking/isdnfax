/*
   ******************************************************************************

   Fax program for ISDN.

   Copyright (C) 1999 Oliver Eichler [oliver.eichler@regensburg.netsurf.de

   This function calculates the angle of a complex number x = real, y = imag.
   The angle is represented as: 0..360° -> 0x0000..0xFFFFF


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

#undef CALC_TBL

#define DEG90		0x4000
#define DEG180		0x8000
#define DEG360		0xFFFF

#ifdef CALC_TBL
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#else
   extern short atantbl[];
#endif

   unsigned short
   intatan (short y, short x)
   {
      int tmp;
      short abs_y, abs_x;
      short res;
   
      abs_y = (y < 0) ? -y : y;
      abs_x = (x < 0) ? -x : x;
   
      if (abs_y <= abs_x){
         tmp = abs_y << 15;
         res = atantbl[tmp / abs_x];
      }
      else{
         tmp = abs_x << 15;
         res = DEG90 - atantbl[tmp / abs_y];
      }
   
      if (x > 0){
         res = (y > 0) ? res : (DEG360 - res);
      }
      else{
         res = (y > 0) ? (DEG180 - res) : (DEG180 + res);
      }
   
      return res;
   }



#ifdef CALC_TBL
void
gen_atan_tbl (void)
{
  float x;
  short y;
  FILE *fid;
  int n, i;

  fid = fopen ("atantbl.c", "wt");
  fprintf (fid, "short atantbl[] = \n");
  fprintf (fid, "{\n");

  n = 8;
  for (i = 0; i < 65536; i++)
    {
      x = (float) i / 32768;
      y = (short) (atan (x) / atan (1) * 8192);
      fprintf (fid, "0x%04X, ", y & 0x0000FFFF);
      n--;
      if (!n)
	{
	  fprintf (fid, "\n");
	  n = 8;
	}
    }

  fprintf (fid, "};\n\n");
  fclose (fid);
}
#endif
