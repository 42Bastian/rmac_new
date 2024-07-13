//
// RMAC - Renamed Macro Assembler for all Atari computers
// 6502.C - 6502 Assembler
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//
//    Init6502	initialization
//    d_6502    handle ".6502" directive
//    m6502cg	generate code for a 6502 mnemonic
//    d_org	handle 6502 section's ".org" directive
//    m6502obj	generate 6502 object file
//
#include "direct.h"
#include "expr.h"
#include "error.h"
#include "mach.h"
#include "procln.h"
#include "riscasm.h"
#include "rmac.h"
#include "sect.h"
#include "token.h"

#define DEF_REG65
#define DECL_REG65
#include "6502regs.h"

#define	UPSEG_SIZE	0x10010L // size of 6502 code buffer, 64K+16bytes

// Internal vars
static uint16_t orgmap[1024][2];		// Mark all 6502 org changes

// Exported vars
const char in_6502mode[] = "directive illegal in .6502 section";
uint16_t * currentorg = &orgmap[0][0];	// Current org range
char strtoa8[128];	// ASCII to Atari 800 internal conversion table

//
// 6502 addressing modes;
// DO NOT CHANGE THESE VALUES.
//
#define	A65_ABS		0
#define	A65_ABSX	1
#define	A65_ABSY	2
#define	A65_IMPL	3
#define	A65_IMMED	4
#define	A65_INDX	5
#define	A65_INDY	6
#define	A65_IND		7
#define	A65_REL		8
#define	A65_ZP		9
#define	A65_ZPX		10
#define	A65_ZPY		11
#define A65_IMMEDH  12
#define A65_IMMEDL  13

#define	NMACHOPS	56		// Number of machine ops
#define	NMODES		14		// Number of addressing modes
#define	NOP			0xEA	// 6502 NOP instruction
#define	ILLEGAL		0xFF	// 'Illegal instr' marker
#define END65		0xFF	// End-of-an-instr-list

static char imodes[] =
{
	A65_IMMED, 0x69, A65_ABS, 0x6D, A65_ZP, 0x65, A65_INDX, 0x61, A65_INDY, 0x71,
	A65_ZPX, 0x75, A65_ABSX, 0x7D, A65_ABSY, 0x79, END65,
	A65_IMMED, 0x29, A65_ABS, 0x2D, A65_ZP, 0x25, A65_INDX, 0x21, A65_INDY, 0x31,
	A65_ZPX, 0x35, A65_ABSX, 0x3D, A65_ABSY, 0x39, END65,
	A65_ABS, 0x0E, A65_ZP, 0x06, A65_IMPL, 0x0A, A65_ZPX, 0x16, A65_ABSX,
	0x1E, END65,
	A65_REL, 0x90, END65,
	A65_REL, 0xB0, END65,
	A65_REL, 0xF0, END65,
	A65_REL, 0xD0, END65,
	A65_REL, 0x30, END65,
	A65_REL, 0x10, END65,
	A65_REL, 0x50, END65,
	A65_REL, 0x70, END65,
	A65_ABS, 0x2C, A65_ZP, 0x24, END65,
	A65_IMPL, 0x00, END65,
	A65_IMPL, 0x18, END65,
	A65_IMPL, 0xD8, END65,
	A65_IMPL, 0x58, END65,
	A65_IMPL, 0xB8, END65,
	A65_IMMED, 0xC9, A65_ABS, 0xCD, A65_ZP, 0xC5, A65_INDX, 0xC1, A65_INDY, 0xD1,
	A65_ZPX, 0xD5, A65_ABSX, 0xDD, A65_ABSY, 0xD9, END65,
	A65_IMMED, 0xE0, A65_ABS, 0xEC, A65_ZP, 0xE4, END65,
	A65_IMMED, 0xC0, A65_ABS, 0xCC, A65_ZP, 0xC4, END65,
	A65_ABS, 0xCE, A65_ZP, 0xC6, A65_ZPX, 0xD6, A65_ABSX, 0xDE, END65,
	A65_IMPL, 0xCA, END65,
	A65_IMPL, 0x88, END65,
	A65_IMMED, 0x49, A65_ABS, 0x4D, A65_ZP, 0x45, A65_INDX, 0x41, A65_INDY, 0x51,
	A65_ZPX, 0x55, A65_ABSX, 0x5D, A65_ABSY, 0x59, END65,
	A65_ABS, 0xEE, A65_ZP, 0xE6, A65_ZPX, 0xF6, A65_ABSX, 0xFE, END65,
	A65_IMPL, 0xE8, END65,
	A65_IMPL, 0xC8, END65,
	A65_ABS, 0x4C, A65_IND, 0x6C, END65,
	A65_ABS, 0x20, END65,
	A65_IMMED, 0xA9, A65_ABS, 0xAD, A65_ZP, 0xA5, A65_INDX, 0xA1, A65_INDY, 0xB1,
	A65_ZPX, 0xB5, A65_ABSX, 0xBD, A65_ABSY, 0xB9, A65_IMMEDH, 0xA9, A65_IMMEDL, 0xA9, END65,
	A65_IMMED, 0xA2, A65_ABS, 0xAE, A65_ZP, 0xA6, A65_ABSY, 0xBE,
	A65_ZPY, 0xB6, A65_IMMEDH, 0xA2, A65_IMMEDL, 0xA2, END65,
	A65_IMMED, 0xA0, A65_ABS, 0xAC, A65_ZP, 0xA4, A65_ZPX, 0xB4,
	A65_ABSX, 0xBC, A65_IMMEDH, 0xA0, A65_IMMEDL, 0xA0, END65,
	A65_ABS, 0x4E, A65_ZP, 0x46, A65_IMPL, 0x4A, A65_ZPX, 0x56,
	A65_ABSX, 0x5E, END65,
	A65_IMPL, 0xEA, END65,
	A65_IMMED, 0x09, A65_ABS, 0x0D, A65_ZP, 0x05, A65_INDX, 0x01, A65_INDY, 0x11,
	A65_ZPX, 0x15, A65_ABSX, 0x1D, A65_ABSY, 0x19, END65,
	A65_IMPL, 0x48, END65,
	A65_IMPL, 0x08, END65,
	A65_IMPL, 0x68, END65,
	A65_IMPL, 0x28, END65,
	A65_ABS, 0x2E, A65_ZP, 0x26, A65_IMPL, 0x2A, A65_ZPX, 0x36,
	A65_ABSX, 0x3E, END65,
	A65_ABS, 0x6E, A65_ZP, 0x66, A65_IMPL, 0x6A, A65_ZPX, 0x76,
	A65_ABSX, 0x7E, END65,
	A65_IMPL, 0x40, END65,
	A65_IMPL, 0x60, END65,
	A65_IMMED, 0xE9, A65_ABS, 0xED, A65_ZP, 0xE5, A65_INDX, 0xE1, A65_INDY, 0xF1,
	A65_ZPX, 0xF5, A65_ABSX, 0xFD, A65_ABSY, 0xF9, END65,
	A65_IMPL, 0x38, END65,
	A65_IMPL, 0xF8, END65,
	A65_IMPL, 0x78, END65,
	A65_ABS, 0x8D, A65_ZP, 0x85, A65_INDX, 0x81, A65_INDY, 0x91, A65_ZPX, 0x95,
	A65_ABSX, 0x9D, A65_ABSY, 0x99, END65,
	A65_ABS, 0x8E, A65_ZP, 0x86, A65_ZPY, 0x96, END65,
	A65_ABS, 0x8C, A65_ZP, 0x84, A65_ZPX, 0x94, END65,
	A65_IMPL, 0xAA, END65,
	A65_IMPL, 0xA8, END65,
	A65_IMPL, 0xBA, END65,
	A65_IMPL, 0x8A, END65,
	A65_IMPL, 0x9A, END65,
	A65_IMPL, 0x98, END65
};

static char ops[NMACHOPS][NMODES];			// Opcodes
static unsigned char inf[NMACHOPS][NMODES];	// Construction info

// Absolute-to-zeropage translation table
static int abs2zp[] =
{
	A65_ZP,		// ABS
	A65_ZPX,	// ABSX
	A65_ZPY,	// ABSY
	-1,			// IMPL
	-1,			// IMMED
	-1,			// INDX
	-1,			// INDY
	-1,			// IND
	-1,			// REL
	-1,			// ZP
	-1,			// ZPX
	-1			// ZPY
};

static char a8internal[] =
{
    ' ', 0,   '!', 1,   '"', 2,   '#', 3,   '$',  4,   '%', 5,   '&', 6,   '\'', 7,
    '(', 8,   ')', 9,   '*', 10,  '+', 11,  ',',  12,  '-', 13,  '.', 14,  '/',  15,
    '0', 16,  '1', 17,  '2', 18,  '3', 19,  '4',  20,  '5', 21,  '6', 22,  '7',  23,
    '8', 24,  '9', 25,  ':', 26,  ';', 27,  '<',  28,  '=', 29,  '>', 30,  '?',  31,
    '@', 32,  'A', 33,  'B', 34,  'C', 35,  'D',  36,  'E', 37,  'F', 38,  'G',  39,
    'H', 40,  'I', 41,  'J', 42,  'K', 43,  'L',  44,  'M', 45,  'N', 46,  'O',  47,
    'P', 48,  'Q', 49,  'R', 50,  'S', 51,  'T',  52,  'U', 53,  'V', 54,  'W',  55,
    'X', 56,  'Y', 57,  'Z', 58,  '[', 59,  '\\', 60,  ']', 61,  '^', 62,  '_',  63,
    'a', 97,  'b', 98,  'c', 99,  'd', 100, 'e',  101, 'f', 102, 'g', 103, 'h',  104,
    'i', 105, 'j', 106, 'k', 107, 'l', 108, 'm',  109, 'n', 110, 'o', 111, 'p',  112,
    'q', 113, 'r', 114, 's', 115, 't', 116, 'u',  117, 'v', 118, 'w', 119, 'x',  120,
    'y', 121, 'z', 122
};


//
// Initialize 6502 assembler
//
void Init6502()
{
	register int i;
	register int j;

	register char * s = imodes;

	// Set all instruction slots to illegal
	for(i=0; i<NMACHOPS; i++)
		for(j=0; j<NMODES; j++)
			inf[i][j] = ILLEGAL;

	// Uncompress legal instructions into their slots
	for(i=0; i<NMACHOPS; i++)
	{
		do
		{
			j = *s & 0xFF;
			inf[i][j] = *s;
			ops[i][j] = s[1];

			/* hack A65_REL mode */
			if (*s == A65_REL)
			{
				inf[i][A65_ABS] = A65_REL;
				ops[i][A65_ABS] = s[1];
				inf[i][A65_ZP] = A65_REL;
				ops[i][A65_ZP] = s[1];
			}
		}
		while (*(s += 2) != (char)END65);

		s++;
	}

	// Set up first org section (set to zero)
	orgmap[0][0] = 0;

	SwitchSection(M6502);	// Switch to 6502 section

	// Initialise string conversion table(s)
	char * p = a8internal;
	memset(strtoa8, 31, 128);   // 31=fallback value ("?")

	for(; p<a8internal+sizeof(a8internal); p+=2)
		strtoa8[p[0]] = p[1];

	if (challoc == 0)
	{
		// Allocate and clear 64K of space for the 6502 section
		chcheck(UPSEG_SIZE);
		memset(sect[M6502].scode->chptr, 0, UPSEG_SIZE);
	}

	SwitchSection(TEXT);    // Go back to TEXT
}


//
// .6502 --- enter 6502 mode
//
int d_6502()
{
	SaveSection();			// Save curent section
	SwitchSection(M6502);	// Switch to 6502 section
	regbase = reg65base;	// Update register DFA tables
	regtab = reg65tab;
	regcheck = reg65check;
	regaccept = reg65accept;
	used_architectures |= M6502;

	return 0;
}


//
// Do 6502 code generation
//
void m6502cg(int op)
{
	register int amode;		// (Parsed) addressing mode
	uint64_t eval = -1;		// Expression value
	WORD eattr = 0;			// Expression attributes
	int zpreq = 0;			// 1 = optimize instr to zero-page form
	ch_size = 0;			// Reset chunk size on every instruction

	//
	// Parse 6502 addressing mode
	//
	switch (tok[0])
	{
	case EOL:
		amode = A65_IMPL;
		break;

	case REG65_A:
		if (tok[1] != EOL)
			goto badmode;

		tok++;
		amode = A65_IMPL;
		break;

	case '#':
		tok++;

		if (*tok == '>')
		{
			tok++;
			amode = A65_IMMEDH;
		}
		else if (*tok == '<')
		{
			tok++;
			amode = A65_IMMEDL;
		}
		else
			amode = A65_IMMED;

		if (expr(exprbuf, &eval, &eattr, NULL) < 0)
			return;

		break;

	case '(':
		tok++;

		if (expr(exprbuf, &eval, &eattr, NULL) < 0)
			return;

		if (*tok == ')')
		{
			// (foo) or (foo),y
			if (*++tok == ',')
			{
				// (foo),y
				tok++;
				amode = A65_INDY;

				if (tok[0] != REG65_Y)
					goto badmode;

				tok++;
			}
			else
				amode = A65_IND;
		}
		else if ((tok[0] == ',') && (tok[1] == REG65_X) && (tok[2] == ')'))
		{
			// (foo,x)
			tok += 3;
			amode = A65_INDX;
		}
		else
			goto badmode;

		break;

	// I'm guessing that the form of this is @<expr>(X) or @<expr>(Y), which
	// I've *never* seen before.  :-/
	case '@':
		tok++;

		if (expr(exprbuf, &eval, &eattr, NULL) < 0)
			return;

		if (*tok == '(')
		{
			tok++;

			if ((tok[1] != ')') || (tok[2] != EOL))
				goto badmode;

			if (tok[0] == REG65_X)
				amode = A65_INDX;
			else if (tok[0] == REG65_Y)
				amode = A65_INDY;
			else
				goto badmode;

			tok += 2;
			zpreq = 1;		// Request zeropage optimization
		}
		else if (*tok == EOL)
			amode = A65_IND;
		else
			goto badmode;

		break;

	default:
		//   <expr>
		//   <expr>,x
		//   <expr>,y
		if (expr(exprbuf, &eval, &eattr, NULL) < 0)
			return;

		zpreq = 1;		// Request zeropage optimization

		if (tok[0] == EOL)
			amode = A65_ABS;
		else if (tok[0] == ',')
		{
			tok++;

			if (tok[0] == REG65_X)
			{
				tok++;
				amode = A65_ABSX;
			}
			else if (tok[0] == REG65_Y)
			{
				tok++;
				amode = A65_ABSY;
			}
		}
		else
			goto badmode;

		break;

badmode:
		error("bad 6502 addressing mode");
		return;
	}

	//
	// Optimize ABS modes to zero-page when possible
	//   o  ZPX or ZPY is illegal, or
	//   o  expr is zeropage && zeropageRequest && expression is defined
	//
	if ((inf[op][amode] == ILLEGAL)	// If current op is illegal OR
		|| (zpreq					// amode requested a zero-page optimize
			&& (eval < 0x100)		// and expr is zero-page
			&& (eattr & DEFINED)))	// and the expression is defined
	{
		int i = abs2zp[amode];		// Get zero-page translation of amode
#ifdef DO_DEBUG
		DEBUG printf(" OPT: op=%d amode=%d i=%d inf[op][i]=%d\n",
					 op, amode, i, inf[op][i]);
#endif
		if (i >= 0 && (inf[op][i] & 0xFF) != ILLEGAL) // & use it if it's legal
			amode = i;
	}

#ifdef DO_DEBUG
	DEBUG printf("6502: op=%d amode=%d ", op, amode);
	DEBUG printf("inf[op][amode]=%d\n", (int)inf[op][amode]);
#endif

	GENLINENOSYM();

	switch (inf[op][amode])
	{
		case A65_IMPL:		// Just leave the instruction
			D_byte(ops[op][amode]);
			break;

		case A65_IMMEDH:
			D_byte(ops[op][amode]);

			if (!(eattr & DEFINED))
			{
				AddFixup(FU_BYTEH, sloc, exprbuf);
				eval = 0;
			}

			eval = (eval >> 8) & 0xFF; // Bring high byte to low
			D_byte(eval);				// Deposit byte following instr
			break;

		case A65_IMMEDL:
			D_byte(ops[op][amode]);

			if (!(eattr & DEFINED))
			{
				AddFixup(FU_BYTEL, sloc, exprbuf);
				eval = 0;
			}

			eval = eval & 0xFF; // Mask high byte
			D_byte(eval);		// Deposit byte following instr
			break;

		case A65_IMMED:
		case A65_INDX:
		case A65_INDY:
		case A65_ZP:
		case A65_ZPX:
		case A65_ZPY:
			D_byte(ops[op][amode]);

			if (!(eattr & DEFINED))
			{
				AddFixup(FU_BYTE, sloc, exprbuf);
				eval = 0;
			}
			else if (eval + 0x100 >= 0x200)
			{
				error(range_error);
				eval = 0;
			}

			D_byte(eval);		// Deposit byte following instr
			break;

		case A65_REL:
			D_byte(ops[op][amode]);

			if (eattr & DEFINED)
			{
				eval -= (sloc + 1);

				if (eval + 0x80 >= 0x100)
				{
					error(range_error);
					eval = 0;
				}

				D_byte(eval);
			}
			else
			{
				AddFixup(FU_6BRA, sloc, exprbuf);
				D_byte(0);
			}

			break;

		case A65_ABS:
		case A65_ABSX:
		case A65_ABSY:
		case A65_IND:
			D_byte(ops[op][amode]);

			if (!(eattr & DEFINED))
			{
				AddFixup(FU_WORD, sloc, exprbuf);
				eval = 0;
			}

			D_rword(eval);
			break;

		//
		// Deposit 3 NOPs for illegal things (why 3? why not 30? or zero?)
		//
		default:
		case ILLEGAL:
			D_byte(NOP);
			D_byte(NOP);
			D_byte(NOP);

			error("illegal 6502 addressing mode");
	}

	// Check for overflow of code region
	if (sloc > 0x10000L)
		fatal("6502 code pointer > 64K");

	ErrorIfNotAtEOL();
}


//
// Generate 6502 object output file.
// ggn: Converted to COM/EXE/XEX output format
//
void m6502obj(int ofd)
{
	uint8_t header[4];

	CHUNK * ch = sect[M6502].scode;

	// If no 6502 code was generated, bail out
	if ((ch == NULL) || (ch->challoc == 0))
		return;

	register uint8_t * p = ch->chptr;

	// Write out mandatory $FFFF header
	header[0] = header[1] = 0xFF;
	uint32_t unused = write(ofd, header, 2);

	for(uint16_t * l=&orgmap[0][0]; l<currentorg; l+=2)
	{
		SETLE16(header, 0, l[0]);
		SETLE16(header, 2, l[1] - 1);

		// Write header for segment
		unused = write(ofd, header, 4);
		// Write the segment data
		unused = write(ofd, p + l[0], l[1] - l[0]);
	}
}


// Write raw 6502 org'd code.
// Super copypasta'd from above function
void m6502raw(int ofd)
{
	CHUNK * ch = sect[M6502].scode;

	// If no 6502 code was generated, bail out
	if ((ch == NULL) || (ch->challoc == 0))
		return;

	register uint8_t *p = ch->chptr;

	for(uint16_t * l=&orgmap[0][0]; l<currentorg; l+=2)
	{
		// Write the segment data
		uint32_t unused = write(ofd, p + l[0], l[1] - l[0]);
	}
}


//
// Generate a C64 .PRG output file
//
void m6502c64(int ofd)
{
	uint8_t header[2];

	CHUNK * ch = sect[M6502].scode;

	// If no 6502 code was generated, bail out
	if ((ch == NULL) || (ch->challoc == 0))
		return;

	if (currentorg != &orgmap[1][0])
	{
		// More than one 6502 section created, this is not allowed
		error("when generating C64 .PRG files only one org section is allowed - aborting");
		return;
	}

	SETLE16(header, 0, orgmap[0][0]);
	register uint8_t * p = ch->chptr;

	// Write header
	uint32_t unused = write(ofd, header, 2);
	// Write the data
	unused = write(ofd, p + orgmap[0][0], orgmap[0][1] - orgmap[0][0]);
}
