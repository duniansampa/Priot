
#include "siglog/agent/hardware/fsys.h"
#include "System/Util/Debug.h"
#include "DefaultStore.h"
#include "DsAgent.h"
#include "System/Util/Logger.h"
#include "System/String.h"
#include <mntent.h>
#include <sys/statfs.h>

#undef _NETSNMP_GETMNTENT_TWO_ARGS

/*
     * Handle naming differences between getmntent() APIs
     */

/* One-argument form (everything else?) */
#define NSFS_MNTENT struct mntent
#define NSFS_PATH mnt_dir
#define NSFS_DEV mnt_fsname
#define NSFS_TYPE mnt_type

#define NSFS_STATFS statfs
#define NSFS_SIZE f_bsize

int _fsys_remote( char* device, int type )
{
    if ( ( type == NETSNMP_FS_TYPE_NFS ) || ( type == NETSNMP_FS_TYPE_AFS ) )
        return 1;
    else
        return 0;
}

int _fsys_type( char* tpname )
{
    DEBUG_MSGTL( ( "fsys:type", "Classifying %s\n", tpname ) );

    if ( !tpname || *tpname == '\0' )
        return NETSNMP_FS_TYPE_UNKNOWN;

#include "mnttypes.h"

    else if ( !strcmp( tpname, MNTTYPE_FFS ) )
        return NETSNMP_FS_TYPE_BERKELEY;
    else if ( !strcmp( tpname, MNTTYPE_UFS ) )
        return _NETSNMP_FS_TYPE_UFS; /* either N_FS_TYPE_BERKELEY or N_FS_TYPE_SYSV */
    else if ( !strcmp( tpname, MNTTYPE_SYSV ) )
        return NETSNMP_FS_TYPE_SYSV;
    else if ( !strcmp( tpname, MNTTYPE_PC ) || !strcmp( tpname, MNTTYPE_MSDOS ) )
        return NETSNMP_FS_TYPE_FAT;
    else if ( !strcmp( tpname, MNTTYPE_HFS ) )
        return NETSNMP_FS_TYPE_HFS;
    else if ( !strcmp( tpname, MNTTYPE_MFS ) )
        return NETSNMP_FS_TYPE_MFS;
    else if ( !strcmp( tpname, MNTTYPE_NTFS ) )
        return NETSNMP_FS_TYPE_NTFS;
    else if ( !strcmp( tpname, MNTTYPE_ISO9660 ) || !strcmp( tpname, MNTTYPE_CD9660 ) )
        return NETSNMP_FS_TYPE_ISO9660;
    else if ( !strcmp( tpname, MNTTYPE_CDFS ) )
        return _NETSNMP_FS_TYPE_CDFS; /* either N_FS_TYPE_ISO9660 or N_FS_TYPE_ROCKRIDGE */
    else if ( !strcmp( tpname, MNTTYPE_HSFS ) )
        return NETSNMP_FS_TYPE_ROCKRIDGE;
    else if ( !strcmp( tpname, MNTTYPE_NFS ) || !strcmp( tpname, MNTTYPE_NFS3 ) || !strcmp( tpname, MNTTYPE_NFS4 ) || !strcmp( tpname, MNTTYPE_CIFS ) || /* i.e. SMB - ?? */
        !strcmp( tpname, MNTTYPE_SMBFS ) /* ?? */ )
        return NETSNMP_FS_TYPE_NFS;
    else if ( !strcmp( tpname, MNTTYPE_NCPFS ) )
        return NETSNMP_FS_TYPE_NETWARE;
    else if ( !strcmp( tpname, MNTTYPE_AFS ) )
        return NETSNMP_FS_TYPE_AFS;
    else if ( !strcmp( tpname, MNTTYPE_EXT2 ) || !strcmp( tpname, MNTTYPE_EXT3 ) || !strcmp( tpname, MNTTYPE_EXT4 ) || !strcmp( tpname, MNTTYPE_EXT2FS ) || !strcmp( tpname, MNTTYPE_EXT3FS ) || !strcmp( tpname, MNTTYPE_EXT4FS ) )
        return NETSNMP_FS_TYPE_EXT2;
    else if ( !strcmp( tpname, MNTTYPE_FAT32 ) || !strcmp( tpname, MNTTYPE_VFAT ) )
        return NETSNMP_FS_TYPE_FAT32;

    /*
     *  The following code covers selected filesystems
     *    which are not covered by the HR-TYPES enumerations,
     *    but should still be monitored.
     *  These are all mapped into type "other"
     *
     *    (The systems listed are not fixed in stone,
     *     but are simply here to illustrate the principle!)
     */
    else if ( !strcmp( tpname, MNTTYPE_MVFS ) || !strcmp( tpname, MNTTYPE_TMPFS ) || !strcmp( tpname, MNTTYPE_GFS ) || !strcmp( tpname, MNTTYPE_GFS2 ) || !strcmp( tpname, MNTTYPE_XFS ) || !strcmp( tpname, MNTTYPE_JFS ) || !strcmp( tpname, MNTTYPE_VXFS ) || !strcmp( tpname, MNTTYPE_REISERFS ) || !strcmp( tpname, MNTTYPE_OCFS2 ) || !strcmp( tpname, MNTTYPE_CVFS ) || !strcmp( tpname, MNTTYPE_SIMFS ) || !strcmp( tpname, MNTTYPE_BTRFS ) || !strcmp( tpname, MNTTYPE_ZFS ) || !strcmp( tpname, MNTTYPE_ACFS ) || !strcmp( tpname, MNTTYPE_LOFS ) )
        return NETSNMP_FS_TYPE_OTHER;

    /*
     *  All other types are silently skipped
     */
    else
        return NETSNMP_FS_TYPE_IGNORE;
}

void netsnmp_fsys_arch_init( void )
{
    return;
}

void netsnmp_fsys_arch_load( void )
{
    FILE* fp = NULL;

    struct mntent* m;
    struct NSFS_STATFS stat_buf;
    netsnmp_fsys_info* entry;
    char tmpbuf[ 1024 ];

    /*
     * Retrieve information about the currently mounted filesystems...
     */
    fp = fopen( ETC_MNTTAB, "r" ); /* OR setmntent()?? */
    if ( !fp ) {
        snprintf( tmpbuf, sizeof( tmpbuf ), "Cannot open %s", ETC_MNTTAB );
        Logger_logPerror( tmpbuf );
        return;
    }

    /*
     * ... and insert this into the filesystem container.
     */
    while

        ( ( m = getmntent( fp ) ) != NULL ) {
        entry = netsnmp_fsys_by_path( m->NSFS_PATH, NETSNMP_FS_FIND_CREATE );
        if ( !entry ) {
            continue;
        }

        String_copyTruncate( entry->path, m->NSFS_PATH, sizeof( entry->path ) );
        String_copyTruncate( entry->device, m->NSFS_DEV, sizeof( entry->device ) );
        entry->type = _fsys_type( m->NSFS_TYPE );
        if ( !( entry->type & _NETSNMP_FS_TYPE_SKIP_BIT ) )
            entry->flags |= NETSNMP_FS_FLAG_ACTIVE;

        if ( _fsys_remote( entry->device, entry->type ) )
            entry->flags |= NETSNMP_FS_FLAG_REMOTE;
        if ( hasmntopt( m, "ro" ) )
            entry->flags |= NETSNMP_FS_FLAG_RONLY;
        /*
         *  The root device is presumably bootable.
         *  Other partitions probably aren't!
         *
         *  XXX - what about /boot ??
         */
        if ( ( entry->path[ 0 ] == '/' ) && ( entry->path[ 1 ] == '\0' ) )
            entry->flags |= NETSNMP_FS_FLAG_BOOTABLE;

        /*
         *  XXX - identify removeable disks
         */

        /*
         *  Optionally skip retrieving statistics for remote mounts
         */
        if ( ( entry->flags & NETSNMP_FS_FLAG_REMOTE ) && DefaultStore_getBoolean( DsStorage_APPLICATION_ID,
                                                              DsAgentBoolean_SKIPNFSINHOSTRESOURCES ) )
            continue;

        if ( NSFS_STATFS( entry->path, &stat_buf ) < 0 ) {
            snprintf( tmpbuf, sizeof( tmpbuf ), "Cannot statfs %s", entry->path );
            Logger_logPerror( tmpbuf );
            continue;
        }
        entry->units = stat_buf.NSFS_SIZE;
        entry->size = stat_buf.f_blocks;
        entry->used = ( stat_buf.f_blocks - stat_buf.f_bfree );
        /* entry->avail is currently unsigned, so protect against negative
         * values!
         * This should be changed to a signed field.
         */
        if ( stat_buf.f_bavail < 0 )
            entry->avail = 0;
        else
            entry->avail = stat_buf.f_bavail;
        entry->inums_total = stat_buf.f_files;
        entry->inums_avail = stat_buf.f_ffree;
        netsnmp_fsys_calculate32( entry );
    }
    fclose( fp );
}
