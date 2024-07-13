//
// RMAC - Renamed Macro Assembler for all Atari computers
// AMODE.H - Addressing Modes
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __AMODE_H__
#define __AMODE_H__

#include "rmac.h"

// 68000 and 68020 addressing modes
#define DREG         000			// Dn
#define AREG         010			// An
#define AIND         020			// (An)
#define DINDW        0112			// (Dn.w)
#define DINDL        0113			// (Dn.l)
#define APOSTINC     030			// (An)+
#define APREDEC      040			// -(An)
#define ADISP        050			// (d16,An) d16(An)
#define AINDEXED     060			// (d8,An,Xn) d8(An,Xn)
#define ABSW         070			// xxx.W
#define ABSL         071			// xxx or xxx.L
#define PCDISP       072			// (d16,PC) d16(PC)
#define PCINDEXED    073			// (d16,PC,Xn) d16(PC,Xn)
#define IMMED        074			// #data
#define ABASE        0100			// (bd,An,Xn)
#define MEMPOST      0101			// ([bd,An],Xn,od)
#define MEMPRE       0102			// ([bc,An,Xn],od)
#define PCBASE       0103			// (bd,PC,Xn)
#define PCMPOST      0104			// ([bd,PC],Xn,od)
#define PCMPRE       0105			// ([bc,PC,Xn],od)
#define AM_USP       0106
#define AM_SR        0107
#define AM_CCR       0110
#define AM_NONE      0111			// Nothing
#define CACHES       0120			// Instruction/Data/Both Caches (IC/DC/BC)
#define CREG         0121			// Control registers (see CREGlut in mach.c)
#define FREG         0122			// FPU registers (fp0-fp7)
#define FPSCR        0123			// FPU system control registers (fpiar, fpsr, fpcr)

// Addressing-mode masks
#define M_DREG       0x00000001L	// Dn
#define M_AREG       0x00000002L	// An
#define M_AIND       0x00000004L	// (An)
#define M_APOSTINC   0x00000008L	// (An)+
#define M_APREDEC    0x00000010L	// -(An)
#define M_ADISP      0x00000020L	// (d16,An) d16(An)
#define M_AINDEXED   0x00000040L	// (d8,An,Xn) d8(An,Xn)
#define M_ABSW       0x00000080L	// xxx.W
#define M_ABSL       0x00000100L	// xxx or xxx.L
#define M_PCDISP     0x00000200L	// (d16,PC) d16(PC)
#define M_PCINDEXED  0x00000400L	// (d16,PC,Xn) d16(PC,Xn)
#define M_IMMED      0x00000800L	// #data
#define M_ABASE      0x00001000L	// (bd,An,Xn)
#define M_MEMPOST    0x00002000L	// ([bd,An],Xn,od)
#define M_MEMPRE     0x00004000L	// ([bd,An,Xn],od)
#define M_PCBASE     0x00008000L	// (bd,PC,Xn)
#define M_PCMPOST    0x00010000L	// ([bd,PC],Xn,od)
#define M_PCMPRE     0x00020000L	// ([bc,PC,Xn],od)
#define M_AM_USP     0x00040000L	// USP
#define M_AM_SR      0x00080000L	// SR
#define M_AM_CCR     0x00100000L	// CCR
#define M_AM_NONE    0x00200000L	// (nothing)
#define M_BITFLD     0x00400000L	// 68020 bitfield
#define M_CREG       0x00800000L	// Control registers
#define M_FREG       0x01000000L	// FPn
#define M_FPSCR      0x02000000L	// fpiar, fpsr, fpcr
#define M_CACHE40    0x04000000L	// 68040 cache registers (IC40,DC40,BC40)

// Addr mode categories
#define C_ALL        0x00000FFFL
#define C_DATA       0x00000FFDL
#define C_MEM        0x00000FFCL
#define C_CTRL       0x000007E4L
#define C_ALT        0x000001FFL
#define C_ALL030     0x0003FFFFL
#define C_ALT030     0x000071FDL
#define C_FPU030     0x0003FFECL    /* (An), #<data>, (An)+, (d16,An), (d16,PC), (d8, An, Xn), (d8, PC, Xn), (bd, An, Xn), An(bd, PC, Xn), ([bd, An, Xn], od), An([bd, PC, Xn], od), ([bd, An], Xn, od), An([bd, PC], Xn, od) */
#define C_CTRL030    0x0003F7E4L
#define C_DATA030    0x0003FFFDL
#define C_MOVES      (M_AIND | M_APOSTINC | M_APREDEC | M_ADISP | M_AINDEXED | M_ABSW | M_ABSL | M_ABASE | M_MEMPRE | M_MEMPOST)
#define C_BF1        (M_DREG | M_AIND | M_AINDEXED | M_ADISP | M_ABSW | M_ABSL | M_ABASE | M_MEMPOST | M_MEMPRE)
#define C_BF2        (C_BF1 | M_PCDISP | M_PCINDEXED | M_PCBASE | M_PCMPOST | M_PCMPRE)
#define C_PMOVE      (M_AIND | M_ADISP | M_AINDEXED  | M_ABSW | M_ABSL | M_ABASE | M_MEMPRE | M_MEMPOST)

#define C_ALTDATA    (C_DATA & C_ALT)
#define C_ALTMEM     (C_MEM & C_ALT)
#define C_ALTCTRL    (C_CTRL & C_ALT)
#define C_LABEL      (M_ABSW | M_ABSL)
#define C_NONE       M_AM_NONE

#define C_CREG       (M_AM_USP | M_CREG)

// Scales
#define TIMES1       00000			// (empty or *1)
#define TIMES2       01000			// *2
#define TIMES4       02000			// *4
#define TIMES8       03000			// *8

#define M_FC		(M_IMMED | M_DREG | M_CREG)
#define M_MRN		(M_DREG | M_AREG | M_CREG)

// EA extension word
#define EXT_D		 0x0000			// Dn
#define EXT_A		 0x8000			// An
#define EXT_W		 0x0000			// Index Size Sign-Extended Word
#define EXT_L		 0x0800			// Index Size Long Word
#define EXT_TIMES1   0x0000			// Scale factor 1
#define EXT_TIMES2   0x0200			// Scale factor 2
#define EXT_TIMES4   0x0400			// Scale factor 4
#define EXT_TIMES8   0x0600			// Scale factor 8
#define EXT_FULLWORD 0x0100			// Use full extension word format
#define EXT_BS		 0x0080			// Base Register Suppressed
#define EXT_IS		 0x0040			// Index Operand Suppressed
#define EXT_BDSIZE0  0x0010			// Base Displacement Size Null (Suppressed)
#define EXT_BDSIZEW  0x0020			// Base Displacement Size Word
#define EXT_BDSIZEL  0x0030			// Base Displacement Size Long

// Indirect and Indexing Operands
#define EXT_IISPRE0  0x0000	// No Memory Indirect Action
#define EXT_IISPREN  0x0001	// Indirect Preindexed with Null Outer Displacement
#define EXT_IISPREW  0x0002	// Indirect Preindexed with Word Outer Displacement
#define EXT_IISPREL  0x0003	// Indirect Preindexed with Long Outer Displacement
#define EXT_IISPOSN  0x0005	// Indirect Postindexed with Null Outer Displacement
#define EXT_IISPOSW  0x0006	// Indirect Postindexed with Word Outer Displacement
#define EXT_IISPOSL  0x0007	// Indirect Postindexed with Long Outer Displacement
#define EXT_IISNOI0  0x0000	// No Memory Indirect Action
#define EXT_IISNOIN  0x0001	// Memory Indirect with Null Outer Displacement
#define EXT_IISNOIW  0x0002	// Memory Indirect with Word Outer Displacement
#define EXT_IISNOIL  0x0003	// Memory Indirect with Long Outer Displacement

#define EXPRSIZE     128	// Maximum #tokens in an expression

// Addressing mode variables, output of amode()
extern int nmodes;
extern int am0, am1;
extern int a0reg, a1reg, a2reg;
extern TOKEN a0expr[], a1expr[];
extern uint64_t a0exval, a1exval;
extern WORD a0exattr, a1exattr;
extern int a0ixreg, a1ixreg;
extern int a0ixsiz, a1ixsiz;
extern TOKEN a0oexpr[], a1oexpr[];
extern uint64_t a0oexval, a1oexval;
extern WORD a0oexattr, a1oexattr;
extern SYM * a0esym, * a1esym;
extern uint64_t a0bexval, a1bexval;
extern WORD a0bexattr, a1bexattr;
extern WORD a0bsize, a1bsize;
extern TOKEN a0bexpr[], a1bexpr[];
extern WORD a0extension, a1extension;
extern WORD mulmode;
extern int bfparam1;
extern int bfparam2;
extern int bfval1;
extern int bfval2;
extern uint64_t bf0exval;

// mnattr:
#define CGSPECIAL    0x8000			// Special (don't parse addr modes)

// Exported functions
int amode(int);
int reglist(WORD *);
int fpu_reglist_left(WORD *);
int fpu_reglist_right(WORD *);

#endif	// __AMODE_H__

