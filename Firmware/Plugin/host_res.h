/*
 *  Host Resources
 *      Device index manipulation data
 */

#ifndef HOST_RES_H
#define HOST_RES_H
/*
 * #include "snmp_vars.linux.h" 
 */

#include "Types.h"

                /*
                 * Deliberately set to the same values as hrDeviceTypes 
                 */
#define	HRDEV_OTHER	1
#define	HRDEV_UNKNOWN	2
#define	HRDEV_PROC	3
#define	HRDEV_NETWORK	4
#define	HRDEV_PRINTER	5
#define	HRDEV_DISK	6
#define	HRDEV_VIDEO	10
#define	HRDEV_AUDIO	11
#define	HRDEV_COPROC	12
#define	HRDEV_KEYBOARD	13
#define	HRDEV_MODEM	14
#define	HRDEV_PARALLEL	15
#define	HRDEV_POINTER	16
#define	HRDEV_SERIAL	17
#define	HRDEV_TAPE	18
#define	HRDEV_CLOCK	19
#define	HRDEV_VMEM	20
#define	HRDEV_NVMEM	21

#define	HRDEV_TYPE_MAX	22      /* one greater than largest device type */
#define	HRDEV_TYPE_SHIFT  16
#define	HRDEV_TYPE_MASK 0xffff

typedef void    (*PFV) (void);
typedef int     (*PFI) (int);
typedef int     (*PFIV) (void);
typedef const char *(*PFS) (int);
typedef oid    *(*PFO) (int, size_t *);

extern PFV      init_device[];  /* Routines for stepping through devices */
extern PFIV     next_device[];
extern PFV      save_device[];
extern int      dev_idx_inc[];  /* Flag - are indices returned in strictly
                                 * increasing order */

extern PFS      device_descr[]; /* Return data for a particular device */
extern PFO      device_prodid[];
extern PFI      device_status[];
extern PFI      device_errors[];


#endif //HOST_RES_H
