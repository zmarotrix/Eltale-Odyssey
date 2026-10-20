#ifndef _ULTRATYPES_H_
#define _ULTRATYPES_H_

// Minimal N64 integer/float typedefs so mods don't need the full libultra headers.

typedef signed char            s8;
typedef unsigned char          u8;
typedef signed short int       s16;
typedef unsigned short int     u16;
typedef signed int             s32;
typedef unsigned int           u32;
typedef signed long long int   s64;
typedef unsigned long long int u64;

typedef float  f32;
typedef double f64;

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif
