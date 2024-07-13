//
// RMAC - Renamed Macro Assembler for all Atari computers
// RMAC.H - Main Application Code
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __RMAC_H__
#define __RMAC_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

//
// TARGET SPECIFIC BUILD SETTINGS
//
#if defined(WIN32) || defined(WIN64)
	#include <io.h>
	#include <fcntl.h>
	#include "dirent_lose.h"
	// Release platform - windows
	#define PLATFORM        "Win32"
	#define _OPEN_FLAGS     _O_TRUNC|_O_CREAT|_O_BINARY|_O_RDWR
	#define _OPEN_INC       _O_RDONLY|_O_BINARY
	#define _PERM_MODE      _S_IREAD|_S_IWRITE
    #define PATH_SEPS       ";"
    #define realpath(_fn, _abs) _fullpath((_abs), (_fn), _MAX_PATH)

	#ifdef _MSC_VER
		#if _MSC_VER > 1000
			#pragma warning(disable:4996)
		#endif

	// Makes warnings double clickable on visual studio (ggn)
	#define STRINGIZE_HELPER(x) #x
	#define STRINGIZE(x) STRINGIZE_HELPER(x)
	#define WARNING(desc) __pragma(message(__FILE__ "(" STRINGIZE(__LINE__) ") : Warning: " #desc))
	#define inline __inline
	// usage:
	// WARNING(FIXME: Code removed because...)

	#else
	// If we're not compiling for Visual Studio let's assume that we're using
	// some flavour of gcc instead. So let's use the gcc compliant macro
	// instead. If some weirdo uses something else (I dunno, Intel compiler or
	// something?) this is probably going to explode spectacularly. Let's wait
	// for the fireworks!
	#define DO_PRAGMA(x) _Pragma (#x)
	#define WARNING(desc) DO_PRAGMA(message (#desc))

	#endif

	// Ever since Visual Studio... 2017? 2019? the following constants come
	// defined in the platform SDK, which leads to endless warnings from the
	// compiler. So let's just put the pacifier on and undef them, sheesh! (No,
	// we won't rename the defines, we've been here since 1986, Visual Studio
	// wasn't even a glimpse in the milkman's eyes, if you catch my drift)
	#undef CONST
	#undef ERROR
	#undef TEXT

#else

	#include <dirent.h>

	#ifdef __GCCUNIX__

	#include <sys/fcntl.h>
	#include <unistd.h>
	// Release platform - Linux or mac OS-X
	#define PLATFORM        "Linux/OSX"
	#define _OPEN_FLAGS     O_TRUNC|O_CREAT|O_RDWR
	#define _OPEN_INC       O_RDONLY
	#define _PERM_MODE      S_IRUSR|S_IWUSR
    #define PATH_SEPS       ";:"

	#ifdef __MINGW32__
	#define off64_t long
	#define off_t long
	#undef _OPEN_FLAGS
	#undef _OPEN_INC
	#define _OPEN_FLAGS     _O_TRUNC|_O_CREAT|_O_BINARY|_O_RDWR
	#define _OPEN_INC       O_RDONLY|_O_BINARY
	#endif

	// WARNING WARNING WARNING
	#define DO_PRAGMA(x) _Pragma (#x)
	#define WARNING(desc) DO_PRAGMA(message (#desc))

	#else

	// Release platform - not specified
	#include <sys/fcntl.h>
	#define PLATFORM        "Unknown"
	#define _OPEN_FLAGS     O_TRUNC|O_CREAT|O_RDWR
	#define _OPEN_INC       O_RDONLY
	#define _PERM_MODE      S_IREAD|S_IWRITE
    #define PATH_SEPS       ":;"
	// Defined here, even though the platform may not support it...
	#define DO_PRAGMA(x) _Pragma (#x)
	#define WARNING(desc) DO_PRAGMA(message (#desc))
	#endif

#endif


//
// Endian related, for safe handling of endian-sensitive data
// USAGE: GETBExx() is *always* an rvalue, a = pointer to a uint8_t,
//        r = offset from 0. SETBExx(), v = value to write into 'a'
//
#define GETBE16(a, r) \
	(((uint16_t)(a)[(r + 0)] << 8) | ((uint16_t)(a)[(r + 1)]))

#define GETBE32(a, r) \
	(((uint32_t)(a)[(r + 0)] << 24) | ((uint32_t)(a)[(r + 1)] << 16) \
	| ((uint32_t)(a)[(r + 2)] << 8) | ((uint32_t)(a)[(r + 3)]))

#define GETBE64(a, r) \
	(((uint64_t)(a)[(r + 0)] << 56) | ((uint64_t)(a)[(r + 1)] << 48) \
	| ((uint64_t)(a)[(r + 2)] << 40) | ((uint64_t)(a)[(r + 3)] << 32) \
	| ((uint64_t)(a)[(r + 4)] << 24) | ((uint64_t)(a)[(r + 5)] << 16) \
	| ((uint64_t)(a)[(r + 6)] << 8) | ((uint64_t)(a)[(r + 7)]))

#define SETBE16(a, r, v) \
	{ (a)[(r + 0)] = (uint8_t)((v) >> 8); \
	(a)[(r + 1)] = (uint8_t)((v) & 0xFF); }

#define SETBE32(a, r, v) \
	{ (a)[(r + 0)] = (uint8_t)((v) >> 24); \
	(a)[(r + 1)] = (uint8_t)(((v) >> 16) & 0xFF); \
	(a)[(r + 2)] = (uint8_t)(((v) >> 8) & 0xFF); \
	(a)[(r + 3)] = (uint8_t)((v) & 0xFF); }

#define SETBE64(a, r, v) \
	{ (a)[(r + 0)] = (uint8_t)((v) >> 56); \
	(a)[(r + 1)] = (uint8_t)(((v) >> 48) & 0xFF); \
	(a)[(r + 2)] = (uint8_t)(((v) >> 40) & 0xFF); \
	(a)[(r + 3)] = (uint8_t)(((v) >> 32) & 0xFF); \
	(a)[(r + 4)] = (uint8_t)(((v) >> 24) & 0xFF); \
	(a)[(r + 5)] = (uint8_t)(((v) >> 16) & 0xFF); \
	(a)[(r + 6)] = (uint8_t)(((v) >> 8) & 0xFF); \
	(a)[(r + 7)] = (uint8_t)((v) & 0xFF); }

// In 6502 mode, turns out we need this:
#define SETLE16(a, r, v) \
	{ (a)[(r + 0)] = (uint8_t)((v) & 0xFF); \
	(a)[(r + 1)] = (uint8_t)((v) >> 8); }

// In DSP56001 mode, this is useful:
#define SETBE24(a, v) \
	{ (a)[0] = (uint8_t)(((v) >> 16) & 0xFF); \
	(a)[1] = (uint8_t)(((v) >> 8) & 0xFF); \
	(a)[2] = (uint8_t)((v) & 0xFF); }

// Byteswap crap
#define BYTESWAP16(x) ((((x) & 0x00FF) << 8) | (((x) & 0xFF00) >> 8))
#define BYTESWAP32(x) ((((x) & 0x000000FF) << 24) | (((x) & 0x0000FF00) << 8) | (((x) & 0x00FF0000) >> 8) | (((x) & 0xFF000000) >> 24))
#define BYTESWAP64(x) (BYTESWAP32(x >> 32) | (BYTESWAP32(x & 0xFFFFFFFF) << 32))
#define WORDSWAP32(x) ((((x) & 0x0000FFFF) << 16) | (((x) & 0xFFFF0000) >> 16))

//
// Non-target specific stuff
//
#include <inttypes.h>
#include "symbol.h"

#define BYTE         uint8_t
#define WORD         uint16_t
#define LONG         uint32_t
#define VOID         void

#define ERROR        (-1)		// Generic error return
#define EOS          '\0'		// End of string
#define SPACE        ' '		// ASCII space
#define SLASHCHAR    '/'
#define SLASHSTRING  "/"
#define FNSIZ        128		// Maximum size of a filename
#define OK           0			// OK return
#define DEBUG        if (debug)	// Debug conditional
#define MAXARGV      100		// Maximum number of commandline args
#define STDOUT       1			// Standard output
#define ERROUT       2			// Error output
#define CREATMASK    0

// Object code formats
enum
{
ALCYON,				// Alcyon/DRI C object format
BSD, 				// BSD object format
ELF, 				// ELF object format
LOD,				// DSP 56001 object format
P56,				// DSP 56001 object format
XEX, 				// COM/EXE/XEX/whatever a8 object format
RAW,				// Output at absolute address
C64PRG,				// C64 .PRG format
};

// Assembler token
#define TOKEN	uint32_t

// Pointer type that can point to (almost) anything
#define PTR union _ptr
PTR
{
	uint8_t *  cp;				// Char pointer
	uint16_t * wp;				// WORD pointer
	uint32_t * lp;				// LONG pointer
	uint32_t * u32;				// 32-bit pointer
	uint64_t * u64;				// 64-bit pointer
	uint32_t   lw;				// LONG (for some reason)
	SYM **     sy;				// SYM pointer
	TOKEN *    tk;				// TOKEN pointer
	double *   dp;				// Double pointer
	int64_t *  i64;				// 64-bit signed int pointer
};

// Symbol spaces
#define LABEL        0			// User-defined symbol
#define MACRO        1			// Macro definition
#define MACARG       2			// Macro argument
#define DBGSYM       3			// stabs debug symbol
#define SY_UNDEF     -1			// Undefined (lookup never matches it)

// Symbol and expression attributes
#define DEFINED      0x8000		// Symbol has been defined
#define GLOBAL       0x4000		// Symbol has been .GLOBL'd
#define COMMON       0x2000		// Symbol has been .COMM'd
#define REFERENCED   0x1000		// Symbol has been referenced
#define EQUATED      0x0800		// Symbol was equated
#define SDECLLIST    0x0400		// Symbol is on 'sdecl'-order list
#define FLOAT        0x0200		// Symbol is a floating point value
#define RISCREG      0x0100		// Symbol is a RISC register

// Expression spaces, ORed with symbol and expression attributes above
#define ABS          0x0000		// In absolute space
#define TEXT         0x0001		// Relative to text
#define DATA         0x0002		// Relative to data
#define BSS          0x0004		// Relative to BSS
//OK, this is bad, mmkay? These are treated as indices into an array which means that this was never meant to be defined this way--at least if it was, it was a compromise that has come home to bite us all in the ass.  !!! FIX !!!
#define M6502        0x0008		// 6502/microprocessor (absolute)
#define M56001P      0x0010		// DSP 56001 Program RAM
#define M56001X      0x0020		// DSP 56001 X RAM
#define M56001Y      0x0040		// DSP 56001 Y RAM
#define M56001L      0x0080		// DSP 56001 L RAM
#define TDB          (TEXT|DATA|BSS)	// Mask for TEXT+DATA+BSS
#define M56KPXYL     (M56001P|M56001X|M56001Y|M56001L)	// Mask for 56K stuff

// Sizes
#define SIZB         0x0001		// .b
#define SIZW         0x0002		// .w
#define SIZL         0x0004		// .l
#define SIZN         0x0008		// no .(size) specifier
#define SIZD         0x0010		// .d (FPU double precision real)
#define SIZS         0x0020		// .s (FPU single precision real)
#define SIZX         0x0040		// .x (FPU extended precision real)
#define SIZP         0x0080		// .p (FPU pakced decimal real)
#define SIZQ         0x0100		// .q (quad word)

// Extended attributes
#define EQUATEDREG   0x0008		// Equated register symbol
#define UNDEF_EQUR   0x0010
#define EQUATEDCC    0x0020
#define UNDEF_CC     0x0040

// Optimisation defines
enum
{
    // These will be set to on/off when .opt "+Oall"/"~Oall" is called
	OPT_ABS_SHORT     = 0,
	OPT_MOVEL_MOVEQ   = 1,
	OPT_BSR_BCC_S     = 2,
	OPT_OUTER_DISP    = 3,
	OPT_LEA_ADDQ      = 4,
	OPT_020_DISP      = 5,		// 020+ base and outer displacements (bd, od) absolute long to short
	OPT_NULL_BRA      = 6,
	OPT_CLR_DX        = 7,
	OPT_ADDA_ADDQ     = 8,
	OPT_ADDA_LEA      = 9,
	OPT_56K_SHORT	  = 10,
	OPT_56K_AUTO_LONG = 11,
	OPT_COUNT,                  // Dummy, used to count number of optimisation switches
    // These will be unaffected by "Oall"
	OPT_PC_RELATIVE   = 30,		// Enforce PC relative
    OPT_COUNT_ALL               // Dummy, used to count all switches
};

// Exported variables
extern int verb_flag;
extern int debug;
extern int rgpu, rdsp;
extern int robjproc;
extern int dsp56001;
extern int err_flag;
extern int err_fd;
extern char * firstfname;
extern int list_fd;
extern int list_pag;
extern int m6502;
extern int list_flag;
extern int glob_flag;
extern int lsym_flag;
extern int dsym_flag;
extern int optim_warn_flag;
extern int obj_format;
extern int legacy_flag;
extern int prg_flag;	// 1 = write ".PRG" relocatable executable
extern LONG PRGFLAGS;
extern int optim_flags[OPT_COUNT_ALL];
extern int activecpu;
extern int activefpu;
extern uint32_t org68k_address;
extern int org68k_active;
extern int *regbase;
extern int *regtab;
extern int *regcheck;
extern int *regaccept;
extern uint32_t used_architectures;

// Exported functions
void strtoupper(char * s);
char * fext(char *, char *, int);
int nthpath(char *, int, char *);
int ParseOptimization(char * optstring);

#endif // __RMAC_H__

