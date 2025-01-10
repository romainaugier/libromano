#if !defined(__LIBROMANO_ENDIAN)
#define __LIBROMANO_ENDIAN

/* from here: https://gist.githubusercontent.com/panzi/6856583/raw/12f9f02f1298bb0bc054ba667bccc0cf032cdb03/portable_endian.h */

#include "libromano/libromano.h"

#if defined(ROMANO_LINUX)

#include <endian.h>

#elif defined(ROMANO_APPLE)

#include <libkern/OSByteOrder.h>

#define htobe16(x) OSSwapHostToBigInt16(x)
#define htole16(x) OSSwapHostToLittleInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)

#define htobe32(x) OSSwapHostToBigInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)

#define htobe64(x) OSSwapHostToBigInt64(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)

#elif defined(ROMANO_OPENBSD)

#include <endian.h>

#elif defined(ROMANO_NETBSD) || defined(ROMANO_FREEBSD) || defined(ROMANO_DRAGONFLY)

#include <sys/endian.h>

#define be16toh(x) betoh16(x)
#define le16toh(x) letoh16(x)

#define be32toh(x) betoh32(x)
#define le32toh(x) letoh32(x)

#define be64toh(x) betoh64(x)
#define le64toh(x) letoh64(x)

#elif defined(ROMANO_WIN)

#include <winsock2.h>
#ifdef ROMANO_GCC 
#include <sys/param.h>
#endif

#if ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN

#define htobe16(x) htons(x)
#define htole16(x) (x)
#define be16toh(x) ntohs(x)
#define le16toh(x) (x)
 
#define htobe32(x) htonl(x)
#define htole32(x) (x)
#define be32toh(x) ntohl(x)
#define le32toh(x) (x)
 
#define htobe64(x) htonll(x)
#define htole64(x) (x)
#define be64toh(x) ntohll(x)
#define le64toh(x) (x)

#elif ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_BIG_ENDIAN

#define htobe16(x) (x)
#define htole16(x) _byteswap_ushort(x)
#define be16toh(x) (x)
#define le16toh(x) _byteswap_ushort(x)
 
#define htobe32(x) (x)
#define htole32(x) _byteswap_ulong(x)
#define be32toh(x) (x)
#define le32toh(x) _byteswap_ulong(x)
 
#define htobe64(x) (x)
#define htole64(x) _byteswap_uint64(x)
#define be64toh(x) (x)
#define le64toh(x) _byteswap_uint64(x)

#else

#error byte order not supported

#endif /* ROMANO_BYTE_ORDER == ROMANO_BYTE_ORDER_LITTLE_ENDIAN */

#else

#error endian.h: platform not supported

#endif /* defined(ROMANO_LINUX) */

#endif /* !defined(__LIBROMANO_ENDIAN) */
