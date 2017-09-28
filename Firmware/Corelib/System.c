#include "System.h"


#include <net/if.h>
#include <sys/ioctl.h>
#include <bits/ioctls.h>
#include <pwd.h>
#include <grp.h>
#include "Debug.h"
#include <arpa/inet.h>

#include "Logger.h"
#include "Api.h"


/**
 * Compute res = a + b.
 *
 * @pre a and b must be normalized 'struct timeval' values.
 *
 * @note res may be the same variable as one of the operands. In other
 *   words, &a == &res || &b == &res may hold.
 */
#define SYSTEM_TIMERADD(a, b, res)                    \
{                                                    \
    (res)->tv_sec  = (a)->tv_sec  + (b)->tv_sec;     \
    (res)->tv_usec = (a)->tv_usec + (b)->tv_usec;    \
    if ((res)->tv_usec >= 1000000L) {                \
        (res)->tv_usec -= 1000000L;                  \
        (res)->tv_sec++;                             \
    }                                                \
}

/**
 * Compute res = a - b.
 *
 * @pre a and b must be normalized 'struct timeval' values.
 *
 * @note res may be the same variable as one of the operands. In other
 *   words, &a == &res || &b == &res may hold.
 */
#define SYSTEM_TIMERSUB(a, b, res)                               \
{                                                               \
    (res)->tv_sec  = (a)->tv_sec  - (b)->tv_sec - 1;            \
    (res)->tv_usec = (a)->tv_usec - (b)->tv_usec + 1000000L;    \
    if ((res)->tv_usec >= 1000000L) {                           \
        (res)->tv_usec -= 1000000L;                             \
        (res)->tv_sec++;                                        \
    }                                                           \
}

void System_daemonPrep(int stderr_log)
{
    /* Avoid keeping any directory in use. */
    chdir("/");

    if (stderr_log)
        return;

    /*
     * Close inherited file descriptors to avoid
     * keeping unnecessary references.
     */
    close(0);
    close(1);
    close(2);

    /*
     * Redirect std{in,out,err} to /dev/null, just in case.
     */
    open("/dev/null", O_RDWR);
    dup(0);
    dup(0);
}

/**
 * fork current process into the background.
 *
 * This function forks a process into the background, in order to
 * become a daemon process. It does a few things along the way:
 *
 * - becoming a process/session group leader, and  forking a second time so
 *   that process/session group leader can exit.
 *
 * - changing the working directory to /
 *
 * - closing stdin, stdout and stderr (unless stderr_log is set) and
 *   redirecting them to /dev/null
 *
 * @param quit_immediately : indicates if the parent process should
 *                           exit after a successful fork.
 * @param stderr_log       : indicates if stderr is being used for
 *                           logging and shouldn't be closed
 * @returns -1 : fork error
 *           0 : child process returning
 *          >0 : parent process returning. returned value is the child PID.
 */
int System_daemonize(int quit_immediately, int stderr_log)
{
    int i = 0;
    DEBUG_MSGT(("daemonize","deamonizing...\n"));

    /*
     * Fork to return control to the invoking process and to
     * guarantee that we aren't a process group leader.
     */
    i = fork();
    if (i != 0) {
        /* Parent. */
        DEBUG_MSGT(("daemonize","first fork returned %d.\n", i));
        if(i == -1) {
            //snmp_log(LOG_ERR,"first fork failed (errno %d) in "
            //                 "netsnmp_daemonize()\n", errno);
            return -1;
        }
        if (quit_immediately) {
            DEBUG_MSGT(("daemonize","parent exiting\n"));
            exit(0);
        }
    } else {
        /* Child. */

        /* Become a process/session group leader. */
        setsid();

        /*
         * Fork to let the process/session group leader exit.
         */
        if ((i = fork()) != 0) {
            DEBUG_MSGT(("daemonize","second fork returned %d.\n", i));
            if(i == -1) {
               //snmp_log(LOG_ERR,"second fork failed (errno %d) in netsnmp_daemonize()\n", errno);
            }
            /* Parent. */
            exit(0);
        }
        else {
            /* Child. */

            DEBUG_MSGT(("daemonize","child continuing\n"));

            System_daemonPrep(stderr_log);

        }
    }

    return i;
}


/*
 * XXX  What if we have multiple addresses?  Or no addresses for that matter?
 * XXX  Could it be computed once then cached?  Probably not worth it (not
 *                                                           used very often).
 */
in_addr_t System_getMyAddr(void)
{
    int             sd, i, lastlen = 0;
    struct ifconf   ifc;
    struct ifreq   *ifrp = NULL;
    in_addr_t       addr;
    char           *buf = NULL;

    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return 0;
    }

    /*
     * Cope with lots of interfaces and brokenness of ioctl SIOCGIFCONF on
     * some platforms; see W. R. Stevens, ``Unix Network Programming Volume
     * I'', p.435.
     */

    for (i = 8;; i += 8) {
        buf = (char *) calloc(i, sizeof(struct ifreq));
        if (buf == NULL) {
            close(sd);
            return 0;
        }
        ifc.ifc_len = i * sizeof(struct ifreq);
        ifc.ifc_buf = (caddr_t) buf;

        if (ioctl(sd, SIOCGIFCONF, (char *) &ifc) < 0) {
            if (errno != EINVAL || lastlen != 0) {
                /*
                 * Something has gone genuinely wrong.
                 */
                free(buf);
                close(sd);
                return 0;
            }
            /*
             * Otherwise, it could just be that the buffer is too small.
             */
        } else {
            if (ifc.ifc_len == lastlen) {
                /*
                 * The length is the same as the last time; we're done.
                 */
                break;
            }
            lastlen = ifc.ifc_len;
        }
        free(buf);
    }

    for (ifrp = ifc.ifc_req; (char *)ifrp < (char *)ifc.ifc_req + ifc.ifc_len; ifrp++) {

        if (ifrp->ifr_addr.sa_family != AF_INET) {
            continue;
        }
        addr = ((struct sockaddr_in *) &(ifrp->ifr_addr))->sin_addr.s_addr;

        if (ioctl(sd, SIOCGIFFLAGS, (char *) ifrp) < 0) {
            continue;
        }
        if ((ifrp->ifr_flags & IFF_UP) && (ifrp->ifr_flags & IFF_RUNNING)
                /* IFF_RUNNING */
                && !(ifrp->ifr_flags & IFF_LOOPBACK)
                && addr != INADDR_LOOPBACK) {
            /*
              * I *really* don't understand why this is necessary.  Perhaps for
              * some broken platform?  Leave it for now.  JBPN
              */
            if (ioctl(sd, SIOCGIFADDR, (char *) ifrp) < 0) {
                continue;
            }
            addr =
                    ((struct sockaddr_in *) &(ifrp->ifr_addr))->sin_addr.
                    s_addr;

            free(buf);
            close(sd);
            return addr;
        }
    }
    free(buf);
    close(sd);
    return 0;
}


/**
 * Returns the system uptime in centiseconds.
 *
 * @note The value returned by this function is not identical to sysUpTime
 *   defined in RFC 1213. get_uptime() returns the system uptime while
 *   sysUpTime represents the time that has elapsed since the most recent
 *   restart of the network manager (snmpd).
 *
 * @see See also netsnmp_get_agent_uptime().
 */
long System_getUptime(void) {

    FILE           *in = fopen("/proc/uptime", "r");
    long            uptim = 0, a, b;
    if (in) {
        if (2 == fscanf(in, "%ld.%ld", &a, &b))
            uptim = a * 100 + b;
        fclose(in);
    }
    return uptim;
}


int System_gethostbynameV4(const char * name, in_addr_t *addr_out)
{

    struct addrinfo *addrs = NULL;
    struct addrinfo hint;
    int             err;

    memset(&hint, 0, sizeof hint);
    hint.ai_flags = 0;
    hint.ai_family = PF_INET;
    hint.ai_socktype = SOCK_DGRAM;
    hint.ai_protocol = 0;

    err = System_getaddrinfo(name, NULL, &hint, &addrs);
    if (err != 0) {
        return -1;
    }

    if (addrs != NULL) {
        memcpy(addr_out,
               &((struct sockaddr_in *) addrs->ai_addr)->sin_addr,
               sizeof(in_addr_t));
        freeaddrinfo(addrs);
    } else {
        DEBUG_MSGTL(("get_thisaddr",
                    "Failed to resolve IPv4 hostname\n"));
    }
    return 0;
}


int System_getaddrinfo(const char *name, const char *service,
                    const struct addrinfo *hints, struct addrinfo **res)
{
    struct addrinfo *addrs = NULL;
    struct addrinfo hint;
    int             err;

    DEBUG_MSGTL(("dns:getaddrinfo", "looking up "));
    if (name)
        DEBUG_MSG(("dns:getaddrinfo", "\"%s\"", name));
    else
        DEBUG_MSG(("dns:getaddrinfo", "<NULL>"));

    if (service)
    DEBUG_MSG(("dns:getaddrinfo", ":\"%s\"", service));

    if (hints)
    DEBUG_MSG(("dns:getaddrinfo", " with hint ({ ... })"));
    else
    DEBUG_MSG(("dns:getaddrinfo", " with no hint"));

    DEBUG_MSG(("dns:getaddrinfo", "\n"));

    if (NULL == hints) {
        memset(&hint, 0, sizeof hint);
        hint.ai_flags = 0;
        hint.ai_family = PF_INET;
        hint.ai_socktype = SOCK_DGRAM;
        hint.ai_protocol = 0;
        hints = &hint;
    } else {
        memcpy(&hint, hints, sizeof hint);
    }

    err = getaddrinfo(name, NULL, &hint, &addrs);

    *res = addrs;
    if ((0 == err) && addrs && addrs->ai_addr) {
        DEBUG_MSGTL(("dns:getaddrinfo", "answer { AF_INET, %s:%hu }\n",
                    inet_ntoa(((struct sockaddr_in*)addrs->ai_addr)->sin_addr),
                    ntohs(((struct sockaddr_in*)addrs->ai_addr)->sin_port)));
    }
    return err;
}

struct hostent * System_gethostbyname(const char *name)
{

    struct hostent *hp = NULL;

    if (NULL == name)
        return NULL;

    DEBUG_MSGTL(("dns:gethostbyname", "looking up %s\n", name));

    hp = gethostbyname(name);

    if (hp == NULL) {
        DEBUG_MSGTL(("dns:gethostbyname",
                    "couldn't resolve %s\n", name));
    } else if (hp->h_addrtype != AF_INET) {
        DEBUG_MSGTL(("dns:gethostbyname",
                    "warning: response for %s not AF_INET!\n", name));
    } else {
        DEBUG_MSGTL(("dns:gethostbyname",
                    "%s resolved okay\n", name));
    }
    return hp;
}


/**
 * Look up the host name via DNS.
 *
 * @param[in] addr Pointer to the address to resolve. This argument points e.g.
 *   to a struct in_addr for AF_INET or to a struct in6_addr for AF_INET6.
 * @param[in] len  Length in bytes of *addr.
 * @param[in] type Address family, e.g. AF_INET or AF_INET6.
 *
 * @return Pointer to a hostent structure if address lookup succeeded or NULL
 *   if the lookup failed.
 *
 * @see See also the gethostbyaddr() man page.
 */
struct hostent * System_gethostbyaddr(const void *addr, socklen_t len, int type)
{
    struct hostent *hp = NULL;
    char buf[64];

    DEBUG_MSGTL(("dns:gethostbyaddr", "resolving %s\n", inet_ntop(type, addr, buf, sizeof(buf))));

    hp = gethostbyaddr(addr, len, type);

    if (hp == NULL) {
        DEBUG_MSGTL(("dns:gethostbyaddr", "couldn't resolve addr\n"));
    } else if (hp->h_addrtype != AF_INET) {
        DEBUG_MSGTL(("dns:gethostbyaddr",
                    "warning: response for addr not AF_INET!\n"));
    } else {
        DEBUG_MSGTL(("dns:gethostbyaddr", "addr resolved okay\n"));
    }
    return hp;
}

/**
 * Compute (*now - *then) in centiseconds.
 */
int System_calculateTimeDiff(const struct timeval *now, const struct timeval *then)
{
    struct timeval  diff;

    SYSTEM_TIMERSUB(now, then, &diff);
    return (int)(diff.tv_sec * 100 + diff.tv_usec / 10000);
}

/** Compute rounded (*now - *then) in seconds. */
uint System_calculateSectimeDiff(const struct timeval *now, const struct timeval *then)
{
    struct timeval  diff;

    SYSTEM_TIMERSUB(now, then, &diff);
    return diff.tv_sec + (diff.tv_usec >= 500000L);
}


int System_mkdirhier(const char *pathname, mode_t mode, int skiplast)
{
    struct stat     sbuf;
    char           *ourcopy = strdup(pathname);
    char           *entry;
    char           *buf = NULL;
    char           *st = NULL;
    int          res;

    res = ErrorCode_GENERR;
    if (!ourcopy)
        goto out;

    buf = (char *)malloc(strlen(pathname) + 2);
    if (!buf)
        goto out;

    entry = strtok_r(ourcopy, "/", &st);

    buf[0] = '\0';

    /*
     * check to see if filename is a directory
     */
    while (entry) {
        strcat(buf, "/");
        strcat(buf, entry);
        entry = strtok_r(NULL, "/", &st);
        if (entry == NULL && skiplast)
            break;
        if (stat(buf, &sbuf) < 0) {
            /*
             * DNE, make it
             */
            if (mkdir(buf, mode) == -1)
                goto out;
            else
                Logger_log(LOGGER_PRIORITY_INFO, "Created directory: %s\n", buf);
        } else {
            /*
             * exists, is it a file?
             */
            if ((sbuf.st_mode & S_IFDIR) == 0) {
                /*
                 * ack! can't make a directory on top of a file
                 */
                goto out;
            }
        }
    }
    res = ErrorCode_SUCCESS;
out:
    free(buf);
    free(ourcopy);
    return res;
}


/**
 * Convert a user name or number into numeric form.
 *
 * @param[in] useroruid Either a Unix user name or the ASCII representation
 *   of a user number.
 *
 * @return Either a user number > 0 or 0 if useroruid is not a valid user
 *   name, not a valid user number or the name of the root user.
 */
int System_strToUid(const char *useroruid) {
    int uid;
    struct passwd *pwd;

    uid = atoi(useroruid);

    if (uid == 0) {
        pwd = getpwnam(useroruid);
        uid = pwd ? pwd->pw_uid : 0;
        endpwent();
        if (uid == 0)
            Logger_log(LOGGER_PRIORITY_WARNING, "Can't identify user (%s).\n", useroruid);
    }
    return uid;
}

/**
 * Convert a group name or number into numeric form.
 *
 * @param[in] grouporgid Either a Unix group name or the ASCII representation
 *   of a group number.
 *
 * @return Either a group number > 0 or 0 if grouporgid is not a valid group
 *   name, not a valid group number or the root group.
 */
int System_strToGid(const char *grouporgid)
{
    int gid;

    gid = atoi(grouporgid);

    if (gid == 0) {
        struct group  *grp;

        grp = getgrnam(grouporgid);
        gid = grp ? grp->gr_gid : 0;
        endgrent();
        if (gid == 0)
            Logger_log(LOGGER_PRIORITY_WARNING, "Can't identify group (%s).\n", grouporgid);
    }
    return gid;
}
