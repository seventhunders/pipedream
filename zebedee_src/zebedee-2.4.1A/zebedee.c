/*
** This file is part of "zebedee".
**
** Copyright 1999, 2000, 2001, 2002 by Neil Winton. All rights reserved.
** 
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** For further details on "zebedee" see http://www.winton.org.uk/zebedee/
**
*/

char *zebedee_c_rcsid = "$Id: zebedee.c,v 1.25 2002/05/28 06:31:15 ndwinton Exp $";
#define RELEASE_STR "2.4.1A"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>

#ifdef USE_GMP_LIBRARY
#include "gmp.h"
#else
#include "huge.h"
/*
** Zebedee originally used the GMP library (and this can still be enabled
** by defining USE_GMP_LIBRARY) but for reasons of portability it now uses
** the "Huge" number routines bundled with the Zebedee distribution. GMP
** is a very high-quality library but is hard to port to non-UN*X/gcc
** environments.
**
** The function calls used in the code are, however, still GMP-compatible
** through the use of the following macro definitions.
*/

typedef Huge *mpz_t;

#define mpz_init(z)
#define mpz_init_set_str(z, s, n)   (z = huge_from_string(s, NULL, n))
#define mpz_powm(k, g, e, m)	    (k = huge_powmod(g, e, m))
#define mpz_get_str(p, n, z)	    huge_format(z, n)
#define mpz_clear(z)		    huge_free(z)
#endif

#include "blowfish.h"
#include "zlib.h"
#ifndef DONT_HAVE_BZIP2
#include "bzlib.h"
#endif
#include "sha.h"

#ifdef __CYGWIN__
#undef WIN32
#endif

/*
** Named mutex values (see mutexLock/Unlock)
*/

#define MUTEX_IO	0   /* Mutex to protect stdio and other library calls */
#define MUTEX_KEYLIST	1   /* Mutex to protect key list access */
#define MUTEX_TOKEN	2   /* Mutex to protect token allocation/access */
#define MUTEX_HNDLIST	3   /* Mutex to protect UDP handler list access */
#define	MUTEX_ACTIVE	4   /* Mutex to protect count of active handlers */
#define MUTEX_MAX	5   /* How many mutexes will we use? */

/*
** Named condition variables
*/

#define COND_ACTIVE	0   /* Condition for change in active handler count */
#define COND_MAX	1   /* How many condition variables? */

#ifdef WIN32
/*
** Windows-specific include files and macros
*/

#include <windows.h>
#include <io.h>
#include <winsock.h>
#include <process.h>
#include <mmsystem.h>

#include "getopt.h"

#define DFLT_SHELL	"c:\\winnt\\system32\\cmd.exe"
#define getpid()	GetCurrentProcessId()
#define FILE_SEP_CHAR	'\\'
#define snprintf	_snprintf
#define vsnprintf	_vsnprintf

/*
** Winsock state data
*/

static struct WSAData WsaState;

/*
** Global Mutexes and Condition Variables
*/

CRITICAL_SECTION Mutex[MUTEX_MAX];
HANDLE Condition[COND_MAX];

extern void svcRun(char *name, VOID (*function)(VOID *), VOID *arg);
extern int svcInstall(char *name, char *configFile);
extern int svcRemove(char *name);

#else /* !WIN32 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/socket.h>
#ifndef DONT_HAVE_SELECT_H
#include <sys/select.h>
#endif
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <syslog.h>
#ifdef USE_UDP_SPOOFING
#include <libnet.h>
#endif

#define DFLT_SHELL	"/bin/sh"
#define FILE_SEP_CHAR	'/'

#define closesocket(fd)	    close((fd))

#ifdef HAVE_PTHREADS
#include <pthread.h>

pthread_mutex_t Mutex[MUTEX_MAX];
pthread_cond_t Condition[COND_MAX];
pthread_attr_t ThreadAttr;
#endif
#endif

/**************************\
**			  **
**  Constants and Macros  **
**			  **
\**************************/

#define MAX_BUF_SIZE	16383	/* Maximum network buffer size (< 2^14) */
#define DFLT_BUF_SIZE	8192	/* Default network buffer size */
#define MAX_LINE_SIZE	1024	/* Maximum file line size */
#define MAX_KEY_BYTES	((BF_ROUNDS + 2)*4) /* Maximum size of Blowfish key */
#define	MIN_KEY_BYTES	5	/* Minimum key length */
#define MAX_LISTEN	5	/* Depth of listen queue */
#define MAX_INCLUDE	5	/* Maximum depth of include files */
#define MAX_KEYGEN_LEVEL 2	/* Maximum key generation strength level */

#define HASH_STR_SIZE	41	/* Size of SHA hash string including null */
#define TIMESTAMP_SIZE	20	/* Size of YYYY-dd-mm-HH:MM:SS timestamp */
#define CHALLENGE_SIZE	4	/* Size of challenge data */
#define THE_ANSWER	42	/* To Life, the Universe and Everything */
#define CHALLENGE_SIZE	4	/* Size of challenge data */
#define NONCE_SIZE	8	/* Size of nonce data */

#ifndef THREAD_STACK_SIZE
#define THREAD_STACK_SIZE   32768   /* Minimum stack for threads */
#endif

#define	CMP_OVERHEAD	250	/* Maximum overhead on 16k message */
#define	CMP_MINIMUM	32	/* Minimum message size to attempt compression */

#define IP_BUF_SIZE	16	/* Size of buffer for IP address string */

/*
** Information about the compression algorithm and level is encoded in
** a single unsigned short value. The high 8 bits are the algorithm and
** the low eight bits the level. Note that the values used ensure that
** taken as a 16-bit quantity all bzip2 values are greater than all
** zlib values. This fact is used so that, in effect, bzip2 compression
** is considered "stronger" than zlib.
*/

#define CMPTYPE_ZLIB	0x0
#define CMPTYPE_BZIP2	0x1
#define GET_CMPTYPE(z)	    (((z) >> 8) & 0xff)
#define SET_CMPTYPE(z, t)   ((z) | ((t) << 8))
#define GET_CMPLEVEL(z)	    ((z) & 0xff)
#define SET_CMPLEVEL(z, l)  ((z) | ((l) & 0xff))

/*
** Each message that Zebedee transmits is preceded by an unsigned short
** value (in big-endian format). The top two bits flag whether the message
** is encrypted and compressed. The lower 14 bits define the payload size
** (which must be no greater than MAX_BUF_SIZE).
*/

#define FLAG_COMPRESSED	0x1
#define FLAG_ENCRYPTED	0x2

#define GET_FLAGS(x)	(((x) >> 14) & 0x3)
#define SET_FLAGS(x, f)	((x) | ((f) << 14))

#define GET_SIZE(x)	((x) & 0x3fff)

#define DFLT_GENERATOR	    "2"	    /* Default generator value */
#define DFLT_MODULUS	    	    /* Default modulus value */ \
"f488fd584e49dbcd20b49de49107366b336c380d451d0f7c88b31c7c5b2d8ef6" \
"f3c923c043f0a55b188d8ebb558cb85d38d334fd7c175743a31d186cde33212c" \
"b52aff3ce1b1294018118d7c84a70a72d686c40319c807297aca950cd9969fab" \
"d00a509b0246d3083d66a45d419f9c7cbd894b221926baaba25ec355e92f78c7"
#define DFLT_CMP_LEVEL	    SET_CMPLEVEL(CMPTYPE_ZLIB, 6)
#define DFLT_KEY_BITS	    128	    /* Default key size */
#define DFLT_TCP_PORT	    0x2EBD  /* Port on which TCP-mode server listens */
#define DFLT_UDP_PORT	    0x2BDE  /* Port on which UDP-mode server listens */
#define DFLT_KEY_LIFETIME   3600    /* Reuseable keys last an hour */
#define DFLT_TCP_TIMEOUT    0	    /* Default never close idle TCP tunnels */
#define DFLT_UDP_TIMEOUT    300	    /* Close UDP tunnels after 5 mins */
#define DFLT_CONN_TIMEOUT   300	    /* Timeout for server-initiated connections */

#define PROTOCOL_V100	    0x0100  /* The original and base */
#define PROTOCOL_V101	    0x0101  /* Extended buffer size */
#define PROTOCOL_V102	    0x0102  /* Optionally omit key exchange */
#define PROTOCOL_V200	    0x0200  /* Header, UDP and reusable key support */
#define PROTOCOL_V201	    0x0201  /* Remote target selection */
#define DFLT_PROTOCOL	    PROTOCOL_V201

#define TOKEN_NEW	    0xffffffff	/* Request new token allocation */
#define TOKEN_EXPIRE_GRACE  10		/* CurrentToken valid until this close to expiry */

#define HDR_SIZE_V200	    22	/* Size of V200 protocol header message */
#define HDR_SIZE_V201	    26	/* Size of V201 protocol header message */
#define HDR_SIZE_MIN	    HDR_SIZE_V200
#define HDR_SIZE_MAX	    HDR_SIZE_V201

#define HDR_OFFSET_FLAGS    0	/* Offset of flags (TCP vs UDP) */
#define HDR_OFFSET_MAXSIZE  2	/* Offset of max message size */
#define HDR_OFFSET_CMPINFO  4	/* Offset of compression info */
#define HDR_OFFSET_PORT	    6	/* Offset of port request */
#define HDR_OFFSET_KEYLEN   8	/* Offset of key length */
#define HDR_OFFSET_TOKEN    10	/* Offset of key token */
#define HDR_OFFSET_NONCE    14	/* Offset of nonce value */
#define HDR_OFFSET_TARGET   22	/* Offset of target host address */

#define HDR_FLAG_UDPMODE    0x1	/* Operate in UDP mode */

#define PORTLIST_TCP	    0x1	/* TCP-type port list */
#define PORTLIST_UDP	    0x2	/* UDP-type port list */
#define PORTLIST_ANY	    (PORTLIST_TCP | PORTLIST_UDP)

/***************************\
**			   **
**  Data Type Definitions  **
**			   **
\***************************/

/*
** The BFState_t structure holds all the state information necessary
** to encrypt one data-stream (unidirectional).
*/

#define	INIT_IVEC	"Time4Bed"  /* ... said Zebedee. Boing! */
typedef struct
{
    BF_KEY key;
    unsigned char iVec[8];
    int pos;
    unsigned char cryptBuf[MAX_BUF_SIZE];
}
BFState_t;

/*
** The PortList_t structure holds the information about a single port
** range (e.g. 10-20). A single port value has both hi and lo set the same.
** A linked list of these structures holds information about a set of ranges.
** The host element holds the name of the host associated with the port
** range, addr the matching IP address and addrList, a list of alias
** addresses. The mask is used if an address mask was specified. The type
** is a bitmask combination of PORTLIST_UDP and PORTLIST_UDP.
*/
 
typedef struct PortList_s
{
    unsigned short lo;
    unsigned short hi;
    char *host;
    struct sockaddr_in addr;
    struct in_addr *addrList;
    struct PortList_s *next;
    unsigned long mask;
    unsigned short type;
}
PortList_t;

/*
** The MsgBuf_t is the general buffer used by the low-level readMessage
** and writeMessage routines. It holds (nearly) all of the state for
** a single connection.
*/

typedef struct MsgBuf_s
{
    unsigned short maxSize;	/* Max size of data buffer read/writes */
    unsigned short size;	/* Size of current message */
    unsigned char data[MAX_BUF_SIZE];	/* Data buffer */
    unsigned char tmp[MAX_BUF_SIZE + CMP_OVERHEAD]; /* Temporary work space */
    unsigned short cmpInfo;	/* Compression level and type */
    BFState_t *bfRead;		/* Encryption context for reads */
    BFState_t *bfWrite;		/* Encryption context for writes */
    unsigned long readCount;	/* Number of reads */
    unsigned long bytesIn;	/* Actual data bytes from network */
    unsigned long expBytesIn;	/* Expanded data bytes in */
    unsigned long writeCount;	/* Number of writes */
    unsigned long bytesOut;	/* Actual data bytes to network */
    unsigned long expBytesOut;	/* Expanded data bytes out */
}
MsgBuf_t;

/*
** These enumerated type values are used to indicate the destination of
** log messages.
*/

typedef enum
{
    LOGFILE_NULL,
    LOGFILE_SYSLOG,
    LOGFILE_LOCAL
}
LogType_t;

/*
** The KeyInfo_t structure holds the mapping between a key "token" value
** used to request the reuse of a previously established shared secret
** key and the key value itself. These structures are strung together in
** a linked list.
*/

typedef struct KeyInfo_s
{
    unsigned long token;
    char *key;
    time_t expiry;
    struct KeyInfo_s *prev;
    struct KeyInfo_s *next;
}
KeyInfo_t;

/*
** This structure is used to pass the arguments to the main "handler"
** thread routines (client() and server()).
*/

typedef struct FnArgs_s
{
    int fd;
    unsigned short port;
    struct sockaddr_in addr;
    int listenFd;
    int inLine;
    int udpMode;
}
FnArgs_t;

/*
** This structure is used in UDP mode to find the local socket for
** the handler for traffic coming from a specific client.
*/

typedef struct HndInfo_s
{
    unsigned long id;
    int fd;
    struct sockaddr_in fromAddr;
    struct sockaddr_in localAddr;
    struct HndInfo_s *prev;
    struct HndInfo_s *next;
}
HndInfo_t;

/*****************\
**		 **
**  Global Data  **
**		 **
\*****************/

/*
** Note: Although this data is global most of it is not protected by mutex
** locks because once set in the start-up phases of the program it is
** read-only by the rest of the routines.
*/

FILE *LogFileP = NULL;		/* File handle for log file (NULL => stderr) */
LogType_t LogFileType = LOGFILE_LOCAL;	/* Type of log file */
unsigned short LogLevel = 1;	/* Message verbosity level */
char *Program = "zebedee";	/* Program name (argv[0]) */
char *Generator = "";		/* DH generator hex string ("" => default) */
char *Modulus = "";		/* DH modulus hex string ("" => default) */ 
char *PrivateKey = NULL;	/* Private key hex string */
unsigned short KeyLength = DFLT_KEY_BITS;	/* Key length in bits */
unsigned short MinKeyLength = 0;		/* Minimum allowed key length */
unsigned short CompressInfo = DFLT_CMP_LEVEL;	/* Compression type and level */
int IsDetached = 1;		/* Flag true if program should run detached */
int IsServer = 0;		/* Flag true if program is a server */
int Debug = 0;			/* Debug mode -- single threaded server */
char *CommandString = NULL;	/* Command string to execute (client) */
unsigned short ServerPort = 0;	/* Port on which server listens */
PortList_t *ClientPorts = NULL;	/* Ports on which client listens */
PortList_t *TargetPorts = NULL;	/* Target port to which to tunnel */
char *ServerHost = NULL;	/* Name of host on which server runs */
char *TargetHost = "localhost";	/* Default host to which tunnels are targeted */
char *IdentityFile = NULL;	/* Name of identity file to check, if any */
PortList_t *AllowedTargets = NULL; /* List of allowed target hosts/ports */
PortList_t *AllowedDefault = NULL; /* List of default allowed redirection ports */
PortList_t *AllowedPeers = NULL; /* List of allowed peer addresses/ports */
char *KeyGenCmd = NULL;		/* Key generator command string */
unsigned short KeyGenLevel = MAX_KEYGEN_LEVEL;	/* Key generation strength level */
int TimestampLog = 0;		/* Should messages have timestamps? */
int MultiUse = 1;		/* Client handles multiple connections? */
unsigned short MaxBufSize = DFLT_BUF_SIZE;  /* Maximum buffer size */
unsigned long CurrentToken = 0;	/* Client reuseable key token */
unsigned short KeyLifetime = DFLT_KEY_LIFETIME;	/* Key lifetime in seconds */
int UdpMode = 0;		/* Run in UDP mode */
int TcpMode = 1;		/* Run in TCP mode */
unsigned short TcpTimeout = DFLT_TCP_TIMEOUT;	/* TCP inactivity timeout */
unsigned short UdpTimeout = DFLT_UDP_TIMEOUT;	/* UDP inactivity timeout */
char *ListenIp = NULL;		/* IP address on which to listen */
int ListenMode = 0;		/* True if client waits for server connection */
char *ClientHost = NULL;	/* Server initiates connection to client */
int ListenSock = -1;		/* Socket on which to listen for server */
unsigned short ConnectTimeout = DFLT_CONN_TIMEOUT;  /* Timeout for server connections */
unsigned short ReadTimeout = 0; /* Timeout for remote data reads */
int ActiveCount = 0;		/* Count of active handlers */
char *ProxyHost = NULL;		/* HTTP proxy host, if used */
unsigned short ProxyPort = 0;	/* HTTP proxy port, if used */
int Transparent = 0;		/* Try to propagate the client IP address */
char *FieldSeparator = NULL;	/* Input field separator character */
char *SharedKey = NULL;		/* Static shared secret key */
char *SharedKeyGenCmd = NULL;	/* Command to generate shared secret key */
int DumpData = 0;		/* Dump out message contents only if true */

extern char *optarg;		/* From getopt */
extern int optind;		/* From getopt */

/*
** The following global data-structure ARE modified during normal operation
** and are protected by mutexes.
**
** The ClientKeyList and ServerKeyList are protected by the MUTEX_KEYLIST
** and the HandlerList by MUTEX_HNDLIST.
*/

KeyInfo_t ClientKeyList = { 0, NULL, (time_t)0, NULL, NULL };
				/* Client-side list of token->key mappings */
KeyInfo_t ServerKeyList = { 0, NULL, (time_t)0, NULL, NULL };
				/* Server-side list of token->key mappings */
HndInfo_t HandlerList;		/* List of address to handler mappings */


/*************************\
**			 **
**  Function Prototypes  **
**			 **
\*************************/

void threadInit(void);
void mutexInit(void);
void mutexLock(int num);
void mutexUnlock(int num);
void conditionInit(void);
void conditionSignal(int num);
void conditionWait(int condNum, int mutexNum);
unsigned long threadPid(void);
unsigned long threadTid(void);
void incrActiveCount(int num);
void waitForInactivity(void);

void logToSystemLog(unsigned short level, char *msg);
void timestamp(char *timeBuf, int local);
void message(unsigned short level, int err, char *fmt, ...);
void dumpData(const char *prefix, unsigned char *data, unsigned short size);

int readData(int fd, unsigned char *buffer, unsigned short size);
int readUShort(int fd, unsigned short *resultP);
int writeData(int fd, unsigned char *buffer, unsigned short size);
int writeUShort(int fd, unsigned short value);

MsgBuf_t *makeMsgBuf(unsigned short maxSize, unsigned short cmpInfo);
void freeMsgBuf(MsgBuf_t *msg);
void getMsgBuf(MsgBuf_t *msg, void *buffer, unsigned short size);
void setMsgBuf(MsgBuf_t *msg, void *buffer, unsigned short size);

int readMessage(int fd, MsgBuf_t *msg, unsigned short thisSize);
int writeMessage(int fd, MsgBuf_t *msg);

int requestResponse(int fd, unsigned short request, unsigned short *responseP);

int getHostAddress(const char *host, struct sockaddr_in *addrP, struct in_addr **addrList, unsigned long *maskP);
char *ipString(struct in_addr addr, char *buf);
int makeConnection(const char *host, const unsigned short port, int udpMode, int useProxy, struct sockaddr_in *fromAddrP, struct sockaddr_in *toAddrP);
int proxyConnection(const char *host, const unsigned short port, struct sockaddr_in *localAddrP);
int sendSpoofed(int fd, char *buf, int len, struct sockaddr_in *toAddrP, struct sockaddr_in *fromAddrP);
int makeListener(unsigned short *portP, char *listenIp, int udpMode);
void setNoLinger(int fd);
void setKeepAlive(int fd);
int acceptConnection(int listenFd, const char *host, int loop, unsigned short timeout);

void headerSetUShort(unsigned char *hdrBuf, unsigned short value, int offset);
void headerSetULong(unsigned char *hdrBuf, unsigned long value, int offset);
unsigned short headerGetUShort(unsigned char *hdrBuf, int offset);
unsigned long headerGetULong(unsigned char *hdrBuf, int offset);

BFState_t *setupBlowfish(char *keyStr, unsigned short keyBits);
char *generateKey(void);
char *runKeyGenCommand(char *keyGenCmd);
void generateNonce(unsigned char *);
char *generateSessionKey(char *secretKey, unsigned char *cNonce, unsigned char *sNonce, unsigned short bits);
unsigned short hexStrToBits(char *hexStr, unsigned short bits, unsigned char *bitVec);
char *diffieHellman(char *genStr, char *modStr, char *expStr);
void makeChallenge(unsigned char *challenge);
void challengeAnswer(unsigned char *challenge);
int clientPerformChallenge(int serverFd, MsgBuf_t *msg);
int serverPerformChallenge(int clientFd, MsgBuf_t *msg);

void freeKeyInfo(KeyInfo_t *info);
char *findKeyByToken(KeyInfo_t *list, unsigned long token);
void addKeyInfoToList(KeyInfo_t *list, unsigned long token, char *key);
unsigned long generateToken(KeyInfo_t *list, unsigned long oldToken);
unsigned long getCurrentToken(void);

int spawnCommand(unsigned short port, char *cmdFormat);
int filterLoop(int localFd, int remoteFd, MsgBuf_t *msgBuf,
	       struct sockaddr_in *toAddrP, struct sockaddr_in *fromAddrP,
	       int replyFd, int udpMode);

void hashStrings(char *hashBuf, ...);
void hashFile(char *hashBuf, char *fileName);
int checkIdentity(char *idFile, char *generator, char *modulus, char *key);
char *generateIdentity(char *generator, char *modulus, char *exponent);

unsigned long spawnHandler(void (*handler)(FnArgs_t *), int listenFd, int clientFd, int inLine, struct sockaddr_in *addrP, int udpMode);
int findHandler(struct sockaddr_in *fromAddrP, struct sockaddr_in *localAddrP);
void addHandler(struct sockaddr_in *fromAddrP, unsigned long id, int fd, struct sockaddr_in *localAddrP);
void removeHandler(struct sockaddr_in *addrP);

void clientListener(PortList_t *localPorts);
int makeClientListeners(PortList_t *ports, fd_set *listenSetP, int udpMode);
void client(FnArgs_t *argP);
void prepareToDetach(void);
void makeDetached(void);
void serverListener(unsigned short *portPtr);
void serverInitiator(char *client, unsigned short port, unsigned short timeout);
int allowRedirect(unsigned short port, struct sockaddr_in *addrP, int udpMode, char **hostP);
int checkPeerAddress(int fd, struct sockaddr_in *addrP);
int countPorts(PortList_t *list);
unsigned short mapPort(unsigned short localPort, char **hostP, struct sockaddr_in *addrP);
void server(FnArgs_t *argP);

unsigned short scanPortRange(const char *str, unsigned short *loP,
			     unsigned short *hiP, unsigned short *typeP);
void setBoolean(char *value, int *resultP);
void setUShort(char *value, unsigned short *resultP);
void setPort(char *value, unsigned short *resultP);
PortList_t *newPortList(unsigned short lo, unsigned short hi, char *host, unsigned short type);
PortList_t *allocPortList(unsigned short lo, unsigned short hi, char *host, struct in_addr *addrP, struct in_addr *addrList, unsigned long mask, unsigned short type);
void setPortList(char *value, PortList_t **listP, char *host, int zeroOk);
void setTarget(char *value);
void setTunnel(char *value);
void setAllowedPeer(char *value);
void setString(char *value, char **resultP);
void setLogFile(char *newFile);
void setCmpInfo(char *value, unsigned short *resultP);
void readConfigFile(const char *fileName, int level);
int parseConfigLine(const char *lineBuf, int level);

char *cleanHexString(char *str);

void usage(void);

void sigpipeCatcher(int sig);
void sigchldCatcher(int sig);
void sigusr1Catcher(int sig);

/*************************************\
**				     **
**  Thread Synchronisation Routines  **
**				     **
\*************************************/

/*
** threadInit
**
** Set up global mutexes, condition variables and thread attributes. Must
** be called before any other thread routines.
*/

void
threadInit(void)
{
    mutexInit();
    conditionInit();
#if defined(HAVE_PTHREADS)
    pthread_attr_init(&ThreadAttr);
    pthread_attr_setstacksize(&ThreadAttr, THREAD_STACK_SIZE);
    pthread_attr_setdetachstate(&ThreadAttr, PTHREAD_CREATE_DETACHED);
#endif
}

/*
** mutexInit
**
** Initialise global mutexes.
*/

void
mutexInit(void)
{
#if defined(WIN32)
    int i;

    for (i = 0; i < MUTEX_MAX; i++)
    {
	InitializeCriticalSection(&(Mutex[i]));
    }
#elif defined(HAVE_PTHREADS)
    int i;

    for (i = 0; i < MUTEX_MAX; i++)
    {
	pthread_mutex_init(&(Mutex[i]), NULL);
    }
#endif
}

/*
** mutexLock
**
** Lock a global mutex
*/

void
mutexLock(int num)
{
    assert(num < MUTEX_MAX);

#if defined(WIN32)
    EnterCriticalSection(&(Mutex[num]));
#elif defined(HAVE_PTHREADS)
    pthread_mutex_lock(&(Mutex[num]));
#endif
}

/*
** mutexUnlock
**
** Unlock a global mutex
*/

void
mutexUnlock(int num)
{
    assert(num < MUTEX_MAX);

#if defined(WIN32)
    LeaveCriticalSection(&(Mutex[num]));
#elif defined(HAVE_PTHREADS)
    pthread_mutex_unlock(&(Mutex[num]));
#endif
}

/*
** conditionInit
**
** Initialise global condition variables.
*/

void
conditionInit(void)
{
#if defined(WIN32)
    int i;

    for (i = 0; i < COND_MAX; i++)
    {
	Condition[i] = CreateEvent(NULL,    /* No security attributes */
				   TRUE,    /* Manual reset */
				   FALSE,   /* Initially cleared */
				   NULL);   /* No name */
    }
#elif defined(HAVE_PTHREADS)
    int i;

    for (i = 0; i < COND_MAX; i++)
    {
	pthread_cond_init(&(Condition[i]), NULL);
    }
#endif
}

/*
** conditionSignal
**
** Signal a condition variable
*/

void
conditionSignal(int num)
{
    assert(num < COND_MAX);

#if defined(WIN32)
    PulseEvent(Condition[num]);
#elif defined(HAVE_PTHREADS)
    pthread_cond_broadcast(&(Condition[num]));
#endif
}

/*
** conditionWait
**
** Wait on a condition variable. Note the specified mutex must be held
** before calling this routine. It will also be held on exit.
*/

void
conditionWait(int condNum, int mutexNum)
{
    assert(condNum < COND_MAX && mutexNum < MUTEX_MAX);

#if defined(WIN32)
    LeaveCriticalSection(&(Mutex[mutexNum]));
    WaitForSingleObject(Condition[condNum], INFINITE);
    EnterCriticalSection(&(Mutex[mutexNum]));
#elif defined(HAVE_PTHREADS)
    pthread_cond_wait(&(Condition[condNum]), &(Mutex[mutexNum]));
#endif
}

/*
** threadPid
**
** Return the current process ID
*/

unsigned long
threadPid(void)
{
#ifdef WIN32
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

/*
** threadTid
**
** Return the current thread ID
*/

unsigned long
threadTid(void)
{
#ifdef WIN32
    return (unsigned long)GetCurrentThreadId();
#elif defined(HAVE_PTHREADS)
    return (unsigned long)pthread_self();
#else
    return 0;
#endif
}

/*
** incrActiveCount
**
** This increments or decrements the count of active handler threads.
** If the count reaches zero it also signals the COND_ACTIVE condition
** variable.
*/

void
incrActiveCount(int num)
{
    mutexLock(MUTEX_ACTIVE);
    ActiveCount += num;
    if (ActiveCount == 0)
    {
	conditionSignal(COND_ACTIVE);
    }
    mutexUnlock(MUTEX_ACTIVE);
}

/*
** waitForInactivity
**
** This routine blocks until the "ActiveCount" global variable reaches
** zero, indicating no more running handler threads.
*/

void
waitForInactivity(void)
{
#if defined(WIN32) || defined(HAVE_PTHREADS)
    mutexLock(MUTEX_ACTIVE);
    while (ActiveCount)
    {
	conditionWait(COND_ACTIVE, MUTEX_ACTIVE);
    }
    mutexUnlock(MUTEX_ACTIVE);
#else
    while (waitpid(-1, NULL, 0) > 0 || errno != ECHILD) /* Wait for children */;
#endif
}

/*********************\
**		     **
**  Message Logging  **
**		     **
\*********************/

/*
** timestamp
**
** Generate a time-stamp string
*/

void
timestamp(char *timeBuf, int local)
{
    time_t now;
    struct tm *tmPtr;

    /* localtime()/gmtime are not thread-safe */

    mutexLock(MUTEX_IO);
    time(&now);
    if (local)
    {
	tmPtr = localtime(&now);
    }
    else
    {
	tmPtr = gmtime(&now);
    }
    strftime(timeBuf, TIMESTAMP_SIZE, "%Y-%m-%d-%H:%M:%S", tmPtr);
    mutexUnlock(MUTEX_IO);
}

/*
** logToSystemLog
**
** Write a message to the system logging facility. On Windows it goes to
** the system application event log. Elsewhere is uses syslog().
*/

void
logToSystemLog(unsigned short level, char *msg)
{
#ifdef WIN32
    HANDLE  eventHandle;
    char    *strings[2];


    eventHandle = RegisterEventSource(NULL, Program);

    strings[0] = msg;
    strings[1] = NULL;

    if (eventHandle != NULL)
    {
	ReportEvent(eventHandle,	    /* Handle of event source */
		    (level ? EVENTLOG_INFORMATION_TYPE :
		     EVENTLOG_ERROR_TYPE),  /* Event type */
		    (WORD)level,	    /* Event category */
		    0,			    /* Event ID */
		    NULL,		    /* User SID */
		    1,			    /* Number of message strings */
		    0,			    /* Bytes of binary data */
		    (const char **)strings, /* Array of message strings */
		    NULL);		    /* No binary data */
	DeregisterEventSource(eventHandle);
    }
#else
    int logLevel;

    /*
    ** Messages at level 0 are errors, 1 is notice, 2 informational
    ** and everything else is classed as debug.
    */

    switch (level)
    {
    case 0:
	logLevel = LOG_ERR;
	break;

    case 1:
	logLevel = LOG_NOTICE;
	break;

    case 2:
	logLevel = LOG_INFO;
	break;

    default:
	logLevel = LOG_DEBUG;
	break;
    }

    syslog(logLevel, msg);
#endif
}

/*
** message
**
** Output a message to the current log file if the message verbosity is
** greater than or equal to the specified level. Messages at level 0
** can not be suppressed (unless the log-file type is NULL) and are all
** error messages.
**
** If errno is non-zero then append the matching error text.
*/

void
message(unsigned short level, int err, char *fmt, ...)
{
    FILE *fp = LogFileP;
    va_list args;
    char timeBuf[TIMESTAMP_SIZE];
    char *timePtr = NULL;
    char msgBuf[MAX_LINE_SIZE];


    if (level > LogLevel || LogFileType == LOGFILE_NULL) return;

    /*
    ** If we are running detached and no logfile has been set then there
    ** is nowhere for the messages to go. Worse still, under UNIX this
    ** trying to write to stderr can hang the process.
    */

    if (IsDetached == -1 && fp == NULL) return;

    va_start(args, fmt);

    if (fp == NULL)
    {
	fp = stderr;
    }

    if (TimestampLog)
    {
	timestamp(timeBuf, 1);
	timePtr = timeBuf;
    }

    /*
    ** The message format is the program name followed by the (low five
    ** digits of) the PID and thread ID then an optional timestamp followed
    ** by an amount of indentation determined by the level. This is
    ** then followed by the supplied message text and arguments and
    ** finally the error message text (if any) associated with the supplied
    ** error number!
    */

    snprintf(msgBuf, sizeof(msgBuf), "%s(%lu/%lu): %s%s%.*s%s",
	     Program, (threadPid() % 100000), (threadTid() % 100000),
	     (timePtr ? timePtr : ""), (timePtr ? ": " : ""),
	     level, "          ", (level ? "" : "ERROR: "));

    vsnprintf(msgBuf + strlen(msgBuf), sizeof(msgBuf) - strlen(msgBuf),
	      fmt, args);

    va_end(args);

    if (err)
    {
	snprintf(msgBuf + strlen(msgBuf), sizeof(msgBuf) - strlen(msgBuf),
		 ": (%s)", strerror(err));
    }

    /* Ensure we don't get overlapping messages */

    mutexLock(MUTEX_IO);

    switch (LogFileType)
    {
    case LOGFILE_LOCAL:
	fprintf(fp, "%s\n", msgBuf);
	fflush(fp);
	break;

    case LOGFILE_SYSLOG:
	logToSystemLog(level, msgBuf);
	break;

    default:
	break;
    }

    mutexUnlock(MUTEX_IO);

}

/*
** dumpData
**
** Dump data buffer (at verbosity level 5) only if DumpData is true.
*/

void dumpData(const char *prefix, unsigned char *data, unsigned short size)
{
    unsigned short i;
    unsigned char buf[128];
    unsigned char *bptr = NULL;
    static char *hex = "0123456789abcdef";

    if (!DumpData) return;

    bptr = buf;
    for (i = 0; i < size; i++)
    {
	if (isprint(data[i]))
	{
	    *bptr++ = data[i];
	    *bptr++ = ' ';
	}
	else
	{
	    *bptr++ = hex[(data[i] >> 4) & 0xf];
	    *bptr++ = hex[data[i] & 0xf];
	}
	*bptr++ = ' ';
	    
	if ((i % 16) == 15)
	{
	    *(bptr - 1) = '\0';
	    message(5, 0, "%s %04hx %s", prefix, (i - 15), buf);
	    bptr = buf;
	}
    }

    if (i % 16)
    {
	*bptr = '\0';
	message(5, 0, "%s %04hx %s", prefix, (i - (i % 16)), buf);
	bptr = buf;
    }
}

/*******************************\
**			       **
**  Network Data Transmission  **
**			       **
\*******************************/

/*
** readData
**
** Read and reassemble a potentially fragmented message from the network.
** If the global ReadTimeout is non-zero then we will only wait for that
** many seconds for data to arrive.
*/

int
readData(int fd, unsigned char *buffer, unsigned short size)
{
    int num = 0;
    char *bufP = NULL;
    unsigned short total = 0;
    struct timeval delay;
    fd_set testSet;
    int ready;

    bufP = (char *)buffer;
    do
    {
	if (ReadTimeout != 0)
	{
	    delay.tv_sec = ReadTimeout;
	    delay.tv_usec = 0;

	    FD_ZERO(&testSet);
	    FD_SET(fd, &testSet);

	    ready = select(fd + 1, &testSet, 0, 0, &delay);

	    if (ready == 0)
	    {
		message(0, errno, "timed out reading data");
		return -1;
	    }
	}

	message(5, 0, "readData: receiving %d of %d", (size - total), size);
	if ((num = recv(fd, (bufP + total), (size - total), 0)) <= 0)
	{
	    message(5, errno, "readData: EOF or error");
	    /* Premature EOF or error */
	    return num;
	}
	message(5, 0, "readData: read %d byte(s)", num);
	total += (unsigned short)num;
    }
    while (total < size);

    return total;
}

/* 
** readUShort
**
** Read an unsigned short value from the network.
**
** The value is transmitted in big-endian format. The routine returns the
** number of bytes read (or 0 on EOF, -1 on error) and the value itself
** via valueP.
*/

int
readUShort(int fd, unsigned short *resultP)
{
    int num = 0;
    unsigned char buffer[2];

    if ((num = readData(fd, buffer, 2)) != 2)
    {
	return num;
    }

    *resultP = ((unsigned short)buffer[0] << 8) + (unsigned short)buffer[1];
    message(4, 0, "readUShort: read %hu", *resultP);

    return num;
}

/*
** writeData
**
** Write the supplied buffer of data to the network, handling fragmentation
** if necessary.
*/

int
writeData(int fd, unsigned char *buffer, unsigned short size)
{
    int num = 0;
    char *bufP = NULL;
    unsigned short total = 0;

    bufP = (char *)buffer;
    do
    {
	message(5, 0, "writeData: sending %d of %d", (size - total), size);
	if ((num = send(fd, (bufP + total), (size - total), 0)) <= 0)
	{
	    /* Premature EOF or error */
	    message(5, errno, "writeData: EOF or error");
	    return num;
	}
	total += (unsigned short)num;
	message(5, 0, "writeData: sent %d byte(s)", num);
    }
    while (total < size);

    return total;
}

/*
** writeUShort
**
** Write an unsigned short value to the network in big-endian format
*/

int
writeUShort(int fd, unsigned short value)
{
    unsigned char buf[2];

    message(4, 0, "writeUShort: writing %hu", value);

    buf[0] = (unsigned char)((value >> 8) & 0xff);
    buf[1] = (unsigned char)(value & 0xff);

    return writeData(fd, buf, 2);
}

/*
** makeMsgBuf
**
** Allocate a MsgBuf_t structure
*/

MsgBuf_t *
makeMsgBuf(unsigned short maxSize, unsigned short cmpInfo)
{
    MsgBuf_t *msg;


    if ((msg = (MsgBuf_t *)malloc(sizeof(MsgBuf_t))) == NULL)
    {
	message(0, errno, "Failed to allocate message structure");
	return NULL;
    }

    msg->maxSize = maxSize;
    msg->size = 0;
    msg->cmpInfo = cmpInfo;
    msg->bfRead = NULL;
    msg->bfWrite = NULL;
    msg->readCount = 0;
    msg->bytesIn = 0;
    msg->expBytesIn = 0;
    msg->writeCount = 0;
    msg->bytesOut = 0;
    msg->expBytesOut = 0;

    return msg;
}

/*
** freeMsgBuf
**
** Free a message buffer. But I bet you could guess that :-)
*/

void
freeMsgBuf(MsgBuf_t *msg)
{
    if (msg)
    {
	if (msg->bfRead) free(msg->bfRead);
	if (msg->bfWrite) free(msg->bfWrite);
	free(msg);
    }
}

/*
** getMsgBuf
**
** Retrieve the contents of a message buffer into the supplied local
** buffer.
*/

void
getMsgBuf(MsgBuf_t *msg, void *buffer, unsigned short size)
{
    if (msg->size > size)
    {
	message(0, 0, "supplied buffer too small for received message (%hu > %hu)", msg->size, size);
    }

    memcpy(buffer, msg->data, (size < msg->size ? size : msg->size));
}

/*
** setMsgBuf
**
** Set the contents of a message buffer from the supplied local
** buffer and size.
*/

void
setMsgBuf(MsgBuf_t *msg, void *buffer, unsigned short size)
{
    msg->size = size;
    memcpy(msg->data, buffer, size);
}

/*
** readMessage
**
** Read a message from the network into the supplied buffer, uncompressing
** and decrypting as necessary. The maximum amount of data read is given
** by msg->maxSize UNLESS thisSize is non-zero in which case this overrides
** the value in the structure.
**
** The size of the expanded, unencrypted message is returned as the value
** of the function and also via msg->size. If there is an error then -1 is
** returned.
*/

int
readMessage(int fd, MsgBuf_t *msg, unsigned short thisSize)
{
    unsigned short hdr;
    unsigned short size;
    unsigned short flags;
    int num = 0;
    unsigned long uncmpSize = MAX_BUF_SIZE;


    /* Read the header */

    if ((num = readUShort(fd, &hdr)) != 2) return num;

    /* Extract the flags and message size */

    flags = GET_FLAGS(hdr);
    size = GET_SIZE(hdr);

    /* Reject invalid messages */

    if (thisSize ? size > thisSize : size > msg->maxSize)
    {
	message(0, 0, "incoming message size too big (%hu > %hu)", size, msg->maxSize);
	return -1;
    }

    msg->size = size;
    msg->readCount++;
    msg->bytesIn += size;

    message(4, 0, "readMessage: message size = %hu, %s, %s", size,
	    ((flags & FLAG_ENCRYPTED) ? "encrypted" : "unencrypted"),
	    ((flags & FLAG_COMPRESSED) ? "compressed" : "uncompressed"));

    /* Read the remaining message data */

    if ((num = readData(fd, msg->tmp, size)) != (int)size) return num;

    /* Decrypt if necessary */

    if (flags & FLAG_ENCRYPTED)
    {
	if (msg->bfRead == NULL)
	{
	    message(0, 0, "message with encryption flag sent with no encryption context");
	    return -1;
	}

	BF_cfb64_encrypt(msg->tmp, msg->bfRead->cryptBuf, size,
			 &(msg->bfRead->key), msg->bfRead->iVec,
			 &(msg->bfRead->pos), BF_DECRYPT);
	memcpy(msg->tmp, msg->bfRead->cryptBuf, size);
    }

    /* Decompress if necessary */

    if (flags & FLAG_COMPRESSED)
    {
	switch (GET_CMPTYPE(msg->cmpInfo))
	{
	case CMPTYPE_ZLIB:
	    if ((num = uncompress(msg->data, &uncmpSize,
				  (Byte *)(msg->tmp), size)) != Z_OK)
	    {
		message(0, errno, "uncompressing message data (zlib status = %d)", num);
		errno = 0;
		return -1;
	    }
	    break;

	case CMPTYPE_BZIP2:
#ifndef DONT_HAVE_BZIP2
	    if ((num = BZ2_bzBuffToBuffDecompress((char *)(msg->data),
						  (unsigned int *)&uncmpSize,
						  (char *)(msg->tmp),
						  (unsigned int)size,
						  0, 0)) != BZ_OK)
	    {
		message(0, errno, "uncompressing message data (bzip2 status = %d)", num);
		errno = 0;
		return -1;
	    }
	    break;
#else
	    message(0, 0, "received unsupported bzip2 compressed message -- should never happen!");
	    return -1;
	    break;
#endif
	default:
	    message(0, 0, "invalid compression info in readMessage (%#hx)", msg->cmpInfo);
	    return -1;
	    break;
	}

	msg->size = size = (unsigned short)uncmpSize;
	message(4, 0, "readMessage: uncompressed size = %hu", size);
    }
    else
    {
	memcpy(msg->data, msg->tmp, size);
    }

    msg->expBytesIn += size;

    return (int)size;
}

/*
** writeMessage
**
** Write a message to the network containing the data from buffer, compressing
** and encrypting as necessary.
**
** The size of the original expanded, unencrypted message is returned as the
** value of the function on success or the status from writeData() on error.
**
** The message is sent as an unsigned short header (in the format written
** by writeUShort) followed by the data itself. The header value consists
** of the length or'ed with flags indicating if the message is compressed
** and encrypted.
*/

int
writeMessage(int fd, MsgBuf_t *msg)
{
    unsigned short size = msg->size;
    unsigned short hdr;
    int num = 0;
    unsigned long cmpSize = MAX_BUF_SIZE + CMP_OVERHEAD;
    unsigned short flags = 0;
    unsigned char *data = msg->data;


    /* Attempt compression if the message size warrants it */

    if (msg->cmpInfo && msg->size > CMP_MINIMUM)
    {
	switch (GET_CMPTYPE(msg->cmpInfo))
	{
	case CMPTYPE_ZLIB:
	    if ((num = compress2(msg->tmp + 2, &cmpSize,
				 (const Byte *)(msg->data), size,
				 GET_CMPLEVEL(msg->cmpInfo))) != Z_OK)
	    {
		message(0, errno, "compressing data (zlib status = %d)", num);
		cmpSize = msg->size;
	    }
	    break;

	case CMPTYPE_BZIP2:
#ifndef DONT_HAVE_BZIP2
	    if ((num = BZ2_bzBuffToBuffCompress((char *)(msg->tmp + 2),
					    (unsigned int *)&cmpSize,
					    (char *)(msg->data),
					    (unsigned int)size,
					    (int)GET_CMPLEVEL(msg->cmpInfo),
					    0, 0)) != BZ_OK)
	    {
		message(0, errno, "compressing data (bzip2 status = %d)", num);
	    }
	    break;
#else
	    message(0, 0, "request to use unsupported bzip2 compression!");
	    cmpSize = (unsigned long)size;
	    break;
#endif

	default:
	    cmpSize = (unsigned long)size;
	    break;
	}

	/* Only use compressed message if it is shorter */

	if (cmpSize < (unsigned long)size)
	{
	    message(4, 0, "writeMessage: message compressed from %hu to %lu bytes", msg->size, cmpSize);
	    data = msg->tmp + 2;
	    size = (unsigned short)cmpSize;
	    flags |= FLAG_COMPRESSED;
	}
    }

    /* Encrypt if required */

    if (msg->bfWrite)
    {
	BF_cfb64_encrypt(data, msg->bfWrite->cryptBuf, size,
			 &(msg->bfWrite->key), msg->bfWrite->iVec,
			 &(msg->bfWrite->pos), BF_ENCRYPT);
	memcpy(msg->tmp + 2, msg->bfWrite->cryptBuf, size);
	flags |= FLAG_ENCRYPTED;
    }
    else
    {
	memmove(msg->tmp + 2, data, size);
    }

    /* Insert the header */

    hdr = SET_FLAGS(size, flags);

    msg->tmp[0] = (unsigned char)((hdr >> 8) & 0xff);
    msg->tmp[1] = (unsigned char)(hdr & 0xff);

    /* Write the message data */

    message(4, 0, "writeMessage: message size = %hu, %s, %s", size,
	    ((flags & FLAG_ENCRYPTED) ? "encrypted" : "unencrypted"),
	    ((flags & FLAG_COMPRESSED) ? "compressed" : "uncompressed"));

    if ((num = writeData(fd, msg->tmp, size + 2)) != (int)(size + 2)) return num;

    msg->writeCount++;
    msg->bytesOut += size;
    msg->expBytesOut += msg->size;
    return msg->size;
}

/*
** requestResponse
**
** This is a helper routine that sends the unsigned short "request" value
** and awaits the response, storing it in "responseP". It returns 1 if
** successful or 0 otherwise.
*/

int
requestResponse(int fd, unsigned short request, unsigned short *responseP)
{
    /* Write request */

    if (writeUShort(fd, request) != 2)
    {
	return 0;
    }

    /* Read response */

    if (readUShort(fd, responseP) != 2)
    {
	return 0;
    }

    return 1;
}

/*
** getHostAddress
**
** Translate a hostname or numeric IP address and store the result in addrP.
** If addrList is not NULL then return the list of aliases addresses in it.
** The list is terminated with an address with all components set to 0xff
** (i.e. 255.255.255.255). If maskP is not NULL and the address is in the
** form of a CIDR mask specification then it will be set to contain the
** appropriate address mask.
**
** Returns 1 on success, 0 on failure.
*/

int
getHostAddress(const char *host,
	       struct sockaddr_in *addrP,
	       struct in_addr **addrList,
	       unsigned long *maskP)
{
    struct hostent *entry = NULL;
    int result = 1;
    int count = 0;
    int i = 0;
    char *s = NULL;
    char *hostCopy = NULL;
    unsigned short bits = 32;


    mutexLock(MUTEX_IO);

    /*
    ** If there is a mask spec then eliminate it from the host name
    ** and create the mask, if required.
    */

    if ((s = strchr(host, '/')) != NULL)
    {
	hostCopy = (char *)malloc(strlen(host) + 1);
	if (!hostCopy || (sscanf(host, "%[^/]/%hu", hostCopy, &bits) != 2))
	{
	    errno = 0;
	    result = 0;
	}
	host = hostCopy;
	if (maskP)
	{
	    if (bits <= 0 || bits > 32) bits = 32;
	    *maskP = htonl(0xffffffff << (32 - bits));
	}
    }

    /*
    ** Try a direct conversion from numeric form first in order to avoid
    ** an unnecessary name-service lookup.
    */

    if ((addrP->sin_addr.s_addr = inet_addr(host)) == 0xffffffff)
    {
	if ((entry = gethostbyname(host)) == NULL)
	{
	    errno = 0;
	    result = 0;
	}
	else
	{
	    memcpy(&(addrP->sin_addr), entry->h_addr, entry->h_length);
	}
    }

    /* Retrieve full list of addresses if required */

    if (addrList != NULL)
    {
	if (entry)
	{
	    for (count = 0; entry->h_addr_list[count]; count++)
	    {
		/* Nothing */
	    }
	    *addrList = (struct in_addr *)calloc(count + 1, sizeof(struct in_addr));
	    if (*addrList == NULL)
	    {
		result = 0;
	    }
	    else
	    {
		for (i = 0; i < count; i++)
		{
		    memcpy(&((*addrList)[i]), entry->h_addr_list[i], sizeof(struct in_addr));
		}
		memset(&((*addrList)[i]), 0xff, sizeof(struct in_addr));
	    }
	}
	else
	{
	    *addrList = (struct in_addr *)calloc(2, sizeof(struct in_addr));
	    memcpy(&((*addrList)[0]), &(addrP->sin_addr), sizeof(struct in_addr));
	    memset(&((*addrList)[1]), 0xff, sizeof(struct in_addr));
	}
    }

    if (hostCopy)
    {
	free(hostCopy);
    }

    mutexUnlock(MUTEX_IO);

    return result;
}

/*
** ipString
**
** Convert IP address to a dotted-quad string. This is effectively a
** reentrant version of inet_ntoa.
*/

char *
ipString(struct in_addr addr, char *buf)
{
    unsigned long val = ntohl(addr.s_addr);
    sprintf(buf, "%d.%d.%d.%d",
	    (val >> 24) & 0xff,
	    (val >> 16) & 0xff,
	    (val >>  8) & 0xff,
	    val & 0xff);
    return buf;
}

/*
** makeConnection
**
** Set up a socket connection to the specified host and port. The host
** name can either be a DNS name or a string IP address. If udpMode is
** true then a UDP socket is created but it is not "connected". If useProxy
** is true and a TCP connection is requested then we will try to connect
** via a HTTP proxy.
**
** If fromAddrP is not NULL then we will try to set the source address for
** the connection (not fatal if we fail). If toAddrP is not NULL then the
** address of the destination endpoint is returned.
*/

int
makeConnection(const char *host, const unsigned short port,
	       int udpMode, int useProxy,
	       struct sockaddr_in *fromAddrP, struct sockaddr_in *toAddrP)
{
    int sfd = -1;
    struct sockaddr_in addr;


    /* Sanity check */

    assert(host != NULL && port != 0);

    /*
    ** Check for connection via proxy and handle if necessary. This should
    ** only be applied to TCP connections between client and server.
    */

    if (!udpMode && useProxy)
    {
	/* Only try if a proxy has been set */

	if (ProxyHost && ProxyPort)
	{
	    return proxyConnection(host, port, toAddrP);
	}
    }

    /* Create the socket */

    if ((sfd = socket(AF_INET, (udpMode ? SOCK_DGRAM : SOCK_STREAM), 0)) < 0)
    {
	message(0, errno, "socket creation failed");
	errno = 0;
	return -1;
    }

    /*
    ** If a source address was specified, try to set it. This is not
    ** fatal if it fails -- not all platforms support it.
    */

#ifdef TCP_TPROXY_SRCADDR
    /*
    ** Transparent proxy functionality should probably work in Linux 2.0/2.2
    ** but will not work in 2.4 when things were changed. You can hack the
    ** kernel if you really want but TCP_TPROXY_SRCADDR should be the way to
    ** go in Linux post 2.4. From what I gather anyway ...
    */
#error "Time to implement transparent proxy using setsockopt(fd, SOL_TCP, TCP_TPROXY_SRCADDR, ...) now!"
#else
    if (fromAddrP && fromAddrP->sin_addr.s_addr)
    {
#ifdef USE_UDP_SPOOFING
	closesocket(sfd);
	if ((sfd = libnet_open_raw_sock(IPPROTO_RAW)) < 0)
	{
	    message(0, errno, "raw socket creation failed");
	    errno = 0;
	    return -1;
	}
#else
	memset(&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = fromAddrP->sin_addr.s_addr;
	if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
	    message(1, errno, "WARNING: failed to set connection source address -- ignored");
	}
#endif
    }
#endif

    /* Translate hostname from DNS or IP-address form */

    memset(&addr, 0, sizeof(addr));
    if (!getHostAddress(host, &addr, NULL, NULL))
    {
	message(0, 0, "can't resolve host or address '%s'", host);
	return -1;
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (!udpMode)
    {
	/* Set the "don't linger on close" option */

	setNoLinger(sfd);

	/* Now connect! */

	if (connect(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
	    closesocket(sfd);
	    return -1;
	}
    }

    /* If the address structure was requested then return it */

    if (toAddrP)
    {
	memcpy(toAddrP, &addr, sizeof(addr));
    }

    return sfd;
}

/*
** proxyConnection
**
** Make a connection to the specified host and port via an HTTP proxy
** supporting the CONNECT method. The toAddrP argument is passed on to
** the (recursive) makeConnection call. Note that ProxyHost must be non-NULL
** before this function is called.
**
** A strictly configured proxy server may not allow connection to arbitrary
** ports but only to that used by HTTPS (port 443). Also, in order to give
** proxy server owners some chance of blocking Zebedee if they wish to,
** the connect method header contains a "User-Agent: Zebedee" line. Unless
** someone has modified this code, that is :-)
*/

int
proxyConnection(const char *host, const unsigned short port,
		struct sockaddr_in *toAddrP)
{
    int fd = -1;
    char buf[MAX_LINE_SIZE + 1];
    int num = 0;
    int total = 0;
    char *bufP = NULL;


    assert(ProxyHost != NULL);

    /* Connect to the proxy server */

    message(4, 0, "connecting to %s:%hu via proxy %s:%hu", host, port, ProxyHost, ProxyPort);

    if ((fd = makeConnection(ProxyHost, ProxyPort, 0, 0, NULL, toAddrP)) == -1)
    {
	message(0, errno, "can't connect to proxy server at %s:%hu", ProxyHost, ProxyPort);
	return -1;
    }

    message(5, 0, "connected to proxy");

    /*
    ** Write the connect string. This includes a "User-Agent: Zebedee" line
    ** in order to help identify this connection as coming from Zebedee.
    ** This should be OK -- and conforms to the spec of the connect method
    ** as far as I can tell -- but may be rejected by some proxies. It may
    ** also be that no proxies currently look at or use this information
    ** but it is there if necessary.
    */

    buf[MAX_LINE_SIZE] = '\0';
    snprintf(buf, sizeof(buf) - 1, "CONNECT %s:%hu HTTP/1.0\r\nUser-Agent: Zebedee\r\n\r\n", host, port);
    if (send(fd, buf, strlen(buf), 0) <= 0)
    {
	message(0, errno, "failed writing to proxy server");
    }

    message(5, 0, "written connect string");

    /*
    ** We will now read the response from the proxy (up to MAX_LINE_SIZE
    ** bytes) and search for the header termination. This is two CR-LF
    ** pairs in succession. All proxies I have tried respond with less
    ** than this amount of data, although it is conceivable they might
    ** reply with more. If so, this will probably cause the Zebedee
    ** protocol exchange to fail.
    */

    bufP = buf;
    do
    {
	if ((num = recv(fd, bufP, (MAX_LINE_SIZE - total), 0)) <= 0)
	{
	    message(0, errno, "failed reading response from proxy");
	    closesocket(fd);
	    return -1;
	}
	total += num;
	bufP += num;
	*bufP = '\0';
	message(5, 0, "read %d bytes from proxy: %s", num, bufP - num);
    }
    while(total < MAX_LINE_SIZE && strncmp(bufP - 4, "\r\n\r\n", 4));

    /* Check for an OK response */

    if (strncmp(buf, "HTTP/1.0 200", 12))
    {
	if ((bufP = strchr(buf, '\r')) != NULL)
	{
	    *bufP = '\0';
	}
	message(0, 0, "proxy server refused connection to %s:%h (%s)", host, port, buf);
	closesocket(fd);
	return -1;
    }

    message(4, 0, "connection via proxy successful");

    return fd;
}

/*
** sendSpoofed
**
** Send a UDP packet to the address and port in toAddrP, purporting to
** originate from the address in fromAddrP.
*/

int
sendSpoofed(int fd, char *buf, int len, struct sockaddr_in *toAddrP, struct sockaddr_in *fromAddrP)
{
#ifdef USE_UDP_SPOOFING
    u_char *packet = NULL;
    int packetSize = 0;
    int num = -1;


    packetSize = LIBNET_IP_H + LIBNET_UDP_H + len;

    libnet_init_packet(packetSize, &packet);
    if (packet == NULL)
    {
	message(0, 0, "failed to allocate packet buffer");
	return -1;
    }

    /* Build IP packet header */

    libnet_build_ip(LIBNET_UDP_H + len,	/* Size beyond IP header */
		    0,			/* IP ToS */
		    rand() % 11965 + 1,	/* IP ID */
		    0,			/* Frag */
		    64,			/* TTL */
		    IPPROTO_UDP,	/* Transport protocol */
		    fromAddrP->sin_addr.s_addr,	/* Source address */
		    toAddrP->sin_addr.s_addr,	/* Destination address */
		    NULL,		/* Pointer to payload */
		    0,			/* Size */
		    packet);		/* Packet buffer */

    /* Add UDP packet header and payload */

    libnet_build_udp(ntohs(fromAddrP->sin_port),    /* Source port */
		     ntohs(toAddrP->sin_port),	    /* Dest port */
		     buf,		/* Payload */
		     len,		/* Payload size */
		     packet + LIBNET_IP_H);

    /* Do the checksum for the UDP header */

    if (libnet_do_checksum(packet, IPPROTO_UDP, LIBNET_UDP_H + len) == -1)
    {
	message(0, 0, "packet checksum failed");
	goto cleanup;
    }

    /* Write the packet */

    num = libnet_write_ip(fd, packet, packetSize);
    if (num < packetSize)
    {
	message(1, 0, "Warning: short packet write (%d < %d)", num, packetSize);
    }
    num -= (LIBNET_IP_H + LIBNET_UDP_H);

cleanup:
    libnet_destroy_packet(&packet);
    return num;
#else
    return -1;
#endif
}

/*
** makeListener
**
** Set up a listener socket on the port supplied via portP. If listenIp
** is not NULL then it specifies the address on which we will listen.
**
** If the requested port is 0 then a new port will be allocated and
** returned in portP. If the requested port is non-zero then an attempt
** will be made to re-use the address if possible.
**
** The routine returns the socket ID on success or -1 on error.
*/

int
makeListener(unsigned short *portP, char *listenIp, int udpMode)
{
    int sfd = -1;
    struct sockaddr_in addr;
    int addrLen = sizeof(addr);
    int trueVal = 1;
    char ipBuf[IP_BUF_SIZE];


    /* Create the socket */

    if ((sfd = socket(AF_INET, (udpMode ? SOCK_DGRAM: SOCK_STREAM), 0)) < 0)
    {
	message(0, errno, "can't create listener socket");
	goto failure;
    }

    /* If we requested a specific port then reuse the address if possible */

    if (portP && *portP)
    {
	if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (char *)&trueVal, sizeof(trueVal)) < 0)
	{
	    message(1, 0, "Warning: failed to set SO_REUSEADDR option on socket");
	}
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (listenIp != NULL)
    {
	if (!getHostAddress(listenIp, &addr, NULL, NULL))
	{
	    message(0, 0, "can't resolve listen address '%s'", listenIp);
	}
    }
    addr.sin_family = AF_INET;
    addr.sin_port = (portP ? htons(*portP) : 0);
    message(5, 0, "listening on %s", ipString(addr.sin_addr, ipBuf));

    if (bind(sfd, (struct sockaddr *)&addr, addrLen) < 0)
    {
	message(0, errno, "listener bind failed");
	goto failure;
    }

    if (!udpMode)
    {
	if (listen(sfd, MAX_LISTEN) < 0)
	{
	    message(0, errno, "listen failed");
	    goto failure;
	}
    }

    if (portP)
    {
	/* Retrieve the port actually being used to return via portP */

	memset(&addr, 0, sizeof(addr));
	if (getsockname(sfd, (struct sockaddr *)&addr, &addrLen))
	{
	    message(0, errno, "can't get local port number");
	    goto failure;
	}
	*portP = ntohs(addr.sin_port);
    }

    return sfd;

failure:
    if (sfd != -1)
    {
	(void)closesocket(sfd);
    }
    errno = -1;
    return -1;
}

/*
** setNoLinger
**
** Turn off "linger on close" behaviour for a socket.
*/

void
setNoLinger(int fd)
{
    struct linger lingerVal;

    lingerVal.l_onoff = 0;
    lingerVal.l_linger = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER,
		   (char *)&lingerVal, sizeof(lingerVal)) < 0)
    {
	message(1, 0, "Warning: failed to set SO_LINGER option on socket");
    }
}

/*
** setKeepAlive
**
** Turn on "keep alives" for a socket.
*/

void
setKeepAlive(int fd)
{
    int trueVal = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
		   (char *)&trueVal, sizeof(trueVal)) < 0)
    {
	message(1, 0, "Warning: failed to set SO_KEEPALIVE option on socket");
    }
}

/*
** acceptConnection
**
** Accept a connection on the specified listenFd. If the host is "*" then
** a connection from any source will be accepted otherwise the source
** address must match that obtained from the forward lookup of the host
** name (which may be a range of addresses if it contains an address mask).
**
** If the loop parameter is true then the routine will wait until a "good"
** connection has been accepted otherwise it will stop on the first error.
** The timeout parameter sets the maximum number of seconds that the
** routine will wait.
**
** On success return the socket number otherwise -1.
*/

int
acceptConnection(int listenFd, const char *host, int loop, unsigned short timeout)
{
    struct sockaddr_in fromAddr;
    struct sockaddr_in hostAddr;
    int addrLen;
    int serverFd = -1;
    struct timeval delay;
    fd_set testSet;
    int ready;
    struct in_addr *addrList = NULL;
    struct in_addr *addrPtr = NULL;
    unsigned long mask = 0xffffffff;
    char ipBuf[IP_BUF_SIZE];


    memset(&hostAddr, 0, sizeof(hostAddr));
    if (strcmp(host, "*") == 0)
    {
	mask = 0;
	hostAddr.sin_addr.s_addr = 0;
    }
    else
    {
	if (!getHostAddress(host, &hostAddr, &addrList, &mask))
	{
	    message(0, 0, "can't resolve host or address '%s'", host);
	    closesocket(serverFd);
	    errno = 0;
	    return -1;
	}
    }

    while (1)
    {
	message(3, 0, "waiting to accept connection");

	delay.tv_sec = timeout;
	delay.tv_usec = 0;

	FD_ZERO(&testSet);
	FD_SET(listenFd, &testSet);

	ready = select(listenFd + 1, &testSet, 0, 0, &delay);

	if (ready == 0)
	{
	    message(0, 0, "timed out waiting to accept connection");
	    goto failure;
	}

	/* Check for error but ignore interrupted system calls */

	if (ready < 0 && errno != EINTR)
	{
	    if (errno != EINTR)
	    {
		message(0, errno, "error in select waiting for client to accept connection");
		goto failure;
	    }
	    else
	    {
		continue;
	    }
	}

	/* Attempt to accept the connection */

	addrLen = sizeof(struct sockaddr_in);
	memset(&fromAddr, 0, sizeof(fromAddr));
	if ((serverFd = accept(listenFd,
			       (struct sockaddr *)&fromAddr,
			       &addrLen)) < 0)
	{
	    /* This is always an error, looping or not */
	    goto failure;
	}

	/*
	** Check the received connection address against the specified
	** server host name (applying a network mask as ppropriate).
	*/

	if ((fromAddr.sin_addr.s_addr & mask) ==
	    (hostAddr.sin_addr.s_addr & mask))
	{
	    /* We've got a straight match */
	    break;
	}
	else
	{
	    /* Try the alias addresses */

	    for (addrPtr = addrList; addrPtr->s_addr != 0xffffffff; addrPtr++)
	    {
		if ((fromAddr.sin_addr.s_addr &mask) ==
		    (addrPtr->s_addr & mask))
		{
		    break;
		}
	    }

	    if (addrPtr->s_addr != 0xffffffff)
	    {
		/* We got a match -- break enclosing loop */
		break;
	    }
	}

	message(1, 0, "Warning: connection from %s rejected, does not match server host %s",
		ipString(fromAddr.sin_addr, ipBuf), host);
	closesocket(serverFd);
	errno = 0;
	if (!loop)
	{
	    goto failure;
	}
    }

    /* Free memory allocated by getHostAddress */

    if (addrList)
    {
	free(addrList);
    }

    message(3, 0, "accepted connection from %s", ipString(fromAddr.sin_addr, ipBuf));

    /*
    ** Set the "don't linger on close" and "keep alive" options. The latter
    ** will (eventually) reap defunct connections.
    */

    setNoLinger(serverFd);
    setKeepAlive(serverFd);

    return serverFd;

failure:
    if (addrList) free(addrList);
    return -1;
}

/*
** headerSetUShort
**
** Set the specified unsigned short value into the protocol header buffer
** (hdrBuf) at the specified byte offset.
*/

void
headerSetUShort(unsigned char *hdrBuf, unsigned short value, int offset)
{
    hdrBuf[offset] = (value >> 8) & 0xff;
    hdrBuf[offset + 1] = value & 0xff;
}

/*
** headerSetULong
**
** Set the specified unsigned long (32-bit) value into the protocol header
** buffer (hdrBuf) at the specified byte offset.
*/

void
headerSetULong(unsigned char *hdrBuf, unsigned long value, int offset)
{
    hdrBuf[offset] = (value >> 24) & 0xff;
    hdrBuf[offset + 1] = (value >> 16) & 0xff;
    hdrBuf[offset + 2] = (value >> 8) & 0xff;
    hdrBuf[offset + 3] = value & 0xff;
}

/*
** headerGetUShort
**
** Retrieve an unsigned short value from the protocol header buffer
** (hdrBuf) at the specified byte offset.
*/

unsigned short
headerGetUShort(unsigned char *hdrBuf, int offset)
{
    return (((unsigned short)hdrBuf[offset]) << 8) + (unsigned short)hdrBuf[offset + 1];
}

/*
** headerGetUShort
**
** Retrieve an unsigned long (32-bit) value from the protocol header buffer
** (hdrBuf) at the specified byte offset.
*/

unsigned long
headerGetULong(unsigned char *hdrBuf, int offset)
{
    return  (((unsigned long)hdrBuf[offset]) << 24) +
	    (((unsigned long)hdrBuf[offset + 1]) << 16) +
	    (((unsigned long)hdrBuf[offset + 2]) << 8) +
	    (unsigned long)hdrBuf[offset + 3];
}

/*********************************\
**				 **
**  Encryption-related Routines  **
**				 **
\*********************************/

/*
** setupBlowfish
**
** Create and initialise a BFState_t structure to hold the encryption
** context for one communication stream (A -> B or B -> A but not both).
**
** keyStr is the key data which is in the form of a string of hexadecimal
** digits. The actual key used is the high-order keyBits bits of this
** number.
**
** The routine returns a pointer to the newly-allocated structure on success
** or NULL on error. As a special case, if the key length is zero then no
** encryption is needed and we also return NULL.
*/

BFState_t *
setupBlowfish(char *keyStr, unsigned short keyBits)
{
    BFState_t *bf = NULL;
    unsigned char keyData[MAX_KEY_BYTES];
    int keyBytes;


    /* Special case -- no encryption requeste */

    if (keyBits == 0) return NULL;

    /* Now allocate the necessary space */

    if ((bf = (BFState_t *)malloc(sizeof(BFState_t))) == NULL)
    {
	message(0, errno, "out of memory allocating Blowfish state data");
	errno = 0;
	return NULL;
    }

    keyBytes = hexStrToBits(keyStr, keyBits, keyData);

    memset(bf, 0, sizeof(BFState_t));
    BF_set_key(&(bf->key), keyBytes, keyData);
    memcpy(bf->iVec, INIT_IVEC, 8);
    bf->pos = 0;

    return bf;
}

/*
** generateKey
**
** Generate the exponent (private key) for the Diffie-Hellman key
** exchange.
**
** Good key generation is crucial for the security of the encryption
** mechanism. Ideally we would generate keys based on truly random
** sources of data -- like radioactive decay. Unfortunately this is
** not tremendously practical so we have to do the best we can to
** generate keys that will be hard to predict. How well we can achieve
** this depends somewhat on the operating system on which the program
** is running. See the inline comments below.
**
** In the comments we will try to set some bounds on the number of
** "bits of uncertainty" (BOU) -- that is how many bits' worth of
** imprecision there is in determining various quantities for an attacker.
** The "Min BOU" is where an attacker has access to the system and can
** examine system performance counters etc. The "Max BOU" is where there
** is no such access and all the attacker has access to is the data "on
** the wire". Please note that these are estimates, NOT guarantees!
**
** Basically, if you are happy that an attacker can not see the state of
** your system when the key is generated (either through a direct login
** or via remote administrative interfaces) then the algorithms below
** are PROBABLY sufficient but in other cases you will need a
** different approach.
**
** If you are not happy with these key generation mechanisms then
** you can call out to an external program. This needs to generate a
** single string of hexadecimal digits (at least MIN_KEY_BYTES long)
** which will be used as the key.
*/

char *
generateKey(void)
{
    SHA_INFO sha;
    time_t now = time(NULL);
    unsigned long pid = threadPid();
    unsigned long tid = threadTid();
    char *result = NULL;


    /* If a private key has been supplied copy it and return it */

    if (PrivateKey)
    {
	if ((result = (char *)malloc(strlen(PrivateKey) + 1)) == NULL)
	{
	    return NULL;
	}
	strcpy(result, PrivateKey);
	return result;
    }

    /*
    ** If a key generator command was specified then use this to generate
    ** the key rather than doing it directly here. If the generator fails,
    ** however, then we will fall back to the inline method.
    */

    if (KeyGenCmd && (result = runKeyGenCommand(KeyGenCmd)) != NULL)
    {
	return result;
    }

    /*
    ** We use SHA to "stir" whatever bits of "entropy" we can acquire.
    ** This distributes the input very well over 160 bits.
    */

    sha_init(&sha);

    /*
    ** In all cases add the current time and process and thread IDs.
    ** The time can be guessed by an attacker to within a second. With
    ** physical access to the machine the PID and probably TID can be
    ** obtained with certainty. Even without physical access on UNIX
    ** the uncertainty in the PID and TID is less than 16 bits. On Win32
    ** it seems even less.
    **
    ** Min BOU: 1
    ** Max BOU: 16
    */

    sha_update(&sha, (SHA_BYTE *)&now, sizeof(now));
    sha_update(&sha, (SHA_BYTE *)&pid, sizeof(pid));
    sha_update(&sha, (SHA_BYTE *)&tid, sizeof(tid));

#if defined(WIN32)
    {
	LARGE_INTEGER perf;
	FILETIME created, exited, kernel, user;
	LONG val;
	POINT point;
	MEMORYSTATUS memoryStatus;

	/*
	** Add in a large number of reasonable hard to guess (from the
	** outside) values. Someone with access to the machines may,
	** however be able to pinpoint these with some accuracy. We will
	** assume a maximum of 8 BOU for each call an a minimum of 1.
	**
	** Min BOU: 13
	** Max BOU: 104
	*/

#define ADDLONGVAL(func) val = ((LONG)func()); sha_update(&sha, (SHA_BYTE *)&val, sizeof(val))
	ADDLONGVAL(GetActiveWindow);
	ADDLONGVAL(GetCapture);
	ADDLONGVAL(GetClipboardOwner);
	ADDLONGVAL(GetClipboardViewer);
	ADDLONGVAL(GetDesktopWindow);
	ADDLONGVAL(GetFocus);
	ADDLONGVAL(GetInputState);
	ADDLONGVAL(GetMessagePos);
	ADDLONGVAL(GetMessageTime);
	ADDLONGVAL(GetOpenClipboardWindow);
	ADDLONGVAL(GetProcessHeap);
	ADDLONGVAL(GetProcessWindowStation);
	ADDLONGVAL(GetTickCount);

	/*
	** QueryPerformanceCounter gives a very high resolution 64-bit
	** time result. Unfortunately, if there is no hardware support
	** for a high-resolution timer it can return zero. On hardware
	** I have available the resolution is over 1 million counts per
	** second.
	**
	** Assume, in the worst case that the process start time can
	** be determined with millisecond accuracy.
	**
	** Min BOU: 10
	** Max BOU: 64
	*/

	(void)QueryPerformanceCounter(&perf);

	sha_update(&sha, (SHA_BYTE *)&perf, sizeof(perf));

	/*
	** The following quantities are 64 bit times in 100nsec
	** intervals since Jan 1, 1601. They are available to be
	** read by other suitably privileged processes. I'm not sure
	** of the resolution and only the kernel and user times
	** have any degree of unpredictability from "outside" so
	** we will make conservative estimates.
	**
	** Min BOU: 4
	** Max BOU: 32
	*/

	GetProcessTimes(GetCurrentProcess(),
			&created, &exited, &kernel, &user);

	sha_update(&sha, (SHA_BYTE *)&created, sizeof(created));
	sha_update(&sha, (SHA_BYTE *)&exited, sizeof(exited));
	sha_update(&sha, (SHA_BYTE *)&kernel, sizeof(kernel));
	sha_update(&sha, (SHA_BYTE *)&user, sizeof(user));

	/*
	** Current caret and cursor positon. Maybe somewhere in a 800x600
	** area ... but known to an attacker with physical access.
	**
	** Min BOU: 0
	** Max BOU: 175
	*/

	GetCaretPos(&point);
	sha_update(&sha, (SHA_BYTE *)&point, sizeof(point));
	GetCursorPos( &point );
	sha_update(&sha, (SHA_BYTE *)&point, sizeof(point));

	/*
	** Memory usage statistics -- percent of memory in use, bytes of
	** physical memory, bytes of free physical memory, bytes in paging
	** file, free bytes in paging file, user bytes of address space,
	** and free user bytes. Even to an attacker with physical access
	** there is likely to be some uncertainty here, but maybe only
	** a bit per variable quantity.
	**
	** Min BOU: 3
	** Max BOU: 20+
	*/

	memoryStatus.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus(&memoryStatus);
	sha_update(&sha, (SHA_BYTE *)&memoryStatus, sizeof(memoryStatus));

	/*
	** Total estimates for Win32
	**
	** Min BOU: 31, Max BOU: 400+
	*/
    }
#else	/* !WIN32 */
    {
	clock_t ticks;
	struct tms tms;

	/*
	** On all UNIX systems we get the process time stats. These are
	** 32-bit quantities relative to system boot.
	**
	** Min BOU: 2
	** Max BOU: 40
	*/

	ticks = times(&tms);

	sha_update(&sha, (SHA_BYTE *)&ticks, sizeof(ticks));
	sha_update(&sha, (SHA_BYTE *)&tms, sizeof(tms));
    }

    if (KeyGenLevel == 2)
    {
	/*
	** Now we're talking! /dev/random uses internal kernel counters
	** and state not accessible to normal users. We will read 10 chunks
	** of 8 bytes which should give us more than enough to justify
	** claiming that we have a full 160 bits of uncertainty coming
	** out of the final hash.
	**
	** If you look closely we actually try /dev/urandom in preference
	** to /dev/random. On Linux, although it is theoretically less
	** secure, it will not block waiting for "enough" entropy unlike
	** /dev/random.
	**
	** BOU: 160+
	*/

	int fd = open("/dev/urandom", O_RDONLY);
	char buffer[8];
	int i;

	if (fd == -1 && (fd = open("/dev/random", O_RDONLY)) == -1)
	{
	    message(3, 0, "can't open /dev/urandom or /dev/random -- downgrading keygenlevel");
	    KeyGenLevel--;
	}
	else
	{
	    for (i = 0; i < 10; i++)
	    {
		read(fd, buffer, 8);
		sha_update(&sha, (SHA_BYTE *)buffer, 8);
	    }
	    close(fd);
	}
    }

    if (KeyGenLevel == 1)
    {
	/*
	** If we haven't got /dev/random but do have /proc then we can
	** probably do quite well on an active system. We stat every
	** process and hash that data in. On a very stable system this
	** could, however, be fairly predictable to an attacker with
	** access to the system.
	**
	** Min BOU: 4
	** Max BOU: 160+
	*/

	struct stat sbuf;
	struct dirent *entryP;
	DIR *dir = opendir("/proc");
	char name[MAX_LINE_SIZE];


	if (dir == NULL)
	{
	    message(4, 0, "can't open /proc -- downgrading keygenlevel");
	    KeyGenLevel--;
	}
	else
	{
	    while ((entryP = readdir(dir)) != NULL)
	    {
		snprintf(name, sizeof(name), "/proc/%s", entryP->d_name);
		stat(name, &sbuf);
		sha_update(&sha, (SHA_BYTE *)entryP, sizeof(struct dirent));
		sha_update(&sha, (SHA_BYTE *)&sbuf, sizeof(sbuf));
	    }
	    closedir(dir);
	}
    }
#endif	/* !WIN32 */

    /* Convert the digest to a string and return */

    sha_final(&sha);

    /* Exclude REALLY bad keys */

    if (sha.digest[0] == 0 && sha.digest[1] == 0 &&
	sha.digest[2] == 0 && sha.digest[3] == 0 &&
	(sha.digest[4] == 0 || sha.digest[4] == 1))
    {
	return generateKey();
    }
	
    if ((result = (char *)malloc(HASH_STR_SIZE)) == NULL)
    {
	return NULL;
    }

    sprintf(result, "%08lx%08lx%08lx%08lx%08lx",
	    (unsigned long)sha.digest[0], (unsigned long)sha.digest[1],
	    (unsigned long)sha.digest[2], (unsigned long)sha.digest[3],
	    (unsigned long)sha.digest[4]);

    return result;
}

/*
** runKeyGenCommand
**
** This runs the specified key generation command, if any. It reads a single
** line from the command's standard output and extracts a hex string from
** it. The string must be at least MIN_KEY_BYTES * 2 bytes long.
**
** If the routine succeeds it malloc's space for the string and returns
** it otherwise it returns NULL.
*/

char *
runKeyGenCommand(char *keyGenCmd)
{
    FILE *fp;
    char buf[MAX_LINE_SIZE];
    char *result = NULL;


    if (keyGenCmd == NULL)
    {
	message(3, 0, "no key generation command specified");
	return NULL;
    }

    if ((fp = popen(keyGenCmd, "r")) == NULL)
    {
	message(0, errno, "failed to spawn key generation command '%s'", keyGenCmd);
	return NULL;
    }

    if (fgets(buf, MAX_LINE_SIZE, fp) != NULL)
    {
	if ((result = (char *)malloc(strlen(buf) + 1)) != NULL)
	{
	    if (sscanf(buf, "%[0-9a-fA-F]", result) != 1 ||
		strlen(buf) < (MIN_KEY_BYTES * 2))
	    {
		free(result);
		result = NULL;
	    }
	}
    }

    fclose(fp);

    message(3, 0, "key generation result %s", (result ? "not null" : "NULL"));
    return result;
}

/*
** generateNonce
**
** Generate a 64-bit nonce value. This is a cut-down version of generateKey.
** It does not need the same strength of randomness because these are
** public values.
**
** See the comments in generateKey for explanations of the various things
** going on in here.
*/

void
generateNonce(unsigned char *nonce)
{
    SHA_INFO sha;
    time_t now = time(NULL);
    unsigned long pid = threadPid();
    unsigned long tid = threadTid();
    int i;

    sha_init(&sha);

    sha_update(&sha, (SHA_BYTE *)&now, sizeof(now));
    sha_update(&sha, (SHA_BYTE *)&pid, sizeof(pid));
    sha_update(&sha, (SHA_BYTE *)&tid, sizeof(tid));

#if defined(WIN32)
    {
	LARGE_INTEGER perf;
	DWORD ticks = timeGetTime();
	FILETIME created, exited, kernel, user;


	sha_update(&sha, (SHA_BYTE *)&ticks, sizeof(ticks));

	(void)QueryPerformanceCounter(&perf);
	sha_update(&sha, (SHA_BYTE *)&perf, sizeof(perf));

	GetProcessTimes(GetCurrentProcess(),
			&created, &exited, &kernel, &user);

	sha_update(&sha, (SHA_BYTE *)&created, sizeof(created));
	sha_update(&sha, (SHA_BYTE *)&exited, sizeof(exited));
	sha_update(&sha, (SHA_BYTE *)&kernel, sizeof(kernel));
	sha_update(&sha, (SHA_BYTE *)&user, sizeof(user));
    }
#else	/* !WIN32 */
    {
	clock_t ticks;
	struct tms tms;

	ticks = times(&tms);

	sha_update(&sha, (SHA_BYTE *)&ticks, sizeof(ticks));
	sha_update(&sha, (SHA_BYTE *)&tms, sizeof(tms));
    }
#endif	/* !WIN32 */

    sha_final(&sha);

    for (i = 0; i < NONCE_SIZE; i++)
    {
	nonce[i] = (sha.digest[i / 4] >> ((i % 4) * 8)) & 0xff;
    }
}

/*
** generateSessionKey
**
** Given the DH-derived secret key string and the client and server
** nonces generate a unique session key at least "keyBits" bits long.
*/

char *
generateSessionKey(char *secretKey, unsigned char *cNonce,
		   unsigned char *sNonce, unsigned short keyBits)
{
    SHA_INFO sha;
    unsigned short bits = 0;
    unsigned short nybbles = 0;
    unsigned short len = (unsigned short)strlen(secretKey);
    char *result = NULL;


    for (bits = 0; bits < keyBits; bits += 160)
    {
	sha_init(&sha);
	sha_update(&sha, (SHA_BYTE *)cNonce, NONCE_SIZE);
	sha_update(&sha, (SHA_BYTE *)sNonce, NONCE_SIZE);
	nybbles = bits / 4;
	if (nybbles > len)
	{
	    nybbles %= len;
	}
	sha_update(&sha, (SHA_BYTE *)(secretKey + nybbles), (len - nybbles));
	sha_final(&sha);

	if ((result = (char *)realloc(result, (bits / 160 + 1) * 40 + 1)) == NULL)
	{
	    return NULL;
	}

	sprintf(result + (bits / 160) * 40,
		"%08lx%08lx%08lx%08lx%08lx",
		(unsigned long)sha.digest[0], (unsigned long)sha.digest[1],
		(unsigned long)sha.digest[2], (unsigned long)sha.digest[3],
		(unsigned long)sha.digest[4]);
    }

    return result;
}

/*
** hexStrToBits
**
** This converts the first "bits" bits of hex string "hexStr" to a
** bit vector in "bitVec", which must be at least MAX_KEY_BYTES bytes
** long. It returns the number of bytes of bitVec that were set.
**
** Note that "bits" is effectively rounded up to the nearest multiple
** of 4.
*/

unsigned short
hexStrToBits(char *hexStr, unsigned short bits, unsigned char *bitVec)
{
    int i;
    unsigned char byte;
    int len = strlen(hexStr);


    memset(bitVec, 0, MAX_KEY_BYTES);

    /* Determine number of nybbles required */

    if ((int)((bits + 3) / 4U) < len)
    {
	len = (int)((bits + 3) / 4U);
    }

    /* Truncate the number of nybbles to fit in the buffer, if necessary */

    if (len > (MAX_KEY_BYTES * 2))
    {
	len = MAX_KEY_BYTES * 2;
    }

    /* Now process the string a nybble at a time */

    for (i = 0; i < len; i += 2)
    {
	byte = '\0';

	/* High nybble */

	if (hexStr[i] >= '0' && hexStr[i] <= '9')
	{
	    byte = (hexStr[i] - '0') << 4;
	}
	else if (toupper(hexStr[i]) >= 'A' && toupper(hexStr[i]) <= 'F')
	{
	    byte = (toupper(hexStr[i]) - 'A' + 0xA) << 4;
	}

	/* Low nybble -- if any, otherwise left as zero */

	if (i + 1 < len)
	{
	    if (hexStr[i + 1] >= '0' && hexStr[i + 1] <= '9')
	    {
		byte |= (hexStr[i + 1] - '0');
	    }
	    else if (toupper(hexStr[i + 1]) >= 'A' && toupper(hexStr[i + 1]) <= 'F')
	    {
		byte |= (toupper(hexStr[i + 1]) - 'A' + 0xA);
	    }
	}

	bitVec[i / 2] = byte;
    }

    /* Return number of bytes */

    return (unsigned short)(i / 2);
}

/*
** diffieHellman
**
** Perform the core Diffie-Hellman calculation which is
**
**	(generator ** exponent) mod modulus
**
** This operates on hex strings and returns a newly-allocated hex string
** as its answer. If genStr or modStr are NULL or empty strings then
** the default, built-in values will be used.
*/

char *
diffieHellman(char *genStr, char *modStr, char *expStr)
{
    mpz_t gen, mod, exp, key;
    char *keyStr = NULL;


    if (genStr == NULL || *genStr == '\0') genStr = DFLT_GENERATOR;
    if (modStr == NULL || *modStr == '\0') modStr = DFLT_MODULUS;

    mpz_init_set_str(gen, genStr, 16);
    mpz_init_set_str(mod, modStr, 16);
    mpz_init_set_str(exp, expStr, 16);
    mpz_init(key);

    mpz_powm(key, gen, exp, mod);

    keyStr = mpz_get_str(NULL, 16, key);

    mpz_clear(gen);
    mpz_clear(exp);
    mpz_clear(mod);
    mpz_clear(key);

    return keyStr;
}

/*
** makeChallenge
**
** Create a challenge value and write it into "challenge", which
** must be CHALLENGE_SIZE bytes long. We will just use clock ticks.
*/

void
makeChallenge(unsigned char *challenge)
{
#ifdef WIN32
    DWORD ticks = timeGetTime();
    memcpy(challenge, &ticks, CHALLENGE_SIZE);
#else
    struct tms tms;
    clock_t ticks = times(&tms);
    memcpy(challenge, &ticks, CHALLENGE_SIZE);
#endif
}

/*
** challengeAnswer
**
** Calculate the answer to the challenge. XOR each byte with THE_ANSWER.
*/

void
challengeAnswer(unsigned char *challenge)
{
    int i;

    for (i = 0; i < CHALLENGE_SIZE; i++)
    {
	challenge[i] ^= THE_ANSWER;
    }
}

/*
** clientPerformChallenge
**
** This the client side of the challenge-response dialogue used to establish
** that both client and server really do know the shared secret key and
** guard against replay attacks.
**
** It returns 1 if the dialogue is successfully completed and 0 otherwise.
*/

int
clientPerformChallenge(int serverFd, MsgBuf_t *msg)
{
    unsigned char challenge[CHALLENGE_SIZE];
    unsigned char myChallenge[CHALLENGE_SIZE];


    /* Read encrypted challenge string from the server */

    message(3, 0, "reading challenge from server");

    if (readMessage(serverFd, msg, CHALLENGE_SIZE) <= 0)
    {
	message(0, errno, "failed to read challenge from server");
	return 0;
    }
    getMsgBuf(msg, challenge, CHALLENGE_SIZE);

    message(3, 0, "read challenge");

    /* Calculate the answer and then send that back to the server */

    challengeAnswer(challenge);

    message(3, 0, "writing challenge response");

    setMsgBuf(msg, challenge, CHALLENGE_SIZE);
    if (writeMessage(serverFd, msg) != CHALLENGE_SIZE)
    {
	message(0, errno, "failed writing challenge response to server");
	return 0;
    }
    message(3, 0, "wrote challenge response");

    /* Now generate our own challenge value and send it to the server */

    makeChallenge(myChallenge);

    message(3, 0, "sending challenge to server");

    setMsgBuf(msg, myChallenge, CHALLENGE_SIZE);
    if (writeMessage(serverFd, msg) != CHALLENGE_SIZE)
    {
	message(0, errno, "failed writing challenge to server");
	return 0;
    }
    message(3, 0, "wrote challenge");

    message(3, 0, "reading challenge response from server");

    if (readMessage(serverFd, msg, CHALLENGE_SIZE) <= 0)
    {
	message(0, errno, "failed to read challenge response from server");
	return 0;
    }
    getMsgBuf(msg, challenge, CHALLENGE_SIZE);

    message(3, 0, "read challenge response");

    /* Calculate the expected answer and then compare with the response */

    challengeAnswer(myChallenge);

    if (memcmp(challenge, myChallenge, CHALLENGE_SIZE) != 0)
    {
	message(0, 0, "server responded incorrectly to challenge");
	return 0;
    }

    memset(challenge, 0, CHALLENGE_SIZE);
    memset(myChallenge, 0, CHALLENGE_SIZE);

    return 1;
}

/*
** serverPerformChallenge
**
** This the server side of the challenge-response dialogue used to establish
** that both client and server really do know the shared secret key and
** guard against replay attacks.
**
** It returns 1 if the dialogue is successfully completed and 0 otherwise.
*/

int
serverPerformChallenge(int clientFd, MsgBuf_t *msg)
{
    unsigned char challenge[CHALLENGE_SIZE];
    unsigned char myChallenge[CHALLENGE_SIZE];


    /* Generate our own challenge value */

    makeChallenge(myChallenge);

    message(3, 0, "sending challenge to client");

    setMsgBuf(msg, myChallenge, CHALLENGE_SIZE);
    if (writeMessage(clientFd, msg) != CHALLENGE_SIZE)
    {
	message(0, errno, "failed writing challenge to client");
	return 0;
    }
    message(3, 0, "wrote challenge");

    message(3, 0, "reading challenge response from client");

    if (readMessage(clientFd, msg, CHALLENGE_SIZE) <= 0)
    {
	message(0, errno, "failed to read challenge response from client");
	return 0;
    }
    getMsgBuf(msg, challenge, CHALLENGE_SIZE);

    message(3, 0, "read challenge response");

    /* Calculate the expected answer and then compare with the response */

    challengeAnswer(myChallenge);

    if (memcmp(challenge, myChallenge, CHALLENGE_SIZE) != 0)
    {
	message(0, 0, "client responded incorrectly to challenge");
	return 0;
    }

    /* Now read challenge from the client */

    message(3, 0, "reading challenge from client");

    if (readMessage(clientFd, msg, CHALLENGE_SIZE) <= 0)
    {
	message(0, errno, "failed to read challenge from client");
	return 0;
    }
    getMsgBuf(msg, challenge, CHALLENGE_SIZE);

    message(3, 0, "read challenge");

    /* Calculate the answer and then send that back to the client */

    challengeAnswer(challenge);

    message(3, 0, "writing challenge response");

    setMsgBuf(msg, challenge, CHALLENGE_SIZE);
    if (writeMessage(clientFd, msg) != CHALLENGE_SIZE)
    {
	message(0, errno, "failed writing challenge response to client");
	return 0;
    }
    message(3, 0, "wrote challenge response");
    memset(challenge, 0, CHALLENGE_SIZE);

    return 1;
}

/************************************\
**				    **
**  Reuseable session key routines  **
**				    **
\************************************/

/*
** freeKeyInfo
**
** Free the storage associated with a KeyInfo_t element.
*/

void
freeKeyInfo(KeyInfo_t *info)
{
    if (info->key)
    {
	memset(info->key, 0, strlen(info->key));
	free(info->key);
    }
    free(info);
}

/*
** findKeyByToken
**
** Given a token value, search the specified list for a matching value
** and return the key string associated with it, or NULL if not found.
** The key is returned in newly-allocated storage which must be freed
** by the caller.
**
** As a side effect this also checks the expiry time of each entry an
** purges expired entries from the list. Note that the head of the list
** is always a static entry (with ptr->expiry zero). This makes the list
** manipulation easier.
**
** If a static shared key or key generation command has been specified
** then this will be used and the value returned regardless of token
** value specified.
*/

char *
findKeyByToken(KeyInfo_t *list, unsigned long token)
{
    KeyInfo_t *ptr = NULL;
    char *found = NULL;
    KeyInfo_t *tmp = NULL;
    time_t now;
    char *result = NULL;


    /* If a shared private key has been supplied copy it and return it */

    if (SharedKey)
    {
	if ((result = (char *)malloc(strlen(SharedKey) + 1)) == NULL)
	{
	    return NULL;
	}
	strcpy(result, SharedKey);
	return result;
    }

    /*
    ** If a key generator command was specified then use this to generate
    ** the key rather than doing it directly here.
    */

    if (SharedKeyGenCmd && (result = runKeyGenCommand(SharedKeyGenCmd)) != NULL)
    {
	return result;
    }

    if (token == 0 || token == TOKEN_NEW) return NULL;

    time(&now);

    mutexLock(MUTEX_KEYLIST);

    for (ptr = list; ptr; ptr = ptr->next)
    {
	/* Check if the entry has expired */

	if (ptr->expiry && now > ptr->expiry)
	{
	    /* It has, so remove it! */

	    ptr->prev->next = ptr->next;
	    if (ptr->next)
	    {
		ptr->next->prev = ptr->prev;
	    }
	    tmp = ptr;
	    ptr = ptr->prev;
	    freeKeyInfo(tmp);
	}
	else if (!found && ptr->token == token)
	{
	    /* We have a matching element, copy the key to return */

	    if ((found = (char *)malloc(strlen(ptr->key) + 1)) == NULL)
	    {
		message(0, errno, "Out of memory allocating copy of key");
	    }
	    else
	    {
		strcpy(found, ptr->key);
	    }

	    /* Carry on with the loop to purge expired entries */
	}
    }

    mutexUnlock(MUTEX_KEYLIST);

    return found;
}

/*
** addKeyInfoToList
**
** This adds a new entry to the end of the specified list with the
** given token and key values. The entry will expire in a number of
** seconds given by the KeyLifetime global variable.
*/

void
addKeyInfoToList(KeyInfo_t *list, unsigned long token, char *key)
{
    KeyInfo_t *new = NULL;
    KeyInfo_t *ptr = NULL;


    mutexLock(MUTEX_KEYLIST);

    if ((new = (KeyInfo_t *)malloc(sizeof(KeyInfo_t))) == NULL ||
	(new->key = (char *)malloc(strlen(key) + 1)) == NULL)
    {
	message(0, errno, "Out of memory allocating key info element");
    }
    else
    {
	strcpy(new->key, key);
	new->token = token;
	new->next = NULL;
	time(&(new->expiry));
	new->expiry += KeyLifetime;

	for (ptr = list; ptr->next; ptr = ptr->next) /* move on ... */;

	ptr->next = new;
	new->prev = ptr;
    }

    mutexUnlock(MUTEX_KEYLIST);
}

/*
** generateToken
**
** This routine generates a new token value. It will explicitly avoid
** generating the same value as oldToken.
*/

unsigned long
generateToken(KeyInfo_t *list, unsigned long oldToken)
{
    static unsigned long nextToken = 0;
    unsigned long token = 0;
    char *key = NULL;


    if (KeyLifetime == 0) return 0;

    mutexLock(MUTEX_TOKEN);

    /* First time, set new random seed and generate first token */

    if (nextToken == 0)
    {
	srand((int)threadPid() + (int)time(NULL));
	nextToken = (unsigned long)((rand() & 0xffff) << 16);
	nextToken |= (unsigned long)(rand() & 0xffff);
    }

    while (token == 0)
    {
	nextToken = (nextToken + 1) & 0xffffffff;   /* Cope with 64-bit! */

	/* Avoid special values */

	if (nextToken == 0 || nextToken == TOKEN_NEW || nextToken == oldToken)
	{
	    continue;
	}

	/* Screen out ones we already have */

	if ((key = findKeyByToken(list, nextToken)) != NULL)
	{
	    free(key);
	    continue;
	}

	/* We've got one! */

	token = nextToken;
    }

    mutexUnlock(MUTEX_TOKEN);

    return token;
}

/*
** getCurrentToken
**
** Validate CurrentToken and return it. If it will expire within
** TOKEN_EXPIRE_GRACE seconds then return TOKEN_NEW.
*/

unsigned long
getCurrentToken(void)
{
    KeyInfo_t *ptr = NULL;
    time_t now;

    if (CurrentToken == 0 || CurrentToken == TOKEN_NEW) return CurrentToken;

    time(&now);

    mutexLock(MUTEX_KEYLIST);

    for (ptr = &ClientKeyList; ptr; ptr = ptr->next)
    {
	if (ptr->token == CurrentToken)
	{
	    /* Check if the entry has expired or will do soon */

	    if (ptr->expiry && now > (ptr->expiry - TOKEN_EXPIRE_GRACE))
	    {
		CurrentToken = TOKEN_NEW;
	    }
	    break;
	}
    }

    mutexUnlock(MUTEX_KEYLIST);

    return CurrentToken;
}

/*****************************\
**			     **
**  General Helper Routines  **
**			     **
\*****************************/

/*
** spawnCommand
**
** Start a sub-process running the specified command using the default
** shell. The command string may contain a single "%d" format character
** which will be replaced with the local port number.
*/

int
spawnCommand(unsigned short port, char *cmdFormat)
{
#ifdef WIN32
    char cmdBuf[MAX_LINE_SIZE];
    STARTUPINFO	suInfo;
    PROCESS_INFORMATION pInfo;

    snprintf(cmdBuf, sizeof(cmdBuf), cmdFormat, (int)port);

    memset(&suInfo, 0, sizeof(suInfo));
    suInfo.cb = sizeof(suInfo);


    if (!CreateProcess(NULL,	/* No executable -- take it from cmdBuf */
		       cmdBuf,	/* Command and arguments */
		       NULL,	/* No security attributes */
		       NULL,	/* No thread attributes */
		       FALSE,	/* Don't inherit handles */
		       0,	/* No special creation flags */
		       NULL,	/* Inherit environment */
		       NULL,	/* Inherit current directory */
		       &suInfo,	/* Start-up info */
		       &pInfo))	/* Process info needed */
    {
	message(0, errno, "failed to spawn '%s'", cmdBuf);
	return 0;
    }
#else
    char *shell = DFLT_SHELL;
    char cmdBuf[MAX_LINE_SIZE];

    snprintf(cmdBuf, sizeof(cmdBuf), cmdFormat, (int)port);

    if (((shell = getenv("SHELL")) == NULL) || *shell == '\0')
    {
	shell = DFLT_SHELL;
    }

    switch (fork())
    {
    case -1:
	message(0, errno, "fork failed");
	return 0;
	break;

    case 0:
	execl(shell, shell, "-c", cmdBuf, NULL);
	message(0, errno, "failed to exec '%s -c \"%s\"'", shell, cmdBuf);
	break;

    default:
	break;
    }
#endif

    return 1;
}

/*
** filterLoop
**
** This is the main processing loop of both client and server. It loops
** reading data from the local system, encrypts and compresses it and
** sends it to the remote system. Conversely it reads data from the remote
** system, decrypts and uncompresses it and writes it to the local system.
**
** On a normal EOF (on either end) it returns 0, on a remote comms failure
** 1 and a local failure -1.
**
** In UDP mode replies are sent back to the address specified by toAddrP using
** the socket named in replyFd. The fromAddrP is used only if UDP source
** address spoofing is being used.
**
** The select will timeout (and the connection be closed) after TcpTimeout
** or UdpTimeout seconds, as applicable.
*/

int
filterLoop(int localFd, int remoteFd, MsgBuf_t *msgBuf,
	   struct sockaddr_in *toAddrP, struct sockaddr_in *fromAddrP,
	   int replyFd, int udpMode)
{
    fd_set testSet;
    int ready = 0;
    int maxTestFd = (localFd > remoteFd ? (localFd + 1) : (remoteFd + 1));
    int num = 0;
    int status = 0;
    struct timeval delay;


    do
    {
	/* Set up the delay for select */

	delay.tv_sec = (udpMode ? UdpTimeout : TcpTimeout);
	delay.tv_usec = 0;

	/* Set up file descriptors in mask to test */

	FD_ZERO(&testSet);
	if (localFd >= 0)
	{
	    FD_SET(localFd, &testSet);
	}
	if (remoteFd >= 0)
	{
	    FD_SET(remoteFd, &testSet);
	}

	/* Do a blocking select waiting for any i/o */

	ready = select(maxTestFd, &testSet, 0, 0, (delay.tv_sec ? &delay : NULL));

	/*
	** If we get zero then there is nothing left on either fd
	** or we hit the timeout.
	*/

	if (ready == 0)
	{
	    break;
	}

	/* Check for error but ignore interrupted system calls */

	if (ready < 0 && errno != EINTR)
	{
	    message(0, errno, "error in select");
	    status = -1;
	    break;
	}

	/* Is there local data ready? */

	if (FD_ISSET(localFd, &testSet))
	{
	    if ((num = recv(localFd, (char *)msgBuf->data, msgBuf->maxSize, 0)) > 0)
	    {
		message(5, 0, "read %d bytes from local socket %d", num, localFd);

		msgBuf->size = (unsigned short)num;
		if (DumpData) dumpData("<", msgBuf->data, msgBuf->size);

		if (writeMessage(remoteFd, msgBuf) != num)
		{
		    status = 1;
		    break;
		}
	    }
	    else
	    {
		status = (num == 0 ? 0 : -1);
		break;
	    }
	}

	/* Is there remote data ready? */

	if (FD_ISSET(remoteFd, &testSet))
	{
	    /* Read the encrypted/compressed message and write to local socket */

	    num = readMessage(remoteFd, msgBuf, 0);
	    if (num > 0)
	    {
		if (udpMode)
		{
#ifdef USE_UDP_SPOOFING
		    if (Transparent)
		    {
			num = sendSpoofed(replyFd, (char *)(msgBuf->data),
					  msgBuf->size, toAddrP, fromAddrP);
		    }
		    else
#endif
		    {
			num = sendto(replyFd, (char *)(msgBuf->data), msgBuf->size,
				     0, (struct sockaddr *)toAddrP,
				     sizeof(struct sockaddr_in));
		    }
		}
		else
		{
		    num = writeData(localFd, (unsigned char *)(msgBuf->data),
				    msgBuf->size);
		}
		if (num != msgBuf->size)
		{
		    status = -1;
		    break;
		}
		message(5, 0, "sent %d bytes to local socket %d", num, localFd);
		if (DumpData) dumpData(">", msgBuf->data, msgBuf->size);
	    }
	    else
	    {
		status = (num == 0 ? 0 : 1);
		break;
	    }
	}
    }
    while (1);

    message(3, 0, "connection closed or timed out");

    message(2, 0, "read %lu bytes (%lu expanded) in %lu messages",
	    msgBuf->bytesIn, msgBuf->expBytesIn, msgBuf->readCount);
    message(2, 0, "wrote %lu bytes (%lu expanded) in %lu messages",
	    msgBuf->bytesOut, msgBuf->expBytesOut, msgBuf->writeCount);

    return status;
}

/*
** hashStrings
**
** Hash the supplied string arguments (up to a NULL pointer) together and
** write the resulting hash string into hashBuf.
*/

void
hashStrings(char *hashBuf, ...)
{
    SHA_INFO sha;
    va_list ap;
    char *str;

    sha_init(&sha);
    va_start(ap, hashBuf);
    while ((str = va_arg(ap, char *)) != NULL)
    {
	sha_update(&sha, (SHA_BYTE *)str, strlen(str));
    }
    sha_final(&sha);
    sprintf(hashBuf, "%08lx%08lx%08lx%08lx%08lx",
	    (unsigned long)sha.digest[0], (unsigned long)sha.digest[1],
	    (unsigned long)sha.digest[2], (unsigned long)sha.digest[3],
	    (unsigned long)sha.digest[4]);
}

/*
** hashFile
**
** Hash the contents of the specified file, read in binary mode in
** chunks of up to MAX_BUF_SIZE bytes. Write the result into hashBuf.
*/

void
hashFile(char *hashBuf, char *fileName)
{
    SHA_INFO sha;
    FILE *fp = NULL;
    unsigned char buf[MAX_BUF_SIZE];
    size_t num;


    if (strcmp(fileName, "-") == 0)
    {
	fp = stdin;
#if defined(WIN32) || defined(__CYGWIN__)
	setmode(0, O_BINARY);
#endif
    }
    else if ((fp = fopen(fileName, "rb")) == NULL)
    {
	message(0, errno, "can't open '%s'", fileName);
	return;
    }

    sha_init(&sha);

    while ((num = fread(buf, 1, MAX_BUF_SIZE, fp)) > 0)
    {
	sha_update(&sha, (SHA_BYTE *)buf, num);
    }

    fclose(fp);

    sha_final(&sha);
    sprintf(hashBuf, "%08lx%08lx%08lx%08lx%08lx",
	    (unsigned long)sha.digest[0], (unsigned long)sha.digest[1],
	    (unsigned long)sha.digest[2], (unsigned long)sha.digest[3],
	    (unsigned long)sha.digest[4]);
}

/*
** checkIdentity
**
** Check the supplied public key string against an "identity file". This
** file contains lines that consist of the SHA hash of the (generator,
** modulus, public-key) tuple followed by some optional commentary text.
** The hash value must begin the line and not contain any white-space.
*/

int
checkIdentity(char *idFile, char *generator, char *modulus, char *key)
{
    FILE *fp;
    char keySig[HASH_STR_SIZE];
    char checkSig[MAX_LINE_SIZE];
    char line[MAX_LINE_SIZE];
    int found = 0;
    int len = 0;


    if ((fp = fopen(idFile, "r")) == NULL)
    {
	message(0, errno, "can't open identity file '%s'", idFile);
	return 0;
    }

    if (*generator == '\0') generator = DFLT_GENERATOR;
    if (*modulus == '\0') modulus = DFLT_MODULUS;

    hashStrings(keySig, generator, modulus, key, NULL);
    len = strlen(keySig);

    message(3, 0, "checking key with identity hash '%s'", keySig);

    while (fgets(line, MAX_LINE_SIZE, fp) != NULL)
    {
	if (sscanf(line, "%s", checkSig) != 1)
	{
	    continue;
	}

	if (strcasecmp(checkSig, keySig) == 0)
	{
	    message(1, 0, "key identity matched: %.*s", strlen(line) - 1, line);
	    found = 1;
	    break;
	}
    }

    fclose(fp);

    return found;
}

/*
** generateIdentity
**
** Given a generator, modulus and private key (exponent) generate
** the hash of the public key string -- the identity used by the
** checkIdentity routine. This is the hash of the (generator,
** modulus, public-key) tuple.
*/

char *
generateIdentity(char *generator, char *modulus, char *exponent)
{
    char *dhKey = diffieHellman(generator, modulus, exponent);
    static char buffer[HASH_STR_SIZE];

    if (*generator == '\0') generator = DFLT_GENERATOR;
    if (*modulus == '\0') modulus = DFLT_MODULUS;

    hashStrings(buffer, generator, modulus, dhKey, NULL);
    free(dhKey);

    return buffer;
}

/*
** prepareToDetach
**
** This routine is necessary only on systems which have problems in
** calling "makeDetached". FreeBSD is one such system because of a
** bad interaction between fork and running threads.
*/

void
prepareToDetach(void)
{
#if defined(HAVE_PTHREADS) && defined (BUGGY_FORK_WITH_THREADS)
    pid_t pid;
    int status = 0;


    /* Fork now -- child returns immediately, parent carries on here */

    if ((pid = fork()) == 0) return;

    /*
    ** The parent now waits for the child or to be sent the SIGUSR1
    ** signal. This indicates that the parent should just exit and
    ** leave the child detached.
    */

    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    signal(SIGUSR1, sigusr1Catcher);
    waitpid(pid, &status, 0);

    exit(status);
#endif
}

/*
** makeDetached
**
** Detach the current process from its controlling terminal and run
** in the background. On UNIX this means running as a daemon. On Windows
** this can not completely detach from a parent process that is waiting
** for it but can free itself from the console. To get the full
** effect of a UNIX daemon you should probably also run this as a
** service (see the -S option).
*/

void
makeDetached(void)
{
    fflush(stdin);
    fflush(stdout);
    fflush(stderr);

#ifdef WIN32
    /* Detach from the console */

    if (!FreeConsole())
    {
	message(0, errno, "failed to detach from console (Win32 error = %ld)", GetLastError());
    }
#elif defined(HAVE_PTHREADS) && defined(BUGGY_FORK_WITH_THREADS)

    /*
    ** This is necessary if threads and fork interact badly. See the
    ** function prepareToDetach for the other part of this story ...
    **
    ** Send a signal telling the parent to exit.
    */

    kill(getppid(), SIGUSR1);

    /* Detach from controlling terminal */

    setsid();
#else
    /* Convert to a daemon */

    switch (fork())
    {
    case -1:
	/* Error! */
	message(0, errno, "fork failed becoming daemon");
	return;

    case 0:
	/* Child -- continue */
	break;

    default:
	/* Parent -- exit and leave child running */
	exit(EXIT_SUCCESS);
	break;
    }

    /* Detach from controlling terminal */

    setsid();
#endif

    /* Close stdio streams because they now have nowhere to go! */

    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    /* Set IsDetached to -1 to indicate that we have now detached */

    IsDetached = -1;
}

/*
** allowRedirect
**
** Decide whether redirection to the specified port at the specified
** address is allowed or not. The udpMode indicates whether this is a
** TCP or UDP mode connection.
**
** Returns 1 if it is or 0 otherwise.
**
** The target hostname is also returned via hostP (this storage must be
** later freed by the caller).
*/

int
allowRedirect(unsigned short port, struct sockaddr_in *addrP, int udpMode, char **hostP)
{
    PortList_t *lp1, *lp2;
    struct in_addr *alp = NULL;
    unsigned long mask = 0;
    char *ipName = NULL;


    assert(AllowedTargets != NULL);

    /*
    ** Port 0 is invalid data in the request packet, never allowed
    */
    if (port == 0)
    {
        message(0, 0, "request for target port 0 disallowed");
        return 0;
    }
    
    /*
    ** If the address is all zeroes then we will assume the default target
    ** host, if any.
    */

    if (addrP->sin_addr.s_addr == 0x00000000)
    {
	if (!getHostAddress(TargetHost, addrP, NULL, NULL))
	{
	    message(0, 0, "can't resolve host or address '%s'", TargetHost);
	    return 0;
	}
    }

    /* Allocate and fill buffer for returned host IP address string */

    if ((ipName = (char *)malloc(IP_BUF_SIZE)) == NULL)
    {
	message(0, errno, "out of memory allocating hostname");
	return 0;
    }
    *hostP = ipString(addrP->sin_addr, ipName);

    /*
    ** Search the AllowedTargets list to determine whether the port lies
    ** within any of the permitted ranges for the specified host.
    */

    for (lp1 = AllowedTargets; lp1; lp1 = lp1->next)
    {
	mask = lp1->mask;

	if ((addrP->sin_addr.s_addr & mask) != (lp1->addr.sin_addr.s_addr & mask))
	{
	    /* Did not match primary address, check aliases */

	    for (alp = lp1->addrList; alp->s_addr != 0xffffffff; alp++)
	    {
		if ((addrP->sin_addr.s_addr & mask) == (alp->s_addr & mask))
		{
		    /* Found a matching address in the aliases */
		
		    break;
		}
	    }

	    if (alp->s_addr == 0xffffffff)
	    {
		/* Did not match at all, try next entry */

		continue;
	    }
	}

	message(4, 0, "checking port %hu against range %hu-%hu for host %s", port, lp1->lo, lp1->hi, lp1->host);

	/* If the port range is 0 -- 0 then look in the default list */

	if (lp1->lo == 0 && lp1->hi == 0)
	{
	    if (AllowedDefault == NULL)
	    {
		message(4, 0, "no default port restrictions, port is allowed");
		return 1;
	    }

	    for (lp2 = AllowedDefault; lp2; lp2 = lp2->next)
	    {
		message(4, 0, "checking port %hu against default range %hu - %hu", port, lp2->lo, lp2->hi);

		if (port >= lp2->lo && port <= lp2->hi)
		{
		    if (lp2->type & (udpMode ? PORTLIST_UDP : PORTLIST_TCP))
		    {
			return 1;
		    }
		}
	    }
	}

	/* Otherwise check against the entry for this specific host */

	else if (port >= lp1->lo && port <= lp1->hi)
	{
	    if (lp1->type & (udpMode ? PORTLIST_UDP : PORTLIST_TCP))
	    {
		return 1;
	    }
	}
    }

    *hostP = NULL;
    free(ipName);
    return 0;
}

/*
** checkPeerAddress
**
** Check the address of the peer of the supplied socket against the
** list of allowed addresses, if any. Returns a true/false result.
**
** As a side effect, if addrP is not NULL the routine will also return
** the peer address information.
*/

int
checkPeerAddress(int fd, struct sockaddr_in *addrP)
{
    struct sockaddr_in addr;
    int addrLen = sizeof(struct sockaddr_in);
    unsigned short port = 0;
    PortList_t *lp1 = NULL;
    struct in_addr *alp = NULL;
    unsigned long mask = 0xffffffff;
    char ipBuf[IP_BUF_SIZE];


    /*
    ** If there is nothing to check and we do not need to return the peer
    ** address we can bail out quickly.
    */

    if (AllowedPeers == NULL && addrP == NULL) return 1;

    if (addrP == NULL) addrP = &addr;

    if (getpeername(fd, (struct sockaddr *)addrP, &addrLen))
    {
	message(0, errno, "can't get peer address for socket");
	return 0;
    }
    message(4, 0, "peer address from connection is %s", ipString(addrP->sin_addr, ipBuf));

    /* Now return if there is nothing more to do */

    if (AllowedPeers == NULL) return 1;

    /* Otherwise, search for a match ... */

    port = ntohs(addrP->sin_port);

    for (lp1 = AllowedPeers; lp1; lp1 = lp1->next)
    {
	mask = lp1->mask;

	if ((addrP->sin_addr.s_addr & mask) != (lp1->addr.sin_addr.s_addr & mask))
	{
	    /* Did not match primary address, check aliases */

	    for (alp = lp1->addrList; alp->s_addr != 0xffffffff; alp++)
	    {
		if ((addrP->sin_addr.s_addr & mask) == (alp->s_addr & mask))
		{
		    /* Found a matching address in the aliases */

		    break;
		}
	    }

	    if (alp->s_addr == 0xffffffff)
	    {
		/* Did not match at all, try next entry */

		continue;
	    }
	}

	message(4, 0, "checking port %hu against range %hu-%hu for host %s", port, lp1->lo, lp1->hi, lp1->host);

	/* If the port range is 0 -- 0 then any port is OK */

	if (lp1->lo == 0 && lp1->hi == 0)
	{
	    return 1;
	}

	/* Otherwise check against the entry for this specific host */

	else if (port >= lp1->lo && port <= lp1->hi)
	{
	    return 1;
	}
    }

    return 0;
}

/*
** countPorts
**
** Count the number of ports named in a PortList_t linked list
*/

int
countPorts(PortList_t *list)
{
    int count = 0;

    while (list)
    {
	count += (int)(list->hi - list->lo + 1);
	list = list->next;
    }

    return count;
}

/*
** mapPort
**
** Given a local port number localPort -- which should be found in ClientPorts
** -- map it to the corresponding remote port number in TargetPorts
**
** Returns the remote port number on success or zero on error. If the hostP
** or addrP parameters are not NULL then any hostname and address associated
** with the port is also returned.
*/

unsigned short
mapPort(unsigned short localPort, char **hostP, struct sockaddr_in *addrP)
{
    PortList_t *localPtr = ClientPorts;
    PortList_t *remotePtr = TargetPorts;
    unsigned short count = 0;


    /* Find the index of the specified port in ClientPorts */

    while (localPtr)
    {
	if (localPort <= localPtr->hi && localPort >= localPtr->lo)
	{
	    count += localPort - localPtr->lo;
	    break;
	}

	count += (localPtr->hi - localPtr->lo + 1);
	localPtr = localPtr->next;
    }

    /* If we have fallen off the end of the list, return 0 */

    if (localPtr == NULL)
    {
	return 0;
    }

    /* Now find the corresponding element in TargetPorts */

    while (remotePtr)
    {
	if (count <= (unsigned short)(remotePtr->hi - remotePtr->lo))
	{
	    if (addrP)
	    {
		addrP->sin_addr.s_addr = remotePtr->addr.sin_addr.s_addr;
	    }
	    if (hostP)
	    {
		if (remotePtr->host)
		{
		    *hostP = remotePtr->host;
		}
		else
		{
		    *hostP = ServerHost;
		}
	    }
	    return (remotePtr->lo + count);
	}

	count -= (remotePtr->hi - remotePtr->lo + 1);
	remotePtr = remotePtr->next;
    }

    return 0;
}

/******************************************\
**					  **
**  Core Client/Server Protocol Routines  **
**					  **
\******************************************/


/*
** spawnHandler
**
** This routine creates a single process/thread running the specified
** handler routine to handle the traffic on a single connection.
*/

unsigned long
spawnHandler(void (*handler)(FnArgs_t *), int listenFd, int clientFd,
	     int inLine, struct sockaddr_in *addrP, int udpMode)
{
    FnArgs_t *argP = NULL;
    struct sockaddr_in localAddr;
    int addrLen = sizeof(localAddr);


    if ((argP = (FnArgs_t *)malloc(sizeof(FnArgs_t))) == NULL)
    {
	message(0, errno, "failed to allocate handler argument structure");
	return 0;
    }
    argP->fd = clientFd;
    if (addrP)
    {
	memcpy(&(argP->addr), addrP, sizeof(struct sockaddr_in));
    }

    /*
    ** Find out what local port is being used so we can map to the
    ** corresponding remote port number (used by the client only)
    */

    argP->listenFd = listenFd;
    if (listenFd >= 0)
    {
	addrLen = sizeof(localAddr);
	memset(&localAddr, 0, sizeof(localAddr));
	if (getsockname(listenFd, (struct sockaddr *)&localAddr, &addrLen))
	{
	    message(0, errno, "can't get local port number");
	    return 0;
	}
	argP->port = ntohs(localAddr.sin_port);
    }

    argP->udpMode = udpMode;

    argP->inLine = inLine;
    if (inLine)
    {
	message(4, 0, "running handler function in-line");
	(*handler)(argP);
	return 0;
    }

#if defined(HAVE_PTHREADS)
    {
	pthread_t tid;

	message(4, 0, "spawning handler function thread");
	if (pthread_create(&tid,
			   &ThreadAttr,
			   (void * (*)(void *))handler,
			   (void *)argP) == -1)
	{
	    message(0, errno, "failed to create handler thread");
	}
	message(4, 0, "handler thread %lu created", (unsigned long)tid);
	return ((unsigned long)tid ? (unsigned long)tid : 0xffffffff );    /* Ensure it is never 0 */
    }
#elif defined(WIN32)
    {
	unsigned long tid = 0;

	message(4, 0, "spawning handler function thread");
	if ((tid = (unsigned long)_beginthread((void (*)(void *))handler,
					       THREAD_STACK_SIZE,
					       (LPVOID)argP)) == 0)
	{
	    message(0, errno, "failed to create handler thread");
	}
	else
	{
	    message(4, 0, "handler thread created");
	}
	return tid;
    }
#else	/* No PTHREADS and not WIN32 */
    {
	pid_t pid;


	message(4, 0, "spawning handler sub-process");
	if ((pid = fork()) < 0)
	{
	    message(0, errno, "failed to fork handler sub-process");
	    return 0;
	}
        else if (pid == 0)
	{
	    /* Child -- listenFd no longer needed */

	    if (!udpMode) closesocket(listenFd);

	    (*handler)(argP);
	    exit(EXIT_SUCCESS);
	}
	else
	{
	    /* Parent -- clientFd no longer needed */

	    if (!udpMode) closesocket(clientFd);

	    message(4, 0, "handler sub-process %lu created", (unsigned long)pid);
	    return (unsigned long)pid;
	}
    }
#endif
}

/*
** findHandler
**
** Find the socket descriptor for associated with the handler for requests
** from the address "fromAddr", if there is one. Returns the socket id
** and local "loopback" socket address via localAddrP or -1 if not found.
*/

int
findHandler(struct sockaddr_in *fromAddrP, struct sockaddr_in *localAddrP)
{
    HndInfo_t *ptr = NULL;
    HndInfo_t *tmp = NULL;
    int found = -1;
    char ipBuf[IP_BUF_SIZE];


    message(5, 0, "searching for handler for address %s:%hu", ipString(fromAddrP->sin_addr, ipBuf), ntohs(fromAddrP->sin_port));

    mutexLock(MUTEX_HNDLIST);

    for (ptr = &HandlerList; ptr != NULL; ptr = ptr->next)
    {
#if !defined(WIN32) && !defined(HAVE_PTHREADS)
	/*
	** If we don't have threads then check whether the handler process
	** is still alive. If not, remove it from the list.
	*/

	if (kill((pid_t)(ptr->id), 0) != 0)
	{
	    message(5, 0, "removing defunct handler, id = %lu", ptr->id);

	    ptr->prev->next = ptr->next;
	    if (ptr->next)
	    {
		ptr->next->prev = ptr->prev;
	    }
	    tmp = ptr;
	    ptr = ptr->prev;
	    closesocket(tmp->fd);
	    free(tmp);
	    continue;
	}
#endif
	if (ptr->fromAddr.sin_port == fromAddrP->sin_port &&
	    ptr->fromAddr.sin_addr.s_addr == fromAddrP->sin_addr.s_addr)
	{
	    message(5, 0, "found handler, id = %lu, socket = %d", ptr->id, ptr->fd);
	    found = ptr->fd;
	    memcpy(localAddrP, &(ptr->localAddr), sizeof(struct sockaddr_in));
	}

	tmp = ptr; /* Shut the compiler up by using tmp! */
    }

    mutexUnlock(MUTEX_HNDLIST);

    return found;
}

/*
** addHandler
**
** Register a new handler for requests from fromAddrP. The "id" is only used
** in the multi-process version and is the PID of the handler process. The
** "fd" is the socket descriptor for the local "loopback" socket used to
** communicate with the handler. The "localAddrP" is the address associated
** with the socket.
*/

void
addHandler(struct sockaddr_in *fromAddrP, unsigned long id, int fd, struct sockaddr_in *localAddrP)
{
    HndInfo_t *ptr = NULL;


    mutexLock(MUTEX_HNDLIST);

    for (ptr = &HandlerList; ptr->next != NULL; ptr = ptr->next)
    {
	/* Walk to end */
    }

    if ((ptr->next = (HndInfo_t *)malloc(sizeof(HndInfo_t))) == NULL)
    {
	message(0, errno, "failed to allocate memory for handler list element");
    }
    else
    {
	ptr->next->id = id;
	ptr->next->fd = fd;
	memcpy(&(ptr->next->fromAddr), fromAddrP, sizeof(struct sockaddr_in));
	memcpy(&(ptr->next->localAddr), localAddrP, sizeof(struct sockaddr_in));
	ptr->next->prev = ptr;
	ptr->next->next = NULL;
    }

    mutexUnlock(MUTEX_HNDLIST);
}

/*
** removeHandler
**
** This removes the handler with the specified address from the list.
*/

void
removeHandler(struct sockaddr_in *addrP)
{
    HndInfo_t *ptr = NULL;
    HndInfo_t *tmp = NULL;

    mutexLock(MUTEX_HNDLIST);

    for (ptr = &HandlerList; ptr != NULL; ptr = ptr->next)
    {
	if (ptr->fromAddr.sin_port == addrP->sin_port &&
	    ptr->fromAddr.sin_addr.s_addr == addrP->sin_addr.s_addr)
	{
	    ptr->prev->next = ptr->next;
	    if (ptr->next)
	    {
		ptr->next->prev = ptr->prev;
	    }
	    tmp = ptr;
	    ptr = ptr->prev;
	    /* socket is closed in client routine */
	    free(tmp);
	}
    }

    mutexUnlock(MUTEX_HNDLIST);
}

/*
** clientListener
**
** This is the top-level client routine that sets up local sockets
** and listens for connections. It the then spawns an individual
** process or thread to handle a client.
**
** This operates slightly differently in TCP and UDP modes. In TCP mode
** when a new connection is detected the connection is accepted and then
** this accepted socket is handed off to a handler routine in a separate
** thread or process. All data from and to the client is handled directly
** by the handler function.
**
** In UDP mode things are more complex. Every datagram from a client is
** received in this routine. It examines the source address and determines
** whether it currently has a handler active. If it doesn't it creates one
** along with a local "loopback" socket which it passes to the handler along
** with the address of the original client. It then sends the messages it
** receives to the loopback socket. The handler itself will exit after
** UdpTimeout seconds of inactivity.
*/

void
clientListener(PortList_t *ports)
{
    int listenFd = -1;
    int clientFd;
    struct sockaddr_in fromAddr;
    struct sockaddr_in localAddr;
    int addrLen;
    unsigned short localPort = 0;
    fd_set tcpSet;
    fd_set udpSet;
    fd_set unionSet;
    fd_set testSet;
    int maxFd = -1;
    int ready = 0;
    unsigned long id = 0;
    char data[MAX_BUF_SIZE];
    int num;
    char ipBuf[IP_BUF_SIZE];


    message(3, 0, "client listener routine entered");

    /*
    ** Create the local listener socket(s) and fire up the requested
    ** sub-command if necessary. We do this first so that the child
    ** process does not inherit the connection to the server.
    */

    FD_ZERO(&tcpSet);
    FD_ZERO(&udpSet);
    if (TcpMode)
    {
	maxFd = makeClientListeners(ports, &tcpSet, 0);
    }
    if (UdpMode)
    {
	listenFd = makeClientListeners(ports, &udpSet, 1);
	if (listenFd > maxFd)
	{
	    maxFd = listenFd;
	}
    }

    /*
    ** Catch possible mix-ups in the client tunnels specification leading to
    ** there being no valid ports to listen on.
    */

    if (maxFd == -1)
    {
	message(0, 0, "client not listening on any ports -- check tunnel specifications");
	exit(EXIT_FAILURE);
    }

    /* Spawn the sub-command, if specified */

    if (CommandString)
    {
	message(3, 0, "spawning command '%s'", CommandString);

	if (!spawnCommand(ports->lo, CommandString))
	{
	    exit(EXIT_FAILURE);
	}
    }

    /* Detach from terminal, if required */

    if (IsDetached)
    {
	message(3, 0, "detaching from terminal");
	makeDetached();
    }

    /*
    ** If running in "listen mode" the client must listen for the server
    ** to connect back to it.
    */

    if (ListenMode)
    {
	message(3, 0, "listening for server connection on port %hu", ServerPort);

	if ((ListenSock = makeListener(&ServerPort, ListenIp, 0)) == -1)
	{
	    message(0, errno, "can't create listener socket for server connection");
	    exit(EXIT_FAILURE);
	}
    }

    /*
    ** Now wait for a connection from a local client. If we are operating
    ** in "persistent" mode then we will loop forever otherwise this
    ** will be a "one shot" connection.
    */

    if (UdpMode) message(1, 0, "listening for client UDP data");

    /* Create union fd sets from the TCP and UDP sets */

    FD_ZERO(&unionSet);
    for (listenFd = 0; listenFd <= maxFd; listenFd++)
    {
	if (FD_ISSET(listenFd, &tcpSet))
	{
	    FD_SET(listenFd, &unionSet);
	}
	else if (FD_ISSET(listenFd, &udpSet))
	{
	    FD_SET(listenFd, &unionSet);
	}
    }

    do
    {
	memcpy(&testSet, &unionSet, sizeof(fd_set));

	if (UdpMode)
	{
	    message(5, 0, "waiting for client data", localPort);
	}
	else
	{
	    message(1, 0, "waiting for client connection", localPort);
	}

	/* Do a blocking select waiting for any i/o */

	ready = select(maxFd + 1, &testSet, 0, 0, 0);

	message((UdpMode ? 5 : 3), 0, "select returned %d", ready);

	/* If we get zero then there is nothing available on any fd. */

	if (ready == 0)
	{
	    break;
	}

	/* Check for error but ignore interrupted system calls */

	if (ready < 0 && errno != EINTR)
	{
	    message(0, errno, "error in select");
	    break;
	}

	/* See which sockets have connections/data and handle them */

	for (listenFd = 0; listenFd <= maxFd; listenFd++)
	{
	    if (FD_ISSET(listenFd, &testSet))
	    {
		if (FD_ISSET(listenFd, &udpSet))
		{
		    /* See who this is from */

		    addrLen = sizeof(fromAddr);
		    if ((num = recvfrom(listenFd, data, MAX_BUF_SIZE, 0,
					(struct sockaddr *)&fromAddr,
					&addrLen)) > 0)
		    {
			/*
			** If there is not already a handler for this
			** address/port combination then create one.
			*/

			if ((clientFd = findHandler(&fromAddr, &localAddr)) == -1)
			{
			    /* Create a "loopback" socket */

			    localPort = 0;
			    if ((clientFd = makeListener(&localPort, "127.0.0.1", 1)) == -1)
			    {
				continue;
			    }

			    id = spawnHandler(client, listenFd, clientFd,
					      (Debug || !MultiUse), &fromAddr, 1);

			    if (id != 0)
			    {
				memset(&localAddr, 0, sizeof(localAddr));
				localAddr.sin_family = AF_INET;
				localAddr.sin_port = htons(localPort);
				localAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

				addHandler(&fromAddr, id, clientFd, &localAddr);
			    }
			}

			/*
			** We should now have a valid loopback socket
			** descriptor associated with a handler. If so,
			** send the data on to it.
			*/

			if (clientFd != -1)
			{
			    if (sendto(clientFd, data, num, 0,
				       (struct sockaddr *)&localAddr,
				       sizeof(localAddr)) != num)
			    {
				message(0, errno, "failed to send data to loopback socket");
			    }
			}
		    }
		    else
		    {
			message(0, errno, "can't read next message");
		    }
		}
		else
		{
		    /*
		    ** New TCP connection -- accept the connection and
		    ** spawn a new handler for it.
		    */

		    message(5, 0, "connection ready on socket %d", listenFd);

		    addrLen = sizeof(struct sockaddr_in);
		    memset(&fromAddr, 0, sizeof(fromAddr));
		    if ((clientFd = accept(listenFd,
					   (struct sockaddr *)&fromAddr,
					   &addrLen)) < 0)
		    {
			message(0, errno, "error on accept");
		    }
		    else
		    {
			message(1, 0, "accepted connection from %s", ipString(fromAddr.sin_addr, ipBuf));

			/* Set the "don't linger on close" option */

			setNoLinger(clientFd);

			/* Set "keep alive" to reap defunct connections */

			setKeepAlive(clientFd);

			/* Create the handler process/thread */

			spawnHandler(client, listenFd, clientFd,
				     (Debug || !MultiUse), &fromAddr, 0);
		    }
		}
	    }
	}
    }
    while (MultiUse);

    /* We do not need to listen for any more clients */

    for (listenFd = 0; listenFd <= maxFd; listenFd++)
    {
	if (FD_ISSET(listenFd, &unionSet)) closesocket(listenFd);
    }
    listenFd = -1;

    /* Wait for handler threads to terminate (should not be necessary) */

    waitForInactivity();
}

/*
** makeClientListeners
**
** Create the listen sockets for the ports in the supplied port list.
** Set the appropriate bits in the fd_set. The udpMode value indicates
** whether we're doing TCP or UDP listens.
*/

int
makeClientListeners(PortList_t *ports, fd_set *listenSetP, int udpMode)
{
    int listenFd = -1;
    unsigned short localPort = 0;
    int maxFd = -1;

    while (ports)
    {
	for (localPort = ports->lo; localPort <= ports->hi; localPort++)
	{
	    if ((ports->type & (udpMode ? PORTLIST_UDP : PORTLIST_TCP)) == 0)
	    {
		/* Skip incompatible port types */
		continue;
	    }

	    message(3, 0, "creating %s-mode local listener socket for port %hu",
		    (udpMode ? "UDP" : "TCP"), localPort);

	    if ((listenFd = makeListener(&localPort, ListenIp, udpMode)) == -1)
	    {
		message(0, errno, "can't create listener socket");
		exit(EXIT_FAILURE);
	    }

	    message(5, 0, "local port %hu has socket %d", localPort, listenFd);

	    FD_SET(listenFd, listenSetP);
	    if (listenFd > maxFd)
	    {
		maxFd = listenFd;
	    }

	    if (!CommandString)
	    {
		message(1, 0, "Listening on local port %hu", localPort);
	    }
	    else
	    {
		message(3, 0, "listening on local port %hu", localPort);
	    }

	    /* Special case port was zero -- modify entry */

	    if (ports->hi == 0)
	    {
		ports->lo = ports->hi = localPort;
	    }
	}

	ports = ports->next;
    }

    return maxFd;
}

/*
** client
**
** This routine implements the client side of the Zebedee protocol. It is
** fully re-entrant.
*/

void
client(FnArgs_t *argP)
{
    int clientFd = argP->fd;
    const char *serverHost = ServerHost;
    const unsigned short serverPort = ServerPort;
    unsigned short redirectPort = 0;
    unsigned short maxSize = MaxBufSize;
    int serverFd = -1;
    unsigned short response = 0;
    char generator[MAX_LINE_SIZE];
    char modulus[MAX_LINE_SIZE];
    char serverDhKey[MAX_LINE_SIZE];
    char *exponent = NULL;
    char *dhKey = NULL;
    char *secretKeyStr = NULL;
    char *sessionKeyStr = NULL;
    MsgBuf_t *msg = NULL;
    unsigned short cmpInfo = CompressInfo;
    unsigned short keyBits = KeyLength;
    unsigned short protocol = DFLT_PROTOCOL;
    unsigned long token = 0;
    unsigned char hdrData[HDR_SIZE_MAX];
    unsigned short hdrSize = HDR_SIZE_MIN;
    unsigned char clientNonce[NONCE_SIZE];
    unsigned char serverNonce[NONCE_SIZE];
    char *targetHost = NULL;
    struct sockaddr_in peerAddr;
    struct sockaddr_in targetAddr;
    int inLine = argP->inLine;
    int udpMode = argP->udpMode;
    char ipBuf[IP_BUF_SIZE];


    incrActiveCount(1);
    message(3, 0, "client routine entered");

    /*
    ** Find out what target port we will tunnel to.
    */

    redirectPort = mapPort(argP->port, &targetHost, &targetAddr);
    if (redirectPort)
    {
	message(3, 0, "client on local port %hu tunnels to target %s:%hu", argP->port, targetHost, redirectPort);
	message(4, 0, "target address is %08x", ntohl(targetAddr.sin_addr.s_addr));
    }
    else
    {
	message(0, 0, "no matching target port for local port %hu", argP->port);
	goto fatal;
    }

    if (ListenMode)
    {
	message(3, 0, "waiting for connection from server");

	if ((serverFd = acceptConnection(ListenSock, serverHost,
					 1, ConnectTimeout)) == -1)
	{
	    message(0, errno, "failed to accept a connection from %s", serverHost);
	    goto fatal;
	}
	message(3, 0, "accepted connection from server");
    }
    else
    {
	message(3, 0, "making connection to %s:%hu", serverHost, serverPort);

	if ((serverFd = makeConnection(serverHost, serverPort, 0, 1, NULL, NULL)) == -1)
	{
	    message(0, errno, "can't connect to %s port %hu", serverHost, serverPort);
	    goto fatal;
	}
	message(3, 0, "connected to %s:%hu", serverHost, serverPort);
    }

    /*
    ** Validate the server IP address, if required.
    */

    message(3, 0, "validating server IP address");

    if (!checkPeerAddress(serverFd, &peerAddr))
    {
	message(0, 0, "connection with server %s disallowed", ipString(peerAddr.sin_addr, ipBuf));
	goto fatal;
    }

    /*
    ** Request protocol version.
    */

    message(3, 0, "requesting protocol version %#hx", protocol);

    if (!requestResponse(serverFd, protocol, &response))
    {
	message(0, errno, "failed requesting protocol version");
	goto fatal;
    }

    switch (response)
    {
    case PROTOCOL_V201:
	protocol = PROTOCOL_V201;
	break;

    case PROTOCOL_V200:
	protocol = PROTOCOL_V200;
	break;

    case PROTOCOL_V102:
	protocol = PROTOCOL_V102;
	break;

    case PROTOCOL_V101:
	protocol = PROTOCOL_V101;
	break;

    case PROTOCOL_V100:
    case 0:
	/*
	** The zero return is a special case. In versions prior to 1.2.0
	** the server would return 0 if it could not handle the requested
	** protocol version. It SHOULD have returned the highest version
	** it supports less than or equal to the requested version.
	** So, we treat 0 as synonymous with 0x0100.
	*/
	protocol = PROTOCOL_V100;
	break;

    default:
	message(0, 0, "server responded with incompatible protocol version (%#hx)", response);
	goto fatal;
    }

    message(3, 0, "accepted protocol version %#hx", response);

    /*
    ** For protocol versions 200 and higher we will send as many parameters
    ** as we can in a single block to improve efficiency.
    */

    if (protocol >= PROTOCOL_V200)
    {
	message(3, 0, "requesting %s mode", (udpMode ? "UDP" : "TCP"));
	headerSetUShort(hdrData, (udpMode ? HDR_FLAG_UDPMODE : 0), HDR_OFFSET_FLAGS);

	message(3, 0, "requesting buffer size %hu", maxSize);
	headerSetUShort(hdrData, maxSize, HDR_OFFSET_MAXSIZE);

	message(3, 0, "requesting compression level %#hx", CompressInfo);
	headerSetUShort(hdrData, CompressInfo, HDR_OFFSET_CMPINFO);

	message(3, 0, "requesting redirection to port %hu", redirectPort);
	headerSetUShort(hdrData, redirectPort, HDR_OFFSET_PORT);

	message(3, 0, "requesting key length %hu", KeyLength);
	headerSetUShort(hdrData, KeyLength, HDR_OFFSET_KEYLEN);

	token = getCurrentToken();
	message(3, 0, "requesting key reuse token %#lx", token);
	headerSetULong(hdrData, token, HDR_OFFSET_TOKEN);

	generateNonce(clientNonce);
	message(3, 0, "sending client nonce %02x%02x...", clientNonce[0], clientNonce[1]);
	memcpy(hdrData + HDR_OFFSET_NONCE, clientNonce, NONCE_SIZE);

	if (protocol > PROTOCOL_V200)
	{
	    hdrSize = HDR_SIZE_V201;
	    /*
	    ** If the target is the same as the ServerHost then we send
	    ** all zeroes to indicate the default server target. Otherwise
	    ** we use the targetAddr.
	    */

	    if (strcmp(targetHost, ServerHost) == 0)
	    {
		message(4, 0, "target is the same as the server");
		targetAddr.sin_addr.s_addr = 0x00000000;
	    }
	    message(3, 0, "requesting target address %08x", ntohl(targetAddr.sin_addr.s_addr));
	    headerSetULong(hdrData, (unsigned long)ntohl(targetAddr.sin_addr.s_addr), HDR_OFFSET_TARGET);
	}

	if (writeData(serverFd, hdrData, hdrSize) != hdrSize)
	{
	    message(0, errno, "failed writing protocol header to server");
	    goto fatal;
	}

	if (readData(serverFd, hdrData, hdrSize) != hdrSize)
	{
	    message(0, errno, "failed reading protocol header response from server");
	    goto fatal;
	}

	if ((udpMode && headerGetUShort(hdrData, HDR_OFFSET_FLAGS) != HDR_FLAG_UDPMODE) ||
	    (!udpMode && headerGetUShort(hdrData, HDR_OFFSET_FLAGS) == HDR_FLAG_UDPMODE))
	{
	    message(0, 0, "client requested %s mode and server is in %s mode",
		    (udpMode ? "UDP" : "TCP"), (udpMode ? "TCP" : "UDP"));
	    goto fatal;
	}
	else
	{
	    message(3, 0, "accepted %s mode", (udpMode ? "UDP" : "TCP"));
	}

	maxSize = headerGetUShort(hdrData, HDR_OFFSET_MAXSIZE);
	if (maxSize > 0)
	{
	    message(3, 0, "accepted buffer size %hu", maxSize);
	}
	else
	{
	    message(0, 0, "server responded with zero buffer size");
	    goto fatal;
	}

	cmpInfo = headerGetUShort(hdrData, HDR_OFFSET_CMPINFO);
	if (cmpInfo <= CompressInfo)
	{
	    message(3, 0, "accepted compression level %#hx", cmpInfo);
	}
	else
	{
	    message(0, 0, "server responded with invalid compression level (%#hx > %#hx)", cmpInfo, CompressInfo);
	    goto fatal;
	}

	response = headerGetUShort(hdrData, HDR_OFFSET_PORT);
	if (response == redirectPort)
	{
	    message(3, 0, "redirection request accepted");
	}
	else
	{
	    message(0, 0, "server refused request for redirection to %s:%hu", targetHost, redirectPort);
	    goto fatal;
	}

	keyBits = headerGetUShort(hdrData, HDR_OFFSET_KEYLEN);
	if (keyBits >= MinKeyLength)
	{
	    message(3, 0, "accepted key length %hu", keyBits);
	}
	else
	{
	    message(0, 0, "server key length too small (%hu < %hu)", keyBits, MinKeyLength);
	    goto fatal;
	}

	token = headerGetULong(hdrData, HDR_OFFSET_TOKEN);
	message(3, 0, "accepted key reuse token %#lx", token);

	memcpy(serverNonce, hdrData + HDR_OFFSET_NONCE, NONCE_SIZE);
	message(3, 0, "received server nonce %02x%02x...", serverNonce[0], serverNonce[1]);
    }
    else
    {
	/*
	** In older protocol versions the fields in the "header" block
	** were sent and acknowledged individually. The basic meanings
	** are the same so the comments have not been duplicated.
	*/

	if (protocol >= PROTOCOL_V101)
	{
	    message(3, 0, "requesting buffer size %hu", maxSize);

	    if (!requestResponse(serverFd, maxSize, &response))
	    {
		message(0, errno, "failed requesting buffer size");
		goto fatal;
	    }

	    maxSize = response;
	    message(3, 0, "accepted buffer size %hu", maxSize);
	}

	message(3, 0, "requesting compression level %#hx", CompressInfo);

	if (!requestResponse(serverFd, CompressInfo, &response))
	{
	    message(0, errno, "failed requesting compression level");
	    goto fatal;
	}

	if (response > CompressInfo)
	{
	    message(0, 0, "server responded with invalid compression level (%#hx > %#hx)", response, CompressInfo);
	    goto fatal;
	}
	cmpInfo = response;
	message(3, 0, "accepted compression level %#hx", response);

	message(3, 0, "requesting redirection to port %hu", redirectPort);

	if (!requestResponse(serverFd, redirectPort, &response))
	{
	    message(0, errno, "failed requesting redirection port");
	    goto fatal;
	}

	if (response != redirectPort)
	{
	    message(0, errno, "server refused request for redirection to port %hu", redirectPort);
	    goto fatal;
	}
	message(3, 0, "redirection request accepted");

	message(3, 0, "requesting key length %hu", KeyLength);

	if (!requestResponse(serverFd, KeyLength, &response))
	{
	    message(0, errno, "failed requesting key length");
	    goto fatal;
	}
	keyBits = response;

	if (keyBits < MinKeyLength)
	{
	    message(0, 0, "server key length too small (%hu < %hu)", keyBits, MinKeyLength);
	    goto fatal;
	}
	message(3, 0, "accepted key length %hu", response);
    }


    /* Allocate message buffer -- common to all protocol versions */

    if ((msg = makeMsgBuf(maxSize, cmpInfo)) == NULL)
    {
	message(0, errno, "client failed to allocate message buffer");
	goto fatal;
    }

    /*
    ** For version 200 of the protocol, if the session token returned
    ** by the server matches that sent by the client (and is not zero)
    ** then we will perform a challenge-response to verify that there
    ** really is a shared key. There is a small possibility that a
    ** a server that has restarted since a client received a token will
    ** see an apparently valid token value from such a client but will,
    ** in reality, have no shared key. This will detect that although
    ** the result will be that the client connection is rejected.
    */

    if (protocol >= PROTOCOL_V200 &&
	(secretKeyStr = findKeyByToken(&ClientKeyList, token)) != NULL)
    {
	sessionKeyStr = generateSessionKey(secretKeyStr, clientNonce,
					   serverNonce, keyBits);

	message(3, 0, "session key ends '...%s'", sessionKeyStr + strlen(sessionKeyStr) - 4);

	msg->bfWrite = setupBlowfish(sessionKeyStr, keyBits);
	msg->bfRead = setupBlowfish(sessionKeyStr, keyBits);

	memset(secretKeyStr, 0, strlen(secretKeyStr));
	free(secretKeyStr);
	secretKeyStr = NULL;
	memset(sessionKeyStr, 0, strlen(sessionKeyStr));
	free(sessionKeyStr);
	sessionKeyStr = NULL;

	if (!clientPerformChallenge(serverFd, msg))
	{
	    message(0, 0, "challenge/response failed to validate shared key (session token = %#08x)", CurrentToken);
	    mutexLock(MUTEX_TOKEN);
	    CurrentToken = TOKEN_NEW;
	    mutexUnlock(MUTEX_TOKEN);
	    goto fatal;
	}
    }

    /*
    ** ELSE ...
    ** If the key length is zero and the protocol version is 102 or
    ** higher then we can omit the key exchange protocol because only
    ** compression is being used.
    */

    else if (keyBits > 0 || protocol < PROTOCOL_V102)
    {

	/* Read the DH generator */

	message(3, 0, "reading DH generator");

	if (readMessage(serverFd, msg, MAX_LINE_SIZE) <= 0)
	{
	    message(0, errno, "failed reading DH generator");
	    goto fatal;
	}
	getMsgBuf(msg, generator, MAX_LINE_SIZE);

	message(3, 0, "accepted generator '%s'", generator);


	/* Read the DH modulus */

	message(3, 0, "reading DH modulus");

	if (readMessage(serverFd, msg, MAX_LINE_SIZE) <= 0)
	{
	    message(0, errno, "failed reading DH modulus");
	    goto fatal;
	}
	getMsgBuf(msg, modulus, MAX_LINE_SIZE);

	message(3, 0, "accepted modulus '%s'", modulus);


	/* Read the server DH key */

	message(3, 0, "reading server DH key");

	if (readMessage(serverFd, msg, MAX_LINE_SIZE) <= 0)
	{
	    message(0, errno, "failed reading server DH key");
	    goto fatal;
	}
	getMsgBuf(msg, serverDhKey, MAX_LINE_SIZE);

	message(3, 0, "accepted server DH key '%s'", serverDhKey);

	/*
	** If requested, check this against a list of "known" keys
	** to validate the server identity.
	*/

	if (IdentityFile)
	{
	    message(3, 0, "checking key against identity file '%s'", IdentityFile);

	    if (!checkIdentity(IdentityFile, generator, modulus, serverDhKey))
	    {
		message(0, 0, "server's identity not found in '%s'", IdentityFile);
		goto fatal;
	    }
	}

	/*
	** Now generate our exponent (the private key). This is returned
	** as a hex string. If a private key string has been specified
	** then use this.
	*/

	message(3, 0, "generating private key");

	if ((exponent = generateKey()) == NULL)
	{
	    message(0, 0, "can't generate private key");
	    goto fatal;
	}
	message(3, 0, "private key generated");


	/*
	** Generate the public DH key.
	*/

	message(3, 0, "generating public DH key");

	if ((dhKey = diffieHellman(generator, modulus, exponent)) == NULL)
	{
	    message(0, 0, "can't generate public DH key");
	    goto fatal;
	}
	message(3, 0, "public DH key is '%s'", dhKey);

	/* Now send the DH key */

	message(3, 0, "sending public DH key");

	setMsgBuf(msg, dhKey, strlen(dhKey) + 1);
	if (writeMessage(serverFd, msg) != msg->size)
	{
	    message(0, errno, "failed writing DH key to server");
	    goto fatal;
	}
	message(3, 0, "sent public DH key");

	/* Now generate the shared secret key */

	message(3, 0, "generating shared secret key");

	secretKeyStr = diffieHellman(serverDhKey, modulus, exponent);

	message(3, 0, "shared key ends '...%s'", secretKeyStr + strlen(secretKeyStr) - 4);

	if (protocol >= PROTOCOL_V200)
	{
	    sessionKeyStr = generateSessionKey(secretKeyStr, clientNonce,
					       serverNonce, keyBits);

	    message(3, 0, "session key ends '...%s'", sessionKeyStr + strlen(sessionKeyStr) - 4);
	}
	else
	{
	    sessionKeyStr = secretKeyStr;
	}

	message(3, 0, "initialising encryption state");

	msg->bfWrite = setupBlowfish(sessionKeyStr, keyBits);
	msg->bfRead = setupBlowfish(sessionKeyStr, keyBits);

	/* Clear unneeded values */

	memset(exponent, 0, strlen(exponent));
	free(exponent);
	exponent = NULL;
	free(dhKey);
	dhKey = NULL;

	/*
	** Having established the encrypted channel we now perform a mutual
	** challenge/response dialogue in order to guard against being spoofed
	** by a replay attack if we are using a static key.
	*/

	if (!clientPerformChallenge(serverFd, msg))
	{
	    goto fatal;
	}

	/*
	** If we are at protocol version 200 or higher and a new session
	** token was allocated by the server then update the CurrentToken
	** and secret key list.
	*/

	if (protocol >= PROTOCOL_V200 && token != 0)
	{
	    message(3, 0, "new reusable key token established (%#lx)", token);

	    addKeyInfoToList(&ClientKeyList, token, secretKeyStr);
	    mutexLock(MUTEX_TOKEN);
	    CurrentToken = token;
	    mutexUnlock(MUTEX_TOKEN);
	}

	if (sessionKeyStr != secretKeyStr)
	{
	    memset(sessionKeyStr, 0, strlen(sessionKeyStr));
	    free(sessionKeyStr);
	    sessionKeyStr = NULL;
	}
	memset(secretKeyStr, 0, strlen(secretKeyStr));
	free(secretKeyStr);
	secretKeyStr = NULL;
    }
    else
    {
	message(3, 0, "key length is zero, omitting key exchange");
	if (IdentityFile != NULL)
	{
	    message(1, 0, "Warning: agreed key length is zero, no identity checking performed");
	}
    }

    message(1, 0, "tunnel established to %s port %hu", serverHost, redirectPort);
    message(2, 0, "compression level %#hx, key length %hu", cmpInfo, keyBits);

    /* Now loop handling i/o */

    message(3, 0, "entering filter loop");

    switch (filterLoop(clientFd, serverFd, msg, &(argP->addr), &peerAddr, argP->listenFd, udpMode))
    {
    case 1:
	message(0, errno, "failed communicating with remote server");
	goto fatal;

    case -1:
	message(0, errno, "failed communicating with local client");
	goto fatal;
    }

    message(1, 0, "connection closed");

    closesocket(clientFd);
    closesocket(serverFd);
    freeMsgBuf(msg);
    removeHandler(&(argP->addr));
    free(argP);
    incrActiveCount(-1);
#ifdef WIN32
    if (!inLine) _endthread();
#endif
    return;

fatal:
    if (serverFd != -1) closesocket(serverFd);
    if (clientFd != -1) closesocket(clientFd);
    if (exponent) free(exponent);
    if (dhKey) free(dhKey);
    if (sessionKeyStr && sessionKeyStr != secretKeyStr) free(sessionKeyStr);
    if (secretKeyStr) free(secretKeyStr);
    freeMsgBuf(msg);
    removeHandler(&(argP->addr));
    free(argP);
    incrActiveCount(-1);
#ifdef WIN32
    if (!inLine) _endthread();
#endif
    inLine = 0; /* Use it to shut up the compiler ... */
    return;
}

/*
** serverListener
**
** This is top-level server routine that listens for incoming connections
** and then spawns an individual process/thread the handle that single
** client.
*/

void
serverListener(unsigned short *portPtr)
{
    int listenFd;
    int clientFd;
    struct sockaddr_in addr;
    int addrLen;
    char ipBuf[IP_BUF_SIZE];


    /* Create the listener socket */

    if ((listenFd = makeListener(portPtr, ListenIp, 0)) == -1)
    {
	message(0, 0, "server can't listen on port %hu", *portPtr);
	exit(EXIT_FAILURE);
    }

    /* Detach from terminal, if requested */

    if (IsDetached)
    {
	message(3, 0, "detaching from terminal");
	makeDetached();
    }

    while (1)
    {
	message(1, 0, "waiting for connection on port %hu", *portPtr);

	memset(&addr, 0, sizeof(addr));
	addrLen = sizeof(struct sockaddr_in);
	if ((clientFd = accept(listenFd, (struct sockaddr *)&addr, &addrLen)) < 0)
	{
	    message(0, errno, "error on accept");
	}
	else
	{
	    message(1, 0, "accepted connection from %s", ipString(addr.sin_addr, ipBuf));

	    /* Set the "don't linger on close" option */

	    setNoLinger(clientFd);

	    /* Set "keep alive" to reap defunct connections */

	    setKeepAlive(clientFd);

	    /* Create the handler process/thread */

	    spawnHandler(server, listenFd, clientFd, Debug, &addr, 0);
	}
    }
}

void
serverInitiator(char *clientHost, unsigned short port, unsigned short timeout)
{
    int clientFd = -1;
    struct timeval delay;
    fd_set testSet;
    int ready;


    while (1)
    {
	message(2, 0, "initiating connection back to client at %s:%hu", clientHost, port);

	if ((clientFd = makeConnection(clientHost, port, 0, 1, NULL, NULL)) == -1)
	{
	    message(0, errno, "failed to connect back to client at %s:%hu", clientHost, port);
	    exit(EXIT_FAILURE);
	}

	/*
	** Now is the time to detach, if we are going to do so. The check
	** against -1 is so that we don't try to do this multiple times.
	*/

	if (IsDetached && IsDetached != -1)
	{
	    message(3, 0, "detaching from terminal");
	    makeDetached();
	}

	/*
	** Now we will wait until either there is data ready to
	** read from the client or we exceed the timeout value.
	*/

	delay.tv_sec = timeout;
	delay.tv_usec = 0;

	FD_ZERO(&testSet);
	FD_SET(clientFd, &testSet);

	ready = select(clientFd + 1, &testSet, 0, 0, &delay);

	if (ready == 0)
	{
	    message(0, 0, "timed out waiting for accepted connection from client");
	    break;
	}

	/* Check for error but ignore interrupted system calls */

	if (ready < 0)
	{
	    if (errno != EINTR)
	    {
		message(0, errno, "error in select waiting for client to accept connection");
		break;
	    }
	}
	else
	{
	    spawnHandler(server, -1, clientFd, 0, NULL, 0);
	}
    }

    closesocket(clientFd);

    /* Wait for all other threads to exit */

    waitForInactivity();
}

/*
** server
**
** This is the server side of the Zebedee protocol. It should match the
** interactions in client(). Note that this routine IS thread-safe and
** should not call exit() directly.
*/

void
server(FnArgs_t *argP)
{
    int clientFd = argP->fd;
    int localFd = -1;
    unsigned short request = 0;
    unsigned short response = 0;
    unsigned short cmpInfo = CompressInfo;
    unsigned short keyBits = KeyLength;
    unsigned short port = 0;
    unsigned short protocol = DFLT_PROTOCOL;
    unsigned short maxSize = MaxBufSize;
    char clientDhKey[MAX_LINE_SIZE];
    char *exponent = NULL;
    char *dhKey = NULL;
    char *secretKeyStr = NULL;
    char *sessionKeyStr = NULL;
    int len = -1;
    MsgBuf_t *msg = NULL;
    unsigned long token = 0;
    unsigned char hdrData[HDR_SIZE_MAX];
    unsigned short hdrSize;
    struct sockaddr_in localAddr;
    struct sockaddr_in peerAddr;
    unsigned char clientNonce[NONCE_SIZE];
    unsigned char serverNonce[NONCE_SIZE];
    char *targetHost = TargetHost;
    int inLine = argP->inLine;
    int udpMode = argP->udpMode;    /* Overridden by client request */
    char ipBuf[IP_BUF_SIZE];


    incrActiveCount(1);
    message(3, 0, "server routine entered");

    /*
    ** Validate the client IP address, if required. Note that this also
    ** retrieves the client address information, which we may need later.
    */

    message(3, 0, "validating client IP address");
    if (!checkPeerAddress(clientFd, &peerAddr))
    {
	message(0, 0, "client connection from %s disallowed", ipString(peerAddr.sin_addr, ipBuf));
	goto fatal;
    }

    /* Read protocol version */

    message(3, 0, "reading protocol version ");
    
    if (readUShort(clientFd, &request) != 2)
    {
	message(0, errno, "failed reading protocol version");
	goto fatal;
    }

    message(3, 0, "read protocol version %#hx", request);

    /*
    ** If the client protocol version matches one that we can support
    ** then we just echo it back otherwise we send back the highest
    ** that we can support.
    */

    if (request == PROTOCOL_V200 ||
	(request <= PROTOCOL_V102 && request >= PROTOCOL_V100))
    {
	protocol = request;
    }
    else
    {
	/* Set to highest we can support */
	protocol = PROTOCOL_V201;
    }

    message(3, 0, "replying with protocol version %#hx", protocol);
    
    if (writeUShort(clientFd, protocol) != 2)
    {
	message(0, errno, "failed writing protocol version back to client");
	goto fatal;
    }

    /*
    ** For protocol versions 200 and higher we will send as many parameters
    ** as we can in a single block to improve efficiency.
    */

    if (protocol >= PROTOCOL_V200)
    {
	if (protocol >= PROTOCOL_V201)
	{
	    hdrSize = HDR_SIZE_V201;
	}
	else
	{
	    hdrSize = HDR_SIZE_V200;
	}

	if (readData(clientFd, hdrData, hdrSize) != hdrSize)
	{
	    message(0, errno, "failed reading protocol header from client");
	    goto fatal;
	}

	udpMode = (headerGetUShort(hdrData, HDR_OFFSET_FLAGS) == HDR_FLAG_UDPMODE);
	if ((udpMode && !UdpMode) || (!udpMode && !TcpMode))
	{
	    message(0, 0, "client requested %s mode tunnel to %s mode server",
		    (udpMode ? "UDP" : "TCP"), (udpMode ? "TCP" : "UDP"));
	    headerSetUShort(hdrData, (udpMode ? 0 : HDR_FLAG_UDPMODE), HDR_OFFSET_FLAGS);
	}
	else
	{
	    message(3, 0, "replying with %s mode", (udpMode ? "UDP" : "TCP"));
	    headerSetUShort(hdrData, (udpMode ? HDR_FLAG_UDPMODE : 0), HDR_OFFSET_FLAGS);
	}

	maxSize = headerGetUShort(hdrData, HDR_OFFSET_MAXSIZE);
	message(3, 0, "read buffer size request of %hu", maxSize);

	/* Take the smallest of the client and server values */

	if (maxSize > MaxBufSize)
	{
	    maxSize = MaxBufSize;
	}

	message(3, 0, "replying with buffer size %hu", maxSize);
	headerSetUShort(hdrData, maxSize, HDR_OFFSET_MAXSIZE);

	request = headerGetUShort(hdrData, HDR_OFFSET_CMPINFO);
	message(3, 0, "read compression level %#hx", request);

	/*
	** Use the minimum of the client's and server's compression levels.
	**
	** Note that all values for zlib compression are less than those for
	** bzip2 compression so if a client requests bzip2 compression but
	** the server doesn't support it then the protocol degrades naturally
	** to zlib.
	*/

	if (request < cmpInfo)
	{
	    cmpInfo = request;
	    response = request;
	}
	else
	{
	    response = cmpInfo;
	}

	message(3, 0, "replying with compression level %#hx", response);
	headerSetUShort(hdrData, response, HDR_OFFSET_CMPINFO);

	request = headerGetUShort(hdrData, HDR_OFFSET_PORT);
	message(3, 0, "read port %hu", request);

	memset(&localAddr, 0, sizeof(localAddr));
	if (protocol >= PROTOCOL_V201)
	{
	    localAddr.sin_addr.s_addr = htonl(headerGetULong(hdrData, HDR_OFFSET_TARGET) & 0xffffffff);
	    message(3, 0, "read target address %s", ipString(localAddr.sin_addr, ipBuf));
	}

	/*
	** The server should not, in general, redirect arbitrary ports because
	** the remote client will appear to the target service to have
	** connected from the server machine -- and may assume that greater
	** access should be allowed as a result. So we check the requested
	** port and host against the list of allowed port/host combinations.
	** If the request is not granted then we send zero back to the client
	** otherwise we carry on and attempt to open a connection to that
        ** port on the target host. If that succeeds we will send back the
        ** requested port number or zero if it fails.
	*/

	message(3, 0, "checking if redirection is allowed");

	if (allowRedirect(request, &localAddr, udpMode, &targetHost))
	{
	    message(3, 0, "allowed redirection request to %s:%hu", targetHost, request);

	    /*
	    ** Attempt to open connection -- if this fails then we write back
	    ** zero to the client otherwise we send back the requested port
	    ** number.
	    */

	    message(3, 0, "opening connection to port %hu on %s", request, targetHost);

	    memset(&localAddr, 0, sizeof(localAddr));
	    if ((localFd = makeConnection(targetHost, request, udpMode, 0,
					  (Transparent ? &peerAddr : NULL),
					  &localAddr)) == -1)
	    {
		port = 0;
		message(0, errno, "failed connecting to port %hu on %s", request, targetHost);
		headerSetUShort(hdrData, 0, HDR_OFFSET_PORT);
	    }
	    else
	    {
		/* All OK -- echo back port number */

		port = request;
		message(3, 0, "made connection to target -- writing back %hu to client", port);
		headerSetUShort(hdrData, port, HDR_OFFSET_PORT);
	    }
	}
	else
	{
	    message(0, 0, "client requested redirection to a disallowed target (%s:%hu/%s)", ipString(localAddr.sin_addr, ipBuf), request, (udpMode ? "udp" : "tcp"));
	    headerSetUShort(hdrData, 0, HDR_OFFSET_PORT);
	}

	request = headerGetUShort(hdrData, HDR_OFFSET_KEYLEN);
	message(3, 0, "client requested key length %hu", request);

	/*
	** Use the minimum of the client and server values or MinKeyLength,
	** whichever is the greater.
	*/

	if (request > keyBits)
	{
	    response = keyBits;
	}
	else if (request >= MinKeyLength)
	{
	    keyBits = request;
	    response = request;
	}
	else
	{
	    keyBits = MinKeyLength;
	    response = MinKeyLength;
	}

	message(3, 0, "replying with key length %hu", response);
	headerSetUShort(hdrData, response, HDR_OFFSET_KEYLEN);

	token = headerGetULong(hdrData, HDR_OFFSET_TOKEN);
	message(3, 0, "client requested key reuse token %#lx", token);

	if (token != 0)
	{
	    /*
	    ** Search for matching token. If not found then allocate
	    ** a new one.
	    */

	    if ((secretKeyStr = findKeyByToken(&ServerKeyList, token)) == NULL)
	    {
		token = generateToken(&ServerKeyList, token);
	    }
	}
	headerSetULong(hdrData, token, HDR_OFFSET_TOKEN);
	message(3, 0, "returned key reuse token %#lx", token);

	memcpy(clientNonce, hdrData + HDR_OFFSET_NONCE, NONCE_SIZE);
	message(3, 0, "received client nonce %02x%02x...", clientNonce[0], clientNonce[1]);

	generateNonce(serverNonce);
	message(3, 0, "sending server nonce %02x%02x...", serverNonce[0], serverNonce[1]);
	memcpy(hdrData + HDR_OFFSET_NONCE, serverNonce, NONCE_SIZE);

	if (writeData(clientFd, hdrData, hdrSize) != hdrSize)
	{
	    message(0, errno, "failed writing protocol header back to client");
	    goto fatal;
	}
    }
    else
    {
	/*
	** In older protocol versions the fields in the "header" block
	** were sent and acknowledged individually. The basic meanings
	** are the same so the comments have not been duplicated.
	*/

	if (protocol >= PROTOCOL_V101)
	{
	    message(3, 0, "reading message buffer size");

	    if (readUShort(clientFd, &maxSize) != 2)
	    {
		message(0, errno, "failed reading message buffer size");
	    }

	    message(3, 0, "read buffer size request of %hu", maxSize);

	    if (maxSize > MaxBufSize)
	    {
		maxSize = MaxBufSize;
	    }

	    message(3, 0, "replying with buffer size %hu", maxSize);

	    if (writeUShort(clientFd, maxSize) != 2)
	    {
		message(0, 0, "failed to write buffer size back to the client");
		goto fatal;
	    }
	}
	else
	{
	    message(3, 0, "using default buffer size (%hu)", maxSize);
	    maxSize = DFLT_BUF_SIZE;
	}

	message(3, 0, "reading compression level");

	if (readUShort(clientFd, &request) != 2)
	{
	    message(0, errno, "failed reading compression level");
	    goto fatal;
	}
	message(3, 0, "read compression level %#hx", request);

	if (request < cmpInfo)
	{
	    cmpInfo = request;
	    response = request;
	}
	else
	{
	    response = cmpInfo;
	}

	message(3, 0, "replying with compression level %#hx", response);

	if (writeUShort(clientFd, response) != 2)
	{
	    message(0, errno, "failed writing compression level back to client");
	    goto fatal;
	}

	message(3, 0, "reading redirection port");

	if (readUShort(clientFd, &request) != 2)
	{
	    message(0, errno, "failed reading redirection port");
	    goto fatal;
	}
	message(3, 0, "read port %hu", request);

	message(3, 0, "checking if redirection is allowed");

	if (!allowRedirect(request, &localAddr, udpMode, &targetHost))
	{
	    message(0, 0, "client requested redirection to a disallowed port (%s:%hu/%s)", request);
	    writeUShort(clientFd, 0);
	    goto fatal;
	}

	message(3, 0, "allowed redirection request to %hu", request);

	message(3, 0, "opening connection to port %hu on %s", request, targetHost);

	memset(&localAddr, 0, sizeof(localAddr));
	if ((localFd = makeConnection(targetHost, request, udpMode, 0,
				      (Transparent ? &peerAddr : NULL),
				      &localAddr)) == -1)
	{
	    message(0, errno, "failed connecting to port %hu on %s", request, targetHost);
	    writeUShort(clientFd, 0);
	    goto fatal;
	}
	port = request;

	message(3, 0, "made local connection -- writing back %hu to client", request);

	if (writeUShort(clientFd, request) != 2)
	{
	    message(0, errno, "failed writing redirection port back to client");
	    goto fatal;
	}

	message(3, 0, "reading key length");

	if (readUShort(clientFd, &request) != 2)
	{
	    message(0, errno, "failed reading key length");
	    goto fatal;
	}

	message(3, 0, "read key length %hu", request);

	if (request > keyBits)
	{
	    response = keyBits;
	}
	else if (request >= MinKeyLength)
	{
	    keyBits = request;
	    response = request;
	}
	else
	{
	    keyBits = MinKeyLength;
	    response = MinKeyLength;
	}

	message(3, 0, "replying with key length %hu", response);

	if (writeUShort(clientFd, response) != 2)
	{
	    message(0, errno, "failed writing key length back to client");
	    goto fatal;
	}
    }

    /* Quit now if we have no local connection */

    if (port == 0)
    {
	message(0, 0, "initial protocol exchange failed");
	goto fatal;
    }

    /* Allocate message buffer */

    if ((msg = makeMsgBuf(maxSize, cmpInfo)) == NULL)
    {
	message(0, errno, "client failed to allocate message buffer");
	goto fatal;
    }

    /*
    ** For version 200 of the protocol, if the session token requested
    ** by the client resulted in us finding a key string then we will
    ** perform a challenge-response to verify that there really is a
    ** shared key ...
    */

    if (protocol >= PROTOCOL_V200 && secretKeyStr != NULL)
    {
	sessionKeyStr = generateSessionKey(secretKeyStr, clientNonce,
					   serverNonce, keyBits);

	message(3, 0, "session key ends '...%s'", sessionKeyStr + strlen(sessionKeyStr) - 4);

	msg->bfWrite = setupBlowfish(sessionKeyStr, keyBits);
	msg->bfRead = setupBlowfish(sessionKeyStr, keyBits);

	memset(sessionKeyStr, 0, strlen(sessionKeyStr));
	free(sessionKeyStr);
	sessionKeyStr = NULL;
	memset(secretKeyStr, 0, strlen(secretKeyStr));
	free(secretKeyStr);
	secretKeyStr = NULL;

	if (!serverPerformChallenge(clientFd, msg))
	{
	    message(0, 0, "challenge/response failed to validate shared key (session token = %#08x)", token);
	    goto fatal;
	}
    }

    /*
    ** ELSE ...
    ** If the key length is zero and the protocol version is 102 or
    ** higher then we can omit all of the key-exchange traffic.
    */

    else if (keyBits > 0 || protocol < PROTOCOL_V102)
    {

	/*
	** Send the Diffie-Hellman generator
	**
	** The server decides the generator and modulus because to avoid
	** "man-in-the-middle" attacks you are more likely to want to know
	** the identity of the server from the client than vice-versa. This
	** lets the server control its public key identity better.
	*/

	message(3, 0, "sending DH generator '%s'", Generator);

	assert((len = strlen(Generator) + 1) <= MAX_LINE_SIZE);
	setMsgBuf(msg, Generator, strlen(Generator) + 1);

	if (writeMessage(clientFd, msg) != len)
	{
	    message(0, errno, "failed writing generator to client");
	    goto fatal;
	}

	message(3, 0, "sent generator");

	/* Send the Diffie-Hellman modulus */

	message(3, 0, "sending DH modulus '%s'", Modulus);

	assert((len = strlen(Modulus) + 1) <= MAX_LINE_SIZE);
	setMsgBuf(msg, Modulus, strlen(Modulus) + 1);

	if (writeMessage(clientFd, msg) != len)
	{
	    message(0, errno, "failed writing modulus to client");
	    goto fatal;
	}

	message(3, 0, "sent modulus", Modulus);


	/*
	** Calculate our DH key
	**
	** This requires that we first generate our private key (the
	** the exponent) and then perform the modular exponentiation
	** to generate the public key.
	*/

	message(3, 0, "generating private key");

	if ((exponent = generateKey()) == NULL)
	{
	    message(0, errno, "can't generate private key");
	    goto fatal;
	}

	message(3, 0, "private key generated");

	message(3, 0, "generating public DH key");

	if ((dhKey = diffieHellman(Generator, Modulus, exponent)) == NULL)
	{
	    message(0, errno, "can't generate public DH key");
	    goto fatal;
	}

	message(3, 0, "public DH key is '%s'", dhKey);

	/* Now send the DH key */

	message(3, 0, "sending public DH key");

	assert((len = strlen(dhKey) + 1) <= MAX_LINE_SIZE);
	setMsgBuf(msg, dhKey, strlen(dhKey) + 1);

	if (writeMessage(clientFd, msg) != len)
	{
	    message(0, errno, "failed writing DH key to client");
	    goto fatal;
	}

	message(3, 0, "sent public DH key");


	/* Read the client DH key */

	message(3, 0, "reading client DH key");

	if (readMessage(clientFd, msg, MAX_LINE_SIZE) <= 0)
	{
	    message(0, errno, "failed reading client DH key");
	    goto fatal;
	}
	getMsgBuf(msg, clientDhKey, MAX_LINE_SIZE);

	message(3, 0, "accepted client DH key '%s'", clientDhKey);


	/*
	** If requested, check this against a list of "known" keys
	** to validate the client identity.
	*/

	if (IdentityFile)
	{
	    message(3, 0, "checking key against identity file '%s'", IdentityFile);

	    if (!checkIdentity(IdentityFile, Generator, Modulus, clientDhKey))
	    {
		message(0, 0, "client's key identity not found in '%s'", IdentityFile);
		goto fatal;
	    }
	}


	/* Now generate the shared secret key */

	message(3, 0, "generating shared secret key");

	secretKeyStr = diffieHellman(clientDhKey, Modulus, exponent);

	message(3, 0, "shared key ends '...%s'", secretKeyStr + strlen(secretKeyStr) - 4);

	if (protocol >= PROTOCOL_V200)
	{
	    sessionKeyStr = generateSessionKey(secretKeyStr, clientNonce,
					       serverNonce, keyBits);

	    message(3, 0, "session key ends '...%s'", sessionKeyStr + strlen(sessionKeyStr) - 4);
	}
	else
	{
	    sessionKeyStr = secretKeyStr;
	}

	message(3, 0, "initialising encryption state");

	msg->bfWrite = setupBlowfish(sessionKeyStr, keyBits);
	msg->bfRead = setupBlowfish(sessionKeyStr, keyBits);

	/* Clear unneeded values */

	free(dhKey);
	dhKey = NULL;
	memset(exponent, 0, strlen(exponent));
	free(exponent);
	exponent = NULL;

	/*
	** Having established the encrypted channel we now perform a mutual
	** challenge/response dialogue in order to guard against being spoofed
	** by a replay attack if we are using a static key.
	*/

	if (!serverPerformChallenge(clientFd, msg))
	{
	    goto fatal;
	}

	/*
	** If we are at protocol version 200 or higher and a new session
	** token was allocated then update the secret key list.
	*/

	if (protocol >= PROTOCOL_V200 && token != 0)
	{
	    message(3, 0, "new reusable key token established (%#lx)", token);
	    addKeyInfoToList(&ServerKeyList, token, secretKeyStr);
	}

	if (sessionKeyStr != secretKeyStr)
	{
	    memset(sessionKeyStr, 0, strlen(sessionKeyStr));
	    free(sessionKeyStr);
	    sessionKeyStr = NULL;
	}
	memset(secretKeyStr, 0, strlen(secretKeyStr));
	free(secretKeyStr);
	secretKeyStr = NULL;
    }
    else
    {
	message(3, 0, "key length is zero, omitting key exchange");
	if (IdentityFile != NULL)
	{
	    message(1, 0, "Warning: agreed key length is zero, no identity checking performed");
	}
    }

    message(1, 0, "tunnel established to target %s, port %hu", targetHost, port);
    message(2, 0, "compression level %#hx, key length %hu", cmpInfo, keyBits);

    /* Now loop handling i/o */

    message(3, 0, "entering filter loop");

    switch (filterLoop(localFd, clientFd, msg, &localAddr, &peerAddr, localFd, udpMode))
    {
    case 1:
	message(0, errno, "failed communicating with remote client");
	goto fatal;

    case -1:
	message(0, errno, "failed communicating with local server");
	goto fatal;
    }

    errno = 0;
    message(1, 0, "connection closed");

    closesocket(clientFd);
    closesocket(localFd);
    if (targetHost != NULL && targetHost != TargetHost) free(targetHost);
    freeMsgBuf(msg);
    free(argP);
    incrActiveCount(-1);
#ifdef WIN32
    if (!inLine) _endthread();
#endif
    return;

fatal:
    if (clientFd != -1) closesocket(clientFd);
    if (localFd != -1) closesocket(localFd);
    if (targetHost != NULL && targetHost != TargetHost) free(targetHost);
    if (exponent) free(exponent);
    if (dhKey) free(dhKey);
    if (sessionKeyStr && sessionKeyStr != secretKeyStr) free(sessionKeyStr);
    if (secretKeyStr) free(secretKeyStr);
    freeMsgBuf(msg);
    free(argP);
    incrActiveCount(-1);
#ifdef WIN32
    if (!inLine) _endthread();
#endif
    inLine = 0; /* Use it to shut up the compiler ... */
}

/*****************************\
**			     **
**  Option Parsing Routines  **
**			     **
\*****************************/

/*
** scanPortRange
**
** Parse a port range specifier and place the high and low ends of the
** range in hiP and loP. A range can be two shorts integers separated by
** a "-" -- for example "5900-5910", a single number or a symbolic port name.
** In the latter two cases both hiP and loP are set the same. If the range
** is suffixed by /tcp or /udp then the typeP is set accordingly otherwise
** it is set to both TCP and UDP.
**
** If either loP or hiP are NULL the corresponding value is not set.
** The command always returns the vale of the low port.
*/

unsigned short
scanPortRange(const char *str, unsigned short *loP, unsigned short *hiP, unsigned short *typeP)
{
    struct servent *entry = NULL;
    unsigned short loVal = 0;
    unsigned short hiVal = 0;
    char portName[MAX_LINE_SIZE];
    char *slash = NULL;
    unsigned short type = PORTLIST_ANY;


    if ((slash = strchr(str, '/')) != NULL)
    {
	if (!strcasecmp(slash, "/tcp"))
	{
	    type = PORTLIST_TCP;
	}
	else if (!strcasecmp(slash, "/udp"))
	{
	    type = PORTLIST_UDP;
	}
	else
	{
	    message(0, 0, "invalid port type (%s)", slash);
	    return 0;
	}
    }
    if (typeP) *typeP = type;

    switch (sscanf(str, "%hu-%hu", &loVal, &hiVal))
    {
    case 0:
	break;

    case 1:
	hiVal = loVal;

	/* Fall through */

    case 2:
	if (hiVal < loVal)
	{
	    hiVal ^= loVal;
	    hiVal = hiVal ^ loVal;
	    hiVal ^= loVal;
	}

	if (hiVal != loVal && !hiP)
	{
	    message(0, 0, "port range found where single value expected");
	    return 0;
	}

	if (loP) *loP = loVal;
	if (hiP) *hiP = hiVal;

	return loVal;
    }

    if (sscanf(str, "%[^/]", portName) != 1)
    {
	message(0, 0, "missing port name");
	return 0;
    }

    if ((entry = getservbyname(portName, (type == PORTLIST_UDP ? "udp" : "tcp"))) == NULL)
    {
	message(0, errno, "can't find port name entry for '%s'", portName);
	return 0;
    }

    loVal = ntohs(entry->s_port);
    if (loP) *loP = loVal;
    if (hiP) *hiP = loVal;

    return loVal;
}

/*
** setBoolean
**
** Parse a boolean value (either "true" or "false") and set the supplied
** result appropriately.
*/

void
setBoolean(char *value, int *resultP)
{
    if (strcasecmp(value, "true") == 0)
    {
	*resultP = 1;
    }
    else if (strcasecmp(value, "false") == 0)
    {
	*resultP = 0;
    }
    else
    {
	message(0, 0, "can't parse boolean value '%s'", value);
    }
}

/*
** setUShort
**
** Parse and set an unsigned short value
*/

void
setUShort(char *value, unsigned short *resultP)
{
    if (sscanf(value, "%hu", resultP) != 1)
    {
	message(0, 0, "can't parse unsigned short value '%s'", value);
    }
}

/*
** setPort
**
** Parse a port name or number and set an unsigned short value
*/

void
setPort(char *value, unsigned short *resultP)
{
    unsigned short port;


    port = scanPortRange(value, NULL, NULL, NULL);
    if (port == 0)
    {
	message(0, 0, "can't parse port value '%s' key", value);
    }
    else
    {
	*resultP = port;
    }
}

/*
** newPortList
**
** Allocate a new PortList_t structure (or list of structures if there are
** multiple addresses for the supplied host name).
*/

PortList_t *
newPortList(unsigned short lo, unsigned short hi, char *host, unsigned short type)
{
    PortList_t *new = NULL;
    struct sockaddr_in addr;
    struct in_addr *addrList = NULL;
    unsigned long mask = 0xffffffff;


    if (host && !getHostAddress(host, &addr, &addrList, &mask))
    {
	message(0, 0, "can't resolve host or address '%s'", host);
	return NULL;
    }

    if (addrList)
    {
	new = allocPortList(lo, hi, host, &(addr.sin_addr), addrList, mask, type);
    }
    else
    {
	new = allocPortList(lo, hi, NULL, NULL, NULL, 0xffffffff, type);
    }

    return new;
}

/*
** allocPortList
**
** Allocate a new PortList_t structure and initialize the hi and lo elements.
** If host is not NULL then we populate the addr element with the supplied
** address and save the hostname.
*/

PortList_t *
allocPortList(unsigned short lo,
	      unsigned short hi, char *host,
	      struct in_addr *addrP,
	      struct in_addr *addrList,
	      unsigned long mask,
	      unsigned short type)
{
    PortList_t *new = NULL;


    if ((new = (PortList_t *)malloc(sizeof(PortList_t))) == NULL)
    {
	return NULL;
    }

    new->lo = lo;
    new->hi = hi;
    memset(&(new->addr), 0, sizeof(struct in_addr));
    new->host = NULL;
    if (host && addrP)
    {
	memcpy(&(new->addr.sin_addr), addrP, sizeof(struct in_addr));
	if ((new->host = (char *)malloc(strlen(host) + 1)) == NULL)
	{
	    message(0, errno, "out of memory");
	    return NULL;
	}
	strcpy(new->host, host);
    }
    new->addrList = addrList;
    new->mask = mask;
    new->type = type;

    new->next = NULL;

    return new;
}

/*
** setPortList
**
** Parse a list of white-space or comma separated ports or port ranges
** and add them to listP. Each element of this list is a low-high pair
** of port numbers. Where single ports are specified both low and high
** are the same. If zeroOk is false then a list containing port 0 is
** not allowed.
**
** The host parameter is passed on to newPortList.
*/

void
setPortList(char *value, PortList_t **listP, char *host, int zeroOk)
{
    PortList_t *new = NULL;
    char *token = NULL;
    char tmpBuf[MAX_LINE_SIZE];
    char *tmpPtr = NULL;
    unsigned short lo = 0;
    unsigned short hi = 0;
    PortList_t *last = *listP;
    unsigned short type = PORTLIST_ANY;


    /* Set "last" to point to the last element of the list */

    while (last && last->next)
    {
	last = last->next;
    }

    token = value;
    while (*token)
    {
	/* Skip whitespace and commas */

	while (*token && (isspace(*token) || *token == ',')) token++;
	if (!*token) break;

	/* Copy the token */

	tmpPtr = tmpBuf;
	while (*token && !(isspace(*token) || *token == ','))
	{
	    *tmpPtr++ = *token++;
	}
	*tmpPtr = '\0';

	if (scanPortRange(tmpBuf, &lo, &hi, &type) != 0 || zeroOk)
	{
	    /* Allocate new list element */

	    if ((new = newPortList(lo, hi, host, type)) == NULL)
	    {
		message(0, errno, "failed allocating memory for port list");
		exit(EXIT_FAILURE);
	    }

	    /* Add to the end of the list */

	    if (*listP == NULL)
	    {
		*listP = new;
	    }
	    else
	    {
		last->next = new;
	    }
	    last = new;
	}
	else
	{
	    message(0, 0, "invalid port range '%s'", tmpBuf);
	}
    }
}

/*
** setTarget
**
** The target value is either of the form "hostname:portlist" or just a
** plain hostname. In the latter case it will use the default port list
** (held in AllowedDefault) and also become the default target. Note that
** the last named target becomes the default.
*/

void
setTarget(char *value)
{
    char target[MAX_LINE_SIZE];
    char portList[MAX_LINE_SIZE];


    if (sscanf(value, "%[^:]:%s", target, portList) == 2)
    {
	setPortList(portList, &AllowedTargets, target, 0);
    }
    else
    {
	setPortList("0", &AllowedTargets, target, 1);
    }

    /* Set the default target host */

    setString(target, &TargetHost);
}

/*
** setTunnel
**
** Parse a complete client tunnel specification.
**
** A tunnel spec is of the form "clientports:targethost:targetports".
*/

void
setTunnel(char *value)
{
    char clientList[MAX_LINE_SIZE];
    char hostName[MAX_LINE_SIZE];
    char targetList[MAX_LINE_SIZE];


    if (sscanf(value, "%[^:]:%[^:]:%[^:]", clientList, hostName, targetList) == 3)
    {
	setPortList(clientList, &ClientPorts, NULL, 0);
	if (ServerHost == NULL)
	{
	    setString(hostName, &ServerHost);
	}
	if (strcmp(hostName, "*") == 0)
	{
	    setPortList(targetList, &TargetPorts, NULL, 0);
	}
	else
	{
	    setPortList(targetList, &TargetPorts, hostName, 0);
	}
    }
    else if (sscanf(value, "%[^:]:%[^:]", hostName, targetList) == 2)
    {
	if (ServerHost == NULL)
	{
	    setString(hostName, &ServerHost);
	}
	if (strcmp(hostName, "*") == 0)
	{
	    setPortList(targetList, &TargetPorts, NULL, 0);
	}
	else
	{
	    setPortList(targetList, &TargetPorts, hostName, 0);
	}
	if (countPorts(TargetPorts) != 1)
	{
	    message(0, 0, "target port list contains more than one port");
	    exit(EXIT_FAILURE);
	}
    }
    else
    {
	if (ServerHost == NULL)
	{
	    setString(value, &ServerHost);
	}
	else
	{
	    message(0, 0, "invalid tunnel specification '%s'", value);
	}
    }
}

/*
** setAllowedPeer
**
** The value is either of the form "address:portlist" or just a
** plain address. Addresses can be include CIDR mask specifications.
*/

void
setAllowedPeer(char *value)
{
    char addr[MAX_LINE_SIZE];
    char portList[MAX_LINE_SIZE];


    if (sscanf(value, "%[^:]:%s", addr, portList) == 2)
    {
	setPortList(portList, &AllowedPeers, addr, 0);
    }
    else
    {
	setPortList("0", &AllowedPeers, addr, 1);
    }
}

/*
** setString
**
** Set a character string value
*/

void
setString(char *value, char **resultP)
{
    if ((*resultP = (char *)malloc(strlen(value) + 1)) == NULL)
    {
	message(0, errno, "failed allocating space for string value '%s'", value);
	exit(EXIT_FAILURE);
    }

    strcpy(*resultP, value);
}

/*
** setLogFile
**
** Open the specified log-file for append, closing any currently open
** log.
**
** This routine recognises the special names "NULL" and "SYSLOG".
*/

void
setLogFile(char *newFile)
{
    if (LogFileP != NULL)
    {
	fclose(LogFileP);
    }

    if (strcmp(newFile, "NULL") == 0)
    {
	LogFileType = LOGFILE_NULL;
	LogFileP = NULL;
    }
    else if (strcmp(newFile, "SYSLOG") == 0)
    {
	LogFileType = LOGFILE_SYSLOG;
	LogFileP = NULL;
    }
    else
    {
	LogFileType = LOGFILE_LOCAL;
	if ((LogFileP = fopen(newFile, "a")) == NULL)
	{
	    message(0, errno, "can't open log file '%s'", newFile);
	}
    }
}

/*
** setCmpInfo
**
** Set compression type and level
*/

void
setCmpInfo(char *value, unsigned short *resultP)
{
    unsigned short level = 0;
    unsigned short type = CMPTYPE_ZLIB;

    if (sscanf(value, "zlib:%hu", &level) == 1)
    {
	type = CMPTYPE_ZLIB;
    }
    else if (sscanf(value, "bzip2:%hu", &level) == 1)
    {
#ifdef DONT_HAVE_BZIP2
	message(1, 0, "Warning: bzip2 compression not supported -- using zlib");
	type = CMPTYPE_ZLIB;
#else
	type = CMPTYPE_BZIP2;

	/* Compression level > 1 is useless as data buffers are too small */

	if (level > 1) level = 1;
#endif
    }
    else if (sscanf(value, "%hu", &level) == 1)
    {
	type = CMPTYPE_ZLIB;
    }
    else
    {
	message(0, 0, "invalid compression specification '%s'", value);
	level = DFLT_CMP_LEVEL;
    }

    if (level > 9)
    {
	message(0, 0, "compression level out of range (%s)", value);
	level = DFLT_CMP_LEVEL;
    }

    if (level == 0)
    {
	type = CMPTYPE_ZLIB;
    }

    *resultP = level;
    *resultP = SET_CMPTYPE(*resultP, type);
}

/*
** readConfigFile
**
** Read a configuration file. If the level is greater than MAX_LEVEL the
** recursive inclusion has probably been detected.
*/

void
readConfigFile(const char *fileName, int level)
{
    FILE *fp = NULL;
    char lineBuf[MAX_LINE_SIZE];
    char *curPtr = NULL;
    int size = 0;
    int len = 0;
    int lineNo = 0;


    message(2, 0, "reading config file '%s' at level %d", fileName, level);

    if (level > MAX_INCLUDE)
    {
	message(0, 0, "include file nesting too deep (> %d)", MAX_INCLUDE);
	return;
    }

    if ((fp = fopen(fileName, "r")) == NULL)
    {
	message(0, errno, "can't open config file '%s'", fileName);
	return;
    }

    curPtr = lineBuf;
    size = MAX_LINE_SIZE;

    while (fgets(curPtr, size, fp) != NULL)
    {
	lineNo++;

	len = strlen(curPtr) - 1;

	/* Strip new-line */

	if (curPtr[len] == '\n')
	{
	    curPtr[len--] = '\0';
	}
	else if (!feof(fp))
	{
	    message(0, 0, "line too long in config file '%s' at line %d", fileName, lineNo);
	    break;
	}

	message(4, 0, "line %d: %s", lineNo, curPtr);

	/* Look for continued lines */

	if (curPtr[len] == '\\')
	{
	    curPtr[len] = '\0';
	    size -= len;
	    curPtr += len;

	    /* Go and read some more */

	    continue;
	}

	/* Reset for next time */

	curPtr = lineBuf;
	size = MAX_LINE_SIZE;

	if (!parseConfigLine(lineBuf, level))
	{
	    message(0, 0, "invalid line in config file '%s' at line %d", fileName, lineNo);
	}
    }

    fclose(fp);
}

/*
** parseConfigLine
**
** Parse a single non-continued line from a config file.
**
** Returns 1 if it was OK and 0 otherwise. Yes, I know that multiple
** returns are not terribly good practice but I don't much care here ...
*/

int
parseConfigLine(const char *lineBuf, int level)
{
    char key[MAX_LINE_SIZE];
    char value[MAX_LINE_SIZE];
    char comment[2];
    char tmpBuf[MAX_LINE_SIZE];
    const char *s = NULL;
    char *t = NULL;


    /* Substitute field separator, if any */

    if (FieldSeparator)
    {
	for (s = lineBuf, t = tmpBuf; *s; s++, t++)
	{
	    if (*s == *FieldSeparator)
	    {
		*t = ' ';
	    }
	    else
	    {
		*t = *s;
	    }
	}
	*t = '\0';
	lineBuf = tmpBuf;
    }

    /* Split into key-value pairs */

    *comment = '#';
    *key = '#';
    if (sscanf(lineBuf, "%s \"%[^\"]\" %1s", key, value, comment) < 2 &&
	sscanf(lineBuf, "%s \'%[^\']' %1s", key, value, comment) < 2 &&
	sscanf(lineBuf, "%s %s %1s", key, value, comment) < 2)
    {
	/*
	** Return OK if this is blank or a comment but flag an error
	** otherwise (if it was blank 'key' will be unchanged -- still
	** a comment character).
	*/

	if (*key != '#')
	{
	    return 0;
	}
	return 1;
    }

    /* Skip pure comment lines (that matched the scanf pattern) */

    if (*key == '#')
    {
	return 1;
    }
	
    /* Third field if present must be a comment */

    if (*comment != '#')
    {
	return 0;
    }

    message(4, 0, "key = '%s', value = '%s'", key, value);

    /* Now check for all possible keywords */

    if (!strcasecmp(key, "server")) setBoolean(value, &IsServer);
    else if (!strcasecmp(key, "detached")) setBoolean(value, &IsDetached);
    else if (!strcasecmp(key, "debug")) setBoolean(value, &Debug);
    else if (!strcasecmp(key, "compression")) setCmpInfo(value, &CompressInfo);
    else if (!strcasecmp(key, "keylength")) setUShort(value, &KeyLength);
    else if (!strcasecmp(key, "minkeylength")) setUShort(value, &MinKeyLength);
    else if (!strcasecmp(key, "maxbufsize")) setUShort(value, &MaxBufSize);
    else if (!strcasecmp(key, "verbosity")) setUShort(value, &LogLevel);
    else if (!strcasecmp(key, "serverport")) setPort(value, &ServerPort);
    else if (!strcasecmp(key, "localport")) setPortList(value, &ClientPorts, NULL, 0);
    else if (!strcasecmp(key, "clientport")) setPortList(value, &ClientPorts, NULL, 0);
    else if (!strcasecmp(key, "remoteport")) setPortList(value, &TargetPorts, NULL, 0);
    else if (!strcasecmp(key, "targetport")) setPortList(value, &TargetPorts, NULL, 0);
    else if (!strcasecmp(key, "remotehost")) setString(value, &ServerHost);
    else if (!strcasecmp(key, "serverhost")) setString(value, &ServerHost);
    else if (!strcasecmp(key, "command"))
    {
	setString(value, &CommandString);
	MultiUse = 0;
    }
    else if (!strcasecmp(key, "keygencommand")) setString(value, &KeyGenCmd);
    else if (!strcasecmp(key, "logfile")) setLogFile(value);
    else if (!strcasecmp(key, "timestamplog")) setBoolean(value, &TimestampLog);
    else if (!strcasecmp(key, "multiuse")) setBoolean(value, &MultiUse);
    else if (!strcasecmp(key, "include")) readConfigFile(value, level+1);
    else if (!strcasecmp(key, "modulus")) setString(value, &Modulus);
    else if (!strcasecmp(key, "generator")) setString(value, &Generator);
    else if (!strcasecmp(key, "privatekey")) setString(value, &PrivateKey);
    else if (!strcasecmp(key, "checkidfile")) setString(value, &IdentityFile);
    else if (!strcasecmp(key, "checkaddress")) setAllowedPeer(value);
    else if (!strcasecmp(key, "redirect"))
    {
	if (!strcasecmp(value, "none"))
	{
	    /*
	    ** Special case of "none" disables default target ports.
	    ** Yes, I know there's a potential memory leak. No, it doesn't
	    ** matter!
	    */

	    AllowedDefault = NULL;
	    setPortList("0-0", &AllowedDefault, NULL, 1);
	}
	else
	{
	    setPortList(value, &AllowedDefault, NULL, 0);
	}
    }
    else if (!strcasecmp(key, "message")) message(1, 0, "%s", value);
    else if (!strcasecmp(key, "name")) setString(value, &Program);
    else if (!strcasecmp(key, "keygenlevel")) setUShort(value, &KeyGenLevel);
    else if (!strcasecmp(key, "redirecthost")) setString(value, &TargetHost);
    else if (!strcasecmp(key, "targethost")) setTarget(value);
    else if (!strcasecmp(key, "keylifetime")) setUShort(value, &KeyLifetime);
    else if (!strcasecmp(key, "udpmode"))
    {
	setBoolean(value, &UdpMode);
	TcpMode = !UdpMode;
    }
    else if (!strcasecmp(key, "ipmode"))
    {
	if (!strcasecmp(value, "tcp"))
	{
	    TcpMode = 1;
	    UdpMode = 0;
	}
	else if (!strcasecmp(value, "udp"))
	{
	    TcpMode = 0;
	    UdpMode = 1;
	}
	else if (!strcasecmp(value, "both") || !strcasecmp(value, "mixed"))
	{
	    TcpMode = 1;
	    UdpMode = 1;
	}
	else
	{
	    message(0, 0, "invalid value for ipmode: %s", value);
	    return 0;
	}
    }
    else if (!strcasecmp(key, "udptimeout")) setUShort(value, &UdpTimeout);
    else if (!strcasecmp(key, "tcptimeout")) setUShort(value, &TcpTimeout);
    else if (!strcasecmp(key, "idletimeout"))
    {
	setUShort(value, &TcpTimeout);
	setUShort(value, &UdpTimeout);
    }
    else if (!strcasecmp(key, "localsource"))
    {
	int yesNo = 0;
	setBoolean(value, &yesNo);
	setString(yesNo ? "127.0.0.1" : "0.0.0.0", &ListenIp);
    }
    else if (!strcasecmp(key, "listenip")) setString(value, &ListenIp);
    else if (!strcasecmp(key, "listenmode")) setBoolean(value, &ListenMode);
    else if (!strcasecmp(key, "clienthost")) setString(value, &ClientHost);
    else if (!strcasecmp(key, "connecttimeout")) setUShort(value, &ConnectTimeout);
    else if (!strcasecmp(key, "readtimeout")) setUShort(value, &ReadTimeout);
    else if (!strcasecmp(key, "target")) setTarget(value);
    else if (!strcasecmp(key, "tunnel")) setTunnel(value);
    else if (!strcasecmp(key, "transparent")) setBoolean(value, &Transparent);
    else if (!strcasecmp(key, "httpproxy"))
    {
	setString(value, &ProxyHost);
	if (sscanf(value, "%[^:]:%hu", ProxyHost, &ProxyPort) != 2)
	{
	    message(0, 0, "invalid httpproxy specification: %s", value);
	    ProxyHost = NULL;
	}
     }
    else if (!strcasecmp(key, "sharedkey")) setString(value, &SharedKey);
    else if (!strcasecmp(key, "sharedkeygencommand")) setString(value, &SharedKeyGenCmd);
    else if (!strcasecmp(key, "dumpdata")) setBoolean(value, &DumpData);
    else
    {
	return 0;
    }

    return 1;
}

/*
** cleanHexStr
**
** Canonicalize a hexadecimal string converting to lower case and
** eliminating white-space.
*/

char *
cleanHexString(char *str)
{
    char *newStr = (char *)malloc(strlen(str) + 1);
    char *newp = newStr;
    char *oldp = str;

    while (*oldp)
    {
	if (isxdigit(*oldp))
	{
	    *newp++ = tolower(*oldp++);
	}
	else if (isspace(*oldp))
	{   
	    oldp++;
	}
	else
	{
	    message(0, 0, "invalid character in hex string '%s'", str);
	    break;
	}
    }
    *newp = '\0';

    return newStr;
}

/*
** usage
**
** Print out usage message and exit
*/

void
usage(void)
{
    fprintf(stderr, "Zebedee -- A Secure Tunnel Program: Release %s\n", RELEASE_STR);
    fprintf(stderr, "Copyright (c) 1999, 2000, 2001, 2002 by Neil Winton. All Rights Reserved.\n");
    fprintf(stderr,
	    "This program is free software and may be distributed under the terms of the\n"
	    "GNU General Public License, Version 2.\n");
    fprintf(stderr, "Zebedee comes with ABSOLUTELY NO WARRANTY.\n\n");
    fprintf(stderr,
	    "Client: %s [options] [[clientports:]serverhost[:targetports]]\n"
	    "Server: %s [options] -s [targethost]\n"
	    "Key generation:  %s -p | -P [-f file]\n"
	    "Hashing: %s -H string ... | -h [file ...]\n"
#ifdef WIN32
	    "Service: %s [options] -S [install[=config-file] | remove | run]\n",
	    Program
#endif
	    , Program, Program, Program, Program);

    fprintf(stderr,
	    "Options are:\n"
	    "    -b address  Bind only this address when listening for connections\n"
	    "    -c host     Server initiates connection to client host\n"
	    "    -D          Debug mode\n"
	    "    -d          Do not detach from terminal\n"
	    "    -e command  Run command connected to local port (client only)\n"
	    "    -F char     Specify additional field separator character\n"
	    "    -f file     Read configuration file\n"
	    "    -H          Generate hash of string values\n"
	    "    -h          Generate hash of file contents\n"
	    "    -k keybits  Specify key length in bits\n"
	    "    -l          Client listens for server connection\n"
	    "    -m          Client accepts multiple connections (default)\n"
	    "    -n name     Specify program name\n"
	    "    -o file     Log output to specified file\n"
	    "    -p          Generate private key\n"
	    "    -P          Generate public key \"fingerprint\"\n"
	    "    -r ports    Specify allowed port redirection list (server only)\n"
	    "    -s          Run as a server\n"
#ifdef WIN32
	    "    -S option   Install/remove/run service\n"
#endif
	    "    -t          Timestamp log entries\n"
	    "    -T port     Specify the server (tunnel) port\n"
	    "    -u          Run in UDP mode\n"
	    "    -U          Run in TCP and UDP mode\n"
	    "    -v level    Set message verbosity level (default 1)\n"
	    "    -x config   Extended configuration statement\n"
	    "    -z type     Set the compression type and level (default zlib:6)\n");

    exit(EXIT_FAILURE);
}

/*
** sigpipeCatcher
**
** Signal handler to ensure that SIGPIPE is caught but does nothing except
** interrupt a system call and rearms itself.
*/

void
sigpipeCatcher(int sig)
{
#ifdef SIGPIPE
    signal(SIGPIPE, sigpipeCatcher);
#endif
}

/*
** sigchldCatcher
**
** Signal handler to reap "zombie" processes.
*/

void
sigchldCatcher(int sig)
{
#ifdef SIGCHLD
    while (waitpid(-1, NULL, WNOHANG) > 0) /* No further action */;

    signal(SIGCHLD, sigchldCatcher);
#endif
}

/*
** sigusr1Catcher
**
** Signal handler to ensure catch SIGUSR1 and set a flag.
*/

void
sigusr1Catcher(int sig)
{
#ifdef SIGUSR1
    _exit(0);
#endif
}

/******************\
**		  **
**  Main Routine  **
**		  **
\******************/

int
main(int argc, char **argv)
{
    int ch;
    char hostName[MAX_LINE_SIZE];
    int doHash = 0;
    int doPrivKey = 0;
    int doPubKey = 0;
    char hashBuf[HASH_STR_SIZE];
    char *last;
    char *serviceArgs = NULL;


    /* Set program name to the last element of the path minus extension */

    if ((last = strrchr(argv[0], FILE_SEP_CHAR)) != NULL)
    {
	Program = last + 1;
    }
    else
    {
	Program = argv[0];
    }
    if ((last = strrchr(Program, '.')) != NULL)
    {
	*last = '\0';
    }

    /* Initialise critical stuff */

    threadInit();
#ifdef WIN32
    if (WSAStartup(0x0101, &WsaState) != 0)
    {
	message(0, errno, "winsock initailization failed");
    }
#endif

    /* Parse the options! */

    while ((ch = getopt(argc, argv, "b:c:Dde:f:F:hHk:lmn:o:pPr:sS:tT:uUv:x:z:")) != -1)
    {
	switch (ch)
	{
	case 'c':
	    ClientHost = optarg;
	    break;

	case 'b':
	    ListenIp = optarg;
	    break;

	case 'D':
	    Debug = 1;
	    break;

	case 'd':
	    IsDetached = 0;
	    break;

	case 'e':
	    MultiUse = 0;
	    CommandString = optarg;
	    break;

	case 'f':
	    readConfigFile(optarg, 1);
	    break;

	case 'F':
	    FieldSeparator = optarg;
	    break;

	case 'h':
	    if (doPrivKey || doPubKey)
	    {
		message(0, 0, "-h and -p or -P are mutually exclusive");
		exit(EXIT_FAILURE);
	    }
	    doHash = 1;
	    break;

	case 'H':
	    if (doPrivKey || doPubKey)
	    {
		message(0, 0, "-H and -p or -P are mutually exclusive");
		exit(EXIT_FAILURE);
	    }
	    doHash = 2;
	    break;

	case 'k':
	    setUShort(optarg, &KeyLength);
	    break;

	case 'l':
	    ListenMode++;
	    break;

	case 'm':
	    MultiUse++;
	    break;

	case 'n':
	    Program = optarg;
	    break;

	case 'o':
	    setLogFile(optarg);
	    break;

	case 'p':
	    if (doHash)
	    {
		message(0, 0, "-h or -H and -p or -P are mutually exclusive");
		exit(EXIT_FAILURE);
	    }
	    doPrivKey++;
	    break;

	case 'P':
	    if (doHash)
	    {
		message(0, 0, "-h or -H and -p or -P are mutually exclusive");
		exit(EXIT_FAILURE);
	    }
	    doPubKey++;
	    break;

	case 'r':
	    setPortList(optarg, &AllowedDefault, NULL, 0);
	    break;

	case 's':
	    IsServer = 1;
	    break;

	case 'S':
	    serviceArgs = optarg;
	    break;

	case 't':
	    TimestampLog = 1;
	    break;

	case 'T':
	    setUShort(optarg, &ServerPort);
	    break;

	case 'u':
	    UdpMode = 1;
	    TcpMode = 0;
	    break;

	case 'U':
	    UdpMode = 1;
	    TcpMode = 1;
	    break;

	case 'v':
	    LogLevel = strtoul(optarg, NULL, 10);
	    break;

	case 'x':
	    if (!parseConfigLine(optarg, 0))
	    {
		message(0, 0, "invalid extended configuration argument '%s'", optarg);
	    }
	    break;

	case 'z':
	    setCmpInfo(optarg, &CompressInfo);
	    break;

	default:
	    usage();
	    break;
	}
    }

    /*
    ** If we are going to detach now is the time to invoke the workaround
    ** for those people with BUGGY_FORK_WITH_PTHREADS defined.
    */

    if (IsDetached)
    {
	prepareToDetach();
    }
	
    /*
    ** If using reusable session keys then initialize the CurrentToken
    ** to request a new one to be generated on first connection.
    */

    if (KeyLifetime != 0)
    {
	CurrentToken = TOKEN_NEW;
    }

    /*
    ** If the user has explicitly set multi-use mode and a command
    ** string (and you've got to try hard to do it) then we will
    ** complain.
    */

    if (CommandString && MultiUse)
    {
	message(0, 0, "can't specify a command for a multi-use client");
	exit(EXIT_FAILURE);
    }

    /*
    ** Figure out what port to listen on/connect to if not specified.
    */

    if (ServerPort == 0)
    {
	if (UdpMode && !TcpMode)
	{
	    ServerPort = DFLT_UDP_PORT;
	}
	else
	{
	    ServerPort = DFLT_TCP_PORT;
	}
    }

    /*
    ** Clean up and canonicalize the modulus and generator strings.
    ** We do this so that slightly different representations of these
    ** values (different case and white space) will be sent across the
    ** wire in the same form. This is important for the identity
    ** checking feature which calculates a hash of these string values.
    */

    Modulus = cleanHexString(Modulus);
    Generator = cleanHexString(Generator);

    /*
    ** Check the maximum buffer size and reset it if necessary.
    */

    if (MaxBufSize > MAX_BUF_SIZE)
    {
	message(1, 0, "Warning: maximum buffer size must be < %hu, rounded down", MAX_BUF_SIZE);
	MaxBufSize = MAX_BUF_SIZE;
    }
    else if (MaxBufSize == 0)
    {
	message(1, 0, "Warning: buffer size must be > 0, set to %hu", DFLT_BUF_SIZE);
	MaxBufSize = DFLT_BUF_SIZE;
    }

    /*
    ** Check for keylength and minkeylength conflict
    */

    if (KeyLength < MinKeyLength)
    {
	message(1, 0, "Warning: specified keylength (%hu) set to minkeylength (%hu)", KeyLength, MinKeyLength);
	KeyLength = MinKeyLength;
    }

#ifdef SIGPIPE
    /*
    ** Set up a handler for SIGPIPE so that it will interrupt a select()
    ** call but otherwise do nothing.
    */

    signal(SIGPIPE, sigpipeCatcher);
#endif

#ifdef SIGCHLD
    /*
    ** Set up a handler for SIGCHLD that will clean up any defunct
    ** sub-processes when they exit.
    */

    signal(SIGCHLD, sigchldCatcher);
#endif
    
#ifdef WIN32
    if (serviceArgs)
    {
	if (!strncmp(serviceArgs, "install", 7))
	{
	    if (strncmp(serviceArgs, "install=", 8) == 0)
	    {
		exit(svcInstall(Program, serviceArgs + 8));
	    }
	    else
	    {
		exit(svcInstall(Program, NULL));
	    }
	}
	else if (!strcmp(serviceArgs, "remove"))
	{
	    exit(svcRemove(Program));
	}
	else if (strcmp(serviceArgs, "run") != 0)
	{
	    message(0, 0, "invalid argument to -S option: %s", serviceArgs);
	    exit(EXIT_FAILURE);
	}

	/* If it was "run" fall through to the rest ... */
    }
#endif

    /*
    ** What we do next and how we handle additional arguments depends on
    ** what mode we are operating in. With -h or -H we calculate has values
    ** and the arguments are either files or strings. With -p or -P then
    ** we will calculate private/public keys and any extra arguments will
    ** be ignored as they will if this is a server. Finally, for a client
    ** the optional additional argument will be a host/port combination.
    */

    if (doHash)
    {
	/* Hashing: -h or -H */

	if (optind >= argc)
	{
	    /* If there are no arguments with -h then we use stdin ... */

	    if (doHash == 1)
	    {
		hashFile(hashBuf, "-");
		printf("%s\n", hashBuf);
	    }
	    else
	    {
		message(0, 0, "no string argument to hash");
		exit(EXIT_FAILURE);
	    }
	}
	else
	{
	    /* We have arguments -- either strings or filenames */

	    while (optind < argc && argv[optind])
	    {
		if (doHash == 1)
		{
		    hashFile(hashBuf, argv[optind]);
		}
		else
		{
		    hashStrings(hashBuf, argv[optind], NULL);
		}
		printf("%s %s\n", hashBuf, argv[optind]);
		optind++;
	    }
	}
    }
    else if (doPrivKey || doPubKey)
    {
	/* Key generation: -p or -P */

	if (doPrivKey)
	{
	    PrivateKey = generateKey();
	    if (PrivateKey != NULL)
	    {
		printf("privatekey \"%s\"\n", PrivateKey);
	    }
	    else
	    {
		message(0, errno, "can't generate private key");
	    }
	}
	if (doPubKey)
	{
	    if (PrivateKey == NULL)
	    {
		message(0, 0, "can't generate a identity without a private key being set");
		exit(EXIT_FAILURE);
	    }
	    gethostname(hostName, MAX_LINE_SIZE);
	    printf("%s %s\n", generateIdentity(Generator, Modulus, PrivateKey), hostName);
	}
    }
    else if (IsServer)
    {
	/* Server mode -- check for target host arguments */

	while (optind < argc)
	{
	    setTarget(argv[optind]);
	    optind++;
	}

	/*
	** Sanity check the default target. This must be a "pure" hostname
	** without an address mask.
	*/

	if (strchr(TargetHost, '/') != NULL)
	{
	    message(0, 0, "default target host (%s) must not have an address mask", TargetHost);
	    exit(EXIT_FAILURE);
	}

	/*
	** If we have not yet set up the allowed redirection ports then we
	** will only allow redirection to ports on the local machine.
	*/

	if (AllowedTargets == NULL)
	{
	    AllowedTargets = newPortList(0, 0, "localhost", PORTLIST_ANY);
	}

#ifdef WIN32
	if (serviceArgs)
	{
	    svcRun(Program, (VOID (*)(VOID *))serverListener, (VOID *)&ServerPort);
	}
	else
#endif
	if (ClientHost != NULL)
	{
	    serverInitiator(ClientHost, ServerPort, ConnectTimeout);
	}
	else
	{
	    serverListener(&ServerPort);
	}
    }
    else
    {
	/*
	** Client mode -- treat any remaining arguments as tunnel
	** specifications.
	*/

	while (optind < argc)
	{
	    setTunnel(argv[optind]);
	    optind++;
	}

	if (ServerHost == NULL)
	{
	    message(0, 0, "no server host specified");
	    exit(EXIT_FAILURE);
	}

	/*
	** This next check is for compatibility -- allowing the command
	**
	**  zebedee -e 'telnet localhost %d' serverhost
	**
	** to work.
	*/

	if (TargetPorts == NULL)
	{
	    setPortList("telnet", &TargetPorts, ServerHost, 0);
	}

	/*
	** If no local port has been specified then we will default to
	** using "0" -- which means that one should be dynamically
	** allocated. Note that this will only be allowed if there is
	** a single remote port -- see the checks below.
	*/

	if (ClientPorts == NULL)
	{
	    if ((ClientPorts = newPortList(0, 0, NULL, PORTLIST_ANY)) == NULL)
	    {
		message(0, errno, "can't allocate space for port list");
		exit(EXIT_FAILURE);
	    }
	}

	/* Make sure that we have matching local and remote port lists */

	if (countPorts(ClientPorts) != countPorts(TargetPorts))
	{
	    message(0, 0, "the numbers of entries in the client and target port lists do not match");
	    exit(EXIT_FAILURE);
	}

	/*
	** If there is more than one target port specified then multi-use
	** mode is implicit. This also means that a command string can not
	** be specified.
	*/

	if (countPorts(TargetPorts) > 1)
	{
	    MultiUse++;
	    if (CommandString)
	    {
		message(0, 0, "can't specify a command with multiple target ports");
		exit(EXIT_FAILURE);
	    }
	}

	/* At last! Invoke the client listener routine! */

#ifdef WIN32
	if (serviceArgs)
	{
	    svcRun(Program, (VOID (*)(VOID *))clientListener, (void *)ClientPorts);
	}
	else
#endif
	{
	    clientListener(ClientPorts);
	}
    }

    exit(EXIT_SUCCESS);
}
