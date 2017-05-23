#ifndef __BYTECONVERSION_H_ASDF
#define __BYTECONVERSION_H_ASDF
#if defined(__linux__)
# include <byteswap.h>
# include <endian.h>

# ifndef htobe64
# if __BYTE_ORDER == __LITTLE_ENDIAN
# define htobe64(x) bswap_64(x)
# else
# define htobe64(x) (x)
# endif
# endif

# ifndef be64toh
# if __BYTE_ORDER == __LITTLE_ENDIAN
# define be64toh(x) bswap_64(x)
# else
# define be64toh(x) (x)
# endif
# endif
#elif defined(__APPLE__)
# include <libkern/OSByteOrder.h>
# define htobe64(u) OSSwapHostToBigInt64(u)
# define be64toh(u) OSSwapBigToHostInt64(u)
#endif

#ifndef be16toh
#define be16toh(x) ntohs(x)
#endif
#ifndef htobe16
#define htobe16(x) htons(x)
#endif

#ifndef be32toh
#define be32toh(x) ntohl(x)
#endif

#ifndef htobe32
#define htobe32(x) htonl(x)
#endif

#ifndef be128toh
static inline void be128toh(uint32_t* x) {
	x[0] = be32toh(x[0]); 
	x[1] = be32toh(x[1]); 
	x[2] = be32toh(x[2]); 
	x[3] = be32toh(x[3]); 
}
#endif
#ifndef htobe128
static inline void htobe128(uint32_t *x) {
	x[0] = htobe32(x[0]); 
	x[1] = htobe32(x[1]); 
	x[2] = htobe32(x[2]); 
	x[3] = htobe32(x[3]); 
}
#endif

#endif /* BYTECONVERSION_H */
