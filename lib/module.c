/* $Id$
******************************************************************************

   Fax program for ISDN.
   Module functionality.

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
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <ifax/ifax.h>

static ifax_module_registry    *ifax_modreg_root=NULL;
static ifax_module_id		ifax_module_lastid=1;

/* Register a module class. Returns a module_id (handle) for the newly
 * registered class on success.
 */
ifax_module_id ifax_register_module_class(char *name,int (*construct)(struct ifax_module *self,va_list args))
{
	ifax_module_registry *newentry;
	
	if (NULL==(newentry=malloc(sizeof(ifax_module_registry))))
		return IFAX_MODULE_ID_INVALID;		/* No memory any more. */

	newentry->next=ifax_modreg_root;
	newentry->id  =ifax_module_lastid++;
	newentry->construct=construct;
	strcpy(newentry->module_name,name);
	ifax_modreg_root=newentry;
	return newentry->id;
}

/* Instantiate a module of the class what_kind. Returns a modp (handle) for 
 * the newly created module on success.
 */
ifax_modp ifax_create_module(ifax_module_id what_kind,...)
{
	ifax_module_registry *current;
	ifax_modp newmodule;
	va_list list;
	
	current=ifax_modreg_root;
	
	while(current)
	{
		if (current->id==what_kind) {

			if (NULL==current->construct) {
				ifax_dprintf(DEBUG_SEVERE,"No constructor for module %s",
					current->module_name);
				break;
			}

			if (NULL==(newmodule=malloc(sizeof(ifax_module)))) 
				break;
			memset(newmodule,0,sizeof(ifax_module));

			newmodule->sendto=NULL;

			va_start(list,what_kind);
			if (current->construct(newmodule,list)) {
				va_end(list);
				free(newmodule);
				break;
			}
			va_end(list);
			return newmodule;
		}
		current=current->next;
	}
	return NULL;
}

/* Send input to a module. Note, that len is defined by the module.
 */
int ifax_handle_input(struct ifax_module *self,void *data,size_t len)
{
	return self->handle_input(self, data,len);
}

/* Send a command to a module. Note, that commands and data are defined 
 * by the module.
 */
int ifax_command(struct ifax_module *self, int command, ...)
{
	va_list list;
	int rc;
	
	va_start(list,command);
	rc=self->command(self, command, list);
	va_end(list);
	
	return rc;
}

