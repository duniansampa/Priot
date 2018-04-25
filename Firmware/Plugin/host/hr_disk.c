/*
 *  Host Resources MIB - disk device group implementation - hr_disk.c
 *
 */
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

#include "hr_disk.h"
#include "AgentReadConfig.h"
#include "AgentRegistry.h"
#include "Impl.h"
#include "ReadConfig.h"
#include "Strlcpy.h"
#include "host_res.h"
#include <linux/hdreg.h>
#include <regex.h>
#include <sys/ioctl.h>

/*
 * define BLKGETSIZE from <linux/fs.h>:
 * Note: cannot include this file completely due to errors with redefinition
 * of structures (at least with older linux versions) --jsf
 */
#define BLKGETSIZE _IO( 0x12, 96 ) /* return device size */

#define HRD_MONOTONICALLY_INCREASING

/*************************************************************
 * constants for enums for the MIB node
 * hrDiskStorageAccess (INTEGER / ASN01_INTEGER)
 */
#define HRDISKSTORAGEACCESS_READWRITE 1
#define HRDISKSTORAGEACCESS_READONLY 2

/*************************************************************
 * constants for enums for the MIB node
 * hrDiskStorageMedia (INTEGER / ASN01_INTEGER)
 */
#define HRDISKSTORAGEMEDIA_OTHER 1
#define HRDISKSTORAGEMEDIA_UNKNOWN 2
#define HRDISKSTORAGEMEDIA_HARDDISK 3
#define HRDISKSTORAGEMEDIA_FLOPPYDISK 4
#define HRDISKSTORAGEMEDIA_OPTICALDISKROM 5
#define HRDISKSTORAGEMEDIA_OPTICALDISKWORM 6
#define HRDISKSTORAGEMEDIA_OPTICALDISKRW 7
#define HRDISKSTORAGEMEDIA_RAMDISK 8

/*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

void Init_HR_Disk( void );
int Get_Next_HR_Disk( void );
int Get_Next_HR_Disk_Partition( char*, size_t, int );
static void Add_HR_Disk_entry( const char*, int, int, int, int,
    const char*, int, int );
static void Save_HR_Disk_General( void );
static void Save_HR_Disk_Specific( void );
static int Query_Disk( int, const char* );
static int Is_It_Writeable( void );
static int What_Type_Disk( void );
static int Is_It_Removeable( void );
static const char* describe_disk( int );

int header_hrdisk( struct Variable_s*, oid*, size_t*, int,
    size_t*, WriteMethodFT** );

static int HRD_type_index;
static int HRD_index;
static char HRD_savedModel[ 40 ];
static long HRD_savedCapacity = 1044;
static int HRD_savedFlags;
static time_t HRD_history[ HRDEV_TYPE_MASK + 1 ];

static struct hd_driveid HRD_info;

static void parse_disk_config( const char*, char* );
static void free_disk_config( void );

static void Add_LVM_Disks( void );
static void Remove_LVM_Disks( void );

/*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

#define HRDISK_ACCESS 1
#define HRDISK_MEDIA 2
#define HRDISK_REMOVEABLE 3
#define HRDISK_CAPACITY 4

struct Variable4_s hrdisk_variables[] = {
    { HRDISK_ACCESS, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_hrdisk, 2, { 1, 1 } },
    { HRDISK_MEDIA, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_hrdisk, 2, { 1, 2 } },
    { HRDISK_REMOVEABLE, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_hrdisk, 2, { 1, 3 } },
    { HRDISK_CAPACITY, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_hrdisk, 2, { 1, 4 } }
};
oid hrdisk_variables_oid[] = { 1, 3, 6, 1, 2, 1, 25, 3, 6 };

void init_hr_disk( void )
{
    int i;

    init_device[ HRDEV_DISK ] = Init_HR_Disk;
    next_device[ HRDEV_DISK ] = Get_Next_HR_Disk;
    save_device[ HRDEV_DISK ] = Save_HR_Disk_General;
    dev_idx_inc[ HRDEV_DISK ] = 1;

    Add_HR_Disk_entry( "/dev/hd%c%d", -1, -1, 'a', 'l', "/dev/hd%c", 1, 15 );
    Add_HR_Disk_entry( "/dev/sd%c%d", -1, -1, 'a', 'p', "/dev/sd%c", 1, 15 );
    Add_HR_Disk_entry( "/dev/md%d", -1, -1, 0, 3, "/dev/md%d", 0, 0 );
    Add_HR_Disk_entry( "/dev/fd%d", -1, -1, 0, 1, "/dev/fd%d", 0, 0 );

    Add_LVM_Disks();

    device_descr[ HRDEV_DISK ] = describe_disk;
    HRD_savedModel[ 0 ] = '\0';
    HRD_savedCapacity = 0;

    for ( i = 0; i < HRDEV_TYPE_MASK; ++i )
        HRD_history[ i ] = -1;

    REGISTER_MIB( "host/hr_disk", hrdisk_variables, Variable4_s,
        hrdisk_variables_oid );

    AgentReadConfig_priotdRegisterConfigHandler( "ignoredisk", parse_disk_config,
        free_disk_config, "name" );
}

void shutdown_hr_disk( void )
{
    Remove_LVM_Disks();
}

#define ITEM_STRING 1
#define ITEM_SET 2
#define ITEM_STAR 3
#define ITEM_ANY 4

typedef unsigned char details_set[ 32 ];

typedef struct _conf_disk_item {
    int item_type; /* ITEM_STRING, ITEM_SET, ITEM_STAR, ITEM_ANY */
    void* item_details; /* content depends upon item_type */
    struct _conf_disk_item* item_next;
} conf_disk_item;

typedef struct _conf_disk_list {
    conf_disk_item* list_item;
    struct _conf_disk_list* list_next;
} conf_disk_list;
static conf_disk_list* conf_list = NULL;

static int match_disk_config( const char* );
static int match_disk_config_item( const char*, conf_disk_item* );

static void
parse_disk_config( const char* token, char* cptr )
{
    conf_disk_list* d_new = NULL;
    conf_disk_item* di_curr = NULL;
    details_set* d_set = NULL;
    char *name = NULL, *p = NULL, *d_str = NULL, c;
    unsigned int i, neg, c1, c2;
    char* st = NULL;

    name = strtok_r( cptr, " \t", &st );
    if ( !name ) {
        ReadConfig_configPerror( "Missing NAME parameter" );
        return;
    }
    d_new = ( conf_disk_list* )malloc( sizeof( conf_disk_list ) );
    if ( !d_new ) {
        ReadConfig_configPerror( "Out of memory" );
        return;
    }
    di_curr = ( conf_disk_item* )malloc( sizeof( conf_disk_item ) );
    if ( !di_curr ) {
        TOOLS_FREE( d_new );
        ReadConfig_configPerror( "Out of memory" );
        return;
    }
    d_new->list_item = di_curr;
    /* XXX: on error/return conditions we need to free the entire new
       list, not just the last node like this is doing! */
    for ( ;; ) {
        if ( *name == '?' ) {
            di_curr->item_type = ITEM_ANY;
            di_curr->item_details = ( void* )0;
            name++;
        } else if ( *name == '*' ) {
            di_curr->item_type = ITEM_STAR;
            di_curr->item_details = ( void* )0;
            name++;
        } else if ( *name == '[' ) {
            d_set = ( details_set* )calloc( sizeof( details_set ), 1 );
            if ( !d_set ) {
                ReadConfig_configPerror( "Out of memory" );
                TOOLS_FREE( d_new );
                TOOLS_FREE( di_curr );
                TOOLS_FREE( d_set );
                TOOLS_FREE( d_str );
                return;
            }
            name++;
            if ( *name == '^' || *name == '!' ) {
                neg = 1;
                name++;
            } else {
                neg = 0;
            }
            while ( *name && *name != ']' ) {
                c1 = ( ( unsigned int )*name++ ) & 0xff;
                if ( *name == '-' && *( name + 1 ) != ']' ) {
                    name++;
                    c2 = ( ( unsigned int )*name++ ) & 0xff;
                } else {
                    c2 = c1;
                }
                for ( i = c1; i <= c2; i++ )
                    ( *d_set )[ i / 8 ] |= ( unsigned char )( 1 << ( i % 8 ) );
            }
            if ( *name != ']' ) {
                ReadConfig_configPerror( "Syntax error in NAME: invalid set specified" );
                TOOLS_FREE( d_new );
                TOOLS_FREE( di_curr );
                TOOLS_FREE( d_set );
                TOOLS_FREE( d_str );
                return;
            }
            if ( neg ) {
                for ( i = 0; i < sizeof( details_set ); i++ )
                    ( *d_set )[ i ] = ( *d_set )[ i ] ^ ( unsigned char )0xff;
            }
            di_curr->item_type = ITEM_SET;
            di_curr->item_details = ( void* )d_set;
            name++;
        } else {
            for ( p = name;
                  *p != '\0' && *p != '?' && *p != '*' && *p != '['; p++ )
                ;
            c = *p;
            *p = '\0';
            d_str = ( char* )malloc( strlen( name ) + 1 );
            if ( !d_str ) {
                TOOLS_FREE( d_new );
                TOOLS_FREE( d_str );
                TOOLS_FREE( di_curr );
                TOOLS_FREE( d_set );
                ReadConfig_configPerror( "Out of memory" );
                return;
            }
            strcpy( d_str, name );
            *p = c;
            di_curr->item_type = ITEM_STRING;
            di_curr->item_details = ( void* )d_str;
            name = p;
        }
        if ( !*name ) {
            di_curr->item_next = ( conf_disk_item* )0;
            break;
        }
        di_curr->item_next = ( conf_disk_item* )malloc( sizeof( conf_disk_item ) );
        if ( !di_curr->item_next ) {
            TOOLS_FREE( di_curr->item_next );
            TOOLS_FREE( d_new );
            TOOLS_FREE( di_curr );
            TOOLS_FREE( d_set );
            TOOLS_FREE( d_str );
            ReadConfig_configPerror( "Out of memory" );
            return;
        }
        di_curr = di_curr->item_next;
    }
    d_new->list_next = conf_list;
    conf_list = d_new;
}

static void
free_disk_config( void )
{
    conf_disk_list *d_ptr = conf_list, *d_next;
    conf_disk_item *di_ptr, *di_next;

    while ( d_ptr ) {
        d_next = d_ptr->list_next;
        di_ptr = d_ptr->list_item;
        while ( di_ptr ) {
            di_next = di_ptr->item_next;
            if ( di_ptr->item_details )
                free( di_ptr->item_details );
            free( ( void* )di_ptr );
            di_ptr = di_next;
        }
        free( ( void* )d_ptr );
        d_ptr = d_next;
    }
    conf_list = ( conf_disk_list* )0;
}

static int
match_disk_config_item( const char* name, conf_disk_item* di_ptr )
{
    int result = 0;
    size_t len;
    details_set* d_set;
    unsigned int c;

    if ( di_ptr ) {
        switch ( di_ptr->item_type ) {
        case ITEM_STRING:
            len = strlen( ( const char* )di_ptr->item_details );
            if ( !strncmp( name, ( const char* )di_ptr->item_details, len ) )
                result = match_disk_config_item( name + len,
                    di_ptr->item_next );
            break;
        case ITEM_SET:
            if ( *name ) {
                d_set = ( details_set* )di_ptr->item_details;
                c = ( ( unsigned int )*name ) & 0xff;
                if ( ( *d_set )[ c / 8 ] & ( unsigned char )( 1 << ( c % 8 ) ) )
                    result = match_disk_config_item( name + 1,
                        di_ptr->item_next );
            }
            break;
        case ITEM_STAR:
            if ( di_ptr->item_next ) {
                for ( ; !result && *name; name++ )
                    result = match_disk_config_item( name,
                        di_ptr->item_next );
            } else {
                result = 1;
            }
            break;
        case ITEM_ANY:
            if ( *name )
                result = match_disk_config_item( name + 1,
                    di_ptr->item_next );
            break;
        }
    } else {
        if ( *name == '\0' )
            result = 1;
    }

    return result;
}

static int
match_disk_config( const char* name )
{
    conf_disk_list* d_ptr = conf_list;

    while ( d_ptr ) {
        if ( match_disk_config_item( name, d_ptr->list_item ) )
            return 1; /* match found in ignorelist */
        d_ptr = d_ptr->list_next;
    }

    /*
     * no match in ignorelist 
     */
    return 0;
}

/*
 * header_hrdisk(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 */

int header_hrdisk( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{
#define HRDISK_ENTRY_NAME_LENGTH 11
    oid newname[ ASN01_MAX_OID_LEN ];
    int disk_idx, LowIndex = -1;
    int result;

    DEBUG_MSGTL( ( "host/hr_disk", "var_hrdisk: " ) );
    DEBUG_MSGOID( ( "host/hr_disk", name, *length ) );
    DEBUG_MSG( ( "host/hr_disk", " %d\n", exact ) );

    memcpy( ( char* )newname, ( char* )vp->name,
        ( int )vp->namelen * sizeof( oid ) );
    /*
     * Find "next" disk entry 
     */

    Init_HR_Disk();
    for ( ;; ) {
        disk_idx = Get_Next_HR_Disk();
        DEBUG_MSGTL( ( "host/hr_disk", "... index %d\n", disk_idx ) );
        if ( disk_idx == -1 )
            break;
        newname[ HRDISK_ENTRY_NAME_LENGTH ] = disk_idx;
        result = Api_oidCompare( name, *length, newname,
            ( int )vp->namelen + 1 );
        if ( exact && ( result == 0 ) ) {
            LowIndex = disk_idx;
            Save_HR_Disk_Specific();
            break;
        }
        if ( ( !exact && ( result < 0 ) ) && ( LowIndex == -1 || disk_idx < LowIndex ) ) {
            LowIndex = disk_idx;
            Save_HR_Disk_Specific();
            break;
        }
    }

    if ( LowIndex == -1 ) {
        DEBUG_MSGTL( ( "host/hr_disk", "... index out of range\n" ) );
        return ( MATCH_FAILED );
    }

    newname[ HRDISK_ENTRY_NAME_LENGTH ] = LowIndex;
    memcpy( ( char* )name, ( char* )newname,
        ( ( int )vp->namelen + 1 ) * sizeof( oid ) );
    *length = vp->namelen + 1;
    *write_method = ( WriteMethodFT* )0;
    *var_len = sizeof( long ); /* default to 'long' results */

    DEBUG_MSGTL( ( "host/hr_disk", "... get disk stats " ) );
    DEBUG_MSGOID( ( "host/hr_disk", name, *length ) );
    DEBUG_MSG( ( "host/hr_disk", "\n" ) );

    return LowIndex;
}

/*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/

u_char*
var_hrdisk( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{
    int disk_idx;

    disk_idx = header_hrdisk( vp, name, length, exact, var_len, write_method );
    if ( disk_idx == MATCH_FAILED )
        return NULL;

    switch ( vp->magic ) {
    case HRDISK_ACCESS:
        vars_longReturn = Is_It_Writeable();
        return ( u_char* )&vars_longReturn;
    case HRDISK_MEDIA:
        vars_longReturn = What_Type_Disk();
        return ( u_char* )&vars_longReturn;
    case HRDISK_REMOVEABLE:
        vars_longReturn = Is_It_Removeable();
        return ( u_char* )&vars_longReturn;
    case HRDISK_CAPACITY:
        vars_longReturn = HRD_savedCapacity;
        return ( u_char* )&vars_longReturn;
    default:
        DEBUG_MSGTL( ( "snmpd", "unknown sub-id %d in var_hrdisk\n",
            vp->magic ) );
    }
    return NULL;
}

/*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/

#define MAX_NUMBER_DISK_TYPES 16 /* probably should be a variable */
#define MAX_DISKS_PER_TYPE 15 /* SCSI disks - not a hard limit */
#define HRDISK_TYPE_SHIFT 4 /* log2 (MAX_DISKS_PER_TYPE+1) */

typedef struct {
    const char* disk_devpart_string; /* printf() format disk part. name */
    short disk_controller; /* controller id or -1 if NA */
    short disk_device_first; /* first device id */
    short disk_device_last; /* last device id */
    const char* disk_devfull_string; /* printf() format full disk name */
    short disk_partition_first; /* first partition id */
    short disk_partition_last; /* last partition id */
} HRD_disk_t;

static HRD_disk_t disk_devices[ MAX_NUMBER_DISK_TYPES ];
static int HR_number_disk_types = 0;

static void
Add_HR_Disk_entry( const char* devpart_string,
    int first_ctl,
    int last_ctl,
    int first_dev,
    int last_dev,
    const char* devfull_string,
    int first_partn, int last_partn )
{
    int lodev, hidev, nbr_created = 0;

    while ( first_ctl <= last_ctl ) {
        for ( lodev = first_dev;
              lodev <= last_dev && MAX_NUMBER_DISK_TYPES > HR_number_disk_types;
              lodev += ( 1 + MAX_DISKS_PER_TYPE ), HR_number_disk_types++ ) {
            nbr_created++;
            /*
         * Split long runs of disks into separate "types"
         */
            hidev = lodev + MAX_DISKS_PER_TYPE;
            if ( last_dev < hidev )
                hidev = last_dev;
            disk_devices[ HR_number_disk_types ].disk_devpart_string = devpart_string;
            disk_devices[ HR_number_disk_types ].disk_controller = first_ctl;
            disk_devices[ HR_number_disk_types ].disk_device_first = lodev;
            disk_devices[ HR_number_disk_types ].disk_device_last = hidev;
            disk_devices[ HR_number_disk_types ].disk_devfull_string = devfull_string;
            disk_devices[ HR_number_disk_types ].disk_partition_first = first_partn;
            disk_devices[ HR_number_disk_types ].disk_partition_last = last_partn;
        }
        first_ctl++;
    }

    if ( nbr_created == 0 || MAX_NUMBER_DISK_TYPES < HR_number_disk_types ) {
        HR_number_disk_types = MAX_NUMBER_DISK_TYPES;
        DEBUG_MSGTL( ( "host/hr_disk",
            "WARNING! Add_HR_Disk_entry '%s' incomplete, %d created\n",
            devpart_string, nbr_created ) );
    }
}

void Init_HR_Disk( void )
{
    HRD_type_index = 0;
    HRD_index = -1;
    DEBUG_MSGTL( ( "host/hr_disk", "Init_Disk\n" ) );
}

int Get_Next_HR_Disk( void )
{
    char string[ PATH_MAX + 1 ];
    int fd, result;
    int iindex;
    int max_disks;
    time_t now;

    HRD_index++;
    time( &now );
    DEBUG_MSGTL( ( "host/hr_disk", "Next_Disk type %d of %d\n",
        HRD_type_index, HR_number_disk_types ) );
    while ( HRD_type_index < HR_number_disk_types ) {
        max_disks = disk_devices[ HRD_type_index ].disk_device_last - disk_devices[ HRD_type_index ].disk_device_first + 1;
        DEBUG_MSGTL( ( "host/hr_disk", "Next_Disk max %d of type %d\n",
            max_disks, HRD_type_index ) );

        while ( HRD_index < max_disks ) {
            iindex = ( HRD_type_index << HRDISK_TYPE_SHIFT ) + HRD_index;

            /*
             * Check to see whether this device
             *   has been probed for 'recently'
             *   and skip if so.
             * This has a *major* impact on run
             *   times (by a factor of 10!)
             */
            if ( ( HRD_history[ iindex ] > 0 ) && ( ( now - HRD_history[ iindex ] ) < 60 ) ) {
                HRD_index++;
                continue;
            }

            /*
             * Construct the full device name in "string" 
             */
            if ( disk_devices[ HRD_type_index ].disk_controller != -1 ) {
                snprintf( string, sizeof( string ) - 1,
                    disk_devices[ HRD_type_index ].disk_devfull_string,
                    disk_devices[ HRD_type_index ].disk_controller,
                    disk_devices[ HRD_type_index ].disk_device_first + HRD_index );
            } else if ( disk_devices[ HRD_type_index ].disk_device_first == disk_devices[ HRD_type_index ].disk_device_last ) {
                /* exact device name */
                snprintf( string, sizeof( string ) - 1, "%s", disk_devices[ HRD_type_index ].disk_devfull_string );
            } else {
                snprintf( string, sizeof( string ) - 1,
                    disk_devices[ HRD_type_index ].disk_devfull_string,
                    disk_devices[ HRD_type_index ].disk_device_first + HRD_index );
            }
            string[ sizeof( string ) - 1 ] = 0;

            DEBUG_MSGTL( ( "host/hr_disk", "Get_Next_HR_Disk: %s (%d/%d)\n",
                string, HRD_type_index, HRD_index ) );

            if ( HRD_history[ iindex ] == -1 ) {
                /*
                 * check whether this device is in the "ignoredisk" list in
                 * the config file. if yes this device will be marked as
                 * invalid for the future, i.e. it won't ever be checked
                 * again.
                 */
                if ( match_disk_config( string ) ) {
                    /*
                     * device name matches entry in ignoredisk list 
                     */
                    DEBUG_MSGTL( ( "host/hr_disk",
                        "Get_Next_HR_Disk: %s ignored\n", string ) );
                    HRD_history[ iindex ] = LONG_MAX;
                    HRD_index++;
                    continue;
                }
            }

            /*
             * use O_NDELAY to avoid CDROM spin-up and media detection
             * * (too slow) --okir 
             */
            /*
             * at least with HP-UX 11.0 this doesn't seem to work properly
             * * when accessing an empty CDROM device --jsf 
             */
            fd = open( string, O_RDONLY | O_NDELAY );

            if ( fd != -1 ) {
                result = Query_Disk( fd, string );
                close( fd );
                if ( result != -1 ) {
                    HRD_history[ iindex ] = 0;
                    return ( ( HRDEV_DISK << HRDEV_TYPE_SHIFT ) + iindex );
                }
                DEBUG_MSGTL( ( "host/hr_disk",
                    "Get_Next_HR_Disk: can't query %s\n", string ) );
            } else {
                DEBUG_MSGTL( ( "host/hr_disk",
                    "Get_Next_HR_Disk: can't open %s\n", string ) );
            }
            HRD_history[ iindex ] = now;
            HRD_index++;
        }
        HRD_type_index++;
        HRD_index = 0;
    }
    HRD_index = -1;
    return -1;
}

int Get_Next_HR_Disk_Partition( char* string, size_t str_len, int HRP_index )
{
    DEBUG_MSGTL( ( "host/hr_disk", "Next_Partition type %d/%d:%d\n",
        HRD_type_index, HRD_index, HRP_index ) );

    /*
     * no more partition names => return -1 
     */
    if ( disk_devices[ HRD_type_index ].disk_partition_last - disk_devices[ HRD_type_index ].disk_partition_first + 1
        <= HRP_index ) {
        return -1;
    }

    /*
     * Construct the partition name in "string" 
     */
    if ( disk_devices[ HRD_type_index ].disk_controller != -1 ) {
        snprintf( string, str_len - 1,
            disk_devices[ HRD_type_index ].disk_devpart_string,
            disk_devices[ HRD_type_index ].disk_controller,
            disk_devices[ HRD_type_index ].disk_device_first + HRD_index,
            disk_devices[ HRD_type_index ].disk_partition_first + HRP_index );
    } else {
        snprintf( string, str_len - 1,
            disk_devices[ HRD_type_index ].disk_devpart_string,
            disk_devices[ HRD_type_index ].disk_device_first + HRD_index,
            disk_devices[ HRD_type_index ].disk_partition_first + HRP_index );
    }
    string[ str_len - 1 ] = 0;

    DEBUG_MSGTL( ( "host/hr_disk",
        "Get_Next_HR_Disk_Partition: %s (%d/%d:%d)\n", string,
        HRD_type_index, HRD_index, HRP_index ) );

    return 0;
}

static void
Save_HR_Disk_Specific( void )
{

    HRD_savedCapacity = HRD_info.lba_capacity / 2;
    HRD_savedFlags = HRD_info.config;
}

static void
Save_HR_Disk_General( void )
{
    Strlcpy_strlcpy( HRD_savedModel, ( const char* )HRD_info.model,
        sizeof( HRD_savedModel ) );
}

static const char*
describe_disk( int idx )
{
    if ( HRD_savedModel[ 0 ] == '\0' )
        return ( "some sort of disk" );
    else
        return ( HRD_savedModel );
}

static int
Query_Disk( int fd, const char* devfull )
{
    int result = -1;

    if ( HRD_type_index == 0 ) /* IDE hard disk */
        result = ioctl( fd, HDIO_GET_IDENTITY, &HRD_info );
    else if ( HRD_type_index != 3 ) { /* SCSI hard disk, md and LVM devices */
        long h;
        result = ioctl( fd, BLKGETSIZE, &h );
        if ( result != -1 && HRD_type_index == 2 && h == 0L )
            result = -1; /* ignore empty md devices */
        if ( result != -1 ) {
            HRD_info.lba_capacity = h;
            if ( HRD_type_index == 1 )
                snprintf( ( char* )HRD_info.model, sizeof( HRD_info.model ) - 1,
                    "SCSI disk (%s)", devfull );
            else if ( HRD_type_index >= 4 )
                snprintf( ( char* )HRD_info.model, sizeof( HRD_info.model ) - 1,
                    "LVM volume (%s)", devfull + strlen( "/dev/mapper/" ) );
            else
                snprintf( ( char* )HRD_info.model, sizeof( HRD_info.model ) - 1,
                    "RAID disk (%s)", devfull );
            HRD_info.model[ sizeof( HRD_info.model ) - 1 ] = 0;
            HRD_info.config = 0;
        }
    }

    return ( result );
}

static int
Is_It_Writeable( void )
{
    return ( 1 ); /* read-write */
}

static int
What_Type_Disk( void )
{
    return ( 2 ); /* Unknown */
}

static int
Is_It_Removeable( void )
{

    if ( HRD_savedFlags & 0x80 )
        return ( 1 ); /* true */

    return ( 2 ); /* false */
}

static char* lvm_device_names[ MAX_NUMBER_DISK_TYPES ];
static int lvm_device_count;

static void
Add_LVM_Disks( void )
{
    /*
     * LVM devices are harder because their name can be almost anything (see 
     * regexp below). Each logical volume is interpreted as its own device with
     * one partition, even if two logical volumes share common volume group. 
     */
    regex_t lvol;
    int res;
    DIR* dir;
    struct dirent* d;

    res = regcomp( &lvol, "[0-9a-zA-Z+_\\.-]+-[0-9a-zA-Z+_\\.-]+",
        REG_EXTENDED | REG_NOSUB );
    if ( res != 0 ) {
        char error[ 200 ];
        regerror( res, &lvol, error, sizeof( error ) - 1 );
        DEBUG_MSGTL( ( "host/hr_disk",
            "Add_LVM_Disks: cannot compile regexp: %s", error ) );
        return;
    }

    dir = opendir( "/dev/mapper/" );
    if ( dir == NULL ) {
        DEBUG_MSGTL( ( "host/hr_disk",
            "Add_LVM_Disks: cannot open /dev/mapper" ) );
        regfree( &lvol );
        return;
    }

    while ( ( d = readdir( dir ) ) != NULL ) {
        res = regexec( &lvol, d->d_name, 0, NULL, 0 );
        if ( res == 0 ) {
            char* path = ( char* )malloc( PATH_MAX + 1 );
            if ( path == NULL ) {
                DEBUG_MSGTL( ( "host/hr_disk",
                    "Add_LVM_Disks: cannot allocate memory for device %s",
                    d->d_name ) );
                break;
            }
            snprintf( path, PATH_MAX - 1, "/dev/mapper/%s", d->d_name );
            Add_HR_Disk_entry( path, -1, -1, 0, 0, path, 0, 0 );

            /*
             * store the device name so we can free it in Remove_LVM_Disks 
             */
            lvm_device_names[ lvm_device_count ] = path;
            ++lvm_device_count;
            if ( lvm_device_count >= MAX_NUMBER_DISK_TYPES ) {
                DEBUG_MSGTL( ( "host/hr_disk",
                    "Add_LVM_Disks: maximum count of LVM devices reached" ) );
                break;
            }
        }
    }
    closedir( dir );
    regfree( &lvol );
}

static void
Remove_LVM_Disks( void )
{
    /*
     * just free the device names allocated in add_lvm_disks 
     */
    int i;
    for ( i = 0; i < lvm_device_count; i++ ) {
        free( lvm_device_names[ i ] );
        lvm_device_names[ i ] = NULL;
    }
    lvm_device_count = 0;
}
