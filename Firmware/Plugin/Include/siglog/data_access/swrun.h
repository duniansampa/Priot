/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#ifndef NETSNMP_SWRUN_H
#define NETSNMP_SWRUN_H

#include "Container.h"


    /*-*****************************************************************
     *
     * Data structure for a row entry 
     */
    typedef struct hrSWRunTable_entry {
        Types_Index   oid_index;
        
        /*
         * Index values 
         */
        oid             hrSWRunIndex;
        
        /*
         * Column values 
         */
        char            hrSWRunName[ 64+1];  /* size per MIB + 1 */
        char            hrSWRunPath[128+1];  /* size per MIB + 1 */
        char            hrSWRunParameters[128+1]; /* size per MIB + 1 */
        u_char          hrSWRunName_len;
        u_char          hrSWRunPath_len;
        u_char          hrSWRunParameters_len;

        u_char          hrSWRunType;
        u_char          hrSWRunStatus;
        u_char          old_hrSWRunStatus;

        /*
         * Perf values
         */
        int32_t         hrSWRunPerfCPU;
        int32_t         hrSWRunPerfMem;
        
    } netsnmp_swrun_entry;

    /*
     * enums for column hrSWRunType
     */
#define HRSWRUNTYPE_UNKNOWN             1
#define HRSWRUNTYPE_OPERATINGSYSTEM     2
#define HRSWRUNTYPE_DEVICEDRIVER        3
#define HRSWRUNTYPE_APPLICATION         4

    /*
     * enums for column hrSWRunStatus
     */
#define HRSWRUNSTATUS_RUNNING           1
#define HRSWRUNSTATUS_RUNNABLE          2
#define HRSWRUNSTATUS_NOTRUNNABLE       3
#define HRSWRUNSTATUS_INVALID           4

    /*-*****************************************************************
     *
     * Prototypes
     */
    Container_Container *
    netsnmp_swrun_container_load(Container_Container *container, u_int flags );

    void netsnmp_swrun_container_free(Container_Container *container, u_int flags);
    void netsnmp_swrun_container_free_items(Container_Container * container);

    netsnmp_swrun_entry *
    netsnmp_swrun_entry_create(int32_t swIndex);

    void netsnmp_swrun_entry_free(netsnmp_swrun_entry *entry);

    int  swrun_count_processes( void );
    int  swrun_max_processes(   void );
    int  swrun_count_processes_by_name( char *name );

#define NETSNMP_SWRUN_NOFLAGS            0x00000000
#define NETSNMP_SWRUN_ALL_OR_NONE        0x00000001
#define NETSNMP_SWRUN_DONT_FREE_ITEMS    0x00000002


#endif /* NETSNMP_SWRUN_H */


