/*
 * auto_nlist.h
 */
#ifndef AUTO_NLIST_H
#define AUTO_NLIST_H

int             auto_nlist_noop(void);

#	define auto_nlist(x,y,z) auto_nlist_noop()
#	define auto_nlist_value(z) auto_nlist_noop()
#	define KNLookup(w,x,y,z) auto_nlist_noop()

#endif
