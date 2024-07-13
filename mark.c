//
// RMAC - Renamed Macro Assembler for all Atari computers
// MARK.C - A record of things that are defined relative to any of the sections
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "mark.h"
#include "error.h"
#include "object.h"
#include "riscasm.h"
#include "sect.h"


#define MARK_ALLOC_INCR 1024		// # bytes to alloc for more mark space
#define MIN_MARK_MEM    (3 * sizeof(uint16_t) + 1 * sizeof(uint32_t) + sizeof(SYM *))

MCHUNK * firstmch;		// First mark chunk
MCHUNK * curmch;		// Current mark chunk
PTR markptr;			// Deposit point in current mark chunk
uint32_t mcalloc;		// # bytes alloc'd to current mark chunk
uint32_t mcused;		// # bytes used in current mark chunk
uint16_t curfrom;		// Current "from" section

// Table to convert from TDB to fixup triad
static uint8_t mark_tr[] = {
	0,		// (N/A)
	2,		// TEXT relocatable
	1, 0,	// DATA relocatable
	3		// BSS relocatable
};

//#define DEBUG_IMAGE_MARKING


//
// Initialize marker
//
void InitMark(void)
{
	firstmch = curmch = NULL;
	mcalloc = mcused = 0;
	curfrom = 0;
	sect[TEXT].relocs = sect[DATA].relocs = sect[BSS].relocs = 0;
}


//
// Wrap up marker (called after final mark is made)
//
void StopMark(void)
{
	if (curmch)
	{
		*markptr.wp = MCHEND;		// Mark end of block
		curmch->mcused = mcused;	// Update # used in mark block
	}
}


//
// Mark a word or longword as relocatable
//
// Record is either 2, 3, or 4 pieces of data long. A mark is of the form:
// .W    <to+flags>	section mark is relative to, and flags in upper byte
// .L    <loc>		location of mark in "from" section
// .W    [from]		new from section (if different from current)
// .L    [symbol]	symbol involved in external reference (if any)
//
uint32_t MarkRelocatable(uint16_t section, uint32_t loc, uint16_t to, uint16_t flags, SYM * symbol)
{
#ifdef DEBUG_IMAGE_MARKING
printf("MarkRelocatable: section=%i, loc=$%X, to=$%X, flags=$%x, symbol=%p\n", section, loc, to, flags, symbol);
if (symbol)
	printf("      symbol->stype=$%02X, sattr=$%04X, sattre=$%08X, svalue=%li, sname=%s\n", symbol->stype, symbol->sattr, symbol->sattre, symbol->svalue, symbol->sname);
#endif

	if ((mcalloc - mcused) < MIN_MARK_MEM)
		AllocateMark();

	// Set up flags
	flags |= to;

	if (section != curfrom)
		flags |= MCHFROM;

	if (symbol != NULL)
		flags |= MSYMBOL;

	//
	// Complain about some things are not allowed in '-p' (PRG) mode:
	//  o  Marks that aren't to LONGs
	//  o  External references
	//
	if (prg_flag)
	{
		if (symbol != NULL)
			error("illegal external reference (in .PRG mode) to '%s'",
				symbol->sname);
	}

	// Dump crap into the mark
	*markptr.wp++ = flags;
	*markptr.lp++ = loc;
	mcused += sizeof(uint16_t) + sizeof(uint32_t);

	if (flags & MCHFROM)
	{
		curfrom = section;
		*markptr.wp++ = section;
		mcused += sizeof(uint16_t);
	}

	if (flags & MSYMBOL)
	{
		*markptr.sy++ = symbol;
		mcused += sizeof(SYM *);
	}

	// Increment # of relocs in this section
	sect[section].relocs++;

	// Not sure what this is about (making sure the next mark is clear until
	// it's marked as the end--I think)...
	*markptr.wp = 0x0000;

	return 0;
}


//
// Allocate another chunk of mark space
//
uint32_t AllocateMark(void)
{
	// Alloc mark block header (and data) and set it up.
	MCHUNK * p = malloc(sizeof(MCHUNK) + MARK_ALLOC_INCR);
	p->mcnext = NULL;
	p->mcalloc = MARK_ALLOC_INCR;
	p->mcptr.cp = (uint8_t *)p + sizeof(MCHUNK);
	p->mcused = 0;

	if (firstmch == NULL)
		firstmch = p;

	if (curmch)
	{
		// Link onto previous chunk
		*markptr.wp++ = MCHEND;		// Mark end of block
		curmch->mcused = mcused;
		curmch->mcnext = p;
	}

	// Setup global vars
	curmch = p;
	markptr = p->mcptr;
	mcalloc = MARK_ALLOC_INCR;
	mcused = 0;

	return 0;
}


//
// Make mark image for Alcyon .o file
// okflag: 1, ok to deposit reloc information
//
uint32_t MarkImage(register uint8_t * mp, uint32_t siz, uint32_t tsize, int okflag)
{
	uint16_t from = 0;		// Section fixups are currently FROM
	uint32_t loc;			// Location (temp)
	uint32_t lastloc;		// Last location fixed up (RELMOD)
	uint8_t * wp;			// Pointer into raw relocation information
	register uint8_t * dp;	// Deposit point for RELMOD information

	if (okflag)
		memset(mp, 0, siz);		// zero relocation buffer

	for(MCHUNK * mch=firstmch; mch!=NULL; mch=mch->mcnext)
	{
		for(PTR p=mch->mcptr;;)
		{
			uint16_t w = *p.wp++;// w = next mark entry

			if (w & MCHEND)		// (end of mark chunk)
				break;

			// Get mark record
			SYM * symbol = NULL;
			loc = *p.lp++;		// mark location

			if (w & MCHFROM)	// maybe change "from" section
				from = *p.wp++;

			if (w & MSYMBOL)	// maybe includes a symbol
				symbol = *p.sy++;

			// Compute mark position in relocation information; in RELMOD mode,
			// get address of data to fix up.
			if (from == DATA)
				loc += tsize;

			wp = (uint8_t *)(mp + loc);

			if (okflag && (w & MLONG)) // indicate first word of long
			{
				wp[1] = 5;
				wp += 2;
			}

			if (symbol)
			{
				// Deposit external reference
				if (okflag)
				{
					if (w & MPCREL)
						w = 6;		// PC-relative fixup
					else
						w = 4;		// Absolute fixup

					w |= symbol->senv << 3;
					*wp++ = w >> 8;
					*wp = (uint8_t)w;
				}
			}
			else
			{
				// Deposit section-relative mark; in RELMOD mode, fix it up in
				// the chunk, kind of like a sleazoid linker.
				//
				// In RELMOD mode, marks to words (MWORDs) "cannot happen,"
				// checks are made when mark() is called, so we don't have to
				// check again here.
				w &= TDB;

				if (okflag)
					wp[1] = mark_tr[w];
				else if (prg_flag && (w & (DATA | BSS)))
				{
					uint32_t diff = GETBE32(wp, 0);
#ifdef DO_DEBUG
					DEBUG printf("diff=%lx ==> ", diff);
#endif
					diff += sect[TEXT].sloc;

					if (w == BSS)
						diff += sect[DATA].sloc;

					SETBE32(wp, 0, diff)
#ifdef DO_DEBUG
					DEBUG printf("%lx\n", diff);
#endif
				}
			}
		}
	}

	// Generate ".PRG" relocation information in place in the relocation words
	// (the "RELMOD" operation).
	if (okflag && prg_flag)
	{
		int firstp = 1;
		wp = dp = mp;

		for(loc=0; loc<siz;)
		{
			if ((wp[1] & 7) == 5)
			{
				if (firstp)
				{
					SETBE32(dp, 0, loc);
					dp += 4;
					firstp = 0;
				}
				else
				{
					uint32_t diff;

					for(diff=loc-lastloc; diff>254; diff-=254)
						*dp++ = 1;

					*dp++ = (uint8_t)diff;
				}

				lastloc = loc;
				loc += 4;
				wp += 4;
			}
			else
			{
				loc += 2;
				wp += 2;
			}
		}

		// Terminate relocation list with 0L (if there was no relocation) or
		// 0.B (if relocation information was written).
		if (!firstp)
			*dp++ = 0;
		else
			for(firstp=0; firstp<4; firstp++)
				*dp++ = 0;

		// Return size of relocation information
		loc = dp - mp;
		return loc;
	}

	return siz;
}


//
// Make mark image for BSD .o file
//
// Assumptions about mark records (for BSD): if there is a symbol, the mark is
// for an undefined symbol, otherwise it's just a normal TDB relocation.
// N.B.: tsize is only used if reqseg is DATA
//
uint32_t MarkBSDImage(uint8_t * mp, uint32_t siz, uint32_t tsize, int reqseg)
{
	uint16_t from = 0;			// Section fixups are currently FROM
	uint32_t rsize = 0;			// Relocation table size (written to mp)
	int validsegment = 0;		// We are not yet in a valid segment...

#ifdef DEBUG_IMAGE_MARKING
printf("MarkBSDImage():\n");
#endif
	// Initialize relocation table point (for D_foo macros)
	chptr = mp;

	// Run through all the relocation mark chunks
	for(MCHUNK * mch=firstmch; mch!=NULL; mch=mch->mcnext)
	{
		for(PTR p=mch->mcptr;;)
		{
			SYM * symbol = NULL;
			uint16_t w = *p.wp++;	// Next mark entry

			// If we hit the end of a chunk, go get the next one
			if (w & MCHEND)
				break;

			// Get the rest of the mark record
			uint32_t loc = *p.lp++;	// Mark location

			// Maybe change "from" section
			if (w & MCHFROM)
			{
				from = *p.wp++;

				if (((reqseg == TEXT) && (from == TEXT))
					|| ((reqseg == DATA) && (from == DATA)))
					validsegment = 1;
				else
					validsegment = 0;
			}

			// Maybe includes a symbol
			if (w & MSYMBOL)
				symbol = *p.sy++;

			if (!validsegment)
				continue;

#ifdef DEBUG_IMAGE_MARKING
printf(" validsegment: raddr = $%08X\n", loc);
#endif
			uint32_t rflag = 0x00000040;	// Absolute relocation

			if (w & MPCREL)
				rflag = 0x000000A0;			// PC-relative relocation

			// This flag tells the linker to WORD swap the LONG when doing the
			// relocation.
			if (w & MMOVEI)
				rflag |= 0x00000001;

			// This tells the linker to do a WORD relocation (otherwise it
			// defaults to doing a LONG, throwing things off for WORD sized
			// fixups)
			if (!(w & (MLONG | MQUAD)))
				rflag |= 0x00000002;

			// Tell the linker that the fixup is an OL QUAD data address
			if (w & MQUAD)
				rflag |= 0x00000004;

			if (symbol != NULL)
			{
				// Deposit external reference
				rflag |= 0x00000010;			// Set external reloc flag bit
				rflag |= (symbol->senv << 8);	// Put symbol index in flags

#ifdef DEBUG_IMAGE_MARKING
printf("  validsegment(2): rflag = $%08X\n", rflag);
#endif
			}
			else
			{
#ifdef DEBUG_IMAGE_MARKING
printf("  w = $%04X\n", w);
#endif
				w &= TDB;				// Set reloc flags to segment

				switch (w)
				{
				case TEXT: rflag |= 0x00000400; break;
				case DATA: rflag |= 0x00000600; break;
				case BSS:  rflag |= 0x00000800; break;
				}

#ifdef DEBUG_IMAGE_MARKING
printf("  validsegment(3): rflag = $%08X\n", rflag);
#endif
				// Fix relocation by adding in start of TEXT segment, since it's
				// currently relative to the start of the DATA (or BSS) segment
				if (w & (DATA | BSS))
				{
					uint8_t * dp = objImage + BSDHDRSIZE + loc;
					uint32_t olBitsSave = 0;

					// Bump the start of the section if it's DATA (& not TEXT)
					if (from == DATA)
						dp += tsize;

					uint32_t diff = (rflag & 0x02 ? GETBE16(dp, 0) : GETBE32(dp, 0));

					// Special handling for OP (data addr) relocation...
					if (rflag & 0x04)
					{
						olBitsSave = diff & 0x7FF;
						diff = (diff & 0xFFFFF800) >> 8;
					}

					DEBUG printf("diff=%uX ==> ", diff);
#ifdef DEBUG_IMAGE_MARKING
printf("  validsegment(4): diff = $%08X ", diff);
#endif
					if (rflag & 0x01)
						diff = WORDSWAP32(diff);

#ifdef DEBUG_IMAGE_MARKING
printf("(sect[TEXT].sloc=$%X) --> ", sect[TEXT].sloc);
#endif
					diff += sect[TEXT].sloc;

					if (w == BSS)
						diff += sect[DATA].sloc;

					if (rflag & 0x01)
						diff = WORDSWAP32(diff);

					// Make sure to deposit the correct size payload
					// N.B.: The braces around the SETBExx macros are needed
					//       because the macro supplies its own set of braces,
					//       thus leaving a naked semicolon afterwards to
					//       screw up the if/else structure. This is the price
					//       you pay when using macros pretending to be code.
					if (rflag & 0x02)		// WORD relocation
					{
						SETBE16(dp, 0, diff);
					}
					else if (rflag & 0x04)	// OP data address relocation
					{
						// We do it this way because we might have an offset
						// that is not a multiple of 8 and thus we need this in
						// place to prevent a bad address at link time. :-P As
						// a consequence of this, the highest address we can
						// have here is $1FFFF8.
						uint32_t diffsave = diff;
						diff = ((diff & 0x001FFFFF) << 11) | olBitsSave;
						SETBE32(dp, 0, diff);
						// But we need those 3 bits, otherwise we can get in
						// trouble with things like OL data that is in the cart
						// space, and BOOM! So the 2nd phrase of the fixup (it
						// will *always* have a 2nd phrase) has a few spare
						// bits, we chuck them in there.
						uint32_t p2 = GETBE32(dp, 8);
						p2 &= 0x1FFFFFFF;
						p2 |= (diffsave & 0x00E00000) << 8;
						SETBE32(dp, 8, p2);
					}
					else					// LONG relocation
					{
						SETBE32(dp, 0, diff);
					}

					DEBUG printf("%uX\n", diff);
#ifdef DEBUG_IMAGE_MARKING
printf("$%08X\n", diff);
#endif
				}
			}

			D_long(loc);		// Write relocation address
			D_long(rflag);		// Write relocation flags
			rsize += 0x08;		// Increment relocation size
		}
	}

	// Return relocation table's size
#ifdef DEBUG_IMAGE_MARKING
printf("  rsize = $%X\n", rsize);
#endif
	return rsize;
}


//
// Make mark image for RAW file
//
uint32_t MarkABSImage(uint8_t * mp, uint32_t siz, uint32_t tsize, int reqseg)
{
	uint16_t from = 0;			// Section fixups are currently FROM
	uint32_t rsize = 0;			// Relocation table size (written to mp)
	int validsegment = 0;		// We are not yet in a valid segment...

	// Initialize relocation table point (for D_foo macros)
	chptr = mp;

	// Run through all the relocation mark chunks
	for(MCHUNK * mch=firstmch; mch!=NULL; mch=mch->mcnext)
	{
		for (PTR p = mch->mcptr;;)
		{
			SYM * symbol = NULL;
			uint16_t w = *p.wp++;	// Next mark entry

			// If we hit the end of a chunk, go get the next one
			if (w & MCHEND)
				break;

			// Get the rest of the mark record
			uint32_t loc = *p.lp++;	// Mark location

			// Maybe change "from" section
			if (w & MCHFROM)
			{
				from = *p.wp++;

				if (((reqseg == TEXT) && (from == TEXT))
				|| ((reqseg == DATA) && (from == DATA)))
					validsegment = 1;
				else
					validsegment = 0;
			}

			// Maybe includes a symbol
			if (w & MSYMBOL)
				symbol = *p.sy++;

			if (!validsegment)
				continue;

			uint32_t rflag = 0x00000040;	// Absolute relocation

			if (w & MPCREL)
				rflag = 0x000000A0;			// PC-relative relocation

			// This flag tells the linker to WORD swap the LONG when doing the
			// relocation.
			if (w & MMOVEI)
				rflag |= 0x00000001;

			// This tells the linker to do a WORD relocation (otherwise it
			// defaults to doing a LONG, throwing things off for WORD sized
			// fixups)
			if (!(w & (MLONG | MQUAD)))
				rflag |= 0x00000002;

			// Tell the linker that the fixup is an OL QUAD data address
			if (w & MQUAD)
				rflag |= 0x00000004;

			if (symbol != NULL)
			{
				return error("Unresolved symbol when outputting raw image");
			}
			else
			{
				w &= TDB;				// Set reloc flags to segment

				switch (w)
				{
				case TEXT: rflag |= 0x00000400; break;
				case DATA: rflag |= 0x00000600; break;
				case BSS:  rflag |= 0x00000800; break;
				}

				// Fix relocation by adding in start of TEXT segment, since it's
				// currently relative to the start of the DATA (or BSS) segment
				uint8_t * dp = objImage + loc;
				uint32_t olBitsSave = 0;

				// Bump the start of the section if it's DATA (& not TEXT)
				if (from == DATA)
					dp += tsize;

				uint32_t diff = (rflag & 0x02 ? GETBE16(dp, 0) : GETBE32(dp, 0));

				if (w & (DATA | BSS))
				{
					// Special handling for OP (data addr) relocation...
					if (rflag & 0x04)
					{
						olBitsSave = diff & 0x7FF;
						diff = (diff & 0xFFFFF800) >> 8;
					}

					if (rflag & 0x01)
						diff = WORDSWAP32(diff);

					diff += sect[TEXT].sloc;

					if (w == BSS)
						diff += sect[DATA].sloc;
				}
				if ((rflag & 0x02) == 0)
				{
					diff += org68k_address;
				}

				if (rflag & 0x01)
					diff = WORDSWAP32(diff);

				// Make sure to deposit the correct size payload
				// Check comments in MarkBSDImage for more candid moments
				if (rflag & 0x02)		// WORD relocation
				{
					SETBE16(dp, 0, diff);
				}
				else if (rflag & 0x04)	// OP data address relocation
				{
					// We do it this way because we might have an offset
					// that is not a multiple of 8 and thus we need this in
					// place to prevent a bad address at link time. :-P As
					// a consequence of this, the highest address we can
					// have here is $1FFFF8.
					uint32_t diffsave = diff;
					diff = ((diff & 0x001FFFFF) << 11) | olBitsSave;
					SETBE32(dp, 0, diff);
					// But we need those 3 bits, otherwise we can get in
					// trouble with things like OL data that is in the cart
					// space, and BOOM! So the 2nd phrase of the fixup (it
					// will *always* have a 2nd phrase) has a few spare
					// bits, we chuck them in there.
					uint32_t p2 = GETBE32(dp, 8);
					p2 &= 0x1FFFFFFF;
					p2 |= (diffsave & 0x00E00000) << 8;
					SETBE32(dp, 8, p2);
				}
				else					// LONG relocation
				{
					SETBE32(dp, 0, diff);
				}
			}
		}
	}

	return OK;
}


//
// Make relocation record for ELF .o file.
// Returns the size of the relocation record.
//
uint32_t CreateELFRelocationRecord(uint8_t * buf, uint8_t * secBuf, uint16_t section)
{
	uint16_t from = 0;		// Section fixups are currently FROM
	uint32_t rsize = 0;		// Size of the relocation table

	// Setup pointer for D_long/word/byte macros
	chptr = buf;
	ch_size = 0;

	for(MCHUNK * mch=firstmch; mch!=NULL; mch=mch->mcnext)
	{
		for(register PTR p=mch->mcptr;;)
		{
			register uint16_t w = *p.wp++;	// w = next mark entry

			if (w & MCHEND)		// (end of mark chunk)
				break;

			// Get mark record
			SYM * symbol = NULL;
			uint16_t symFlags = 0;
			uint32_t r_offset = *p.lp++;	// Mark's location

			if (w & MCHFROM)		// Maybe change "from" section
				from = *p.wp++;

			if (w & MSYMBOL)		// Maybe includes a symbol
			{
				symbol = *p.sy++;

				if (symbol)
					symFlags = symbol->sattr;
			}

			// Create relocation record for ELF object, if the mark is in the
			// current section.
			if (from & section)
			{
				uint32_t r_sym = 0;
				uint32_t r_type = 0;
				uint32_t r_addend = 0;

				// Since we're chucking all symbols here for ELF objects by
				// default (cf. sect.c), we discriminate here (normally, if
				// there is a symbol in the mark record, it means an undefined
				// symbol) :-P
				if (symbol && !(symFlags & DEFINED) && (symFlags & GLOBAL))
					r_sym = symbol->senv + extraSyms;
				else if (w & TEXT)
					r_sym = elfHdrNum[ES_TEXT];	// Mark TEXT segment
				else if (w & DATA)
					r_sym = elfHdrNum[ES_DATA];	// Mark DATA segment
				else if (w & BSS)
					r_sym = elfHdrNum[ES_BSS];	// Mark BSS segment

				// Set the relocation type next
				if (w & MPCREL)
					r_type = 5;  // R_68K_PC16
				// N.B.: Since we've established that (from & section) is non-
				//       zero, this condition will *never* be satisfied... :-P
				//       It might be better to check the symbol's senv; that is,
				//       if this is a real problem that needs addressing...
				else if ((from & section) == 0)
					// In the case of a section referring to a label in another
					// section (for example text->data) use a R_68K_PC32 mark.
					r_type = 4;  // R_68K_PC32
				else
					r_type = 1;  // R_68K_32

				r_addend = GETBE32(secBuf + r_offset, 0);

				// Deposit the relocation record
				D_long(r_offset);
				D_long(((r_sym << 8) | r_type));
				D_long(r_addend);
				rsize += 0x0C;
			}
		}
	}

	return rsize;
}

