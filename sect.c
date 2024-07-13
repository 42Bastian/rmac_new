//
// RMAC - Renamed Macro Assembler for all Atari computers
// SECT.C - Code Generation, Fixups and Section Management
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "sect.h"
#include "6502.h"
#include "direct.h"
#include "dsp56k.h"
#include "error.h"
#include "expr.h"
#include "listing.h"
#include "mach.h"
#include "mark.h"
#include "riscasm.h"
#include "symbol.h"
#include "token.h"
#define DEF_REGRISC
#include "riscregs.h"

// Function prototypes
void MakeSection(int, uint16_t);
void SwitchSection(int);

// Section descriptors
SECT sect[NSECTS];		// All sections...
int cursect;			// Current section number

// These are copied from the section descriptor, the current code chunk
int fixups_active = 0;	// Give a heads up to expr() because it might need to take some special measures

// These are copied from the section descriptor, the current code chunk
// descriptor and the current fixup chunk descriptor when a switch is made into
// a section. They are copied back to the descriptors when the section is left.
uint16_t scattr;		// Section attributes
uint32_t sloc;			// Current loc in section

CHUNK * scode;			// Current (last) code chunk
uint32_t challoc;		// # bytes alloc'd to code chunk
uint32_t ch_size;		// # bytes used in code chunk
uint8_t * chptr;		// Deposit point in code chunk buffer
uint8_t * chptr_opcode;	// Backup of chptr, updated before entering code generators

// Return a size (SIZB, SIZW, SIZL) or 0, depending on what kind of fixup is
// associated with a location.
static uint8_t fusiztab[] = {
	0,	// FU_QUICK
	1,	// FU_BYTE
	2,	// FU_WORD
	2,	// FU_WBYTE
	4,	// FU_LONG
	1,	// FU_BBRA
	0,	// (unused)
	1,	// FU_6BRA
};

// Offset to REAL fixup location
static uint8_t fusizoffs[] = {
	0,	// FU_QUICK
	0,	// FU_BYTE
	0,	// FU_WORD
	1,	// FU_WBYTE
	0,	// FU_LONG
	1,	// FU_BBRA
	0,	// (unused)
	0,	// FU_6BRA
};


//
// Initialize sections; setup initial ABS, TEXT, DATA and BSS sections
//
void InitSection(void)
{
	// Initialize all sections
	for(int i=0; i<NSECTS; i++)
		MakeSection(i, 0);

	// Construct default sections, make TEXT the current section
	MakeSection(ABS,     SUSED | SABS | SBSS);	// ABS
	MakeSection(TEXT,    SUSED | TEXT       );	// TEXT
	MakeSection(DATA,    SUSED | DATA       );	// DATA
	MakeSection(BSS,     SUSED | BSS  | SBSS);	// BSS
	MakeSection(M6502,   SUSED | TEXT       );	// 6502 code section
	MakeSection(M56001P, SUSED | SABS       );	// DSP 56001 Program RAM
	MakeSection(M56001X, SUSED | SABS       );	// DSP 56001 X RAM
	MakeSection(M56001Y, SUSED | SABS       );	// DSP 56001 Y RAM

	// Switch to TEXT for starters
	SwitchSection(TEXT);
}


//
// Make a new (clean) section
//
void MakeSection(int sno, uint16_t attr)
{
	SECT * sp = &sect[sno];
	sp->scattr = attr;
	sp->sloc = 0;
	sp->orgaddr = 0;
	sp->scode = sp->sfcode = NULL;
	sp->sfix = sp->sffix = NULL;
}


//
// Switch to another section (copy section & chunk descriptors to global vars
// for fast access)
//
void SwitchSection(int sno)
{
	CHUNK * cp;
	cursect = sno;
	SECT * sp = &sect[sno];

	m6502 = (sno == M6502);	// Set 6502-mode flag

	// Copy section vars
	scattr = sp->scattr;
	sloc = sp->sloc;
	scode = sp->scode;
	orgaddr = sp->orgaddr;

	// Copy code chunk vars
	if ((cp = scode) != NULL)
	{
		challoc = cp->challoc;
		ch_size = cp->ch_size;
		chptr = cp->chptr + ch_size;

		// For 6502 mode, add the last org'd address
// Why?
/*
Because the way this is set up it treats the 6502 assembly space as a single 64K space (+ 16 bytes, for some unknown reason) and just bobbles around inside that space and uses a stack of org "pointers" to show where the data ended up.

This is a shitty way to handle things, and we can do better than this!  :-P

Really, there's no reason to have the 6502 (or DSP56001 for that matter) have their own private sections for this kind of thing, as there's literally *no* chance that it would be mingled with 68K+ code.  It should be able to use the TEXT, DATA & BSS sections just like the 68K.

Or should it?  After looking at the code, maybe it's better to keep the 56001 sections segregated from the rest.  But we can still make the 6502 stuff better.
*/
		if (m6502)
			chptr = cp->chptr + orgaddr;
	}
	else
		challoc = ch_size = 0;
}


//
// Save current section
//
void SaveSection(void)
{
	SECT * sp = &sect[cursect];

	sp->scattr = scattr;			// Bailout section vars
	sp->sloc = sloc;
	sp->orgaddr = orgaddr;

	if (scode != NULL)				// Bailout code chunk (if any)
		scode->ch_size = ch_size;
}


//
// Test to see if a location has a fixup set on it. This is used by the
// listing generator to print 'xx's instead of '00's for forward references
//
int fixtest(int sno, uint32_t loc)
{
	// Force update to sect[] variables
	StopMark();

	// Ugly linear search for a mark on our location. The speed doesn't
	// matter, since this is only done when generating a listing, which is
	// SLOW anyway.
	for(FIXUP * fp=sect[sno].sffix; fp!=NULL; fp=fp->next)
	{
		uint32_t w = fp->attr;
		uint32_t xloc = fp->loc + (int)fusizoffs[w & FUMASK];

		if (xloc == loc)
			return (int)fusiztab[w & FUMASK];
	}

	return 0;
}


//
// Check that there are at least 'amt' bytes left in the current chunk. If
// there are not, allocate another chunk of at least CH_CODE_SIZE bytes or
// 'amt', whichever is larger.
//
// If 'amt' is zero, ensure there are at least CH_THRESHOLD bytes, likewise.
//
void chcheck(uint32_t amt)
{
	DEBUG { printf("chcheck(%u)\n", amt); }

	// If in BSS section, no allocation required
	if (scattr & SBSS)
		return;

	if (amt == 0)
		amt = CH_THRESHOLD;

	DEBUG { printf("    challoc=%i, ch_size=%i, diff=%i\n", challoc, ch_size, challoc - ch_size); }

	if ((int)(challoc - ch_size) >= (int)amt)
		return;

	if (amt < CH_CODE_SIZE)
		amt = CH_CODE_SIZE;

	DEBUG { printf("    amt (adjusted)=%u\n", amt); }
	SECT * p = &sect[cursect];
	CHUNK * cp = malloc(sizeof(CHUNK) + amt);
	int first = 0;

	if (scode == NULL)
	{
		// First chunk in section
		cp->chprev = NULL;
		p->sfcode = cp;
		first = 1;
	}
	else
	{
		// Add second and on to previous chunk
		cp->chprev = scode;
		scode->chnext = cp;
		scode->ch_size = ch_size;	// Save old chunk's globals
	}

	// Setup chunk and global vars
/*
So, whenever there's an ORG in a 56K section, it sets sloc TO THE ADDRESS IN THE ORG.  Also, the loc/sloc are incremented by 1s, which means to alias correctly to the byte-oriented memory model we have here, we have to fix that kind of crap.
*/
	cp->chloc = sloc; // <-- HERE'S THE PROBLEM FOR 56K  :-/
	cp->chnext = NULL;
	challoc = cp->challoc = amt;
	ch_size = cp->ch_size = 0;
	chptr = cp->chptr = ((uint8_t *)cp) + sizeof(CHUNK);
	scode = p->scode = cp;

	// A quick kludge
/*
OK, so this is a bit shite, but at least it gets things working the way they should.  The right way to do this is not rely on sloc & friends for the right fixup address but to have an accurate model of the thing.  That will probably come with v2.0.1  :-P

So the problem is, d_org sets sloc to the address of the ORG statement, and that gives an incorrect base for the fixup.  And so when a second (or later) chunk is allocated, it gets set wrong.  Further complicating things is that the orgaddress *does not* get used in a typical way with the DSP56001 code, and, as such, causes incorrect addresses to be generated.  All that has to be dealt with in order to get this right and do away with this kludge.
*/
	if (((cursect == M56001P) || (cursect == M56001X) || (cursect == M56001Y)) && !first)
		cp->chloc = cp->chprev->chloc + cp->chprev->ch_size;

	return;
}


//
// Arrange for a fixup on a location
//
int AddFixup(uint32_t attr, uint32_t loc, TOKEN * fexpr)
{
	uint16_t exprlen = 0;
	SYM * symbol = NULL;
	uint32_t _orgaddr = 0;

	// First, check to see if the expression is a bare label, otherwise, force
	// the FU_EXPR flag into the attributes and count the tokens.
	if ((fexpr[0] == SYMBOL) && (fexpr[2] == ENDEXPR))
	{
		symbol = symbolPtr[fexpr[1]];

		// Save the org address for JR RISC instruction
		if ((attr & FUMASKRISC) == FU_JR)
			_orgaddr = orgaddr;
	}
	else
	{
		attr |= FU_EXPR;
		exprlen = ExpressionLength(fexpr);
	}

	// Second, check to see if it's a DSP56001 fixup, and force the FU_56001
	// flag into the attributes if so; also save the current org address.
	if (attr & FUMASKDSP)
	{
		attr |= FU_56001;
		// Save the exact spot in this chunk where the fixup should go
		_orgaddr = chptr - scode->chptr + scode->chloc;
	}

	// Allocate space for the fixup + any expression
	FIXUP * fixup = malloc(sizeof(FIXUP) + (sizeof(TOKEN) * exprlen)*2);

	// Store the relevant fixup information in the FIXUP
	fixup->next = NULL;
	fixup->attr = attr;
	fixup->loc = loc;
	fixup->fileno = cfileno;
	fixup->lineno = curlineno;
	fixup->expr = NULL;
	fixup->symbol = symbol;
	fixup->orgaddr = _orgaddr;

	// Copy the passed in expression to the FIXUP, if any
	if (exprlen > 0)
	{
		// Here we used to to a plain memcpy and punt on trying to evaluate the expression by then.
		// As discussed in bug #176, this can lead to robustness issues because some symbols might
		// have changed by the time we perform the relocations (think of a symbol that's SET multiple
		// times). So instead we perform a symbol-by-symbol copy and check to see if there are any
		// resolved symbols that can be evaluated immediately. Those, we replace with constants.
		// Also of note: because "fixup" can be larger than what ExpressionLength() returns
		// (due to constants taking up more space than symbols), we allocate twice as RAM as we should
		// without expansions just to be on the safe side. The "correct" thing to do would be to
		// modify ExpressionLength() to cater for defined symbols and return the exact amount of items.

		fixup->expr = (TOKEN *)((uint8_t *)fixup + sizeof(FIXUP));
		int i;
		PTR dstexpr;
		dstexpr.u32 = fixup->expr;
		SYM *sy;
		for (i = 0; i < exprlen; i++)
		{
			if (*fexpr == SYMBOL)
			{
				sy = symbolPtr[fexpr[1]];
				if (sy->sattr & DEFINED && !(sy->sattr & (TDB| M56KPXYL|M6502)))
				{
					// Only convert symbols that are defined and are absolute
					*dstexpr.u32++ = CONST;
					*dstexpr.u64++ = sy->svalue;
					fexpr += 2;
					i++;
				}
				else
				{
					// Just copy the symbol
					*dstexpr.u32++ = *fexpr++;
					*dstexpr.u32++ = *fexpr++;
					i++;
				}
			}
			else if (*fexpr == CONST || *fexpr == FCONST)
			{
				// Copy constants
				*dstexpr.u32++ = *fexpr++;
				*dstexpr.u32++ = *fexpr++;
				*dstexpr.u32++ = *fexpr++;
				i += 2;
			}
			else
				*dstexpr.u32++ = *fexpr++;
		}
	}

	// Finally, put the FIXUP in the current section's linked list
	if (sect[cursect].sffix == NULL)
	{
		sect[cursect].sffix = fixup;
		sect[cursect].sfix = fixup;
	}
	else
	{
		sect[cursect].sfix->next = fixup;
		sect[cursect].sfix = fixup;
	}

	DEBUG { printf("AddFixup: sno=%u, l#=%u, attr=$%X, loc=$%X, expr=%p, sym=%p, org=$%X\n", cursect, fixup->lineno, fixup->attr, fixup->loc, (void *)fixup->expr, (void *)fixup->symbol, fixup->orgaddr);
		if (symbol != NULL)
			printf("          name: %s, value: $%llX\n", symbol->sname, symbol->svalue);
	}

	return 0;
}


//
// Resolve fixups in the passed in section
//
int ResolveFixups(int sno)
{
	SECT * sc = &sect[sno];

	// "Cache" first chunk
	CHUNK * cch = sc->sfcode;

	// Can't fixup a section with nothing in it
	if (cch == NULL)
		return 0;

	// Wire the 6502 segment's size to its allocated size (64K)
	if (sno == M6502)
		cch->ch_size = cch->challoc;

	// Get first fixup for the passed in section
	FIXUP * fixup = sect[sno].sffix;

	while (fixup != NULL)
	{
		// We do it this way because we have continues everywhere... :-P
		FIXUP * fup = fixup;
		fixup = fixup->next;

		uint32_t dw = fup->attr;	// Fixup long (type + modes + flags)
		uint32_t loc = fup->loc;	// Location to fixup
		cfileno = fup->fileno;
		curlineno = fup->lineno;
		DEBUG { printf("ResolveFixups: sect#=%u, l#=%u, attr=$%X, loc=$%X, expr=%p, sym=%p, org=$%X\n", sno, fup->lineno, fup->attr, fup->loc, (void *)fup->expr, (void *)fup->symbol, fup->orgaddr); }

		// This is based on global vars cfileno, curfname :-P
		// This approach is kinda meh as well. I think we can do better
		// than this.
		SetFilenameForErrorReporting();

		if ((sno == M56001P) || (sno == M56001X) || (sno == M56001Y) || (sno == M56001L))
			loc = fup->orgaddr;

		// Search for chunk containing location to fix up; compute a
		// pointer to the location (in the chunk). Often we will find the
		// Fixup is in the "cached" chunk, so the linear-search is seldom
		// executed.
		if (loc < cch->chloc || loc >= (cch->chloc + cch->ch_size))
		{
			for(cch=sc->sfcode; cch!=NULL; cch=cch->chnext)
			{
				if (loc >= cch->chloc && loc < (cch->chloc + cch->ch_size))
					break;
			}

			if (cch == NULL)
			{
				// Fixup (loc) is out of range--this should never happen!
				// Once we call this function, it winds down immediately; it
				// doesn't return.
				interror(7);
			}
		}

		// Location to fix (in current chunk)
		// We use the address of the chunk that loc is actually in, then
		// subtract the chunk's starting location from loc to get the offset
		// into the current chunk.
		uint8_t * locp = cch->chptr + (loc - cch->chloc);

		uint16_t eattr = 0;			// Expression attrib
		SYM * esym = NULL;			// External symbol involved in expr
		uint64_t eval;				// Expression value
		uint16_t flags;				// Mark flags

		// Compute expression/symbol value and attributes

		// Complex expression
		if (dw & FU_EXPR)
		{
			// evexpr presumably issues the errors/warnings here
			if (evexpr(fup->expr, &eval, &eattr, &esym) != OK)
				continue;

			if (esym)
				if (!(esym->sattr & DEFINED) && eval==0)
				{
					// If our expression still has an undefined symbol at this stage, it's bad news.
					// The linker is never going to resolve the expression, so that's an error.
					error("cannot export complex expression with unresloved symbol '%s'", esym->sname);
					continue;
				}

			if ((CHECK_OPTS(OPT_PC_RELATIVE)) && (eattr & (DEFINED | REFERENCED | EQUATED)) == (DEFINED | REFERENCED))
			{
				error("relocation not allowed when o30 is enabled");
				continue;
			}
		}
		// Simple symbol
		else
		{
			SYM * sy = fup->symbol;
			eattr = sy->sattr;

			if ((CHECK_OPTS(OPT_PC_RELATIVE)) && (eattr & (DEFINED | REFERENCED | EQUATED)) == (DEFINED | REFERENCED))
			{
				error("relocation not allowed when o30 is enabled");
				continue;
			}

			if (eattr & DEFINED)
				eval = sy->svalue;
			else
				eval = 0;

			// If the symbol is not defined, but global, set esym to sy
			if ((eattr & (GLOBAL | DEFINED)) == GLOBAL)
				esym = sy;

			DEBUG { printf("               name: %s, value: $%" PRIX64 "\n", sy->sname, sy->svalue); }
		}

		uint16_t tdb = eattr & TDB;

		// If the expression/symbol is undefined and no external symbol is
		// involved, then that's an error.
		if (!(eattr & DEFINED) && (esym == NULL))
		{
			error(undef_error);
			continue;
		}

		// Do the fixup
		//
		// If a PC-relative fixup is undefined, its value is *not* subtracted
		// from the location (that will happen in the linker when the external
		// reference is resolved).
		//
		// PC-relative fixups must be DEFINED and either in the same section
		// (whereupon the subtraction takes place) or ABS (with no subtract).
		if ((dw & FU_PCREL) || (dw & FU_PCRELX))
		{
			if (eattr & DEFINED)
			{
				if (tdb == sno)
				{
					eval -= loc;

					// In this instruction the PC is located a DWORD away
					if (dw & FU_PCRELX)
						eval += 2;
				}
				else if (tdb)
				{
					// Allow cross-section PCREL fixups in Alcyon mode
					if (prg_flag || (obj_format == RAW))
					{
						switch (tdb)
						{
						case TEXT:
// Shouldn't there be a break here, since otherwise, it will point to the DATA section?
							break;
						case DATA:
							eval += sect[TEXT].sloc;
							break;
						case BSS:
							eval += sect[TEXT].sloc + sect[DATA].sloc;
							break;
						default:
							error("invalid section");
						break;
						}

						eval -= loc;

						// In this instruction the PC is located a DWORD away
						if (dw & FU_PCRELX)
							eval += 2;
						
						if ((int64_t)eval > 0x7fff || (int64_t)eval < -32768)
							error(range_error);
					}
					else
					{
						error("PC-relative expr across sections");
						continue;
					}
				}

				if (optim_warn_flag && (dw & FU_LBRA) && (eval + 0x80 < 0x100))
					warn("unoptimized short branch");
			}

			// Be sure to clear any TDB flags, since we handled it just now
			tdb = 0;
			eattr &= ~TDB;
		}

		// Handle fixup classes
		switch (dw & FUMASK)
		{
		// FU_BBRA fixes up a one-byte branch offset.
		case FU_BBRA:
			if (!(eattr & DEFINED))
			{
				error("external short branch");
				continue;
			}

			eval -= 2;

			if (eval + 0x80 >= 0x100)
				goto rangeErr;

			if (eval == 0)
			{
				if (*locp) // optim_flags[OPT_NULL_BRA] is stored there, check the comment in mach.s under m_br
				{
					// Just output a NOP
					*locp++ = 0x4E;
					*locp = 0x71;

					if (optim_warn_flag)
						warn("o6: bra.s with zero offset converted to NOP");

					continue;
				}
				else
				{
					error("illegal bra.s with zero offset");
					continue;
				}
			}

			*++locp = (uint8_t)eval;
			break;

		// Fixup one-byte value at locp + 1.
		case FU_WBYTE:
			locp++;
			// FALLTHROUGH

		// Fixup one-byte forward references
		case FU_BYTE:
			if (!(eattr & DEFINED))
			{
				error("external byte reference");
				continue;
			}

			if (tdb)
			{
				error("non-absolute byte reference");
				continue;
			}

			if ((dw & FU_PCREL) && ((eval + 0x80) >= 0x100))
				goto rangeErr;

			if (dw & FU_SEXT)
			{
				if ((eval + 0x100) >= 0x200)
					goto rangeErr;
			}
			else if (eval >= 0x100)
				goto rangeErr;

			*locp = (uint8_t)eval;
			break;

		// Fixup high/low byte off word for 6502
		case FU_BYTEH:
			if (!(eattr & DEFINED))
			{
				error("external byte reference");
				continue;
			}

			*locp = (uint8_t)(eval >> 8);
			break;

		case FU_BYTEL:
			if (!(eattr & DEFINED))
			{
				error("external byte reference");
				continue;
			}

			*locp = (uint8_t)eval;
			break;

		// Fixup WORD forward references; the word could be unaligned in the
		// section buffer, so we have to be careful. (? careful about what?)
		case FU_WORD:
			if ((dw & FUMASKRISC) == FU_JR)
			{
				int reg = (signed)((eval - ((fup->orgaddr ? fup->orgaddr : loc) + 2)) / 2);

				if ((reg < -16) || (reg > 15))
				{
					error("relative jump out of range");
					break;
				}

				*locp |= ((uint8_t)reg >> 3) & 0x03;
				locp++;
				*locp |= ((uint8_t)reg & 0x07) << 5;
				break;
			}
			else if ((dw & FUMASKRISC) == FU_NUM15)
			{
				if (((int)eval < -16) || ((int)eval > 15))
				{
					error("constant out of range (-16 - +15)");
					break;
				}

				*locp |= ((uint8_t)eval >> 3) & 0x03;
				locp++;
				*locp |= ((uint8_t)eval & 0x07) << 5;
				break;
			}
			else if ((dw & FUMASKRISC) == FU_NUM31)
			{
				if (eval > 31)
				{
					error("constant out of range (0-31)");
					break;
				}

				*locp |= ((uint8_t)eval >> 3) & 0x03;
				locp++;
				*locp |= ((uint8_t)eval & 0x07) << 5;
				break;
			}
			else if ((dw & FUMASKRISC) == FU_NUM32)
			{
				if ((eval < 1) || (eval > 32))
				{
					error("constant out of range (1-32)");
					break;
				}

				if (dw & FU_SUB32)
					eval = (32 - eval);

				eval = (eval == 32) ? 0 : eval;
				*locp |= ((uint8_t)eval >> 3) & 0x03;
				locp++;
				*locp |= ((uint8_t)eval & 0x07) << 5;
				break;
			}
			else if ((dw & FUMASKRISC) == FU_REGONE)
			{
				eval -= REGRISC_R0;
				if (eval > 31)
				{
					error("register one value out of range");
					break;
				}

				*locp |= ((uint8_t)eval >> 3) & 0x03;
				locp++;
				*locp |= ((uint8_t)eval & 0x07) << 5;
				break;
			}
			else if ((dw & FUMASKRISC) == FU_REGTWO)
			{
				eval -= REGRISC_R0;
				if (eval > 31)
				{
					error("register two value out of range");
					break;
				}

				locp++;
				*locp |= (uint8_t)eval & 0x1F;
				break;
			}

			if (!(eattr & DEFINED))
			{
				flags = MWORD;

				if (dw & FU_PCREL)
					flags |= MPCREL;

				MarkRelocatable(sno, loc, 0, flags, esym);
			}
			else
			{
				if (tdb)
					MarkRelocatable(sno, loc, tdb, MWORD, NULL);

				if (dw & FU_SEXT)
				{
					if (eval + 0x10000 >= 0x20000)
						goto rangeErr;
				}
				else
				{
					// Range-check BRA and DBRA
					if (dw & FU_ISBRA)
					{
						if (eval + 0x8000 >= 0x10000)
							goto rangeErr;
					}
					else if (eval >= 0x10000)
						goto rangeErr;
				}
			}

			// 6502 words are little endian, so handle that here
			if (sno == M6502)
				SETLE16(locp, 0, eval)
			else
				SETBE16(locp, 0, eval)

			break;

		// Fixup LONG forward references; the long could be unaligned in the
		// section buffer, so be careful (again).
		case FU_LONG:
			flags = MLONG;

			if ((dw & FUMASKRISC) == FU_MOVEI)
			{
				// Long constant in MOVEI # is word-swapped, so fix it here
				eval = WORDSWAP32(eval);
				flags |= MMOVEI;
			}

			// If the symbol is undefined, make sure to pass the symbol in
			// to the MarkRelocatable() function.
			if (!(eattr & DEFINED))
				MarkRelocatable(sno, loc, 0, flags, esym);
			else if (tdb)
				MarkRelocatable(sno, loc, tdb, flags, NULL);

			SETBE32(locp, 0, eval);
			break;

		// Fixup QUAD forward references (mainly used by the OP assembler)
		case FU_QUAD:
			if (dw & FU_OBJLINK)
			{
				uint64_t quad = GETBE64(locp, 0);
				uint64_t addr = eval;

//Hmm, not sure how this can be set, since it's only set if it's a DSP56001 fixup or a FU_JR...  :-/
//				if (fup->orgaddr)
//					addr = fup->orgaddr;

				eval = (quad & 0xFFFFFC0000FFFFFFLL) | ((addr & 0x3FFFF8) << 21);
			}
			else if (dw & FU_OBJDATA)
			{
				// If it's in a TEXT or DATA section, be sure to mark for a
				// fixup later
				if (tdb)
					MarkRelocatable(sno, loc, tdb, MQUAD, NULL);

				uint64_t quad = GETBE64(locp, 0);
				uint64_t addr = eval;

//Hmm, not sure how this can be set, since it's only set if it's a DSP56001 fixup or a FU_JR...  :-/
//				if (fup->orgaddr)
//					addr = fup->orgaddr;

				eval = (quad & 0x000007FFFFFFFFFFLL) | ((addr & 0xFFFFF8) << 40);
			}

			SETBE64(locp, 0, eval);
			break;

		// Fixup a 3-bit "QUICK" reference in bits 9..1
		// (range of 1..8) in a word. [Really bits 1..3 in a byte.]
		case FU_QUICK:
			if (!(eattr & DEFINED))
			{
				error("External quick reference");
				continue;
			}

			if ((eval < 1) || (eval > 8))
				goto rangeErr;

			*locp |= (eval & 7) << 1;
			break;

		// Fix up 6502 funny branch
		case FU_6BRA:
			eval -= (loc + 1);

			if (eval + 0x80 >= 0x100)
				goto rangeErr;

			*locp = (uint8_t)eval;
			break;

		// Fixup DSP56001 addresses
		case FU_56001:
			switch (dw & FUMASKDSP)
			{
			// DSPIMM5 actually is clamped from 0 to 23 for our purposes
			// and does not use the full 5 bit range.
			case FU_DSPIMM5:
				if (eval > 23)
				{
					error("immediate value must be between 0 and 23");
					break;
				}

				locp[2] |= eval;
				break;

			// This is a 12-bit address encoded into the lower 12
			// bits of a DSP word
			case FU_DSPADR12:
				if (eval >= 0x1000)
				{
					error("address out of range ($0-$FFF)");
					break;
				}

				locp[1] |= eval >> 8;
				locp[2] = eval & 0xFF;
				break;

			// This is a full DSP word containing Effective Address Extension
			case FU_DSPADR24:
			case FU_DSPIMM24:
				if (eval >= 0x1000000)
				{
					error("value out of range ($0-$FFFFFF)");
					break;
				}

				locp[0] = (uint8_t)((eval >> 16) & 0xFF);
				locp[1] = (uint8_t)((eval >> 8) & 0xFF);
				locp[2] = (uint8_t)(eval & 0xFF);
				break;

			// This is a 16bit absolute address into a 24bit field
			case FU_DSPADR16:
				if (eval >= 0x10000)
				{
					error("address out of range ($0-$FFFF)");
					break;
				}

				locp[1] = (uint8_t)(eval >> 8);
				locp[2] = (uint8_t)eval;
				break;

			// This is 12-bit immediate short data
			// The upper nibble goes into the last byte's low nibble
			// while the remainder 8 bits go into the 2nd byte.
			case FU_DSPIMM12:
				if (eval >= 0x1000)
				{
					error("immediate out of range ($0-$FFF)");
					break;
				}

				locp[1] = (uint8_t)eval;
				locp[2] |= (uint8_t)(eval >> 8);
				break;

			// This is 8-bit immediate short data
			// which goes into the middle byte of a DSP word.
			case FU_DSPIMM8:
				if (eval >= 0x100)
				{
					error("immediate out of range ($0-$FF)");
					break;
				}

				locp[1] = (uint8_t)eval;
				break;

			// This is a 6 bit absoulte short address. It occupies the low 6
			// bits of the middle byte of a DSP word.
			case FU_DSPADR06:
				if (eval > 63)
				{
					error("address must be between 0 and 63");
					break;
				}

				locp[1] |= eval;
				break;

			// This is a 6 bit absoulte short address. It occupies the low 6
			// bits of the middle byte of a DSP word.
			case FU_DSPPP06:
				if (eval < 0xFFFFFFC0)
				{
					error("address must be between $FFC0 and $FFFF");
					break;
				}

				locp[1] |= eval & 0x3F;
				break;

			// Shamus: I'm pretty sure these don't make any sense...
			case FU_DSPIMMFL8:
				warn("FU_DSPIMMFL8 missing implementation\n%s", "And you may ask yourself, \"Self, how did I get here?\"");
				break;

			case FU_DSPIMMFL16:
				warn("FU_DSPIMMFL16 missing implementation\n%s", "And you may ask yourself, \"Self, how did I get here?\"");
				break;

			case FU_DSPIMMFL24:
				warn("FU_DSPIMMFL24 missing implementation\n%s", "And you may ask yourself, \"Self, how did I get here?\"");
				break;

			// Bad fixup type--this should *never* happen!
			default:
				interror(4);
				// NOTREACHED
			}
			break;

		// Fixup a 4-byte float
		case FU_FLOATSING:
			warn("FU_FLOATSING missing implementation\n%s", "And you may ask yourself, \"Self, how did I get here?\"");
			break;

		// Fixup a 8-byte float
		case FU_FLOATDOUB:
			warn("FU_FLOATDOUB missing implementation\n%s", "And you may ask yourself, \"Self, how did I get here?\"");
			break;

		// Fixup a 12-byte float
		case FU_FLOATEXT:
			warn("FU_FLOATEXT missing implementation\n%s", "And you may ask yourself, \"Self, how did I get here?\"");
			break;

		default:
			// Bad fixup type--this should *never* happen!
			// Once we call this function, it winds down immediately; it
			// doesn't return.
			interror(4);
		}

		continue;
rangeErr:
		error("expression out of range");
	}

	return 0;
}


//
// Resolve all fixups
//
int ResolveAllFixups(void)
{
	// Make undefined symbols GLOBL
	if (glob_flag)
		ForceUndefinedSymbolsGlobal();

	if (prg_flag)
	{
		fixups_active = 1;
	}

	DEBUG printf("Resolving TEXT sections...\n");
	ResolveFixups(TEXT);
	DEBUG printf("Resolving DATA sections...\n");
	ResolveFixups(DATA);
	DEBUG printf("Resolving 6502 sections...\n");
	ResolveFixups(M6502);		// Fixup 6502 section (if any)
	DEBUG printf("Resolving DSP56001 P: sections...\n");
	ResolveFixups(M56001P);		// Fixup 56001 P: section (if any)
	DEBUG printf("Resolving DSP56001 X: sections...\n");
	ResolveFixups(M56001X);		// Fixup 56001 X: section (if any)
	DEBUG printf("Resolving DSP56001 Y: sections...\n");
	ResolveFixups(M56001Y);		// Fixup 56001 Y: section (if any)

	return 0;
}

