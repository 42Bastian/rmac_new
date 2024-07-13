//
// RMAC - Renamed Macro Assembler for the Atari Jaguar Console System
// AMODE.C - DSP 56001 Addressing Modes
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "dsp56k_amode.h"
#include "error.h"
#include "token.h"
#include "expr.h"
#include "rmac.h"
#include "procln.h"
#include "sect.h"
#include "math.h"

#define DEF_MN
#include "mntab.h"
#define DEF_REG56
#include "56kregs.h"

// Address-mode information
int dsp_am0;					// Addressing mode
int dsp_a0reg;					// Register
TOKEN dsp_a0expr[EXPRSIZE];		// Expression
uint64_t dsp_a0exval;			// Expression's value
WORD dsp_a0exattr;				// Expression's attribute
LONG dsp_a0memspace;			// Addressing mode's memory space (P, X, Y)
SYM * dsp_a0esym;				// External symbol involved in expr

int dsp_am1;					// Addressing mode
int dsp_a1reg;					// Register
TOKEN dsp_a1expr[EXPRSIZE];		// Expression
uint64_t dsp_a1exval;			// Expression's value
WORD dsp_a1exattr;				// Expression's attribute
LONG dsp_a1memspace;			// Addressing mode's memory space (P, X, Y)
SYM * dsp_a1esym;				// External symbol involved in expr

int dsp_am2;					// Addressing mode
int dsp_a2reg;					// Register
TOKEN dsp_a2expr[EXPRSIZE];		// Expression
uint64_t dsp_a2exval;			// Expression's value
WORD dsp_a2exattr;				// Expression's attribute
SYM * dsp_a2esym;				// External symbol involved in expr

int dsp_am3;					// Addressing mode
int dsp_a3reg;					// Register
TOKEN dsp_a3expr[EXPRSIZE];		// Expression
uint64_t dsp_a3exval;			// Expression's value
WORD dsp_a3exattr;				// Expression's attribute
SYM * dsp_a3esym;				// External symbol involved in expr

TOKEN dspImmedEXPR[EXPRSIZE];	// Expression
uint64_t dspImmedEXVAL;			// Expression's value
WORD  dspImmedEXATTR;			// Expression's attribute
SYM * dspImmedESYM;				// External symbol involved in expr
int  deposit_extra_ea;			// Optional effective address extension
TOKEN dspaaEXPR[EXPRSIZE];		// Expression
uint64_t dspaaEXVAL;			// Expression's value
WORD  dspaaEXATTR;				// Expression's attribute
SYM * dspaaESYM;				// External symbol involved in expr

LONG dsp_a0perspace;			// Peripheral space (X, Y - used in movep)
LONG dsp_a1perspace;			// Peripheral space (X, Y - used in movep)

int dsp_k;						// Multiplications sign

static inline LONG checkea(const uint32_t termchar, const int strings);

// ea checking error strings put into a table because I'm not sure there's an
// easy way to do otherwise (the messages start getting differerent in many
// places so it will get awkward to code those in) (I'd rather burn some RAM
// in order to have more helpful error messages than the other way round)

#define X_ERRORS 0
#define Y_ERRORS 1
#define L_ERRORS 2
#define P_ERRORS 3

const char *ea_errors[][12] = {
	// X:
	{
		"unrecognised X: parallel move syntax: expected '(' after 'X:-'",                                       // 0
		"unrecognised X: parallel move syntax: expected ')' after 'X:-(Rn'",                                    // 1
		"unrecognised X: parallel move syntax: expected R0-R7 after 'X:-('",                                    // 2
		"unrecognised X: parallel move syntax: expected N0-N7 after 'X:(Rn+'",                                  // 3
		"unrecognised X: parallel move syntax: expected same register number in Rn and Nn for 'X:(Rn+Nn)'",     // 4
		"unrecognised X: parallel move syntax: expected ')' after 'X:(Rn+Nn'",                                  // 5
		"unrecognised X: parallel move syntax: expected same register number in Rn and Nn for 'X:(Rn)+Nn'",     // 6
		"unrecognised X: parallel move syntax: expected N0-N7 after 'X:(Rn)+'",                                 // 7
		"unrecognised X: parallel move syntax: expected same register number in Rn and Nn for 'X:(Rn)-Nn'",     // 8
		"unrecognised X: parallel move syntax: expected N0-N7 after 'X:(Rn)-'",                                 // 9
		"unrecognised X: parallel move syntax: expected '+', '-' or ',' after 'X:(Rn)'",                        // 10
		"unrecognised X: parallel move syntax: expected '+' or ')' after 'X:(Rn'",                              // 11
	},
	// Y:
	{
		"unrecognised Y: parallel move syntax: expected '(' after 'Y:-'",                                       // 0
		"unrecognised Y: parallel move syntax: expected ')' after 'Y:-(Rn'",                                    // 1
		"unrecognised Y: parallel move syntax: expected R0-R7 after 'Y:-('",                                    // 2
		"unrecognised Y: parallel move syntax: expected N0-N7 after 'Y:(Rn+'",                                  // 3
		"unrecognised Y: parallel move syntax: expected same register number in Rn and Nn for 'Y:(Rn+Nn)'",     // 4
		"unrecognised Y: parallel move syntax: expected ')' after 'Y:(Rn+Nn'",                                  // 5
		"unrecognised Y: parallel move syntax: expected same register number in Rn and Nn for 'Y:(Rn)+Nn'",     // 6
		"unrecognised Y: parallel move syntax: expected N0-N7 after 'Y:(Rn)+'",                                 // 7
		"unrecognised Y: parallel move syntax: expected same register number in Rn and Nn for 'Y:(Rn)-Nn'",     // 8
		"unrecognised Y: parallel move syntax: expected N0-N7 after 'Y:(Rn)-'",                                 // 9
		"unrecognised Y: parallel move syntax: expected '+', '-' or ',' after 'Y:(Rn)'",                        // 10
		"unrecognised Y: parallel move syntax: expected '+' or ')' after 'Y:(Rn'",                              // 11
	},
	// L:
	{
		"unrecognised L: parallel move syntax: expected '(' after 'L:-'",                                       // 0
		"unrecognised L: parallel move syntax: expected ')' after 'L:-(Rn'",                                    // 1
		"unrecognised L: parallel move syntax: expected R0-R7 after 'L:-('",                                    // 2
		"unrecognised L: parallel move syntax: expected N0-N7 after 'L:(Rn+'",                                  // 3
		"unrecognised L: parallel move syntax: expected same register number in Rn and Nn for 'L:(Rn+Nn)'",     // 4
		"unrecognised L: parallel move syntax: expected ')' after 'L:(Rn+Nn'",                                  // 5
		"unrecognised L: parallel move syntax: expected same register number in Rn and Nn for 'L:(Rn)+Nn'",     // 6
		"unrecognised L: parallel move syntax: expected N0-N7 after 'L:(Rn)+'",                                 // 7
		"unrecognised L: parallel move syntax: expected same register number in Rn and Nn for 'L:(Rn)-Nn'",     // 8
		"unrecognised L: parallel move syntax: expected N0-N7 after 'L:(Rn)-'",                                 // 9
		"unrecognised L: parallel move syntax: expected '+', '-' or ',' after 'L:(Rn)'",                        // 10
		"unrecognised L: parallel move syntax: expected '+' or ')' after 'L:(Rn'",                              // 11
	},
	// P:
	{
		"unrecognised P: effective address syntax: expected '(' after 'P:-'",                                       // 0
		"unrecognised P: effective address syntax: expected ')' after 'P:-(Rn'",                                    // 1
		"unrecognised P: effective address syntax: expected R0-R7 after 'P:-('",                                    // 2
		"unrecognised P: effective address syntax: expected N0-N7 after 'P:(Rn+'",                                  // 3
		"unrecognised P: effective address syntax: expected same register number in Rn and Nn for 'P:(Rn+Nn)'",     // 4
		"unrecognised P: effective address syntax: expected ')' after 'P:(Rn+Nn'",                                  // 5
		"unrecognised P: effective address syntax: expected same register number in Rn and Nn for 'P:(Rn)+Nn'",     // 6
		"unrecognised P: effective address syntax: expected N0-N7 after 'P:(Rn)+'",                                 // 7
		"unrecognised P: effective address syntax: expected same register number in Rn and Nn for 'P:(Rn)-Nn'",     // 8
		"unrecognised P: effective address syntax: expected N0-N7 after 'P:(Rn)-'",                                 // 9
		"unrecognised P: effective address syntax: expected '+', '-' or ',' after 'P:(Rn)'",                        // 10
		"unrecognised P: effective address syntax: expected '+' or ')' after 'P:(Rn'",                              // 11
	}
};

enum
{
	NUM_NORMAL = 0,
	NUM_FORCE_LONG = 1,
	NUM_FORCE_SHORT = 2
};


//
// Parse a single addressing mode
//
static inline int dsp_parmode(int *am, int *areg, TOKEN * AnEXPR, uint64_t * AnEXVAL, WORD * AnEXATTR, SYM ** AnESYM, LONG *memspace, LONG *perspace, const int operand)
{
	if (*tok == REG56_A || *tok == REG56_B)
	{
		*am = M_ACC56;
		*areg = *tok++;
		return OK;
	}
	else if (*tok == '#')
	{
		tok++;

		if (*tok == '<')
		{
			// Immediate Short Addressing Mode Force Operator
			tok++;
			if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
				return ERROR;

			if (*AnEXVAL > 0xFFF && *AnEXVAL < -4096)
				return error("immediate short addressing mode forced but address is bigger than $FFF");

			if ((int32_t)*AnEXVAL <= 0xFF && (int32_t)*AnEXVAL > -0x100)
			{
				*am = M_DSPIM8;
				return OK;
			}

			*am = M_DSPIM12;
			return OK;
		}
		else if (*tok == '>')
		{
			// Immediate Long Addressing Mode Force Operator
			tok++;

			if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
				return ERROR;

			if ((int32_t)*AnEXVAL > 0xFFFFFF || (int32_t)*AnEXVAL < -0xFFFFFF)
				return error("long immediate is bigger than $FFFFFF");

			*am = M_DSPIM;
			return OK;
		}

		if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
			return ERROR;

		if (*AnEXATTR & DEFINED)
		{
			if ((int32_t)*AnEXVAL < 0x100 && (int32_t)*AnEXVAL >= -0x100)
			{
				*AnEXVAL &= 0xFF;
				*am = M_DSPIM8;
			}
			else if (*AnEXVAL < 0x1000)
				*am = M_DSPIM12;
			else
				*am = M_DSPIM;
		}
		else
		{
			// We have no clue what size our immediate will be
			// so we have to assume the worst
			*am = M_DSPIM;
		}

		return OK;
	}
	else if (*tok >= REG56_X0 && *tok <= REG56_Y1)
	{
		*am = M_ALU24;
		*areg = *tok++;
		return OK;
	}
	else if (*tok == REG56_X && *(tok + 1) == ':')
	{
		tok = tok + 2;

		if (*tok == CONST || *tok == FCONST || *tok == SYMBOL)
		{
			if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
				return ERROR;

			if (*AnEXATTR & DEFINED)
			{
				if (*AnEXVAL > 0xFFFFFF)
					return error("long address is bigger than $FFFFFF");

				*memspace = 0 << 6;     // Mark we're on X memory space

				// Check if value is between $FFC0 and $FFFF, AKA X:pp
				uint32_t temp = (LONG)(((int32_t)(((uint32_t)*AnEXVAL) << (32 - 6))) >> (32 - 6));  // Sign extend 6 to 32 bits

				if ((temp >= 0xFFFFFFC0                      /* Check for 32bit sign extended number */
					&& ((int32_t)*AnEXVAL < 0))              /* Check if 32bit signed number is negative*/
					|| (*AnEXVAL < 0xFFFF && *AnEXVAL >= 0x8000)) /* Check if 16bit number is negative*/
				{
					*AnEXVAL = temp;
					*am = M_DSPPP;
					*memspace = 0 << 6;          // Mark we're on X memory space
					*perspace = 0 << 16;         // Mark we're on X peripheral space
					*areg = *AnEXVAL & 0x3F;     // Since this is only going to get used in dsp_ea_imm5...
					return OK;
				}

				// If the symbol/expression is defined then check for valid range.
				// Otherwise the value had better fit or Fixups will bark!
				if (*AnEXVAL > 0x3F)
				{
					*am = M_DSPEA;
					*areg = DSP_EA_ABS;
				}
				else
				{
					*am = M_DSPAA;
				}
			}
			else
			{
				// Assume the worst
				*memspace = 0 << 6;     // Mark we're on X memory space
				*am = M_DSPEA;
				*areg = DSP_EA_ABS;
			}

			return OK;
		}
		else if (*tok == '<')
		{
			// X:aa
			// Short Addressing Mode Force Operator in the case of '<'
			tok++;

			if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
				return ERROR;

			// If the symbol/expression is defined then check for valid range.
			// Otherwise the value had better fit or Fixups will bark!
			if (*AnEXATTR & DEFINED)
			{
				if (*AnEXVAL > 0x3F)
					return error("short addressing mode forced but address is bigger than $3F");
			}
			else
			{
				// Mark it as a fixup
				deposit_extra_ea = DEPOSIT_EXTRA_FIXUP;
			}

			*am = M_DSPAA;
			*memspace = 0 << 6;     // Mark we're on X memory space
			*areg = (int)*AnEXVAL;     // Since this is only going to get used in dsp_ea_imm5...
			return OK;
		}
		else if (*tok == '>')
		{
			// Long Addressing Mode Force Operator
			tok++;

			if (*tok == CONST || *tok == FCONST || *tok == SYMBOL)
			{
				if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
					return ERROR;

				if (*AnEXATTR&DEFINED)
				{
					if (*AnEXVAL > 0xFFFFFF)
						return error("long address is bigger than $FFFFFF");

					*memspace = 0 << 6;     // Mark we're on X memory space
					*am = M_DSPEA;
					*areg = DSP_EA_ABS;
				}
				else
				{
					// Assume the worst
					*memspace = 0 << 6;     // Mark we're on X memory space
					*am = M_DSPEA;
					*areg = DSP_EA_ABS;
				}
				return OK;
			}
		}
		else if (*tok == SHL)  // '<<'
		{
			// I/O Short Addressing Mode Force Operator
			// X:pp
			tok++;

			if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
				return ERROR;

			// If the symbol/expression is defined then check for valid range.
			// Otherwise the value had better fit or Fixups will bark!
			if (*AnEXATTR & DEFINED)
			{
				*AnEXVAL = (LONG)(((int32_t)(((uint32_t)*AnEXVAL) << (32 - 6))) >> (32 - 6));  // Sign extend 6 to 32 bits

				if (*AnEXVAL < 0xFFFFFFC0)
					return error("I/O Short Addressing Mode addresses must be between $FFC0 and $FFFF");
			}

			*am = M_DSPPP;
			*memspace = 0 << 6;          // Mark we're on X memory space
			*perspace = 0 << 16;         // Mark we're on X peripheral space
			*areg = *AnEXVAL & 0x3F;     // Since this is only going to get used in dsp_ea_imm5...
			return OK;
		}

		if ((*areg = checkea(0, X_ERRORS)) != ERROR)
		{
			// TODO: what if we need M_DSPAA here????
			*memspace = 0 << 6;     // Mark we're on X memory space
			*am = M_DSPEA;
			return OK;
		}
		else
			return ERROR;
	}
	else if (*tok == REG56_Y && *(tok + 1) == ':')
	{
		tok = tok + 2;

		if (*tok == CONST || *tok == FCONST || *tok == SYMBOL)
		{
			if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
				return ERROR;

			if (*AnEXVAL > 0xFFFFFF)
				return error("long address is bigger than $FFFFFF");

			*memspace = 1 << 6;     // Mark we're on Y memory space

			// Check if value is between $ffc0 and $ffff, AKA Y:pp
			uint32_t temp = (LONG)(((int32_t)(((uint32_t)*AnEXVAL) << (32 - 6))) >> (32 - 6));  // Sign extend 6 to 32 bits

			if ((temp >= 0xFFFFFFC0                         /* Check for 32bit sign extended number */
				&& ((int32_t)*AnEXVAL < 0))                 /* Check if 32bit signed number is negative*/
				|| (*AnEXVAL < 0xFFFF && *AnEXVAL >= 0x8000)) /* Check if 16bit number is negative*/
			{
				*AnEXVAL = temp;
				*am = M_DSPPP;
				*perspace = 1 << 16;         // Mark we're on X peripheral space
				*areg = *AnEXVAL & 0x3F;     // Since this is only going to get used in dsp_ea_imm5...
				return OK;
			}

			// If the symbol/expression is defined then check for valid range.
			// Otherwise the value had better fit or Fixups will bark!
			if (*AnEXATTR & DEFINED)
			{
				if (*AnEXVAL > 0x3F)
				{
					*am = M_DSPEA;
					*areg = DSP_EA_ABS;
				}
				else
				{
					*am = M_DSPAA;
				}
			}
			else
			{
				*am = M_DSPEA;
				*areg = DSP_EA_ABS;
			}

			return OK;
		}
		else if (*tok == '<')
		{
			// Y:aa
			// Short Addressing Mode Force Operator in the case of '<'
			tok++;

			if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
				return ERROR;

			// If the symbol/expression is defined then check for valid range.
			// Otherwise the value had better fit or Fixups will bark!
			if (*AnEXATTR & DEFINED)
			{
				if (*AnEXVAL > 0x3F)
				{
					if (CHECK_OPTS(OPT_56K_AUTO_LONG))
					{
						if (optim_warn_flag)
							warn("o11: short addressing mode forced but address is bigger than $3F - switching to long");

						*am = M_DSPEA;
						*memspace = 1 << 6;     // Mark we're on Y memory space
						*areg = DSP_EA_ABS;
						return OK;
					}
					else
					{
						return error("short addressing mode forced but address is bigger than $3F - turn opt switch o11 on to bypass");
					}
				}
			}
			else
			{
				// Mark it as a fixup
				deposit_extra_ea = DEPOSIT_EXTRA_FIXUP;
			}

			*am = M_DSPAA;
			*memspace = 1 << 6;     // Mark we're on Y memory space
			*areg = (int)*AnEXVAL;      // Since this is only going to get used in dsp_ea_imm5...
			return OK;
		}
		else if (*tok == '>')
		{
			// Long Addressing Mode Force Operator
			tok++;

			if (*tok == CONST || *tok == FCONST || *tok == SYMBOL)
			{
				if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
					return ERROR;

				if (*AnEXATTR&DEFINED)
				{
					if (*AnEXVAL > 0xFFFFFF)
						return error("long address is bigger than $FFFFFF");

					*memspace = 1 << 6;     // Mark we're on Y memory space

					*am = M_DSPEA;
					*areg = DSP_EA_ABS;
				}
				else
				{
					// Assume the worst
					*memspace = 1 << 6;     // Mark we're on Y memory space
					*am = M_DSPEA;
					*areg = DSP_EA_ABS;
				}
				return OK;
			}
		}
		else if (*tok == SHL)  // '<<'
		{
			// I/O Short Addressing Mode Force Operator
			// Y:pp
			tok++;

			if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
				return ERROR;

			// If the symbol/expression is defined then check for valid range.
			// Otherwise the value had better fit or Fixups will bark!
			if (*AnEXATTR & DEFINED)
			{
				*AnEXVAL = (LONG)(((int32_t)(((uint32_t)*AnEXVAL) << (32 - 6))) >> (32 - 6));  // Sign extend 6 to 32 bits

				if (*AnEXVAL < 0xFFFFFFC0)
					return error("I/O Short Addressing Mode addresses must be between $FFE0 and $1F");
			}

			*am = M_DSPPP;
			*memspace = 1 << 6;          // Mark we're on Y memory space
			*perspace = 1 << 16;         // Mark we're on Y peripheral space
			*areg = *AnEXVAL & 0x3F;     // Since this is only going to get used in dsp_ea_imm5...
			return OK;
		}

		if ((*areg = checkea(0, X_ERRORS)) != ERROR)
		{
			*memspace = 1 << 6;     // Mark we're on Y memory space
			*am = M_DSPEA;
			return OK;
		}
		else
			return ERROR;
		// TODO: add absolute address checks
	}
	else if ((*tok >= REG56_X) && (*tok <= REG56_Y))
	{
		*am = M_INP48;
		*areg = *tok++;
		return OK;
	}
	else if ((*tok >= REG56_M0) && (*tok <= REG56_M7))
	{
		*am = M_DSPM;
		*areg = (*tok++) & 7;
		return OK;
	}
	else if ((*tok >= REG56_R0) && (*tok <= REG56_R7))
	{
		*am = M_DSPR;
		*areg = (*tok++) - REG56_R0;
		return OK;
	}
	else if ((*tok >= REG56_N0) && (*tok <= REG56_N7))
	{
		*am = M_DSPN;
		*areg = (*tok++) & 7;
		return OK;
	}
	else if ((*tok == REG56_A0) || (*tok == REG56_A1) || (*tok == REG56_B0)
		|| (*tok == REG56_B1))
	{
		*am = M_ACC24;
		*areg = *tok++;
		return OK;
	}
	else if ((*tok == REG56_A2) || (*tok == REG56_B2))
	{
		*am = M_ACC8;
		*areg = *tok++;
		return OK;
	}
	else if ((*tok == '-') && (*(tok + 1) == REG56_X0 || *(tok + 1) == REG56_X1 || *(tok + 1) == REG56_Y0 || *(tok + 1) == REG56_Y1))
	{
		// '-X0', '-Y0', '-X1' or '-Y1', used in multiplications
		tok++;

		// Check to see if this is the first operand
		if (operand != 0)
			return error("-x0/-x1/-y0/-y1 only allowed in the first operand");

		*am = M_ALU24;
		*areg = *tok++;
		dsp_k = 1 << 2;
		return OK;
	}
	else if (*tok == '+' && (*(tok + 1) == REG56_X0 || *(tok + 1) == REG56_X1 || *(tok + 1) == REG56_Y0 || *(tok + 1) == REG56_Y1))
	{
		// '+X0', '+Y0', '+X1' or '+Y1', used in multiplications
		tok++;

		// Check to see if this is the first operand
		if (operand != 0)
			return error("+x0/+x1/+y0/+y1 only allowed in the first operand");

		*am = M_ALU24;
		*areg = *tok++;
		dsp_k = 0 << 2;
		return OK;
	}
	else if (*tok == '(' || *tok == '-')
	{
		// Could be either an expression or ea mode
		if (*tok + 1 == SYMBOL)
		{
			tok++;

			if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
				return ERROR;

			*am = M_DSPIM;
			return OK;
		}

		if ((*areg = checkea(0, P_ERRORS)) != ERROR)
		{
			*am = M_DSPEA;
			return OK;
		}
		else
			return ERROR;

		// TODO: add absolute address checks
		return error("internal assembler error: parmode checking for '(' and '-' does not have absolute address checks yet!");
	}
	else if (*tok == REG56_P && *(tok + 1) == ':')
	{
		tok = tok + 2;

		if (*tok == CONST || *tok == FCONST || *tok == SYMBOL)
		{
			// Address
			if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
				return ERROR;

			if (*AnEXVAL > 0xFFFFFF)
				return error("long address is bigger than $FFFFFF");

			if (*AnEXVAL > 0x3F)
			{
				*am = M_DSPEA;
				*areg = DSP_EA_ABS;
			}
			else
			{
				*areg = (int)*AnEXVAL;     // Lame, but what the hell
				*am = M_DSPAA;
			}

			return OK;
		}
		else if (*tok == '<')
		{
			// X:aa
			// Short Addressing Mode Force Operator in the case of '<'
			tok++;

			if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
				return ERROR;

			if (*AnEXVAL > 0x3F)
				return error("short addressing mode forced but address is bigger than $3F");

			*am = M_DSPAA;
			*areg = (int)*AnEXVAL;     // Since this is only going to get used in dsp_ea_imm5...
			return OK;
		}
		else if (*tok == '>')
		{
			// Long Addressing Mode Force Operator
			tok++;

			// Immediate Short Addressing Mode Force Operator
			if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
				return ERROR;

			if (*AnEXATTR & DEFINED)
			{
				if (*AnEXVAL > 0xFFFFFF)
					return error("long address is bigger than $FFFFFF");
			}

			*am = M_DSPEA;
			*areg = DSP_EA_ABS;
			return OK;
		}

		if ((*areg = checkea(0, P_ERRORS)) != ERROR)
		{
			*am = M_DSPEA;
			return OK;
		}
		else
			return ERROR;
	}
	else if (*tok == SHL)
	{
		// I/O Short Addressing Mode Force Operator
		tok++;

		if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
			return ERROR;

		if (*AnEXVAL > 0xFFF)
			return error("I/O short addressing mode forced but address is bigger than $FFF");

		*am = M_DSPABS06;
		return OK;
	}
	else if (*tok == '<')
	{
		// Short Addressing Mode Force Operator
		tok++;

		if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
			return ERROR;

		if (*AnEXATTR & DEFINED)
		{
			if (*AnEXVAL > 0xFFF)
				return error("short addressing mode forced but address is bigger than $FFF");
		}

		*am = M_DSPABS12;
		return OK;
	}
	else if (*tok == '>')
	{
		// Long Addressing Mode Force Operator
		tok++;

		// Immediate Short Addressing Mode Force Operator
		if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
			return ERROR;

		if (*AnEXATTR & DEFINED)
		{
			if (*AnEXVAL > 0xFFFFFF)
				return error("long address is bigger than $FFFFFF");
		}

		*am = M_DSPEA;
		*areg = DSP_EA_ABS;
		return OK;
	}
	else if (*tok == REG56_PC || *tok == REG56_CCR || *tok == REG56_SR || *tok == REG56_SP || (*tok >= REG56_MR&&*tok <= REG56_SS))
	{
		*areg = *tok++;
		*am = M_DSPPCU;
		return OK;
	}
	// expr
	else
	{
		if (expr(AnEXPR, AnEXVAL, AnEXATTR, AnESYM) != OK)
			return ERROR;

		// We'll store M_DSPEA_ABS in areg and if we have
		// any extra info, it'll go in am
		if (*AnEXATTR & DEFINED)
		{
			*areg = DSP_EA_ABS;

			if (*AnEXVAL < 0x1000)
				*am = M_DSPABS12;
			else if (*AnEXVAL < 0x10000)
				*am = M_DSPABS16;
			else if (*AnEXVAL < 0x1000000)
				*am = M_DSPABS24;
			else
				return error("address must be smaller than $1000000");

			return OK;
		}
		else
		{
			// Well, we have no opinion on the expression's size, so let's assume the worst
			*areg = DSP_EA_ABS;
			*am = M_DSPABS24;
			return OK;
		}
	}

	return error("internal assembler error: Please report this error message: 'reached the end of dsp_parmode' with the line of code that caused it. Thanks, and sorry for the inconvenience");   // Something bad happened
}


//
// Parse all addressing modes except parallel moves
//
int dsp_amode(int maxea)
{
	LONG dummy;
	// Initialize global return values
	nmodes = dsp_a0reg = dsp_a1reg = 0;
	dsp_am0 = dsp_am1 = M_AM_NONE;
	dsp_a0expr[0] = dsp_a1expr[0] = ENDEXPR;
	dsp_a0exval = 0;
	dsp_a1exval = 0;
	dsp_a0exattr = dsp_a1exattr = 0;
	dsp_a0esym = dsp_a1esym = (SYM *)NULL;
	dsp_a0memspace = dsp_a1memspace = -1;
	dsp_a0perspace = dsp_a1perspace = -1;
	dsp_k = 0;

	// If at EOL, then no addr modes at all
	if (*tok == EOL)
		return 0;

	if (dsp_parmode(&dsp_am0, &dsp_a0reg, dsp_a0expr, &dsp_a0exval, &dsp_a0exattr, &dsp_a0esym, &dsp_a0memspace, &dsp_a0perspace, 0) == ERROR)
		return ERROR;


	// If caller wants only one mode, return just one (ignore comma);
	// If there is no second addressing mode (no comma), then return just one anyway.
	nmodes = 1;

	if (*tok != ',')
	{
		if (dsp_k != 0)
			return error("-x0/-x1/-y0/-y1 only allowed in multiply operations");

		return 1;
	}

	// Eat the comma
	tok++;

	// Parse second addressing mode
	if (dsp_parmode(&dsp_am1, &dsp_a1reg, dsp_a1expr, &dsp_a1exval, &dsp_a1exattr, &dsp_a1esym, &dsp_a1memspace, &dsp_a1perspace, 1) == ERROR)
		return ERROR;

	if (maxea == 2 || *tok == EOL)
	{
		if (dsp_k != 0)
			return error("-x0/-x1/-y0/-y1 only allowed in multiply operations");

		nmodes = 2;
		return 2;
	}

	if (*tok == ',')
	{
		// Only MAC-like or jsset/clr/tst/chg instructions here
		tok++;
		if (dsp_parmode(&dsp_am2, &dsp_a2reg, dsp_a2expr, &dsp_a2exval, &dsp_a2exattr, &dsp_a2esym, &dummy, &dummy, 2) == ERROR)
			return ERROR;
		if (maxea == 3)
			return 3;
		if (*tok != EOL)
			return error(extra_stuff);
		nmodes = 3;
		return 3;

	}

	// Only Tcc instructions here, and then only those that accept 4 operands

	if (dsp_parmode(&dsp_am2, &dsp_a2reg, dsp_a2expr, &dsp_a2exval, &dsp_a2exattr, &dsp_a2esym, &dummy, &dummy, 2) == ERROR)
		return ERROR;

	if (*tok++ != ',')
		return error("expected 4 parameters");

	if (dsp_parmode(&dsp_am3, &dsp_a3reg, dsp_a3expr, &dsp_a3exval, &dsp_a3exattr, &dsp_a3esym, &dummy, &dummy, 3) == ERROR)
		return ERROR;

	if (*tok == EOL)
	{
		if (dsp_k != 0)
			return error("-x0/-x1/-y0/-y1 only allowed in multiply operations");

		nmodes = 4;
		return 4;
	}
	else
	{
		// Tcc instructions do not support parallel moves, so any remaining tokens are garbage
		return error(extra_stuff);
	}

	return error("internal assembler error: Please report this error message: 'reached the end of dsp_amode' with the line of code that caused it. Thanks, and sorry for the inconvenience");   //Something bad happened
}


//
// Helper function which gives us the encoding of a DSP register
//
static inline int SDreg(int reg)
{
	if (reg >= REG56_X0 && reg <= REG56_N7)
		return reg & 0xFF;
	else if (reg >= REG56_A0&&reg <= REG56_A2)
		return (8 >> (reg & 7)) | 8;
	else //if (reg>=REG56_R0&&reg<=REG56_R7)
		return reg - REG56_R0 + 16;
	// Handy map for the above:
	// (values are of course taken from keytab)
	// Register | Value | Return value
	//	x0      | 260   | 4
	//	x1      | 261   | 5
	//	y0      | 262   | 6
	//	y1      | 263   | 7
	//	b0      | 265   | 8
	//	b2      | 267   | 9
	//	b1      | 269   | 10
	//	a       | 270   | 14
	//	b       | 271   | 15
	//	n0      | 280   | 24
	//	n1      | 281   | 25
	//	n2      | 282   | 26
	//	n3      | 283   | 27
	//	n4      | 284   | 28
	//	n5      | 285   | 29
	//	n6      | 286   | 30
	//	n7      | 287   | 31
	//	a0	    | 136   | 0
	//	a1	    | 137   | 1
	//	a2      | 138   | 2
	//	r0	    | 151   | 16
	//	r1	    | 152   | 17
	//	r2	    | 153   | 18
	//	r3	    | 154   | 19
	//	r4	    | 155   | 20
	//	r5	    | 156   | 21
	//	r6	    | 157   | 22
	//	r7	    | 158   | 23
}


//
// Check for X:Y: parallel mode syntax
//
static inline LONG check_x_y(LONG ea1, LONG S1)
{
	LONG inst;
	LONG eax_temp, eay_temp;
	LONG D1, D2, S2, ea2;
	LONG K_D1, K_D2;
	LONG w = 1 << 7;		// S1=0, D1=1<<14

	if ((ea1 & 0x38) == DSP_EA_POSTINC || (ea1 & 0x38) == DSP_EA_POSTINC1 ||
		(ea1 & 0x38) == DSP_EA_POSTDEC1 || (ea1 & 0x38) == DSP_EA_NOUPD)
	{
		switch (ea1 & 0x38)
		{
		case DSP_EA_POSTINC:  ea1 = (ea1 & (~0x38)) | 0x8; break;
		case DSP_EA_POSTINC1: ea1 = (ea1 & (~0x38)) | 0x18; break;
		case DSP_EA_POSTDEC1: ea1 = (ea1 & (~0x38)) | 0x10; break;
		case DSP_EA_NOUPD:    ea1 = (ea1 & (~0x38)) | 0x00; break;
		}

		if (S1 == 0)
		{
			// 'X:eax,D1 Y:eay,D2', 'X:eax,D1 S2,Y:eay'
			// Check for D1
			switch (K_D1 = *tok++)
			{
			case REG56_X0: D1 = 0 << 10; break;
			case REG56_X1: D1 = 1 << 10; break;
			case REG56_A:  D1 = 2 << 10; break;
			case REG56_B:  D1 = 3 << 10; break;
			default:    return error("unrecognised X:Y: parallel move syntax: expected x0, x1, a or b after 'X:eax,'");
			}
		}
		else
		{
			// 'S1,X:eax Y:eay,D2' 'S1,X:eax S2,Y:eay'
			w = 0;

			switch (S1)
			{
			case 4:  D1 = 0 << 10; break;
			case 5:  D1 = 1 << 10; break;
			case 14: D1 = 2 << 10; break;
			case 15: D1 = 3 << 10; break;
			default: return error("unrecognised X:Y: parallel move syntax: S1 can only be x0, x1, a or b in 'S1,X:eax'");
			}
		}

		if (*tok == REG56_Y)
		{
			tok++;
			// 'X:eax,D1 Y:eay,D2' 'S1,X:eax Y:eay,D2'
			if (*tok++ != ':')
				return error("unrecognised X:Y: parallel move syntax: expected ':' after 'X:ea,D1/S1,X:ea Y'");

			if (*tok++ == '(')
			{
				if (*tok >= REG56_R0 && *tok <= REG56_R7)
				{
					ea2 = (*tok++ - REG56_R0);

					if (((ea1 & 7) < 4 && ea2 < 4) || ((ea1 & 7) >= 4 && ea2 > 4))
						return error("unrecognised X:Y: parallel move syntax: eax and eay register banks must be different in 'X:ea,D1/S1,X:ea Y:eay,D2'");
				}
				else
					return error("unrecognised X:Y: parallel move syntax: expected 'Rn' after 'X:ea,D1/S1,X:ea Y:('");

				// If eax register is r0-r3 then eay register is r4-r7.
				// Encode that to 2 bits (i.e. eay value is 0-3)
				eax_temp = (ea2 & 3) << 5;  // Store register temporarily

				if (*tok++ != ')')
					return error("unrecognised X:Y: parallel move syntax: expected ')' after 'X:ea,D1/S1,X:ea Y:(Rn'");

				if (*tok == '+')
				{
					tok++;

					if (*tok == ',')
					{
						// (Rn)+
						ea2 = 3 << 12;
						tok++;
					}
					else if (*tok >= REG56_N0 && *tok <= REG56_N7)
					{
						// (Rn)+Nn
						if ((*tok++ & 7) != ea2)
							return error("unrecognised X:Y: parallel move syntax(Same register number expected for Rn, Nn in 'X:ea,D1/S1,X:ea Y:(Rn)+Nn,D')");

						ea2 = 1 << 12;

						if (*tok++ != ',')
							return error("unrecognised X:Y: parallel move syntax: expected ',' after 'X:ea,D1/S1,X:ea Y:(Rn)+Nn'");
					}
					else
						return error("unrecognised X:Y: parallel move syntax: expected ',' or 'Nn' after 'X:ea,D1/S1,X:ea Y:(Rn)+'");

				}
				else if (*tok == '-')
				{
					// (Rn)-
					ea2 = 2 << 12;
					tok++;

					if (*tok++ != ',')
						return error("unrecognised X:Y: parallel move syntax: expected ',' after 'X:ea,D1/S1,X:ea Y:(Rn)-'");
				}
				else if (*tok++ == ',')
				{
					// (Rn)
					ea2 = 0 << 12;
				}
				else
					return error("unrecognised X:Y: parallel move syntax: expected ',' after 'X:ea,D1/S1,X:ea Y:eay'");

				ea2 |= eax_temp; // OR eay back from temp

				switch (K_D2 = *tok++)
				{
				case REG56_Y0: D2 = 0 << 8; break;
				case REG56_Y1: D2 = 1 << 8; break;
				case REG56_A:  D2 = 2 << 8; break;
				case REG56_B:  D2 = 3 << 8; break;
				default:    return error("unrecognised X:Y: parallel move syntax: expected y0, y1, a or b after 'X:ea,D1/S1,X:ea Y:eay,'");
				}

				if (*tok != EOL)
					return error("unrecognised X:Y: parallel move syntax: expected end-of-line after 'X:ea,D1/S1,X:ea Y:eay,D'");

				if (S1 == 0)
					if (K_D1 == K_D2)
						return error("unrecognised X:Y: parallel move syntax: D1 and D2 cannot be the same in 'X:ea,D1 Y:eay,D2'");

				inst = 0b1100000000000000 | w;
				inst |= ea1 | D1 | ea2 | D2;
				return inst;
			}
			else
				return error("unrecognised X:Y: parallel move syntax: expected '(Rn)', '(Rn)+', '(Rn)-', '(Rn)+Nn' after 'X:ea,D1/S1,X:ea Y:'");
		}
		else if (*tok == REG56_Y0 || *tok == REG56_Y1 || *tok == REG56_A || *tok == REG56_B)
		{
			// 'X:eax,D1 S2,Y:eay' 'S1,X:eax1 S2,Y:eay'
			switch (*tok++)
			{
			case REG56_Y0: S2 = 0 << 8; break;
			case REG56_Y1: S2 = 1 << 8; break;
			case REG56_A:  S2 = 2 << 8; break;
			case REG56_B:  S2 = 3 << 8; break;
			default: return error("unrecognised X:Y: parallel move syntax: expected y0, y1, a or b after 'X:ea,D1/S1,X:ea Y:eay,'");
			}

			if (*tok++ != ',')
				return error("unrecognised X:Y: parallel move syntax: expected ',' after 'X:ea,D1/S1,X:ea S2'");

			if (*tok++ == REG56_Y)
			{
				// 'X:eax,D1 Y:eay,D2' 'S1,X:eax Y:eay,D2'
				if (*tok++ != ':')
					return error("unrecognised X:Y: parallel move syntax: expected ':' after 'X:ea,D1/S1,X:ea Y'");

				if (*tok++ == '(')
				{
					if (*tok >= REG56_R0 && *tok <= REG56_R7)
					{
						ea2 = (*tok++ - REG56_R0);

						if (((ea1 & 7) < 4 && ea2 < 4) || ((ea1 & 7) >= 4 && ea2 > 4))
							return error("unrecognised X:Y: parallel move syntax: eax and eay register banks must be different in 'X:ea,D1/S1,X:ea S2,Y:eay'");
					}
					else
						return error("unrecognised X:Y: parallel move syntax: expected 'Rn' after 'X:ea,D1/S1,X:ea S2,Y:('");
					// If eax register is r0-r3 then eay register is r4-r7.
					// Encode that to 2 bits (i.e. eay value is 0-3)
					eay_temp = (ea2 & 3) << 5; //Store register temporarily

					if (*tok++ != ')')
						return error("unrecognised X:Y: parallel move syntax: expected ')' after 'X:ea,D1/S1,X:ea S2,Y:(Rn'");

					if (*tok == '+')
					{
						tok++;

						if (*tok == EOL)
							// (Rn)+
							ea2 = 3 << 12;
						else if (*tok >= REG56_N0 && *tok <= REG56_N7)
						{
							// (Rn)+Nn
							if ((*tok++ & 7) != ea2)
								return error("unrecognised X:Y: parallel move syntax(Same register number expected for Rn, Nn in 'X:ea,D1/S1,X:ea S2,Y:(Rn)+Nn')");

							ea2 = 1 << 12;

							if (*tok != EOL)
								return error("unrecognised X:Y: parallel move syntax: expected end-of-line after 'X:ea,D1/S1,X:ea S2,Y:(Rn)+Nn'");
						}
						else
							return error("unrecognised X:Y: parallel move syntax: expected ',' or 'Nn' after 'X:ea,D1/S1,X:ea S2,Y:(Rn)+'");

					}
					else if (*tok == '-')
					{
						// (Rn)-
						ea2 = 2 << 12;
						tok++;
						if (*tok != EOL)
							return error("unrecognised X:Y: parallel move syntax: expected end-of-line after 'X:ea,D1/S1,X:ea S2,Y:(Rn)-'");
					}
					else if (*tok == EOL)
					{
						// (Rn)
						ea2 = 0 << 12;
					}
					else
						return error("unrecognised X:Y: parallel move syntax: expected end-of-line after 'X:ea,D1/S1,X:ea S2,Y:eay'");

					ea2 |= eay_temp; //OR eay back from temp

					inst = 0b1000000000000000 | w;
					inst |= (ea1 & 0x1f) | D1 | S2 | ea2;
					return inst;
				}
				else
					return error("unrecognised X:Y: parallel move syntax: expected '(Rn)', '(Rn)+', '(Rn)-', '(Rn)+Nn' after 'X:ea,D1/S1,X:ea Y:'");
			}
			else
				return error("unrecognised X:Y: parallel move syntax: expected '(Rn)', '(Rn)+', '(Rn)-', '(Rn)+Nn' in 'X:ea,D1/S1,X:ea'");
		}
		else
			return error("unrecognised X:Y: or X:R parallel move syntax: expected Y:, A or B after 'X:ea,D1/S1,X:ea S2,'");
	}

	return error("unrecognised X:Y: parallel move syntax: expected '(Rn)', '(Rn)+', '(Rn)-', '(Rn)+Nn' in 'X:ea,D1/S1,X:ea'");
}

//
// Parse X: addressing space parallel moves
//
static inline LONG parse_x(const int W, LONG inst, const LONG S1, const int check_for_x_y, WORD dest)
{
	int immreg;					// Immediate register destination
	LONG S2, D1, D2;			// Source and Destinations
	LONG ea1;					// ea bitfields
	uint32_t termchar = ',';	// Termination character for ea checks
	int force_imm = NUM_NORMAL;	// Holds forced immediate value (i.e. '<' or '>')
	ea1 = -1;					// initialise e1 (useful for some code paths)

	if (W == 0)
		termchar = EOL;

	if (*tok == '-')
	{
		if (tok[1] == CONST || tok[1] == FCONST)
		{
			tok++;
			dspImmedEXVAL = *tok++;
			goto x_check_immed;
		}

		// This could be either -(Rn), -aa or -ea. Check for immediate first
		if (tok[1] == SYMBOL)
		{
			if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) == OK)
			{
				if (S1 != 0)
				{
x_checkea_right:
					// 'S1,X:ea S2,D2', 'A,X:ea X0,A', 'B,X:ea X0,B', 'S1,X:eax Y:eay,D2', 'S1,X:eax S2,Y:eay'
					if (*tok == REG56_X0 && tok[1] == ',' && tok[2] == REG56_A)
					{
						// 'A,X:ea X0,A'
						if (ea1 == DSP_EA_ABS)
							deposit_extra_ea = DEPOSIT_EXTRA_WORD;

						if (S1 != 14)
							return error("unrecognised X:R parallel move syntax: S1 can only be a in 'a,X:ea x0,a'");

						if (S1 == 14 && ea1 == DSP_EA_ABS)
							return error("unrecognised X:R parallel move syntax: absolute address not allowed in 'a,X:ea x0,a'");

						if (ea1 == -1)
							return error("unrecognised X:R parallel move syntax: absolute address not allowed in 'a,X:ea x0,a'");

						if (ea1 == 0b00110100)
							return error("unrecognised X:R parallel move syntax: immediate data not allowed in 'a,X:ea x0,a'");

						inst = 0b0000100000000000 | ea1 | (0 << 8);
						return inst;
					}
					else if (*tok == REG56_X0 && tok[1] == ',' && tok[2] == REG56_B)
					{
						// 'B,X:ea X0,B'
						if (dest == REG56_B)
							return error("illegal X:R parallel move syntax: base instruction's destination register cannot be b in 'b,X:ea x0,b'");

						if (ea1 == DSP_EA_ABS)
							deposit_extra_ea = DEPOSIT_EXTRA_WORD;

						if (S1 != 15)
							return error("unrecognised X:R parallel move syntax: S1 can only be b in 'b,X:ea x0,b'");

						if (S1 == 15 && ea1 == DSP_EA_ABS)
							return error("unrecognised X:R parallel move syntax: absolute address not allowed in 'b,X:ea x0,b'");

						if (ea1 == -1)
							return error("unrecognised X:R parallel move syntax: absolute address not allowed in 'b,X:ea x0,b'");

						if (ea1 == 0b00110100)
							return error("unrecognised X:R parallel move syntax: immediate data not allowed in 'b,X:ea x0,b'");

						inst = 0b0000100100000000 | ea1 | (1 << 8);
						return inst;
					}
					else if (*tok == REG56_A || *tok == REG56_B)
					{
						// 'S1,X:ea S2,D2', 'S1,X:eax S2,Y:eay'
						switch (S1)
						{
						case 4:  D1 = 0 << 10; break;
						case 5:  D1 = 1 << 10; break;
						case 14: D1 = 2 << 10; break;
						case 15: D1 = 3 << 10; break;
						default: return error("unrecognised X:R parallel move syntax: S1 can only be x0, x1, a or b in 'S1,X:ea S2,D2'");
						}

						if (tok[1] == ',' && tok[2] == REG56_Y)
						{
							// 'S1,X:eax S2,Y:eay'
							return check_x_y(ea1, S1);
						}

						// 'S1,X:ea S2,D2'
						if (ea1 == DSP_EA_ABS)
							deposit_extra_ea = DEPOSIT_EXTRA_WORD;

						switch (*tok++)
						{
						case REG56_A: S2 = 0 << 9; break;
						case REG56_B: S2 = 1 << 9; break;
						default:   return error("unrecognised X:R parallel move syntax: expected a or b after 'S1,X:eax'");
						}

						if (*tok++ != ',')
							return error("unrecognised X:R parallel move syntax: expected ',' after 'S1,X:eax S2'");

						if (*tok == REG56_Y0 || *tok == REG56_Y1)
						{
							if (*tok++ == REG56_Y0)
								D2 = 0 << 8;
							else
								D2 = 1 << 8;

							if (*tok != EOL)
								return error("unrecognised X:R parallel move syntax: unexpected text after 'X:eax,D1 S2,S2'");

							inst = 0b0001000000000000 | (0 << 7);
							inst |= ea1 | D1 | S2 | D2;
							return inst;
						}
						else
							return error("unrecognised X:R parallel move syntax: expected y0 or y1 after 'X:eax,D1 S2,'");
					}
					else if (*tok == REG56_Y)
					{
						// 'S1,X:eax Y:eay,D2'
						return check_x_y(ea1, S1);
					}
					else if (*tok == REG56_Y0 || *tok == REG56_Y1)
					{
						// 'S1,X:eax S2,Y:eay'
						return check_x_y(ea1, S1);
					}
					else
						return error("unrecognised X:Y or X:R parallel move syntax: expected y0 or y1 after 'X:eax,D1/X:ea,D1");
				}
				else
				{
					// Only check for aa if we have a defined number in our hands or we've
					// been asked to use a short number format. The former case we'll just test
					// it to see if it's small enough. The later - it's the programmer's call
					// so he'd better have a small address or the fixups will bite him/her in the arse!
					if (dspImmedEXATTR&DEFINED || force_imm == NUM_FORCE_SHORT)
					{

						// It's an immediate, so ea or eax is probably an absolute address
						// (unless it's aa if the immediate is small enough)
						// 'X:ea,D' or 'X:aa,D' or 'X:ea,D1 S2,D2' or 'X:eax,D1 Y:eay,D2' or 'X:eax,D1 S2,Y:eay'
x_check_immed:
						// Check for aa (which is 6 bits zero extended)
						if (dspImmedEXVAL < 0x40 && force_imm != NUM_FORCE_LONG)
						{
							if (W == 1)
							{
								// It might be X:aa but we're not 100% sure yet.
								// If it is, the only possible syntax here is 'X:aa,D'.
								// So check ahead to see if EOL follows D, then we're good to go.
								if (*tok == ',' && ((*(tok + 1) >= REG56_X0 && *(tok + 1) <= REG56_N7) || (*(tok + 1) >= REG56_R0 && *(tok + 1) <= REG56_R7) || (*(tok + 1) >= REG56_A0 && *(tok + 1) <= REG56_A2)) && *(tok + 2) == EOL)
								{
									// Yup, we're good to go - 'X:aa,D' it is
									tok++;
									immreg = SDreg(*tok++);
									inst = inst | (uint32_t)dspImmedEXVAL;
									inst |= ((immreg & 0x18) << (12 - 3)) + ((immreg & 7) << 8);
									inst |= 1 << 7; // W
									return inst;
								}
							}
							else
							{
								if (*tok == EOL)
								{
									// 'S,X:aa'
									inst = inst | (uint32_t)dspImmedEXVAL;
									inst |= ((S1 & 0x18) << (12 - 3)) + ((S1 & 7) << 8);
									return inst;
								}
								else
								{
									// 'S1,X:ea S2,D2', 'A,X:ea X0,A', 'B,X:ea X0,B',
									// 'S1,X:eax Y:eay,D2', 'S1,X:eax S2,Y:eay'
									ea1 = DSP_EA_ABS;
									deposit_extra_ea = DEPOSIT_EXTRA_WORD;
									goto x_checkea_right;
								}
							}
						}
					}

					// Well, that settles it - we do have an ea in our hands
					if (W == 1)
					{
						// 'X:ea,D' [... S2,d2]
						if (*tok++ != ',')
							return error("unrecognised X: parallel move syntax: expected ',' after 'X:ea'");

						if ((*tok >= REG56_X0 && *tok <= REG56_N7) || (*tok >= REG56_R0 && *tok <= REG56_R7) || (*tok >= REG56_A0 && *tok <= REG56_A2))
						{
							D1 = SDreg(*tok++);

							if (*tok == EOL)
							{
								// 'X:ea,D'
								inst = inst | 0b01000000 | (1 << 7);
								inst |= ((D1 & 0x18) << (12 - 3)) + ((D1 & 7) << 8);
								inst |= ea1;

								if (ea1 == DSP_EA_ABS)
									deposit_extra_ea = DEPOSIT_EXTRA_WORD;

								return inst;
							}
							else
							{
								// 'X:ea,D1 S2,D2'
								if (*tok == REG56_A || *tok == REG56_B)
								{
									S2 = SDreg(*tok++);

									if (*tok++ != ',')
										return error("unrecognised X:R parallel move syntax: expected comma after X:ea,D1 S2");

									if (*tok == REG56_Y0 || *tok == REG56_Y1)
									{
										D2 = SDreg(*tok++);

										if (*tok != EOL)
											return error("unrecognised X:R parallel move syntax: expected EOL after X:ea,D1 S2,D2");

										inst = 0b0001000000000000 | (1 << 7);
										inst |= ((D1 & 0x8) << (12 - 4)) + ((D1 & 1) << 10);
										inst |= (S2 & 1) << 9;
										inst |= (D2 & 1) << 8;
										inst |= ea1;
										return inst;
									}
									else
										return error("unrecognised X:R parallel move syntax: expected y0,y1 after X:ea,D1 S2,");
								}
								else
									return error("unrecognised X:R parallel move syntax: expected a,b after X:ea,D1");
							}
						}
						else
							return error("unrecognised X: parallel move syntax: expected x0,x1,y0,y1,a0,b0,a2,b2,a1,b1,a,b,r0-r7,n0-n7 after 'X:ea,'");
					}
					else
					{
						if (*tok == EOL)
						{
							// 'S,X:ea'
							inst = inst | 0b01000000 | (0 << 7);
							inst |= ((S1 & 0x18) << (12 - 3)) + ((S1 & 7) << 8);
							inst |= ea1;

							if (ea1 == DSP_EA_ABS)
								deposit_extra_ea = DEPOSIT_EXTRA_WORD;

							return inst;
						}
						else
						{
							// 'S1,X:ea S2,D2', 'A,X:ea X0,A', 'B,X:ea X0,B',
							// 'S1,X:eax Y:eay,D2', 'S1,X:eax S2,Y:eay'
							goto x_checkea_right;
						}
					}

				}
			}
		}
		else
		{
			// It's not an immediate, check for '-(Rn)'
			ea1 = checkea(termchar, X_ERRORS);

			if (ea1 == ERROR)
				return ERROR;

			goto x_gotea1;

		}
	}
	else if (*tok == '#')
	{
		tok++;

		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		// Okay so we have immediate data - mark it down
		ea1 = DSP_EA_IMM;
		// Now, proceed to the main code for this branch
		goto x_gotea1;
	}
	else if (*tok == '(')
	{
		// Maybe we got an expression here, check for it
		if (tok[1] == CONST || tok[1] == FCONST || tok[1] == SUNARY || tok[1] == SYMBOL || tok[1] == STRING)
		{
			// Evaluate the expression and go to immediate code path
			expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM);
			goto x_check_immed;
		}

		// Nope, let's check for ea then
		ea1 = checkea(termchar, X_ERRORS);

		if (ea1 == ERROR)
			return ERROR;

x_gotea1:
		if (W == 1)
		{
			if (*tok++ != ',')
				return error("Comma expected after 'X:(Rn)')");

			// It might be 'X:(Rn..)..,D' but we're not 100% sure yet.
			// If it is, the only possible syntax here is 'X:ea,D'.
			// So check ahead to see if EOL follows D, then we're good to go.
			if (((*tok >= REG56_X0 && *tok <= REG56_N7) || (*tok >= REG56_R0 && *tok <= REG56_R7) || (*tok >= REG56_A0 && *tok <= REG56_A2)) && *(tok + 1) == EOL)
			{
				//'X:ea,D'
				D1 = SDreg(*tok++);

				inst = inst | 0b01000000 | (1 << 7);
				inst |= ea1;
				inst |= ((D1 & 0x18) << (12 - 3)) + ((D1 & 7) << 8);
				return inst;
			}
		}
		else
		{
			if (*tok == EOL)
			{
				//'S,X:ea'
				inst = inst | 0b01000000 | (0 << 7);
				inst |= ea1;
				inst |= ((S1 & 0x18) << (12 - 3)) + ((S1 & 7) << 8);
				return inst;
			}
			else
			{
				goto x_checkea_right;
			}
		}

		// 'X:eax,D1 Y:eay,D2', 'X:eax,D1 S2,Y:eay' or 'X:ea,D1 S2,D2'
		// Check ahead for S2,D2 - if that's true then we have 'X:ea,D1 S2,D2'
		if ((*tok == REG56_X0 || *tok == REG56_X1 || *tok == REG56_A || *tok == REG56_B) && (*(tok + 1) == REG56_A || *(tok + 1) == REG56_B) && (*(tok + 2) == ',') && (*(tok + 3) == REG56_Y0 || (*(tok + 3) == REG56_Y1)))
		{
			// 'X:ea,D1 S2,D2'
			// Check if D1 is x0, x1, a or b
			switch (*tok++)
			{
			case REG56_X0: D1 = 0 << 10; break;
			case REG56_X1: D1 = 1 << 10; break;
			case REG56_A:  D1 = 2 << 10; break;
			case REG56_B:  D1 = 3 << 10; break;
			default:    return error("unrecognised X:R parallel move syntax: expected x0, x1, a or b after 'X:eax,'");
			}

			switch (*tok++)
			{
			case REG56_A: S2 = 0 << 9; break;
			case REG56_B: S2 = 1 << 9; break;
			default:   return error("unrecognised X:R parallel move syntax: expected a or b after 'X:eax,D1 '");
			}

			if (*tok++ != ',')
				return error("unrecognised X:R parallel move syntax: expected ',' after 'X:eax,D1 S2'");

			if (*tok == REG56_Y0 || *tok == REG56_Y1)
			{
				if (*tok++ == REG56_Y0)
					D2 = 0 << 8;
				else
					D2 = 1 << 8;

				if (*tok != EOL)
					return error("unrecognised X:R parallel move syntax: unexpected text after 'X:eax,D1 S2,S2'");

				inst = 0b0001000000000000 | (W << 7);
				inst |= ea1 | D1 | S2 | D2;
				return inst;
			}
			else
				return error("unrecognised X:R parallel move syntax: expected y0 or y1 after 'X:eax,D1 S2,'");
		}

		// Check to see if we got eax (which is a subset of ea)
		if (check_for_x_y)
		{
			if ((inst = check_x_y(ea1, 0)) != 0)
				return inst;
			else
			{
				// Rewind pointer as it might be an expression and check for it
				tok--;

				if (expr(dspaaEXPR, &dspaaEXVAL, &dspaaEXATTR, &dspaaESYM) != OK)
					return ERROR;

				// Yes, we have an expression, so we now check for
				// 'X:ea,D' or 'X:aa,D' or 'X:ea,D1 S2,D2' or 'X:eax,D1 Y:eay,D2' or 'X:eax,D1 S2,Y:eay'
				goto x_check_immed;
			}
		}
	}
	else if (*tok == CONST || *tok == FCONST || *tok == SYMBOL)
	{
		// Check for immediate address
		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		// We set ea1 here - if it's aa instead of ea
		// then it won't be used anyway
		ea1 = DSP_EA_ABS;

		if (!(dspImmedEXATTR&DEFINED))
		{
			force_imm = NUM_FORCE_LONG;
			deposit_extra_ea = DEPOSIT_EXTRA_WORD;
		}

		goto x_check_immed;
	}
	else if (*tok == '>')
	{
		// Check for immediate address forced long
		tok++;

		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		if ((dspImmedEXATTR & DEFINED) && (dspImmedEXVAL > 0xFFFFFF))
			return error("long address is bigger than $FFFFFF");

		deposit_extra_ea = DEPOSIT_EXTRA_WORD;

		force_imm = NUM_FORCE_LONG;
		ea1 = DSP_EA_ABS;
		goto x_check_immed;
	}
	else if (*tok == '<')
	{
		// Check for immediate address forced short
		tok++;

		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		force_imm = NUM_FORCE_SHORT;

		if (dspImmedEXATTR & DEFINED)
		{
			if (dspImmedEXVAL > 0x3F)
			{
				if (CHECK_OPTS(OPT_56K_AUTO_LONG))
				{
					if (optim_warn_flag)
						warn("o11: short addressing mode forced but address is bigger than $3F - switching to long");

					force_imm = NUM_FORCE_LONG;
					deposit_extra_ea = DEPOSIT_EXTRA_WORD;
					ea1 = DSP_EA_ABS;
				}
				else
				{
					return error("short addressing mode forced but address is bigger than $3F - turn opt switch o11 on to bypass");
				}
			}
		}
		else
		{
			// This might end up as something like 'move Y:<adr,register'
			// so let's mark it as an extra aa fixup here.
			// Note: we are branching to x_check_immed without a
			// defined dspImmed so it's going to be 0. It probably
			// doesn't harm anything.
			deposit_extra_ea = DEPOSIT_EXTRA_FIXUP;
		}

		goto x_check_immed;
	}

	return error("unknown x: addressing mode");
}


//
// Parse Y: addressing space parallel moves
//
static inline LONG parse_y(LONG inst, LONG S1, LONG D1, LONG S2, WORD dest)
{
    int immreg;					// Immediate register destination
    LONG D2;					// Destination
    LONG ea1;					// ea bitfields
    int force_imm = NUM_NORMAL;	// Holds forced immediate value (i.e. '<' or '>')
    if (*tok == '-')
    {
        if (tok[1] == CONST || tok[1] == FCONST)
        {
            tok++;
            dspImmedEXVAL = *tok++;
            goto y_check_immed;
        }
        // This could be either -(Rn), -aa or -ea. Check for immediate first
		if (*tok == SYMBOL || tok[1] == SYMBOL)
		{
			if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) == OK)
			{
				// Only check for aa if we have a defined number in our hands or we've
				// been asked to use a short number format. The former case we'll just test
				// it to see if it's small enough. The later - it's the programmer's call
				// so he'd better have a small address or the fixups will bite him/her in the arse!
				if (dspImmedEXATTR&DEFINED || force_imm == NUM_FORCE_SHORT)
				{
					// It's an immediate, so ea is probably an absolute address
					// (unless it's aa if the immediate is small enough)
					// 'Y:ea,D', 'Y:aa,D', 'S,Y:ea' or 'S,Y:aa'
				y_check_immed:
					// Check for aa (which is 6 bits zero extended)
					if (dspImmedEXVAL < 0x40 && force_imm != NUM_FORCE_LONG)
					{
						// It might be Y:aa but we're not 100% sure yet.
						// If it is, the only possible syntax here is 'Y:aa,D'/'S,Y:aa'.
						// So check ahead to see if EOL follows D, then we're good to go.
						if (*tok == EOL && S1 != 0)
						{
							// 'S,Y:aa'
							inst = 0b0100100000000000;
							inst |= dspImmedEXVAL;
							inst |= ((S1 & 0x18) << (12 - 3)) + ((S1 & 7) << 8);
							return inst;
						}

						if (*tok == ',' && ((*(tok + 1) >= REG56_X0 && *(tok + 1) <= REG56_N7) || (*(tok + 1) >= REG56_R0 && *(tok + 1) <= REG56_R7) || (*(tok + 1) >= REG56_A0 && *(tok + 1) <= REG56_A2)) && *(tok + 2) == EOL)
						{
							// Yup, we're good to go - 'Y:aa,D' it is
							tok++;
							immreg = SDreg(*tok++);
							inst |= dspImmedEXVAL;
							inst |= ((immreg & 0x18) << (12 - 3)) + ((immreg & 7) << 8);
							return inst;
						}
					}
				}
				// Well, that settles it - we do have a ea in our hands
				if (*tok == EOL && S1 != 0)
				{
					// 'S,Y:ea'
					inst = 0b0100100001110000;
					inst |= ea1;
					inst |= ((S1 & 0x18) << (12 - 3)) + ((S1 & 7) << 8);
					if (ea1 == DSP_EA_ABS)
						deposit_extra_ea = DEPOSIT_EXTRA_WORD;
					return inst;
				}

				if (*tok++ != ',')
					return error("unrecognised Y: parallel move syntax: expected ',' after 'Y:ea'");

				if (D1 == 0 && S1 == 0)
				{
					// 'Y:ea,D'
					if ((*tok >= REG56_X0 && *tok <= REG56_N7) || (*tok >= REG56_R0 && *tok <= REG56_R7) || (*tok >= REG56_A0 && *tok <= REG56_A2))
					{
						D1 = SDreg(*tok++);

						if (*tok != EOL)
							return error("unrecognised Y: parallel move syntax: expected EOL after 'Y:ea,D'");

						inst |= 0b0000000001110000;
						inst |= ea1;
						inst |= ((D1 & 0x18) << (12 - 3)) + ((D1 & 7) << 8);
						if (ea1 == DSP_EA_ABS)
							deposit_extra_ea = DEPOSIT_EXTRA_WORD;
						return inst;
					}
					else
						return error("unrecognised Y: parallel move syntax: expected x0,x1,y0,y1,a0,b0,a2,b2,a1,b1,a,b,r0-r7,n0-n7 after 'Y:ea,'");
				}
				else
				{
					// 'S1,D1 Y:ea,D2'
					if (*tok == REG56_A || *tok == REG56_B || *tok == REG56_Y0 || *tok == REG56_Y1)
					{
						D2 = SDreg(*tok++);
						inst |= ea1;
						inst |= 1 << 7;
						inst |= (S1 & 1) << 11;
						inst |= (D1 & 1) << 10;
						inst |= (D2 & 8) << (9 - 3);
						inst |= (D2 & 1) << 8;
						return inst;
					}
					else
						return error("unrecognised R:Y: parallel move syntax: expected a,b after 'S1,D1 Y:ea,'");
				}
			}
			else
				return ERROR;
		}
		else
		{
			// It's not an immediate, check for '-(Rn)'
			ea1 = checkea(',', Y_ERRORS);

			if (ea1 == ERROR)
				return ERROR;

			goto y_gotea1;

		}
	}
	else if (*tok == '#')
	{
		tok++;

		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		// Okay so we have immediate data - mark it down
		ea1 = DSP_EA_IMM;
		// Now, proceed to the main code for this branch
		goto y_gotea1;
	}
	else if (*tok == '(')
	{
		// Maybe we got an expression here, check for it
		if (tok[1] == CONST || tok[1] == FCONST || tok[1] == SUNARY || tok[1] == SYMBOL || tok[1] == STRING)
		{
			// Evaluate the expression and go to immediate code path
			expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM);
			goto y_check_immed;
		}

		// Nope, let's check for ea then
		if (S1 == 0 || (S1 != 0 && D1 != 0))
			ea1 = checkea(',', Y_ERRORS);
		else
			ea1 = checkea(EOL, Y_ERRORS);

		if (ea1 == ERROR)
			return ERROR;

	y_gotea1:
		if (S1 != 0 && *tok == EOL)
		{
			// 'S,Y:ea'
			inst = 0b0100100001000000;
			inst |= ea1;
			inst |= ((S1 & 0x18) << (12 - 3)) + ((S1 & 7) << 8);
			return inst;
		}
		else if (S1 != 0 && D1 != 0 && S2 == 0)
		{
			// 'S1,D1 Y:ea,D2'
			switch (S1)
			{
			case 14: S1 = 0 << 11; break; // A
			case 15: S1 = 1 << 11; break; // B
			default: return error("unrecognised R:Y parallel move syntax: S1 can only be A or B in 'S1,D1 Y:ea,D2'"); break;
			}

			switch (D1)
			{
			case 4: D1 = 0 << 10; break; // X0
			case 5: D1 = 1 << 10; break; // X1
			default: return error("unrecognised R:Y parallel move syntax: D1 can only be x0 or x1 in 'S1,D1 Y:ea,D2'");break;
			}

			if (*tok++ != ',')
				return error("unrecognised R:Y parallel move syntax: expected ',' after 'S1,D1 Y:ea'");

			switch (*tok++)
			{
			case REG56_Y0: D2 = 0 << 8; break;
			case REG56_Y1: D2 = 1 << 8; break;
			case REG56_A:  D2 = 2 << 8; break;
			case REG56_B:  D2 = 3 << 8; break;
			default: return error("unrecognised R:Y parallel move syntax: D2 can only be y0, y1, a or b after 'S1,D1 Y:ea'");
			}

			inst = 0b0001000011000000;
			inst |= S1 | D1 | D2;
			inst |= ea1;
			return inst;
		}

		if (*tok++ != ',')
            return error("Comma expected after 'Y:(Rn)')");

		// It might be 'Y:(Rn..)..,D' but we're not 100% sure yet.
		// If it is, the only possible syntax here is 'Y:ea,D'.
		// So check ahead to see if EOL follows D, then we're good to go.
		if (((*tok >= REG56_X0 && *tok <= REG56_N7) || (*tok >= REG56_R0 && *tok <= REG56_R7) || (*tok >= REG56_A0 && *tok <= REG56_A2)) && *(tok + 1) == EOL)
		{
			//'Y:ea,D'
			D1 = SDreg(*tok++);
			
			if (dest && dest == REG56_A && (D1 == 14 || D1 == 0 || D1 == 1 || D1 == 2))
				return error("illegal R:Y parallel move syntax: cannot have base instruction's destination register to be a and D in 'Y:ea,D' to be a, a0, a1 or a2");

			if (dest && dest == REG56_B && (D1 == 15 || D1 == 8 || D1 == 9 || D1 == 10))
				return error("illegal R:Y parallel move syntax: cannot have base instruction's destination register to be a and D in 'Y:ea,D' to be a, a0, a1 or a2");

			inst |= 0b0000000001000000;
			inst |= ea1;
			inst |= ((D1 & 0x18) << (12 - 3)) + ((D1 & 7) << 8);
			return inst;
		}
	}
	else if (*tok == CONST || *tok == FCONST || *tok == SYMBOL)
	{
		// Check for immediate address
		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		// Yes, we have an expression, so we now check for
		// 'Y:ea,D' or 'Y:aa,D'
		ea1 = DSP_EA_ABS; // Reluctant - but it's probably correct

		if (!(dspImmedEXATTR&DEFINED))
		{
			force_imm = NUM_FORCE_LONG;
			deposit_extra_ea = DEPOSIT_EXTRA_WORD;
		}

		goto y_check_immed;
	}
	else if (*tok == '>')
	{
		// Check for immediate address forced long
		tok++;

		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		if ((dspImmedEXATTR & DEFINED) && (dspImmedEXVAL > 0xFFFFFF))
			return error("long address is bigger than $FFFFFF");

		deposit_extra_ea = DEPOSIT_EXTRA_WORD;

		force_imm = NUM_FORCE_LONG;
		ea1 = DSP_EA_ABS;
		goto y_check_immed;
	}
	else if (*tok == '<')
	{
		tok++;

		if (S1 != 0 && D1 != 0)
		{
			// We're in 'S1,D1 Y:ea,D2' or 'S1,D1 S1,Y:ea'
			// there's no Y:aa mode here, so we'll force long
			if (CHECK_OPTS(OPT_56K_AUTO_LONG))
			{
				if (optim_warn_flag)
					warn("forced short addressing in R:Y mode is not allowed - switching to long");

				if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
					return ERROR;

				ea1 = DSP_EA_ABS;

				force_imm = NUM_FORCE_LONG;
				deposit_extra_ea = DEPOSIT_EXTRA_WORD;
				goto y_check_immed;
			}
			else
			{
				return error("forced short addressing in R:Y mode is not allowed - turn opt switch o11 on to bypass");
			}
		}
		else
		{
			// Check for immediate address forced short
			ea1 = DSP_EA_ABS; // Reluctant - but it's probably correct

			if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
				return ERROR;

			force_imm = NUM_FORCE_SHORT;

			if (dspImmedEXATTR & DEFINED)
			{
				if (dspImmedEXVAL > 0xFFF)
				{
					if (CHECK_OPTS(OPT_56K_AUTO_LONG))
					{
						if (optim_warn_flag)
							warn("short addressing mode forced but address is bigger than $FFF - switching to long");

						ea1 = DSP_EA_ABS;
						force_imm = NUM_FORCE_LONG;
						deposit_extra_ea = DEPOSIT_EXTRA_WORD;
					}
					else
					{
						return error("short addressing mode forced but address is bigger than $FFF - turn opt switch o11 on to bypass");
					}
				}
			}
			else
			{
				// This might end up as something like 'move Y:<adr,register'
				// so let's mark it as an extra aa fixup here.
				// Note: we are branching to y_check_immed without a
				// defined dspImmed so it's going to be 0. It probably
				// doesn't harm anything.
				deposit_extra_ea = DEPOSIT_EXTRA_FIXUP;
			}

			goto y_check_immed;
		}
	}

	return error("unrecognised Y: parallel move syntax");
}


//
// Parse L: addressing space parallel moves
//
static inline LONG parse_l(const int W, LONG inst, LONG S1)
{
	int immreg;					// Immediate register destination
	LONG D1;					// Source and Destinations
	LONG ea1;					// ea bitfields
	int force_imm = NUM_NORMAL;	// Holds forced immediate value (i.e. '<' or '>')

	if (*tok == '-')
	{
		if (*tok == CONST || tok[1] == FCONST)
		{
			tok++;
			dspImmedEXVAL = *tok++;
			goto l_check_immed;
		}

		// This could be either -(Rn), -aa or -ea. Check for immediate first
		// Maybe we got an expression here, check for it
		if (*tok == SYMBOL || tok[1] == SYMBOL)
		{
			// Evaluate the expression and go to immediate code path
			if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) == OK)
			{
				// Only check for aa if we have a defined number in our hands
				// or we've been asked to use a short number format. The
				// former case we'll just test it to see if it's small enough.
				// The later - it's the programmer's call so he'd better have
				// a small address or the fixups will bite him/her in the arse!
				if (dspImmedEXATTR&DEFINED || force_imm == NUM_FORCE_SHORT)
				{
					// It's an immediate, so ea is probably an absolute address
					// (unless it's aa if the immediate is small enough)
					// 'L:ea,D' or 'L:aa,D'
				l_check_immed:
					// Check for aa (which is 6 bits zero extended)
					if (*tok == EOL)
					{
						// 'S,L:aa'
						if (dspImmedEXVAL < 0x40 && force_imm != NUM_FORCE_LONG)
						{
							// 'S,L:aa'
							if (S1 == REG56_A)
								S1 = 4;
							else if (S1 == REG56_B)
								S1 = 5;
							else
								S1 &= 7;

							inst = 0b0100000000000000;
							inst |= dspImmedEXVAL;
							inst |= ((S1 & 0x4) << (11 - 2)) + ((S1 & 3) << 8);
							return inst;
						}
						else
						{
							// 'S,L:ea'
							if (S1 == REG56_A)
								S1 = 4;
							else if (S1 == REG56_B)
								S1 = 5;
							else
								S1 &= 7;

							if (ea1 == DSP_EA_ABS)
								deposit_extra_ea = DEPOSIT_EXTRA_WORD;

							inst |= 0b0100000001110000;
							inst |= ((S1 & 0x4) << (11 - 2)) + ((S1 & 3) << 8);
							inst |= ea1;
							return inst;
						}

					}

					if (*tok++ != ',')
						return error("unrecognised L: parallel move syntax: expected ',' after 'L:ea/L:aa'");

					// Check for allowed registers for D (a0, b0, x, y, a, b, ab or ba)
					if (!((*tok >= REG56_A10 && *(tok + 1) <= REG56_BA) || (*tok >= REG56_A && *tok <= REG56_B)))
						return error("unrecognised L: parallel move syntax: expected a0, b0, x, y, a, b, ab or ba after 'L:ea/L:aa'");

					if (dspImmedEXVAL < (1 << 6) && (dspImmedEXATTR&DEFINED))
					{
						// 'L:aa,D'
						l_aa:
						immreg = *tok++;

						if (immreg == REG56_A)
							immreg = 4;
						else if (immreg == REG56_B)
							immreg = 5;
						else
							immreg &= 7;

						if (*tok != EOL)
							return error("unrecognised L: parallel move syntax: expected End-Of-Line after L:aa,D");

						inst &= 0b1111111110111111;
						inst |= dspImmedEXVAL;
						inst |= ((immreg & 0x4) << (11 - 2)) + ((immreg & 3) << 8);
						return inst;
					}
				}

				if (deposit_extra_ea == DEPOSIT_EXTRA_FIXUP)
				{
					// Hang on, we've got a L:<aa here, let's do that instead
					goto l_aa;
				}

				// Well, that settles it - we do have a ea in our hands
				// 'L:ea,D'
				D1 = *tok++;

				if (D1 == REG56_A)
					D1 = 4;
				else if (D1 == REG56_B)
					D1 = 5;
				else
					D1 &= 7;

				if (*tok != EOL)
					return error("unrecognised L: parallel move syntax: expected End-Of-Line after L:ea,D");

				inst |= 0b0000000000110000;
				inst |= ((D1 & 0x4) << (11 - 2)) + ((D1 & 3) << 8);
				return inst;
			}
		}
		else
		{
			//It's not an immediate, check for '-(Rn)'
			ea1 = checkea(',', L_ERRORS);

			if (ea1 == ERROR)
				return ERROR;

			goto l_gotea1;

		}
	}
	else if (*tok == '(')
	{
		// Maybe we got an expression here, check for it
		if (tok[1] == CONST || tok[1] == FCONST || tok[1] == SUNARY || tok[1] == SYMBOL || tok[1] == STRING)
		{
			// Evaluate the expression and go to immediate code path
			expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM);
			goto l_check_immed;
		}

		//Nope, let's check for ea then
		if (S1 == 0)
			ea1 = checkea(',', L_ERRORS);
		else
			ea1 = checkea(EOL, L_ERRORS);

		if (ea1 == ERROR)
			return ERROR;

	l_gotea1:
		if (*tok == EOL)
		{
			// 'S,L:ea'
			inst = 0b0100000001000000;

			if (S1 == REG56_A)
				S1 = 4;
			else if (S1 == REG56_B)
				S1 = 5;
			else
				S1 &= 7;

			inst |= ea1;
			inst |= ((S1 & 0x4) << (11 - 2)) + ((S1 & 3) << 8);
			return inst;
		}
		else if (*tok++ != ',')
			return error("Comma expected after 'L:(Rn)')");

		// It might be 'L:(Rn..)..,D' but we're not 100% sure yet.
		// If it is, the only possible syntax here is 'L:ea,D'.
		// So check ahead to see if EOL follows D, then we're good to go.
		if (((*tok >= REG56_A10 && *tok <= REG56_BA) || (*tok >= REG56_A && *tok <= REG56_B)) && *(tok + 1) == EOL)
		{
			//'L:ea,D'
			D1 = *tok++;

			if (D1 == REG56_A)
				D1 = 4;
			else if (D1 == REG56_B)
				D1 = 5;
			else
				D1 &= 7;

			inst |= ea1;
			inst |= ((D1 & 0x4) << (11 - 2)) + ((D1 & 3) << 8);
			return inst;
		}
	}
	else if (*tok == CONST || *tok == FCONST || *tok == SYMBOL)
	{
		// Check for immediate address
		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		// We set ea1 here - if it's aa instead of ea
		// then it won't be used anyway
		ea1 = DSP_EA_ABS;

		if (!(dspImmedEXATTR & DEFINED))
		{
			force_imm = NUM_FORCE_LONG;
			deposit_extra_ea = DEPOSIT_EXTRA_WORD;
		}
		else if (dspImmedEXVAL > 0x3f)
		{
			// Definitely no aa material, so it's going to be a long
			// Mark that we need to deposit an extra word
			deposit_extra_ea = DEPOSIT_EXTRA_WORD;
		}

		// Yes, we have an expression, so we now check for
		// 'L:ea,D' or 'L:aa,D'
		goto l_check_immed;
	}
	else if (*tok == '>')
	{
		// Check for immediate address forced long
		tok++;

		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		if ((dspImmedEXATTR & DEFINED) && (dspImmedEXVAL > 0xFFFFFF))
			return error("long address is bigger than $FFFFFF");

		deposit_extra_ea = DEPOSIT_EXTRA_WORD;

		force_imm = NUM_FORCE_LONG;
		goto l_check_immed;
	}
	else if (*tok == '<')
	{
		// Check for immediate address forced short
		tok++;

		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		if (dspImmedEXATTR & DEFINED)
		{
			if (dspImmedEXVAL > 0xFFF)
				return error("short addressing mode forced but address is bigger than $FFF");
		}
		else
		{
			// This might end up as something like 'move Y:<adr,register'
			// so let's mark it as an extra aa fixup here.
			// Note: we are branching to l_check_immed without a
			// defined dspImmed so it's going to be 0. It probably
			// doesn't harm anything.
			deposit_extra_ea = DEPOSIT_EXTRA_FIXUP;
		}

		force_imm = NUM_FORCE_SHORT;
		goto l_check_immed;
	}

	return error("internal assembler error: Please report this error message: 'reached the end of parse_l' with the line of code that caused it. Thanks, and sorry for the inconvenience");
}


//
// Checks for all ea cases where indexed addressing is concenred
//
static inline LONG checkea(const uint32_t termchar, const int strings)
{
	LONG ea;

	if (*tok == '-')
	{
		// -(Rn)
		tok++;

		if (*tok++ != '(')
			return error(ea_errors[strings][0]);

		if (*tok >= REG56_R0 && *tok <= REG56_R7)
		{
			// We got '-(Rn' so mark it down
			ea = DSP_EA_PREDEC1 | (*tok++ - REG56_R0);

			if (*tok++ != ')')
				return error(ea_errors[strings][1]);

			// Now, proceed to the main code for this branch
			return ea;
		}
		else
			return error(ea_errors[strings][2]);
	}
	else if (*tok == '(')
	{
		// Checking for ea of type (Rn)
		tok++;

		if (*tok >= REG56_R0 && *tok <= REG56_R7)
		{
			// We're in 'X:(Rn..)..,D', 'X:(Rn..)..,D1 Y:eay,D2', 'X:(Rn..)..,D1 S2,Y:eay'
			ea = *tok++ - REG56_R0;

			if (*tok == '+')
			{
				// '(Rn+Nn)'
				tok++;

				if (*tok < REG56_N0 || *tok > REG56_N7)
					return error(ea_errors[strings][3]);

				if ((*tok++ & 7) != ea)
					return error(ea_errors[strings][4]);

				ea |= DSP_EA_INDEX;

				if (*tok++ != ')')
					return error(ea_errors[strings][5]);

				return ea;
			}
			else if (*tok == ')')
			{
				// Check to see if we have '(Rn)+', '(Rn)-', '(Rn)-Nn', '(Rn)+Nn' or '(Rn)'
				tok++;

				if (*tok == '+')
				{
					tok++;

					if (termchar == ',')
					{
						if (*tok == ',')
						{
							// (Rn)+
							ea |= DSP_EA_POSTINC1;
							return ea;
						}
						else if (*tok >= REG56_N0 && *tok <= REG56_N7)
						{
							// (Rn)+Nn
							if ((*tok++ & 7) != ea)
								return error(ea_errors[strings][6]);

							ea |= DSP_EA_POSTINC;
							return ea;
						}
						else
							return error(ea_errors[strings][7]);
					}
					else
					{
						if (*tok >= REG56_N0 && *tok <= REG56_N7)
						{
							// (Rn)+Nn
							if ((*tok++ & 7) != ea)
								return error(ea_errors[strings][6]);

							ea |= DSP_EA_POSTINC;
							return ea;
						}
						else
						{
							// (Rn)+
							ea |= DSP_EA_POSTINC1;
							return ea;
						}
					}
				}
				else if (*tok == '-')
				{
					tok++;

					if (termchar == ',')
					{
						if (*tok == ',')
						{
							// (Rn)-
							ea |= DSP_EA_POSTDEC1;
							return ea;
						}
						else if (*tok >= REG56_N0 && *tok <= REG56_N7)
						{
							// (Rn)-Nn
							if ((*tok++ & 7) != ea)
								return error(ea_errors[strings][8]);

							ea |= DSP_EA_POSTDEC;
							return ea;
						}
						else
							return error(ea_errors[strings][9]);
					}
					else
					{
						if (*tok >= REG56_N0 && *tok <= REG56_N7)
						{
							// (Rn)-Nn
							if ((*tok++ & 7) != ea)
								return error(ea_errors[strings][8]);

							ea |= DSP_EA_POSTDEC;
							return ea;
						}
						else
						{
							// (Rn)-
							ea |= DSP_EA_POSTDEC1;
							return ea;
						}
					}
				}
				else if (termchar == ',')
				{
					if (*tok == ',')
					{
						// (Rn)
						ea |= DSP_EA_NOUPD;
						return ea;
					}
					else
						return error(ea_errors[strings][10]);
				}
				else
				{
					// (Rn)
					ea |= DSP_EA_NOUPD;
					return ea;
				}
			}
			else
				return error(ea_errors[strings][11]);
		}
	}

	return error("internal assembler error: Please report this error message: 'reached the end of checkea' with the line of code that caused it. Thanks, and sorry for the inconvenience");
}


//
// Checks for all ea cases, i.e. all addressing modes that checkea handles
// plus immediate addresses included forced short/long ones.
// In other words this is a superset of checkea (and in fact calls checkea).
//
LONG checkea_full(const uint32_t termchar, const int strings)
{
	LONG ea1;

	if (*tok == CONST || *tok == FCONST || *tok == SYMBOL)
	{
		// Check for immediate address
		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		deposit_extra_ea = DEPOSIT_EXTRA_WORD;

		// Yes, we have an expression
		return DSP_EA_ABS;
	}
	else if (*tok == '>')
	{
		// Check for immediate address forced long
		tok++;

		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		if ((dspImmedEXATTR & DEFINED) && (dspImmedEXVAL > 0xFFFFFF))
			return error("long address is bigger than $FFFFFF");

		deposit_extra_ea = DEPOSIT_EXTRA_WORD;

		// Yes, we have an expression
		return DSP_EA_ABS;
	}
	else if (*tok == '<')
	{
		// Check for immediate address forced short
		tok++;

		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		if ((dspImmedEXATTR & DEFINED) && (dspImmedEXVAL > 0xFFF))
			return error("short addressing mode forced but address is bigger than $FFF");

		// Yes, we have an expression
		return DSP_EA_ABS;
	}
	else
	{
		ea1 = checkea(termchar, strings);

		if (ea1 == ERROR)
			return ERROR;
		else
			return ea1;
	}

}


//
// Main routine to check parallel move modes.
// It's quite complex so it's split into a few procedures (in fact most of the
// above ones). A big effort was made so this can be managable and not too
// hacky, however one look at the 56001 manual regarding parallel moves and
// you'll know that this is not an easy // problem to deal with!
// dest=destination register from the main opcode. This must not be the same
// as D1 or D2 and that even goes for stuff like dest=A, D1=A0/1/2!!!
//
//
LONG parmoves(WORD dest)
{
	int force_imm;          // Addressing mode force operator
	int immreg;             // Immediate register destination
	LONG inst;              // 16 bit bitfield that has the parallel move opcode
	LONG S1, S2, D1, D2;    // Source and Destinations
	LONG ea1;				// ea bitfields

	if (*tok == EOL)
	{
		// No parallel move
		return 0b0010000000000000;
	}

	if (*tok == '#')
	{
		// '#xxxxxx,D', '#xx,D'
		tok++;
		force_imm = NUM_NORMAL;

		if (*tok == '>')
		{
			force_imm = NUM_FORCE_LONG;
			tok++;
		}
		else if (*tok == '<')
		{
			force_imm = NUM_FORCE_SHORT;
			tok++;
		}

		if (expr(dspImmedEXPR, &dspImmedEXVAL, &dspImmedEXATTR, &dspImmedESYM) != OK)
			return ERROR;

		if (*tok++ != ',')
			return error("expected comma");

		if (!((*tok >= REG56_X0 && *tok <= REG56_N7) || (*tok >= REG56_R0 && *tok <= REG56_R7) || (*tok >= REG56_A0 && *tok <= REG56_A2)))
			return error("expected x0,x1,y0,y1,a0,b0,a2,b2,a1,b1,a,b,r0-r7,n0-n7 after immediate");

		D1 = *tok++;
		immreg = SDreg(D1);

		if (*tok == EOL)
		{
			if (dest == REG56_A && (D1 == REG56_A || D1 == REG56_A0 || D1 == REG56_A1 || D1 == REG56_A2))
			{
				return error("illegal #xxxxxx,D parallel move syntax: instruction's destination register cannot be a and D in '#xxxxxx,D' be a, a0, a1 or a2");
			}
			if (dest == REG56_B && (D1 == REG56_B || D1 == REG56_B0 || D1 == REG56_B1 || D1 == REG56_B2))
			{
				return error("illegal #xxxxxx,D parallel move syntax: instruction's destination register cannot be b and D in '#xxxxxx,D' be b, b0, b1 or b2");
			}

			if (!(dspImmedEXATTR & FLOAT))
			{
				if (dspImmedEXATTR & DEFINED)
				{
					// From I parallel move:
					// "If the destination register D is X0, X1, Y0, Y1, A, or
					// B, the 8-bit immediate short operand is interpreted as
					// a signed fraction and is stored in the specified
					// destination register. That is, the 8 - bit data is
					// stored in the eight MS bits of the destination operand,
					// and the remaining bits of the destination operand D are
					// zeroed."
					// The funny bit is that Motorola assembler can parse
					// something like 'move #$FF0000,b' into an I (immediate
					// short move) - so let's do that as well then...
					if (((immreg >= 4 && immreg <= 7) || immreg == 14 || immreg == 15) && force_imm != NUM_FORCE_LONG)
					{
						if ((dspImmedEXVAL & 0xFFFF) == 0)
						{
							dspImmedEXVAL >>= 16;
						}
					}

					if (force_imm == NUM_FORCE_SHORT)
					{
						if (dspImmedEXVAL < 0xFF && (int32_t)dspImmedEXVAL > -0x100)
						{
							// '#xx,D'
							// value fits in 8 bits - immediate move
							inst = 0b0010000000000000 + (immreg << 8) + (uint32_t)dspImmedEXVAL;
							return inst;
						}
						else
						{
							if (CHECK_OPTS(OPT_56K_AUTO_LONG))
							{
								if (optim_warn_flag)
									warn("forced short immediate value doesn't fit in 8 bits - switching to long");
								force_imm = NUM_FORCE_LONG;
							}
							else
							{
								return error("forced short immediate value doesn't fit in 8 bits - turn opt switch o11 on to bypass");
							}
						}
					}

					if (force_imm == NUM_FORCE_LONG)
					{
						// '#xxxxxx,D'
						// it can either be
						// X or Y Data move. I don't think it matters much
						// which of the two it will be, so let's use X.
deposit_immediate_long_with_register:
						inst = 0b0100000011110100;
						inst |= ((immreg & 0x18) << (12 - 3)) + ((immreg & 7) << 8);
						deposit_extra_ea = DEPOSIT_EXTRA_WORD;
						return inst;
					}

					if (dspImmedEXVAL + 0x100 < 0x200)
					{
						// value fits in 8 bits - immediate move
deposit_immediate_short_with_register:
						inst = 0b0010000000000000 + (immreg << 8) | (uint8_t)dspImmedEXVAL;
						return inst;
					}
					else
					{
						// value doesn't fit in 8 bits, so it can either be
						// X or Y Data move. I don't think it matters much
						// which of the two it will be, so let's use X:.
						// TODO: if we're just goto'ing perhaps the logic can be simplified
						goto deposit_immediate_long_with_register;
					}
				}
				else
				{
					if (force_imm != NUM_FORCE_SHORT)
					{
						// '#xxxxxx,D'
						// TODO: if we're just goto'ing perhaps the logic can be simplified
						goto deposit_immediate_long_with_register;
					}
					else
					{
						// '#xx,D' - I mode
						// No visibility of the number so let's add a fixup for this
						AddFixup(FU_DSPIMM8, sloc, dspImmedEXPR);
						inst = 0b0010000000000000;
						inst |= ((immreg & 0x18) << (11 - 3)) + ((immreg & 7) << 8);
						return inst;
					}
				}
			}
			else
			{
				// Float constant
				if (dspImmedEXATTR & DEFINED)
				{
					double f = *(double *)&dspImmedEXVAL;
					// Check direct.c for ossom comments regarding conversion!
					//N.B.: This is bogus, we need to fix this so it does this the right way... !!! FIX !!!
					dspImmedEXVAL = ((uint32_t)(int32_t)round(f * (1 << 23))) & 0xFFFFFF;
					double g;
					g = f * (1 << 23);
					g = round(g);

					if ((dspImmedEXVAL & 0xFFFF) == 0)
					{
						if (CHECK_OPTS(OPT_56K_SHORT))
						{
							// Value's 16 lower bits are not set so the value can
							// fit in a single byte (check parallel I move quoted
							// above)
							if (optim_warn_flag)
								warn("o10: Immediate value fits inside 8 bits, so using instruction short format");

							dspImmedEXVAL >>= 16;
							goto deposit_immediate_short_with_register;
						}
						else
						{
							return error("Immediate value fits inside 8 bits, so using instruction short format - turn opt switch o10 on to bypass");
						}
					}

					if (force_imm == NUM_FORCE_SHORT)
					{
						if ((dspImmedEXVAL & 0xFFFF) != 0)
						{
							if (CHECK_OPTS(OPT_56K_AUTO_LONG))
							{
								if (optim_warn_flag)
									warn("o11: Immediate value short format forced but value does not fit inside 8 bits - switching to long format");

								goto deposit_immediate_long_with_register;
							}
							else
							{
								return error("Immediate value short format forced but value does not fit inside 8 bits - turn opt switch o11 on to bypass");
							}
						}

						return error("internal assembler error: we haven't implemented floating point constants in parallel mode parser yet!");
					}

					// If we reach here we either have NUM_FORCE_LONG or nothing, so we might as well store a long.
					goto deposit_immediate_long_with_register;
				}
				else
				{
					if (force_imm == NUM_FORCE_SHORT)
					{
						goto deposit_immediate_short_with_register;
					}
					else
					{
						// Just deposit a float fixup
						AddFixup(FU_DSPIMMFL8, sloc, dspImmedEXPR);
						inst = 0b0010000000000000;
						inst |= ((immreg & 0x18) << (12 - 3)) + ((immreg & 7) << 8);
						return inst;
					}
				}
			}
		}
		else
		{
			// At this point we can only have '#xxxxxx,D1 S2,D2' (X:R Class I)
			switch (immreg)
			{
			case 4: D1 = 0 << 10;break;  // X0
			case 5: D1 = 1 << 10;break;  // X1
			case 14: D1 = 2 << 10;break; // A
			case 15: D1 = 3 << 10;break; // B
			default: return error("unrecognised X:R parallel move syntax: D1 can only be x0,x1,a,b in '#xxxxxx,D1 S2,D2'"); break;
			}

			switch (*tok++)
			{
			case REG56_A: S2 = 0 << 9; break;
			case REG56_B: S2 = 1 << 9; break;
			default: return error("unrecognised X:R parallel move syntax: S2 can only be A or B in '#xxxxxx,D1 S2,D2'"); break;
			}

			if (*tok++ != ',')
				return error("unrecognised X:R parallel move syntax: expected comma after '#xxxxxx,D1 S2'");

			switch (*tok++)
			{
			case REG56_Y0: D2 = 0 << 8; break;
			case REG56_Y1: D2 = 1 << 8; break;
			default: return error("unrecognised X:R parallel move syntax: D2 can only be Y0 or Y1 in '#xxxxxx,D1 S2,D2'"); break;
			}

			if (*tok != EOL)
				return error("unrecognised X:R parallel move syntax: expected end-of-line after '#xxxxxx,D1 S2,D2'");

			inst = 0b0001000010110100 | D1 | S2 | D2;
			deposit_extra_ea = DEPOSIT_EXTRA_WORD;
			return inst;
		}
	}
	else if (*tok == REG56_X)
	{
		if (tok[1] == ',')
			// Hey look, it's just the register X and not the addressing mode - fall through to general case
			goto parse_everything_else;

		tok++;

		if (*tok++ != ':')
			return error("expected ':' after 'X' in parallel move (i.e. X:)");

		// 'X:ea,D' or 'X:aa,D' or 'X:ea,D1 S2,D2' or 'X:eax,D1 Y:eay,D2' or 'X:eax,D1 S2,Y:eay'
		return parse_x(1, 0b0100000000000000, 0, 1, dest);
	}
	else if (*tok == REG56_Y)
	{
		if (tok[1] == ',')
			// Hey look, it's just the register y and not the addressing mode - fall through to general case
			goto parse_everything_else;

		tok++;

		if (*tok++ != ':')
			return error("expected ':' after 'Y' in parallel move (i.e. Y:)");

		// 'Y:ea,D' or 'Y:aa,D'
		return parse_y(0b0100100010000000, 0, 0, 0, dest);
	}
	else if (*tok == REG56_L)
	{
		// 'L:ea,D' or 'L:aa,D'
		tok++;
		if (*tok++ != ':')
			return error("expected ':' after 'L' in parallel move (i.e. L:)");

		return parse_l(1, 0b0100000011000000, 0);
	}
	else if ((*tok >= REG56_X0 && *tok <= REG56_N7) || (*tok >= REG56_R0 && *tok <= REG56_R7) || (*tok >= REG56_A0 && *tok <= REG56_A2) || (*tok >= REG56_A10 && *tok <= REG56_BA))
	{
		// Everything else - brace for impact!
		// R:   'S,D'
		// X:   'S,X:ea' 'S,X:aa'
		// X:R  'S,X:ea S2,D2' 'A,X:ea X0,A' 'B,X:ea X0,B'
		// Y:   'S,Y:ea' 'S,Y:aa'
		// R:Y: 'S1,D1 Y:ea,D2' 'S1,D1 S2,Y:ea' 'Y0,A A,Y:ea' 'Y0,B B,Y:ea' 'S1,D1 #xxxxxx,D2' 'Y0,A A,Y:ea' 'Y0,B B,Y:ea'
		// L:   'S,L:ea' 'S,L:aa'
		LONG L_S1;
parse_everything_else:
		L_S1 = *tok++;
		S1 = SDreg(L_S1);

		if (*tok++ != ',')
			return error("Comma expected after 'S')");

		if (*tok == REG56_X)
		{
			// 'S,X:ea' 'S,X:aa' 'S,X:ea S2,D2' 'S1,X:eax Y:eay,D2' 'S1,X:eax S2,Y:eay'
			// 'A,X:ea X0,A' 'B,X:ea X0,B'
			tok++;

			if (*tok++ != ':')
				return error("unrecognised X: parallel move syntax: expected ':' after 'S,X'");

			return parse_x(0, 0b0100000000000000, S1, 1, dest);
		}
		else if (*tok == REG56_Y)
		{
			// 'S,Y:ea' 'S,Y:aa'
			tok++;

			if (*tok++ != ':')
				return error("unrecognised Y: parallel move syntax: expected ':' after 'S,Y'");

			return parse_y(0b000000000000000, S1, 0, 0, dest);
		}
		else if (*tok == REG56_L)
		{
			// 'S,L:ea' 'S,L:aa'
			tok++;

			if (*tok++ != ':')
				return error("unrecognised L: parallel move syntax: expected ':' after 'S,L'");

			return parse_l(1, 0b0000000000000000, L_S1);
		}
		else if ((*tok >= REG56_X0 && *tok <= REG56_N7) || (*tok >= REG56_R0 && *tok <= REG56_R7) || (*tok >= REG56_A0 && *tok <= REG56_A2))
		{
			// 'S,D'
			// 'S1,D1 Y:ea,D2' 'S1,D1 S2,Y:ea' 'S1,D1 #xxxxxx,D2'
			// 'Y0,A A,Y:ea' 'Y0,B B,Y:ea'
			D1 = SDreg(*tok++);

			if (*tok == EOL)
			{
				// R 'S,D'
				inst = 0b0010000000000000;
				inst |= (S1 << 5) | (D1);
				return inst;
			}
			else if (*tok == REG56_Y)
			{
				// 'S1,D1 Y:ea,D2'
				tok++;
				if (*tok++ != ':')
					return error("expected ':' after 'Y' in parallel move (i.e. Y:)");
				return parse_y(0b0001000001000000, S1, D1, 0, dest);

			}
			else if (*tok == REG56_A || *tok == REG56_B || *tok == REG56_Y0 || *tok == REG56_Y1)
			{
				// 'Y0,A A,Y:ea' 'Y0,B B,Y:ea' 'S1,D1 S2,Y:ea'
				S2 = SDreg(*tok++);

				if (S1 == 6 && D1 == 14 && S2 == 14)
				{
					// 'Y0,A A,Y:ea'
					if (*tok++ != ',')
						return error("unrecognised Y: parallel move syntax: expected ',' after Y0,A A");

					if (*tok++ != REG56_Y)
						return error("unrecognised Y: parallel move syntax: expected 'Y' after Y0,A A,");

					if (*tok++ != ':')
						return error("unrecognised Y: parallel move syntax: expected ':' after Y0,A A,Y");

					ea1 = checkea_full(EOL, Y_ERRORS);

					if (ea1 == DSP_EA_ABS)
						return error("unrecognised X:R parallel move syntax: absolute address not allowed in 'y0,a a,y:ea'");

					if (ea1 == ERROR)
						return ERROR;

					inst = 0b0000100010000000;
					inst |= 0 << 8;
					inst |= ea1;
					return inst;
				}
				else if (S1 == 6 && D1 == 15 && S2 == 15)
				{
					// 'Y0,B B,Y:ea'
					if (*tok++ != ',')
						return error("unrecognised Y: parallel move syntax: expected ',' after Y0,B B");

					if (*tok++ != REG56_Y)
						return error("unrecognised Y: parallel move syntax: expected 'Y' after Y0,B B,");

					if (*tok++ != ':')
						return error("unrecognised Y: parallel move syntax: expected ':' after Y0,B B,Y");

					ea1 = checkea_full(EOL, Y_ERRORS);

					if (ea1 == ERROR)
						return ERROR;

					inst = 0b0000100010000000;
					inst |= 1 << 8;
					inst |= ea1;
					return inst;
				}
				else if ((S1 == 14 || S1 == 15) && (D1 == 4 || D1 == 5) && (S2 == 6 || S2 == 7 || S2 == 14 || S2 == 15))
				{
					//'S1,D1 S2,Y:ea'
					if (*tok++ != ',')
						return error("unrecognised Y: parallel move syntax: expected ',' after S1,D1 S2");

					if (*tok++ != REG56_Y)
						return error("unrecognised Y: parallel move syntax: expected 'Y' after S1,D1 S2,");

					if (*tok++ != ':')
						return error("unrecognised Y: parallel move syntax: expected ':' after S1,D1 S2,Y");

					ea1 = checkea_full(EOL, Y_ERRORS);

					if (ea1 == ERROR)
						return ERROR;

					inst = 0b0001000001000000;
					inst |= (S1 & 1) << 11;
					inst |= (D1 & 1) << 10;
					inst |= ((S2 & 8) << (10 - 4)) | ((S2 & 1) << 8);
					inst |= ea1;
					return inst;
				}
				else
					return error("unrecognised Y: parallel move syntax: only 'Y0,A A,Y:ea' 'Y0,B B,Y:ea' allowed'");
				// Check for Y:
			}
			else if (*tok == '#')
			{
				// R:Y: 'S1,D1 #xxxxxx,D2'
				tok++;

				if (*tok == '>')
				{
					// Well, forcing an immediate to be 24 bits is legal here
					// but then it's the only available option so my guess is that this
					// is simply superfluous. So let's just eat the character
					tok++;
				}

				if (expr(dspaaEXPR, &dspaaEXVAL, &dspaaEXATTR, &dspaaESYM) != OK)
					return ERROR;

				if ((dspImmedEXATTR & DEFINED) && (dspImmedEXVAL > 0xFFFFFF))
					return error("immediate is bigger than $FFFFFF");

				deposit_extra_ea = DEPOSIT_EXTRA_WORD;

				if (*tok++ != ',')
					return error("Comma expected after 'S1,D1 #xxxxxx')");

				// S1 is a or b, D1 is x0 or x1 and d2 is y0, y1, a or b
				switch (*tok++)
				{
				case REG56_Y0: D2 = 0 << 8; break;
				case REG56_Y1: D2 = 1 << 8; break;
				case REG56_A:  D2 = 2 << 8; break;
				case REG56_B:  D2 = 3 << 8; break;
				default:    return error("unrecognised R:Y: parallel move syntax: D2 must be y0, y1, a or b in 'S1,D1 #xxxxxx,D2'");
				}

				if (S1 == 14 || S1 == 15)
				{
					if (D1 == 4 || D1 == 5)
					{
						inst = 0b0001000011110100;
						inst |= (S1 & 1) << 11;
						inst |= (D1 & 1) << 10;
						inst |= D2;
						dspImmedEXVAL = dspaaEXVAL;
						return inst;
					}
					else
						return error("unrecognised R:Y: parallel move syntax: D1 must be x0 or x1 in 'S1,D1 #xxxxxx,D2'");
				}
				else
					return error("unrecognised R:Y: parallel move syntax: S1 must be a or b in 'S1,D1 #xxxxxx,D2'");
			}
			else
				return error("unrecognised R:Y: parallel move syntax: Unexpected text after S,D in 'S1,D1 #xxxxxx,D2'");
		}
		else
			return error("unrecognised R:Y: parallel move syntax: Unexpected text after 'S,'");
	}
	else if (*tok == '(')
	{
		// U: 'ea'
		// U 'ea' can only be '(Rn)-Nn', '(Rn)+Nn', '(Rn)-' or '(Rn)+'
		tok++;

		if (*tok >= REG56_R0 && *tok <= REG56_R7)
		{
			ea1 = (*tok++ - REG56_R0);
		}
		else
			return error("unrecognised U parallel move syntax: expected 'Rn' after '('");

		if (*tok++ != ')')
			return error("unrecognised U parallel move syntax: expected ')' after '(Rn'");

		if (*tok == '+')
		{
			tok++;

			if (*tok == EOL)
				// (Rn)+
				ea1 |= 3 << 3;
			else if (*tok >= REG56_N0 && *tok <= REG56_N7)
			{
				// (Rn)+Nn
				if ((*tok++ & 7) != ea1)
					return error("unrecognised U parallel move syntax: Same register number expected for Rn, Nn in '(Rn)+Nn')");

				ea1 |= 1 << 3;

				if (*tok != EOL)
					return error("unrecognised U parallel move syntax: expected End-Of-Line after '(Rn)+Nn'");
			}
			else
				return error("unrecognised U parallel move syntax: expected End-Of-Line or 'Nn' after '(Rn)+'");
		}
		else if (*tok == '-')
		{
			tok++;

			if (*tok == EOL)
			{
				// (Rn)-
				ea1 |= 2 << 3;
				tok++;
			}
			else if (*tok >= REG56_N0 && *tok <= REG56_N7)
			{
				// (Rn)-Nn
				if ((*tok++ & 7) != ea1)
					return error("unrecognised U parallel move syntax: Same register number expected for Rn, Nn in '(Rn)-Nn')");

				ea1 |= 0 << 3;

				if (*tok != EOL)
					return error("unrecognised U parallel move syntax: expected End-Of-Line after '(Rn)-Nn'");
			}
		}

		inst = 0b0010000001000000;
		inst |= ea1;
		return inst;
	}
	else
		return error("extra (unexpected) text found");

	return OK;
}

