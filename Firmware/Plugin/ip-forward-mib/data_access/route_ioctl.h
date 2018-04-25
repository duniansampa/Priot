/*
 * internal header, not for distribution
 */

#ifndef ROUTE_IOCTL_H
#define ROUTE_IOCTL_H

#include "siglog/data_access/route.h"

int _netsnmp_ioctl_route_set_v4(netsnmp_route_entry * entry);
int _netsnmp_ioctl_route_remove_v4(netsnmp_route_entry * entry);
int _netsnmp_ioctl_route_delete_v4(netsnmp_route_entry * entry);

#endif //ROUTE_IOCTL_H

