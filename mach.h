//
// RMAC - Renamed Macro Assembler for all Atari computers
// MACH.H - Code Generation
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __MACH_H__
#define __MACH_H__

#include "rmac.h"

// Mnemonic table structure
#define MNTAB  struct _mntab
MNTAB {
	WORD mnattr;					// Attributes (CGSPECIAL, SIZN, ...)
	LONG mn0, mn1;					// Addressing modes
	WORD mninst;					// Instruction mask
	WORD mncont;					// Continuation (or -1)
	int (* mnfunc)(WORD, WORD);		// Mnemonic builder
};

// Exported variables
extern char seg_error[];
extern char undef_error[];
extern char rel_error[];
extern char range_error[];
extern char abs_error[];
extern char unsupport[];
extern MNTAB machtab[];
extern int movep;

#endif // __MACH_H__

