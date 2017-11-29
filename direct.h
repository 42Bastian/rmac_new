//
// RMAC - Reboot's Macro Assembler for all Atari computers
// DIRECT.H - Directive Handling
// Copyright (C) 199x Landon Dyer, 2011-2017 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __DIRECT_H__
#define __DIRECT_H__

#include "rmac.h"
#include "token.h"

// Exported variables
extern TOKEN exprbuf[];
extern SYM * symbolPtr[];
extern int (* dirtab[])();
extern int largestAlign[];

// Exported functions
void auto_even(void);
int dep_block(uint32_t, WORD, uint32_t, WORD, TOKEN *);
int eject(void);
int abs_expr(uint64_t *);
int symlist(int(*)());

int d_even(void);
int d_long(void);
int d_phrase(void);
int d_dphrase(void);
int d_qphrase(void);

int d_if(void);
int d_else(void);
int d_endif(void);

#endif // __DIRECT_H__

