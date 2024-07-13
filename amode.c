//
// RMAC - Renamed Macro Assembler for all Atari computers
// AMODE.C - Addressing Modes
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "amode.h"
#include "error.h"
#include "expr.h"
#include "mach.h"
#include "procln.h"
#include "rmac.h"
#include "sect.h"
#include "token.h"

#define DEF_REG68
#include "68kregs.h"
#define DEF_MN
#include "mntab.h"

extern char unsupport[];

// Address-mode information
int nmodes;					// Number of addr'ing modes found
int am0;					// Addressing mode
int a0reg;					// Register
TOKEN a0expr[EXPRSIZE];		// Expression
uint64_t a0exval;			// Expression's value
WORD a0exattr;				// Expression's attribute
int a0ixreg;				// Index register
int a0ixsiz;				// Index register size (and scale)
SYM * a0esym;				// External symbol involved in expr
TOKEN a0bexpr[EXPRSIZE];	// Base displacement expression
uint64_t a0bexval;			// Base displacement value
WORD a0bexattr;				// Base displacement attribute
WORD a0bsize;				// Base displacement size
WORD a0extension;			// 020+ extension address word
WORD am0_030;				// ea bits for 020+ addressing modes

int am1;					// Addressing mode
int a1reg;					// Register
TOKEN a1expr[EXPRSIZE];		// Expression
uint64_t a1exval;			// Expression's value
WORD a1exattr;				// Expression's attribute
int a1ixreg;				// Index register
int a1ixsiz;				// Index register size (and scale)
SYM * a1esym;				// External symbol involved in expr
TOKEN a1bexpr[EXPRSIZE];	// Base displacement expression
uint64_t a1bexval;			// Base displacement value
WORD a1bexattr;				// Base displacement attribute
WORD a1bsize;				// Base displacement size
WORD a1extension;			// 020+ extension address word
WORD am1_030;				// ea bits for 020+ addressing modes

int a2reg;					// Register for div.l (68020+)

int bfparam1;				// bfxxx / fmove instruction parameter 1
int bfparam2;				// bfxxx / fmove instruction parameter 2
int bfval1;					// bfxxx / fmove value 1
int bfval2;					// bfxxx / fmove value 2
TOKEN bf0expr[EXPRSIZE];	// Expression
uint64_t bf0exval;			// Expression's value
WORD bf0exattr;				// Expression's attribute
SYM * bf0esym;				// External symbol involved in expr

// Function prototypes
int Check030Bitfield(void);


//
// Parse addressing mode
//
int amode(int acount)
{
	// Initialize global return values
	nmodes = a0reg = a1reg = 0;
	am0 = am1 = AM_NONE;
	a0expr[0] = a1expr[0] = ENDEXPR;
	a0exattr = a1exattr = 0;
	a0esym = a1esym = NULL;
	a0bexpr[0] = a1bexpr[0] = ENDEXPR;
	a0bexval = a1bexval = 0;
	a0bsize = a0extension = a1bsize = a1extension = 0;
	am0_030 = am1_030 = 0;
	bfparam1 = bfparam2 = 0;
	bf0expr[0] = ENDEXPR;
	bf0exattr = 0;
	bf0esym = NULL;

	// If at EOL, then no addr modes at all
	if (*tok == EOL)
		return 0;

	// Parse first addressing mode
	#define AnOK      a0ok
	#define AMn       am0
	#define AnREG     a0reg
	#define AnIXREG   a0ixreg
	#define AnIXSIZ   a0ixsiz
	#define AnEXPR    a0expr
	#define AnEXVAL   a0exval
	#define AnEXATTR  a0exattr
	#define AnESYM    a0esym
	#define AMn_IX0   am0_ix0
	#define AMn_IXN   am0_ixn
	#define CHK_FOR_DISPn CheckForDisp0
	#define AnBEXPR   a0bexpr
	#define AnBEXVAL  a0bexval
	#define AnBEXATTR a0bexattr
	#define AnBZISE   a0bsize
	#define AnEXTEN   a0extension
	#define AMn_030   am0_030
	#define IS_SUPPRESSEDn IS_SUPPRESSED0
	#define CHECKODn CHECKOD0
	#include "parmode.h"

	// If caller wants only one mode, return just one (ignore comma);. If there
	// is no second addressing mode (no comma), then return just one anyway.
	nmodes = 1;

	// it's a bitfield instruction--check the parameters inside the {} block
	// for validity
	if (*tok == '{')
		if (Check030Bitfield() == ERROR)
			return ERROR;

	if ((acount == 0) || (*tok != ','))
		return 1;

	// Eat the comma
	tok++;

	// Parse second addressing mode
	#define AnOK      a1ok
	#define AMn       am1
	#define AnREG     a1reg
	#define AnIXREG   a1ixreg
	#define AnIXSIZ   a1ixsiz
	#define AnEXPR    a1expr
	#define AnEXVAL   a1exval
	#define AnEXATTR  a1exattr
	#define AnESYM    a1esym
	#define AMn_IX0   am1_ix0
	#define AMn_IXN   am1_ixn
	#define CHK_FOR_DISPn CheckForDisp1
	#define AnBEXPR   a1bexpr
	#define AnBEXVAL  a1bexval
	#define AnBEXATTR a1bexattr
	#define AnBZISE   a1bsize
	#define AnEXTEN   a1extension
	#define AMn_030   am1_030
	#define IS_SUPPRESSEDn IS_SUPPRESSED1
	#define CHECKODn CHECKOD1
	#include "parmode.h"

	// It's a bitfield instruction--check the parameters inside the {} block
	// for validity
	if (*tok == '{')
        if (Check030Bitfield() == ERROR)
		return ERROR;

	// At this point, it is legal for 020+ to have a ':'. For example divu.l
	// d0,d2:d3
	if (*tok == ':')
	{
		if ((activecpu & (CPU_68020 | CPU_68030 | CPU_68040)) == 0)
			return error(unsupport);

		// TODO: protect this from combinations like Dx:FPx etc :)
		tok++;  //eat the colon

		if ((*tok >= REG68_D0) && (*tok <= REG68_D7))
			a2reg = (*tok++) & 7;
		else if ((*tok >= REG68_FP0) && (*tok <= REG68_FP7))
			a2reg = (*tok++) & 7;
		else
			return error("a data or FPU register must follow a :");
	}
	else
	{
		// If no ':' is present then maybe we have something like divs.l d0,d1
		// which sould translate to divs.l d0,d1:d1
		a2reg = a1reg;
	}

	nmodes = 2;
	return 2;

	// Error messages:
badmode:
	return error("addressing mode syntax");
}


//
// Parse register list
//
int reglist(WORD * a_rmask)
{
	static WORD msktab[] = {
		0x0001, 0x0002, 0x0004, 0x0008,
		0x0010, 0x0020, 0x0040, 0x0080,
		0x0100, 0x0200, 0x0400, 0x0800,
		0x1000, 0x2000, 0x4000, 0x8000
	};

	WORD rmask = 0;
	int r, cnt;

	for(;;)
	{
		if ((*tok >= REG68_D0) && (*tok <= REG68_A7))
			r = *tok++ & 0x0F;
		else
			break;

		if (*tok == '-')
		{
			tok++;

			if ((*tok >= REG68_D0) && (*tok <= REG68_A7))
				cnt = *tok++ & 0x0F;
			else
				return error("register list syntax");

			if (cnt < r)
				return error("register list order");

			cnt -= r;
		}
		else
			cnt = 0;

		while (cnt-- >= 0)
			rmask |= msktab[r++];

		if (*tok != '/')
			break;

		tok++;
	}

	*a_rmask = rmask;

	return OK;
}


//
// Parse FPU register list
//
int fpu_reglist_left(WORD * a_rmask)
{
	static WORD msktab_minus[] = {
		0x0080, 0x0040, 0x0020, 0x0010,
		0x0008, 0x0004, 0x0002, 0x0001
	};

	WORD rmask = 0;
	int r, cnt;

	for(;;)
	{
		if ((*tok >= REG68_FP0) && (*tok <= REG68_FP7))
			r = *tok++ & 0x07;
		else
			break;

		if (*tok == '-')
		{
			tok++;

			if ((*tok >= REG68_FP0) && (*tok <= REG68_FP7))
				cnt = *tok++ & 0x07;
			else
				return error("register list syntax");

			if (cnt < r)
				return error("register list order");

			cnt -= r;
		}
		else
			cnt = 0;

		r = 0;

		while (cnt-- >= 0)
			rmask |= msktab_minus[r++];

		if (*tok != '/')
			break;

		tok++;
	}

	*a_rmask = rmask;

	return OK;
}


int fpu_reglist_right(WORD * a_rmask)
{
	static WORD msktab_plus[] = {
		0x0080, 0x0040, 0x0020, 0x0010,
		0x0008, 0x0004, 0x0002, 0x0001
	};

	WORD rmask = 0;
	int r, cnt;

	for(;;)
	{
		if ((*tok >= REG68_FP0) && (*tok <= REG68_FP7))
			r = *tok++ & 0x07;
		else
			break;

		if (*tok == '-')
		{
			tok++;

			if ((*tok >= REG68_FP0) && (*tok <= REG68_FP7))
				cnt = *tok++ & 0x07;
			else
				return error("register list syntax");

			if (cnt < r)
				return error("register list order");

			cnt -= r;
		}
		else
			cnt = 0;

		while (cnt-- >= 0)
			rmask |= msktab_plus[r++];

		if (*tok != '/')
			break;

		tok++;
	}

	*a_rmask = rmask;

	return OK;
}


//
// Check for bitfield instructions extra params
// These are 020+ instructions and have the following syntax:
// bfxxx <ea>{param1,param2}
// param1/2 are either data registers or immediate values
//
int Check030Bitfield(void)
{
	PTR tp;
	CHECK00;
	tok++;

	if (*tok == CONST)
	{
		tp.u32 = tok + 1;
		bfval1 = (int)*tp.u64++;
		tok = tp.u32;

		// Do=0, offset=immediate - shift it to place
		bfparam1 = (0 << 11);
	}
	else if (*tok == SYMBOL)
	{
		if (expr(bf0expr, &bf0exval, &bf0exattr, &bf0esym) != OK)
			return ERROR;

		if (!(bf0exattr & DEFINED))
			return error("bfxxx offset: immediate value must evaluate");

		bfval1 = (int)bf0exval;

		// Do=0, offset=immediate - shift it to place
		bfparam1 = (0 << 11);
	}
	else if ((*tok >= REG68_D0) && (*tok <= REG68_D7))
	{
		// Do=1, offset=data register - shift it to place
		bfparam1 = (1 << 11);
		bfval1 = (*(int *)tok - 128);
		tok++;
	}
	else
		return ERROR;

	// Eat the ':', if any
	if (*tok == ':')
		tok++;

	if (*tok == '}' && tok[1] == EOL)
	{
		// It is ok to have }, EOL here - it might be "fmove fpn,<ea> {dx}"
		tok++;
		return OK;
	}

	if (*tok == CONST)
	{
		tp.u32 = tok + 1;
		bfval2 = (int)*tp.u64++;
		tok = tp.u32;

		// Do=0, offset=immediate - shift it to place
		bfparam2 = (0 << 5);
	}
	else if (*tok == SYMBOL)
	{
		if (expr(bf0expr, &bf0exval, &bf0exattr, &bf0esym) != OK)
			return ERROR;

		bfval2 = (int)bf0exval;

		if (!(bf0exattr & DEFINED))
			return error("bfxxx width: immediate value must evaluate");

		// Do=0, offset=immediate - shift it to place
		bfparam2 = (0 << 5);
	}
	else if ((*tok >= REG68_D0) && (*tok <= REG68_D7))
	{
		// Do=1, offset=data register - shift it to place
		bfval2 = (*(int *)tok - 128);
		bfparam2 = (1 << 5);
		tok++;
	}
	else
		return ERROR;

	tok++;	// Eat the '}'

	return OK;
}

