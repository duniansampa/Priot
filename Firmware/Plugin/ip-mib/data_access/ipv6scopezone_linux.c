/*
 *  Interface MIB architecture support
 *
 * $Id: ipv6scopezone_linux.c 14170 2007-04-29 02:22:12Z varun_c $
 */

#include "siglog/data_access/scopezone.h"

/*
 *
 * @retval  0 success
 * @retval -1 no container specified
 * @retval -2 could not open file
 * @retval -3 could not create entry (probably malloc)
 * @retval -4 file format error
 */
int netsnmp_access_scopezone_container_arch_load( Container_Container* container,
    u_int load_flags )
{
    int rc1 = 0;

    if ( rc1 > 0 )
        rc1 = 0;
    return rc1;
}
