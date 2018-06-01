#include "System.h"
#include "ReadConfig.h"
#include "System/String.h"
#include "System/Util/Assert.h"
#include "System/Util/Logger.h"
#include "System/Util/Trace.h"
#include <arpa/inet.h>
#include <grp.h>
#include <net/if.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>

static void _System_daemonPrep( int stderr_log )
{
    /* Avoid keeping any directory in use. */
    chdir( "/" );

    if ( stderr_log )
        return;

    /*
     * Close inherited file descriptors to avoid
     * keeping unnecessary references.
     */
    close( 0 );
    close( 1 );
    close( 2 );

    /*
     * Redirect std{in,out,err} to /dev/null, just in case.
     */
    open( "/dev/null", O_RDWR );
    dup( 0 );
    dup( 0 );
}

int System_daemonize( int quitImmediately, int stderrLog )
{
    int i = 0;
    DEBUG_MSGT( ( "daemonize", "deamonizing...\n" ) );

    /*
     * Fork to return control to the invoking process and to
     * guarantee that we aren't a process group leader.
     */
    i = fork();
    if ( i != 0 ) {
        /* Parent. */
        DEBUG_MSGT( ( "daemonize", "first fork returned %d.\n", i ) );
        if ( i == -1 ) {
            Logger_log( LOGGER_PRIORITY_ERR, "first fork failed (errno %d) in "
                                             "System_daemonize()\n",
                errno );
            return -1;
        }
        if ( quitImmediately ) {
            DEBUG_MSGT( ( "daemonize", "parent exiting\n" ) );
            exit( 0 );
        }
    } else {
        /* Child. */

        /* creates a new session if the calling process is not a process group leader.
         * Become a process/session group leader. */
        setsid();

        /*
         * Fork to let the process/session group leader exit.
         */
        if ( ( i = fork() ) != 0 ) {
            DEBUG_MSGT( ( "daemonize", "second fork returned %d.\n", i ) );
            if ( i == -1 ) {
                Logger_log( LOGGER_PRIORITY_ERR, "second fork failed (errno %d) in System_daemonize()\n", errno );
            }
            /* Parent. */
            exit( 0 );
        } else {
            /* Child. */

            DEBUG_MSGT( ( "daemonize", "child continuing\n" ) );

            _System_daemonPrep( stderrLog );
        }
    }

    return i;
}

in_addr_t System_getMyAddress( void )
{
    int sd, i, lastlen = 0;
    struct ifconf ifc;
    struct ifreq* ifrp = NULL;
    in_addr_t addr;
    char* buf = NULL;

    if ( ( sd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
        return 0;
    }

    /*
     * Cope with lots of interfaces and brokenness of ioctl SIOCGIFCONF on
     * some platforms; see W. R. Stevens, ``Unix Network Programming Volume
     * I'', p.435.
     */

    for ( i = 8;; i += 8 ) {
        buf = ( char* )Memory_calloc( i, sizeof( struct ifreq ) );
        if ( buf == NULL ) {
            close( sd );
            return 0;
        }
        ifc.ifc_len = i * sizeof( struct ifreq );
        ifc.ifc_buf = ( caddr_t )buf;

        if ( ioctl( sd, SIOCGIFCONF, ( char* )&ifc ) < 0 ) {
            if ( errno != EINVAL || lastlen != 0 ) {
                /*
                 * Something has gone genuinely wrong.
                 */
                free( buf );
                close( sd );
                return 0;
            }
            /*
             * Otherwise, it could just be that the buffer is too small.
             */
        } else {
            if ( ifc.ifc_len == lastlen ) {
                /*
                 * The length is the same as the last time; we're done.
                 */
                break;
            }
            lastlen = ifc.ifc_len;
        }
        free( buf );
    }

    for ( ifrp = ifc.ifc_req; ( char* )ifrp < ( char* )ifc.ifc_req + ifc.ifc_len; ifrp++ ) {

        if ( ifrp->ifr_addr.sa_family != AF_INET ) {
            continue;
        }
        addr = ( ( struct sockaddr_in* )&( ifrp->ifr_addr ) )->sin_addr.s_addr;

        if ( ioctl( sd, SIOCGIFFLAGS, ( char* )ifrp ) < 0 ) {
            continue;
        }
        if ( ( ifrp->ifr_flags & IFF_UP ) && ( ifrp->ifr_flags & IFF_RUNNING )
            /* IFF_RUNNING */
            && !( ifrp->ifr_flags & IFF_LOOPBACK )
            && addr != INADDR_LOOPBACK ) {
            /*
              * I *really* don't understand why this is necessary.  Perhaps for
              * some broken platform?  Leave it for now.  JBPN
              */
            if ( ioctl( sd, SIOCGIFADDR, ( char* )ifrp ) < 0 ) {
                continue;
            }
            addr = ( ( struct sockaddr_in* )&( ifrp->ifr_addr ) )->sin_addr.s_addr;

            free( buf );
            close( sd );
            return addr;
        }
    }
    free( buf );
    close( sd );
    return 0;
}

long System_getUpTime( void )
{

    FILE* in = fopen( "/proc/uptime", "r" );
    long uptim = 0, a, b;
    if ( in ) {
        if ( 2 == fscanf( in, "%ld.%ld", &a, &b ) )
            uptim = a * 100 + b;
        fclose( in );
    }
    return uptim;
}

int System_getHostByNameV4( const char* name, in_addr_t* addrOut )
{

    struct addrinfo* addrs = NULL;
    struct addrinfo hint;
    int err;

    memset( &hint, 0, sizeof hint );
    hint.ai_flags = 0;
    hint.ai_family = PF_INET;
    hint.ai_socktype = SOCK_DGRAM;
    hint.ai_protocol = 0;

    err = System_getAddrInfo( name, NULL, &hint, &addrs );
    if ( err != 0 ) {
        return -1;
    }

    if ( addrs != NULL ) {
        memcpy( addrOut,
            &( ( struct sockaddr_in* )addrs->ai_addr )->sin_addr,
            sizeof( in_addr_t ) );
        freeaddrinfo( addrs );
    } else {
        DEBUG_MSGTL( ( "System_getHostByNameV4", "Failed to resolve IPv4 hostname\n" ) );
    }
    return 0;
}

int System_getAddrInfo( const char* hostname, const char* service,
    const struct addrinfo* hints, struct addrinfo** res )
{
    struct addrinfo* addrs = NULL;
    struct addrinfo hint;
    int err;

    DEBUG_MSGTL( ( "dns:getaddrinfo", "looking up " ) );
    if ( hostname )
        DEBUG_MSG( ( "dns:getaddrinfo", "\"%s\"", hostname ) );
    else
        DEBUG_MSG( ( "dns:getaddrinfo", "<NULL>" ) );

    if ( service )
        DEBUG_MSG( ( "dns:getaddrinfo", ":\"%s\"", service ) );

    if ( hints )
        DEBUG_MSG( ( "dns:getaddrinfo", " with hint ({ ... })" ) );
    else
        DEBUG_MSG( ( "dns:getaddrinfo", " with no hint" ) );

    DEBUG_MSG( ( "dns:getaddrinfo", "\n" ) );

    if ( NULL == hints ) {
        memset( &hint, 0, sizeof hint );
        hint.ai_flags = 0;
        hint.ai_family = PF_INET;
        hint.ai_socktype = SOCK_DGRAM;
        hint.ai_protocol = 0;
        hints = &hint;
    } else {
        memcpy( &hint, hints, sizeof hint );
    }

    err = getaddrinfo( hostname, NULL, &hint, &addrs );

    *res = addrs;
    if ( ( 0 == err ) && addrs && addrs->ai_addr ) {
        DEBUG_MSGTL( ( "dns:getaddrinfo", "answer { AF_INET, %s:%hu }\n",
            inet_ntoa( ( ( struct sockaddr_in* )addrs->ai_addr )->sin_addr ),
            ntohs( ( ( struct sockaddr_in* )addrs->ai_addr )->sin_port ) ) );
    }
    return err;
}

struct hostent* System_getHostByName( const char* hostname )
{

    struct hostent* hp = NULL;

    if ( NULL == hostname )
        return NULL;

    DEBUG_MSGTL( ( "dns:gethostbyname", "looking up %s\n", hostname ) );

    hp = gethostbyname( hostname );

    if ( hp == NULL ) {
        DEBUG_MSGTL( ( "dns:gethostbyname",
            "couldn't resolve %s\n", hostname ) );
    } else if ( hp->h_addrtype != AF_INET ) {
        DEBUG_MSGTL( ( "dns:gethostbyname",
            "warning: response for %s not AF_INET!\n", hostname ) );
    } else {
        DEBUG_MSGTL( ( "dns:gethostbyname",
            "%s resolved okay\n", hostname ) );
    }
    return hp;
}

struct hostent* System_getHostByAddr( const void* address, socklen_t length, int type )
{
    struct hostent* hp = NULL;
    char buf[ 64 ];

    DEBUG_MSGTL( ( "dns:gethostbyaddr", "resolving %s\n", inet_ntop( type, address, buf, sizeof( buf ) ) ) );

    hp = gethostbyaddr( address, length, type );

    if ( hp == NULL ) {
        DEBUG_MSGTL( ( "dns:gethostbyaddr", "couldn't resolve addr\n" ) );
    } else if ( hp->h_addrtype != AF_INET ) {
        DEBUG_MSGTL( ( "dns:gethostbyaddr",
            "warning: response for addr not AF_INET!\n" ) );
    } else {
        DEBUG_MSGTL( ( "dns:gethostbyaddr", "addr resolved okay\n" ) );
    }
    return hp;
}

const char*
System_makeTemporaryFile( void )
{
    static char name[ PATH_MAX ];
    int fd = -1;

    String_copyTruncate( name, ReadConfig_getTempFilePattern(), sizeof( name ) );

    mode_t oldmask = umask( ~( S_IRUSR | S_IWUSR ) );
    Assert_assert( oldmask != ( mode_t )( -1 ) );
    fd = mkstemp( name );
    umask( oldmask );

    if ( fd >= 0 ) {
        close( fd );
        DEBUG_MSGTL( ( "System_makeTemporaryFile", "temp file created: %s\n",
            name ) );
        return name;
    }
    Logger_log( LOGGER_PRIORITY_ERR, "System_makeTemporaryFile: error creating file %s\n",
        name );
    return NULL;
}

int System_isOsPrefixMatch( const char* kernelName, const char* kernelReleasePrefix )
{
    static int printOSonce = 1;
    struct utsname utsbuf;
    if ( 0 > uname( &utsbuf ) )
        return -1;

    if ( printOSonce ) {
        printOSonce = 0;
        /* show the four elements that the kernel can be sure of */
        DEBUG_MSGT( ( "daemonize", "sysname '%s',\nrelease '%s',\nversion '%s',\nmachine '%s'\n",
            utsbuf.sysname, utsbuf.release, utsbuf.version, utsbuf.machine ) );
    }
    if ( 0 != strcasecmp( utsbuf.sysname, kernelName ) )
        return -1;

    /* Required to match only the leading characters */
    return strncasecmp( utsbuf.release, kernelReleasePrefix, strlen( kernelReleasePrefix ) );
}

int System_getUserUidByUserName( const char* userNameOrNumber )
{
    int uid;
    struct passwd* pwd;

    uid = atoi( userNameOrNumber );

    if ( uid == 0 ) {
        pwd = getpwnam( userNameOrNumber );
        uid = pwd ? pwd->pw_uid : 0;
        endpwent();
        if ( uid == 0 )
            Logger_log( LOGGER_PRIORITY_WARNING, "Can't identify user (%s).\n", userNameOrNumber );
    }
    return uid;
}

int System_getUserGidByUserName( const char* groupNameOrNumber )
{
    int gid;

    gid = atoi( groupNameOrNumber );

    if ( gid == 0 ) {
        struct group* grp;

        grp = getgrnam( groupNameOrNumber );
        gid = grp ? grp->gr_gid : 0;
        endgrent();
        if ( gid == 0 )
            Logger_log( LOGGER_PRIORITY_WARNING, "Can't identify group (%s).\n", groupNameOrNumber );
    }
    return gid;
}

char* System_getEnvVariable( const char* name )
{
    return ( getenv( name ) );
}
