//
// RMAC - Renamed Macro Assembler for all Atari computers
// MACH.C - Code Generation
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "mach.h"
#include "amode.h"
#include "direct.h"
#include "eagen.h"
#include "error.h"
#include "expr.h"
#include "procln.h"
#include "riscasm.h"
#include "sect.h"
#include "token.h"

#define DEF_REG68
#include "68kregs.h"

// Exported variables
int movep = 0; // Global flag to indicate we're generating a movep instruction

// Function prototypes
int m_unimp(WORD, WORD), m_badmode(WORD, WORD);
int m_self(WORD, WORD);
int m_abcd(WORD, WORD);
int m_reg(WORD, WORD);
int m_imm(WORD, WORD);
int m_imm8(WORD, WORD);
int m_shi(WORD, WORD);
int m_shr(WORD, WORD);
int m_bitop(WORD, WORD);
int m_exg(WORD, WORD);
int m_ea(WORD, WORD);
int m_lea(WORD, WORD);
int m_br(WORD, WORD);
int m_dbra(WORD, WORD);
int m_link(WORD, WORD);
int m_adda(WORD, WORD);
int m_addq(WORD, WORD);
int m_move(WORD, WORD);
int m_moveq(WORD, WORD);
int m_usp(WORD, WORD);
int m_movep(WORD, WORD);
int m_trap(WORD, WORD);
int m_movem(WORD, WORD);
int m_clra(WORD, WORD);
int m_clrd(WORD, WORD);

int m_move30(WORD, WORD);			// 68020/30/40/60
int m_br30(WORD inst, WORD siz);
int m_ea030(WORD inst, WORD siz);
int m_bfop(WORD inst, WORD siz);
int m_callm(WORD inst, WORD siz);
int m_cas(WORD inst, WORD siz);
int m_cas2(WORD inst, WORD siz);
int m_chk2(WORD inst, WORD siz);
int m_cmp2(WORD inst, WORD siz);
int m_bkpt(WORD inst, WORD siz);
int m_cpbcc(WORD inst, WORD siz);
int m_cpdbr(WORD inst, WORD siz);
int m_muls(WORD inst, WORD siz);
int m_move16a(WORD inst, WORD siz);
int m_move16b(WORD inst, WORD siz);
int m_pack(WORD inst, WORD siz);
int m_rtm(WORD inst, WORD siz);
int m_rtd(WORD inst, WORD siz);
int m_trapcc(WORD inst, WORD siz);
int m_cinv(WORD inst, WORD siz);
int m_cprest(WORD inst, WORD siz);
int m_movec(WORD inst, WORD siz);
int m_moves(WORD inst, WORD siz);
int m_lpstop(WORD inst, WORD siz);
int m_plpa(WORD inst, WORD siz);

// PMMU
int m_pbcc(WORD inst, WORD siz);
int m_pflusha(WORD inst, WORD siz);
int m_pflush(WORD inst, WORD siz);
int m_pflushr(WORD inst, WORD siz);
int m_pflushan(WORD inst, WORD siz);
int m_pload(WORD inst, WORD siz, WORD extension);
int m_pmove(WORD inst, WORD siz);
int m_pmovefd(WORD inst, WORD siz);
int m_ptest(WORD inst, WORD siz, WORD extension);
int m_ptestr(WORD inste, WORD siz);
int m_ptestw(WORD inste, WORD siz);
int m_ptrapcc(WORD inst, WORD siz);
int m_ploadr(WORD inst, WORD siz);
int m_ploadw(WORD inst, WORD siz);

// FPU
int m_fabs(WORD inst, WORD siz);
int m_fbcc(WORD inst, WORD siz);
int m_facos(WORD inst, WORD siz);
int m_fadd(WORD inst, WORD siz);
int m_fasin(WORD inst, WORD siz);
int m_fatan(WORD inst, WORD siz);
int m_fatanh(WORD inst, WORD siz);
int m_fcmp(WORD inst, WORD siz);
int m_fcos(WORD inst, WORD siz);
int m_fcosh(WORD inst, WORD siz);
int m_fdabs(WORD inst, WORD siz);
int m_fdadd(WORD inst, WORD siz);
int m_fdbcc(WORD inst, WORD siz);
int m_fddiv(WORD inst, WORD siz);
int m_fdfsqrt(WORD inst, WORD siz);
int m_fdiv(WORD inst, WORD siz);
int m_fdmove(WORD inst, WORD siz);
int m_fdmul(WORD inst, WORD siz);
int m_fdneg(WORD inst, WORD siz);
int m_fdsub(WORD inst, WORD siz);
int m_fetox(WORD inst, WORD siz);
int m_fetoxm1(WORD inst, WORD siz);
int m_fgetexp(WORD inst, WORD siz);
int m_fgetman(WORD inst, WORD siz);
int m_fint(WORD inst, WORD siz);
int m_fintrz(WORD inst, WORD siz);
int m_flog10(WORD inst, WORD siz);
int m_flog2(WORD inst, WORD siz);
int m_flogn(WORD inst, WORD siz);
int m_flognp1(WORD inst, WORD siz);
int m_fmod(WORD inst, WORD siz);
int m_fmove(WORD inst, WORD siz);
int m_fmovescr(WORD inst, WORD siz);
int m_fmovecr(WORD inst, WORD siz);
int m_fmovem(WORD inst, WORD siz);
int m_fmul(WORD inst, WORD siz);
int m_fneg(WORD inst, WORD siz);
int m_fnop(WORD inst, WORD siz);
int m_frem(WORD inst, WORD siz);
int m_frestore(WORD inst, WORD siz);
int m_fsabs(WORD inst, WORD siz);
int m_fsadd(WORD inst, WORD siz);
int m_fscc(WORD inst, WORD siz);
int m_fscale(WORD inst, WORD siz);
int m_fsdiv(WORD inst, WORD siz);
int m_fsfsqrt(WORD inst, WORD siz);
int m_fsfsub(WORD inst, WORD siz);
int m_fsgldiv(WORD inst, WORD siz);
int m_fsglmul(WORD inst, WORD siz);
int m_fsin(WORD inst, WORD siz);
int m_fsincos(WORD inst, WORD siz);
int m_fsinh(WORD inst, WORD siz);
int m_fsmove(WORD inst, WORD siz);
int m_fsmul(WORD inst, WORD siz);
int m_fsneg(WORD inst, WORD siz);
int m_fsqrt(WORD inst, WORD siz);
int m_fsub(WORD inst, WORD siz);
int m_ftan(WORD inst, WORD siz);
int m_ftanh(WORD inst, WORD siz);
int m_ftentox(WORD inst, WORD siz);
int m_ftst(WORD inst, WORD siz);
int m_ftwotox(WORD inst, WORD siz);
int m_ftrapcc(WORD inst, WORD siz);

// Common error messages
char range_error[] = "expression out of range";
char abs_error[] = "illegal absolute expression";
char seg_error[] = "bad (section) expression";
char rel_error[] = "illegal relative address";
char siz_error[] = "bad size specified";
char undef_error[] = "undefined expression";
char fwd_error[] = "forward or undefined expression";
char unsupport[] = "unsupported for selected CPU";

// Include code tables
MNTAB machtab[] = {
	{ 0xFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0000, 0, m_badmode }, // 0
#include "68ktab.h"
	{  0,  0L,  0L, 0x0000, 0, m_unimp   }            // Last entry
};

// Register number << 9
WORD reg_9[8] = {
	0, 1 << 9, 2 << 9, 3 << 9, 4 << 9, 5 << 9, 6 << 9, 7 << 9
};

// SIZB==>00, SIZW==>01, SIZL==>10, SIZN==>01 << 6
WORD siz_6[] = {
	(WORD)-1,                                        // n/a
	0,                                               // SIZB
	1<<6, (WORD)-1,                                  // SIZW, n/a
	2<<6, (WORD)-1, (WORD)-1, (WORD)-1,              // SIZL, n/a, n/a, n/a
	1<<6                                             // SIZN
};

// Byte/word/long size for MOVE instrs
WORD siz_12[] = {
   (WORD)-1,
   0x1000,                                           // Byte
   0x3000, (WORD)-1,                                 // Word
   0x2000, (WORD)-1, (WORD)-1, (WORD)-1,             // Long
   0x3000                                            // Word (SIZN)
};

// Word/long size (0=.w, 1=.l) in bit 8
WORD lwsiz_8[] = {
   (WORD)-1,                                         // n/a
   (WORD)-1,                                         // SIZB
   0, (WORD)-1,                                      // SIZW, n/a
   1<<8, (WORD)-1, (WORD)-1, (WORD)-1,               // SIZL, n/a, n/a, n/a
   0                                                 // SIZN
};

// Byte/Word/long size (0=.w, 1=.l) in bit 9
WORD lwsiz_9[] = {
	(WORD)-1,
	0,                                               // Byte
	1<<9, (WORD)-1,                                  // Word
	1<<10, (WORD)-1, (WORD)-1, (WORD)-1,             // Long
	1<<9                                             // Word (SIZN)
};

// Addressing mode in bits 6..11 (register/mode fields are reversed)
WORD am_6[] = {
   00000, 01000, 02000, 03000, 04000, 05000, 06000, 07000,
   00100, 01100, 02100, 03100, 04100, 05100, 06100, 07100,
   00200, 01200, 02200, 03200, 04200, 05200, 06200, 07200,
   00300, 01300, 02300, 03300, 04300, 05300, 06300, 07300,
   00400, 01400, 02400, 03400, 04400, 05400, 06400, 07400,
   00500, 01500, 02500, 03500, 04500, 05500, 06500, 07500,
   00600, 01600, 02600, 03600, 04600, 05600, 06600, 07600,
   00700, 01700, 02700, 03700, 04700, 05700, 06700, 07700
};

// Control registers lookup table
WORD CREGlut[21] = {
	// MC68010/MC68020/MC68030/MC68040/CPU32
	0x000,		// Source Function Code(SFC)
	0x001,		// Destination Function Code(DFC)
	0x800,		// User Stack Pointer(USP)
	0x801,		// Vector Base Register(VBR)
	// MC68020 / MC68030 / MC68040
	0x002,		// Cache Control Register(CACR)
	0x802,		// Cache Address Register(CAAR) (020/030 only)
	0x803,		// Master Stack Pointer(MSP)
	0x804,		// Interrupt Stack Pointer(ISP)
	// MC68040 / MC68LC040
	0x003,		// MMU Translation Control Register(TC)
	0x004,		// Instruction Transparent Translation Register 0 (ITT0)
	0x005,		// Instruction Transparent Translation Register 1 (ITT1)
	0x006,		// Data Transparent Translation Register 0 (DTT0)
	0x007,		// Data Transparent Translation Register 1 (DTT1)
	0x805,		// MMU Status Register(MMUSR)
	0x806,		// User Root Pointer(URP)
	0x807,		// Supervisor Root Pointer(SRP)
	// MC68EC040 only
	0x004,		// Instruction Access Control Register 0 (IACR0)
	0x005,		// Instruction Access Control Register 1 (IACR1)
	0x006,		// Data Access Control Register 0 (DACR1)
	0x007,		// Data Access Control Register 1 (DACR1)
	// 68851 only
	0xFFF		// CPU Root Pointer (CRP) - There's no movec with CRP in it, this is just a guard entry
};


// Error messages
int m_unimp(WORD unused1, WORD unused2)
{
	return (int)error("unimplemented mnemonic");
}


//int m_badmode(void)
int m_badmode(WORD unused1, WORD unused2)
{
	return (int)error("inappropriate addressing mode");
}


int m_self(WORD inst, WORD usused)
{
	D_word(inst);
	return OK;
}


//
// Do one EA in bits 0..5
//
// Bits in `inst' have the following meaning:
//
// Bit zero specifies which ea (ea0 or ea1) to generate in the lower six bits
// of the instr.
//
// If bit one is set, the OTHER ea (the one that wasn't generated by bit zero)
// is generated after the instruction. Regardless of bit 0's value, ea0 is
// always deposited in memory before ea1.
//
// If bit two is set, standard size bits are set in the instr in bits 6 and 7.
//
// If bit four is set, bit three specifies which eaXreg to place in bits 9..11
// of the instr.
//
int m_ea(WORD inst, WORD siz)
{
	WORD flg = inst;					// Save flag bits
	inst &= ~0x3F;						// Clobber flag bits in instr

	// Install "standard" instr size bits
	if (flg & 4)
		inst |= siz_6[siz];

	if (flg & 16)
	{
		// OR-in register number
		if (flg & 8)
			inst |= reg_9[a1reg];		// ea1reg in bits 9..11
		else
			inst |= reg_9[a0reg];		// ea0reg in bits 9..11
	}

	if (flg & 1)
	{
		// Use am1
		inst |= am1 | a1reg;			// Get ea1 into instr
		D_word(inst);					// Deposit instr

		// Generate ea0 if requested
		if (flg & 2)
			ea0gen(siz);

		ea1gen(siz);					// Generate ea1
	}
	else
	{
		// Use am0
		inst |= am0 | a0reg;			// Get ea0 into instr
		D_word(inst);					// Deposit instr
		ea0gen(siz);					// Generate ea0

		// Generate ea1 if requested
		if (flg & 2)
			ea1gen(siz);
	}

	return OK;
}


//
// Check if lea x(an),an can be optimised to addq.w #x,an--otherwise fall back
// to m_ea.
//
int m_lea(WORD inst, WORD siz)
{
	if (CHECK_OPTS(OPT_LEA_ADDQ)
		&& ((am0 == ADISP) && (a0reg == a1reg) && (a0exattr & DEFINED))
		&& ((a0exval > 0) && (a0exval <= 8)))
	{
		inst = 0b0101000001001000 | (((uint16_t)a0exval & 7) << 9) | (a0reg);
		D_word(inst);

		if (optim_warn_flag)
			warn("o4: lea size(An),An converted to addq #size,An");

		return OK;
	}

	return m_ea(inst, siz);
}


int m_ea030(WORD inst, WORD siz)
{
	CHECK00;
	WORD flg = inst;					// Save flag bits
	inst &= ~0x3F;						// Clobber flag bits in instr

	// Install "standard" instr size bits
	if (flg & 4)
		inst |= siz_6[siz];

	if (flg & 16)
	{
		// OR-in register number
		if (flg & 8)
		{
			inst |= reg_9[a1reg];		// ea1reg in bits 9..11
		}
		else
		{
			inst |= reg_9[a0reg];		// ea0reg in bits 9..11
		}
	}

	if (flg & 1)
	{
		// Use am1
		inst |= am1 | a1reg;			// Get ea1 into instr
		D_word(inst);					// Deposit instr

		// Generate ea0 if requested
		if (flg & 2)
			ea0gen(siz);

		ea1gen(siz);					// Generate ea1
	}
	else
	{
		// Use am0
		if (am0 == AREG)
			// We get here if we're doing 020+ addressing and an address
			// register is used. For example, something like "tst a0". A bit of
			// a corner case, so kludge it
			a0reg = a0reg + 8;
		else if (am0 == PCDISP)
			// Another corner case (possibly!), so kludge ahoy
			inst |= am0;				// Get ea0 into instr
		else if (am0 == IMMED && am1 == MEMPOST)
		{
			// Added for addi/andi/cmpi/eori/ori/subi #xx,(bd,An,Dm)
			inst |= a1reg | AINDEXED;
		}
		else if (am0 == IMMED)
			inst |= am0 | a0reg;		// Get ea0 into instr
		else if (am0 == AM_CCR)
			inst |= am1 | a1reg;
		else if (am0 == AIND)
			inst |= am0;

		inst |= a0reg;					// Get ea0 into instr
		D_word(inst);					// Deposit instr
		ea0gen(siz);					// Generate ea0

		// Generate ea1 if requested
		if (flg & 2)
			ea1gen(siz);
	}

	return OK;
}


//
// Dx,Dy nnnnXXXnssnnnYYY If bit 0 of `inst' is set, install size bits in bits
// 6..7
//
int m_abcd(WORD inst, WORD siz)
{
	if (inst & 1)
	{
		// Install size bits
		inst--;
		inst |= siz_6[siz];
	}

	inst |= a0reg | reg_9[a1reg];
	D_word(inst);

	return OK;
}


//
// {adda} ea,AREG
//
int m_adda(WORD inst, WORD siz)
{
	if ((a0exattr & DEFINED) && (am0 == IMMED))
	{
		if (CHECK_OPTS(OPT_ADDA_ADDQ))
		{
			if ((a0exval > 1) && (a0exval <= 8))
			{
				// Immediate is between 1 and 8 so let's convert to addq
				return m_addq(0b0101000000000000, siz);

				if (optim_warn_flag)
					warn("o8: adda/suba size(An),An converted to addq/subq #size,An");
			}
		}

		if (CHECK_OPTS(OPT_ADDA_LEA))
		{
			if ((a0exval > 8) && ((a0exval + 0x8000) < 0x10000))
			{
				// Immediate is larger than 8 and word size so let's convert to lea
				am0 = ADISP;    // Change addressing mode
				a0reg = a1reg;  // In ADISP a0reg is used instead of a1reg!

				if (!(inst & (1 << 14)))
				{
					// We have a suba #x,AREG so let's negate the value
					a0exval = -a0exval;
				}

				// We're going to rely on +o4 for this, so let's ensure that
				// it's on, even just for this instruction
				int return_value;
				int temp_flag = optim_flags[OPT_LEA_ADDQ];
				optim_flags[OPT_LEA_ADDQ] = 1;				// Temporarily save switch state
				return_value = m_lea(0b0100000111011000, SIZW);
				optim_flags[OPT_LEA_ADDQ] = temp_flag;		// Restore switch state
				if (optim_warn_flag)
					warn("o9: adda.w/l #x,Ay converted to lea x(Dy),Ay");
				return return_value;
			}
		}
	}

	inst |= am0 | a0reg | lwsiz_8[siz] | reg_9[a1reg];
	D_word(inst);
	ea0gen(siz);	// Generate EA

	return OK;
}


//
// If bit 0 of `inst' is 1, install size bits in bits 6..7 of instr.
// If bit 1 of `inst' is 1, install a1reg in bits 9..11 of instr.
//
int m_reg(WORD inst, WORD siz)
{
	if (inst & 1)
		// Install size bits
		inst |= siz_6[siz];

	if (inst & 2)
		// Install other register (9..11)
		inst |= reg_9[a1reg];

	inst &= ~7;			// Clear off crufty bits
	inst |= a0reg;		// Install first register
	D_word(inst);

	return OK;
}


//
// <op> #expr
//
int m_imm(WORD inst, WORD siz)
{
	D_word(inst);
	ea0gen(siz);

	return OK;
}


//
// <op>.b #expr
//
int m_imm8(WORD inst, WORD siz)
{
	siz = siz;
	D_word(inst);
	ea0gen(SIZB);

	return OK;
}


//
// <shift> Dn,Dn
//
int m_shr(WORD inst, WORD siz)
{
	inst |= reg_9[a0reg] | a1reg | siz_6[siz];
	D_word(inst);

	return OK;
}


//
// <shift> #n,Dn
//
int m_shi(WORD inst, WORD siz)
{
	inst |= a1reg | siz_6[siz];

	if (a0exattr & DEFINED)
	{
		if (a0exval > 8)
			return error(range_error);

		inst |= (a0exval & 7) << 9;
		D_word(inst);
	}
	else
	{
		AddFixup(FU_QUICK, sloc, a0expr);
		D_word(inst);
	}

	return OK;
}


//
// {bset, btst, bchg, bclr} -- #immed,ea -- Dn,ea
//
int m_bitop(WORD inst, WORD siz)
{
	// Enforce instruction sizes
	if (am1 == DREG)
	{                               // X,Dn must be .n or .l
		if (siz & (SIZB | SIZW))
			return error(siz_error);
	}
	else if (siz & (SIZW | SIZL))	// X,ea must be .n or .b
		return error(siz_error);

	// Construct instr and EAs
	inst |= am1 | a1reg;

	if (am0 == IMMED)
	{
		D_word(inst);
		ea0gen(SIZB);				// Immediate bit number
	}
	else
	{
		inst |= reg_9[a0reg];
		D_word(inst);
	}

	// ea to bit-munch
	ea1gen(SIZB);

	return OK;
}


int m_dbra(WORD inst, WORD siz)
{
	siz = siz;
	inst |= a0reg;
	D_word(inst);

	if (a1exattr & DEFINED)
	{
		if ((a1exattr & TDB) != cursect)
			return error(rel_error);

		uint32_t v = (uint32_t)a1exval - sloc;

		if (v + 0x8000 > 0x10000)
			return error(range_error);

		D_word(v);
	}
	else
	{
		AddFixup(FU_WORD | FU_PCREL | FU_ISBRA, sloc, a1expr);
		D_word(0);
	}

	return OK;
}


//
// EXG
//
int m_exg(WORD inst, WORD siz)
{
	int m;

	siz = siz;

	if (am0 == DREG && am1 == DREG)
		m = 0x0040;                      // Dn,Dn
	else if (am0 == AREG && am1 == AREG)
		m = 0x0048;                      // An,An
	else
	{
		if (am0 == AREG)
		{                                // Dn,An or An,Dn
			m = a1reg;                   // Get AREG into a1reg
			a1reg = a0reg;
			a0reg = m;
		}

		m = 0x0088;
	}

	inst |= m | reg_9[a0reg] | a1reg;
	D_word(inst);

	return OK;
}


//
// LINK
//
int m_link(WORD inst, WORD siz)
{
	if (siz != SIZL)
	{
		// Is this an error condition???
	}
	else
	{
		CHECK00;
		inst &= ~((3 << 9) | (1 << 6) | (1 << 4));
		inst |= 1 << 3;
	}

	inst |= a0reg;
	D_word(inst);
	ea1gen(siz);

	return OK;
}


WORD extra_addressing[16]=
{
	0x30,  // 0100 (bd,An,Xn)
	0x30,  // 0101 ([bd,An],Xn,od)
	0x30,  // 0102 ([bc,An,Xn],od)
	0x30,  // 0103 (bd,PC,Xn)
	0x30,  // 0104 ([bd,PC],Xn,od)
	0x30,  // 0105 ([bc,PC,Xn],od)
	0,     // 0106
	0,     // 0107
	0,     // 0110
	0,     // 0111 Nothing
	0x30,  // 0112 (Dn.w)
	0x30,  // 0113 (Dn.l)
	0,     // 0114
	0,     // 0115
	0,     // 0116
	0      // 0117
};


//
// Handle MOVE <C_ALL> <C_ALTDATA>
//        MOVE <C_ALL> <M_AREG>
//
// Optimize MOVE.L #<smalldata>,D0 to a MOVEQ
//
int m_move(WORD inst, WORD size)
{
	// Cast the passed in value to an int
	int siz = (int)size;

	// Try to optimize to MOVEQ
	// N.B.: We can get away with casting the uint64_t to a 32-bit value
	//       because it checks for a SIZL (i.e., a 32-bit value).
	if (CHECK_OPTS(OPT_MOVEL_MOVEQ)
		&& (siz == SIZL) && (am0 == IMMED) && (am1 == DREG)
		&& ((a0exattr & (TDB | DEFINED)) == DEFINED)
		&& ((uint32_t)a0exval + 0x80 < 0x100))
	{
		m_moveq((WORD)0x7000, (WORD)0);

		if (optim_warn_flag)
			warn("o1: move.l #size,dx converted to moveq");
	}
	else
	{
		if ((am0 < ABASE) && (am1 < ABASE))			// 68000 modes
		{
			inst |= siz_12[siz] | am_6[am1] | reg_9[a1reg] | am0 | a0reg;

			D_word(inst);

			if (am0 >= ADISP)
				ea0gen((WORD)siz);

			if (am1 >= ADISP)
				ea1gen((WORD)siz | 0x8000);   // Tell ea1gen we're move ea,ea
		}
		else					// 68020+ modes
		{
			inst |= siz_12[siz] | reg_9[a1reg] | extra_addressing[am0 - ABASE];

			D_word(inst);

			if (am0 >= ADISP)
				ea0gen((WORD)siz);

			if (am1 >= ADISP)
				ea1gen((WORD)siz);
		}
	}

	return OK;
}


//
// Handle MOVE <C_ALL030> <C_ALTDATA>
//		MOVE <C_ALL030> <M_AREG>
//
int m_move30(WORD inst, WORD size)
{
	int siz = (int)size;

	if (am0 > ABASE)
		inst |= siz_12[siz] | reg_9[a1reg & 7] | a0reg | extra_addressing[am0 - ABASE];
	else
		inst |= siz_12[siz] | reg_9[a1reg & 7] | a0reg | extra_addressing[am1 - ABASE] << 3;

	D_word(inst);

	if (am0 >= ADISP)
		ea0gen((WORD)siz);

	if (am1 >= ADISP)
		ea1gen((WORD)siz);

	return OK;
}


//
// move USP,An -- move An,USP
//
int m_usp(WORD inst, WORD siz)
{
	siz = siz;

	if (am0 == AM_USP)
		inst |= a1reg;		// USP, An
	else
		inst |= a0reg;		// An, USP

	D_word(inst);

	return OK;
}


//
// moveq
//
int m_moveq(WORD inst, WORD siz)
{
	siz = siz;

	// Arrange for future fixup
	if (!(a0exattr & DEFINED))
	{
		AddFixup(FU_BYTE | FU_SEXT, sloc + 1, a0expr);
		a0exval = 0;
	}
	else if ((uint32_t)a0exval + 0x100 >= 0x200)
		return error(range_error);

	inst |= reg_9[a1reg] | (a0exval & 0xFF);
	D_word(inst);

	return OK;
}


//
// movep Dn, disp(An) -- movep disp(An), Dn
//
int m_movep(WORD inst, WORD siz)
{
	// Tell ea0gen to lay off the 0(a0) optimisations on this one
	movep = 1;

	if (siz == SIZL)
		inst |= 0x0040;

	if (am0 == DREG)
	{
		inst |= reg_9[a0reg] | a1reg;
		D_word(inst);

		if (am1 == AIND)
			D_word(0)
		else
			ea1gen(siz);
	}
	else
	{
		inst |= reg_9[a1reg] | a0reg;
		D_word(inst);

		if (am0 == AIND)
			D_word(0)
		else
			ea0gen(siz);
	}

	movep = 0;
	return 0;
}


//
// Bcc -- BSR
//
int m_br(WORD inst, WORD siz)
{
	if (a0exattr & DEFINED)
	{
		if ((a0exattr & TDB) != cursect)
//{
//printf("m_br(): a0exattr = %X, cursect = %X, a0exval = %X, sloc = %X\n", a0exattr, cursect, a0exval, sloc);
			return error(rel_error);
//}

		uint32_t v = (uint32_t)a0exval - (sloc + 2);

		// Optimize branch instr. size
		if (siz == SIZN)
		{
			if (CHECK_OPTS(OPT_BSR_BCC_S) && (v != 0) && ((v + 0x80) < 0x100))
			{
				// Fits in .B
				inst |= v & 0xFF;
				D_word(inst);

				if (optim_warn_flag)
					warn("o2: Bcc.w/BSR.w converted to .s");

				return OK;
			}
			else
			{
				// Fits in .W
				if ((v + 0x8000) > 0x10000)
					return error(range_error);

				D_word(inst);
				D_word(v);
				return OK;
			}
		}

		if (siz == SIZB || siz == SIZS)
		{
			if ((v + 0x80) >= 0x100)
				return error(range_error);

			inst |= v & 0xFF;
			D_word(inst);
		}
		else
		{
			if ((v + 0x8000) >= 0x10000)
				return error(range_error);

			D_word(inst);
			D_word(v);
		}

		return OK;
	}
	else if (siz == SIZN)
		siz = SIZW;

	if (siz == SIZB || siz == SIZS)
	{
		// .B
		AddFixup(FU_BBRA | FU_PCREL | FU_SEXT, sloc, a0expr);
		// So here we have a small issue: this bra.s could be zero offset, but
		// we can never know. Because unless we know beforehand that the
		// offset will be zero (i.e. "bra.s +0"), it's going to be a label
		// below this instruction! We do have an optimisation flag that can
		// check against this during fixups, but we cannot rely on the state
		// of the flag after all the file(s) have been processed because its
		// state might have changed multiple times during file parsing. (Yes,
		// there's a very low chance that this will ever happen but it's not
		// zero!). So, we can use the byte that is going to be filled during
		// fixups to store the state of the optimisation flag and read it
		// during that stage so each bra.s will have its state stored neatly.
		// Sleazy? Eh, who cares, like this will ever happen ;)
		// One final note: we'd better be damn sure that the flag's value is
		// less than 256 or magical stuff will happen!
		D_word(inst | optim_flags[OPT_NULL_BRA]);
		return OK;
	}
	else
	{
		// .W
		D_word(inst);
		AddFixup(FU_WORD | FU_PCREL | FU_LBRA | FU_ISBRA, sloc, a0expr);
		D_word(0);
	}

	return OK;
}


//
// ADDQ -- SUBQ
//
int m_addq(WORD inst, WORD siz)
{
	inst |= siz_6[siz] | am1 | a1reg;

	if (a0exattr & DEFINED)
	{
		if ((a0exval > 8) || (a0exval == 0))	// Range in 1..8
			return error(range_error);

		inst |= (a0exval & 7) << 9;
		D_word(inst);
	}
	else
	{
		AddFixup(FU_QUICK, sloc, a0expr);
		D_word(inst);
	}

	ea1gen(siz);

	return OK;
}


//
// trap #n
//
int m_trap(WORD inst, WORD siz)
{
	siz = siz;

	if (a0exattr & DEFINED)
	{
		if (a0exattr & TDB)
			return error(abs_error);

		if (a0exval >= 16)
			return error(range_error);

		inst |= a0exval;
		D_word(inst);
	}
	else
		return error(undef_error);

	return OK;
}


//
// movem <rlist>,ea -- movem ea,<rlist>
//
int m_movem(WORD inst, WORD siz)
{
	uint64_t eval;
	WORD i;
	WORD w;
	WORD rmask;

	if (siz & SIZB)
		return error("bad size suffix");

	if (siz == SIZL)
		inst |= 0x0040;

	if (*tok == '#')
	{
		// Handle #<expr>, ea
		tok++;

		if (abs_expr(&eval) != OK)
			return OK;

		if (eval >= 0x10000L)
			return error(range_error);

		rmask = (WORD)eval;
		goto immed1;
	}

	if ((*tok >= REG68_D0) && (*tok <= REG68_A7))
	{
		// <rlist>, ea
		if (reglist(&rmask) < 0)
			return OK;

immed1:
		if (*tok++ != ',')
			return error("missing comma");

		if (amode(0) < 0)
			return OK;

		inst |= am0 | a0reg;

		if (!(amsktab[am0] & (C_ALTCTRL | M_APREDEC)))
			return error("invalid addressing mode");

		// If APREDEC, reverse register mask
		if (am0 == APREDEC)
		{
			w = rmask;
			rmask = 0;

			for(i=0x8000; i; i>>=1, w>>=1)
				rmask = (WORD)((rmask << 1) | (w & 1));
		}
	}
	else
	{
		// ea, <rlist>
		if (amode(0) < 0)
			return OK;

		inst |= 0x0400 | am0 | a0reg;

		if (*tok++ != ',')
			return error("missing comma");

		if (*tok == EOL)
			return error("missing register list");

		if (*tok == '#')
		{
			// ea, #<expr>
			tok++;

			if (abs_expr(&eval) != OK)
				return OK;

			if (eval >= 0x10000)
				return error(range_error);

			rmask = (WORD)eval;
		}
		else if (reglist(&rmask) < 0)
			return OK;

		if (!(amsktab[am0] & (C_CTRL | M_APOSTINC)))
			return error("invalid addressing mode");
	}

	D_word(inst);
	D_word(rmask);
	ea0gen(siz);

	return OK;
}


//
// CLR.x An ==> SUBA.x An,An
//
int m_clra(WORD inst, WORD siz)
{
	inst |= a0reg | reg_9[a0reg] | lwsiz_8[siz];
	D_word(inst);

	return OK;
}


//
// CLR.L Dn ==> CLR.L An or MOVEQ #0,Dx
//
int m_clrd(WORD inst, WORD siz)
{
	if (!CHECK_OPTS(OPT_CLR_DX))
		inst |= a0reg;
	else
	{
		inst = (a0reg << 9) | 0b0111000000000000;
		if (optim_warn_flag)
			warn("o7: clr.l Dx converted to moveq #0,Dx");
	}

	D_word(inst);

	return OK;
}


////////////////////////////////////////
//
// 68020/30/40/60 instructions
//
////////////////////////////////////////

//
// Bcc.l -- BSR.l
//
int m_br30(WORD inst, WORD siz)
{
	if (a0exattr & DEFINED)
	{
		if ((a0exattr & TDB) != cursect)
			return error(rel_error);

		uint32_t v = (uint32_t)a0exval - (sloc + 2);
		D_word(inst);
		D_long(v);

		return OK;
	}
	else
	{
		// .L
		AddFixup(FU_LONG | FU_PCREL | FU_SEXT, sloc, a0expr);
		D_word(inst);
		return OK;
	}
}


//
// bfchg, bfclr, bfexts, bfextu, bfffo, bfins, bfset
// (68020, 68030, 68040)
//
int m_bfop(WORD inst, WORD siz)
{
	if ((bfval1 > 31) || (bfval1 < 0))
		return error("bfxxx offset: immediate value must be between 0 and 31");

	// First instruction word - just the opcode and first EA
	// Note: both am1 is ORed because solely of bfins - maybe it's a good idea
	// to make a dedicated function for it?
	if (am1 == AM_NONE)
	{
		am1 = 0;
	}
	else
	{
		if (bfval2 > 31 || bfval2 < 0)
			return error("bfxxx width: immediate value must be between 0 and 31");

		// For Dw both immediate and register number are stuffed
		// into the same field O_o
		bfparam2 = (bfval2 << 0);
	}

	if (bfparam1 == 0)
		bfparam1 = (bfval1 << 6);
	else
		bfparam1 = bfval1 << 12;

	//D_word((inst | am0 | a0reg | am1 | a1reg));
	if (inst == 0b1110111111000000)
	{
		// bfins special case
		D_word((inst | am1 | a1reg));
	}
	else
	{
		D_word((inst | am0 | a0reg));
	}

	ea0gen(siz);	// Generate EA

	// Second instruction word - Dest register (if exists), Do, Offset, Dw, Width
	if (inst == 0b1110111111000000)
	{
		// bfins special case
		inst = bfparam1 | bfparam2;

		if (am1 == DREG)
			inst |= a0reg << 12;

		D_word(inst);
	}
	else
	{
		inst = bfparam1 | bfparam2;

		if (am1 == DREG)
			inst |= a0reg << 0;

		if (am0 == DREG)
			inst |= a1reg << 12;

		D_word(inst);
	}

	return OK;
}


//
// bkpt (68EC000, 68010, 68020, 68030, 68040, CPU32)
//
int m_bkpt(WORD inst, WORD siz)
{
	CHECK00;

	if (a0exattr & DEFINED)
	{
		if (a0exattr & TDB)
			return error(abs_error);

		if (a0exval >= 8)
			return error(range_error);

		inst |= a0exval;
		D_word(inst);
	}
	else
		return error(undef_error);

	return OK;
}


//
// callm (68020)
//
int m_callm(WORD inst, WORD siz)
{
	CHECKNO20;

	inst |= am1;
	D_word(inst);

	if (a0exattr & DEFINED)
	{
		if (a0exattr & TDB)
			return error(abs_error);

		if (a0exval > 255)
			return error(range_error);

		inst = (uint16_t)a0exval;
		D_word(inst);
	}
	else
		return error(undef_error);

	ea1gen(siz);

	return OK;

}


//
// cas (68020, 68030, 68040)
//
int m_cas(WORD inst, WORD siz)
{
	WORD inst2;
	LONG amsk;
	int modes;

	if ((activecpu & (CPU_68020 | CPU_68030 | CPU_68040)) == 0)
		return error(unsupport);

	switch (siz)
	{
	case SIZB:
		inst |= 1 << 9;
		break;
	case SIZW:
	case SIZN:
		inst |= 2 << 9;
		break;
	case SIZL:
		inst |= 3 << 9;
		break;
	default:
		return error("bad size suffix");
		break;
	}

	// Dc
	if ((*tok < REG68_D0) && (*tok > REG68_D7))
		return error("CAS accepts only data registers");

	inst2 = (*tok++) & 7;

	if (*tok++ != ',')
		return error("missing comma");

	// Du
	if ((*tok < REG68_D0) && (*tok > REG68_D7))
		return error("CAS accepts only data registers");

	inst2 |= ((*tok++) & 7) << 6;

	if (*tok++ != ',')
		return error("missing comma");

	// ea
	if ((modes = amode(1)) < 0)
		return OK;

	if (modes > 1)
		return error("too many ea fields");

	if (*tok != EOL)
		return error("extra (unexpected) text found");

	// Reject invalid ea modes
	amsk = amsktab[am0];

	if ((amsk & (M_AIND | M_APOSTINC | M_APREDEC | M_ADISP | M_AINDEXED | M_ABSW | M_ABSL | M_ABASE | M_MEMPOST | M_MEMPRE)) == 0)
		return error("unsupported addressing mode");

	inst |= am0 | a0reg;
	D_word(inst);
	D_word(inst2);
	ea0gen(siz);

	return OK;
}


//
// cas2 (68020, 68030, 68040)
//
int m_cas2(WORD inst, WORD siz)
{
	WORD inst2, inst3;

	if ((activecpu & (CPU_68020 | CPU_68030 | CPU_68040)) == 0)
		return error(unsupport);

	switch (siz)
	{
	case SIZB:
		inst |= 1 << 9;
		break;
	case SIZW:
	case SIZN:
		inst |= 2 << 9;
		break;
	case SIZL:
		inst |= 3 << 9;
		break;
	default:
		return error("bad size suffix");
		break;
	}

	// Dc1
	if ((*tok < REG68_D0) && (*tok > REG68_D7))
		return error("CAS2 accepts only data registers for Dx1:Dx2 pairs");

	inst2 = (*tok++) & 7;

	if (*tok++ != ':')
		return error("missing colon");

	// Dc2
	if ((*tok < REG68_D0) && (*tok > REG68_D7))
		return error("CAS2 accepts only data registers for Dx1:Dx2 pairs");

	inst3 = (*tok++) & 7;

	if (*tok++ != ',')
		return error("missing comma");

	// Du1
	if ((*tok < REG68_D0) && (*tok > REG68_D7))
		return error("CAS2 accepts only data registers for Dx1:Dx2 pairs");

	inst2 |= ((*tok++) & 7) << 6;

	if (*tok++ != ':')
		return error("missing colon");

	// Du2
	if ((*tok < REG68_D0) && (*tok > REG68_D7))
		return error("CAS2 accepts only data registers for Dx1:Dx2 pairs");

	inst3 |= ((*tok++) & 7) << 6;

	if (*tok++ != ',')
		return error("missing comma");

	// Rn1
	if (*tok++ != '(')
		return error("missing (");
	if ((*tok >= REG68_D0) && (*tok <= REG68_D7))
		inst2 |= (((*tok++) & 7) << 12) | (0 << 15);
	else if ((*tok >= REG68_A0) && (*tok <= REG68_A7))
		inst2 |= (((*tok++) & 7) << 12) | (1 << 15);
	else
		return error("CAS accepts either data or address registers for Rn1:Rn2 pair");

	if (*tok++ != ')')
		return error("missing (");

	if (*tok++ != ':')
		return error("missing colon");

	// Rn2
	if (*tok++ != '(')
		return error("missing (");
	if ((*tok >= REG68_D0) && (*tok <= REG68_D7))
		inst3 |= (((*tok++) & 7) << 12) | (0 << 15);
	else if ((*tok >= REG68_A0) && (*tok <= REG68_A7))
		inst3 |= (((*tok++) & 7) << 12) | (1 << 15);
	else
		return error("CAS accepts either data or address registers for Rn1:Rn2 pair");

	if (*tok++ != ')')
		return error("missing (");

	if (*tok != EOL)
		return error("extra (unexpected) text found");

	D_word(inst);
	D_word(inst2);
	D_word(inst3);

	return OK;
}


//
// cmp2 (68020, 68030, 68040, CPU32)
//
int m_cmp2(WORD inst, WORD siz)
{
	if ((activecpu & (CPU_68020 | CPU_68030 | CPU_68040)) == 0)
		return error(unsupport);

	switch (siz & 0x000F)
	{
	case SIZW:
	case SIZN:
		inst |= 1 << 9;
		break;
	case SIZL:
		inst |= 2 << 9;
		break;
	default:
		// SIZB
		break;
	}

	WORD flg = inst;					// Save flag bits
	inst &= ~0x3F;						// Clobber flag bits in instr

	// Install "standard" instr size bits
	if (flg & 4)
		inst |= siz_6[siz];

	if (flg & 16)
	{
		// OR-in register number
		if (flg & 8)
			inst |= reg_9[a1reg];		// ea1reg in bits 9..11
		else
			inst |= reg_9[a0reg];		// ea0reg in bits 9..11
	}

	if (flg & 1)
	{
		// Use am1
		inst |= am1 | a1reg;			// Get ea1 into instr
		D_word(inst);					// Deposit instr

		// Generate ea0 if requested
		if (flg & 2)
			ea0gen(siz);

		ea1gen(siz);					// Generate ea1
	}
	else
	{
		// Use am0
		inst |= am0 | a0reg;			// Get ea0 into instr
		D_word(inst);					// Deposit instr
		ea0gen(siz);					// Generate ea0

		// Generate ea1 if requested
		if (flg & 2)
			ea1gen(siz);
	}

	// If we're called from chk2 then bit 11 of size will be set. This is just
	// a dumb mechanism to pass this, required by the extension word. (You might
	// have noticed the siz & 15 thing above!)
	inst = (a1reg << 12) | (siz & (1 << 11));

	if (am1 == AREG)
		inst |= 1 << 15;

	D_word(inst);

	return OK;
}


//
// chk2 (68020, 68030, 68040, CPU32)
//
int m_chk2(WORD inst, WORD siz)
{
	return m_cmp2(inst, siz | (1 << 11));
}


//
// cpbcc(68020, 68030, 68040 (FBcc), 68060 (FBcc)), pbcc (68851)
//
int m_fpbr(WORD inst, WORD siz)
{

	if (a0exattr & DEFINED)
	{
		if ((a0exattr & TDB) != cursect)
			return error(rel_error);

		uint32_t v = (uint32_t)a0exval - (sloc + 2);

		// Optimize branch instr. size
		if (siz == SIZL)
		{
			if ((v != 0) && ((v + 0x8000) < 0x10000))
			{
				inst |= (1 << 6);
				D_word(inst);
				D_long(v);
				return OK;
			}
		}
		else // SIZW/SIZN
		{
			if ((v + 0x8000) >= 0x10000)
				return error(range_error);

			D_word(inst);
			D_word(v);
		}

		return OK;
	}
	else if (siz == SIZN)
		siz = SIZW;

	if (siz == SIZL)
	{
		// .L
		D_word(inst);
		AddFixup(FU_LONG | FU_PCREL | FU_SEXT, sloc, a0expr);
		D_long(0);
		return OK;
	}
	else
	{
		// .W
		D_word(inst);
		AddFixup(FU_WORD | FU_PCREL | FU_SEXT, sloc, a0expr);
		D_word(0);
	}

	return OK;
}


//
// cpbcc(68020, 68030, 68040 (FBcc), 68060 (FBcc))
//
int m_cpbcc(WORD inst, WORD siz)
{
	if (!(activecpu & (CPU_68020 | CPU_68030)))
		return error(unsupport);

	return m_fpbr(inst, siz);
}


//
// fbcc(6808X, 68040, 68060)
//
int m_fbcc(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return m_fpbr(inst, siz);
}


//
// pbcc(68851 but let's assume 68020 only)
//
int m_pbcc(WORD inst, WORD siz)
{
	CHECKNO20;
	return m_fpbr(inst, siz);
}


//
// cpdbcc(68020, 68030)
//
int m_cpdbr(WORD inst, WORD siz)
{
	CHECK00;

	uint32_t v;
	WORD condition = inst & 0x1F; // Grab condition sneakily placed in the lower 5 bits of inst
	inst &= 0xFFE0;               // And then mask them out - you ain't seen me, roit?

	inst |= (1 << 9);	// Bolt on FPU id
	inst |= a0reg;

	D_word(inst);

	D_word(condition);

	if (a1exattr & DEFINED)
	{
		if ((a1exattr & TDB) != cursect)
			return error(rel_error);

		v = (uint32_t)a1exval - sloc;

		if (v + 0x8000 > 0x10000)
			return error(range_error);

		D_word(v);
	}
	else
	{
		AddFixup(FU_WORD | FU_PCREL | FU_ISBRA, sloc, a1expr);
		D_word(0);
	}

	return OK;

}


//
// muls.l / divs.l / divu.l / mulu.l (68020+)
//
int m_muls(WORD inst, WORD siz)
{
	if ((activecpu & (CPU_68020 | CPU_68030 | CPU_68040)) == 0)
		return error(unsupport);

	WORD flg = inst;					// Save flag bits
	inst &= ~0x33F;						// Clobber flag and extension bits in instr

	// Install "standard" instr size bits
	if (flg & 4)
		inst |= siz_6[siz];

	if (flg & 16)
	{
		// OR-in register number
		if (flg & 8)
			inst |= reg_9[a1reg];		// ea1reg in bits 9..11
		else
			inst |= reg_9[a0reg];		// ea0reg in bits 9..11
	}

	// Regarding extension word: bit 11 is signed/unsigned selector
	//                           bit 10 is 32/64 bit selector
	// Both of these are packed in bits 9 and 8 of the instruction
	// field in 68ktab. Extra compilcations arise from the fact we
	// have to distinguish between divu/s.l Dn,Dm (which is encoded
	// as divu/s.l Dn,Dm:Dm) and divu/s.l Dn,Dm:Dx - the first is
	// 32 bit while the second 64 bit

	if (flg & 1)
	{
		// Use am1
		inst |= am1 | a1reg;			// Get ea1 into instr
		D_word(inst);					// Deposit instr

		// Extension word
		if (a1reg == a2reg)
			inst = a1reg + (a2reg << 12) + ((flg & 0x200) << 2);
		else
			inst = a1reg + (a2reg << 12) + ((flg & 0x300) << 2);

		 D_word(inst);

		// Generate ea0 if requested
		if (flg & 2)
			ea0gen(siz);

		ea1gen(siz);					// Generate ea1

		return OK;
	}
	else
	{
		// Use am0
		inst |= am0 | a0reg;			// Get ea0 into instr
		D_word(inst);					// Deposit instr

		// Extension word
		if (a1reg == a2reg)
			inst = a1reg + (a2reg << 12) + ((flg & 0x200) << 2);
		else
			inst = a1reg + (a2reg << 12) + ((flg & 0x300) << 2);

		D_word(inst);

		ea0gen(siz);					// Generate ea0

		// Generate ea1 if requested
		if (flg & 2)
			ea1gen(siz);

		return OK;
	}
}


//
// move16 (ax)+,(ay)+
//
int m_move16a(WORD inst, WORD siz)
{
	if ((activecpu & (CPU_68040 | CPU_68060)) == 0)
		return error(unsupport);

	inst |= a0reg;
	D_word(inst);
	inst = (1 << 15) + (a1reg << 12);
	D_word(inst);

	return OK;
}


//
// move16 with absolute address
//
int m_move16b(WORD inst, WORD siz)
{
	if ((activecpu & (CPU_68040 | CPU_68060)) == 0)
		return error(unsupport);

	int v;
	inst |= a1reg;
	D_word(inst);

	if (am0 == APOSTINC)
	{
		if (am1 == AIND)
			return error("Wasn't this suppose to call m_move16a???");
		else
		{
			// move16 (ax)+,(xxx).L
			inst |= 0 << 3;
			v = (int)a1exval;
		}
	}
	else if (am0 == ABSL)
	{
		if (am1 == AIND)
		{
			// move16 (xxx).L,(ax)+
			inst |= 1 << 3;
			v = (int)a0exval;
		}
		else // APOSTINC
		{
			// move16 (xxx).L,(ax)
			inst |= 3 << 3;
			v = (int)a0exval;
		}
	}
	else if (am0 == AIND)
	{
		// move16 (ax),(xxx).L
		inst |= 2 << 3;
		v = (int)a1exval;
	}

	D_word(inst);
	D_long(v);

	return OK;
}


//
// pack/unpack (68020/68030/68040)
//
int m_pack(WORD inst, WORD siz)
{
	CHECK00;

	if (siz != SIZN)
		return error("bad size suffix");

	if (*tok >= REG68_D0 && *tok <= REG68_D7)
	{
		// Dx,Dy,#<adjustment>
		inst |= (0 << 3);   // R/M
		inst |= (*tok++ & 7);

		if (*tok != ',' && tok[2] != ',')
			return error("missing comma");

		if (tok[1] < REG68_D0 && tok[1] > REG68_D7)
			return error(syntax_error);

		inst |= ((tok[1] & 7)<<9);
		tok = tok + 3;
		D_word(inst);
		// Fall through for adjustment (common in both valid cases)
	}
	else if (*tok == '-')
	{
		// -(Ax),-(Ay),#<adjustment>
		inst |= (1 << 3);   // R/M
		tok++;  // eat the minus

		if ((*tok != '(') && (tok[2]!=')') && (tok[3]!=',') && (tok[4] != '-') && (tok[5] != '(') && (tok[7] != ')') && (tok[8] != ','))
			return error(syntax_error);

		if (tok[1] < REG68_A0 && tok[1] > REG68_A7)
			return error(syntax_error);

		if (tok[5] < REG68_A0 && tok[6] > REG68_A7)
			return error(syntax_error);

		inst |= ((tok[1] & 7) << 0);
		inst |= ((tok[6] & 7) << 9);
		tok = tok + 9;
		D_word(inst);
		// Fall through for adjustment (common in both valid cases)
	}
	else
		return error("invalid syntax");

	if ((*tok != CONST) && (*tok != SYMBOL) && (*tok != '-'))
		return error(syntax_error);

	if (expr(a0expr, &a0exval, &a0exattr, &a0esym) == ERROR)
		return ERROR;

	if ((a0exattr & DEFINED) == 0)
		return error(undef_error);

	if (a0exval + 0x8000 > 0x10000)
		return error("");

	if (*tok != EOL)
		return error(extra_stuff);

	D_word((a0exval & 0xFFFF));

	return OK;
}


//
// rtm Rn
//
int m_rtm(WORD inst, WORD siz)
{
	CHECKNO20;

	if (am0 == DREG)
	{
		inst |= a0reg;
	}
	else if (am0 == AREG)
	{
		inst |= (1 << 3) + a0reg;
	}
	else
		return error("rtm only allows data or address registers.");

	D_word(inst);

	return OK;
}


//
// rtd #n
//
int m_rtd(WORD inst, WORD siz)
{
	CHECK00;

	if (a0exattr & DEFINED)
	{
		if (a0exattr & TDB)
			return error(abs_error);

		if ((a0exval + 0x8000) <= 0x7FFF)
			return error(range_error);

		D_word(inst);
		D_word(a0exval);
	}
	else
		return error(undef_error);

	return OK;
}


//
// trapcc
//
int m_trapcc(WORD inst, WORD siz)
{
	CHECK00;

	if (am0 == AM_NONE)
	{
		D_word(inst);
	}
	else if (am0 == IMMED)
	{
		if (siz == SIZW)
		{
			if (a0exval < 0x10000)
			{
				inst |= 2;
				D_word(inst);
				D_word(a0exval);
			}
			else
				return error("Immediate value too big");
		}
		else //DOTL
		{
			inst |= 3;
			D_word(inst);
			D_long(a0exval);
		}
	}
	else
		return error("Invalid parameter for trapcc");

	return OK;
}


//
// cinvl/p/a (68040/68060)
//
int m_cinv(WORD inst, WORD siz)
{
	CHECKNO40;

	if (am1 == AM_NONE)
		inst |= (0 << 6) | (a1reg);
	switch (a0reg)
	{
	case 0:     // REG68_IC40
		inst |= (2 << 6) | (a1reg);
		break;
	case 1:     // REG68_DC40
		inst |= (1 << 6) | (a1reg);
		break;
	case 2:     // REG68_BC40
		inst |= (3 << 6) | (a1reg);
		break;
	}

	D_word(inst);
	return OK;
}


int m_fpusavrest(WORD inst, WORD siz)
{
	inst |= am0 | a0reg;
	D_word(inst);
	ea0gen(siz);

	return OK;
}


//
// cpSAVE/cpRESTORE (68020, 68030)
//
int m_cprest(WORD inst, WORD siz)
{
	if (activecpu & !(CPU_68020 | CPU_68030))
		return error(unsupport);

	return m_fpusavrest(inst, siz);

}


//
// FSAVE/FRESTORE (68040, 68060)
//
int m_frestore(WORD inst, WORD siz)
{
	if ((!(activecpu & (CPU_68040 | CPU_68060))) ||
		(activefpu&(FPU_68881 | FPU_68882)))
		return error(unsupport);

	return m_fpusavrest(inst, siz);
}


//
// movec (68010, 68020, 68030, 68040, 68060, CPU32)
//
int m_movec(WORD inst, WORD siz)
{
	CHECK00;

	if (am0 == DREG || am0 == AREG)
	{
		// movec Rn,Rc
		inst |= 1;
		D_word(inst);

		if (am0 == DREG)
		{
			inst = (0 << 15) + (a0reg << 12) + CREGlut[a1reg];
			D_word(inst);
		}
		else
		{
			inst = (1 << 15) + (a0reg << 12) + CREGlut[a1reg];
			D_word(inst);
		}
	}
	else
	{
		// movec Rc,Rn
		D_word(inst);

		if (am1 == DREG)
		{
			inst = (0 << 15) + (a1reg << 12) + CREGlut[a0reg];
			D_word(inst);
		}
		else
		{
			inst = (1 << 15) + (a1reg << 12) + CREGlut[a0reg];
			D_word(inst);
		}
	}

	return OK;
}


//
// moves (68010, 68020, 68030, 68040, CPU32)
//
int m_moves(WORD inst, WORD siz)
{
	if (activecpu & !(CPU_68020 | CPU_68030 | CPU_68040))
		return error(unsupport);

	if (siz == SIZB)
		inst |= 0 << 6;
	else if (siz == SIZL)
		inst |= 2 << 6;
	else // SIZW/SIZN
		inst |= 1 << 6;

	if (am0 == DREG)
	{
		inst |= am1 | a1reg;
		D_word(inst);
		inst = (a0reg << 12) | (1 << 11) | (0 << 15);
		D_word(inst);
	}
	else if (am0 == AREG)
	{
		inst |= am1 | a1reg;
		D_word(inst);
		inst = (a0reg << 12) | (1 << 11) | (1 << 15);
		D_word(inst);
	}
	else
	{
		if (am1 == DREG)
		{
			inst |= am0 | a0reg;
			D_word(inst);
			inst = (a1reg << 12) | (0 << 11) | (0 << 15);
			D_word(inst);
		}
		else
		{
			inst |= am0 | a0reg;
			D_word(inst);
			inst = (a1reg << 12) | (0 << 11) | (1 << 15);
			D_word(inst);
		}
	}

	return OK;
}


//
// pflusha (68030, 68040)
//
int m_pflusha(WORD inst, WORD siz)
{
	if (activecpu == CPU_68030)
	{
		D_word(inst);
		inst = (1 << 13) | (1 << 10) | (0 << 5) | 0;
		D_word(inst);
		return OK;
	}
	else if (activecpu == CPU_68040)
	{
		inst = 0b1111010100011000;
		D_word(inst);
		return OK;
	}
	else
		return error(unsupport);

	return OK;
}


//
// pflush (68030, 68040, 68060)
//
int m_pflush(WORD inst, WORD siz)
{
	if (activecpu == CPU_68030)
	{
		// PFLUSH FC, MASK
		// PFLUSH FC, MASK, < ea >
		WORD mask, fc;

		switch ((int)*tok)
		{
		case '#':
			tok++;

			if (*tok != CONST && *tok != SYMBOL)
				return error("function code should be an expression");

			if (expr(a0expr, &a0exval, &a0exattr, &a0esym) == ERROR)
				return ERROR;

			if ((a0exattr & DEFINED) == 0)
				return error("function code immediate should be defined");

			if (a0exval > 7)
				return error("function code out of range (0-7)");

			fc = (uint16_t)a0exval;
			break;
		case REG68_D0:
		case REG68_D1:
		case REG68_D2:
		case REG68_D3:
		case REG68_D4:
		case REG68_D5:
		case REG68_D6:
		case REG68_D7:
			fc = (1 << 4) | (*tok++ & 7);
			break;
		case REG68_SFC:
			fc = 0;
			tok++;
			break;
		case REG68_DFC:
			fc = 1;
			tok++;
			break;
		default:
			return error(syntax_error);
		}

		if (*tok++ != ',')
			return error("comma exptected");

		if (*tok++ != '#')
			return error("mask should be an immediate value");

		if (*tok != CONST && *tok != SYMBOL)
			return error("mask is supposed to be immediate");

		if (expr(a0expr, &a0exval, &a0exattr, &a0esym) == ERROR)
			return ERROR;

		if ((a0exattr & DEFINED) == 0)
			return error("mask immediate value should be defined");

		if (a0exval > 7)
			return error("function code out of range (0-7)");

		mask = (uint16_t)a0exval << 5;

		if (*tok == EOL)
		{
			// PFLUSH FC, MASK
			D_word(inst);
			inst = (1 << 13) | fc | mask | (4 << 10);
			D_word(inst);
			return OK;
		}
		else if (*tok == ',')
		{
			// PFLUSH FC, MASK, < ea >
			tok++;

			if (amode(0) == ERROR)
				return ERROR;

			if (*tok != EOL)
				return error(extra_stuff);

			if (am0 == AIND || am0 == ABSW || am0 == ABSL || am0 == ADISP || am0 == ADISP || am0 == AINDEXED || am0 == ABASE || am0 == MEMPOST || am0 == MEMPRE)
			{
				inst |= am0 | a0reg;
				D_word(inst);
				inst = (1 << 13) | fc | mask | (6 << 10);
				D_word(inst);
				ea0gen(siz);
				return OK;
			}
			else
				return error("unsupported addressing mode");

		}
		else
			return error(syntax_error);

		return OK;
	}
	else if (activecpu == CPU_68040 || activecpu == CPU_68060)
	{
		// PFLUSH(An)
		// PFLUSHN(An)
		if (*tok != '(' && tok[2] != ')')
			return error(syntax_error);

		if (tok[1] < REG68_A0 && tok[1] > REG68_A7)
			return error("expected (An)");

		if ((inst & 7) == 7)
			// With pflushn/pflush there's no easy way to distinguish between
			// the two in 68040 mode. Ideally the opcode bitfields would have
			// been hardcoded in 68ktab but there is aliasing between 68030
			// and 68040 opcode. So we just set the 3 lower bits to 1 in
			// pflushn inside 68ktab and detect it here.
			inst = (inst & 0xff8) | 8;

		inst |= (tok[1] & 7) | (5 << 8);

		if (tok[3] != EOL)
			return error(extra_stuff);

		D_word(inst);
	}
	else
		return error(unsupport);

	return OK;
}


//
// pflushan (68040, 68060)
//
int m_pflushan(WORD inst, WORD siz)
{
	if (activecpu == CPU_68040 || activecpu == CPU_68060)
		D_word(inst);

	return OK;
}


//
// pflushr (68851)
//
int m_pflushr(WORD inst, WORD siz)
{
	CHECKNO20;

	WORD flg = inst;					// Save flag bits
	inst &= ~0x3F;						// Clobber flag bits in instr

	// Install "standard" instr size bits
	if (flg & 4)
		inst |= siz_6[siz];

	if (flg & 16)
	{
		// OR-in register number
		if (flg & 8)
			inst |= reg_9[a1reg];		// ea1reg in bits 9..11
		else
			inst |= reg_9[a0reg];		// ea0reg in bits 9..11
	}

	if (flg & 1)
	{
		// Use am1
		inst |= am1 | a1reg;			// Get ea1 into instr
		D_word(inst);					// Deposit instr

		// Generate ea0 if requested
		if (flg & 2)
			ea0gen(siz);

		ea1gen(siz);					// Generate ea1
	}
	else
	{
		// Use am0
		inst |= am0 | a0reg;			// Get ea0 into instr
		D_word(inst);					// Deposit instr
		ea0gen(siz);					// Generate ea0

		// Generate ea1 if requested
		if (flg & 2)
			ea1gen(siz);
	}

	D_word(0b1010000000000000);
	return OK;
}


//
// ploadr, ploadw (68030)
//
int m_pload(WORD inst, WORD siz, WORD extension)
{
	// TODO: 68851 support is not added yet.
	// None of the ST series of computers had a 68020 + 68851 socket and since
	// this is an Atari targetted assembler...
	CHECKNO30;

	inst |= am1;
	D_word(inst);

	switch (am0)
	{
	case CREG:
		if (a0reg == REG68_SFC - REG68_SFC)
			inst = 0;
		else if (a0reg == REG68_DFC - REG68_SFC)
			inst = 1;
		else
			return error("illegal control register specified");
		break;
	case DREG:
		inst = (1 << 3) | a0reg;
		break;
	case IMMED:
		if ((a0exattr & DEFINED) == 0)
			return error("constant value must be defined");

		if (a0exval>7)
		return error("constant value must be between 0 and 7");

		inst = (2 << 3) | (uint16_t)a0exval;
		break;
	}

	inst |= extension | (1 << 13);
	D_word(inst);

	ea1gen(siz);

	return OK;
}


int m_ploadr(WORD inst, WORD siz)
{
	return m_pload(inst, siz, 1 << 9);
}


int m_ploadw(WORD inst, WORD siz)
{
	return m_pload(inst, siz, 0 << 9);
}


//
// pmove (68030/68851)
//
int m_pmove(WORD inst, WORD siz)
{
	int inst2,reg;

	// TODO: 68851 support is not added yet. None of the ST series of
	// computers had a 68020 + 68851 socket and since this is an Atari
	// targetted assembler.... (same for 68EC030)
	CHECKNO30;

	inst2 = inst & (1 << 8);	// Copy the flush bit over to inst2 in case we're called from m_pmovefd
	inst &= ~(1 << 8);			// And mask it out

	if (am0 == CREG)
	{
		reg = a0reg;
		inst2 |= (1 << 9);
	}
	else if (am1 == CREG)
	{
		reg = a1reg;
		inst2 |= 0;
	}
	else
		return error("pmove sez: Wut?");

	// The instruction is a quad-word (8 byte) operation
	// for the CPU root pointer and the supervisor root pointer.
	// It is a long-word operation for the translation control register
	// and the transparent translation registers(TT0 and TT1).
	// It is a word operation for the MMU status register.

	if (((reg == (REG68_URP - REG68_SFC)) || (reg == (REG68_SRP - REG68_SFC)))
		&& ((siz != SIZD) && (siz != SIZN)))
		return error(siz_error);

	if (((reg == (REG68_TC - REG68_SFC)) || (reg == (REG68_TT0 - REG68_SFC)) || (reg == (REG68_TT1 - REG68_SFC)))
		&& ((siz != SIZL) && (siz != SIZN)))
		return error(siz_error);

	if ((reg == (REG68_MMUSR - REG68_SFC)) && ((siz != SIZW) && (siz != SIZN)))
		return error(siz_error);

	if (am0 == CREG)
	{
		inst |= am1 | a1reg;
		D_word(inst);
	}
	else if (am1 == CREG)
	{
		inst |= am0 | a0reg;
		D_word(inst);
	}

	switch (reg + REG68_SFC)
	{
	case REG68_TC:
		inst2 |= (0 << 10) + (1 << 14); break;
	case REG68_SRP:
		inst2 |= (2 << 10) + (1 << 14); break;
	case REG68_CRP:
		inst2 |= (3 << 10) + (1 << 14); break;
	case REG68_TT0:
		inst2 |= (2 << 10) + (0 << 13); break;
	case REG68_TT1:
		inst2 |= (3 << 10) + (0 << 13); break;
	case REG68_MMUSR:
		if (am0 == CREG)
			inst2 |= (1 << 9) + (3 << 13);
		else
			inst2 |= (0 << 9) + (3 << 13);
		break;
	default:
		return error("unsupported register");
		break;
	}

	D_word(inst2);

	if (am0 == CREG)
		ea1gen(siz);
	else if (am1 == CREG)
		ea0gen(siz);

	return OK;
}


//
// pmovefd (68030)
//
int m_pmovefd(WORD inst, WORD siz)
{
	CHECKNO30;

	return m_pmove(inst | (1 << 8), siz);
}


//
// ptrapcc (68851)
//
int m_ptrapcc(WORD inst, WORD siz)
{
	CHECKNO20;
	// We stash the 5 condition bits inside the opcode in 68ktab (bits 0-4),
	// so we need to extract them first and fill in the clobbered bits.
	WORD opcode = inst & 0x1F;
	inst = (inst & 0xFFE0) | (0x18);

	if (siz == SIZW)
	{
		inst |= 2;
		D_word(inst);
		D_word(opcode);
		D_word(a0exval);
	}
	else if (siz == SIZL)
	{
		inst |= 3;
		D_word(inst);
		D_word(opcode);
		D_long(a0exval);
	}
	else if (siz == SIZN)
	{
		inst |= 4;
		D_word(inst);
		D_word(opcode);
	}

	return OK;
}


//
// ptestr, ptestw (68030, 68040)
// TODO See comment on m_pmove about 68851 support
// TODO quite a good chunk of the 030 code is copied from m_pload, perhaps merge these somehow?
//
int m_ptest(WORD inst, WORD siz, WORD extension)
{
	uint64_t eval;

	if (activecpu != CPU_68030 && activecpu != CPU_68040)
		return error(unsupport);

	if (activecpu == CPU_68030)
	{
		inst |= am1;
		D_word(inst);

		switch (am0)
		{
		case CREG:
			if (a0reg == REG68_SFC - REG68_SFC)
				extension |= 0;
			else if (a0reg == REG68_DFC - REG68_SFC)
				extension |= 1;
			else
				return error("illegal control register specified");
			break;
		case DREG:
			extension |= (1 << 3) | a0reg;
			break;
		case IMMED:
			if ((a0exattr & DEFINED) == 0)
				return error("constant value must be defined");

			if (a0exval > 7)
				return error("constant value must be between 0 and 7");

			extension |= (2 << 3) | (uint16_t)a0exval;
			break;
		}

		// Operand 3 must be an immediate
		CHECK_COMMA

		if (*tok++ != '#')
			return error("ptest level must be immediate");

		// Let's be a bit inflexible here and demand that this
		// is fully defined at this stage. Otherwise we'd have
		// to arrange for a bitfield fixup, which would mean
		// polluting the bitfields and codebase with special
		// cases that might most likely never be used.
		// So if anyone gets bit by this: sorry for being a butt!
		if (abs_expr(&eval) != OK)
			return OK;      // We're returning OK because error() has already been called and error count has been increased

		if (eval > 7)
			return error("ptest level must be between 0 and 7");

		extension |= eval << 10;

		// Operand 4 is optional and must be an address register

		if (*tok != EOL)
		{
			CHECK_COMMA

			if ((*tok >= REG68_A0) && (*tok <= REG68_A7))
			{
				extension |= (1 << 8) | ((*tok++ & 7) << 4);
			}
			else
			{
				return error("fourth parameter must be an address register");
			}
		}

		ErrorIfNotAtEOL();

		D_word(extension);
		return OK;
	}
	else
		return error("Not implemented yet.");

	return ERROR;
}

int m_ptestr(WORD inst, WORD siz)
{
	return m_ptest(inst, siz, (1 << 15) | (0 << 9));
}

int m_ptestw(WORD inst, WORD siz)
{
	return m_ptest(inst, siz, (1 << 15) | (1 << 9));
}

//////////////////////////////////////////////////////////////////////////////
//
// 68020/30/40/60 instructions
// Note: the map of which instructions are allowed on which CPUs came from the
// 68060 manual, section D-1 (page 392 of the PDF). The current implementation
// is missing checks for the EC models which have a simplified FPU.
//
//////////////////////////////////////////////////////////////////////////////


#define FPU_NOWARN 0
#define FPU_FPSP   1


//
// Generate a FPU opcode
//
static inline int gen_fpu(WORD inst, WORD siz, WORD opmode, WORD emul)
{
	if (am0 < AM_NONE)	// Check first operand for ea or fp - is this right?
	{
		inst |= (1 << 9);	// Bolt on FPU id
		inst |= am0;

		//if (am0 == DREG || am0 == AREG)
			inst |= a0reg;

		D_word(inst);
		inst = 1 << 14; // R/M field (we have ea so have to set this to 1)

		switch (siz)
		{
		case SIZB:	inst |= (6 << 10); break;
		case SIZW:	inst |= (4 << 10); break;
		case SIZL:	inst |= (0 << 10); break;
		case SIZN:
		case SIZS:	inst |= (1 << 10); break;
		case SIZD:	inst |= (5 << 10); break;
		case SIZX:	inst |= (2 << 10); break;
		case SIZP:
			inst |= (3 << 10);

			if (emul)
				warn("This encoding will cause an unimplemented data type exception in the MC68040 to allow emulation in software.");

			break;
		default:
			return error("Something bad happened, possibly, in gen_fpu.");
			break;
		}

		inst |= (a1reg << 7);
		inst |= opmode;
		D_word(inst);
		ea0gen(siz);
	}
	else
	{
		inst |= (1 << 9);	// Bolt on FPU id
		D_word(inst);
		inst = 0;
		inst = a0reg << 10;
		inst |= (a1reg << 7);
		inst |= opmode;
		D_word(inst);
	}

	if ((emul & FPU_FPSP) && (activefpu == (FPU_68040 | FPU_68060)))
		warn("Instruction is emulated in 68040/060");

	return OK;
}


//
// fabs (6888X, 68040FPSP, 68060FPSP)
//
int m_fabs(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00011000, FPU_NOWARN);
}


//
// fsabs (68040, 68060)
//
int m_fsabs(WORD inst, WORD siz)
{
	CHECKNO40;
	if (activefpu == FPU_68040)
		return gen_fpu(inst, siz, 0b01011000, FPU_NOWARN);

	return error("Unsupported in current FPU");
}


//
// fdabs (68040, 68060)
//
int m_fdabs(WORD inst, WORD siz)
{
	if (activefpu == FPU_68040)
		return gen_fpu(inst, siz, 0b01011100, FPU_NOWARN);

	return error("Unsupported in current FPU");
}


//
// facos (6888X, 68040FPSP, 68060FPSP)
//
int m_facos(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00011100, FPU_FPSP);
}


//
// fadd (6888X, 68040, 68060)
//
int m_fadd(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00100010, FPU_NOWARN);
}


//
// fsadd (68040, 68060)
//
int m_fsadd(WORD inst, WORD siz)
{
	if (activefpu & (FPU_68040 | FPU_68060))
		return gen_fpu(inst, siz, 0b01100010, FPU_NOWARN);

	return error("Unsupported in current FPU");
}


//
// fxadd (68040)
//
int m_fdadd(WORD inst, WORD siz)
{
	if (activefpu & (FPU_68040 | FPU_68060))
		return gen_fpu(inst, siz, 0b01100110, FPU_NOWARN);

	return error("Unsupported in current FPU");
}


//
// fasin (6888X, 68040FPSP, 68060FPSP)
//
int m_fasin(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00001100, FPU_FPSP);
}


//
// fatan (6888X, 68040FPSP, 68060FPSP)
//
int m_fatan(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00001010, FPU_FPSP);
}


//
// fatanh (6888X, 68040FPSP, 68060FPSP)
//
int m_fatanh(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00001101, FPU_FPSP);
}


//
// fcmp (6888X, 68040, 68060)
//
int m_fcmp(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00111000, FPU_FPSP);
}


//
// fcos (6888X, 68040FPSP, 68060FPSP)
//
int m_fcos(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00011101, FPU_FPSP);
}


//
// fcosh (6888X, 68040FPSP, 68060FPSP)
//
int m_fcosh(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00011001, FPU_FPSP);
}


//
// fdbcc (6888X, 68040, 68060FPSP)
//
int m_fdbcc(WORD inst, WORD siz)
{
	CHECKNOFPU;
	WORD opcode = inst & 0x3F;	// Grab conditional bitfield

	inst &= ~0x3F;
	inst |= 1 << 3;

	siz = siz;
	inst |= a0reg;
	D_word(inst);
	D_word(opcode);

	if (a1exattr & DEFINED)
	{
		if ((a1exattr & TDB) != cursect)
			return error(rel_error);

		uint32_t v = (uint32_t)a1exval - sloc;

		if ((v + 0x8000) > 0x10000)
			return error(range_error);

		D_word(v);
	}
	else
	{
		AddFixup(FU_WORD | FU_PCREL | FU_ISBRA, sloc, a1expr);
		D_word(0);
	}

	if (activefpu == FPU_68060)
		warn("Instruction is emulated in 68060");

	return OK;
}


//
// fdiv (6888X, 68040, 68060)
//
int m_fdiv(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00100000, FPU_NOWARN);
}


//
// fsdiv (68040, 68060)
//
int m_fsdiv(WORD inst, WORD siz)
{
	if (activefpu & (FPU_68040 | FPU_68060))
		return gen_fpu(inst, siz, 0b01100000, FPU_NOWARN);

	return error("Unsupported in current FPU");
}


//
// fddiv (68040, 68060)
//
int m_fddiv(WORD inst, WORD siz)
{
	if (activefpu & (FPU_68040 | FPU_68060))
		return gen_fpu(inst, siz, 0b01100100, FPU_NOWARN);

	return error("Unsupported in current FPU");
}


//
// fetox (6888X, 68040FPSP, 68060FPSP)
//
int m_fetox(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00010000, FPU_FPSP);
}


//
// fetoxm1 (6888X, 68040FPSP, 68060FPSP)
//
int m_fetoxm1(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00001000, FPU_FPSP);
}


//
// fgetexp (6888X, 68040FPSP, 68060FPSP)
//
int m_fgetexp(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00011110, FPU_FPSP);
}


//
// fgetman (6888X, 68040FPSP, 68060FPSP)
//
int m_fgetman(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00011111, FPU_FPSP);
}


//
// fint (6888X, 68040FPSP, 68060)
//
int m_fint(WORD inst, WORD siz)
{
	if (am1 == AM_NONE)
		// special case - fint fpx = fint fpx,fpx
		a1reg = a0reg;

	if (activefpu == FPU_68040)
		warn("Instruction is emulated in 68040");

	return gen_fpu(inst, siz, 0b00000001, FPU_NOWARN);
}


//
// fintrz (6888X, 68040FPSP, 68060)
//
int m_fintrz(WORD inst, WORD siz)
{
	if (am1 == AM_NONE)
		// special case - fintrz fpx = fintrz fpx,fpx
		a1reg = a0reg;

	if (activefpu == FPU_68040)
		warn("Instruction is emulated in 68040");

	return gen_fpu(inst, siz, 0b00000011, FPU_NOWARN);
}


//
// flog10 (6888X, 68040FPSP, 68060FPSP)
//
int m_flog10(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00010101, FPU_FPSP);
}


//
// flog2 (6888X, 68040FPSP, 68060FPSP)
//
int m_flog2(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00010110, FPU_FPSP);
}


//
// flogn (6888X, 68040FPSP, 68060FPSP)
//
int m_flogn(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00010100, FPU_FPSP);
}


//
// flognp1 (6888X, 68040FPSP, 68060FPSP)
//
int m_flognp1(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00000110, FPU_FPSP);
}


//
// fmod (6888X, 68040FPSP, 68060FPSP)
//
int m_fmod(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00100001, FPU_FPSP);
}


//
// fmove (6888X, 68040, 68060)
//
int m_fmove(WORD inst, WORD siz)
{
	CHECKNOFPU;

	// EA to register
	if ((am0 == FREG) && (am1 < AM_USP))
	{
		// fpx->ea
		// EA
		inst |= am1 | a1reg;
		D_word(inst);

		// R/M
		inst = 3 << 13;

		// Source specifier
		switch (siz)
		{
		case SIZB:	inst |= (6 << 10); break;
		case SIZW:	inst |= (4 << 10); break;
		case SIZL:	inst |= (0 << 10); break;
		case SIZN:
		case SIZS:	inst |= (1 << 10); break;
		case SIZD:	inst |= (5 << 10); break;
		case SIZX:	inst |= (2 << 10); break;
		case SIZP:	inst |= (3 << 10);
			// In P size we have 2 cases: {#k} where k is immediate
			// and {Dn} where Dn=Data register
			if (bfparam1)
			{
				// Dn
				inst |= 1 << 12;
				inst |= bfval1 << 4;
			}
			else
			{
				// #k
				if (bfval1 > 63 && bfval1 < -64)
					return error("K-factor must be between -64 and 63");

				inst |= bfval1 & 127;
			}

			break;
		default:
			return error("Something bad happened, possibly.");
			break;
		}

		// Destination specifier
		inst |= (a0reg << 7);

		// Opmode
		inst |= 0;

		D_word(inst);
		ea1gen(siz);
	}
	else if ((am0 < AM_USP) && (am1 == FREG))
	{
		// ea->fpx

		// EA
		inst |= am0 | a0reg;
		D_word(inst);

		// R/M
		inst = 1 << 14;

		// Source specifier
		switch (siz)
		{
		case SIZB:	inst |= (6 << 10); break;
		case SIZW:	inst |= (4 << 10); break;
		case SIZL:	inst |= (0 << 10); break;
		case SIZN:
		case SIZS:	inst |= (1 << 10); break;
		case SIZD:	inst |= (5 << 10); break;
		case SIZX:	inst |= (2 << 10); break;
		case SIZP:	inst |= (3 << 10); break;
		default:
			return error("Something bad happened, possibly.");
			break;
		}

		// Destination specifier
		inst |= (a1reg << 7);

		// Opmode
		inst |= 0;

		D_word(inst);
		ea0gen(siz);
	}
	else if ((am0 == FREG) && (am1 == FREG))
	{
		// register-to-register
		// Essentially ea to register with R/0=0

		// EA
		D_word(inst);

		// R/M
		inst = 0 << 14;

		// Source specifier
		if (siz != SIZX && siz != SIZN)
			return error("Invalid size");

		// Source register
		inst |= (a0reg << 10);

		// Destination register
		inst |= (a1reg << 7);

		D_word(inst);
	}

	return OK;
}


//
// fmove (6888X, 68040, 68060)
//
int m_fmovescr(WORD inst, WORD siz)
{
	CHECKNOFPU;

	// Move Floating-Point System Control Register (FPCR)
	// ea
	// dr
	// Register select
	if ((am0 == FPSCR) && (am1 < AM_USP))
	{
		inst |= am1 | a1reg;
		D_word(inst);
		inst = (1 << 13) + (1 << 15);
		inst |= a0reg;
		D_word(inst);
		ea1gen(siz);
		return OK;
	}
	else if ((am1 == FPSCR) && (am0 < AM_USP))
	{
		inst |= am0 | a0reg;
		D_word(inst);
		inst = (0 << 13) + (1 << 15);
		inst |= a1reg;
		D_word(inst);
		ea0gen(siz);
		return OK;
	}

	return error("m_fmovescr says: wut?");
}

//
// fsmove/fdmove (68040, 68060)
//
int m_fsmove(WORD inst, WORD siz)
{
	if (!(activefpu & (FPU_68040 | FPU_68060)))
		return error("Unsupported in current FPU");

	return gen_fpu(inst, siz, 0b01100100, FPU_FPSP);
}


int m_fdmove(WORD inst, WORD siz)
{
	if (!(activefpu & (FPU_68040 | FPU_68060)))
		return error("Unsupported in current FPU");

	return gen_fpu(inst, siz, 0b01100100, FPU_FPSP);
}


//
// fmovecr (6888X, 68040FPSP, 68060FPSP)
//
int m_fmovecr(WORD inst, WORD siz)
{
	CHECKNOFPU;

	D_word(inst);
	inst = 0x5c00;
	inst |= a1reg << 7;
	inst |= a0exval;
	D_word(inst);

	if (activefpu == FPU_68040)
		warn("Instruction is emulated in 68040/060");

	return OK;
}


//
// fmovem (6888X, 68040, 68060FPSP)
//
int m_fmovem(WORD inst, WORD siz)
{
	CHECKNOFPU;

	WORD regmask;
	WORD datareg;

	if (siz == SIZX || siz == SIZN)
	{
		if ((*tok >= REG68_FP0) && (*tok <= REG68_FP7))
		{
			// fmovem.x <rlist>,ea
			if (fpu_reglist_left(&regmask) < 0)
				return OK;

			if (*tok++ != ',')
				return error("missing comma");

			if (amode(0) < 0)
				return OK;

			inst |= am0 | a0reg;

			if (!(amsktab[am0] & (C_ALTCTRL | M_APREDEC)))
				return error("invalid addressing mode");

			D_word(inst);
			inst = (1 << 15) | (1 << 14) | (1 << 13) | (0 << 11) | regmask;
			D_word(inst);
			ea0gen(siz);
			return OK;
		}
		else if ((*tok >= REG68_D0) && (*tok <= REG68_D7))
		{
			// fmovem.x Dn,ea
			datareg = (*tok++ & 7) << 10;

			if (*tok++ != ',')
				return error("missing comma");

			if (amode(0) < 0)
				return OK;

			inst |= am0 | a0reg;

			if (!(amsktab[am0] & (C_ALTCTRL | M_APREDEC)))
				return error("invalid addressing mode");

			// Quote from the 060 manual:
			// "[..] when the processor attempts an FMOVEM.X instruction using a dynamic register list."
			if (activefpu == FPU_68060)
				warn("Instruction is emulated in 68060");

			D_word(inst);
			inst = (1 << 15) | (1 << 14) | (1 << 13) | (1 << 11) | (datareg << 4);
			D_word(inst);
			ea0gen(siz);
			return OK;
		}
		else
		{
			// fmovem.x ea,...
			if (amode(0) < 0)
				return OK;

			inst |= am0 | a0reg;

			if (*tok++ != ',')
				return error("missing comma");

			if ((*tok >= REG68_FP0) && (*tok <= REG68_FP7))
			{
				// fmovem.x ea,<rlist>
				if (fpu_reglist_right(&regmask) < 0)
					return OK;

				D_word(inst);
				inst = (1 << 15) | (1 << 14) | (0 << 13) | (2 << 11) | regmask;
				D_word(inst);
				ea0gen(siz);
				return OK;
			}
			else
			{
				// fmovem.x ea,Dn
				datareg = (*tok++ & 7) << 10;

				// Quote from the 060 manual:
				// "[..] when the processor attempts an FMOVEM.X instruction using a dynamic register list."
				if (activefpu == FPU_68060)
					warn("Instruction is emulated in 68060");

				D_word(inst);
				inst = (1 << 15) | (1 << 14) | (0 << 13) | (3 << 11) | (datareg << 4);
				D_word(inst);
				ea0gen(siz);
				return OK;
			}
		}
	}
	else if (siz == SIZL)
	{
		if ((*tok == REG68_FPCR) || (*tok == REG68_FPSR) || (*tok == REG68_FPIAR))
		{
			// fmovem.l <rlist>,ea
			regmask = (1 << 15) | (1 << 13);
			int no_control_regs = 0;

fmovem_loop_1:
			if (*tok == REG68_FPCR)
			{
				regmask |= (1 << 12);
				tok++;
				no_control_regs++;
				goto fmovem_loop_1;
			}

			if (*tok == REG68_FPSR)
			{
				regmask |= (1 << 11);
				tok++;
				no_control_regs++;
				goto fmovem_loop_1;
			}

			if (*tok == REG68_FPIAR)
			{
				regmask |= (1 << 10);
				tok++;
				no_control_regs++;
				goto fmovem_loop_1;
			}

			if ((*tok == '/') || (*tok == '-'))
			{
				tok++;
				goto fmovem_loop_1;
			}

			if (*tok++ != ',')
				return error("missing comma");

			if (amode(0) < 0)
				return OK;

			// Quote from the 060 manual:
			// "[..] when the processor attempts to execute an FMOVEM.L instruction with
			// an immediate addressing mode to more than one floating - point
			// control register (FPCR, FPSR, FPIAR)[..]"
			if (activefpu == FPU_68060)
				if (no_control_regs > 1 && am0 == IMMED)
					warn("Instruction is emulated in 68060");

			inst |= am0 | a0reg;
			D_word(inst);
			D_word(regmask);
			ea0gen(siz);
		}
		else
		{
			// fmovem.l ea,<rlist>
			if (amode(0) < 0)
				return OK;

			inst |= am0 | a0reg;

			if (*tok++ != ',')
				return error("missing comma");

			regmask = (1 << 15) | (0 << 13);

fmovem_loop_2:
			if (*tok == REG68_FPCR)
			{
				regmask |= (1 << 12);
				tok++;
				goto fmovem_loop_2;
			}

			if (*tok == REG68_FPSR)
			{
				regmask |= (1 << 11);
				tok++;
				goto fmovem_loop_2;
			}

			if (*tok == REG68_FPIAR)
			{
				regmask |= (1 << 10);
				tok++;
				goto fmovem_loop_2;
			}

			if ((*tok == '/') || (*tok == '-'))
			{
				tok++;
				goto fmovem_loop_2;
			}

			if (*tok != EOL)
				return error("extra (unexpected) text found");

			inst |= am0 | a0reg;
			D_word(inst);
			D_word(regmask);
			ea0gen(siz);
		}
	}
	else
		return error("bad size suffix");

	return OK;
}


//
// fmul (6888X, 68040, 68060)
//
int m_fmul(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00100011, FPU_NOWARN);
}


//
// fsmul (68040, 68060)
//
int m_fsmul(WORD inst, WORD siz)
{
	if (activefpu & (FPU_68040 | FPU_68060))
		return gen_fpu(inst, siz, 0b01100011, FPU_NOWARN);

	return error("Unsupported in current FPU");
}


//
// fdmul (68040)
//
int m_fdmul(WORD inst, WORD siz)
{
	if (activefpu & (FPU_68040 | FPU_68060))
		return gen_fpu(inst, siz, 0b01100111, FPU_NOWARN);

	return error("Unsupported in current FPU");
}


//
// fneg (6888X, 68040, 68060)
//
int m_fneg(WORD inst, WORD siz)
{
	CHECKNOFPU;

	if (am1 == AM_NONE)
	{
		a1reg = a0reg;
		return gen_fpu(inst, siz, 0b00011010, FPU_NOWARN);
	}

	return gen_fpu(inst, siz, 0b00011010, FPU_NOWARN);
}


//
// fsneg (68040, 68060)
//
int m_fsneg(WORD inst, WORD siz)
{
	if (activefpu & (FPU_68040 | FPU_68060))
	{
		if (am1 == AM_NONE)
		{
			a1reg = a0reg;
			return gen_fpu(inst, siz, 0b01011010, FPU_NOWARN);
		}

		return gen_fpu(inst, siz, 0b01011010, FPU_NOWARN);
	}

	return error("Unsupported in current FPU");
}


//
// fdneg (68040, 68060)
//
int m_fdneg(WORD inst, WORD siz)
{
	if (activefpu & (FPU_68040 | FPU_68060))
	{
		if (am1 == AM_NONE)
		{
				a1reg = a0reg;
				return gen_fpu(inst, siz, 0b01011110, FPU_NOWARN);
		}

		return gen_fpu(inst, siz, 0b01011110, FPU_NOWARN);
	}

	return error("Unsupported in current FPU");
}


//
// fnop (6888X, 68040, 68060)
//
int m_fnop(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00000000, FPU_NOWARN);
}


//
// frem (6888X, 68040FPSP, 68060FPSP)
//
int m_frem(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00100101, FPU_FPSP);
}


//
// fscale (6888X, 68040FPSP, 68060FPSP)
//
int m_fscale(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00100110, FPU_FPSP);
}


//
// FScc (6888X, 68040, 68060), cpScc (68851, 68030), PScc (68851)
// TODO: Add check for PScc to ensure 68020+68851 active
// TODO: Add check for cpScc to ensure 68020+68851, 68030
//
int m_fscc(WORD inst, WORD siz)
{
	CHECKNOFPU;

	// We stash the 5 condition bits inside the opcode in 68ktab (bits 4-0),
	// so we need to extract them first and fill in the clobbered bits.
	WORD opcode = inst & 0x1F;
	inst &= 0xFFE0;
	inst |= am0 | a0reg;
	D_word(inst);
	ea0gen(siz);
	D_word(opcode);
	if (activefpu == FPU_68060)
		warn("Instruction is emulated in 68060");
	return OK;
}


//
// fsgldiv (6888X, 68040FPSP, 68060FPSP)
//
int m_fsgldiv(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00100100, FPU_FPSP);
}


//
// fsglmul (6888X, 68040, 68060FPSP)
//
int m_fsglmul(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00100111, FPU_FPSP);
}


//
// fsin (6888X, 68040FPSP, 68060FPSP)
//
int m_fsin(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00001110, FPU_FPSP);
}


//
// fsincos (6888X, 68040FPSP, 68060FPSP)
//
int m_fsincos(WORD inst, WORD siz)
{
	CHECKNOFPU;

	// Swap a1reg, a2reg as a2reg should be stored in the bitfield gen_fpu
	// generates
	int temp;
	temp = a2reg;
	a2reg = a1reg;
	a1reg = temp;

	if (gen_fpu(inst, siz, 0b00110000, FPU_FPSP) == OK)
	{
		chptr[-1] |= a2reg;
		return OK;
	}

	return ERROR;
}


//
// fsinh (6888X, 68040FPSP, 68060FPSP)
//
int m_fsinh(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00000010, FPU_FPSP);
}


//
// fsqrt (6888X, 68040, 68060)
//
int m_fsqrt(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00000100, FPU_NOWARN);
}


//
// fsfsqrt (68040, 68060)
//
int m_fsfsqrt(WORD inst, WORD siz)
{
	if (activefpu & (FPU_68040 | FPU_68060))
		return gen_fpu(inst, siz, 0b01000001, FPU_NOWARN);

	return error("Unsupported in current FPU");
}


//
// fdfsqrt (68040, 68060)
//
int m_fdfsqrt(WORD inst, WORD siz)
{
	if (activefpu & (FPU_68040 | FPU_68060))
		return gen_fpu(inst, siz, 0b01000101, FPU_NOWARN);

	return error("Unsupported in current FPU");
}


//
// fsub (6888X, 68040, 68060)
//
int m_fsub(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00101000, FPU_NOWARN);
}


//
// fsfsub (68040, 68060)
//
int m_fsfsub(WORD inst, WORD siz)
{
	if (activefpu & (FPU_68040 | FPU_68060))
		return gen_fpu(inst, siz, 0b01101000, FPU_NOWARN);

	return error("Unsupported in current FPU");
}


//
// fdfsub (68040, 68060)
//
int m_fdsub(WORD inst, WORD siz)
{
	if (activefpu & (FPU_68040 | FPU_68060))
		return gen_fpu(inst, siz, 0b01101100, FPU_NOWARN);

	return error("Unsupported in current FPU");
}


//
// ftan (6888X, 68040FPSP, 68060FPSP)
//
int m_ftan(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00001111, FPU_FPSP);
}


//
// ftanh (6888X, 68040FPSP, 68060FPSP)
//
int m_ftanh(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00001001, FPU_FPSP);
}


//
// ftentox (6888X, 68040FPSP, 68060FPSP)
//
int m_ftentox(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00010010, FPU_FPSP);
}


//
// FTRAPcc (6888X, 68040, 68060FPSP)
//
int m_ftrapcc(WORD inst, WORD siz)
{
	CHECKNOFPU;

	// We stash the 5 condition bits inside the opcode in 68ktab (bits 3-7),
	// so we need to extract them first and fill in the clobbered bits.
	WORD opcode = (inst >> 3) & 0x1F;
	inst = (inst & 0xFF07) | (0xF << 3);

	if (siz == SIZW)
	{
		inst |= 2;
		D_word(inst);
		D_word(opcode);
		D_word(a0exval);
	}
	else if (siz == SIZL)
	{
		inst |= 3;
		D_word(inst);
		D_word(opcode);
		D_long(a0exval);
	}
	else if (siz == SIZN)
	{
		inst |= 4;
		D_word(inst);
		D_word(opcode);
		return OK;
	}

	if (activefpu == FPU_68060)
		warn("Instruction is emulated in 68060");

	return OK;
}


//
// ftst (6888X, 68040, 68060)
//
int m_ftst(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00111010, FPU_NOWARN);
}


//
// ftwotox (6888X, 68040FPSP, 68060FPSP)
//
int m_ftwotox(WORD inst, WORD siz)
{
	CHECKNOFPU;
	return gen_fpu(inst, siz, 0b00010001, FPU_FPSP);
}


/////////////////////////////////
//                             //
// 68060 specific instructions //
//                             //
/////////////////////////////////


//
// lpstop (68060)
//
int m_lpstop(WORD inst, WORD siz)
{
	CHECKNO60;
	D_word(0b0000000111000000);

	if (a0exattr & DEFINED)
	{
		D_word(a0exval);
	}
	else
	{
		AddFixup(FU_WORD, sloc, a0expr);
		D_word(0);
	}

	return OK;
}


//
// plpa (68060)
//
int m_plpa(WORD inst, WORD siz)
{
	CHECKNO60;
	inst |= a0reg;		// Install register
	D_word(inst);

	return OK;
}

