/*
   ******************************************************************************

   Fax program for ISDN.

   Copyright (C) 1999 Oliver Eichler [oliver.eichler@regensburg.netsurf.de

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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ifax/ifax.h>

#include "../include/ifax/modules/V.29_demod.h"

#define 		PHASEINC1700HZ 	15474

#define		DEG0					0x0000
#define		DEG45					0x2000
#define		DEG90					0x4000
#define		DEG135				0x6000
#define		DEG180				0x8000
#define		DEG225				0xA000
#define		DEG270				0xC000
#define		DEG315				0xE000

#define		A9600					8
#define		B9600					15

#define 		NOSYMB				3
#define 		NOSAMP				3

#define		L_MIN					0x0006
#define		L_2					0x0013

#define		NIL					0x0
#define		LISTEN				0x1
#define		PHASEHUNT			0x2

   typedef struct 
   {
      signed short Re;
      signed short Im;
      unsigned short angl;
   }
   V29_symbol;

   V29_symbol V29_symb_tbl[] =
   {
   {0x4CCC, 0x0000, DEG0},					/* P=000 Q=0  (0)   */
   {0x7FFF, 0x0000, DEG0},					/* P=000 Q=1  (0)   */
   {0x1999, 0x1999, DEG45},				/* P=001 Q=0  (45)  */
   {0x4CCC, 0x4CCC, DEG45},				/* P=001 Q=1  (45)  */
   {0x0000, 0x4CCC, DEG90},				/* P=010 Q=0  (90)  */
   {0x0000, 0x7FFF, DEG90},				/* P=010 Q=1  (90)  */
   {0xE666, 0x1999, DEG135},				/* P=011 Q=0  (135) */
   {0xB333, 0x4CCC, DEG135},				/* P=011 Q=1  (135) */
   {0xB333, 0x0000, DEG180},				/* P=100 Q=0  (180) */
   {0x8000, 0x0000, DEG180},				/* P=100 Q=1  (180) */
   {0xE666, 0xE666, DEG225},				/* P=101 Q=0  (225) */
   {0xB333, 0xB333, DEG225},				/* P=101 Q=1  (225) */
   {0x0000, 0xB333, DEG270},				/* P=110 Q=0  (270) */
   {0x0000, 0x8000, DEG270},				/* P=110 Q=1  (270) */
   {0x1999, 0xE666, DEG315},				/* P=111 Q=0  (315) */
   {0x4CCC, 0xB333, DEG315},				/* P=111 Q=1  (315) */
   }; 

   typedef struct
   {
      unsigned short w;
      int phi;
      short a;
      short power;
      int state;
      short dem_lpf_buf[NOSYMB * NOSAMP];
      short dem_re[NOSYMB * NOSAMP];
      short dem_im[NOSYMB * NOSAMP];
      unsigned short dem_angl[2 * NOSYMB * NOSAMP];
      int dem_angl_dif[NOSYMB * NOSAMP];
      int dem_angl_dist[NOSYMB * NOSAMP];
      int dem_index;
      int smpl_cnt;
      V29_symbol current_symbol;
   
   }
   V29demod_private;

   short dem_lpf_coef[] =
   {0x203E, 0x3F82, 0x203E};


   short DCD (short s, V29demod_private * priv);
   short Demod (short s, V29demod_private * priv);
   short FindSeq2(V29demod_private * priv);
   short PhaseHuntSeq2(V29demod_private * priv);

   void
   V29demod_destroy (ifax_modp self)
   {
   /* V29demod_private *priv=(V29demod_private *)self->private; */
   
      free (self->private);
   
      return;
   }

   int
   V29demod_command (ifax_modp self, int cmd, va_list cmds)
   {
      return 0;			/* Not yet used. */
   }

   int
   V29demod_handle (ifax_modp self, void *data, size_t length)
   {
      V29demod_private *priv = (V29demod_private *) self->private;
      int n;
      short *ps_s = data;
   
      for (n = 0; n < length; n++)
      {
         switch (priv->state)
         {
         
            case NIL:
               DCD (*ps_s, priv);
            
               ps_s++;
               break;
         
            case LISTEN:
               DCD (*ps_s, priv);
               Demod (*ps_s, priv);
               FindSeq2(priv);
            
               ps_s++;
               break;
         
            case PHASEHUNT:
               DCD (*ps_s, priv);
               Demod (*ps_s, priv);
               PhaseHuntSeq2(priv);
            
               ps_s++;
               break;
         }
      }
   
      return 0;
   }


   static void
   V29demod_demand (ifax_modp self, size_t demand)
   {
      V29demod_private *priv = (V29demod_private *) self->private;
   
      return;
   }

   int
   V29demod_construct (ifax_modp self, va_list args)
   {
      V29demod_private *priv;
   
   
      if (NULL == (priv = self->private = malloc (sizeof (V29demod_private))))
         return 1;
      self->destroy = V29demod_destroy;
      self->handle_input = V29demod_handle;
      self->command = V29demod_command;
      self->handle_demand = V29demod_demand;
   
      priv->w = 0;
      priv->phi = 0;
      priv->a = 0x0288;		/* equals a tau of 50 */
      priv->state = NIL;
      priv->dem_index = 0;
      priv->smpl_cnt = -1;
      return 0;
   }


   short
   Demod (short s, V29demod_private * priv)
   {
      static int cnt = 0;
      int sum_Re = 0;
      int sum_Im = 0;
      unsigned short w;	
   
      priv->dem_index = (priv->dem_index < NOSYMB * NOSAMP - 1) ?
         (priv->dem_index + 1) : 0;
   
      w = priv->w + priv->phi;
      switch (cnt)
      {
         case 0:
            priv->dem_lpf_buf[0] = (s * intcos (w)) >> 15;
            priv->dem_lpf_buf[1] = (s * intsin (w)) >> 15;
            sum_Re += priv->dem_lpf_buf[0] * dem_lpf_coef[0];
            sum_Im += priv->dem_lpf_buf[1] * dem_lpf_coef[0];
            sum_Re += priv->dem_lpf_buf[2] * dem_lpf_coef[2];
            sum_Im += priv->dem_lpf_buf[3] * dem_lpf_coef[2];
            sum_Re += priv->dem_lpf_buf[4] * dem_lpf_coef[1];
            sum_Im += priv->dem_lpf_buf[5] * dem_lpf_coef[1];
            cnt++;
            break;
         case 1:
            priv->dem_lpf_buf[2] = (s * intcos (w)) >> 15;
            priv->dem_lpf_buf[3] = (s * intsin (w)) >> 15;
            sum_Re += priv->dem_lpf_buf[0] * dem_lpf_coef[1];
            sum_Im += priv->dem_lpf_buf[1] * dem_lpf_coef[1];
            sum_Re += priv->dem_lpf_buf[2] * dem_lpf_coef[0];
            sum_Im += priv->dem_lpf_buf[3] * dem_lpf_coef[0];
            sum_Re += priv->dem_lpf_buf[4] * dem_lpf_coef[2];
            sum_Im += priv->dem_lpf_buf[5] * dem_lpf_coef[2];
            cnt++;
            break;
         case 2:
            priv->dem_lpf_buf[4] = (s * intcos (w)) >> 15;
            priv->dem_lpf_buf[5] = (s * intsin (w)) >> 15;
            sum_Re += priv->dem_lpf_buf[0] * dem_lpf_coef[2];
            sum_Im += priv->dem_lpf_buf[1] * dem_lpf_coef[2];
            sum_Re += priv->dem_lpf_buf[2] * dem_lpf_coef[1];
            sum_Im += priv->dem_lpf_buf[3] * dem_lpf_coef[1];
            sum_Re += priv->dem_lpf_buf[4] * dem_lpf_coef[0];
            sum_Im += priv->dem_lpf_buf[5] * dem_lpf_coef[0];
            cnt = 0;
            break;
      }
   
      sum_Re = sum_Re >> 15;
      sum_Im = sum_Im >> 15;
      priv->dem_re[priv->dem_index] = sum_Re;
      priv->dem_im[priv->dem_index] = sum_Im;
   
      priv->dem_angl[priv->dem_index] = intatan (sum_Im, sum_Re);
      priv->dem_angl[priv->dem_index + NOSYMB * NOSAMP] =
         priv->dem_angl[priv->dem_index];
   
      priv->w += PHASEINC1700HZ;
      return 0;
   }


   short 
   FindSeq2(V29demod_private * priv)
   {
   
      int i;
      int res = 0;
   
      priv->dem_angl_dif[priv->dem_index] =
         priv->dem_angl[priv->dem_index + NOSYMB * NOSAMP] -
         priv->dem_angl[priv->dem_index + NOSYMB * NOSAMP - 3];
   
      if(priv->dem_angl_dif[priv->dem_index] > 0){
         priv->dem_angl_dist[priv->dem_index] = priv->dem_angl_dif[priv->dem_index] - DEG135;
      }
      else{
         priv->dem_angl_dist[priv->dem_index] = DEG135 + priv->dem_angl_dif[priv->dem_index];
      }
   
      abs(priv->dem_angl_dist[priv->dem_index])	;
   
   
      if(priv->dem_index == 0)
      {
      
         static int flag = 1;
      
         res |= (priv->dem_angl_dist[0] < priv->dem_angl_dist[NOSYMB*NOSAMP-1]) && 
            (priv->dem_angl_dist[0] < priv->dem_angl_dist[1]) ? 0 : 1;
         res <<= 1;
         for(i=1; i< NOSYMB*NOSAMP-1; i++){
            res |= (priv->dem_angl_dist[i] < priv->dem_angl_dist[i-1]) && 
               (priv->dem_angl_dist[i] < priv->dem_angl_dist[i+1]) ? 0 : 1;
            res <<= 1;
         }
         res |= (priv->dem_angl_dist[NOSYMB*NOSAMP-1] < priv->dem_angl_dist[NOSYMB*NOSAMP-2]) && 
            (priv->dem_angl_dist[NOSYMB*NOSAMP-1] < priv->dem_angl_dist[0]) ? 0 : 1;
      
         if (res == 0x1B6) priv->smpl_cnt = 1;
         if (res == 0x16B) priv->smpl_cnt = 0;
         if (res == 0x0DB) priv->smpl_cnt = 5;
         if (priv->smpl_cnt < 0){
            if(flag){
               flag = 0;
            }
            else{
               priv->phi += DEG180;
               priv->phi &= 0x0000FFFF;
               flag = 1;
            }
         }
         else{
            if(priv->dem_angl_dif[priv->dem_index + priv->smpl_cnt] < 0)
               priv->current_symbol = V29_symb_tbl[B9600];
            else
               priv->current_symbol = V29_symb_tbl[A9600];
            priv->state = PHASEHUNT;
         }
      }
   
      return res;
   }

   short 
   PhaseHuntSeq2(V29demod_private * priv)
   {
   
      if(priv->smpl_cnt == 0){
         priv->phi += 
            priv->current_symbol.angl - 
            priv->dem_angl[priv->dem_index];
      
         if(priv->current_symbol.angl == DEG180)
            priv->current_symbol = V29_symb_tbl[B9600];	
         else
            priv->current_symbol = V29_symb_tbl[A9600];
         priv->smpl_cnt = 3;
      }
   
      priv->smpl_cnt--;
      return 0;
   }

   short
   DCD (short s, V29demod_private * priv)
   {
      int s_s;
   /* Power Measurement   s_s = a*(s[n] ) + s_s[n-1]*(1-a) */
   
      s_s = (s * s) >> 15;
      s_s = s_s - priv->power;
      s_s = (s_s * priv->a) >> 15;
      s_s = s_s + priv->power;
   
      priv->power = s_s;
   
      if (s_s < L_MIN)
      {
         priv->state = NIL;
         return s_s;
      }
      if (s_s > L_2)
      {
         if(priv->state == NIL) priv->state = LISTEN;
         return s_s;
      }
      return s_s;
   }
