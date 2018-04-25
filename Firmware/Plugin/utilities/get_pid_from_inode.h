/*
 * util_funcs/get_pid_from_inode.h:  utilitiy function to retrieve the pid
 * that controls a given inode on linux.
 */
#ifndef NETSNMP_MIBGROUP_UTIL_FUNCS_GET_PID_FROM_INODE_H
#define NETSNMP_MIBGROUP_UTIL_FUNCS_GET_PID_FROM_INODE_H

#define _LARGEFILE64_SOURCE 1


#include "Types.h"

void netsnmp_get_pid_from_inode_init(void);
pid_t netsnmp_get_pid_from_inode(ino64_t);

#endif /* NETSNMP_MIBGROUP_UTIL_FUNCS_GET_PID_FROM_INODE_H */
