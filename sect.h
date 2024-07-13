//
// RMAC - Renamed Macro Assembler for all Atari computers
// SECT.H - Code Generation, Fixups and Section Management
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilized with the kind permission of Landon Dyer
//

#ifndef __SECT_H__
#define __SECT_H__

#include "rmac.h"
#include "riscasm.h"

// Macros to deposit code in the current section (in Big Endian)
#define D_byte(b)	{chcheck(1);*chptr++=(uint8_t)(b); sloc++; ch_size++; \
	if(orgactive) orgaddr++;}
#define D_word(w)	{chcheck(2);*chptr++=(uint8_t)((w)>>8); \
	*chptr++=(uint8_t)(w); \
	sloc += 2; ch_size += 2; if(orgactive) orgaddr += 2;}
#define D_long(lw)	{chcheck(4);*chptr++=(uint8_t)((lw)>>24); \
	*chptr++=(uint8_t)((lw)>>16);\
	*chptr++=(uint8_t)((lw)>>8); \
	*chptr++=(uint8_t)(lw); \
	sloc += 4; ch_size += 4; if(orgactive) orgaddr += 4;}
#define D_quad(qw)	{chcheck(8);*chptr++=(uint8_t)((qw)>>56); \
	*chptr++=(uint8_t)((qw)>>48);\
	*chptr++=(uint8_t)((qw)>>40);\
	*chptr++=(uint8_t)((qw)>>32);\
	*chptr++=(uint8_t)((qw)>>24);\
	*chptr++=(uint8_t)((qw)>>16);\
	*chptr++=(uint8_t)((qw)>>8); \
	*chptr++=(uint8_t)(qw); \
	sloc += 8; ch_size += 8; if(orgactive) orgaddr += 8;}

// D_rword deposits a "6502" format (low, high) word (01).
#define D_rword(w)	{chcheck(2);*chptr++=(uint8_t)(w); \
	*chptr++=(uint8_t)((w)>>8); \
	sloc+=2; ch_size+=2;if(orgactive) orgaddr += 2;}

// Macro for the 56001. Word size on this device is 24 bits wide. I hope that
// orgaddr += 1 means that the addresses in the device reflect this. [A: Yes.]
#define D_dsp(w)	{chcheck(3);*chptr++=(uint8_t)(w>>16); \
	*chptr++=(uint8_t)(w>>8); *chptr++=(uint8_t)w; \
	sloc+=1; ch_size += 3; if(orgactive) orgaddr += 1; \
	dsp_written_data_in_current_org=1;}

// This macro expects to get an array of uint8_ts with the hi bits in a[0] and
// the low bits in a[11] (Big Endian).
#define D_extend(a)	{chcheck(12); memcpy(chptr, a, 12); chptr+=12; sloc+=12, \
	ch_size+=12; if (orgactive) orgaddr+=12;}

// Fill n bytes with zeroes
#define D_ZEROFILL(n)	{chcheck(n); memset(chptr, 0, n); chptr+=n; sloc+=n; \
	ch_size+=n; if (orgactive) orgaddr+=n;}

//OK, this is bad, mmkay? The constants defined in rmac.h are used as indices into an array which means that this was never meant to be defined this way--at least if it was, it was a compromise that has come home to bite us all in the ass.  !!! FIX !!!
//#define NSECTS       16			// Max. number of sections
#define NSECTS       256			// Max. number of sections

// Tunable (storage) definitions
#define CH_THRESHOLD    32		// Minimum amount of space in code chunk
#define CH_CODE_SIZE    4096	// Code chunk normal allocation (4K)

// Section attributes (.scattr)
#define SUSED        0x8000		// Section is used (really, valid)
#define SBSS         0x4000		// Section can contain no data
#define SABS         0x2000		// Section is absolute
#define SPIC         0x1000		// Section is position-independent code

// FIXUP attributes
#define FUMASK       0x000F		// Mask for fixup cases:
#define FU_QUICK     0x0000		// Fixup 3-bit quick instruction field
#define FU_BYTE      0x0001		// Fixup byte
#define FU_WORD      0x0002		// Fixup word
#define FU_WBYTE     0x0003		// Fixup byte (at loc+1)
#define FU_LONG      0x0004		// Fixup long
#define FU_BBRA      0x0005		// Fixup byte branch
#define FU_6BRA      0x0007		// Fixup 6502-format branch offset
#define FU_BYTEH     0x0008		// Fixup 6502 high byte of immediate word
#define FU_BYTEL     0x0009		// Fixup 6502 low byte of immediate word
#define FU_QUAD      0x000A		// Fixup quad-word (8 bytes, typically OP)
#define FU_56001     0x000B		// Fixup code for all 56001 modes

#define FU_SEXT      0x0010		// Ok to sign extend
#define FU_PCREL     0x0020		// Subtract PC first
#define FU_PCRELX    0x1000000	// 030 variant
#define FU_EXPR      0x0040		// Expression (not symbol) follows

#define FU_GLOBAL    0x0080		// Mark global symbol

#define FUMASKRISC   0x0F00		// Mask for RISC fixup cases
#define FU_MOVEI     0x0100
#define FU_JR        0x0200
#define FU_REGONE    0x0400
#define FU_NUM15     0x0500
#define FU_NUM31     0x0600
#define FU_NUM32     0x0700
#define FU_REGTWO    0x0800

#define FU_SUB32     0x1000
#define FU_ISBRA     0x2000		// Word forward fixup is a BRA or DBRA
#define FU_LBRA      0x4000		// Long branch, for short branch detect
#define FU_DONE      0x8000		// Fixup has been done

// FPU fixups
#define FU_FLOATSING 0x000D		// Fixup 32-bit float
#define FU_FLOATDOUB 0x000E		// Fixup 64-bit float
#define FU_FLOATEXT  0x000F		// Fixup 96-bit float

// OP fixups
#define FU_OBJLINK   0x10000	// Fixup OL link addr (bits 24-42, drop last 3)
#define FU_OBJDATA   0x20000	// Fixup OL data addr (bits 43-63, drop last 3)

// DSP56001 fixups
#define FUMASKDSP     0xF00000	// Mask for DSP56001 fuckups^Wfixups
#define FU_DSPIMM5    0x100000	// Fixup  5-bit immediate
#define FU_DSPADR12   0x200000	// Fixup 12-bit address
#define FU_DSPADR24   0x300000	// Fixup 24-bit address
#define FU_DSPADR16   0x400000	// Fixup 16-bit address
#define FU_DSPIMM12   0x500000	// Fixup 12-bit immediate
#define FU_DSPIMM24   0x600000	// Fixup 24-bit immediate
#define FU_DSPIMM8    0x700000	// Fixup  8-bit immediate
#define FU_DSPADR06   0x800000	// Fixup  6-bit address
#define FU_DSPPP06    0x900000	// Fixup  6 bit pp address
#define FU_DSPIMMFL8  0xA00000	// Fixup  8-bit immediate float
#define FU_DSPIMMFL16 0xB00000	// Fixup 16-bit immediate float
#define FU_DSPIMMFL24 0xC00000	// Fixup 24-bit immediate float


// Chunks are used to hold generated code and fixup records
#define CHUNK  struct _chunk
CHUNK {
	CHUNK *   chnext;	// Next, previous chunks in section
	CHUNK *   chprev;
	uint32_t  chloc;	// Base addr of this chunk
	uint32_t  challoc;	// # bytes allocated for chunk
	uint32_t  ch_size;	// # bytes chunk actually uses
	uint8_t * chptr;	// Data for this chunk
};

// Fixup records can also hold an expression (if any)
#define FIXUP struct _fixup
FIXUP {
	FIXUP *  next;		// Pointer to next FIXUP
	uint32_t attr;		// Fixup type
	uint32_t loc;		// Location in section
	uint16_t fileno;	// ID of current file
	uint32_t lineno;	// Current line
	TOKEN *  expr;		// Pointer to stored expression (if any)
	SYM *    symbol;	// Pointer to symbol (if any)
	uint32_t orgaddr;	// Fixup origin address (used for FU_JR)
};

// Section descriptor
#define SECT   struct _sect
SECT {
	uint16_t scattr;	// Section attributes
	uint32_t sloc;		// Current loc-in / size-of section
	uint32_t relocs;	// # of relocations for this section
	uint32_t orgaddr;	// Current org'd address ***NEW***
	CHUNK *  sfcode;	// First chunk in section
	CHUNK *  scode;		// Last chunk in section
	FIXUP *  sffix;		// First fixup
	FIXUP *  sfix;		// Last fixup ***NEW***
};

// 680x0 defines
#define CPU_68000 1
#define CPU_68020 2
#define CPU_68030 4
#define CPU_68040 8
#define CPU_68060 16
#define FPU_NONE  0
#define FPU_68881 1
#define FPU_68882 2
#define FPU_68040 4
#define FPU_68060 8

// Helper macros to test for active CPU
#define CHECK00 if (activecpu == CPU_68000) return error(unsupport)
#define CHECK20 if (activecpu == CPU_68020) return error(unsupport)
#define CHECK30 if (activecpu == CPU_68030) return error(unsupport)
#define CHECK40 if (activecpu == CPU_68040) return error(unsupport)
#define CHECK60 if (activecpu == CPU_68060) return error(unsupport)
#define CHECKNO00 if (activecpu != CPU_68000) return error(unsupport)
#define CHECKNO20 if (activecpu != CPU_68020) return error(unsupport)
#define CHECKNO30 if (activecpu != CPU_68030) return error(unsupport)
#define CHECKNO40 if (activecpu != CPU_68040) return error(unsupport)
#define CHECKNO60 if (activecpu != CPU_68060) return error(unsupport)
#define CHECKNOFPU if (!activefpu) return error(unsupport)

// Globals, external etc
extern uint32_t sloc;
extern uint16_t scattr;
extern uint8_t * chptr;
extern uint8_t * chptr_opcode;
extern uint32_t ch_size;
extern int cursect;
extern SECT sect[];
extern uint32_t challoc;
extern CHUNK * scode;
extern int fixups_active;

// Prototypes
void InitSection(void);
void SwitchSection(int);
void SaveSection(void);
int fixtest(int, uint32_t);
void chcheck(uint32_t);
int AddFixup(uint32_t, uint32_t, TOKEN *);
int ResolveAllFixups(void);

#endif // __SECT_H__

