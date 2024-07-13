//
// RMAC - Renamed Macro Assembler for all Atari computers
// RISCA.C - GPU/DSP Assembler
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "riscasm.h"
#include "amode.h"
#include "direct.h"
#include "error.h"
#include "expr.h"
#include "mark.h"
#include "procln.h"
#include "rmac.h"
#include "sect.h"
#include "token.h"

#define DEF_MR				// Declare keyword values
#include "risckw.h"			// Incl. generated risc keywords

#define DEF_REGRISC
#include "riscregs.h"		// Incl. generated keyword tables & defs

#define MAXINTERNCC 26		// Maximum internal condition codes

// Useful macros
#define EVAL_REG_RETURN_IF_ERROR(x, y) \
x = EvaluateRegisterFromTokenStream(y); \
\
if (x == ERROR) \
	return ERROR;

#define EVAL_REG_RETURN_IF_ERROR_OR_NO_EOL(x, y) \
x = EvaluateRegisterFromTokenStream(y); \
\
if ((x == ERROR) || (ErrorIfNotAtEOL() == ERROR)) \
	return ERROR;

#define CHECK_EOL \
if (ErrorIfNotAtEOL() == ERROR) \
	return ERROR;

unsigned altbankok = 0;		// Ok to use alternate register bank
unsigned orgactive = 0;		// RISC/6502 org directive active
unsigned orgaddr = 0;		// Org'd address
unsigned orgwarning = 0;	// Has an ORG warning been issued
int lastOpcode = -1;		// Last RISC opcode assembled
uint8_t riscImmTokenSeen;	// The '#' (immediate) token was seen

static const char reg_err[] = "missing register R0...R31";

// Jaguar jump condition names
static const char condname[MAXINTERNCC][5] = {
	"NZ", "Z", "NC", "NCNZ", "NCZ", "C", "CNZ", "CZ", "NN", "NNNZ", "NNZ",
	"N", "N_NZ", "N_Z", "T", "A", "NE", "EQ", "CC", "HS", "HI", "CS", "LO",
	"PL", "MI", "F"
};

// Jaguar jump condition numbers
static const char condnumber[] = {
	1, 2, 4, 5, 6, 8, 9, 10, 20, 21, 22, 24, 25, 26,
	0, 0, 1, 2, 4, 4, 5,  8,  8, 20, 24, 31
};

// Opcode Specific Data
struct opcoderecord {
	uint16_t state;		// Opcode Name (unused)
	uint16_t type;		// Opcode Type
	uint16_t param;		// Opcode Parameter
};

static const struct opcoderecord roptbl[] = {
	{ MR_ADD,     RI_TWO,    0 },
	{ MR_ADDC,    RI_TWO,    1 },
	{ MR_ADDQ,    RI_NUM_32, 2 },
	{ MR_ADDQT,   RI_NUM_32, 3 },
	{ MR_SUB,     RI_TWO,    4 },
	{ MR_SUBC,    RI_TWO,    5 },
	{ MR_SUBQ,    RI_NUM_32, 6 },
	{ MR_SUBQT,   RI_NUM_32, 7 },
	{ MR_NEG,     RI_ONE,    8 },
	{ MR_AND,     RI_TWO,    9 },
	{ MR_OR,      RI_TWO,    10 },
	{ MR_XOR,     RI_TWO,    11 },
	{ MR_NOT,     RI_ONE,    12 },
	{ MR_BTST,    RI_NUM_31, 13 },
	{ MR_BSET,    RI_NUM_31, 14 },
	{ MR_BCLR,    RI_NUM_31, 15 },
	{ MR_MULT,    RI_TWO,    16 },
	{ MR_IMULT,   RI_TWO,    17 },
	{ MR_IMULTN,  RI_TWO,    18 },
	{ MR_RESMAC,  RI_ONE,    19 },
	{ MR_IMACN,   RI_TWO,    20 },
	{ MR_DIV,     RI_TWO,    21 },
	{ MR_ABS,     RI_ONE,    22 },
	{ MR_SH,      RI_TWO,    23 },
	{ MR_SHLQ,    RI_NUM_32, 24 + SUB32 },
	{ MR_SHRQ,    RI_NUM_32, 25 },
	{ MR_SHA,     RI_TWO,    26 },
	{ MR_SHARQ,   RI_NUM_32, 27 },
	{ MR_ROR,     RI_TWO,    28 },
	{ MR_RORQ,    RI_NUM_32, 29 },
	{ MR_ROLQ,    RI_NUM_32, 29 + SUB32 },
	{ MR_CMP,     RI_TWO,    30 },
	{ MR_CMPQ,    RI_NUM_15, 31 },
	{ MR_SAT8,    RI_ONE,    32 + GPUONLY },
	{ MR_SUBQMOD, RI_NUM_32, 32 + DSPONLY },
	{ MR_SAT16,   RI_ONE,    33 + GPUONLY },
	{ MR_SAT16S,  RI_ONE,    33 + DSPONLY },
	{ MR_MOVEQ,   RI_NUM_31, 35 },
	{ MR_MOVETA,  RI_TWO,    36 },
	{ MR_MOVEFA,  RI_TWO,    37 },
	{ MR_MOVEI,   RI_MOVEI,  38 },
	{ MR_LOADB,   RI_LOADN,  39 },
	{ MR_LOADW,   RI_LOADN,  40 },
	{ MR_LOADP,   RI_LOADN,  42 + GPUONLY },
	{ MR_SAT32S,  RI_ONE,    42 + DSPONLY },
	{ MR_STOREB,  RI_STOREN, 45 },
	{ MR_STOREW,  RI_STOREN, 46 },
	{ MR_STOREP,  RI_STOREN, 48 + GPUONLY },
	{ MR_MIRROR,  RI_ONE,    48 + DSPONLY },
	{ MR_JUMP,    RI_JUMP,   52 },
	{ MR_JR,      RI_JR,     53 },
	{ MR_MMULT,   RI_TWO,    54 },
	{ MR_MTOI,    RI_TWO,    55 },
	{ MR_NORMI,   RI_TWO,    56 },
	{ MR_NOP,     RI_NONE,   57 },
	{ MR_SAT24,   RI_ONE,    62 },
	{ MR_UNPACK,  RI_ONE,    63 + GPUONLY | (1 << 6) },
	{ MR_PACK,    RI_ONE,    63 + GPUONLY | (0 << 6) },
	{ MR_ADDQMOD, RI_NUM_32, 63 + DSPONLY },
	{ MR_MOVE,    RI_MOVE,   0 },
	{ MR_LOAD,    RI_LOAD,   0 },
	{ MR_STORE,   RI_STORE,  0 }
};

#define MALF_NUM		0
#define MALF_EXPR		1
#define MALF_LPAREN		2
#define MALF_RPAREN		3

static const char malform1[] = "missing '#'";
static const char malform2[] = "bad expression";
static const char malform3[] = "missing ')'";
static const char malform4[] = "missing '('";

static const char * malformErr[] = {
	malform1, malform2, malform3, malform4
};

//
// Function to return "malformed expression" error
// This is done mainly to remove a bunch of GOTO statements in the parser
//
static inline int MalformedOpcode(int signal)
{
	return error("Malformed opcode, %s", malformErr[signal]);
}

//
// Function to return "Illegal Indexed Register" error
// Anyone trying to index something other than R14 or R15
//
static inline int IllegalIndexedRegister(int reg)
{
	return error("Attempted index reference with non-indexable register (r%d)", reg - REGRISC_R0);
}

//
// Function to return "Illegal Indexed Register" error for EQUR scenarios
// Trying to use register value within EQUR that isn't 14 or 15
//
static inline int IllegalIndexedRegisterEqur(SYM * sy)
{
	return error("Attempted index reference with non-indexable register within EQUR (%s = r%d)", sy->sname, sy->svalue);
}

//
// Build up & deposit RISC instruction word
//
static void DepositRISCInstructionWord(uint16_t opcode, int reg1, int reg2)
{
	// Check for absolute address setting
	if (!orgwarning && !orgactive)
	{
		warn("RISC code generated with no origin defined");
		orgwarning = 1;
	}

	int value = ((opcode & 0x3F) << 10) + ((reg1 & 0x1F) << 5) + (reg2 & 0x1F);
	GENLINENOSYM();
	D_word(value);
}

//
// Evaluate the RISC register from the token stream. Passed in value is the
// FIXUP attribute to use if the expression comes back as undefined.
//
static int EvaluateRegisterFromTokenStream(uint32_t fixup)
{
	// Firstly, check to see if it's a register token and return that.  No
	// need to invoke expr() for easy cases like this.
	int reg = *tok & 255;
	if (reg >= REGRISC_R0 && reg <= REGRISC_R31)
	{
		reg -= REGRISC_R0;
		tok++;
		return reg;
	}

	if (*tok != SYMBOL)
	{
		// If at this point we don't have a symbol then it's garbage.  Punt.
		return error("Expected register number or EQUREG");
	}

	uint64_t eval;				// Expression value
	WORD eattr;					// Expression attributes
	SYM * esym;					// External symbol involved in expr.
	TOKEN r_expr[EXPRSIZE];		// Expression token list

	// Evaluate what's in the global "tok" buffer
	// N.B.: We should either get a fixup or a register name from EQUR
	if (expr(r_expr, &eval, &eattr, &esym) != OK)
		return ERROR;

	if (!(eattr & DEFINED))
	{
		AddFixup(FU_WORD | fixup, sloc, r_expr);
		return 0;
	}

	// We shouldn't get here, that should not be legal
	interror(9);
	return 0; // Not that this will ever execute, but let's be nice and pacify gcc warnings
}

//
// Do RISC code generation
//
int GenerateRISCCode(int state)
{
	int reg1;					// Register 1
	int reg2;					// Register 2
	int val = 0;				// Constructed value
	char scratch[80];
	SYM * ccsym;
	SYM * sy;
	int i, commaFound;
	TOKEN * t;
	uint16_t attrflg;
	int indexed;				// Indexed register flag

	uint64_t eval;				// Expression value
	uint16_t eattr;				// Expression attributes
	SYM * esym = NULL;			// External symbol involved in expr.
	TOKEN r_expr[EXPRSIZE];		// Expression token list

	// Get opcode parameter and type
	uint16_t parm = roptbl[state - 3000].param;
	uint16_t type = roptbl[state - 3000].type;
	riscImmTokenSeen = 0;		// Set to "token not seen yet"

	// Detect whether the opcode parmeter passed determines that the opcode is
	// specific to only one of the RISC processors and ensure it is legal in
	// the current code section. If not then show error and return.
	if (((parm & GPUONLY) && rdsp) || ((parm & DSPONLY) && rgpu))
		return error("Opcode is not valid in this code section");

	// Process RISC opcode
	switch (type)
	{
	// No operand instructions
	// NOP (57)
	case RI_NONE:
		DepositRISCInstructionWord(parm, 0, 0);
		break;

	// Single operand instructions (Rd)
	// ABS, MIRROR, NEG, NOT, PACK, RESMAC, SAT8, SAT16, SAT16S, SAT24, SAT32S,
	// UNPACK
	case RI_ONE:
		EVAL_REG_RETURN_IF_ERROR_OR_NO_EOL(reg2, FU_REGTWO);
		DepositRISCInstructionWord(parm, parm >> 6, reg2);
		break;

	// Two operand instructions (Rs,Rd)
	// ADD, ADDC, AND, CMP, DIV, IMACN, IMULT, IMULTN, MOVEFA, MOVETA, MULT,
	// MMULT, MTOI, NORMI, OR, ROR, SH, SHA, SUB, SUBC, XOR
	case RI_TWO:
		if (parm == 37)
			altbankok = 1;                      // MOVEFA

		EVAL_REG_RETURN_IF_ERROR(reg1, FU_REGONE);
		CHECK_COMMA;

		if (parm == 36)
			altbankok = 1;                      // MOVETA

		EVAL_REG_RETURN_IF_ERROR_OR_NO_EOL(reg2, FU_REGTWO);
		DepositRISCInstructionWord(parm, reg1, reg2);
		break;

	// Numeric operand (n,Rd) where n = -16..+15
	// CMPQ
	case RI_NUM_15:

	// Numeric operand (n,Rd) where n = 0..31
	// BCLR, BSET, BTST, MOVEQ
	case RI_NUM_31:

	// Numeric operand (n,Rd) where n = 1..32
	// ADDQ, ADDQMOD, ADDQT, SHARQ, SHLQ, SHRQ, SUBQ, SUBQMOD, SUBQT, ROLQ,
	// RORQ
	case RI_NUM_32:
		switch (type)
		{
		case RI_NUM_15:
			reg1 = -16; reg2 = 15; attrflg = FU_NUM15;
			break;
		default:
		case RI_NUM_31:
			reg1 =   0; reg2 = 31; attrflg = FU_NUM31;
			break;
		case RI_NUM_32:
			reg1 =   1; reg2 = 32; attrflg = FU_NUM32;
			break;
		}

		if (parm & SUB32)
			attrflg |= FU_SUB32;

		if (*tok != '#')
			return MalformedOpcode(MALF_NUM);

		tok++;
		riscImmTokenSeen = 1;

		if (expr(r_expr, &eval, &eattr, &esym) != OK)
			return MalformedOpcode(MALF_EXPR);

		if (!(eattr & DEFINED))
		{
			AddFixup((WORD)(FU_WORD | attrflg), sloc, r_expr);
			reg1 = 0;
		}
		else
		{
			if (esym && (esym->sattre & EQUATEDREG))
				return error("equated register seen for immediate value");

			if (eattr & RISCREG)
				return error("register seen for immediate value");

			if (((int)eval < reg1) || ((int)eval > reg2))
				return error("constant out of range (%d to %d)", reg1, reg2);

			if (parm & SUB32)
				reg1 = 32 - (int)eval;
			else if (type == RI_NUM_32)
				reg1 = (reg1 == 32 ? 0 : (int)eval);
			else
				reg1 = (int)eval;
		}

		CHECK_COMMA;
		EVAL_REG_RETURN_IF_ERROR_OR_NO_EOL(reg2, FU_REGTWO);
		DepositRISCInstructionWord(parm, reg1, reg2);
		break;

	// Move Immediate--n,Rn--n in Second Word
	case RI_MOVEI:
		if (*tok != '#')
			return MalformedOpcode(MALF_NUM);

		tok++;
		riscImmTokenSeen = 1;

		// Check for equated register after # and return error if so
		if (*tok == SYMBOL)
		{
			sy = lookup(string[tok[1]], LABEL, 0);

			if (sy && (sy->sattre & EQUATEDREG))
				return error("equated register in 1st operand of MOVEI instruction");
		}

		if (expr(r_expr, &eval, &eattr, &esym) != OK)
			return MalformedOpcode(MALF_EXPR);

		if ((lastOpcode == RI_JUMP) || (lastOpcode == RI_JR))
		{
			if (legacy_flag)
			{
				// User doesn't care, emit a NOP to fix
				DepositRISCInstructionWord(57, 0, 0);
				warn("MOVEI following JUMP, inserting NOP to fix your BROKEN CODE");
			}
			else
				warn("MOVEI immediately follows JUMP");
		}

		if (!(eattr & DEFINED))
		{
			AddFixup(FU_LONG | FU_MOVEI, sloc + 2, r_expr);
			eval = 0;
		}
		else
		{
			if (eattr & TDB)
				MarkRelocatable(cursect, sloc + 2, (eattr & TDB), (MLONG | MMOVEI), NULL);
		}

		CHECK_COMMA;
		EVAL_REG_RETURN_IF_ERROR_OR_NO_EOL(reg2, FU_REGTWO);

		DepositRISCInstructionWord(parm, 0, reg2);
		val = WORDSWAP32(eval);
		D_long(val);
		break;

	// PC,Rd or Rs,Rd
	case RI_MOVE:
		if (*tok == REGRISC_PC)
		{
			parm = 51;
			reg1 = 0;
			tok++;
		}
		else
		{
			parm = 34;
			EVAL_REG_RETURN_IF_ERROR(reg1, FU_REGONE);
		}

		CHECK_COMMA;
		EVAL_REG_RETURN_IF_ERROR_OR_NO_EOL(reg2, FU_REGTWO);
		DepositRISCInstructionWord(parm, reg1, reg2);
		break;

	// (Rn),Rn = 41 / (R14/R15+n),Rn = 43/44 / (R14/R15+Rn),Rn = 58/59
	case RI_LOAD:
		indexed = 0;
		parm = 41;

		if (*tok != '(')
			return MalformedOpcode(MALF_LPAREN);

		tok++;

        if ((tok[1] == '+') || (tok[1] == '-'))
		{
			// Trying to make indexed call
			if ((*tok == REGRISC_R14) || (*tok == REGRISC_R15))
				indexed = (*tok - REGRISC_R0);
			else
				return IllegalIndexedRegister(*tok);
		}

		if (!indexed)
		{
			EVAL_REG_RETURN_IF_ERROR(reg1, FU_REGONE);
		}
		else
		{
			reg1 = indexed;
			indexed = 0;
			tok++;

			if (*tok == '+')
			{
				parm = (WORD)(reg1 - 14 + 58);
				tok++;

				if ((*tok >= REGRISC_R0) && (*tok <= REGRISC_R31))
					indexed = 1;

				if (*tok == SYMBOL)
				{
					sy = lookup(string[tok[1]], LABEL, 0);

					if (!sy)
					{
						error(reg_err);
						return ERROR;
					}

					if (sy->sattre & EQUATEDREG)
						indexed = 1;
				}

				if (indexed)
				{
					EVAL_REG_RETURN_IF_ERROR(reg1, FU_REGONE);
				}
				else
				{
					if (expr(r_expr, &eval, &eattr, &esym) != OK)
						return MalformedOpcode(MALF_EXPR);

					if (!(eattr & DEFINED))
						return error("constant expected after '+'");

					reg1 = (int)eval;

					if (reg1 == 0)
					{
						reg1 = 14 + (parm - 58);
						parm = 41;
						warn("NULL offset in LOAD ignored");
					}
					else
					{
						if ((reg1 < 1) || (reg1 > 32))
							return error("constant in LOAD out of range (1-32)");

						if (reg1 == 32)
							reg1 = 0;

						parm = (WORD)(parm - 58 + 43);
					}
				}
			}
			else
			{
				EVAL_REG_RETURN_IF_ERROR(reg1, FU_REGONE);
			}
		}

		if (*tok != ')')
			return MalformedOpcode(MALF_RPAREN);

		tok++;
		CHECK_COMMA;
		EVAL_REG_RETURN_IF_ERROR_OR_NO_EOL(reg2, FU_REGTWO);
		DepositRISCInstructionWord(parm, reg1, reg2);
		break;

	// Rn,(Rn) = 47 / Rn,(R14/R15+n) = 49/50 / Rn,(R14/R15+Rn) = 60/61
	case RI_STORE:
		parm = 47;
		EVAL_REG_RETURN_IF_ERROR(reg1, FU_REGONE);
		CHECK_COMMA;

		if (*tok != '(')
			return MalformedOpcode(MALF_LPAREN);

		tok++;
		indexed = 0;

		if (((*tok == REGRISC_R14) || (*tok == REGRISC_R15)) && (tok[1] != ')'))
			indexed = *tok - REGRISC_R0;

		if (!indexed)
		{
			EVAL_REG_RETURN_IF_ERROR(reg2, FU_REGTWO);
		}
		else
		{
			reg2 = indexed;
			indexed = 0;
			tok++;

			if (*tok == '+')
			{
				parm = (WORD)(reg2 - 14 + 60);
				tok++;

				if ((*tok >= REGRISC_R0) && (*tok <= REGRISC_R31))
					indexed = 1;

				if (*tok == SYMBOL)
				{
					sy = lookup(string[tok[1]], LABEL, 0);

					if (!sy)
					{
						error(reg_err);
						return ERROR;
					}

					if (sy->sattre & EQUATEDREG)
						indexed = 1;
				}

				if (indexed)
				{
					EVAL_REG_RETURN_IF_ERROR(reg2, FU_REGTWO);
				}
				else
				{
					if (expr(r_expr, &eval, &eattr, &esym) != OK)
						return MalformedOpcode(MALF_EXPR);

					if (!(eattr & DEFINED))
					{
						AddFixup(FU_WORD | FU_REGTWO, sloc, r_expr);
						reg2 = 0;
					}
					else
					{
						reg2 = (int)eval;

						if (reg2 == 0)
						{
							reg2 = 14 + (parm - 60);
							parm = 47;
							warn("NULL offset in STORE ignored");
						}
						else
						{
							if ((reg2 < 1) || (reg2 > 32))
								return error("constant in STORE out of range (1-32)");

							if (reg2 == 32)
								reg2 = 0;

							parm = (WORD)(parm - 60 + 49);
						}
					}
				}
			}
			else
			{
				EVAL_REG_RETURN_IF_ERROR(reg2, FU_REGTWO);
			}
		}

		if (*tok != ')')
			return MalformedOpcode(MALF_RPAREN);

		tok++;
		CHECK_EOL;
		DepositRISCInstructionWord(parm, reg2, reg1);
		break;

	// LOADB/LOADP/LOADW (Rn),Rn
	case RI_LOADN:
		if (*tok != '(')
			return MalformedOpcode(MALF_LPAREN);

		tok++;
		EVAL_REG_RETURN_IF_ERROR(reg1, FU_REGONE);

		if (*tok != ')')
			return MalformedOpcode(MALF_RPAREN);

		tok++;
		CHECK_COMMA;
		EVAL_REG_RETURN_IF_ERROR_OR_NO_EOL(reg2, FU_REGTWO);
		DepositRISCInstructionWord(parm, reg1, reg2);
		break;

	// STOREB/STOREP/STOREW Rn,(Rn)
	case RI_STOREN:
		EVAL_REG_RETURN_IF_ERROR(reg1, FU_REGONE);
		CHECK_COMMA;

		if (*tok != '(')
			return MalformedOpcode(MALF_LPAREN);

		tok++;
		EVAL_REG_RETURN_IF_ERROR(reg2, FU_REGTWO);

		if (*tok != ')')
			return MalformedOpcode(MALF_RPAREN);

		tok++;
		CHECK_EOL;
		DepositRISCInstructionWord(parm, reg2, reg1);
		break;

	// Jump Relative - cc,n - n=-16..+15 words, reg2=cc
	case RI_JR:

	// Jump Absolute - cc,(Rs) - reg2=cc
	case RI_JUMP:
		// Check to see if there is a comma in the token string. If not then
		// the JR or JUMP should default to 0, Jump Always
		commaFound = 0;

		for(t=tok; *t!=EOL; t++)
		{
			if (*t == ',')
			{
				commaFound = 1;
				break;
			}
		}

		if (commaFound)
		{
			if (*tok == CONST)
			{
				// CC using a constant number (O_o)
				PTR tp;
				tp.tk = tok + 1;
				val = *tp.i64++;
				tok = tp.tk;
				CHECK_COMMA;
			}
			else if (*tok == SYMBOL)
			{
				val = 9999;
				strcpy(scratch, string[tok[1]]);
				strtoupper(scratch);

				for(i=0; i<MAXINTERNCC; i++)
				{
					// Look for the condition code & break if found
					if (strcmp(condname[i], scratch) == 0)
					{
						val = condnumber[i];
						break;
					}
				}

				// Standard CC was not found, look for an equated one
				if (val == 9999)
				{
					ccsym = lookup(string[tok[1]], LABEL, 0);

					if (ccsym && (ccsym->sattre & EQUATEDCC) && !(ccsym->sattre & UNDEF_CC))
						val = (int)ccsym->svalue;
					else
						return error("unknown condition code");
				}

				tok += 2;
				CHECK_COMMA;
			}
			else if (*tok == '(')
			{
				// Set CC to "Jump Always"
				val = 0;
			}
		}
		else
		{
			// Set CC to "Jump Always"
			val = 0;
		}

		if ((val < 0) || (val > 31))
			return error("condition constant out of range");

		// Store condition code
		reg1 = val;

		if (type == RI_JR)
		{
			// JR cc,n
			if (expr(r_expr, &eval, &eattr, &esym) != OK)
				return MalformedOpcode(MALF_EXPR);

			if (!(eattr & DEFINED))
			{
				AddFixup(FU_WORD | FU_JR, sloc, r_expr);
				reg2 = 0;
			}
			else
			{
				reg2 = ((int)(eval - ((orgactive ? orgaddr : sloc) + 2))) / 2;

				if ((reg2 < -16) || (reg2 > 15))
					error("PC relative overflow in JR (outside of -16 to 15)");
			}
		}
		else
		{
			// JUMP cc, (Rn)
			if (*tok != '(')
				return MalformedOpcode(MALF_LPAREN);

			tok++;
			EVAL_REG_RETURN_IF_ERROR(reg2, FU_REGTWO);

			if (*tok != ')')
				return MalformedOpcode(MALF_RPAREN);

			tok++;
			CHECK_EOL;
		}

		DepositRISCInstructionWord(parm, reg2, reg1);
		break;

	// We should never get here. If we do, somebody done fucked up. :-D
	default:
		return error("Unknown RISC opcode type");
	}

	lastOpcode = type;
	return 0;
}
