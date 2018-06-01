#ifndef IOT_SYSTEM_H
#define IOT_SYSTEM_H

#include "Generals.h"
#include <netdb.h>
#include <netinet/in.h>

/**
 * @brief System_daemonize
 *        forks current process into the background.
 *
 * This function forks a process into the background, in order to
 * become a daemon process. It does a few things along the way:
 *
 * - becoming a process/session group leader, and  forking a second time so
 *   that process/session group leader can exit.
 *
 * - changing the working directory to /
 *
 * - closing stdin, stdout and stderr (unless stderrLog is set) and
 *   redirecting them to /dev/null
 *
 * @param quitImmediately - indicates if the parent process should
 *                          exit after a successful fork.
 *
 * @param stderrLog       - indicates if stderr is being used for
 *                          logging and shouldn't be closed
 *
 * @returns -1 : fork error
 *           0 : child process returning
 *          >0 : parent process returning. returned value is the child PID.
 */
int System_daemonize( int quitImmediately, int stderrLog );

/**
 * @brief System_getHostByAddr
 *         looks up the host name via DNS.
 *
 * @param[in] address - Pointer to the address to resolve. This argument points e.g.
 *            to a struct in_addr for AF_INET or to a struct in6_addr for AF_INET6.
 *
 * @param[in] length -  Length in bytes of *addr.
 * @param[in] type - Address family, e.g. AF_INET or AF_INET6.
 *
 * @return Pointer to a hostent structure if address lookup succeeded or NULL
 *         if the lookup failed.
 *
 * @see See also the gethostbyaddr() man page.
 */
struct hostent* System_getHostByAddr( const void* address, socklen_t length, int type );

/**
 * @brief System_getUpTime
 *        returns the system uptime in centiseconds.
 *
 * @note The value returned by this function is not identical to sysUpTime
 *       defined in RFC 1213. System_getUptime() returns the system uptime while
 *       sysUpTime represents the time that has elapsed since the most recent
 *       restart of the network manager (priotd).
 *
 * @return the system uptime in centiseconds.
 *
 * @see See also Agent_getAgentUptime().
 */
long System_getUpTime( void );

/**
 * @brief System_getHostByNameV4
 * @param name
 * @param addrOut
 * @return
 */
int System_getHostByNameV4( const char* name, in_addr_t* addrOut );

/**
 * @brief System_getAddrInfo
 *        Given hostname and service, which identify an Internet host and a
 *        service, returns one or more addrinfo structures, each
 *        of which contains an Internet address that can be specified in a call
 *        to bind(2) or connect(2).  The getaddrinfo() function combines the
 *        functionality provided by the gethostbyname(3) and getservbyname(3)
 *        functions into a single interface,
 *
 * @param hostname - can be either a domain name, such as "example.com",
 *                   an address string, such as "127.0.0.1", or NULL, in
 *                   which case the address 0.0.0.0 or 127.0.0.1 is
 *                   assigned depending on the hints flags.
 *
 * @param service - can be a port number passed as string, such as "80",
 *                  or a service name, e.g. "echo". In the latter case,
 *                  gethostbyname() is used to query the file /etc/services
 *                  to resolve the service to a port number.
 *
 * @param hints - can be either NULL or an addrinfo structure
 *                with the type of service requested. specifies
 *                criteria for selecting the socket address structures
 *                returned in the list pointed to by @p res
 *
 * @param[out] res - one or more addrinfo structures, each
                     of which contains an Internet address
 * @return returns 0 upon success and negative if it fails
 *
 * @see getaddrinfo
 */
int System_getAddrInfo( const char* hostname, const char* service, const struct addrinfo* hints, struct addrinfo** res );

/**
 * @brief System_getHostByName
 *        returns a structure of type hostent for the given host hostname.
 *
 * @param hostname - a pointer to the null-terminated name of the host to resolve.
 *
 * @return if no error occurs, gethostbyname returns a pointer to the
 *         hostent structure described above. Otherwise, it returns a null pointer
 */
struct hostent* System_getHostByName( const char* hostname );

/**
 * @brief System_makeTemporaryFile
 *        creates a temporary file based on the configured tempFilePattern.
 *
 * @return on success, returns the file name . Otherwise, returns NULL.
 *
 * @see ReadConfig_getTempFilePattern
 */
const char* System_makeTemporaryFile( void );

/**
 * @brief System_isOsPrefixMatch
 *        tests whether the name and kernel version correspond to @p kernelName
 *        and @p kernelReleasePrefix, respectively. It can be used to test
 *        kernels on any platform that supports uname().
 *
 * This function was created to differentiate actions
 * that are appropriate for Linux 2.4 kernels, but not later kernels. 
 *
 * @param kernelName -  the os kernel name
 * @param kernelReleasePrefix - the kernel release prefix
 * @returns -1 : If not running a platform that supports uname().
 *           0 : If @p kernelName matches up the OS kernel name, and the
 *               @p kernelReleasePrefix matches up the prefix of kernel version.
 *           1 : If the release is ordered higher. Be aware that
 *               "ordered higher" is not a guarantee of correctness.
 */
int System_isOsPrefixMatch( const char* kernelName, const char* kernelReleasePrefix );

/**
 * @brief System_getUserUidByUserName
 *        returns the user uid associated with the user name/number.
 *
 * @param[in] userNameOrNumber - either a Unix user name or the ASCII representation
 *                               of a user number.
 *
 * @returns >0 :  a user uid
 *           0 : if userNameOrNumber is not a valid user name/number or the number of the root user.
 */
int System_getUserUidByUserName( const char* userNameOrNumber );

/**
 * @brief System_getUserGidByUserName
 *        returns the group gid associated with the group name/number.
 *
 * @param[in] groupNameOrNumber - either a Unix group name or the ASCII representation
 *                                of a group number.
 *
 * @returns >0 :  a group gid
 *           0 : if groupNameOrNumber is not a valid group name/number or the number of the root group.
 */
int System_getUserGidByUserName( const char* groupNameOrNumber );

/**
 * @brief System_getMyAddress
 *        returns the IP address.
 *
 * @note What if we have multiple addresses?  Or no
 *       addresses for that matter? Could it be computed
 *       once then cached?  Probably not worth it (not used very often).
 *
 * @return on success, the IP address. Otherwise, returns 0.
 */
in_addr_t System_getMyAddress( void );

/**
 * @brief System_getEnvVariable
 *        returns a pointer to the desired environment variable.
 *        or NULL if the environment variable does not exist.
 *
 * @param envVariableName - environment variable name
 * @return on success, returns a pointer to the env variable.
 *         Otherwise, returns NULL.
 */
char* System_getEnvVariable( const char* envVariableName );

#endif // IOT_SYSTEM_H
