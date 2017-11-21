//
// RMAC - Reboot's Macro Assembler for all Atari computers
// EXPR.C - Expression Analyzer
// Copyright (C) 199x Landon Dyer, 2011-2017 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "expr.h"
#include "direct.h"
#include "error.h"
#include "listing.h"
#include "mach.h"
#include "procln.h"
#include "riscasm.h"
#include "sect.h"
#include "symbol.h"
#include "token.h"

#define DEF_KW						// Declare keyword values
#include "kwtab.h"					// Incl generated keyword tables & defs

// N.B.: The size of tokenClass should be identical to the largest value of
//       a token; we're assuming 256 but not 100% sure!
static char tokenClass[256];		// Generated table of token classes
static uint64_t evstk[EVSTACKSIZE];	// Evaluator value stack
static WORD evattr[EVSTACKSIZE];	// Evaluator attribute stack

// Token-class initialization list
char itokcl[] = {
	0,								// END
	CONST, FCONST, SYMBOL, 0,		// ID
	'(', '[', '{', 0,				// OPAR
	')', ']', '}', 0,				// CPAR
	CR_DEFINED, CR_REFERENCED,		// SUNARY (special unary)
	CR_STREQ, CR_MACDEF,
	CR_DATE, CR_TIME,
	CR_ABSCOUNT, 0,
	'!', '~', UNMINUS, 0,			// UNARY
	'*', '/', '%', 0,				// MULT
	'+', '-', 0,					// ADD
	SHL, SHR, 0,					// SHIFT
	LE, GE, '<', '>', NE, '=', 0,	// REL
	'&', 0,							// AND
	'^', 0,							// XOR
	'|', 0,							// OR
	1								// (the end)
};

const char missym_error[] = "missing symbol";
const char str_error[] = "missing symbol or string";

// Convert expression to postfix
static TOKENPTR evalTokenBuffer;	// Deposit tokens here (this is really a
									// pointer to exprbuf from direct.c)
									// (Can also be from others, like
									// riscasm.c)
static int symbolNum;				// Pointer to the entry in symbolPtr[]


//
// Obtain a string value
//
static uint32_t str_value(char * p)
{
	uint32_t v;

	for(v=0; *p; p++)
		v = (v << 8) | (*p & 0xFF);

	return v;
}


//
// Initialize expression analyzer
//
void InitExpression(void)
{
	// Initialize token-class table (all set to END)
	for(int i=0; i<256; i++)
		tokenClass[i] = END;

	int i = 0;

	for(char * p=itokcl; *p!=1; p++)
	{
		if (*p == 0)
			i++;
		else
			tokenClass[(int)(*p)] = (char)i;
	}

	symbolNum = 0;
}


//
// Binary operators (all the same precedence)
//
int expr0(void)
{
	TOKEN t;

	if (expr1() != OK)
		return ERROR;

	while (tokenClass[*tok.u32] >= MULT)
	{
		t = *tok.u32++;

		if (expr1() != OK)
			return ERROR;

		*evalTokenBuffer.u32++ = t;
	}

	return OK;
}


//
// Unary operators (detect unary '-')
// ggn: If expression starts with a plus then also eat it up. For some reason
//      the parser gets confused when this happens and emits a "bad
//      expression".
//
int expr1(void)
{
	int class;
	TOKEN t;
	SYM * sy;
	char * p, * p2;
	WORD w;
	int j;

	class = tokenClass[*tok.u32];

	if (*tok.u32 == '-' || *tok.u32 == '+' || class == UNARY)
	{
		t = *tok.u32++;

		if (expr2() != OK)
			return ERROR;

		if (t == '-')
			t = UNMINUS;

		// With leading + we don't have to deposit anything to the buffer
		// because there's no unary '+' nor we have to do anything about it
		if (t != '+')
			*evalTokenBuffer.u32++ = t;
	}
	else if (class == SUNARY)
	{
		switch (*tok.u32++)
		{
		case CR_ABSCOUNT:
			*evalTokenBuffer.u32++ = CONST;
			*evalTokenBuffer.u64++ = (uint64_t)sect[ABS].sloc;
			break;
		case CR_TIME:
			*evalTokenBuffer.u32++ = CONST;
			*evalTokenBuffer.u64++ = dos_time();
			break;
		case CR_DATE:
			*evalTokenBuffer.u32++ = CONST;
			*evalTokenBuffer.u64++ = dos_date();
			break;
		case CR_MACDEF: // ^^macdef <macro-name>
			if (*tok.u32++ != SYMBOL)
				return error(missym_error);

			p = string[*tok.u32++];
			w = (lookup(p, MACRO, 0) == NULL ? 0 : 1);
			*evalTokenBuffer.u32++ = CONST;
			*evalTokenBuffer.u64++ = (uint64_t)w;
			break;
		case CR_DEFINED:
			w = DEFINED;
			goto getsym;
		case CR_REFERENCED:
			w = REFERENCED;
getsym:
			if (*tok.u32++ != SYMBOL)
				return error(missym_error);

			p = string[*tok.u32++];
			j = (*p == '.' ? curenv : 0);
			w = ((sy = lookup(p, LABEL, j)) != NULL && (sy->sattr & w) ? 1 : 0);
			*evalTokenBuffer.u32++ = CONST;
			*evalTokenBuffer.u64++ = (uint64_t)w;
			break;
		case CR_STREQ:
			if (*tok.u32 != SYMBOL && *tok.u32 != STRING)
				return error(str_error);

			p = string[tok.u32[1]];
			tok.u32 +=2;

			if (*tok.u32++ != ',')
				return error(comma_error);

			if (*tok.u32 != SYMBOL && *tok.u32 != STRING)
				return error(str_error);

			p2 = string[tok.u32[1]];
			tok.u32 += 2;

			w = (WORD)(!strcmp(p, p2));
			*evalTokenBuffer.u32++ = CONST;
			*evalTokenBuffer.u64++ = (uint64_t)w;
			break;
		}
	}
	else
		return expr2();

	return OK;
}


//
// Terminals (CONSTs) and parenthesis grouping
//
int expr2(void)
{
	char * p;
	SYM * sy;
	int j;

	switch (*tok.u32++)
	{
	case CONST:
		*evalTokenBuffer.u32++ = CONST;
		*evalTokenBuffer.u64++ = *tok.u64++;
		break;
	case FCONST:
		*evalTokenBuffer.u32++ = FCONST;
		*evalTokenBuffer.u64++ =  *tok.u64++;
		break;
	case SYMBOL:
		p = string[*tok.u32++];
		j = (*p == '.' ? curenv : 0);
		sy = lookup(p, LABEL, j);

		if (sy == NULL)
			sy = NewSymbol(p, LABEL, j);

		// Check register bank usage
		if (sy->sattre & EQUATEDREG)
		{
			if ((regbank == BANK_0) && (sy->sattre & BANK_1) && !altbankok)
				warn("equated symbol \'%s\' cannot be used in register bank 0", sy->sname);

			if ((regbank == BANK_1) && (sy->sattre & BANK_0) && !altbankok)
				warn("equated symbol \'%s\' cannot be used in register bank 1", sy->sname);
		}

		*evalTokenBuffer.u32++ = SYMBOL;
		*evalTokenBuffer.u32++ = symbolNum;
		symbolPtr[symbolNum] = sy;
		symbolNum++;
		break;
	case STRING:
		*evalTokenBuffer.u32++ = CONST;
		*evalTokenBuffer.u64++ = str_value(string[*tok.u32++]);
		break;
	case '(':
		if (expr0() != OK)
			return ERROR;

		if (*tok.u32++ != ')')
			return error("missing closing parenthesis ')'");

		break;
	case '[':
		if (expr0() != OK)
			return ERROR;

		if (*tok.u32++ != ']')
			return error("missing closing bracket ']'");

		break;
	case '$':
		*evalTokenBuffer.u32++ = ACONST;			// Attributed const
		*evalTokenBuffer.u32++ = sloc;				// Current location
		*evalTokenBuffer.u32++ = cursect | DEFINED;	// Store attribs
		break;
	case '*':
		*evalTokenBuffer.u32++ = ACONST;			// Attributed const

		// pcloc == location at start of line
		*evalTokenBuffer.u32++ = (orgactive ? orgaddr : pcloc);
		// '*' takes attributes of current section, not ABS!
		*evalTokenBuffer.u32++ = cursect | DEFINED;
		break;
	case '{':
		if (expr0() != OK)							// Eat up first parameter (register or immediate)
			return ERROR;

		if (*tok.u32++ != ':')						// Demand a ':' there
			return error("missing colon ':'");

		if (expr0() != OK)							// Eat up second parameter (register or immediate)
			return ERROR;

		if (*tok.u32++ != '}')
			return error("missing closing brace '}'");

		break;
	default:
		return error("bad expression");
	}

	return OK;
}


//
// Recursive-descent expression analyzer (with some simple speed hacks)
//
int expr(TOKENPTR otk, uint64_t * a_value, WORD * a_attr, SYM ** a_esym)
{
	// Passed in values (once derefenced, that is) can all be zero. They are
	// there so that the expression analyzer can fill them in as needed. The
	// expression analyzer gets its input from the global token pointer "tok",
	// and not from anything passed in by the user.
	SYM * symbol;
	char * p;
	int j;

	evalTokenBuffer = otk;	// Set token pointer to 'exprbuf' (direct.c)
							// Also set in various other places too (riscasm.c,
							// e.g.)

//printf("expr(): tokens 0-2: %i %i %i (%c %c %c); tc[2] = %i\n", tok.u32[0], tok.u32[1], tok.u32[2], tok.u32[0], tok.u32[1], tok.u32[2], tokenClass[tok.u32[2]]);
	// Optimize for single constant or single symbol.
	// Shamus: Subtle bug here. EOL token is 101; if you have a constant token
	//         followed by the value 101, it will trigger a bad evaluation here.
	//         This is probably a really bad assumption to be making here...!
	//         (assuming tok.u32[1] == EOL is a single token that is)
	//         Seems that even other tokens (SUNARY type) can fuck this up too.
#if 0
//	if ((tok.u32[1] == EOL)
	if ((tok.u32[1] == EOL && ((tok.u32[0] != CONST || tok.u32[0] != FCONST) && tokenClass[tok.u32[0]] != SUNARY))
//		|| (((*tok.u32 == CONST || *tok.u32 == FCONST || *tok.u32 == SYMBOL) || (*tok.u32 >= KW_R0 && *tok.u32 <= KW_R31))
//		&& (tokenClass[tok.u32[2]] < UNARY)))
		|| (((tok.u32[0] == SYMBOL) || (tok.u32[0] >= KW_R0 && tok.u32[0] <= KW_R31))
			&& (tokenClass[tok.u32[2]] < UNARY))
		|| ((tok.u32[0] == CONST || tok.u32[0] == FCONST) && (tokenClass[tok.u32[3]] < UNARY))
		)
#else
// Shamus: Seems to me that this could be greatly simplified by 1st checking if the first token is a multibyte token, *then* checking if there's an EOL after it depending on the actual length of the token (multiple vs. single). Otherwise, we have the horror show that is the following:
	if ((tok.u32[1] == EOL
			&& (tok.u32[0] != CONST && tokenClass[tok.u32[0]] != SUNARY))
		|| (((tok.u32[0] == SYMBOL)
				|| (tok.u32[0] >= KW_R0 && tok.u32[0] <= KW_R31))
			&& (tokenClass[tok.u32[2]] < UNARY))
		|| ((tok.u32[0] == CONST) && (tokenClass[tok.u32[3]] < UNARY))
		)
// Shamus: Yes, you can parse that out and make some kind of sense of it, but damn, it takes a while to get it and understand the subtle bugs that result from not being careful about what you're checking; especially vis-a-vis niavely checking tok.u32[1] for an EOL. O_o
#endif
	{
		if (*tok.u32 >= KW_R0 && *tok.u32 <= KW_R31)
		{
			*evalTokenBuffer.u32++ = CONST;
			*evalTokenBuffer.u64++ = *a_value = (*tok.u32 - KW_R0);
			*a_attr = ABS | DEFINED;

			if (a_esym != NULL)
				*a_esym = NULL;

			tok.u32++;
		}
		else if (*tok.u32 == CONST)
		{
			*evalTokenBuffer.u32++ = *tok.u32++;
			*evalTokenBuffer.u64++ = *a_value = *tok.u64++;
			*a_attr = ABS | DEFINED;

			if (a_esym != NULL)
				*a_esym = NULL;

//printf("Quick eval in expr(): CONST = %i, tokenClass[tok.u32[2]] = %i\n", *a_value, tokenClass[*tok.u32]);
		}
// Not sure that removing float constant here is going to break anything and/or
// make things significantly slower, but having this here seems to cause the
// complexity of the check to get to this part of the parse to go through the
// roof, and dammit, I just don't feel like fighting that fight ATM. :-P
#if 0
		else if (*tok.u32 == FCONST)
		{
			*evalTokenBuffer.u32++ = *tok.u32++;
			*evalTokenBuffer.u64++ = *a_value = *tok.u64++;
			*a_attr = ABS | DEFINED | FLOAT;

			if (a_esym != NULL)
				*a_esym = NULL;

//printf("Quick eval in expr(): CONST = %i, tokenClass[tok.u32[2]] = %i\n", *a_value, tokenClass[*tok.u32]);
		}
#endif
		else if (*tok.u32 == '*')
		{
			*evalTokenBuffer.u32++ = CONST;

			if (orgactive)
				*evalTokenBuffer.u64++ = *a_value = orgaddr;
			else
				*evalTokenBuffer.u64++ = *a_value = pcloc;

			// '*' takes attributes of current section, not ABS!
			*a_attr = cursect | DEFINED;

			if (a_esym != NULL)
				*a_esym = NULL;

			tok.u32++;
		}
		else if (*tok.u32 == STRING || *tok.u32 == SYMBOL)
		{
			p = string[tok.u32[1]];
			j = (*p == '.' ? curenv : 0);
			symbol = lookup(p, LABEL, j);
#if 0
printf("eval: Looking up symbol (%s) [=%08X]\n", p, symbol);
if (symbol)
	printf("      attr=%04X, attre=%08X, val=%i, name=%s\n", symbol->sattr, symbol->sattre, symbol->svalue, symbol->sname);
#endif

			if (symbol == NULL)
				symbol = NewSymbol(p, LABEL, j);

			symbol->sattr |= REFERENCED;

			// Check for undefined register equates, but only if it's not part
			// of a #<SYMBOL> construct, as it could be that the label that's
			// been undefined may later be used as an address label--which
			// means it will be fixed up later, and thus, not an error.
			if ((symbol->sattre & UNDEF_EQUR) && !riscImmTokenSeen)
			{
				error("undefined register equate '%s'", symbol->sname);
//if we return right away, it returns some spurious errors...
//				return ERROR;
			}

			// Check register bank usage
			if (symbol->sattre & EQUATEDREG)
			{
				if ((regbank == BANK_0) && (symbol->sattre & BANK_1) && !altbankok)
					warn("equated symbol '%s' cannot be used in register bank 0", symbol->sname);

				if ((regbank == BANK_1) && (symbol->sattre & BANK_0) && !altbankok)
					warn("equated symbol '%s' cannot be used in register bank 1", symbol->sname);
			}

			*evalTokenBuffer.u32++ = SYMBOL;
#if 0
			*evalTokenBuffer++ = (TOKEN)symbol;
#else
/*
While this approach works, it's wasteful. It would be better to use something
that's already available, like the symbol "order defined" table (which needs to
be converted from a linked list into an array).
*/
			*evalTokenBuffer.u32++ = symbolNum;
			symbolPtr[symbolNum] = symbol;
			symbolNum++;
#endif

			if (symbol->sattr & DEFINED)
				*a_value = symbol->svalue;
			else
				*a_value = 0;

/*
All that extra crap that was put into the svalue when doing the equr stuff is
thrown away right here. What the hell is it for?
*/
			if (symbol->sattre & EQUATEDREG)
				*a_value &= 0x1F;

			*a_attr = (WORD)(symbol->sattr & ~GLOBAL);

			if ((symbol->sattr & (GLOBAL | DEFINED)) == GLOBAL
				&& a_esym != NULL)
				*a_esym = symbol;

			tok.u32 += 2;
		}
		else
		{
			// Unknown type here... Alert the user!,
			error("undefined RISC register in expression");
			// Prevent spurious error reporting...
			tok.u32++;
			return ERROR;
		}

		*evalTokenBuffer.u32++ = ENDEXPR;
		return OK;
	}

	if (expr0() != OK)
		return ERROR;

	*evalTokenBuffer.u32++ = ENDEXPR;
	return evexpr(otk, a_value, a_attr, a_esym);
}


//
// Evaluate expression.
// If the expression involves only ONE external symbol, the expression is
// UNDEFINED, but it's value includes everything but the symbol value, and
// 'a_esym' is set to the external symbol.
//
int evexpr(TOKENPTR tk, uint64_t * a_value, WORD * a_attr, SYM ** a_esym)
{
	WORD attr, attr2;
	SYM * sy;
	uint64_t * sval = evstk;				// (Empty) initial stack
	WORD * sattr = evattr;
	SYM * esym = NULL;						// No external symbol involved
	WORD sym_seg = 0;

	while (*tk.u32 != ENDEXPR)
	{
		switch ((int)*tk.u32++)
		{
		case SYMBOL:
//printf("evexpr(): SYMBOL\n");
			sy = symbolPtr[*tk.u32++];
			sy->sattr |= REFERENCED;		// Set "referenced" bit

			if (!(sy->sattr & DEFINED))
			{
				// Reference to undefined symbol
				if (!(sy->sattr & GLOBAL))
				{
					*a_attr = 0;
					*a_value = 0;
					return OK;
				}

				if (esym != NULL)			// Check for multiple externals
					return error(seg_error);

				esym = sy;
			}

			if (sy->sattr & DEFINED)
			{
				*++sval = sy->svalue;		// Push symbol's value
			}
			else
			{
				*++sval = 0;				// 0 for undefined symbols
			}

			*++sattr = (WORD)(sy->sattr & ~GLOBAL);	// Push attribs
			sym_seg = (WORD)(sy->sattr & TDB);
			break;
		case CONST:
			*++sval = *tk.u64++;
//printf("evexpr(): CONST = %lX\n", *sval);
			*++sattr = ABS | DEFINED;		// Push simple attribs
			break;
		case FCONST:
//printf("evexpr(): FCONST = %i\n", *tk.u32);
			*((double *)sval) = *((double *)tk.u32);
			tk.u32 += 2;
			*++sattr = ABS | DEFINED | FLOAT; // Push simple attribs
			break;
		case ACONST:
//printf("evexpr(): ACONST = %i\n", *tk.u32);
			*++sval = *tk.u32++;				// Push value
			*++sattr = (WORD)*tk.u32++;			// Push attribs
			break;

			// Binary "+" and "-" matrix:
			//
			// 	          ABS	 Sect	  Other
			//     ----------------------------
			//   ABS     |	ABS   |  Sect  |  Other |
			//   Sect    |	Sect  |  [1]   |  Error |
			//   Other   |	Other |  Error |  [1]   |
			//      ----------------------------
			//
			//   [1] + : Error
			//       - : ABS
		case '+':
//printf("evexpr(): +\n");
			--sval;							// Pop value
			--sattr;						// Pop attrib
//printf("--> N+N: %i + %i = ", *sval, sval[1]);
			// Extract float attributes from both terms and pack them
			// into a single value
			attr = sattr[0] & FLOAT | ((sattr[1] & FLOAT) >> 1);
			attr2 = sattr[0] | sattr[1] & FLOAT; // Returns FLOAT if either of the two numbers are FLOAT

			if (attr == (FLOAT | (FLOAT >> 1)))
			{
				// Float + Float
				double * dst = (double *)sval;
				double * src = (double *)(sval + 1);
				*dst += *src;
			}
			else if (attr == FLOAT)
			{
				// Float + Int
				double * dst = (double *)sval;
				uint64_t * src = (uint64_t *)(sval + 1);
				*dst += *src;
			}
			else if (attr == FLOAT >> 1)
			{
				// Int + Float
				uint64_t * dst = (uint64_t *)sval;
				double * src = (double *)(sval + 1);
				*(double *)dst = *src + *dst;
			}
			else
			{
				*sval += sval[1];				// Compute value
			}
//printf("%i\n", *sval);

			if (!(*sattr & TDB))
				*sattr = sattr[1] | attr2;
			else if (sattr[1] & TDB)
				return error(seg_error);

			break;
		case '-':
//printf("evexpr(): -\n");
			--sval;							// Pop value
			--sattr;						// Pop attrib
//printf("--> N-N: %i - %i = ", *sval, sval[1]);
			// Extract float attributes from both terms and pack them
			// into a single value
			attr = sattr[0] & FLOAT | ((sattr[1] & FLOAT) >> 1);
			attr2 = sattr[0] | sattr[1] & FLOAT; // Returns FLOAT if either of the two numbers are FLOAT

			if (attr == (FLOAT | (FLOAT >> 1)))
			{
				// Float - Float
				double * dst = (double *)sval;
				double * src = (double *)(sval + 1);
				*dst -= *src;
			}
			else if (attr == FLOAT)
			{
				// Float - Int
				double * dst = (double *)sval;
				uint64_t * src = (uint64_t *)(sval + 1);
				*dst -= *src;
			}
			else if (attr == FLOAT >> 1)
			{
				// Int - Float
				uint64_t * dst = (uint64_t *)sval;
				double * src = (double *)(sval + 1);
				*(double *)dst = *dst - *src;
			}
			else
			{
				*sval -= sval[1];				// Compute value
			}

//printf("%i\n", *sval);

			attr = (WORD)(*sattr & TDB);
#if 0
printf("EVEXPR (-): sym1 = %X, sym2 = %X\n", attr, sattr[1]);
#endif
			*sattr |= attr2;                // Inherit FLOAT attribute
			// If symbol1 is ABS, take attributes from symbol2
			if (!attr)
				*sattr = sattr[1];
			// Otherwise, they're both TDB and so attributes cancel out
			else if (sattr[1] & TDB)
				*sattr &= ~TDB;

			break;
		// Unary operators only work on ABS items
		case UNMINUS:
//printf("evexpr(): UNMINUS\n");
			if (*sattr & TDB)
				return error(seg_error);

			if (*sattr & FLOAT)
			{
				double *dst = (double *)sval;
				*dst = -*dst;
				*sattr = ABS | DEFINED | FLOAT; // Expr becomes absolute
			}
			else
			{
				*sval = -(int)*sval;
				*sattr = ABS | DEFINED;			// Expr becomes absolute
			}
			break;
		case '!':
//printf("evexpr(): !\n");
			if (*sattr & TDB)
				return error(seg_error);

			if (*sattr & FLOAT)
				return error("floating point numbers not allowed with operator '!'.");

			*sval = !*sval;
			*sattr = ABS | DEFINED;			// Expr becomes absolute
			break;
		case '~':
//printf("evexpr(): ~\n");
			if (*sattr & TDB)
				return error(seg_error);

			if (*sattr & FLOAT)
				return error("floating point numbers not allowed with operator '~'.");

			*sval = ~*sval;
			*sattr = ABS | DEFINED;			// Expr becomes absolute
			break;
		// Comparison operators must have two values that
		// are in the same segment, but that's the only requirement.
		case LE:
//printf("evexpr(): LE\n");
			sattr--;
			sval--;

			if ((*sattr & TDB) != (sattr[1] & TDB))
				return error(seg_error);

			// Extract float attributes from both terms and pack them
			// into a single value
			attr = sattr[0] & FLOAT | ((sattr[1] & FLOAT) >> 1);

			if (attr == (FLOAT | (FLOAT >> 1)))
			{
				// Float <= Float
				double * dst = (double *)sval;
				double * src = (double *)(sval + 1);
				*sval = *dst <= *src;
			}
			else if (attr == FLOAT)
			{
				// Float <= Int
				double * dst = (double *)sval;
				uint64_t * src = (uint64_t *)(sval + 1);
				*sval = *dst <= *src;
			}
			else if (attr == FLOAT >> 1)
			{
				// Int <= Float
				uint64_t * dst = (uint64_t *)sval;
				double * src = (double *)(sval + 1);
				*sval = *dst <= *src;
			}
			else
			{
				*sval = *sval <= sval[1];
			}

			*sattr = ABS | DEFINED;
			break;
		case GE:
//printf("evexpr(): GE\n");
			sattr--;
			sval--;

			if ((*sattr & TDB) != (sattr[1] & TDB))
				return error(seg_error);

			// Extract float attributes from both terms and pack them
			// into a single value
			attr = sattr[0] & FLOAT | ((sattr[1] & FLOAT) >> 1);

			if (attr == (FLOAT | (FLOAT >> 1)))
			{
				// Float >= Float
				double * dst = (double *)sval;
				double * src = (double *)(sval + 1);
				*sval = *dst >= *src;
			}
			else if (attr == FLOAT)
			{
				// Float >= Int
				double * dst = (double *)sval;
				uint64_t * src = (uint64_t *)(sval + 1);
				*sval = *dst >= *src;
			}
			else if (attr == FLOAT >> 1)
			{
				// Int >= Float
				uint64_t * dst = (uint64_t *)sval;
				double * src = (double *)(sval + 1);
				*sval = *dst >= *src;
			}
			else if (attr == 0)
			{
				*sval = *sval >= sval[1];
			}
			else
				*sattr = ABS | DEFINED;

			break;
		case '>':
//printf("evexpr(): >\n");
			sattr--;
			sval--;

			if ((*sattr & TDB) != (sattr[1] & TDB))
				return error(seg_error);

			// Extract float attributes from both terms and pack them
			// into a single value
			attr = sattr[0] & FLOAT | ((sattr[1] & FLOAT) >> 1);

			if (attr == (FLOAT | (FLOAT >> 1)))
			{
				// Float > Float
				double * dst = (double *)sval;
				double * src = (double *)(sval + 1);
				*sval = *dst > *src;
			}
			else if (attr == FLOAT)
			{
				// Float > Int
				double * dst = (double *)sval;
				uint64_t * src = (uint64_t *)(sval + 1);
				*sval = *dst > *src;
			}
			else if (attr == FLOAT >> 1)
			{
				// Int > Float
				uint64_t * dst = (uint64_t *)sval;
				double * src = (double *)(sval + 1);
				*sval = *dst > *src;
			}
			else
			{
				*sval = *sval > sval[1];
			}

			*sattr = ABS | DEFINED;

			break;
		case '<':
//printf("evexpr(): <\n");
			sattr--;
			sval--;

			if ((*sattr & TDB) != (sattr[1] & TDB))
				return error(seg_error);

			// Extract float attributes from both terms and pack them
			// into a single value
			attr = sattr[0] & FLOAT | ((sattr[1] & FLOAT) >> 1);

			if (attr == (FLOAT | (FLOAT >> 1)))
			{
				// Float < Float
				double * dst = (double *)sval;
				double * src = (double *)(sval + 1);
				*sval = *dst < *src;
			}
			else if (attr == FLOAT)
			{
				// Float < Int
				double * dst = (double *)sval;
				uint64_t * src = (uint64_t *)(sval + 1);
				*sval = *dst < *src;
			}
			else if (attr == FLOAT >> 1)
			{
				// Int < Float
				uint64_t * dst = (uint64_t *)sval;
				double * src = (double *)(sval + 1);
				*sval = *dst < *src;
			}
			else
			{
				*sval = *sval < sval[1];
			}

			*sattr = ABS | DEFINED;

			break;
		case NE:
//printf("evexpr(): NE\n");
			sattr--;
			sval--;

			if ((*sattr & TDB) != (sattr[1] & TDB))
				return error(seg_error);

			// Extract float attributes from both terms and pack them
			// into a single value
			attr = sattr[0] & FLOAT | ((sattr[1] & FLOAT) >> 1);

			if (attr == (FLOAT | (FLOAT >> 1)))
			{
				// Float <> Float
				return error("comparison for equality with float types not allowed.");
			}
			else if (attr == FLOAT)
			{
				// Float <> Int
				return error("comparison for equality with float types not allowed.");
			}
			else if (attr == FLOAT >> 1)
			{
				// Int != Float
				return error("comparison for equality with float types not allowed.");
			}
			else
			{
				*sval = *sval != sval[1];
			}

			*sattr = ABS | DEFINED;

			break;
		case '=':
//printf("evexpr(): =\n");
			sattr--;
			sval--;

			if ((*sattr & TDB) != (sattr[1] & TDB))
				return error(seg_error);

			// Extract float attributes from both terms and pack them
			// into a single value
			attr = sattr[0] & FLOAT | ((sattr[1] & FLOAT) >> 1);

			if (attr == (FLOAT | (FLOAT >> 1)))
			{
				// Float = Float
				double * dst = (double *)sval;
				double * src = (double *)(sval + 1);
				*sval = *src == *dst;
			}
			else if (attr == FLOAT)
			{
				// Float = Int
				return error("equality with float ");
			}
			else if (attr == FLOAT >> 1)
			{
				// Int == Float
				uint64_t * dst = (uint64_t *)sval;
				double * src = (double *)(sval + 1);
				*sval = *src == *dst;
			}
			else
			{
				*sval = *sval == sval[1];
			}

			*sattr = ABS | DEFINED;

			break;
		// All other binary operators must have two ABS items to work with.
		// They all produce an ABS value.
		default:
//printf("evexpr(): default\n");
			// GH - Removed for v1.0.15 as part of the fix for indexed loads.
			//if ((*sattr & (TEXT|DATA|BSS)) || (*--sattr & (TEXT|DATA|BSS)))
			//error(seg_error);

			switch ((int)tk.u32[-1])
			{
			case '*':
				sval--;
				sattr--;					// Pop attrib
//printf("--> NxN: %i x %i = ", *sval, sval[1]);
				// Extract float attributes from both terms and pack them
				// into a single value
				attr = sattr[0] & FLOAT | ((sattr[1] & FLOAT) >> 1);
				attr2 = sattr[0] | sattr[1] & FLOAT; // Returns FLOAT if either of the two numbers are FLOAT

				if (attr == (FLOAT | (FLOAT >> 1)))
				{
					// Float * Float
					double * dst = (double *)sval;
					double * src = (double *)(sval + 1);
					*dst *= *src;
				}
				else if (attr == FLOAT)
				{
					// Float * Int
					double * dst = (double *)sval;
					uint64_t * src = (uint64_t *)(sval + 1);
					*dst *= *src;
				}
				else if (attr == FLOAT >> 1)
				{
					// Int * Float
					uint64_t * dst = (uint64_t *)sval;
					double * src = (double *)(sval + 1);
					*(double *)dst = *src * *dst;
				}
				else
				{
					*sval *= sval[1];
				}
//printf("%i\n", *sval);

				*sattr = ABS | DEFINED;			// Expr becomes absolute
				*sattr |= attr2;

				break;
			case '/':
				sval--;
				sattr--;					// Pop attrib


//printf("--> N/N: %i / %i = ", sval[0], sval[1]);
				// Extract float attributes from both terms and pack them
				// into a single value
				attr = sattr[0] & FLOAT | ((sattr[1] & FLOAT) >> 1);
				attr2 = sattr[0] | sattr[1] & FLOAT; // Returns FLOAT if either of the two numbers are FLOAT

				if (attr == (FLOAT | (FLOAT >> 1)))
				{
					// Float / Float
					double * dst = (double *)sval;
					double * src = (double *)(sval + 1);

					if (*src == 0)
						return error("divide by zero");

					*dst = *dst / *src;
				}
				else if (attr == FLOAT)
				{
					// Float / Int
					double * dst = (double *)sval;
					uint64_t * src = (uint64_t *)(sval + 1);

					if (*src == 0)
						return error("divide by zero");

					*dst = *dst / *src;
				}
				else if (attr == FLOAT >> 1)
				{
					// Int / Float
					uint64_t * dst=(uint64_t *)sval;
					double * src=(double *)(sval + 1);

					if (*src == 0)
						return error("divide by zero");

					*(double *)dst = *dst / *src;
				}
				else
				{
					if (sval[1] == 0)
						return error("divide by zero");
//printf("--> N/N: %i / %i = ", sval[0], sval[1]);

					// Compiler is picky here: Without casting these, it
					// discards the sign if dividing a negative # by a
					// positive one, creating a bad result. :-/
					// Definitely a side effect of using uint32_ts intead of
					// ints.
					*sval = (int32_t)sval[0] / (int32_t)sval[1];
				}

				*sattr = ABS | DEFINED;			// Expr becomes absolute
				*sattr |= attr2;

//printf("%i\n", *sval);
				break;
			case '%':
				sval--;
				sattr--;					// Pop attrib

				if ((*sattr | sattr[1]) & FLOAT)
					return error("floating point numbers not allowed with operator '%'.");

				if (sval[1] == 0)
					return error("mod (%) by zero");

				*sattr = ABS | DEFINED;			// Expr becomes absolute
				*sval %= sval[1];
				break;
			case SHL:
				sval--;
				sattr--;					// Pop attrib

				if ((*sattr | sattr[1]) & FLOAT)
					return error("floating point numbers not allowed with operator '<<'.");

				*sattr = ABS | DEFINED;			// Expr becomes absolute
				*sval <<= sval[1];
				break;
			case SHR:
				sval--;
				sattr--;					// Pop attrib

				if ((*sattr | sattr[1]) & FLOAT)
					return error("floating point numbers not allowed with operator '>>'.");

				*sattr = ABS | DEFINED;			// Expr becomes absolute
				*sval >>= sval[1];
				break;
			case '&':
				sval--;
				sattr--;					// Pop attrib

				if ((*sattr | sattr[1]) & FLOAT)
					return error("floating point numbers not allowed with operator '&'.");

				*sattr = ABS | DEFINED;			// Expr becomes absolute
				*sval &= sval[1];
				break;
			case '^':
				sval--;
				sattr--;					// Pop attrib

				if ((*sattr | sattr[1]) & FLOAT)
					return error("floating point numbers not allowed with operator '^'.");

				*sattr = ABS | DEFINED;			// Expr becomes absolute
				*sval ^= sval[1];
				break;
			case '|':
				sval--;
				sattr--;					// Pop attrib

				if ((*sattr | sattr[1]) & FLOAT)
					return error("floating point numbers not allowed with operator '|'.");

				*sattr = ABS | DEFINED;			// Expr becomes absolute
				*sval |= sval[1];
				break;
			default:
				interror(5);				// Bad operator in expression stream
			}
		}
	}

	if (esym != NULL)
		*sattr &= ~DEFINED;

	if (a_esym != NULL)
		*a_esym = esym;

	// sym_seg added in 1.0.16 to solve a problem with forward symbols in
	// expressions where absolute values also existed. The absolutes were
	// overiding the symbol segments and not being included :(
	//*a_attr = *sattr | sym_seg;           // Copy value + attrib

	*a_attr = *sattr;						// Copy value + attrib
	*a_value = *sval;

	return OK;
}

