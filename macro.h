//
// RMAC - Renamed Macro Assembler for all Atari computers
// MACRO.H - Macro Definition and Invocation
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __MACRO_H__
#define __MACRO_H__

#include "rmac.h"

// Exported variables
extern LONG curuniq;
extern TOKEN * argPtrs[];
extern LONG reptuniq;
extern int rptlevel;

// Exported functions
void InitMacro(void);
int ExitMacro(void);
int DefineMacro(void);
int HandleRept(void);
int InvokeMacro(SYM *, WORD);

#endif // __MACRO_H__
