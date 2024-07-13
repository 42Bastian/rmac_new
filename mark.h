//
// RMAC - Renamed Macro Assembler for all Atari computers
// MARK.H - A record of things that are defined relative to any of the sections
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __MARK_H__
#define __MARK_H__

#include "rmac.h"

// A mark is of the form:
// .W    <to+flags>	section mark is relative to, and flags in upper byte
// .L    <loc>		location of mark in "from" section
// .W    [from]		new from section
// .L[L] [symbol]	symbol involved in external reference (LL for 64-bit pointers)
#define MCHUNK struct _mchunk
MCHUNK {
   MCHUNK * mcnext;				// Next mark chunk
   PTR mcptr;					// Vector of marks
   uint16_t mcalloc;			// # marks allocted to mark block
   uint16_t mcused;				// # marks used in block
};

#define MWORD        0x0000		// Marked word
#define MLONG        0x0100		// Marked long
#define MQUAD        0x0400		// Marked quad
//This will have to be defined eventually. Might have to overhaul the mark
//system as 8-bits doesn't seem to be enough, at least for a bitfield (which it
//might not have to be, in which case it would be big enough...)
//#define MQUAD        0x		// Marked quad word (TODO: merge with MDOUBLE?)
#define MMOVEI       0x0200		// Mark RISC MOVEI instruction
//#define MDOUBLE      0x0400		// Marked double float
//#define MEXTEND      0x0800		// Marked extended float
//#define MSINGLE      0x0880		// Marked single float (TODO: merge with MLONG?)
#define MGLOBAL      0x0800		// Mark contains global
#define MPCREL       0x1000		// Mark is PC-relative
#define MCHEND       0x2000		// Indicates end of mark chunk
#define MSYMBOL      0x4000		// Mark includes symbol pointer
#define MCHFROM      0x8000		// Mark includes change-to-from

// Exported variables
extern MCHUNK * firstmch;

// Exported functions
void InitMark(void);
void StopMark(void);
uint32_t MarkRelocatable(uint16_t, uint32_t, uint16_t, uint16_t, SYM *);
uint32_t AllocateMark(void);
uint32_t MarkImage(register uint8_t * mp, uint32_t siz, uint32_t tsize, int okflag);
uint32_t MarkBSDImage(uint8_t *, uint32_t, uint32_t, int);
uint32_t CreateELFRelocationRecord(uint8_t *, uint8_t *, uint16_t section);
uint32_t MarkABSImage(uint8_t * mp, uint32_t siz, uint32_t tsize, int reqseg);

#endif // __MARK_H__

