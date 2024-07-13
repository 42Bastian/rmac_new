//
// RMAC - Renamed Macro Assembler for the Atari Jaguar Console System
// DSP56L_MACH.C - Code Generation for Motorola DSP56001
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "dsp56k_mach.h"
#include "direct.h"
#include "dsp56k.h"
#include "error.h"
#include "rmac.h"
#include "sect.h"
#include "token.h"

#define DEF_REG56
#include "56kregs.h"


// Globals
unsigned int dsp_orgaddr;	// DSP 56001 ORG address
unsigned int dsp_orgseg;	// DSP 56001 ORG segment


// Fucntion prototypes
int m_unimp(WORD, WORD), m_badmode(WORD, WORD);
int dsp_ab(LONG);
int dsp_baab(LONG inst);
int dsp_acc48(LONG inst);
int dsp_self(LONG inst);
int dsp_xyab(LONG inst);
int dsp_x0y0ab(LONG inst);
int dsp_immcr(LONG inst);
int dsp_immmovec(LONG inst);
int dsp_imm12(LONG inst);
int dsp_tcc2(LONG inst);
int dsp_tcc4(LONG inst);
int dsp_ea(LONG inst);
int dsp_ea_imm5(LONG inst);
int dsp_abs12(LONG inst);
int dsp_reg_imm5(LONG inst);
int dsp_ea_abs16(LONG inst);
int dsp_reg_abs16(LONG inst);
int dsp_imm12_abs16(LONG inst);
int dsp_alu24_abs16(LONG inst);
int dsp_reg(LONG inst);
int dsp_alu24(LONG inst);
int dsp_reg_imm5_abs16(LONG inst);
int dsp_ea_imm5_abs16(LONG inst);
int dsp_ea_lua(LONG inst);
int dsp_ab_rn(LONG inst);
int dsp_movec_ea(LONG inst);
int dsp_movec_aa(LONG inst);
int dsp_movec_reg(LONG inst);
int dsp_mult(LONG inst);
int dsp_movem_ea(LONG inst);
int dsp_movem_aa(LONG inst);
int dsp_movep_ea(LONG inst);
int dsp_movep_reg(LONG inst);


// Common error messages


// Include code tables
MNTABDSP dsp56k_machtab[] = {
   { 0xFFFFFFFF, 0xFFFFFFFF, 0x0000, 0x0000, 0, (int (*)(LONG))m_badmode }, // 0
   #include "dsp56ktab.h"
   {  0L,  0L, 0x0000, 0x0000, 0, (int (*)(LONG))m_unimp   }                   // Last entry
};


static inline int dsp_extra_ea()
{
	if (deposit_extra_ea == DEPOSIT_EXTRA_WORD)
	{
		if (!(dspImmedEXATTR & FLOAT))
		{
			if (dspImmedEXATTR & DEFINED)
			{
				D_dsp(dspImmedEXVAL);
			}
			else
			{
				// TODO: what if it's an address and not an immediate? Does it matter at all?
				AddFixup(FU_DSPIMM24, sloc, dspImmedEXPR);
				D_dsp(0);
			}
		}
		else
		{
			if (dspImmedEXATTR & DEFINED)
			{
				D_dsp(dspImmedEXVAL);
			}
			else
			{
				// TODO: what if it's an address and not an immediate? Does it matter at all?
				AddFixup(FU_DSPIMMFL24, sloc, dspImmedEXPR);
				D_dsp(0);
			}
		}
	}
	else if (deposit_extra_ea == DEPOSIT_EXTRA_FIXUP)
	{
		// Probably superfluous check (we're not likely to land here with a
		// known aa) but oh well
		if (!(dspImmedEXATTR & DEFINED))
		{
			// Since we already deposited the word to be fixup'd we need to
			// subtract 1 from sloc
			chptr -= 3;
			AddFixup(FU_DSPADR06, sloc - 1, dspImmedEXPR);
			chptr += 3;
		}
	}

	return OK;
}


int dsp_ab(LONG inst)
{
	inst |= (dsp_a0reg & 1) << 3;
	D_dsp(inst);
	dsp_extra_ea();		// Deposit effective address if needed
	return OK;
}


int dsp_baab(LONG inst)
{
	if (dsp_a0reg == dsp_a1reg)
		return error("source and destination registers must not be the same");

	inst |= ((dsp_a0reg + 1) & 1) << 3;
	D_dsp(inst);
	dsp_extra_ea();		// Deposit effective address if needed

	return OK;
}


int dsp_acc48(LONG inst)
{
	if (dsp_a0reg == dsp_a1reg)
		return error("source and destination registers must not be the same");

	inst |= (dsp_a1reg & 1) << 3;

	switch (dsp_a0reg)
	{
	case REG56_X:  inst |= 2 << 4; break;
	case REG56_Y:  inst |= 3 << 4; break;
	case REG56_X0: inst |= 4 << 4;break;
	case REG56_Y0: inst |= 5 << 4;break;
	case REG56_X1: inst |= 6 << 4;break;
	case REG56_Y1: inst |= 7 << 4;break;
	default: return error("dsp_acc48: shouldn't reach here!");
	}

	D_dsp(inst);
	dsp_extra_ea();		// Deposit effective address if needed

	return OK;
}


int dsp_self(LONG inst)
{
	D_dsp(inst);
	dsp_extra_ea();		// Deposit effective address if needed

	return OK;
}


int dsp_xyab(LONG inst)
{
	if (dsp_a0reg == dsp_a1reg)
		return error("source and destination registers must not be the same");

	inst |= (dsp_a0reg & 1) << 4;
	inst |= (dsp_a1reg & 1) << 3;
	D_dsp(inst);
	dsp_extra_ea();		// Deposit effective address if needed

	return OK;
}


int dsp_x0y0ab(LONG inst)
{
	if (dsp_a0reg == dsp_a1reg)
		return error("source and destination registers must not be the same");

	int inverse = (dsp_a0reg & 3);
	inverse = ((inverse & 1) << 1) | ((inverse & 2) >> 1);
	inst |= inverse << 4;
	inst |= (dsp_a1reg & 1) << 3;
	D_dsp(inst);
	dsp_extra_ea();		// Deposit effective address if needed

	return OK;
}


int dsp_immcr(LONG inst)
{
	switch (dsp_a1reg)
	{
	case REG56_CCR: inst |= 1; break;
	case REG56_MR:inst |= 0; break;
	case REG56_OMR:inst |= 2; break;
	default: return error("invalid destination register (only ccr, mr, omr allowed");
	}

	if (dsp_a0exattr & DEFINED)
	{
		inst |= dsp_a0exval << 8;
		D_dsp(inst);
	}
	else
	{
		AddFixup(FU_DSPIMM8, sloc, dsp_a0expr);
		D_dsp(inst);
	}

	return OK;
}


int dsp_immmovec(LONG inst)
{
	switch (dsp_a1reg)
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:      inst |= dsp_a1reg; break; // M0-M7
	case REG56_SR:  inst |= 25; break;
	case REG56_OMR: inst |= 26; break;
	case REG56_SP:  inst |= 27; break;
	case REG56_SSH: inst |= 28; break;
	case REG56_SSL: inst |= 29; break;
	case REG56_LA:  inst |= 30; break;
	case REG56_LC:  inst |= 31; break;
	default: return error("invalid destination register (only m0-m7, SR, OMR, SP, SSH, SSL, LA, LC allowed");
	}

	if (dsp_a0exattr & DEFINED)
	{
		inst |= (dsp_a0exval & 0xff) << 8;
		D_dsp(inst);
	}
	else
	{
		AddFixup(FU_DSPIMM8, sloc, dsp_a0expr);
		D_dsp(inst);
	}

	return OK;
}


int dsp_imm12(LONG inst)
{
	if (dsp_a0exattr & DEFINED)
	{
		if ((dsp_am0 & (M_DSPIM12 | M_DSPIM8)) == 0)
			return error("immediate out of range ($000-$fff)");
		inst |= ((dsp_a0exval & 0x0ff) << 8) | ((dsp_a0exval & 0xf00) >> 8);
		D_dsp(inst);
	}
	else
	{
		AddFixup(FU_DSPIMM12, sloc, dsp_a0expr);
		D_dsp(inst);
	}

	return OK;
}


// Tcc instructions with 2 operands (48bit)
int dsp_tcc2(LONG inst)
{
	if (dsp_a0reg == dsp_a1reg)
		return error("First pair of source and destination registers must not be the same");

	int inverse;
	inverse = (dsp_a0reg & 7);
	inverse = ((inverse & 1) << 1) | ((inverse & 2) >> 1) | (inverse & 4);
	inst |= inverse << 4;
	inst |= ((dsp_a1reg) & 1) << 3;
	D_dsp(inst);

	return OK;
}


// Tcc instructions with 4 operands
int dsp_tcc4(LONG inst)
{
	if (dsp_a0reg == dsp_a1reg)
		return error("First pair of source and destination registers must not be the same");

	if (dsp_am2 != M_DSPR || dsp_am3 != M_DSPR)
		return error("Second pair of source and destination registers must be R0-R7");

	if (dsp_am0 == M_ACC56 && dsp_am1 == M_ACC56)
	{
		inst |= ((dsp_a0reg + 1) & 1) << 3;
	}
	else
	{
		int inverse;
		inverse = (dsp_a0reg & 7);
		inverse = ((inverse & 1) << 1) | ((inverse & 2) >> 1) | (inverse & 4);
		inst |= inverse << 4;
		inst |= ((dsp_a1reg) & 1) << 3;
	}

	inst |= 1 << 16;
	inst |= (dsp_a2reg << 8) | (dsp_a3reg);
	D_dsp(inst);

	return OK;
}


// Just store ea
int dsp_ea(LONG inst)
{
	inst |= dsp_a0reg << 8;

	if (dsp_a0memspace != -1)
		inst |= dsp_a0memspace;

	if (dsp_am0 == M_DSPAA)
		inst |= ((dsp_a0exval & 0x3f) << 8);

	D_dsp(inst);

	if (dsp_a0reg == DSP_EA_ABS)
	{
		if (dsp_a0exattr & DEFINED)
		{
			D_dsp(dsp_a0exval);
		}
		else
		{
			AddFixup(FU_DSPADR24, sloc, dsp_a0expr);
			D_dsp(0);
		}
	}

	return OK;
}


// Store ea and 5-bit constant
int dsp_ea_imm5(LONG inst)
{
	if (dsp_a1memspace == -1)
		return error("Only X: or Y: memory space allowed");

	if (dsp_a0exattr & DEFINED)
	{
		int v = (int)dsp_a0exval;

		if (v < 0 || v > 23)
			return error("immediate value must be between 0 and 23");

		if (dsp_a1reg == DSP_EA_ABS)
		{
			inst |= (v | dsp_a1memspace | (dsp_a1reg << 8));
		}
		else
		{
			inst |= ((dsp_a1exval & 0x3F) << 8) | v | dsp_a1memspace | (dsp_a1reg << 8);
		}

		D_dsp(inst);

		if (dsp_a1reg == DSP_EA_ABS)
		{
			if (dsp_a1exattr & DEFINED)
			{
				D_dsp(dsp_a1exval);
			}
			else
			{
				AddFixup(FU_DSPADR24, sloc, dsp_a1expr);
				D_dsp(0);
			}
		}
	}
	else
	{
		if (dsp_a1reg == DSP_EA_ABS)
		{
			inst |= dsp_a1memspace | (dsp_a1reg << 8);
		}
		else
		{
			inst |= ((dsp_a1exval & 0x3F) << 8) | dsp_a1memspace | (dsp_a1reg << 8);
		}

		AddFixup(FU_DSPIMM5, sloc, dsp_a0expr);
		D_dsp(inst);

		if (dsp_a1reg == DSP_EA_ABS)
		{
			if (dsp_a1exattr & DEFINED)
			{
				D_dsp(dsp_a1exval);
			}
			else
			{
				AddFixup(FU_DSPADR24, sloc, dsp_a1expr);
				D_dsp(0);
			}
		}
	}

	return OK;
}


// Processes the input register according to table A-18 of the Motorola DSP
// manual and returns the correct encoding.
// Note: returns only the 3 lower bits of the table. The rest is handled in
//       dsp56ktab.
static inline LONG tab_A18(int *am, int *reg)
{
	switch (*am)
	{
	case M_ALU24:
		return (*reg & 7);
	case M_DSPM:
	case M_DSPN:
	case M_DSPR:
		return *reg;
		break;
	case M_ACC56:
	case M_ACC24:
	case M_ACC8:
		if (*reg == REG56_A1)
			return 4;
		else
			return (*reg & 7);

		break;
	case M_DSPPCU:
		switch (*reg)
		{
		case REG56_SR:  return 1; break;
		case REG56_OMR: return 2; break;
		case REG56_SP:  return 3; break;
		case REG56_SSH: return 4; break;
		case REG56_SSL: return 5; break;
		case REG56_LA:  return 6; break;
		case REG56_LC:  return 7; break;
		default:
			return error("specified control register not allowed as destination");
			break;
		}

		break;
	default:
		return error("reached at the end of tab_A18 - shouldn't happen!");
	}
}


// Store register (table A-18 in the motorola manual) and 5-bit constant
int dsp_reg_imm5(LONG inst)
{
	LONG reg;

	if ((reg = tab_A18(&dsp_am1, &dsp_a1reg)) == ERROR)
		return ERROR;

	inst |= (reg << 8);

	if (dsp_a0exattr & DEFINED)
	{
		int v = (int)dsp_a0exval;

		if (v < 0 || v > 23)
			return error("immediate value must be between 0 and 23");

		inst |= v;
		D_dsp(inst);
	}
	else
	{
		AddFixup(FU_DSPIMM5, sloc, dsp_a0expr);
		D_dsp(inst);
	}

	return OK;
}


// Store 12-bit address
int dsp_abs12(LONG inst)
{
	if (dsp_a0exattr & DEFINED)
	{
		int v = (int)dsp_a0exval;

		if (v < 0 || v > 0xFFF)
			return error("immediate out of range ($000-$FFF)");

		inst |= v;
		D_dsp(inst);
	}
	else
	{
		AddFixup(FU_DSPADR12, sloc, dsp_a0expr);
		D_dsp(inst);
	}

	return OK;
}


// Manipulate expr to append a '-1'. Used specifically for DO.
void append_minus_1(TOKEN * expr)
{
	// Find where the end of expression is
	while (*expr != ENDEXPR)
	{
		if (*expr == SYMBOL || *expr == CONST || *expr == FCONST)
			expr++;
		else if (*expr == ACONST)
			expr += 3;

		expr++;
	}

	// Overwrite ENDEXPR and append '-1'
	*expr++ = CONST;
	uint64_t *expr64 = (uint64_t *)expr;
	*expr64++ = 1;
	expr = (uint32_t *)expr64;
	*expr++ = '-';
	*expr = ENDEXPR;
}


// Store a 12bit immediate and 16bit address.
// Note: This function is specifically handling DO. DO has a requirement of
//       storing the address of a label minus 1! Quoting the manual:
//       "Note: The assembler calculates the end-of-loop address to be loaded
//        into LA (the absolute address extension word) by evaluating the end
//        -of-loop expression <expr> and subtracting one. This is done to
//        accommodate the case where the last word in the DO loop is a two-word
//        instruction. Thus, the end-of-loop expression <expr> in the source
//        code must represent the address of the instruction AFTER the last
//        instruction in the loop as shown in the example."
//       This is fine if we know the address already, but a problem when we
//       don't.
int dsp_imm12_abs16(LONG inst)
{
	if (dsp_a0exattr & DEFINED)
	{
		if ((dsp_am0 & (M_DSPIM12 | M_DSPIM8)) == 0)
			return error("immediate out of range ($000-$FFF)");

		inst |= ((dsp_a0exval & 0x0FF) << 8) | ((dsp_a0exval & 0xF00) >> 8);
		D_dsp(inst);
	}
	else
	{
		AddFixup(FU_DSPIMM12, sloc, dsp_a0expr);
		D_dsp(inst);
	}

	if (dsp_a1exattr & DEFINED)
	{
		D_dsp((a1exval - 1));
	}
	else
	{
		append_minus_1(dsp_a1expr);
		AddFixup(FU_DSPADR16, sloc, dsp_a1expr);
		D_dsp(0);
	}

	return OK;
}


// Just store ea and 16bit address
// Note: this function is specifically handling DO.
// The same notes as dsp_imm12_abs16 apply here.
int dsp_ea_abs16(LONG inst)
{
	if ((dsp_a0reg == DSP_EA_ABS && dsp_a0memspace == -1) || dsp_a1reg == DSP_EA_IMM)
		return error("immediate values > 31 or absolute values not allowed");

	if (dsp_a0exattr & DEFINED)
	{
		if (dsp_a0exval > 31)
			return error("absolute address (aa) bigger than $1F");

		inst |= dsp_a0exval << 8;
	}

	inst |= dsp_a0reg << 8;

	if (dsp_a0memspace == -1)
		return error("only X:, Y: address spaces allowed");

	if ((deposit_extra_ea == DEPOSIT_EXTRA_FIXUP) || (dsp_a0reg == DSP_EA_ABS && dsp_am0 == M_DSPEA))
	{
		// Change instruction to aa instead of ea. TODO: check if this is true
		// for all cases
		inst = 0x060000;
		inst |= dsp_a0memspace;

		// Probably superfluous check (we're not likely to land here with a
		// known aa) but oh well
		if (!(dsp_a0exattr & DEFINED))
		{
			AddFixup(FU_DSPADR06, sloc, dsp_a0expr);
			D_dsp(inst);
		}
		else
		{
			D_dsp(inst);
		}
	}
	else
	{
		inst |= dsp_a0memspace;
		D_dsp(inst);
	}

	if (dsp_a1exattr & DEFINED)
	{
		D_dsp((dsp_a1exval - 1));
	}
	else
	{
		append_minus_1(dsp_a1expr);
		AddFixup(FU_DSPADR16, sloc, dsp_a1expr);
		D_dsp(0);
	}

	return OK;
}


// Store register (table A-18 in the motorola manual) 5-bit constant and 16bit address
// Note: this function is specifically handling DO.
// The same notes as dsp_imm12_abs16 apply here.
int dsp_reg_abs16(LONG inst)
{
	LONG reg;

	if ((reg = tab_A18(&dsp_am0, &dsp_a0reg)) == ERROR)
		return ERROR;

	inst |= reg << 8;

	if (dsp_a1exattr & DEFINED)
	{
		int v = (int)dsp_a1exval - 1;
		D_dsp(inst);
		D_dsp(v);
	}
	else
	{
		D_dsp(inst);
		append_minus_1(dsp_a1expr);
		AddFixup(FU_DSPADR16, sloc, dsp_a1expr);
		D_dsp(0);
	}

	return OK;
}


// Store ALU24 register and 16bit address
// Note: this function is specifically handling DO.
// The same notes as dsp_imm12_abs16 apply here.
int dsp_alu24_abs16(LONG inst)
{
	inst |= (dsp_a0reg & 7) << 8;

	if (dsp_a1exattr & DEFINED)
	{
		int v = (int)dsp_a1exval - 1;
		D_dsp(inst);
		D_dsp(v);
	}
	else
	{
		D_dsp(inst);
		append_minus_1(dsp_a1expr);
		AddFixup(FU_DSPADR16, sloc, dsp_a1expr);
		D_dsp(0);
	}

	return OK;
}


// Store register (table A-18 in the motorola manual)
int dsp_reg(LONG inst)
{
	LONG reg;

	if ((reg = tab_A18(&dsp_am0, &dsp_a0reg)) == ERROR)
		return ERROR;

	inst |= reg << 8;
	D_dsp(inst);

	return OK;
}


int dsp_alu24(LONG inst)
{
	inst |= (dsp_a0reg & 7) << 8;
	D_dsp(inst);

	return OK;
}


// Store register (table A-18 in the motorola manual) and 5-bit constant
int dsp_reg_imm5_abs16(LONG inst)
{
	LONG reg;

	// First, check that we have at best an 16bit absolute address in
	// operand 3 since we don't check that anywhere else
	if (dsp_a2exattr & DEFINED)
	{
		if ((dsp_am2 & C_DSPABS16) == 0)
			return error("expected 16-bit address as third operand.");
	}

	if ((reg = tab_A18(&dsp_am1, &dsp_a1reg)) == ERROR)
		return ERROR;

	inst |= reg << 8;

	if (dsp_a0exattr & DEFINED)
	{
		int v = (int)dsp_a0exval;

		if (v < 0 || v > 23)
			return error("immediate value must be between 0 and 23");

		inst |= v;
		D_dsp(inst);

		if (dsp_a2exattr & DEFINED)
		{
			int v = (int)dsp_a2exval;
			D_dsp(v);
		}
		else
		{
			AddFixup(FU_DSPADR16, sloc, dsp_a2expr);
			D_dsp(0);
		}
	}
	else
	{
		AddFixup(FU_DSPIMM5, sloc, dsp_a0expr);
		D_dsp(inst);

		if (dsp_a2exattr & DEFINED)
		{
			int v = (int)dsp_a2exval;
			D_dsp(v);
		}
		else
		{
			AddFixup(FU_DSPADR16, sloc, dsp_a2expr);
			D_dsp(0);
		}
	}

	return OK;
}


// Store ea, 5-bit constant and 16-bit address in the extension word
int dsp_ea_imm5_abs16(LONG inst)
{
	// First, check that we have at best an 16bit absolute address in
	// operand 3 since we don't check that anywhere else
	if (dsp_a2exattr&DEFINED)
	{
		if ((dsp_am2&C_DSPABS16) == 0)
			return error("expected 16-bit address as third operand.");
	}

	if (dsp_a1memspace == -1)
		return error("Only X: or Y: memory space allowed");

	if (dsp_am1 == M_DSPAA)
	{
		if (dsp_a1exattr & DEFINED)
			inst |= (dsp_a1exval & 0x3F) << 8;
		else
			AddFixup(FU_DSPADR06, sloc, dsp_a1expr);
	}

	if (dsp_am1 == M_DSPPP)
	{
		if (dsp_a1exattr & DEFINED)
			inst |= (dsp_a1exval & 0x3f) << 8;
		else
			AddFixup(FU_DSPPP06, sloc, dsp_a1expr);
	}

	if (dsp_a0exattr & DEFINED)
	{
		int v = (int)dsp_a0exval;

		if (v < 0 || v > 23)
			return error("immediate value must be between 0 and 23");

		inst |= (dsp_a1reg << 8) | v | dsp_a1memspace;
		D_dsp(inst);

		if (dsp_a2exattr & DEFINED)
		{
			int v = (int)dsp_a2exval;
			D_dsp(v);
		}
		else
		{
			AddFixup(FU_DSPADR16, sloc, dsp_a2expr);
			D_dsp(0);
		}
	}
	else
	{
		inst |= (dsp_a1reg << 8) | dsp_a1memspace;
		AddFixup(FU_DSPIMM5, sloc, dsp_a0expr);
		D_dsp(inst);

		if (dsp_a2exattr & DEFINED)
		{
			int v = (int)dsp_a2exval;
			D_dsp(v);
		}
		else
		{
			AddFixup(FU_DSPADR16, sloc, dsp_a2expr);
			D_dsp(0);
		}
	}

	return OK;
}


int dsp_ea_lua(LONG inst)
{
	int am = dsp_a0reg & 0x38;

	if (am != DSP_EA_POSTDEC && am != DSP_EA_POSTINC &&
		am != DSP_EA_POSTDEC1 && am != DSP_EA_POSTINC1)
		return error("addressing mode not allowed");

	inst |= dsp_a0reg << 8;

	if (dsp_am1 == M_DSPN)
		inst |= 1 << 3;

	inst |= dsp_a1reg;
	D_dsp(inst);

	return OK;
}


int dsp_ab_rn(LONG inst)
{
	inst |= (dsp_a1reg & 1) << 3;
	inst |= (dsp_a0reg) << 8;
	D_dsp(inst);

	return OK;
}


int dsp_movec_ea(LONG inst)
{
	int ea = dsp_a1reg;
	int memspace = dsp_a1memspace;
	WORD exattr = dsp_a1exattr;
	LONG exval = (uint32_t)dsp_a1exval;
	TOKEN * expr = dsp_a1expr;
	int reg = dsp_a0reg;
	int am = dsp_am0;
	int reg2 = dsp_a1reg;

	if (dsp_am0 == M_DSPEA || (dsp_am0 & C_DSPIM))
	{
		ea = dsp_a0reg;
		exattr = dsp_a0exattr;
		exval = (uint32_t)dsp_a0exval;
		memspace = dsp_a0memspace;
		expr = dsp_a0expr;
		reg = dsp_a1reg;
		reg2 = dsp_a0reg;
		am = dsp_am1;
	}

	// Abort if unsupported registers are requested
	if (reg == REG56_PC || reg == REG56_MR || reg == REG56_CCR)
		return error("illegal registers for instruction.");

	if (dsp_am0 & C_DSPIM)
		memspace = 0;

	if (memspace == -1)
		return error("only x: or y: memory spaces allowed.");

	// No memspace required when loading an immediate
	if (dsp_am0 & C_DSPIM)
		memspace = 0;

	reg = tab_A18(&am, &reg);
	inst |= (ea << 8) | memspace | reg;

	if (am == M_DSPPCU)
		inst |= 3 << 3;

	D_dsp(inst);

	if (reg2 == DSP_EA_ABS || (dsp_am0 & C_DSPIM))
	{
		if (exattr & DEFINED)
		{
			int v = exval;
			D_dsp(v);
		}
		else
		{
			if (dsp_am0 == M_DSPIM)
			{
				AddFixup(FU_DSPIMM24, sloc, expr);
				D_dsp(0);
			}
			else
			{
				AddFixup(FU_DSPADR24, sloc, expr);
				D_dsp(0);
			}
		}
	}

	return OK;
}


int dsp_movec_aa(LONG inst)
{
	int ea = dsp_a1reg;
	int memspace = dsp_a1memspace;
	WORD exattr = dsp_a1exattr;
	LONG exval = (uint32_t)dsp_a1exval;
	TOKEN * expr = dsp_a1expr;
	int reg = dsp_a0reg;
	int am = dsp_am0;
	int reg2 = dsp_a1reg;

	if (dsp_am0 == M_DSPAA)
	{
		ea = dsp_a0reg;
		exattr = dsp_a0exattr;
		exval = (uint32_t)dsp_a0exval;
		memspace = dsp_a0memspace;
		expr = dsp_a0expr;
		reg = dsp_a1reg;
		reg2 = dsp_a0reg;
		am = dsp_am1;
	}

	// Abort if unsupported registers are requested
	if (reg == REG56_PC || reg == REG56_MR || reg == REG56_CCR)
		return error("PC, MR, CCR are illegal registers for this instruction.");

	if (memspace == -1)
		return error("only x: or y: memory spaces allowed.");

	reg = tab_A18(&am, &reg);
	inst |= (ea << 8) | memspace | reg;

	if (am == M_DSPPCU)
		inst |= 3 << 3;

	if (exattr & DEFINED)
	{
		inst |= exval << 8;
		D_dsp(inst);
	}
	else
	{
		AddFixup(FU_DSPADR06, sloc, expr);
		D_dsp(inst);
	}

	return OK;
}


int dsp_movec_reg(LONG inst)
{
	int am0 = dsp_am0;
	int am1 = dsp_am1;

	// Abort if unsupported registers are requested
	if (dsp_a0reg == REG56_PC || dsp_a0reg == REG56_MR || dsp_a0reg == REG56_CCR ||
		dsp_a1reg == REG56_PC || dsp_a1reg == REG56_MR || dsp_a1reg == REG56_CCR)
		return error("PC, MR, CCR are illegal registers for this instruction.");

	int reg1 = tab_A18(&dsp_am0, &dsp_a0reg);
	int reg2 = tab_A18(&dsp_am1, &dsp_a1reg);

	if (inst & (1 << 15))
	{
		// S1,D2
	}
	else
	{
		// S2,D1
		int temp = am0;
		am0 = am1;
		am1 = temp;
		temp = reg1;
		reg1 = reg2;
		reg2 = temp;
	}

	switch (am0)
	{
	case M_ALU24:  reg1 |= 0x00; break;
	case M_ACC8:
	case M_ACC24:  reg1 |= 0x08; break;
	case M_ACC56:  reg1 |= 0x0E; break;
	case M_DSPR:   reg1 |= 0x10; break;
	case M_DSPN:   reg1 |= 0x18; break;
	case M_DSPM:   reg1 |= 0x20; break;
	case M_DSPPCU: reg1 |= 0x38; break;
	default:
		return error("reached the end of dsp_movec_reg case 1 - should not happen!");
	}

	switch (am1)
	{
	case M_DSPM:   reg2 |= 0x00; break;
	case M_DSPPCU: reg2 |= 0x18; break;
	default:
		return error("reached the end of dsp_movec_reg case 2 - should not happen!");
	}

	inst |= (reg1 << 8) | reg2;
	D_dsp(inst);

	return OK;
}


int dsp_mult(LONG inst)
{
	if (dsp_am2 != M_ACC56)
		return error("only A or B allowed as third operand.");

	switch (((dsp_a0reg & 3) << 2) + (dsp_a1reg & 3))
	{
	case (0 << 2) + 0: inst |= 0 << 4; break; // x0 x0
	case (2 << 2) + 2: inst |= 1 << 4; break; // y0 y0
	case (1 << 2) + 0: inst |= 2 << 4; break; // x1 x0
	case (0 << 2) + 1: inst |= 2 << 4; break; // x0 x1
	case (3 << 2) + 2: inst |= 3 << 4; break; // y1 y0
	case (2 << 2) + 3: inst |= 3 << 4; break; // y0 y1
	case (0 << 2) + 3: inst |= 4 << 4; break; // x0 y1
	case (3 << 2) + 0: inst |= 4 << 4; break; // y1 x0
	case (2 << 2) + 0: inst |= 5 << 4; break; // y0 x0
	case (0 << 2) + 2: inst |= 5 << 4; break; // x0 y0
	case (1 << 2) + 2: inst |= 6 << 4; break; // x1 y0
	case (2 << 2) + 1: inst |= 6 << 4; break; // y0 x1
	case (3 << 2) + 1: inst |= 7 << 4; break; // y1 x1
	case (1 << 2) + 3: inst |= 7 << 4; break; // x1 y1
	default:
		return error("x0/y0/x1/y1 combination not allowed for multiplication.");
	}

	if (dsp_a2reg == REG56_B)
		inst |= 1 << 3;

	inst |= dsp_k;
	D_dsp(inst);
	dsp_extra_ea();		// Deposit effective address if needed

	return OK;
}


int dsp_movem_ea(LONG inst)
{
	int ea = dsp_a1reg;
	int memspace = dsp_a0memspace;
	WORD exattr = dsp_a1exattr;
	LONG exval = (uint32_t)dsp_a1exval;
	TOKEN * expr = dsp_a1expr;
	int reg = dsp_a0reg;
	int am = dsp_am0;
	int reg2 = dsp_a1reg;

	if (dsp_am0 == M_DSPEA || dsp_am0 == M_DSPIM)
	{
		ea = dsp_a0reg;
		exattr = dsp_a0exattr;
		exval = (uint32_t)dsp_a0exval;
		memspace = dsp_a0memspace;
		expr = dsp_a0expr;
		reg = dsp_a1reg;
		reg2 = dsp_a0reg;
		am = dsp_am1;
		inst |= 1 << 15;
	}

	// Abort if unsupported registers are requested
	if (reg == REG56_PC || reg == REG56_MR || reg == REG56_CCR)
		return error("illegal registers for instruction.");

	if (memspace != -1)
		return error("only p: memory space allowed.");

	reg = tab_A18(&am, &reg);
	inst |= (ea << 8) | reg;

	if (am == M_DSPPCU)
		inst |= 3 << 3;

	D_dsp(inst);

	if (reg2 == DSP_EA_ABS || dsp_am0 == M_DSPIM)
	{
		if (exattr & DEFINED)
		{
			int v = exval;
			D_dsp(v);
		}
		else
		{
			if (dsp_am0 == M_DSPIM)
			{
				AddFixup(FU_DSPIMM24, sloc, expr);
				D_dsp(0);
			}
			else
			{
				AddFixup(FU_DSPADR24, sloc, expr);
				D_dsp(0);
			}
		}
	}

	return OK;
}


int dsp_movem_aa(LONG inst)
{
	int ea = dsp_a1reg;
	int memspace = dsp_a1memspace;
	WORD exattr = dsp_a1exattr;
	LONG exval = (uint32_t)dsp_a1exval;
	TOKEN * expr = dsp_a1expr;
	int reg = dsp_a0reg;
	int am = dsp_am0;
	int reg2 = dsp_a1reg;

	if (dsp_am0 == M_DSPAA)
	{
		ea = dsp_a0reg;
		exattr = dsp_a0exattr;
		exval = (uint32_t)dsp_a0exval;
		memspace = dsp_a0memspace;
		expr = dsp_a0expr;
		reg = dsp_a1reg;
		reg2 = dsp_a0reg;
		am = dsp_am1;
	}

	// Abort if unsupported registers are requested
	if (reg == REG56_PC || reg == REG56_MR || reg == REG56_CCR)
		return error("PC, MR, CCR are illegal registers for this instruction.");

	if (memspace != -1)
		return error("only p: memory space allowed.");

	reg = tab_A18(&am, &reg);
	inst |= (ea << 8) | reg;

	if (am == M_DSPPCU)
		inst |= 3 << 3;

	if (exattr & DEFINED)
	{
		inst |= exval << 8;
		D_dsp(inst);
	}
	else
	{
		AddFixup(FU_DSPADR06, sloc, expr);
		D_dsp(inst);
	}

    return OK;
}


int dsp_movep_ea(LONG inst)
{
	// movep doesn't allow any aa modes but we might detect this during amode
	// detection. No worries, just change it to ea with extra address instead
	if (dsp_am0 == M_DSPAA)
	{
		dsp_a0reg = DSP_EA_ABS;
		dsp_a1reg = 0;
	}
	if (dsp_am1 == M_DSPAA)
	{
		dsp_a0reg = 0;
		dsp_a1reg = DSP_EA_ABS;
	}

	// So we might encounter something like 'movep x:pp,x:pp' which we
	// obviously flagged as M_DSPPP during ea parsing. In this case we give up
	// and declare the second term as a classic absolute address (we chop off
	// the high bits too) and let the routine treat is as such. At least that's
	// what Motorola's assembler seems to be doing.
	if (dsp_am0 == M_DSPPP && dsp_am1 == M_DSPPP)
	{
		dsp_am1 = DSP_EA_ABS;
		dsp_a1reg = DSP_EA_ABS;
		dsp_a1exval &= 0xFFFF;
	}

	// Assume first operand is :pp
	int ea = dsp_a1reg;
	int memspace = dsp_a1memspace;
	int perspace = dsp_a0perspace;
	WORD exattr = dsp_a1exattr;
	WORD exattr2 = dsp_a0exattr;
	LONG exval = (uint32_t)dsp_a1exval;
	LONG exval2 = (uint32_t)dsp_a0exval;
	TOKEN * expr = dsp_a1expr;
	TOKEN * expr2 = dsp_a0expr;
	int reg = dsp_a0reg;
	int am = dsp_am0;
	int reg2 = dsp_a1reg;

	if (dsp_am1 == M_DSPPP)
	{
		ea = dsp_a0reg;
		exattr = dsp_a0exattr;
		exattr2 = dsp_a1exattr;
		exval = (uint32_t)dsp_a0exval;
		exval2 = (uint32_t)dsp_a1exval;
		memspace = dsp_a0memspace;
		perspace = dsp_a1perspace;
		expr = dsp_a0expr;
		expr2 = dsp_a1expr;
		reg = dsp_a1reg;
		reg2 = dsp_a0reg;
		am = dsp_am1;
	}

	if (dsp_a0perspace == -1 && dsp_a1perspace == -1)
	{
		// Ok, so now we have to guess which of the two parameters is X:pp or
		// Y:pp. This happened because we didn't get a << marker in any of the
		// addressing modes
		if (((dsp_a0exattr | dsp_a1exattr) & DEFINED) == 0)
			// You have got to be shitting me...
			// One way to deal with this (for example X:ea,X:pp / X:pp,X:ea
			// aliasing would be to check number ranges and see which one is
			// negative. ...unless one of the two isn't known during this phase
			return error("internal assembler error: could not deduce movep syntax");

		if (dsp_a0exattr & DEFINED)
		{
			if (dsp_a0exval >= 0xFFC0 && dsp_a0exval <= 0xFFFF)
			{
				// First operand is :pp - do nothing
				perspace = dsp_a0memspace << 10;
				// When the source contains a << then we bolt on the :pp
				// address during ea parsing, but since we couldn't recognise
				// the addressing mode in this case let's just insert it right
				// now...
				reg |= (dsp_a0exval & 0x3F);
			}
		}

		if (dsp_a1exattr & DEFINED)
		{
			if (dsp_a1exval >= 0xFFC0 && dsp_a1exval <= 0xFFFF)
			{
				ea = dsp_a0reg;
				exattr = dsp_a0exattr;
				exval = (uint32_t)dsp_a0exval;
				memspace = dsp_a0memspace;
				perspace = dsp_a1memspace << 10;
				expr = dsp_a0expr;
				reg = dsp_a0reg;
				reg2 = dsp_a1reg;
				am = dsp_am1;
				// See above
				reg |= (dsp_a1exval & 0x3F);
			}
		}

		if (perspace == -1)
			// You have got to be shitting me (twice)...
			return error("internal assembler error: could not deduce movep syntax");
	}

	inst |= reg | (ea << 8) | perspace;       // reg contains memory space

	if ((dsp_am0 & (M_DSPIM | M_DSPIM8 | M_DSPIM12)) == 0)
	{
		if (memspace == -1)
		{
			inst &= ~(1 << 7);
			inst |= 1 << 6;
		}
		else
			inst |= memspace;
	}

	if (am == M_DSPPP)
	{
		if (exattr2&DEFINED)
		{
			inst |= (exval2 & 0x3F);
			D_dsp(inst);
		}
		else
		{
			AddFixup(FU_DSPIMM5, sloc, expr2);
			D_dsp(inst);
		}
	}
	else
	{
		D_dsp(inst);
	}

	if (dsp_am0 & (M_DSPIM | M_DSPIM8 | M_DSPIM12))
	{
		if (dsp_a0exattr & DEFINED)
		{
			int v = (int)dsp_a0exval;
			D_dsp(v);
		}
		else
		{
			AddFixup(FU_DSPADR16, sloc, dsp_a0expr);
			D_dsp(0);
		}
	}
	else if (reg2 == DSP_EA_ABS)
	{
		if (exattr & DEFINED)
		{
			int v = exval;
			D_dsp(v);
		}
		else
		{
			AddFixup(FU_DSPADR24, sloc, expr);
			D_dsp(0);
		}
	}

	return OK;
}


int dsp_movep_reg(LONG inst)
{
	// Assume first operand is :pp
	int ea = dsp_a0reg;
	int memspace = dsp_a0memspace;
	int perspace = dsp_a0perspace;
	WORD exattr = dsp_a1exattr;
	LONG exval = (uint32_t)dsp_a1exval;
	TOKEN * expr = dsp_a1expr;
	int reg = dsp_a0reg;
	int am = dsp_am1;
	int reg2 = dsp_a1reg;

	if (dsp_am1 == M_DSPPP)
	{
		ea = dsp_a1reg;
		exattr = dsp_a0exattr;
		exval = (uint32_t)dsp_a0exval;
		memspace = dsp_a1memspace;
		perspace = dsp_a1perspace;
		expr = dsp_a0expr;
		reg = dsp_a1reg;
		reg2 = dsp_a0reg;
		am = dsp_am0;
	}

	// Abort if unsupported registers are requested
	if (reg == REG56_PC || reg == REG56_MR || reg == REG56_CCR)
		return error("illegal registers for instruction.");

	reg2 = tab_A18(&am, &reg2);
	inst |= (reg2 << 8) | reg;

	if (perspace == -1)
		return error("only x: or y: memory space allowed.");
	else
		inst |= perspace;

	D_dsp(inst);

	if (reg2 == DSP_EA_ABS)
	{
		if (exattr & DEFINED)
		{
			int v = exval;
			D_dsp(v);
		}
		else
		{
			AddFixup(FU_DSPADR24, sloc, expr);
			D_dsp(0);
		}
	}

	return OK;
}

