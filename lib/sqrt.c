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


#ifdef CALC_TBL
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#else
   extern short sqrttbl[];
#endif

   unsigned short intsqrt (short x)
   {
      short res;
      if(x>=0){
         res = sqrttbl[x];	
      }
      else{
         res = (short)0x0FFFF;
      }
      return res;
   }



#ifdef CALC_TBL
   void
   gen_sqrt_tbl (void)
   {
      float x;
      short y;
      FILE *fid;
      int n, i;
   
      fid = fopen ("sqrttbl.c", "wt");
      fprintf (fid, "short sqrttbl[] = \n");
      fprintf (fid, "{\n");
   
      n = 8;
      for (i = 0; i < 32767; i++)
      {
         x = (float) i / 32768;
         y = (short) (sqrt(x) * 32768);
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
