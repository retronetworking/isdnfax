/* $Id$
******************************************************************************

   Fax program for ISDN.
   Debugging module. Take input stream and print it out to stderr.
   Then pass it on.

   Copyright (C) 1998 Andreas Beck	[becka@ggi-project.org]
  
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
#include <stdarg.h>
#include <ifax/ifax.h>

#include <ifax/modules/debug.h>

   typedef struct {
   
   /* How many bytes are there per "len" unit ? */
      int bytes_per_sample;
      
      /* See 'debug.h' for details on the formats and methods */
      int output_format;
      int output_method;
   
   } debug_private;

/* Free the private data
 */
   void	debug_destroy(ifax_modp self)
   {
      free(self->private);
      return;
   }

   int	debug_command(ifax_modp self,int cmd,va_list cmds)
   {
      return 0;	/* Not yet used. */
   }

   int	debug_handle(ifax_modp self, void *data, size_t length)
   {
      ifax_uint8 *dat = data;
      ifax_sint16 *s16 = data;
      ifax_uint8 *u8 = data, mask = 1;
      int handled=0;
      int x;
      FILE *file;
   
      debug_private *priv=(debug_private *)self->private;
   
      if ( priv->bytes_per_sample ) {
      
         while(handled<length) {
            for (x=0;x<priv->bytes_per_sample;x++,dat++)
               fprintf(stderr,"%2x(%c) ",*dat,*dat);
         
            fprintf(stderr,"\n");
            fflush(stderr);
         
            handled++;
         }
      } 
      else {
         switch ( priv->output_method ) {
            case DEBUG_METHOD_STDOUT:
               file = stdout;
               break;
            case DEBUG_METHOD_STDERR:
               file = stderr;
               break;
            default:
               fprintf(stderr,"Bad DEBUG_METHOD_*\n");
               exit(1);
         }
      
         while ( handled < length ) {
            switch ( priv->output_format ) {
               case DEBUG_FORMAT_SIGNED16BIT:
                  fprintf(file,"%d\n",(signed int)(*s16++));
                  break;
               case DEBUG_FORMAT_16BIT_HEX:
                  fprintf(file,"0x%04x\n",((signed int)(*s16++))&0xffff);
                  break;
	       case DEBUG_FORMAT_PACKED_BINARY:
		 fprintf(file,"%c",((*u8)&mask)?'1':'0');
		 if ( mask == 0x80 ) {
		   mask = 1;
		   u8++;
		 } else {
		   mask <<= 1;
		 }
		 break;
	       case DEBUG_FORMAT_CONFIDENCE:
		 fprintf(file,"%d\n",u8[0]?u8[1]:-u8[1]);
		 u8 += 2;
		 break;
               default:
                  fprintf(stderr,"Bad DEBUG_FORMAT_*\n");
                  exit(1);
            }
            handled++;
         }
      }
   
      if (self->sendto)
         ifax_handle_input(self->sendto,data,length);
   
      return handled;
   }

   int	debug_construct(ifax_modp self,va_list args)
   {
      debug_private *priv;
   	
      if (NULL==(priv=self->private=malloc(sizeof(debug_private)))) 
         return 1;
   		
      self->destroy		=debug_destroy;
      self->handle_input	=debug_handle;
      self->command		=debug_command;
   
      priv->bytes_per_sample=va_arg(args,int);
   
      if ( priv->bytes_per_sample == 0 ) {
         priv->output_format = va_arg(args,int);
         priv->output_method = va_arg(args,int);
      }
   
      return 0;
   }
