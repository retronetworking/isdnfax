/*
   ******************************************************************************

   Fax program for ISDN.
	
	This is the V.29 demodulator. The module handles the training phase as well
	as the final data transmission.

   Copyright (C) 1999 Oliver Eichler [oliver.eichler@regensburg.netsurf.de]

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
#define 		INTERVAL_CNT		2


#define		X1						0x1555
#define		X2						0x2AAA
#define		X3						0x4000
#define		X4						0x5555
#define		X5						0x6AAA
#define		X6						0x7FFF
#define		Y1						0x1555
#define		Y2						0x2AAA
#define		Y3						0x4000
#define		Y4						0x5555
#define		Y5						0x6AAA
#define		Y6						0x7FFF


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
#define		C9600					0
#define		D9600					7


#define 		NOSYMB				3
#define 		NOSAMP				3

#define		L_MIN					0x0006
#define		L_2					0x0013

#define		NIL					0x0
#define 		ESTIMATEGAIN		0x1
#define		SEG2HUNT				0x2
#define		PHASEHUNT			0x3
#define		GAINHUNT				0x4
#define		TRAINEQ				0x5

#define		TAU50
#undef		TAU30
#ifdef TAU50
	#define 	AGCESTDLY			50
	#define 	TAU					0x0288
#endif
#ifdef TAU30
	#define 	AGCESTDLY			30
	#define 	TAU					0x0432
#endif
#define		B						0x2000
#define		LEVEL					0x16D6
#define		LEVEL63				0x0E63
#define		SQRT_LEVEL63		0x2AE9

   typedef struct 
   {
      signed short Re;
      signed short Im;
      unsigned short angl;
      int	point;
   }
   V29_symbol;

   V29_symbol V29_symb_tbl[] = {
   {0x4000, 0x0000, DEG0, 0},					/* P=000 Q=0  (0)   */
   {0x6AAA, 0x0000, DEG0, 1},					/* P=000 Q=1  (0)   */
   {0x1555, 0x1555, DEG45, 2},				/* P=001 Q=0  (45)  */
   {0x4000, 0x4000, DEG45, 3},				/* P=001 Q=1  (45)  */
   {0x0000, 0x4000, DEG90, 4},				/* P=010 Q=0  (90)  */
   {0x0000, 0x6AAA, DEG90, 5},				/* P=010 Q=1  (90)  */
   {0xEAAB, 0x1555, DEG135, 6},				/* P=011 Q=0  (135) */
   {0xC000, 0x4000, DEG135, 7},				/* P=011 Q=1  (135) */
   {0xC000, 0x0000, DEG180, 8},				/* P=100 Q=0  (180) */
   {0x9556, 0x0000, DEG180, 9},				/* P=100 Q=1  (180) */
   {0xEAAB, 0xEAAB, DEG225, 10},				/* P=101 Q=0  (225) */
   {0xC000, 0xC000, DEG225, 11},				/* P=101 Q=1  (225) */
   {0x0000, 0xC000, DEG270, 12},				/* P=110 Q=0  (270) */
   {0x0000, 0x9556, DEG270, 13},				/* P=110 Q=1  (270) */
   {0x1555, 0xEAAB, DEG315, 14},				/* P=111 Q=0  (315) */
   {0x4000, 0xC000, DEG315, 15}				/* P=111 Q=1  (315) */
   }; 

   typedef struct
   {
      unsigned short agc_gain;
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

   short dem_lpf_coef[] = {0x203E, 0x3F82, 0x203E};
   int FirstQuadTbl[] = {0,1,2,3,4,5,2,3};
   int ReMinusTbl[] = {8,9,6,7,4,5};
   int ImMinusTbl[] = {0,1,14,15,12,13,10,11,8,9};


   short DigitalCarrierDetect (short s, V29demod_private * priv);
   short Demod (short s, V29demod_private * priv);
   short FindSeq2(V29demod_private * priv);
   short AdjustPhase(V29demod_private * priv);
   V29_symbol SymbolMapping(V29demod_private * priv);
   void EstimateGain(V29demod_private *priv);
   short InputGain(short s, V29demod_private * priv);
   void AdjustGain(V29demod_private * priv);
   void CalcSeg2Ref(V29demod_private * priv);


   FILE * fid_re;
   FILE * fid_im;
   FILE * fid_sym;

   void V29demod_destroy (ifax_modp self)
   {
   /* V29demod_private *priv=(V29demod_private *)self->private; */
   
      fclose(fid_re);
      fclose(fid_im);
      fclose(fid_sym);
      free (self->private);
   
      return;
   }

   int V29demod_command (ifax_modp self, int cmd, va_list cmds)
   {
      return 0;			/* Not yet used. */
   }

   int V29demod_handle (ifax_modp self, void *data, size_t length)
   {
      V29demod_private *priv = (V29demod_private *) self->private;
      int n;
      short *ps_s = data;
      V29_symbol  estSymb;
   
      for (n = 0; n < length; n++)
      {
      	/* this gain is only for debugging*/
         *ps_s = (*ps_s*0x7FFF)>>15;
         switch (priv->state){
            case NIL:						/* idle and measure power */
               DigitalCarrierDetect (*ps_s, priv);
               break;
         
            case ESTIMATEGAIN:			/* estimate initial input gain */
               DigitalCarrierDetect (*ps_s, priv);		
               EstimateGain(priv);
               break;
         
            case SEG2HUNT:					/* try to detect Segment 2 */
               *ps_s = InputGain(*ps_s, priv);
               DigitalCarrierDetect (*ps_s, priv);
               Demod (*ps_s, priv);
               FindSeq2(priv);
               break;
         
            case PHASEHUNT:				/* synchronize the carrier phase */
               *ps_s = InputGain(*ps_s, priv);
               DigitalCarrierDetect (*ps_s, priv);
               Demod (*ps_s, priv);
            	/* 3:1 downsampling */
               if(priv->smpl_cnt == 0){
                  priv->smpl_cnt = 3;
                  estSymb = SymbolMapping(priv);
                  AdjustPhase(priv);
               
                  if(estSymb.angl == priv->current_symbol.angl)
                     priv->state = GAINHUNT;
                  fprintf(fid_re,"%i \n",(short)priv->dem_re[priv->dem_index]);
                  fprintf(fid_im,"%i \n",(short)priv->dem_im[priv->dem_index]);
                  fprintf(fid_sym,"%i \n",priv->current_symbol.point);
                  CalcSeg2Ref(priv);
               }
            
               priv->smpl_cnt--;
               break;
         
            case GAINHUNT:				/* fine adjust input gain */
               *ps_s = InputGain(*ps_s, priv);
               DigitalCarrierDetect (*ps_s, priv);
               Demod (*ps_s, priv);
            	/* 3:1 downsampling */
               if(priv->smpl_cnt == 0){
                  priv->smpl_cnt = 3;
               
                  estSymb = SymbolMapping(priv);
               
                  if(estSymb.point == C9600){
                     priv->state = TRAINEQ;
                     priv->current_symbol = V29_symb_tbl[C9600];
                  }
                  else{
                     AdjustPhase(priv);
                     AdjustGain(priv);
                     fprintf(fid_re,"%i \n",(short)priv->dem_re[priv->dem_index]);
                     fprintf(fid_im,"%i \n",(short)priv->dem_im[priv->dem_index]);
                     fprintf(fid_sym,"%i \n",priv->current_symbol.point);
                     CalcSeg2Ref(priv);
                  }
               }
               priv->smpl_cnt--;
               break;
         
            case TRAINEQ:			/* seg. 3 equalizer training */
               *ps_s = InputGain(*ps_s, priv);
               DigitalCarrierDetect (*ps_s, priv);
               Demod (*ps_s, priv);
               /*printf("%04X ",(short)priv->dem_re[priv->dem_index]);
               printf("%04X \n",(short)priv->dem_im[priv->dem_index]);*/
            	/* 3:1 downsampling */
               if(priv->smpl_cnt == 0){
                  priv->current_symbol = SymbolMapping(priv);
                  fprintf(fid_re,"%i \n",(short)priv->dem_re[priv->dem_index]);
                  fprintf(fid_im,"%i \n",(short)priv->dem_im[priv->dem_index]);
                  fprintf(fid_sym,"%i \n",priv->current_symbol.point);
                  AdjustPhase(priv);
                  AdjustGain(priv);
               
                  priv->smpl_cnt = 3;
               }
            
               priv->smpl_cnt--;
               break;
         
         }
      
         ps_s++;
      }
   
      return 0;
   }


   static void V29demod_demand (ifax_modp self, size_t demand)
   {
      V29demod_private *priv = (V29demod_private *) self->private;
   
      return;
   }

   int V29demod_construct (ifax_modp self, va_list args)
   {
      V29demod_private *priv;
   
      fid_re = fopen("dem_re.out","wt");
      fid_im = fopen("dem_im.out","wt");
      fid_sym = fopen("dem_sym.out","wt");
   
      if (NULL == (priv = self->private = malloc (sizeof (V29demod_private))))
         return 1;
      self->destroy = V29demod_destroy;
      self->handle_input = V29demod_handle;
      self->command = V29demod_command;
      self->handle_demand = V29demod_demand;
   
      priv->w = 0;						/* the carrier's omega */
      priv->phi = 0;						/* phi to achive carrier sync. */
      priv->a = TAU;
      priv->state = NIL;				/* state variable for the demodulators statemachine*/
      priv->dem_index = 0;				/* index of the actual demodulated sample within
   												the demodulator buffers */
      priv->smpl_cnt = -1;				/* offset for symbol sync. 
   												-1 stands for 'no symb. found' */
      priv->agc_gain = 0x0D55;		/* equals 0.8333 in (4:12) format */ 
      return 0;
   }



   short InputGain(short s, V29demod_private * priv)
   {
      return (s*priv->agc_gain)>>12;
   }

   void EstimateGain(V29demod_private *priv)
   {
      static int cnt;
      long	num;
      short g;
   
      cnt++;
   	/* tau tabs after digital carrier detect the measured power
   		should be 63% of the final power. A good time to estimate
   		the initial input gain factor. */
      if(cnt == AGCESTDLY){
         num = SQRT_LEVEL63<<12;
         g = num/intsqrt(priv->power);
         priv->agc_gain = g;
         printf("I would guess gain is %04X\n", g);
         priv->state = SEG2HUNT;
      }
   }

   void AdjustGain(V29demod_private * priv)
   {
      short e;
   	/* we know the symbol - we know the needed gain */
      e = priv->current_symbol.Re - priv->dem_re[priv->dem_index];
      e = (e * B) >> 18;
      priv->agc_gain -= e;
      printf("agc gain: %04X\n",priv->agc_gain);
   }

   short Demod (short s, V29demod_private * priv)
   {
      static int cnt = 0;
      int sum_Re = 0;
      int sum_Im = 0;
      unsigned short w;	
   
   	/*modulo increment of sample index */
      priv->dem_index = (priv->dem_index < NOSYMB * NOSAMP - 1) ?
         (priv->dem_index + 1) : 0;
   
   	/* calulate phase corrected omega */
      w = priv->w + priv->phi;
   
   	/*	demodulate signal. The lowpass filter supresses frequencies
   		at 2*f_c */
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
   
      sum_Re = sum_Re >> 14;
      sum_Im = sum_Im >> 14;
      priv->dem_re[priv->dem_index] = sum_Re;
      priv->dem_im[priv->dem_index] = sum_Im;
   
   	/* calculate the absolute angle of the signal */
      priv->dem_angl[priv->dem_index] = intatan (sum_Im, sum_Re);
      priv->dem_angl[priv->dem_index + NOSYMB * NOSAMP] =
         priv->dem_angl[priv->dem_index];
   
      priv->w += PHASEINC1700HZ;
      return 0;
   }


   V29_symbol SymbolMapping(V29demod_private * priv)
   {
      short re = priv->dem_re[priv->dem_index];
      short im = priv->dem_im[priv->dem_index];
      short abs_re;
      short abs_im;
      short tmp;
      int d1, d2, point;
      int offset = 0;
   
      abs_re = re < 0 ? -re : re;
      abs_im = im < 0 ? -im : im;
   
      if(abs_re < abs_im){
         tmp = abs_re;
         abs_re = abs_im;
         abs_im = tmp;
         offset = 4;
      }
   
      if(abs_re < X4){
         if((abs_re+abs_im) < X2+Y2){
            d1 = (abs_re - X3)*(abs_re - X3) + abs_im*abs_im;
            d2 = (abs_re - X1)*(abs_re - X1) + (abs_im-Y1)*(abs_im-Y1);
            point = d1 > d2  ? 2 : 0;
         }
         else{
            d1 = (abs_re - X3)*(abs_re - X3) + abs_im*abs_im;
            d2 = (abs_re - X3)*(abs_re - X3) + (abs_im-Y3)*(abs_im-Y3);
            point = d1 > d2 ? 3 : 0;
         }
      }
      else{
         d1 = (abs_re - X5)*(abs_re - X5) + abs_im*abs_im;
         d2 = (abs_re - X3)*(abs_re - X3) + (abs_im-Y3)*(abs_im-Y3);
         point = d1 > d2 ? 3 : 1;
      } 
   
      point = FirstQuadTbl[offset+point];
      if(re < 0) point = ReMinusTbl[point];
      if(im < 0) point = ImMinusTbl[point];
   
      printf("%i\n",point);
      return V29_symb_tbl[point];
   }

   void CalcSeg2Ref(V29demod_private * priv)
   {
      if(priv->current_symbol.angl == DEG180){
         priv->current_symbol = V29_symb_tbl[B9600];	
      }
      else
         priv->current_symbol = V29_symb_tbl[A9600];
   }

   short 
   FindSeq2(V29demod_private * priv)
   {
   
      int i;
      int res = 0;
   
   	/* calculate the relative angle between two symbols */
      priv->dem_angl_dif[priv->dem_index] =
         priv->dem_angl[priv->dem_index + NOSYMB * NOSAMP] -
         priv->dem_angl[priv->dem_index + NOSYMB * NOSAMP - 3];
   
   	/* calculate the absolute distance to the expected 135? phase difference */
      if(priv->dem_angl_dif[priv->dem_index] > 0){
         priv->dem_angl_dist[priv->dem_index] = priv->dem_angl_dif[priv->dem_index] - DEG135;
      }
      else{
         priv->dem_angl_dist[priv->dem_index] = DEG135 + priv->dem_angl_dif[priv->dem_index];
      }
      abs(priv->dem_angl_dist[priv->dem_index])	;
   
   	/* check for sequence 2 every NOSYMB*NOSAMP */
      if(priv->dem_index == 0)
      {
      
         static int flag = INTERVAL_CNT;
      
      	/* find minima within angle distance buffer */
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
      
      	/* valid received symbols will produce the binary patterns:
      		
      			110110110b
      			101101101b
      			011011011b
      	
      	Depending on the point of maximum likelihood for sampling, given by 
      	the positions of '0''s in the result line, the initial value for 
      	the sample counter is set.
      	
      	Depending on the absolute phases of the symbols the angle difference
      	can be either 135? or 225?. As the distance is only derived for 135?
      	the received signal is turned by 180? after INTERVAL_CNT (>0) decision
      	intervals.
      	*/
         if (res == 0x1B6) priv->smpl_cnt = 1;
         if (res == 0x16B) priv->smpl_cnt = 0;
         if (res == 0x0DB) priv->smpl_cnt = 5;
         if (priv->smpl_cnt < 0){
            if(flag){
               flag--;
            }
            else{
               priv->phi += DEG180;
               priv->phi &= 0x0000FFFF;
               flag = INTERVAL_CNT;
            }
         }
         else{ /* symbol sync. achieved */
         
         /* depending on the current symbol the sign of the angle difference
         	will change.
         	
         		[n]			[n-1]
         		135?	-		315?		< 0
         		315? 	-		135		> 0
         */
            if(priv->dem_angl_dif[priv->dem_index + priv->smpl_cnt] < 0)
               priv->current_symbol = V29_symb_tbl[B9600];
            else
               priv->current_symbol = V29_symb_tbl[A9600];
            priv->state = PHASEHUNT;
            printf("Locked on symbol \n");
         }
      }
   
      return res;
   }

   short AdjustPhase(V29demod_private * priv)
   {
     	/* adjust phi by the current phase difference */
      priv->phi += 
         priv->current_symbol.angl - 
         priv->dem_angl[priv->dem_index];
   
      return 0;
   }

   short DigitalCarrierDetect (short s, V29demod_private * priv)
   {
      int s_s;
   
   	/* Power Measurement   s_s = a*(s[n] ) + s_s[n-1]*(1-a) */
   
      s_s = (s * s) >> 15;
      s_s = s_s - priv->power;
      s_s = (s_s * priv->a) >> 15;
      s_s = s_s + priv->power;
   
      priv->power = s_s;
      printf("power: 0x%04X\n",s_s);
   
      if (s_s < L_MIN){
         priv->state = NIL;
         return s_s;
      }
      if (s_s > L_2){
         if(priv->state == NIL) priv->state = ESTIMATEGAIN;
         return s_s;
      }
      return s_s;
   }
