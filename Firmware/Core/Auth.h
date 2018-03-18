#ifndef AUTH_H
#define AUTH_H

#include "Generals.h"

u_char * Auth_comstrParse(u_char * data,
                  size_t * length,
                  u_char * psid, size_t * slen, long *version);

u_char * Auth_comstrBuild(u_char * data,
                  size_t * length,
                  u_char * psid,
                  size_t * slen, long *version, size_t messagelen);
#endif // AUTH_H
