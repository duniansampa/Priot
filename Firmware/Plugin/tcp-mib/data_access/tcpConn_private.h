#ifndef TCP_CONN_PRIAVTE_H
#define TCP_CONN_PRIAVTE_H

#include "System/Containers/Container.h"
#include "siglog/data_access/tcpConn.h"

int netsnmp_arch_tcpconn_container_load(Container_Container *, u_int);
int netsnmp_arch_tcpconn_entry_init(netsnmp_tcpconn_entry *);
void netsnmp_arch_tcpconn_entry_cleanup(netsnmp_tcpconn_entry *);
int netsnmp_arch_tcpconn_entry_delete(netsnmp_tcpconn_entry *);
int netsnmp_arch_tcpconn_entry_copy(netsnmp_tcpconn_entry *,
                                    netsnmp_tcpconn_entry *);

#endif //TCP_CONN_PRIAVTE_H
