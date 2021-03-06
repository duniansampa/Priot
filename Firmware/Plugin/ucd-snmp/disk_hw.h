/*
 *  Template MIB group interface - disk.h
 *
 */
#ifndef _MIBGROUP_DISK_HW_H
#define _MIBGROUP_DISK_HW_H

#include "mibdefs.h"
#include "Vars.h"

void            init_disk_hw(void);

extern FindVarMethodFT var_extensible_disk;


#define DISKDEVICE 3
#define DISKMINIMUM 4
#define DISKMINPERCENT 5
#define DISKTOTAL 6
#define DISKAVAIL 7
#define DISKUSED 8
#define DISKPERCENT 9
#define DISKPERCENTNODE 10
#define DISKTOTALLOW 11
#define DISKTOTALHIGH 12
#define DISKAVAILLOW 13
#define DISKAVAILHIGH 14
#define DISKUSEDLOW 15
#define DISKUSEDHIGH 16

#endif                          /* _MIBGROUP_DISK_HW_H */
