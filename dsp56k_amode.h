//
// RMAC - Renamed Macro Assembler for the Atari Jaguar Console System
// DSP56K_AMODE.H - Addressing Modes for Motorola DSP56001
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __DSP56K_AMODE_H__
#define __DSP56K_AMODE_H__

#include "rmac.h"
#include "amode.h"

// Addressing-mode masks

#define M_ACC56      0x00000001L	// Accumulators A=A2:A1:A0 and B=B2:B1:B0
#define M_ACC48      0x00000002L	// Accumulators AB=A1:B1, BA=B1:A1, A10=A1:A0, B10=B1:B0
#define M_ACC24      0x00000004L	// Accumulators A0, A1, B0 and B1
#define M_ACC8       0x00000008L	// Accumulators A2 and B2
#define M_INP48      0x00000010L	// Input registers X=X1:X0 and Y=Y1:Y0
#define M_ALU24      0x00000020L	// Data ALU input registers X1, X0, Y1, Y0
#define M_DSPIM      0x00000040L	// #data
#define M_DSPIM12    0x00000080L	// #data
//#define M_DSPIM24    0x0000010	// #data
#define M_DSPPCU     0x00000200L	// Program control unit registers PC, MR, CCR, SR, OMR, LA, LC, SP, SS, SSH, SSL
#define M_DSPEA      0x00000400L	// Effective addressing modes (Rn)-Nn, (Rn)+Nn, (Rn)-, (Rn)+, (Rn), (Rn+Nn), -(Rn), <absolute address>
#define M_DSPAA      0x00000800L	// 6-bit Absolute Short Address
#define M_DSPPP      0x00001000L	// 6-bit I/O Short Address
#define M_DSPM       0x00002000L	// Modifier registers M0-M7
#define M_DSPR       0x00004000L	// Address registers R0-R7
#define M_DSPN       0x00008000L	// Address offset registers N0-N7
#define M_DSPABS12   0x00010000L	// xxx.12bit
#define M_DSPABS24   0x00020000L	// xxx.24bit
#define M_DSPABS06   0x00040000L	// xxx.6bit
#define M_DSPABS16   0x00080000L	// xxx.16bit
#define M_DSPIM8     0x00100000L	// #data

#define M_ALL48 (M_ACC56|M_INP48|M_ALU24)   // Input registers X=X1:X0, Y=Y1:Y0, A=A2:A1:A0, B=B2:B1:B0, X0, X1, Y0, Y1

#define C_DD    (M_ALU24)                   // 4 registers in data ALU: x0, x1, y0, y1
#define C_DDD   (M_ACC56|M_ACC24|M_ACC8)    // 8 accumulators in data ALU: a0, b0, a2, b2, a1, b1, a, b
#define C_LLL   (M_ACC56|M_ACC48|M_INP48)   // 8 extended-precision registers in data ALU: a10, b10, x, y, a, b, ab, ba
#define C_FFF   (M_DSPM)                    // 8 address modifier registers in address ALU:  m0-m7
#define C_NNN   (M_DSPN)                    // 8 address offset registers in address ALU: n0-n7
#define C_TTT   (M_DSPR)                    // 8 address registers in address: r0-r7
#define C_GGG   (M_DSPPCU)                  // 8 program controller registers: sr, omr, sp, ssh, la, lc
#define C_A18   (M_ALU24|C_DDD|C_LLL|C_FFF|C_NNN|C_TTT|C_GGG)   // All of the above registers

#define C_DSPABS24	(M_DSPABS06|M_DSPABS12|M_DSPABS16|M_DSPABS24)	// Everything up to 24-bit addresses
#define C_DSPABSEA	(C_DSPABS24|M_DSPEA)							// All absolute addresses and all other ea addressing modes
#define C_DSPABS16  (M_DSPABS06|M_DSPABS12|M_DSPABS16)				// Everything up to 16-bit addresses
#define C_LUADST    (M_DSPR|M_DSPN)									// Mask for LUA instruction destination
#define C_MOVEC     (M_DSPM|M_DSPPCU)								// M0-M7 and SR, OMR, LA, LC, SP, SS, SSH, SSL
#define C_DSPIM		(M_DSPIM8 | M_DSPIM | M_DSPIM12)				// All the immmediate sizes we want to alias

// Xn Input Register X1 or X0 (24 Bits)
// Yn Input Register Y1 or Y0 (24 Bits)
// An Accumulator Registers A2, A1, A0 (A2 — 8 Bits, A1 and A0 — 24 Bits)
// Bn Accumulator Registers B2, B1, B0 (B2 — 8 Bits, B1 and B0 — 24 Bits)
// X Input Register X = X1: X0 (48 Bits)
// Y Input Register Y = Y1: Y0 (48 Bits)
// A Accumulator A = A2: A1: A0 (56 Bits)*
// B Accumulator B = B2: B1: B0 (56 BIts)*
// AB Accumulators A and B = A1: B1 (48 Bits)*
// BA Accumulators B and A = B1: A1 (48 Bits)*
// A10 Accumulator A = A1: A0 (48 Bits)
// B10 Accumulator B= B1:B0 (48 bits)

// Attribute masks
#define PARMOVE      0x00000001L
#define NOPARMO      0x00000000L
#define PARNOWRITE	0x00000002L	// Instruction does not write to a destination (CMP and TST mostly)
#define PARTEST		(PARMOVE|PARNOWRITE)

// DSP EA modes

#define DSP_EA_POSTDEC  0b00000000
#define DSP_EA_POSTINC  0b00001000
#define DSP_EA_POSTDEC1 0b00010000
#define DSP_EA_POSTINC1 0b00011000
#define DSP_EA_NOUPD    0b00100000
#define DSP_EA_INDEX    0b00101000
#define DSP_EA_PREDEC1  0b00111000
#define DSP_EA_ABS      0b00110000
#define DSP_EA_IMM      0b00110100


// Mnemonic table structure
#define MNTABDSP  struct _mntabdsp
MNTABDSP {
	LONG mn0, mn1;				// Addressing modes
	WORD mnattr;				// Attributes (PARMOVE, ...)
	LONG mninst;				// Instruction mask
	WORD mncont;				// Continuation (or -1)
	int (* mnfunc)(LONG);		// Mnemonic builder
};

// Addressing mode variables, output of dsp_amode()
extern int dsp_am0;					// Addressing mode
extern int dsp_a0reg;					// Register
extern int dsp_am1;					// Addressing mode
extern int dsp_a1reg;					// Register
extern int dsp_am2;					// Addressing mode
extern int dsp_a2reg;					// Register
extern int dsp_am3;					// Addressing mode
extern int dsp_a3reg;					// Register

extern TOKEN dsp_a0expr[EXPRSIZE];		// Expression
extern uint64_t dsp_a0exval;			// Expression's value
extern WORD dsp_a0exattr;				// Expression's attribute
extern SYM * dsp_a0esym;				// External symbol involved in expr
extern LONG dsp_a0memspace;			// Addressing mode's memory space (P, X, Y)
extern LONG dsp_a0perspace;			// Peripheral space (X, Y - used in movep)
extern TOKEN dsp_a1expr[EXPRSIZE];		// Expression
extern uint64_t dsp_a1exval;			// Expression's value
extern WORD dsp_a1exattr;				// Expression's attribute
extern SYM * dsp_a1esym;				// External symbol involved in expr
extern LONG dsp_a1memspace;			// Addressing mode's memory space (P, X, Y)
extern LONG dsp_a1perspace;			// Peripheral space (X, Y - used in movep)
extern TOKEN dsp_a2expr[EXPRSIZE];		// Expression
extern uint64_t dsp_a2exval;			// Expression's value
extern WORD dsp_a2exattr;				// Expression's attribute
extern SYM * dsp_a2esym;				// External symbol involved in expr
extern TOKEN dsp_a3expr[EXPRSIZE];		// Expression
extern uint64_t dsp_a3exval;			// Expression's value
extern WORD dsp_a3exattr;				// Expression's attribute
extern SYM * dsp_a3esym;				// External symbol involved in expr
extern int dsp_k;						// Multiplications sign
extern TOKEN dspImmedEXPR[EXPRSIZE];	// Expression
extern uint64_t dspImmedEXVAL;			// Expression's value
extern WORD  dspImmedEXATTR;			// Expression's attribute
extern SYM * dspImmedESYM;				// External symbol involved in expr
extern int deposit_extra_ea;			// Optional effective address extension


// Extra ea deposit modes
enum
{
	DEPOSIT_EXTRA_WORD  = 1,
	DEPOSIT_EXTRA_FIXUP = 2,
};


// Prototypes
int dsp_amode(int maxea);
LONG parmoves(WORD dest);
int dsp_tcc4(LONG inst);

#endif // __DSP56K_AMODE_H__

