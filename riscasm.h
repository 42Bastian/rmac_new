//
// RMAC - Renamed Macro Assembler for all Atari computers
// RISCA.H - GPU/DSP Assembler
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __RISCASM_H__
#define __RISCASM_H__

#include "rmac.h"

// RISC Instruction Types
#define RI_NONE     0x0000		// No Operands - NOP
#define RI_ONE      0x0001		// One Operand - Rd - ABS/NEG/etc
#define RI_TWO      0x0002		// Two Operands - Rs,Rd - Most Instructions
#define RI_NUM_15   0x0003		// Numeric Operand - n,Rd - n=-16..+15 - CMPQ
#define RI_NUM_31   0x0004		// Numeric Operand - n,Rd - n=0..31 - BCLR/BSET/BTST/MOVEQ
#define RI_NUM_32   0x0005		// Numeric Operand - n,Rd - n=1..32 - ADDQ/SUBQ
#define RI_JR       0x0006		// Jump Relative - cc,n - n=-16..+15 words, reg2=cc
#define RI_JUMP     0x0007		// Jump Absolute - cc,(Rs) - reg2=cc
#define RI_MOVEI    0x0008		// Move Immediate - n,Rn - n in second word
#define RI_MOVE     0x0009		// MOVE Instruction - PC,Rn / Rn,Rn
#define RI_LOAD     0x000A		// LOAD Instruction - Various Forms
#define RI_LOADN    0x000B		// LOADB/LOADP/LOADW - (Rs),Rd
#define RI_STORE    0x000C		// STORE Instruction - Various Forms
#define RI_STOREN   0x000D		// STOREB/STOREP/STOREM - Rs,(Rd)

// Supplementry Instruction Flags
#define SUB32       0x2000		// (n = 32-n)
#define GPUONLY     0x4000		// Opcode is for the GPU Only
#define DSPONLY     0x8000		// Opcode is for the DSP Only

#define CHECK_COMMA  if (*tok++ != ',') { return error(comma_error); }

// Globals, externals etc
extern unsigned orgactive;
extern unsigned orgaddr;
extern unsigned orgwarning;
extern unsigned altbankok;
extern uint8_t riscImmTokenSeen;

// Prototypes
int GenerateRISCCode(int);

#endif // __RISCASM_H__
