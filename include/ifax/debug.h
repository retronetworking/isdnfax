/*
 * Debugging messages for ISDNfax.
 * (c) 1998 Andreas Beck	<becka@ggi-project.org>
 */

enum debuglevel {
	DEBUG_ALL,
	DEBUG_JUNK,
	DEBUG_DEBUG,
	DEBUG_INFO,
	DEBUG_NOTICE,
	DEBUG_WARNING,
	DEBUG_ERROR,
	DEBUG_SEVERE,
	DEBUG_FATAL,
	DEBUG_LAST
};

int  ifax_dprintf(enum debuglevel severity,char *format,...);

void ifax_debugsetlevel(enum debuglevel);
