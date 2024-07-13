//
// RMAC - Renamed Macro Assembler for all Atari computers
// PROCLN.H - Line Processing
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __PROCLN_H__
#define __PROCLN_H__

#include "rmac.h"
#include "token.h"

// Imported functions
int m_ptestr(WORD inste, WORD siz);
int m_ptestw(WORD inste, WORD siz);

// Exported variables
extern const char comma_error[];
extern const char locgl_error[];
extern const char syntax_error[];
extern const char extra_stuff[];
extern int just_bss;
extern uint32_t pcloc;
extern SYM * lab_sym;
extern LONG amsktab[];
extern IFENT * ifent;
extern IFENT * f_ifent;
extern int disabled;

// Exported functions
void InitLineProcessor(void);
void Assemble(void);

#endif // __PROCLN_H__

