//
// RMAC - Renamed Macro Assembler for all Atari computers
// 6502.H - 6502 assembler
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __6502_H__
#define __6502_H__

// Exported variables
extern const char in_6502mode[];
extern uint16_t * currentorg;	// Current org range
extern char strtoa8[];

// Exported functions
extern void Init6502();
extern int d_6502();
extern void m6502cg(int op);
extern void m6502obj(int ofd);
extern void m6502raw(int ofd);
extern void m6502c64(int ofd);

#endif // __6502_H__

