/* $Id$
******************************************************************************

   Fax program for ISDN.
   Module layer.

   Copyright (C) 1998 Andreas Beck      [becka@ggi-project.org]
  
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

#ifndef _IFAX_MODULE_H
#define _IFAX_MODULE_H

/*
 * The ideas here were stolen from the EvStack concept, that was developed
 * mainly be me and Jason McMullan for the GGI project 
 * (http://www.ggi-project.org).
 */

/* Every module instance creates such a control structure through which it
 * is referenced.
 */
typedef struct ifax_module {

	/* This function destroys this instance and frees all private data.
	 */
	void	(*destroy)(struct ifax_module *self);


	/* This function gets called from the previous modules in the chain
	 * to handle the data generated.
	 */
	int 	(*handle_input)(struct ifax_module *self, void *data, size_t len);

	/* This function is called to request data from the previous module
	 * in the chain.  The resulting data is delivered by means of the
	 * 'handle_input' method.
	 */
	void	(*handle_demand)(struct ifax_module *self, size_t len);

	/* Use this to implement "commands" to modules like changing operating
	 * parameters "on-the-fly".
	 */
	int 	(*command)(struct ifax_module *self, int command, va_list args);
	

	/* The usual place to store private data.
	 */
	void	*private;

	/* The usual place to send resulting data to.
	 * Some modules (like replicate.c) have their own ideas about that.
	 */
	struct ifax_module	*sendto;

	/* We need a reverse chain as well, to be able to request data from
	 * from the previous module.  The 'replicate' module and similar
	 * will have to know which chain to request data from.
	 */
	struct ifax_module	*recvfrom;

} ifax_module;

/* A module instance is identified by that handle type.
 */
typedef ifax_module *ifax_modp;

/* A module_id is used to reference a module class. It is used to create
 * instances.
 */
typedef unsigned int ifax_module_id;

/* Internal registry of modules.
 */
typedef struct ifax_module_registry {

	struct ifax_module_registry	*next;	/* linked list */

	ifax_module_id	id;

	int	(*construct)(struct ifax_module *self,va_list args);
	
	char module_name[64];

} ifax_module_registry;

/* This return code signifies failure for ifax_register_module_class.
 */
#define IFAX_MODULE_ID_INVALID	0

/* Register a module for the system. You need to know the constructor call.
 */
ifax_module_id ifax_register_module_class(char *name,int (*construct)(struct ifax_module *self,va_list args));

/* Instantiate a module from its class.
 */
ifax_modp ifax_create_module(ifax_module_id what_kind,...);

/* Send input to a module. Note, that len is defined by the module.
 */
int ifax_handle_input(struct ifax_module *self,void *data,size_t len);

/* Request data from previous modules.
 */
void ifax_handle_demand(struct ifax_module *self, size_t len);

/* Send a command to a module. Semantics are defined by the module.
 */
int ifax_command(struct ifax_module *self, int command, ...);

/* Establish a signal chain from src-module to dst-module */
void ifax_connect(struct ifax_module *src, struct ifax_module *dst);

#endif
