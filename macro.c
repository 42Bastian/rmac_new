//
// RMAC - Renamed Macro Assembler for all Atari computers
// MACRO.C - Macro Definition and Invocation
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "macro.h"
#include "debug.h"
#include "direct.h"
#include "error.h"
#include "expr.h"
#include "listing.h"
#include "procln.h"
#include "symbol.h"
#include "token.h"


LONG curuniq;				// Current macro's unique number
int macnum;					// Unique number for macro definition

static LONG macuniq;		// Unique-per-macro number
static SYM * curmac;		// Macro currently being defined
static uint32_t argno;		// Formal argument count
LONG reptuniq;				// Unique-per-rept number

static LLIST * firstrpt;	// First .rept line
static LLIST * nextrpt;		// Last .rept line
int rptlevel;				// .rept nesting level

// Function prototypes
static int KWMatch(char *, char *);
static int LNCatch(int (*)(), char *);


//
// Initialize macro processor
//
void InitMacro(void)
{
	macuniq = 0;
	macnum = 1;
	reptuniq = 0;
}


//
// Exit from a macro;
// -- pop any intervening include files and repeat blocks;
// -- restore argument stack;
// -- pop the macro.
//
int ExitMacro(void)
{
WARNING(!!! Bad macro exiting !!!)
/*
This is a problem. Currently, the argument logic just keeps the current
arguments and doesn't save anything if a new macro is called in the middle
of another (nested macros). Need to fix that somehow.

Is this still true, now that we have IMACROs with TOKENSTREAMs in them? Need to
check it out for sure...!
*/
	// Pop intervening include files and .rept blocks
	while (cur_inobj != NULL && cur_inobj->in_type != SRC_IMACRO)
		fpop();

	if (cur_inobj == NULL)
		fatal("too many ENDMs");

	// Restore
	// o  old arg context
	// o  old unique number
	// ...and then pop the macro.

	IMACRO * imacro = cur_inobj->inobj.imacro;
	curuniq = imacro->im_olduniq;

	DEBUG { printf("ExitMacro: nargs = %d\n", imacro->im_nargs); }

	return fpop();
}


//
// Add a formal argument to a macro definition
//
int defmac2(char * argname)
{
	if (lookup(argname, MACARG, (int)curmac->sattr) != NULL)
		return error("multiple formal argument definition");

	SYM * arg = NewSymbol(argname, MACARG, (int)curmac->sattr);
	arg->svalue = argno++;

	return OK;
}


//
// Add a line to a macro definition; also print lines to listing file (if
// enabled). The last line of the macro (containing .endm) is not included in
// the macro. A label on that line will be lost.
// notEndFlag is -1 for all lines but the last one (.endm), when it is 0.
//
int defmac1(char * ln, int notEndFlag)
{
	if (list_flag)
	{
		listeol();		// Flush previous source line
		lstout('.');	// Mark macro definition with period
	}

	if (notEndFlag)
	{
		if (curmac->lineList == NULL)
		{
			curmac->lineList = malloc(sizeof(LLIST));
			curmac->lineList->next = NULL;
			curmac->lineList->line = strdup(ln);
			curmac->lineList->lineno = curlineno;
			curmac->last = curmac->lineList;
		}
		else
		{
			curmac->last->next = malloc(sizeof(LLIST));
			curmac->last->next->next = NULL;
			curmac->last->next->line = strdup(ln);
			curmac->lineList->lineno = curlineno;
			curmac->last = curmac->last->next;
		}

		return 1;		// Keep looking
	}

	return 0;			// Stop looking; at the end
}


//
// Define macro
//
// macro foo arg1,arg2,...
//    :
//     :
//    endm
//
//  Helper functions:
//  -----------------
//  `defmac1' adds lines of text to the macro definition
//  `defmac2' processes the formal arguments (and sticks them into the symbol
//   table)
//
int DefineMacro(void)
{
	// Setup entry in symbol table, make sure the macro isn't a duplicate
	// entry, and that it doesn't override any processor mnemonic or assembler
	// directive.
	if (*tok++ != SYMBOL)
		return error("missing symbol");

	char * name = string[*tok++];

	if (lookup(name, MACRO, 0) != NULL)
		return error("duplicate macro definition");

	curmac = NewSymbol(name, MACRO, 0);
	curmac->svalue = 0;
	curmac->sattr = (WORD)(macnum++);

	// Parse and define formal arguments in symbol table
	if (*tok != EOL)
	{
		argno = 0;
		symlist(defmac2);
		ErrorIfNotAtEOL();
	}

	// Suck in the macro definition; we're looking for an ENDM symbol on a line
	// by itself to terminate the definition.
//	curmln = NULL;
	curmac->lineList = NULL;
	LNCatch(defmac1, "endm ");

	return 0;
}


//
// Add lines to a .rept definition
//
int defr1(char * line, int kwno)
{
	if (list_flag)
	{
		listeol();			// Flush previous source line
		lstout('#');		// Mark this a 'rept' block
	}

	if (kwno == 0)			// .endr
	{
		if (--rptlevel == 0)
			return 0;
	}
	else if (kwno == 1)		// .rept
		rptlevel++;

//DEBUG { printf("  defr1: line=\"%s\", kwno=%d, rptlevel=%d\n", line, kwno, rptlevel); }

#if 0
//MORE stupidity here...
WARNING("!!! Casting (char *) as LONG !!!")
	// Allocate length of line + 1('\0') + LONG
	LONG * p = (LONG *)malloc(strlen(line) + 1 + sizeof(LONG));
	*p = 0;
	strcpy((char *)(p + 1), line);

	if (nextrpt == NULL)
		firstrpt = p;		// First line of rept statement
	else
		*nextrpt = (LONG)p;

	nextrpt = p;
#else
	if (firstrpt == NULL)
	{
		firstrpt = malloc(sizeof(LLIST));
		firstrpt->next = NULL;
		firstrpt->line = strdup(line);
		firstrpt->lineno = curlineno;
		nextrpt = firstrpt;
	}
	else
	{
		nextrpt->next = malloc(sizeof(LLIST));
		nextrpt->next->next = NULL;
		nextrpt->next->line = strdup(line);
		nextrpt->next->lineno = curlineno;
		nextrpt = nextrpt->next;
	}
#endif

	return rptlevel;
}


//
// Handle a .rept block; this gets hairy because they can be nested
//
int HandleRept(void)
{
	uint64_t eval;

	// Evaluate repeat expression
	if (abs_expr(&eval) != OK)
		return ERROR;

	// Suck in lines for .rept block
	firstrpt = NULL;
	nextrpt = NULL;
	rptlevel = 1;
	LNCatch(defr1, "endr rept ");

//DEBUG { printf("HandleRept: firstrpt=$%X\n", firstrpt); }
	// Alloc and init input object
	if (firstrpt)
	{
		INOBJ * inobj = a_inobj(SRC_IREPT);	// Create a new REPT input object
		IREPT * irept = inobj->inobj.irept;
		irept->ir_firstln = firstrpt;
		irept->ir_nextln = NULL;
		irept->ir_count = (uint32_t)eval;
	}

	return 0;
}


//
// Hand off lines of text to the function 'lnfunc' until a line containing one
// of the directives in 'dirlist' is encountered.
//
// 'dirlist' contains space-separated terminated keywords. A final space
// terminates the list. Directives are case-insensitively compared to the
// keywords.
//
// If 'lnfunc' is NULL, then lines are simply skipped.
// If 'lnfunc' returns an error, processing is stopped.
//
// 'lnfunc' is called with an argument of -1 for every line but the last one,
// when it is called with an argument of the keyword number that caused the
// match.
//
static int LNCatch(int (* lnfunc)(), char * dirlist)
{
	if (lnfunc != NULL)
		lnsave++;			// Tell tokenizer to keep lines

	while (1)
	{
		if (TokenizeLine() == TKEOF)
		{
			error("encountered end-of-file looking for '%s'", dirlist);
			fatal("cannot continue");
		}

		DEBUG { DumpTokenBuffer(); }

		// Test for end condition.  Two cases to handle:
		//            <directive>
		//    symbol: <directive>
		char * p = NULL;
		int k = -1;

		if (*tok == SYMBOL)
		{
			// A string followed by a colon or double colon is a symbol and
			// *not* a directive, see if we can find the directive after it
			if ((tok[2] == ':' || tok[2] == DCOLON))
			{
				if (tok[3] == SYMBOL)
					p = string[tok[4]];
			}
			else
			{
				// Otherwise, just grab the directive
				p = string[tok[1]];
			}
		}

		if (p != NULL)
		{
			if (*p == '.')		// Ignore leading periods
				p++;

			k = KWMatch(p, dirlist);
		}

		// Hand-off line to function
		// if it returns 0, and we found a keyword, stop looking.
		// if it returns 1, hand off the line and keep looking.
		if (lnfunc != NULL)
			k = (*lnfunc)(lnbuf, k);

		if (k == 0)
			break;
	}

	if (lnfunc != NULL)
		lnsave--;				// Tell tokenizer to stop keeping lines

	return 0;
}


//
// See if the string `kw' matches one of the keywords in `kwlist'. If so,
// return the number of the keyword matched. Return -1 if there was no match.
// Strings are compared without regard for case.
//
static int KWMatch(char * kw, char * kwlist)
{
	for(int k=0; *kwlist; k++)
	{
		for(char * p=kw;;)
		{
			char c1 = *kwlist++;
			char c2 = *p++;

			if (c2 >= 'A' && c2 <= 'Z')
				c2 += 32;

			if (c1 == ' ' && c2 == EOS)
				return k;

			if (c1 != c2)
				break;
		}

		// Skip to beginning of next keyword in `kwlist'
		while (*kwlist && (*kwlist != ' '))
			++kwlist;

		if (*kwlist== ' ')
			kwlist++;
	}

	return -1;
}


//
// Invoke a macro by creating a new IMACRO object & chopping up the arguments
//
int InvokeMacro(SYM * mac, WORD siz)
{
	DEBUG { printf("InvokeMacro: arguments="); DumpTokens(tok); }

	INOBJ * inobj = a_inobj(SRC_IMACRO);	// Alloc and init IMACRO
	IMACRO * imacro = inobj->inobj.imacro;
	uint16_t nargs = 0;

	// Chop up the arguments, if any (tok comes from token.c, which at this
	// point points at the macro argument token stream)
	if (*tok != EOL)
	{
		// Parse out the arguments and set them up correctly
		TOKEN * p = imacro->argument[nargs].token;
		int stringNum = 0;
		int numTokens = 0;

		while (*tok != EOL)
		{
			if (*tok == ACONST)
			{
				// Sanity checking (it's numTokens + 1 because we need an EOL
				// if we successfully parse this argument)
				if ((numTokens + 3) >= TS_MAXTOKENS)
					return error("Too many tokens in argument #%d in MACRO invocation", nargs + 1);

				for(int i=0; i<3; i++)
					*p++ = *tok++;

				numTokens += 3;
			}
			else if (*tok == CONST)		// Constants are 64-bits
			{
				// Sanity checking (it's numTokens + 1 because we need an EOL
				// if we successfully parse this argument)
				if ((numTokens + 3) >= TS_MAXTOKENS)
					return error("Too many tokens in argument #%d in MACRO invocation", nargs + 1);

				*p++ = *tok++;			// Token
				uint64_t *p64 = (uint64_t *)p;
				uint64_t *tok64 = (uint64_t *)tok;
				*p64++ = *tok64++;
				tok = (TOKEN *)tok64;
				p = (uint32_t *)p64;
				numTokens += 3;
			}
			else if ((*tok == STRING) || (*tok == SYMBOL))
			{
				// Sanity checking (it's numTokens + 1 because we need an EOL
				// if we successfully parse this argument)
				if (stringNum >= TS_MAXSTRINGS)
					return error("Too many strings in argument #%d in MACRO invocation", nargs + 1);

				if ((numTokens + 2) >= TS_MAXTOKENS)
					return error("Too many tokens in argument #%d in MACRO invocation", nargs + 1);

				*p++ = *tok++;
				imacro->argument[nargs].string[stringNum] = strdup(string[*tok++]);
				*p++ = stringNum++;
				numTokens += 2;
			}
			else if (*tok == ',')
			{
				// Sanity checking
				if ((nargs + 1) >= TS_MAXARGS)
					return error("Too many arguments in MACRO invocation");

				// Comma delimiter was found, so set up for next argument
				*p++ = EOL;
				tok++;
				stringNum = 0;
				numTokens = 0;
				nargs++;
				p = imacro->argument[nargs].token;
			}
			else
			{
				// Sanity checking (it's numTokens + 1 because we need an EOL
				// if we successfully parse this argument)
				if ((numTokens + 1) >= TS_MAXTOKENS)
					return error("Too many tokens in argument #%d in MACRO invocation", nargs + 1);

				*p++ = *tok++;
				numTokens++;
			}
		}

		// Make sure to stuff the final EOL (otherwise, it will be skipped)
		*p++ = EOL;
		nargs++;
	}

	// Setup IMACRO:
	//  o  # arguments;
	//  o  -> macro symbol;
	//  o  -> macro definition string list;
	//  o  save 'curuniq', to be restored when the macro pops;
	//  o  bump `macuniq' counter and set 'curuniq' to it;
	imacro->im_nargs = nargs;
	imacro->im_macro = mac;
	imacro->im_siz = siz;
	imacro->im_nextln = mac->lineList;
	imacro->im_olduniq = curuniq;
	curuniq = macuniq++;

	DEBUG
	{
		printf("# args = %d\n", nargs);

		for(uint16_t i=0; i<nargs; i++)
		{
			printf("arg%d=", i);
			DumpTokens(imacro->argument[i].token);
		}
	}

	return OK;
}

