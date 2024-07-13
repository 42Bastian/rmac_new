//
// RMAC -  Macro Assembler for all Atari computers
// DEBUG.C - Debugging Messages
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "debug.h"
#include "6502.h"
#include "amode.h"
#include "direct.h"
#include "expr.h"
#include "mark.h"
#include "sect.h"
#include "token.h"


static int siztab[4] = { 3, 5, 9, 9 };
static PTR tp;


//
// Print 'c' visibly
//
int visprt(char c)
{
	if (c < 0x20 || c >= 0x7F)
		putchar('.');
	else
		putchar(c);

	return 0;
}


//
// Print expression, return ptr to just past the ENDEXPR
//
TOKEN * printexpr(TOKEN * tokenptr)
{
	if (tokenptr != NULL)
	{
		while (*tokenptr != ENDEXPR)
		{
			switch ((int)*tokenptr++)
			{
			case SYMBOL:
				printf("'%s' ", symbolPtr[*tokenptr]->sname);
				tokenptr++;
				break;
			case CONST:
				tp.u32 = tokenptr;
				printf("$%llX ", *tp.u64++);
				tokenptr = tp.u32;
				break;
			case ACONST:
				printf("ACONST=($%X,$%X) ", *tokenptr, tokenptr[1]);
				tokenptr += 2;
				break;
			default:
				printf("%c ", (char)tokenptr[-1]);
				break;
			}
		}
	}

	return tokenptr + 1;
}


//
// Dump data in a chunk (and maybe others) in the appropriate format
//
int chdump(CHUNK * ch, int format)
{
	while (ch != NULL)
	{
		printf("chloc=$%08X, chsize=$%X\n", ch->chloc, ch->ch_size);
		mdump(ch->chptr, ch->ch_size, format, ch->chloc);
		ch = ch->chnext;
	}

	return 0;
}


//
// Dump fixup records in printable format
//
int fudump(FIXUP * fup)
{
	for(; fup!=NULL;)
	{
		uint32_t attr = fup->attr;
		uint32_t loc = fup->loc;
		uint16_t file = fup->fileno;
		uint16_t line = fup->lineno;

		printf("$%08X $%08X %d.%d: ", attr, loc, (int)file, (int)line);

		if (attr & FU_EXPR)
		{
			uint16_t esiz = ExpressionLength(fup->expr);
			printf("(%d long) ", (int)esiz);
			printexpr(fup->expr);
		}
		else
			printf("`%s' ;", fup->symbol->sname);

		if ((attr & FUMASKRISC) == FU_JR)
			printf(" *=$%X", fup->orgaddr);

		printf("\n");

		fup = fup->next;
	}

	return 0;
}


//
// Dump marks
//
int mudump(void)
{
	MCHUNK * mch;
	PTR p;
	WORD from;
	WORD w;
	LONG loc;
	SYM * symbol;

	from = 0;

	for(mch=firstmch; mch!=NULL; mch=mch->mcnext)
	{
		printf("mch=$%p mcptr=$%08X mcalloc=$%X mcused=$%X\n",
			mch, (mch->mcptr.lw), mch->mcalloc, (mch->mcused));

		p = mch->mcptr;

		for(;;)
		{
			w = *p.wp++;

			if (w & MCHEND)
				break;

			symbol = NULL;
			loc = *p.lp++;

			if (w & MCHFROM)
				from = *p.wp++;

			if (w & MSYMBOL)
				symbol = *p.sy++;

			printf("m=$%04X to=%d loc=$%X from=%d siz=%s",
					w, w & 0x00FF, loc, from, (w & MLONG) ? "long" : "word");

			if (symbol != NULL)
				printf(" sym=`%s'", symbol->sname);

			printf("\n");
		}
	}

	return 0;
}


//
// Dump memory from 'start' for 'count' bytes; `flg' is the following ORed
// together:
// 0 - bytes
// 1 - words
// 2 - longwords
//
// if `base' is not -1, then print it at the start of each line, incremented
// accordingly.
//
int mdump(char * start, LONG count, int flg, LONG base)
{
	int i, j, k;
	j = 0;

	for(i=0; i<(int)count;)
	{
		if ((i & 15) == 0)
		{
			if (j < i)
			{
				printf("  ");

				while (j < i)
				visprt(start[j++]);

				putchar('\n');
			}

			j = i;

			if (base != -1)
				printf("%08X  ", base);
		}

		switch (flg & 3)
		{
		case 0:
			printf("%02X ", start[i] & 0xFF);
			i++;
			break;
		case 1:
			printf("%02X%02X ", start[i] & 0xFF, start[i+1] & 0xFF);
			i += 2;
			break;
		case 2:
			printf("%02X%02X%02X%02X ", start[i] & 0xFF, start[i+1] & 0xFF,
				start[i+2] & 0xFF, start[i+3] & 0xFF);
			i += 4;
			break;
		}

		if (base != -1)
			base += 1 << (flg & 3);
	}

	// Print remaining bit of ASCII; the hairy expression computes the number
	// of spaces to print to make the ASCII line up nicely.
	if (j != i)
	{
		k = ((16 - (i - j)) / (1 << (flg & 3))) * siztab[flg & 3];

		while (k--)
			putchar(' ');

		printf("  ");

		while (j < i)
			visprt(start[j++]);

		putchar('\n');
	}

	return 0;
}


//
// Dump list of tokens on stdout in printable form
//
int dumptok(TOKEN * tk)
{
	int flg = 0;

	while (*tk != EOL)
	{
		if (flg++)
			printf(" ");

		if (*tk >= 128)
		{
			printf("REG=%u", *tk++ - 128);
			continue;
		}

		switch ((int)*tk++)
		{
		case CONST:                                        // CONST <value>
			tp.u32 = tk;
			printf("CONST=%llu", *tp.u64++);
			tk = tp.u32;
			break;
		case STRING:                                       // STRING <address>
			printf("STRING='%s'", string[*tk++]);
			break;
		case SYMBOL:                                       // SYMBOL <address>
			printf("SYMBOL='%s'", string[*tk++]);
			break;
		case EOL:                                          // End of line
			printf("EOL");
			break;
		case TKEOF:                                        // End of file (or macro)
			printf("TKEOF");
			break;
		case DEQUALS:                                      // ==
			printf("DEQUALS");
			break;
		case DCOLON:                                       // ::
			printf("DCOLON");
			break;
		case GE:                                           // >=
			printf("GE");
			break;
		case LE:                                           // <=
			printf("LE");
			break;
		case NE:                                           // <> or !=
			printf("NE");
			break;
		case SHR:                                          // >>
			printf("SHR");
			break;
		case SHL:                                          // <<
			printf("SHL");
			break;
		default:
			printf("%c", (int)tk[-1]);
			break;
		}
	}

	printf("\n");

	return 0;
}


void DumpTokens(TOKEN * tokenBuffer)
{
//	printf("Tokens [%X]: ", sloc);

	for(TOKEN * t=tokenBuffer; *t!=EOL; t++)
	{
		if (*t == COLON)
			printf("[COLON]");
		else if (*t == CONST)
		{
			tp.u32 = t + 1;
			printf("[CONST: $%llX]", *tp.u64);
			t += 2;
		}
		else if (*t == ACONST)
		{
			printf("[ACONST: $%X, $%X]", (uint32_t)t[1], (uint32_t)t[2]);
			t += 2;
		}
		else if (*t == STRING)
		{
			t++;
			printf("[STRING:\"%s\"]", string[*t]);
		}
		else if (*t == SYMBOL)
		{
			t++;
			printf("[SYMBOL:\"%s\"]", string[*t]);
		}
		else if (*t == EOS)
			printf("[EOS]");
		else if (*t == TKEOF)
			printf("[TKEOF]");
		else if (*t == DEQUALS)
			printf("[DEQUALS]");
		else if (*t == SET)
			printf("[SET]");
		else if (*t == REG)
			printf("[REG]");
		else if (*t == DCOLON)
			printf("[DCOLON]");
		else if (*t == GE)
			printf("[GE]");
		else if (*t == LE)
			printf("[LE]");
		else if (*t == NE)
			printf("[NE]");
		else if (*t == SHR)
			printf("[SHR]");
		else if (*t == SHL)
			printf("[SHL]");
		else if (*t == UNMINUS)
			printf("[UNMINUS]");
		else if (*t == DOTB)
			printf("[DOTB]");
		else if (*t == DOTW)
			printf("[DOTW]");
		else if (*t == DOTL)
			printf("[DOTL]");
		else if (*t == DOTI)
			printf("[DOTI]");
		else if (*t == ENDEXPR)
			printf("[ENDEXPR]");
		else if (*t == CR_ABSCOUNT)
			printf("[CR_ABSCOUNT]");
		else if (*t == CR_FILESIZE)
			printf("[CR_FILESIZE]");
		else if (*t == CR_DEFINED)
			printf("[CR_DEFINED]");
		else if (*t == CR_REFERENCED)
			printf("[CR_REFERENCED]");
		else if (*t == CR_STREQ)
			printf("[CR_STREQ]");
		else if (*t == CR_MACDEF)
			printf("[CR_MACDEF]");
		else if (*t == CR_TIME)
			printf("[CR_TIME]");
		else if (*t == CR_DATE)
			printf("[CR_DATE]");
		else if (*t >= 0x20 && *t <= 0x2F)
			printf("[%c]", (char)*t);
		else if (*t >= 0x3A && *t <= 0x3F)
			printf("[%c]", (char)*t);
		else if (*t >= 0x80 && *t <= 0x87)
			printf("[D%u]", ((uint32_t)*t) - 0x80);
		else if (*t >= 0x88 && *t <= 0x8F)
			printf("[A%u]", ((uint32_t)*t) - 0x88);
		else
			printf("[%X:%c]", (uint32_t)*t, (char)*t);
	}

	printf("[EOL]\n");
}


//
// Dump everything
//
int dump_everything(void)
{
	// FFS
	if ((currentorg[1] - currentorg[0]) == 0)
		sect[M6502].sfcode = NULL;

	for(int i=1; i<NSECTS; i++)
	{
		if (sect[i].scattr & SUSED)
		{
			printf("Section %d sloc=$%X\n", i, sect[i].sloc);
			printf("Code:\n");
			chdump(sect[i].sfcode, 1);

			printf("Fixup:\n");
			fudump(sect[i].sffix);

			printf("\n");
		}
	}

	printf("\nMarks:\n");
	mudump();								// Dump marks

	return 0;
}

