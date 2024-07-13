//
// RMAC - Renamed Macro Assembler for all Atari computers
// PROCLN.C - Line Processing
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "procln.h"
#include "6502.h"
#include "amode.h"
#include "direct.h"
#include "dsp56k_amode.h"
#include "dsp56k_mach.h"
#include "error.h"
#include "expr.h"
#include "listing.h"
#include "mach.h"
#include "macro.h"
#include "op.h"
#include "riscasm.h"
#include "sect.h"
#include "symbol.h"

#define DEF_KW					// Declare keyword values
#include "kwtab.h"				// Incl generated keyword tables & defs

#define DEF_MN					// Incl 68k keyword definitions
#define DECL_MN					// Incl 68k keyword state machine tables
#include "mntab.h"
#define DEF_REG68				// Incl 68k register definitions
#include "68kregs.h"

#define DEF_MR
#define DECL_MR
#include "risckw.h"

#define DEF_MP					// Include 6502 keyword definitions
#define DECL_MP					// Include 6502 keyword state machine tables
#include "6502kw.h"

#define DEF_MO					// Include OP keyword definitions
#define DECL_MO					// Include OP keyword state machine tables
#include "opkw.h"

#define DEF_DSP					// Include DSP56K keywords definitions
#define DECL_DSP				// Include DSP56K keyword state machine tables
#include "dsp56kkw.h"
#define DEF_REG56				// Include DSP56K register definitions
#include "56kregs.h"

IFENT * ifent;					// Current ifent
static IFENT ifent0;			// Root ifent
IFENT * f_ifent;				// Freelist of ifents
int disabled;					// Assembly conditionally disabled
int just_bss;					// 1, ds.b in microprocessor mode
uint32_t pcloc;					// Value of "PC" at beginning of line
SYM * lab_sym;					// Label on line (or NULL)
char * label_defined;			// The name of the last label defined in current line (if any)

const char extra_stuff[] = "extra (unexpected) text found after addressing mode";
const char comma_error[] = "missing comma";
const char syntax_error[] = "syntax error";
const char locgl_error[] = "cannot GLOBL local symbol";
const char lab_ignored[] = "label ignored";

// Table to convert an addressing-mode number to a bitmask.
LONG amsktab[0124] = {
	M_DREG, M_DREG, M_DREG, M_DREG,
	M_DREG, M_DREG, M_DREG, M_DREG,

	M_AREG, M_AREG, M_AREG, M_AREG,
	M_AREG, M_AREG, M_AREG, M_AREG,

	M_AIND, M_AIND, M_AIND, M_AIND,
	M_AIND, M_AIND, M_AIND, M_AIND,

	M_APOSTINC, M_APOSTINC, M_APOSTINC, M_APOSTINC,
	M_APOSTINC, M_APOSTINC, M_APOSTINC, M_APOSTINC,

	M_APREDEC, M_APREDEC, M_APREDEC, M_APREDEC,
	M_APREDEC, M_APREDEC, M_APREDEC, M_APREDEC,

	M_ADISP, M_ADISP, M_ADISP, M_ADISP,
	M_ADISP, M_ADISP, M_ADISP, M_ADISP,

	M_AINDEXED, M_AINDEXED, M_AINDEXED, M_AINDEXED,
	M_AINDEXED, M_AINDEXED, M_AINDEXED, M_AINDEXED,

	M_ABSW,			// 070
	M_ABSL,			// 071
	M_PCDISP,		// 072
	M_PCINDEXED,	// 073
	M_IMMED,		// 074
	0L,				// 075
	0L,				// 076
	0L,				// 077
	M_ABASE,		// 0100
	M_MEMPOST,		// 0101
	M_MEMPRE,		// 0102
	M_PCBASE,		// 0103
	M_PCMPOST,		// 0104
	M_PCMPRE,		// 0105
	M_AM_USP,		// 0106
	M_AM_SR,		// 0107
	M_AM_CCR,		// 0110
	M_AM_NONE,		// 0111
	0x30,			// 0112
	0x30,			// 0113
	0L,				// 0114
	0L,				// 0115
	0L,				// 0116
	0L,				// 0117
	M_CACHE40,		// 0120
	M_CREG,			// 0121
	M_FREG,			// 0122
	M_FPSCR			// 0123
};					// 0123 length

// Function prototypes
int HandleLabel(char *, int);

//
// Initialize line processor
//
void InitLineProcessor(void)
{
	disabled = 0;
	ifent = &ifent0;
	f_ifent = ifent0.if_prev = NULL;
	ifent0.if_state = 0;
}

//
// Line processor
//
void Assemble(void)
{
	int state;					// Keyword machine state (output)
	int j;						// Random int, must be fast
	char * p;					// Random char ptr, must be fast
	TOKEN * tk;					// First token in line
	char * label;				// Symbol (or NULL)
	char * equate;				// Symbol (or NULL)
	int labtyp = 0;				// Label type (':', DCOLON)
	int equtyp = 0;				// Equ type ('=', DEQUALS)
	uint64_t eval;				// Expression value
	WORD eattr;					// Expression attributes
	SYM * esym;					// External symbol involved in expr.
	WORD siz = 0;				// Size suffix to mnem/diretve/macro
	LONG amsk0, amsk1;			// Address-type masks for ea0, ea1
	MNTAB * m;					// Code generation table pointer
	SYM * sy, * sy2;			// Symbol (temp usage)
	char * opname = NULL;		// Name of dirctve/mnemonic/macro
	int listflag;				// 0: Don't call listeol()
	WORD rmask;					// Register list, for REG
	int equreg;				// RISC register
	listflag = 0;				// Initialise listing flag

loop:							// Line processing loop label

	// Get another line of tokens
	if (TokenizeLine() == TKEOF)
	{
DEBUG { printf("Assemble: Found TKEOF flag...\n"); }
		if (list_flag && listflag)			// Flush last line of source
			listeol();

		if (ifent->if_prev != NULL)			// Check conditional token
			error("hit EOF without finding matching .endif");

		return;
	}

	DEBUG { DumpTokenBuffer(); }

	if (list_flag)
	{
		if (listflag && listing > 0)
			listeol();						// Tell listing generator about EOL

		lstout((char)(disabled ? '-' : lntag));	// Prepare new line for listing
		listflag = 1;						// OK to call `listeol' now
		just_bss = 0;						// Reset just_bss mode
	}

	state = -3;								// No keyword (just EOL)
	label = NULL;							// No label
	label_defined = NULL;					// No label defined yet
	lab_sym = NULL;							// No (exported) label
	equate = NULL;							// No equate
	tk = tok;								// Save first token in line
	pcloc = (uint32_t)sloc;					// Set beginning-of-line PC

loop1:										// Internal line processing loop

	if (*tok == EOL)						// Restart loop if end-of-line
		goto loop;

	// First token MUST be a symbol (Shamus: not sure why :-/)
	if (*tok != SYMBOL)
	{
		error("syntax error; expected symbol");

		goto loop;
	}

	j = (int)tok[2];						// Skip equates (normal statements)

	if (j == '=' || j == DEQUALS || j == SET || j == REG || j == EQUREG || j == CCDEF)
	{
		equate = string[tok[1]];
		equtyp = j;
		tok += 3;
		goto normal;
	}

	// Skip past label (but record it)
	if (j == ':' || j == DCOLON)
	{
		label = string[tok[1]];				// Get label name
		labtyp = tok[2];					// Get label type
		tok += 3;							// Go to next line token
	}

	// EOL is legal here...
	if (*tok == EOL)
		goto normal;

	// First token MUST be a symbol (if we get here, tok didn't advance)
	if (*tok++ != SYMBOL)
	{
		error("syntax error; expected symbol");
		goto loop;
	}

	opname = p = string[*tok++];

	// Check to see if the SYMBOL is a keyword (a mnemonic or directive).
	// On output, `state' will have one of the values:
	//    -3          there was no symbol (EOL)
	//    -2..-1      the symbol didn't match any keyword
	//    0..499      vanilla directives (dc, ds, etc.)
	//    500..999    electric directives (macro, rept, etc.)
	//    1000..+     mnemonics (move, lsr, etc.)
	for(state=0; state>=0;)
	{
		j = mnbase[state] + (int)tolowertab[*p];

		// Reject, character doesn't match
		if (mncheck[j] != state)
		{
			state = -1;						// No match
			break;
		}

		// Must accept or reject at EOS
		if (!*++p)
		{
			state = mnaccept[j];			// (-1 on no terminal match)
			break;
		}

		state = mntab[j];
	}

	// Check for ".b" ".w" ".l" after directive, macro or mnemonic.
	siz = SIZN;

	switch (*tok)
	{
	case DOTW: siz = SIZW, tok++; break;
	case DOTL: siz = SIZL, tok++; break;
	case DOTB: siz = SIZB, tok++; break;
	case DOTD: siz = SIZD, tok++; break;
	case DOTP: siz = SIZP, tok++; break;
	case DOTQ: siz = SIZQ, tok++; break;
	case DOTS: siz = SIZS, tok++; break;
	case DOTX: siz = SIZX, tok++; break;
	}

	// Do special directives (500..999) (These must be handled in "real time")
	if (state >= 500 && state < 1000)
	{
		switch (state)
		{
		case MN_IF:
			d_if();
			goto loop;

		case MN_ELSE:
			d_else();
			goto loop;

		case MN_ENDIF:
			d_endif();
			goto loop;

		case MN_IIF:						// .iif --- immediate if
			if (disabled || expr(exprbuf, &eval, &eattr, &esym) != OK)
				goto loop;

			if (!(eattr & DEFINED))
			{
				error(undef_error);
				goto loop;
			}

			if (*tok++ != ',')
			{
				error(comma_error);
				goto loop;
			}

			if (eval == 0)
				goto loop;

			goto loop1;

		case MN_MACRO:						// .macro --- macro definition
			if (!disabled)
			{
				// Label on a macro definition is bad mojo... Warn the user
				if (label != NULL)
					warn(lab_ignored);

				DefineMacro();
			}

			goto loop;

		case MN_EXITM:						// .exitm --- exit macro
		case MN_ENDM:						// .endm --- same as .exitm
			if (!disabled)
			{
				if (label != NULL)
					warn(lab_ignored);

				ExitMacro();
			}

			goto loop;

		case MN_REPT:
			if (!disabled)
			{
				// Handle labels on REPT directive lines...
				if (label)
				{
					if (HandleLabel(label, labtyp) != 0)
						goto loop;

					label_defined = label;
				}

				HandleRept();
			}

			goto loop;

		case MN_ENDR:
			if (!disabled)
				error("mis-nested .endr");

			goto loop;
		}
	}

normal:
	if (disabled)							// Conditionally disabled code
		goto loop;

	// Do equates
	if (equate != NULL)
	{
		// Pick global or local symbol enviroment
		j = (*equate == '.' ? curenv : 0);
		sy = lookup(equate, LABEL, j);

		if (sy == NULL)
		{
			sy = NewSymbol(equate, LABEL, j);
			sy->sattr = 0;

			if (equtyp == DEQUALS)
			{
				// Can't GLOBAL a local symbol
				if (j)
				{
					error(locgl_error);
					goto loop;
				}

				sy->sattr = GLOBAL;
			}
		}
		else if ((sy->sattr & DEFINED) && equtyp != SET)
		{
			if ((equtyp == EQUREG) && (sy->sattre & UNDEF_EQUR))
			{
				sy->sattre &= ~UNDEF_EQUR;
				sy->svalue = 0;
			}
			else if ((equtyp == CCDEF) && (sy->sattre & UNDEF_CC))
			{
				sy->sattre &= ~UNDEF_CC;
				sy->svalue = 0;
			}
			else
			{
				error("multiple equate to '%s'", sy->sname);
				goto loop;
			}
		}

		// Put symbol in "order of definition" list if it's not already there
		AddToSymbolDeclarationList(sy);

		// Parse value to equate symbol to;
		// o  .equr
		// o  .reg
		// o  everything else
		if (equtyp == EQUREG)
		{
//Linko's request to issue a warning on labels that equated to the same
//register would go here. Not sure how to implement it though. :-/
/*
Maybe like this way:
have an array of bools with 64 entries. Whenever a register is equated, set the
corresponding register bool to true. Whenever it's undef'ed, set it to false.
When checking to see if it's already been equated, issue a warning.
*/

			// Check for register to equate to
			// This check will change once we split the registers per architecture into their own tables
			// and out of kw.tab. But for now it'll do...
			if ((*tok >= REG68_D0) && (*tok <= REG56_BA))
			{
				sy->sattre = EQUATEDREG;	// Mark as equated register
				equreg = *tok;
				// Check for ",<bank #>" override notation and skip past it.
				// It is ignored now. Was that ever useful anyway?
				if ((rgpu ||rdsp) && (tok[1] == ',') && (tok[2] == CONST))
				{
					// Advance token pointer and skip everything
					tok += 4;
				}

				eattr = ABS | DEFINED | GLOBAL;
				eval = equreg;
				tok++;
			}
			// Checking for a register symbol
			else if (tok[0] == SYMBOL)
			{
				sy2 = lookup(string[tok[1]], LABEL, j);

				// Make sure symbol is a valid equreg
				if (!sy2 || !(sy2->sattre & EQUATEDREG))
				{
					error("invalid .equr/.regequ definition");
					goto loop;
				}
				else
				{
					eattr = ABS | DEFINED | GLOBAL;	// Copy symbol's attributes
					sy->sattre = sy2->sattre;
					eval = (sy2->svalue & 0xFFFFF0FF);
					tok += 2;
				}
			}
			else
			{
				error("invalid .equr/.regequ definition");
				goto loop;
			}
		}
		else if (equtyp == REG)
		{
			if (reglist(&rmask) < 0)
				goto loop;

			eval = (uint32_t)rmask;
			eattr = ABS | DEFINED;
		}
		else if (equtyp == CCDEF)
		{
			sy->sattre |= EQUATEDCC;
			eattr = ABS | DEFINED | GLOBAL;

			if (tok[0] == SYMBOL)
			{
				sy2 = lookup(string[tok[1]], LABEL, j);

				if (!sy2 || !(sy2->sattre & EQUATEDCC))
				{
					error("invalid gpu/dsp .ccdef definition");
					goto loop;
				}
				else
				{
					eattr = ABS | DEFINED | GLOBAL;
					sy->sattre = sy2->sattre;
					eval = sy2->svalue;
					tok += 2;
				}
			}
			else if (expr(exprbuf, &eval, &eattr, &esym) != OK)
				goto loop;
		}
		// equ an equr
		else if (*tok == SYMBOL)
		{
			sy2 = lookup(string[tok[1]], LABEL, j);

			if (sy2 && (sy2->sattre & EQUATEDREG))
			{
				sy->stype = sy2->stype;
				sy->sattr = sy2->sattr;
				sy->sattre = sy2->sattre;
				sy->svalue = sy2->svalue;
				goto loop;
			}
			else if (expr(exprbuf, &eval, &eattr, &esym) != OK)
				goto loop;
		}
		else if (expr(exprbuf, &eval, &eattr, &esym) != OK)
			goto loop;

		if (!(eattr & DEFINED))
		{
			error(undef_error);
			goto loop;
		}

		sy->sattr |= eattr | EQUATED;	// Symbol inherits value and attributes
		sy->svalue = eval;

		if (list_flag)					// Put value in listing
			listvalue((uint32_t)eval);

		ErrorIfNotAtEOL();				// Must be at EOL now
		goto loop;
	}

	// Do labels
	if (label != NULL)
	{
		// Non-zero == error occurred
		if (HandleLabel(label, labtyp) != 0)
			goto loop;

		label_defined = label;
	}

	// Punt on EOL
	if (state == -3)
		goto loop;

	// If we're in 6502 mode and are still in need of a mnemonic, then search
	// for valid 6502 mnemonic.
	if (m6502 && (state < 0 || state >= 1000))
	{
#ifdef ST
		state = kmatch(opname, mpbase, mpcheck, mptab, mpaccept);
#else
		for(state=0, p=opname; state>= 0; )
		{
			j = mpbase[state] + tolowertab[*p];

			if (mpcheck[j] != state)	// Reject, character doesn't match
			{
				state = -1;		// No match
				break;
			}

			if (!*++p)
			{			// Must accept or reject at EOS
				state = mpaccept[j];	// (-1 on no terminal match)
				break;
			}

			state = mptab[j];
		}
#endif

		// Call 6502 code generator if we found a mnemonic
		if (state >= 2000)
		{
			m6502cg(state - 2000);
			goto loop;
		}
	}

	// If we are in GPU or DSP mode and still in need of a mnemonic then search
	// for one
	if ((rgpu || rdsp) && (state < 0 || state >= 1000))
	{
		for(state=0, p=opname; state>=0;)
		{
			j = mrbase[state] + (int)tolowertab[*p];

			// Reject, character doesn't match
			if (mrcheck[j] != state)
			{
				state = -1;					// No match
				break;
			}

			// Must accept or reject at EOS
			if (!*++p)
			{
				state = mraccept[j];		// (-1 on no terminal match)
				break;
			}

			state = mrtab[j];
		}

		// Call RISC code generator if we found a mnemonic
		if (state >= 3000)
		{
			GenerateRISCCode(state);
			goto loop;
		}
	}

	// If we are in OP mode and still in need of a mnemonic then search for one
	if (robjproc && ((state < 0) || (state >= 1000)))
	{
		for(state=0, p=opname; state>=0;)
		{
			j = mobase[state] + (int)tolowertab[*p];

			// Reject, character doesn't match
			if (mocheck[j] != state)
			{
				state = -1;					// No match
				break;
			}

			// Must accept or reject at EOS
			if (!*++p)
			{
				state = moaccept[j];		// (-1 on no terminal match)
				break;
			}

			state = motab[j];
		}

		// Call OP code generator if we found a mnemonic
		if (state >= 3100)
		{
			GenerateOPCode(state);
			goto loop;
		}
	}

	// If we are in 56K mode and still in need of a mnemonic then search for one
	if (dsp56001 && ((state < 0) || (state >= 1000)))
	{
		for(state=0, p=opname; state>=0;)
		{
			j = dspbase[state] + (int)tolowertab[*p];

			// Reject, character doesn't match
			if (dspcheck[j] != state)
			{
				state = -1;					// No match
				break;
			}

			// Must accept or reject at EOS
			if (!*++p)
			{
				state = dspaccept[j];		// (-1 on no terminal match)
				break;
			}

			state = dsptab[j];
		}

		// Call DSP code generator if we found a mnemonic
		if (state >= 2000)
		{
			LONG parcode;
			int operands = 0;
			MNTABDSP * md = &dsp56k_machtab[state - 2000];
			deposit_extra_ea = 0;   // Assume no extra word needed
			dsp_a1reg = 0;

			if (md->mnfunc == dsp_mult)
			{
				// Special case for multiplication instructions: they require
				// 3 operands
				if ((operands = dsp_amode(3)) == ERROR)
					goto loop;
			}
			else if ((md->mnattr & PARMOVE) && md->mn0 != M_AM_NONE)
			{
				if ((operands = dsp_amode(2)) == ERROR)
					goto loop;
			}
			else if ((md->mnattr & PARMOVE) && md->mn0 == M_AM_NONE)
			{
				// Instructions that have parallel moves but use no operands
				// (probably only move). In this case, don't parse addressing
				// modes--just go straight to parallel parse
				dsp_am0 = dsp_am1 = M_AM_NONE;
			}
			else
			{
				// Non parallel move instructions can have up to 4 parameters
				// (well, only tcc instructions really)
				if ((operands = dsp_amode(4)) == ERROR)
					goto loop;

				if (operands == 4)
				{
					dsp_tcc4(md->mninst);
					goto loop;
				}
			}

			if (md->mnattr & PARMOVE)
			{
				// Check for parallel moves
				// We need to pass the information of which destination 
				// register (if any) is in the main opcode to the parallel
				// moves routine. This is a bit more nuanced than originally
				// expected: for starters, different variables hold the 
				// destination register depending on the amount of operands
				// (dsp_a1reg, etc). Then some instructions with parallel moves
				// simply do not alter any register (cmp and tst mostly). There
				// are also cases of the instruction not containing any register
				// (example: move with only a X:R field).
				int destination_register = dsp_a1reg;
				if (md->mnattr & PARNOWRITE)
				{
					destination_register = 0;
				}
				else if (operands == 3)
				{
					destination_register = dsp_a2reg;
				}
				else if (operands == 4)
				{
					// No parallel moves
				}
				else if (operands == 0)
				{
					destination_register = 0;
				}
				if ((parcode = parmoves(destination_register)) == ERROR)
					goto loop;
			}
			else
			{
				if (*tok != EOL)
					error("parallel moves not allowed with this instruction");

				parcode = 0;
			}

			while ((dsp_am0 & md->mn0) == 0 || (dsp_am1 & md->mn1) == 0)
				md = &dsp56k_machtab[md->mncont];

			GENLINENOSYM();
			(*md->mnfunc)(md->mninst | (parcode << 8));
			goto loop;
		}
	}

	// Invoke macro or complain about bad mnemonic
	if (state < 0)
	{
		if ((sy = lookup(opname, MACRO, 0)) != NULL)
			InvokeMacro(sy, siz);
		else
			error("unknown op '%s'", opname);

		goto loop;
	}

	// Call directive handlers
	if (state < 500)
	{
		(*dirtab[state])(siz);
		goto loop;
	}

	// Do mnemonics
	//  o  can't deposit instrs in BSS or ABS
	//  o  do automatic .EVEN for instrs
	//  o  allocate space for largest possible instr
	//  o  can't do ".b" operations with an address register
	if (scattr & SBSS)
	{
		error("cannot initialize non-storage (BSS) section");
		goto loop;
	}

	if (sloc & 1)					// Automatic .even
		auto_even();

	if (challoc - ch_size < 18)		// Make sure have space in current chunk
		chcheck(0);

	m = &machtab[state - 1000];

	// Call special-mode handler
	if (m->mnattr & CGSPECIAL)
	{
		GENLINENOSYM();
		(*m->mnfunc)(m->mninst, siz);
		goto loop;
	}

	if (amode(1) < 0)				// Parse 0, 1 or 2 addr modes
		goto loop;

	// Check that we're at EOL
	// The only exception is ptestr/ptestw instructions
	// that have 3 or 4 operands and are not handled by
	// amode(). (yes, we're taking a performance hit here sadly)
	if (m->mnfunc != m_ptestr && m->mnfunc != m_ptestw)
		if (*tok != EOL)
			error(extra_stuff);

	amsk0 = amsktab[am0];
	amsk1 = amsktab[am1];

	// Catch attempts to use ".B" with an address register (yes, this check
	// does work at this level)
	if (siz == SIZB && (am0 == AREG || am1 == AREG))
	{
		error("cannot use '.b' with an address register");
		goto loop;
	}

	// Keep a backup of chptr (used for optimisations during codegen)
	chptr_opcode = chptr;

	while (!(m->mnattr & siz) || (amsk0 & m->mn0) == 0 || (amsk1 & m->mn1) == 0)
		m = &machtab[m->mncont];

	DEBUG { printf("    68K: mninst=$%X, siz=$%X, mnattr=$%X, amsk0=$%X, mn0=$%X, amsk1=$%X, mn1=$%X\n", m->mninst, siz, m->mnattr, amsk0, m->mn0, amsk1, m->mn1); }

	GENLINENOSYM();
	(*m->mnfunc)(m->mninst, siz);
	goto loop;
}

//
// Handle the creation of labels
//
int HandleLabel(char * label, int labelType)
{
	// Check for dot in front of label; means this is a local label if present
	int environment = (*label == '.' ? curenv : 0);
	SYM * symbol = lookup(label, LABEL, environment);

	if (symbol == NULL)
	{
		symbol = NewSymbol(label, LABEL, environment);
		symbol->sattr = 0;
		symbol->sattre = 0;
	}
	else if (symbol->sattr & DEFINED)
		return error("multiply-defined label '%s'", label);

	// Put symbol in "order of definition" list if it's not already in it
	AddToSymbolDeclarationList(symbol);

	if (orgactive)
	{
		symbol->svalue = orgaddr;
		symbol->sattr |= ABS | DEFINED | EQUATED;
	}
	else
	{
		symbol->svalue = sloc;
		symbol->sattr |= DEFINED | cursect;
	}

	lab_sym = symbol;

	// Yes, our CS professors told us to write checks for equality this way,
	// but damn, it hurts my brain every time I look at it. :-/
	if (0 == environment)
		curenv++;

	// Make label global if it has a double colon
	if (labelType == DCOLON)
	{
		if (environment != 0)
			return error(locgl_error);

		symbol->sattr |= GLOBAL;
	}

	return 0;
}
