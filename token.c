//
// RMAC - Renamed Macro Assembler for all Atari computers
// TOKEN.C - Token Handling
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "token.h"

#include <errno.h>
#include "direct.h"
#include "error.h"
#include "macro.h"
#include "procln.h"
#include "sect.h"
#include "symbol.h"

#define DECL_KW				// Declare keyword arrays
#define DEF_KW				// Declare keyword values
#include "kwtab.h"			// Incl generated keyword tables & defs
#define DEF_REG68			// Incl 68k register definitions
#include "68kregs.h"
#define DEF_REGRISC			// Include GPU/DSP register definitions
#include "riscregs.h"
#define DEF_UNARY			// Declare unary values
#define DECL_UNARY			// Incl uanry keyword state machine tables
#include "unarytab.h"		// Incl generated unary tables & defs


int lnsave;					// 1; strcpy() text of current line
uint32_t curlineno;			// Current line number (64K max currently)
int totlines;				// Total # of lines
int mjump_align = 0;		// mjump alignment flag
char lntag;					// Line tag
char * curfname;			// Current filename
char tolowertab[128];		// Uppercase ==> lowercase
int8_t hextab[128];			// Table of hex values
char dotxtab[128];			// Table for ".b", ".s", etc.
char irbuf[LNSIZ];			// Text for .rept block line
char lnbuf[LNSIZ];			// Text of current line
WORD filecount;				// Unique file number counter
WORD cfileno;				// Current file number
TOKEN * tok;				// Ptr to current token
TOKEN * etok;				// Ptr past last token in tokbuf[]
TOKEN tokeol[1] = {EOL};	// Bailout end-of-line token
char * string[TOKBUFSIZE*2];// Token buffer string pointer storage
int optimizeOff;			// Optimization override flag


FILEREC * filerec;
FILEREC * last_fr;

INOBJ * cur_inobj;			// Ptr current input obj (IFILE/IMACRO)
static INOBJ * f_inobj;		// Ptr list of free INOBJs
static IFILE * f_ifile;		// Ptr list of free IFILEs
static IMACRO * f_imacro;	// Ptr list of free IMACROs

static TOKEN tokbuf[TOKBUFSIZE];	// Token buffer (stack-like, all files)

uint8_t chrtab[0x100] = {
	ILLEG, ILLEG, ILLEG, ILLEG,			// NUL SOH STX ETX
	ILLEG, ILLEG, ILLEG, ILLEG,			// EOT ENQ ACK BEL
	ILLEG, WHITE, ILLEG, ILLEG,			// BS HT LF VT
	WHITE, ILLEG, ILLEG, ILLEG,			// FF CR SO SI

	ILLEG, ILLEG, ILLEG, ILLEG,			// DLE DC1 DC2 DC3
	ILLEG, ILLEG, ILLEG, ILLEG,			// DC4 NAK SYN ETB
	ILLEG, ILLEG, ILLEG, ILLEG,			// CAN EM SUB ESC
	ILLEG, ILLEG, ILLEG, ILLEG,			// FS GS RS US

	WHITE, MULTX, MULTX, SELF,			// SP ! " #
	MULTX+CTSYM, MULTX, SELF, MULTX,	// $ % & '
	SELF, SELF, SELF, SELF,				// ( ) * +
	SELF, SELF, STSYM, SELF,			// , - . /

	DIGIT+HDIGIT+CTSYM, DIGIT+HDIGIT+CTSYM,		// 0 1
	DIGIT+HDIGIT+CTSYM, DIGIT+HDIGIT+CTSYM,		// 2 3
	DIGIT+HDIGIT+CTSYM, DIGIT+HDIGIT+CTSYM,		// 4 5
	DIGIT+HDIGIT+CTSYM, DIGIT+HDIGIT+CTSYM,		// 6 7
	DIGIT+HDIGIT+CTSYM, DIGIT+HDIGIT+CTSYM,		// 8 9
	MULTX, MULTX,								// : ;
	MULTX, MULTX, MULTX, STSYM+CTSYM,			// < = > ?

	MULTX, STSYM+CTSYM+HDIGIT,					// @ A
	DOT+STSYM+CTSYM+HDIGIT, STSYM+CTSYM+HDIGIT,	// B C
	DOT+STSYM+CTSYM+HDIGIT, STSYM+CTSYM+HDIGIT,	// D E
	STSYM+CTSYM+HDIGIT, STSYM+CTSYM,			// F G
	STSYM+CTSYM, DOT+STSYM+CTSYM, STSYM+CTSYM, STSYM+CTSYM,	// H I J K
	DOT+STSYM+CTSYM, STSYM+CTSYM, STSYM+CTSYM, STSYM+CTSYM,	// L M N O

	DOT+STSYM+CTSYM, DOT+STSYM+CTSYM, STSYM+CTSYM, DOT+STSYM+CTSYM,	// P Q R S
	STSYM+CTSYM, STSYM+CTSYM, STSYM+CTSYM, DOT+STSYM+CTSYM,	// T U V W
	STSYM+CTSYM, STSYM+CTSYM, STSYM+CTSYM, SELF,// X Y Z [
	SELF, SELF, MULTX, STSYM+CTSYM,				// \ ] ^ _

	ILLEG, STSYM+CTSYM+HDIGIT,					// ` a
	DOT+STSYM+CTSYM+HDIGIT, STSYM+CTSYM+HDIGIT,	// b c
	DOT+STSYM+CTSYM+HDIGIT, STSYM+CTSYM+HDIGIT,	// d e
	STSYM+CTSYM+HDIGIT, STSYM+CTSYM,			// f g
	STSYM+CTSYM, DOT+STSYM+CTSYM, STSYM+CTSYM, STSYM+CTSYM,	// h i j k
	DOT+STSYM+CTSYM, STSYM+CTSYM, STSYM+CTSYM, STSYM+CTSYM,	// l m n o

	DOT+STSYM+CTSYM, DOT+STSYM+CTSYM, STSYM+CTSYM, DOT+STSYM+CTSYM,	// p q r s
	STSYM+CTSYM, STSYM+CTSYM, STSYM+CTSYM, DOT+STSYM+CTSYM,	// t u v w
	DOT+STSYM+CTSYM, STSYM+CTSYM, STSYM+CTSYM, SELF,		// x y z {
	SELF, SELF, SELF, ILLEG,					// | } ~ DEL

	// Anything above $7F is illegal (and yes, we need to check for this,
	// otherwise you get strange and spurious errors that will lead you astray)
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG,
	ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG, ILLEG
};

// Names of registers
static char * regname[] = {
	"d0","d1","d2","d3","d4","d5","d6","d7", // 128,135
	"a0","a1","a2","a3","a4","a5","a6","sp", // 136,143
	"ssp","pc","sr","ccr","regequ","set","reg","r0", // 144,151
	"r1","r2","r3","r4","r5","r6","r7","r8", // 152,159
	"r9","r10","r11","r12","r13","r14","r15","r16", // 160,167
	"r17","r18","r19","r20","r21","r22","r23","r24", // 168,175
	"r25","r26","r27","r28","r29","r30","r31","ccdef", // 176,183
	"usp","ic40","dc40","bc40","sfc","dfc","","vbr", // 184,191
	"cacr","caar","msp","isp","tc","itt0","itt1","dtt0", // 192,199
	"dtt1","mmusr","urp","srp","iacr0","iacr1","dacr0","dacr1", // 200,207
	"tt0","tt1","crp","","","","","", // 208,215
	"","","","","fpiar","fpsr","fpcr","", // 216,223
	"fp0","fp1","fp2","fp3","fp4","fp5","fp6","fp7", // 224,231
	"","","","","","","","", // 232,239
	"","","","","","","","", // 240,247
	"","","","","","","","", // 248,255
	"","","","","x0","x1","y0","y1", // 256,263
	"","b0","","b2","","b1","a","b", // 264,271
	"mr","omr","la","lc","ssh","ssl","ss","", // 272,279
	"n0","n1","n2","n3","n4","n5","n6","n7", // 280,287
	"m0","m1","m2","m3","m4","m5","m6","m7", // 288,295
	"","","","","","","l","p", // 296,303
	"mr","omr","la","lc","ssh","ssl","ss","", // 304,311
	"a10","b10","x","y","","","ab","ba"  // 312,319
};


//
// Initialize tokenizer
//
void InitTokenizer(void)
{
	int i;									// Iterator
	char * htab = "0123456789abcdefABCDEF";	// Hex character table

	lnsave = 0;								// Don't save lines
	curfname = "";							// No file, empty filename
	filecount = (WORD)-1;
	cfileno = (WORD)-1;						// cfileno gets bumped to 0
	curlineno = 0;
	totlines = 0;
	etok = tokbuf;
	f_inobj = NULL;
	f_ifile = NULL;
	f_imacro = NULL;
	cur_inobj = NULL;
	filerec = NULL;
	last_fr = NULL;
	lntag = SPACE;

	// Initialize hex, "dot" and tolower tables
	for(i=0; i<128; i++)
	{
		hextab[i] = -1;
		dotxtab[i] = 0;
		tolowertab[i] = (char)i;
	}

	for(i=0; htab[i]!=EOS; i++)
		hextab[htab[i]] = (char)((i < 16) ? i : i - 6);

	for(i='A'; i<='Z'; i++)
		tolowertab[i] |= 0x20;

	// These characters are legal immediately after a period
	dotxtab['b'] = DOTB;					// .b .B .s .S
	dotxtab['B'] = DOTB;
	//dotxtab['s'] = DOTB;
	//dotxtab['S'] = DOTB;
	dotxtab['w'] = DOTW;					// .w .W
	dotxtab['W'] = DOTW;
	dotxtab['l'] = DOTL;					// .l .L
	dotxtab['L'] = DOTL;
	dotxtab['i'] = DOTI;					// .i .I (WTF is this???)
	dotxtab['I'] = DOTI;
	dotxtab['D'] = DOTD;					// .d .D (double)
	dotxtab['d'] = DOTD;
	dotxtab['S'] = DOTS;					// .s .S
	dotxtab['s'] = DOTS;
	dotxtab['Q'] = DOTQ;					// .q .Q (quad word)
	dotxtab['q'] = DOTQ;
	dotxtab['X'] = DOTX;					// .x .x
	dotxtab['x'] = DOTX;
	dotxtab['P'] = DOTP;					// .p .P
	dotxtab['p'] = DOTP;
}


void SetFilenameForErrorReporting(void)
{
	WORD fnum = cfileno;

	// Check for absolute top filename (this should never happen)
	if ((int16_t)fnum == -1)
	{
		curfname = "(*top*)";
		return;
	}

	FILEREC * fr = filerec;

	// Advance to the correct record...
	while (fr != NULL && fnum != 0)
	{
		fr = fr->frec_next;
		fnum--;
	}

	// Check for file # record not found (this should never happen either)
	if (fr == NULL)
	{
		curfname = "(*NOT FOUND*)";
		return;
	}

	curfname = fr->frec_name;
}


//
// Allocate an IFILE or IMACRO
//
INOBJ * a_inobj(int typ)
{
	INOBJ * inobj;
	IFILE * ifile;
	IMACRO * imacro;

	// Allocate and initialize INOBJ first
	if (f_inobj == NULL)
		inobj = malloc(sizeof(INOBJ));
	else
	{
		inobj = f_inobj;
		f_inobj = f_inobj->in_link;
	}

	switch (typ)
	{
	case SRC_IFILE:							// Alloc and init an IFILE
		if (f_ifile == NULL)
			ifile = malloc(sizeof(IFILE));
		else
		{
			ifile = f_ifile;
			f_ifile = f_ifile->if_link;
		}

		inobj->inobj.ifile = ifile;
		break;

	case SRC_IMACRO:						// Alloc and init an IMACRO
		if (f_imacro == NULL)
			imacro = malloc(sizeof(IMACRO));
		else
		{
			imacro = f_imacro;
			f_imacro = f_imacro->im_link;
		}

		inobj->inobj.imacro = imacro;
		break;

	case SRC_IREPT:							// Alloc and init an IREPT
		inobj->inobj.irept = malloc(sizeof(IREPT));
		DEBUG { printf("alloc IREPT\n"); }
		break;
	}

	// Install INOBJ on top of input stack
	inobj->in_ifent = ifent;				// Record .if context on entry
	inobj->in_type = (WORD)typ;
	inobj->in_otok = tok;
	inobj->in_etok = etok;
	inobj->in_link = cur_inobj;
	cur_inobj = inobj;

	return inobj;
}


//
// Perform macro substitution from 'orig' to 'dest'. Return OK or some error.
// A macro reference is in one of two forms:
// \name <non-name-character>
// \{name}
// A doubled backslash (\\) is compressed to a single backslash (\).
// Argument definitions have been pre-tokenized, so we have to turn them back
// into text. This means that numbers, in particular, become hex, regardless of
// their representation when the macro was invoked. This is a hack.
// A label may appear at the beginning of the line:
// :<name><whitespace>
// (the colon must be in the first column). These labels are stripped before
// macro expansion takes place.
//
int ExpandMacro(char * src, char * dest, int destsiz)
{
	int i;
	int questmark;			// \? for testing argument existence
	char mname[128];		// Assume max size of a formal arg name
	char numbuf[20];		// Buffer for text of CONSTs
	TOKEN * tk;
	SYM * arg;
	char ** symbolString;

	DEBUG { printf("ExM: src=\"%s\"\n", src); }

	IMACRO * imacro = cur_inobj->inobj.imacro;
	int macnum = (int)(imacro->im_macro->sattr);

	char * dst = dest;						// Next dest slot
	char * edst = dest + destsiz - 1;		// End + 1(?) of dest buffer

	// Check for (and skip over) any "label" on the line
	char * s = src;
	char * d = NULL;

	if (*s == ':')
	{
		while (*s != EOS && !(chrtab[*s] & WHITE))
			s++;

		if (*s != EOS)
			s++;							// Skip first whitespace
	}

	// Expand the rest of the line
	while (*s != EOS)
	{
		// Copy single character
		if (*s != '\\')
		{
			if (dst >= edst)
				goto overflow;

			// Skip comments in case a loose @ or \ is in there
			// In that case the tokeniser was trying to expand it.
			if ((*s == ';') || ((*s == '/') && (*(s + 1) == '/')))
				goto skipcomments;

			*dst++ = *s++;
		}
		// Do macro expansion
		else
		{
			questmark = 0;

			// Do special cases
			switch (*++s)
			{
			case '\\':						// \\, \ (collapse to single backslash)
				if (dst >= edst)
					goto overflow;

				*dst++ = *s++;
				continue;
			case '?':						// \? <macro>  set `questmark' flag
				s++;
				questmark = 1;
				break;
			case '#':						// \#, number of arguments
				sprintf(numbuf, "%d", (int)imacro->im_nargs);
				goto copystr;
			case '!':						// \! size suffix supplied on invocation
				switch ((int)imacro->im_siz)
				{
				case SIZN: d = "";   break;
				case SIZB: d = ".b"; break;
				case SIZW: d = ".w"; break;
				case SIZL: d = ".l"; break;
				}

				goto copy_d;
			case '~':						// ==> unique label string Mnnnn...
				sprintf(numbuf, "M%u", curuniq);
copystr:
				d = numbuf;
copy_d:
				s++;

				while (*d != EOS)
				{
					if (dst >= edst)
						goto overflow;
					else
						*dst++ = *d++;
				}

				continue;
			case EOS:
				return error("missing argument name");
			}

			// \n ==> argument number 'n', 0..9
			if (chrtab[*s] & DIGIT)
			{
				i = *s++ - '1';

				if (i < 0)
					i = 9;

				goto arg_num;
			}

			// Get argument name: \name, \{name}
			d = mname;

			// \label
			if (*s != '{')
			{
				do
				{
					*d++ = *s++;
				}
				while (chrtab[*s] & CTSYM);
			}
			// \\{label}
			else
			{
				for(++s; *s != EOS && *s != '}';)
					*d++ = *s++;

				if (*s != '}')
					return error("missing closing brace ('}')");
				else
					s++;
			}

			*d = EOS;

			// Lookup the argument and copy its (string) value into the
			// destination string
			DEBUG { printf("argument='%s'\n", mname); }

			if ((arg = lookup(mname, MACARG, macnum)) == NULL)
				return error("undefined argument: '%s'", mname);
			else
			{
				// Convert a string of tokens (terminated with EOL) back into
				// text. If an argument is out of range (not specified in the
				// macro invocation) then it is ignored.
				i = (int)arg->svalue;
arg_num:
				DEBUG { printf("~argnumber=%d\n", i); }
				tk = NULL;

				if (i < imacro->im_nargs)
				{
					tk = imacro->argument[i].token;
					symbolString = imacro->argument[i].string;
//DEBUG
//{
//	printf("ExM: Preparing to parse argument #%u...\n", i);
//	DumpTokens(tk);
//}
				}

				// \?arg yields:
				//    0  if the argument is empty or non-existant,
				//    1  if the argument is not empty
				if (questmark)
				{
					if (tk == NULL || *tk == EOL)
						questmark = 0;

					if (dst >= edst)
						goto overflow;

					*dst++ = (char)(questmark + '0');
					continue;
				}

				// Argument # is in range, so expand it
				if (tk != NULL)
				{
					while (*tk != EOL)
					{
						// Reverse-translation from a token number to a string.
						// This is a hack. It might be better table-driven.
						d = NULL;

						if (*tk >= REG68_D0)
						{
							d = regname[(int)*tk++ - REG68_D0];
							goto strcopy;
						}
						else
						{
							switch ((int)*tk++)
							{
							case SYMBOL:
								d = symbolString[*tk++];
DEBUG { printf("ExM: SYMBOL=\"%s\"", d); }
								break;
							case STRING:
								d = symbolString[*tk++];

								if (dst >= edst)
									goto overflow;

								*dst++ = '"';

								while (*d != EOS)
								{
									if (dst >= edst)
										goto overflow;
									else
										*dst++ = *d++;
								}

								if (dst >= edst)
									goto overflow;

								*dst++ = '"';
								continue;
								break;
// Shamus: Changing the format specifier from %lx to %ux caused the assembler
//         to choke on legitimate code... Need to investigate this further
//         before changing anything else here!
							case CONST:
//								sprintf(numbuf, "$%lx", (uint64_t)*tk++);
								sprintf(numbuf, "$%" PRIX64, (uint64_t)*tk++);
								tk++;
								d = numbuf;
								break;
							case DEQUALS:
								d = "==";
								break;
							case SET:
								d = "set";
								break;
							case COLON:
								d = ":";
								break;
							case DCOLON:
								d = "::";
								break;
							case GE:
								d = ">=";
								break;
							case LE:
								d = "<=";
								break;
							case NE:
								d = "<>";
								break;
							case SHR:
								d = ">>";
								break;
							case SHL:
								d = "<<";
								break;
							case DOTB:
								d = ".b";
								break;
							case DOTW:
								d = ".w";
								break;
							case DOTL:
								d = ".l";
								break;
							case CR_ABSCOUNT:
								d = "^^abscount";
								break;
							case CR_FILESIZE:
								d = "^^filesize";
								break;
							case CR_DATE:
								d = "^^date";
								break;
							case CR_TIME:
								d = "^^time";
								break;
							case CR_DEFINED:
								d = "^^defined ";
								break;
							case CR_REFERENCED:
								d = "^^referenced ";
								break;
							case CR_STREQ:
								d = "^^streq ";
								break;
							case CR_MACDEF:
								d = "^^macdef ";
								break;
							default:
								if (dst >= edst)
									goto overflow;

								*dst++ = (char)*(tk - 1);
								break;
							}
						}

						// If 'd' != NULL, copy string to destination
						if (d != NULL)
						{
strcopy:
							DEBUG printf("d='%s'\n", d);

							while (*d != EOS)
							{
								if (dst >= edst)
									goto overflow;
								else
									*dst++ = *d++;
							}
						}
					}
				}
			}
		}
	}

skipcomments:

	*dst = EOS;
	DEBUG { printf("ExM: dst=\"%s\"\n", dest); }
	return OK;

overflow:
	*dst = EOS;
	DEBUG { printf("*** OVERFLOW LINE ***\n%s\n", dest); }
	return fatal("line too long as a result of macro expansion");
}


//
// Get next line of text from a macro
//
char * GetNextMacroLine(void)
{
	IMACRO * imacro = cur_inobj->inobj.imacro;
	LLIST * strp = imacro->im_nextln;

	if (strp == NULL)						// End-of-macro
		return NULL;

	imacro->im_nextln = strp->next;
//	ExpandMacro((char *)(strp + 1), imacro->im_lnbuf, LNSIZ);
	ExpandMacro(strp->line, imacro->im_lnbuf, LNSIZ);

	return imacro->im_lnbuf;
}


//
// Get next line of text from a repeat block
//
char * GetNextRepeatLine(void)
{
	IREPT * irept = cur_inobj->inobj.irept;
//	LONG * strp = irept->ir_nextln;			// initial null

	// Do repeat at end of .rept block's string list
//	if (strp == NULL)
	if (irept->ir_nextln == NULL)
	{
		DEBUG { printf("back-to-top-of-repeat-block count=%d\n", (int)irept->ir_count); }
		irept->ir_nextln = irept->ir_firstln;	// copy first line

		if (irept->ir_count-- == 0)
		{
			DEBUG { printf("end-repeat-block\n"); }
			return NULL;
		}
		reptuniq++;
//		strp = irept->ir_nextln;
	}
	// Mark the current macro line in the irept object
	// This is probably overkill - a global variable
	// would suffice here (it only gets used during
	// error reporting anyway)
	irept->lineno = irept->ir_nextln->lineno;

	// Copy the rept lines verbatim, unless we're in nest level 0.
	// Then, expand any \~ labels to unique numbers (Rn)
	if (rptlevel)
	{
		strcpy(irbuf, irept->ir_nextln->line);
	}
	else
	{
		uint32_t linelen = strlen(irept->ir_nextln->line);
		uint8_t *p_line = irept->ir_nextln->line;
		char *irbufwrite = irbuf;
		for (int i = 0; i <= linelen; i++)
		{
			uint8_t c;
			c = *p_line++;
			if (c == '\\' && *p_line == '~')
			{
				p_line++;
				irbufwrite += sprintf(irbufwrite, "R%u", reptuniq);
			}
			else
			{
				*irbufwrite++ = c;
			}
		}
	}

	DEBUG { printf("repeat line='%s'\n", irbuf); }
//	irept->ir_nextln = (LONG *)*strp;
	irept->ir_nextln = irept->ir_nextln->next;

	return irbuf;
}


//
// Include a source file used at the root, and for ".include" files
//
int include(int handle, char * fname)
{
	// Debug mode
	DEBUG { printf("[include: %s, cfileno=%u]\n", fname, cfileno); }

	// Alloc and initialize include-descriptors
	INOBJ * inobj = a_inobj(SRC_IFILE);
	IFILE * ifile = inobj->inobj.ifile;

	ifile->ifhandle = handle;			// Setup file handle
	ifile->ifind = ifile->ifcnt = 0;	// Setup buffer indices
	ifile->ifoldlineno = curlineno;		// Save old line number
	ifile->ifoldfname = curfname;		// Save old filename
	ifile->ifno = cfileno;				// Save old file number

	// NB: This *must* be preincrement, we're adding one to the filecount here!
	cfileno = ++filecount;				// Compute NEW file number
	curfname = strdup(fname);			// Set current filename (alloc storage)
	curlineno = 0;						// Start on line zero

	// Add another file to the file-record
	FILEREC * fr = (FILEREC *)malloc(sizeof(FILEREC));
	fr->frec_next = NULL;
	fr->frec_name = curfname;

	if (last_fr == NULL)
		filerec = fr;					// Add first filerec
	else
		last_fr->frec_next = fr;		// Append to list of filerecs

	last_fr = fr;
	DEBUG { printf("[include: curfname: %s, cfileno=%u]\n", curfname, cfileno); }

	return OK;
}


//
// Pop the current input level
//
int fpop(void)
{
	INOBJ * inobj = cur_inobj;

	if (inobj == NULL)
		return 0;

	// Pop IFENT levels until we reach the conditional assembly context we
	// were at when the input object was entered.
	int numUnmatched = 0;

	while (ifent != inobj->in_ifent)
	{
		if (d_endif() != 0)	// Something bad happened during endif parsing?
			return -1;		// If yes, bail instead of getting stuck in a loop

		numUnmatched++;
	}

	// Give a warning to the user that we had to wipe their bum for them
	if (numUnmatched > 0)
		warn("missing %d .endif(s)", numUnmatched);

	tok = inobj->in_otok;	// Restore tok and etok
	etok = inobj->in_etok;

	switch (inobj->in_type)
	{
	case SRC_IFILE:			// Pop and release an IFILE
	{
		DEBUG { printf("[Leaving: %s]\n", curfname); }

		IFILE * ifile = inobj->inobj.ifile;
		ifile->if_link = f_ifile;
		f_ifile = ifile;
		close(ifile->ifhandle);			// Close source file
DEBUG { printf("[fpop (pre):  curfname=%s]\n", curfname); }
		curfname = ifile->ifoldfname;	// Set current filename
DEBUG { printf("[fpop (post): curfname=%s]\n", curfname); }
DEBUG { printf("[fpop: (pre)  cfileno=%d ifile->ifno=%d]\n", (int)cfileno, (int)ifile->ifno); }
		curlineno = ifile->ifoldlineno;	// Set current line#
		DEBUG { printf("cfileno=%d ifile->ifno=%d\n", (int)cfileno, (int)ifile->ifno); }
		cfileno = ifile->ifno;			// Restore current file number
DEBUG { printf("[fpop: (post) cfileno=%d ifile->ifno=%d]\n", (int)cfileno, (int)ifile->ifno); }
		break;
	}

	case SRC_IMACRO:					// Pop and release an IMACRO
	{
		IMACRO * imacro = inobj->inobj.imacro;
		imacro->im_link = f_imacro;
		f_imacro = imacro;
		break;
	}

	case SRC_IREPT:						// Pop and release an IREPT
	{
		DEBUG { printf("dealloc IREPT\n"); }
		LLIST * p = inobj->inobj.irept->ir_firstln;

		// Deallocate repeat lines
		while (p != NULL)
		{
			free(p->line);
			p = p->next;
		}

		break;
	}
	}

	cur_inobj = inobj->in_link;
	inobj->in_link = f_inobj;
	f_inobj = inobj;

	return 0;
}


//
// Get line from file into buf, return NULL on EOF or ptr to the start of a
// null-term line
//
char * GetNextLine(void)
{
	int i, j;
	char * p, * d;
	int readamt = -1;						// 0 if last read() yeilded 0 bytes
	IFILE * fl = cur_inobj->inobj.ifile;

	for(;;)
	{
		// Scan for next end-of-line; handle stupid text formats by treating
		// \r\n the same as \n. (lone '\r' at end of buffer means we have to
		// check for '\n').
		d = &fl->ifbuf[fl->ifind];

		for(p=d, i=0, j=fl->ifcnt; i<j; i++, p++)
		{
			if (*p == '\r' || *p == '\n')
			{
				i++;

				if (*p == '\r')
				{
					if (i >= j)
						break;	// Need to read more, then look for '\n' to eat
					else if (p[1] == '\n')
						i++;
				}

				// Cover up the newline with end-of-string sentinel
				*p = '\0';

				fl->ifind += i;
				fl->ifcnt -= i;
				return d;
			}
		}

		// Handle hanging lines by ignoring them (Input file is exhausted, no
		// \r or \n on last line)
		// Shamus: This is retarded. Never ignore any input!
		if (!readamt && fl->ifcnt)
		{
#if 0
			fl->ifcnt = 0;
			*p = '\0';
			return NULL;
#else
			// Really should check to see if we're at the end of the buffer!
			// :-P
			fl->ifbuf[fl->ifind + fl->ifcnt] = '\0';
			fl->ifcnt = 0;
			return &fl->ifbuf[fl->ifind];
#endif
		}

		// Truncate and return absurdly long lines.
		if (fl->ifcnt >= QUANTUM)
		{
			fl->ifbuf[fl->ifind + fl->ifcnt - 1] = '\0';
			fl->ifcnt = 0;
			return &fl->ifbuf[fl->ifind];
		}

		// Relocate what's left of a line to the beginning of the buffer, and
		// read some more of the file in; return NULL if the buffer's empty and
		// on EOF.
		if (fl->ifind != 0)
		{
			p = &fl->ifbuf[fl->ifind];
			d = &fl->ifbuf[fl->ifcnt & 1];

			for(i=0; i<fl->ifcnt; i++)
				*d++ = *p++;

			fl->ifind = fl->ifcnt & 1;
		}

		readamt = read(fl->ifhandle, &fl->ifbuf[fl->ifind + fl->ifcnt], QUANTUM);

		if (readamt < 0)
			return NULL;

		if ((fl->ifcnt += readamt) == 0)
			return NULL;
	}
}


//
// Tokenize a line
//
int TokenizeLine(void)
{
	uint8_t * ln = NULL;		// Ptr to current position in line
	uint8_t * p;				// Random character ptr
	PTR tk;						// Token-deposit ptr
	int state = 0;				// State for keyword detector
	int j = 0;					// Var for keyword detector
	uint8_t c;					// Random char
	uint64_t v;					// Random value
	uint32_t cursize = 0;		// Current line's size (.b, .w, .l, .s, .q, .d)
	uint8_t * nullspot = NULL;	// Spot to clobber for SYMBOL termination
	int stuffnull;				// 1:terminate SYMBOL '\0' at *nullspot
	uint8_t c1;
	int stringNum = 0;			// Pointer to string locations in tokenized line
	SYM* sy;					// For looking up symbols (.equr)
	int equrundef = 0;			// Flag for equrundef scanning

retry:

	if (cur_inobj == NULL)		// Return EOF if input stack is empty
		return TKEOF;

	// Get another line of input from the current input source: a file, a
	// macro, or a repeat-block
	switch (cur_inobj->in_type)
	{
	// Include-file:
	// o  handle EOF;
	// o  bump source line number;
	// o  tag the listing-line with a space;
	// o  kludge lines generated by Alcyon C.
	case SRC_IFILE:
		if ((ln = GetNextLine()) == NULL)
		{
DEBUG { printf("TokenizeLine: Calling fpop() from SRC_IFILE...\n"); }
			if (fpop() == 0)	// Pop input level
				goto retry;		// Try for more lines
			else
			{
				ifent->if_prev = (IFENT *)-1;	//Signal Assemble() that we have reached EOF with unbalanced if/endifs
				return TKEOF;
			}
		}

		curlineno++;			// Bump line number
		lntag = SPACE;

		break;

	// Macro-block:
	// o  Handle end-of-macro;
	// o  tag the listing-line with an at (@) sign.
	case SRC_IMACRO:
		if ((ln = GetNextMacroLine()) == NULL)
		{
			if (ExitMacro() == 0)	// Exit macro (pop args, do fpop(), etc)
				goto retry;			// Try for more lines...
			else
				return TKEOF;		// Oops, we got a non zero return code, signal EOF
		}

		lntag = '@';
		break;

	// Repeat-block:
	// o  Handle end-of-repeat-block;
	// o  tag the listing-line with a pound (#) sign.
	case SRC_IREPT:
		if ((ln = GetNextRepeatLine()) == NULL)
		{
			DEBUG { printf("TokenizeLine: Calling fpop() from SRC_IREPT...\n"); }
			fpop();
			goto retry;
		}

		lntag = '#';
		break;
	}

	// Save text of the line. We only do this during listings and within
	// macro-type blocks, since it is expensive to unconditionally copy every
	// line.
	if (lnsave)
	{
		// Sanity check
		if (strlen(ln) > LNSIZ)
			return error("line too long (%d, max %d)", strlen(ln), LNSIZ);

		strcpy(lnbuf, ln);
	}

	// General housekeeping
	tok = tokeol;			// Set "tok" to EOL in case of error
	tk.u32 = etok;			// Reset token ptr
	stuffnull = 0;			// Don't stuff nulls
	totlines++;				// Bump total #lines assembled

	// See if the entire line is a comment. This is a win if the programmer
	// puts in lots of comments
	if (*ln == '*' || *ln == ';' || ((*ln == '/') && (*(ln + 1) == '/')))
		goto goteol;

	// And here we have a very ugly hack for signalling a single line 'turn off
	// optimization'. There's really no nice way to do this, so hack it is!
	optimizeOff = 0;		// Default is to take optimizations as they come

	if (*ln == '!')
	{
		optimizeOff = 1;	// Signal that we don't want to optimize this line
		ln++;				// & skip over the darned thing
	}

	// Main tokenization loop;
	//  o  skip whitespace;
	//  o  handle end-of-line;
	//  o  handle symbols;
	//  o  handle single-character tokens (operators, etc.);
	//  o  handle multiple-character tokens (constants, strings, etc.).
	for(; *ln!=EOS;)
	{
		// Check to see if there's enough space in the token buffer
		if (tk.cp >= ((uint8_t *)(&tokbuf[TOKBUFSIZE])) - 20)
		{
			return error("token buffer overrun");
		}

		// Skip whitespace, handle EOL
		while (chrtab[*ln] & WHITE)
			ln++;

		// Handle EOL, comment with ';'
		if (*ln == EOS || *ln == ';'|| ((*ln == '/') && (*(ln + 1) == '/')))
			break;

		// Handle start of symbol. Symbols are null-terminated in place. The
		// termination is always one symbol behind, since there may be no place
		// for a null in the case that an operator immediately follows the name.
		c = chrtab[*ln];

		if (c & STSYM)
		{
			if (stuffnull)			// Terminate old symbol from previous pass
				*nullspot = EOS;

			v = 0;					// Assume no DOT attrib follows symbol
			stuffnull = 1;

			// In some cases, we need to check for a DOTx at the *beginning*
			// of a symbol, as the "start" of the line we're currently looking
			// at could be somewhere in the middle of that line!
			if (*ln == '.')
			{
				// Make sure that it's *only* a .[bwsl] following, and not the
				// start of a local symbol:
				if ((chrtab[*(ln + 1)] & DOT)
					&& (dotxtab[*(ln + 1)] != 0)
					&& !(chrtab[*(ln + 2)] & CTSYM))
				{
					// We found a legitimate DOTx construct, so add it to the
					// token stream:
					ln++;
					stuffnull = 0;
					*tk.u32++ = (TOKEN)dotxtab[*ln++];
					continue;
				}
			}

			p = nullspot = ln++;	// Nullspot -> start of this symbol

			// Find end of symbol (and compute its length)
			for(j=1; (int)chrtab[*ln]&CTSYM; j++)
				ln++;

			// Handle "DOT" special forms (like ".b") that follow a normal
			// symbol or keyword:
			if (*ln == '.')
			{
				*ln++ = EOS;		// Terminate symbol
				stuffnull = 0;		// And never try it again

				// Character following the '.' must have a DOT attribute, and
				// the chararacter after THAT one must not have a start-symbol
				// attribute (to prevent symbols that look like, for example,
				// "zingo.barf", which might be a good idea anyway....)
				if (((chrtab[*ln] & DOT) == 0) || (dotxtab[*ln] == 0))
					return error("[bwsl] must follow '.' in symbol");

				v = (uint32_t)dotxtab[*ln++];
				cursize = (uint32_t)v;

				if (chrtab[*ln] & CTSYM)
					return error("misuse of '.'; not allowed in symbols");
			}

			// If the symbol is small, check to see if it's really the name of
			// a register.
			uint8_t *p2 = p;
			if (j <= 5)
			{
				for (state = 0; state >= 0;)
				{
					j = (int)tolowertab[*p++];
					j += regbase[state];

					if (regcheck[j] != state)
					{
						j = -1;
						break;
					}

					if (*p == EOS || p == ln)
					{
						j = regaccept[j];
						goto skip_keyword;
						break;
					}

					state = regtab[j];
				}
			}

			// Scan for keywords
			if ((j <= 0 || state <= 0) || p==p2)
			{
				if (j <= KWSIZE)
				{
					for (state = 0; state >= 0;)
					{
						j = (int)tolowertab[*p2++];
						j += kwbase[state];

						if (kwcheck[j] != state)
						{
							j = -1;
							break;
						}

						if (*p == EOS || p2 == ln)
						{
							j = kwaccept[j];
							break;
						}

						state = kwtab[j];
					}
				}
				else
				{
					j = -1;
				}
			}

			skip_keyword:

			// If we detected equrundef/regundef set relevant flag
			if (j == KW_EQURUNDEF)
			{
				equrundef = 1;
				j = -1;
			}

			// If not tokenized keyword OR token was not found
			if ((j < 0) || (state < 0))
			{
				// Only proceed if no equrundef has been detected. In that case we need to store the symbol
				// because the directive handler (d_equrundef) will run outside this loop, further into procln.c
				if (!equrundef && !disabled)
				{
					// Last attempt: let's see if this is an equated register.
					// If yes, then just store the register's keyword value instead of the symbol
					char temp = *ln;
					*ln = 0;
					sy = lookup(nullspot, LABEL, 0);
					*ln = temp;
					if (sy)
					{
						if (sy->sattre & EQUATEDREG)
						{
							*tk.u32++ = sy->svalue;
							stuffnull = 0;
							continue;
						}
					}
				}
				// Ok, that failed, let's store the symbol instead
				*tk.u32++ = SYMBOL;
				string[stringNum] = nullspot;
				*tk.u32++ = stringNum;
				stringNum++;
			}
			else
			{
				*tk.u32++ = (TOKEN)j;
				stuffnull = 0;
			}

			if (v)			// Record attribute token (if any)
				*tk.u32++ = (TOKEN)v;

			if (stuffnull)	// Arrange for string termination on next pass
				nullspot = ln;

			if (disabled)
			{
				// When we are in a disabled code block, the only thing that can break out
				// of this is an ".endif" keyword, so this is the minimum we have to parse
				// in order to discover such a keyword.
				goto goteol;
			}

			continue;
		}

		// Handle identity tokens
		if (c & SELF)
		{
			*tk.u32++ = *ln++;
			continue;
		}

		// Handle multiple-character tokens
		if (c & MULTX)
		{
			switch (*ln++)
			{
			case '!':		// ! or !=
				if (*ln == '=')
				{
					*tk.u32++ = NE;
					ln++;
				}
				else
					*tk.u32++ = '!';

				continue;
			case '\'':		// 'string'
				if (m6502)
				{
					// Hardcoded for now, maybe this will change in the future
					*tk.u32++ = STRINGA8;
					goto dostring;
				}
				// Fall through
			case '\"':		// "string"
				*tk.u32++ = STRING;
dostring:
				c1 = ln[-1];
				string[stringNum] = ln;
				*tk.u32++ = stringNum;
				stringNum++;

				for(p=ln; *ln!=EOS && *ln!=c1;)
				{
					c = *ln++;

					if (c == '\\')
					{
						switch (*ln++)
						{
						case EOS:
							return(error("unterminated string"));
						case 'e':
							c = '\033';
							break;
						case 'n':
							c = '\n';
							break;
						case 'b':
							c = '\b';
							break;
						case 't':
							c = '\t';
							break;
						case 'r':
							c = '\r';
							break;
						case 'f':
							c = '\f';
							break;
						case '\"':
							c = '\"';
							break;
						case '\'':
							c = '\'';
							break;
						case '\\':
							c = '\\';
							break;
						case '{':
							// If we're evaluating a macro
							// this is valid because it's
							// a parameter expansion
						case '!':
							// If we're evaluating a macro
							// this is valid and expands to
							// "dot-size"
							break;
						default:
							warn("bad backslash code in string");
							ln--;
							break;
						}
					}

					*p++ = c;
				}

				if (*ln++ != c1)
					return error("unterminated string");

				*p++ = EOS;
				continue;
			case '$':		// $, hex constant
				if (chrtab[*ln] & HDIGIT)
				{
					v = 0;

					// Parse the hex value
					while (hextab[*ln] >= 0)
						v = (v << 4) + (int)hextab[*ln++];

					*tk.u32++ = CONST;
					*tk.u64++ = v;

					if (*ln == '.')
					{
						if ((*(ln + 1) == 'w') || (*(ln + 1) == 'W'))
						{
							*tk.u32++ = DOTW;
							ln += 2;
						}
						else if ((*(ln + 1) == 'l') || (*(ln + 1) == 'L'))
						{
							*tk.u32++ = DOTL;
							ln += 2;
						}
					}
				}
				else
					*tk.u32++ = '$';

				continue;
			case '<':		// < or << or <> or <=
				switch (*ln)
				{
				case '<':
					*tk.u32++ = SHL;
					ln++;
					continue;
				case '>':
					*tk.u32++ = NE;
					ln++;
					continue;
				case '=':
					*tk.u32++ = LE;
					ln++;
					continue;
				default:
					*tk.u32++ = '<';
					continue;
				}
			case ':':		// : or ::
				if (*ln == ':')
				{
					*tk.u32++ = DCOLON;
					ln++;
				}
				else
					*tk.u32++ = ':';

				continue;
			case '=':		// = or ==
				if (*ln == '=')
				{
					*tk.u32++ = DEQUALS;
					ln++;
				}
				else
					*tk.u32++ = '=';

				continue;
			case '>':		// > or >> or >=
				switch (*ln)
				{
				case '>':
					*tk.u32++ = SHR;
					ln++;
					continue;
				case '=':
					*tk.u32++ = GE;
					ln++;
					continue;
				default:
					*tk.u32++ = '>';
					continue;
				}
			case '%':		// % or binary constant
				if (*ln < '0' || *ln > '1')
				{
					*tk.u32++ = '%';
					continue;
				}

				v = 0;

				while (*ln >= '0' && *ln <= '1')
					v = (v << 1) + *ln++ - '0';

				if (*ln == '.')
				{
					if ((*(ln + 1) == 'b') || (*(ln + 1) == 'B'))
					{
						v &= 0x000000FF;
						ln += 2;
					}

					if ((*(ln + 1) == 'w') || (*(ln + 1) == 'W'))
					{
						v &= 0x0000FFFF;
						ln += 2;
					}

					if ((*(ln + 1) == 'l') || (*(ln + 1) == 'L'))
					{
						v &= 0xFFFFFFFF;
						ln += 2;
					}
				}

				*tk.u32++ = CONST;
				*tk.u64++ = v;
				continue;
			case '@':		// @ or octal constant
				if (*ln < '0' || *ln > '7')
				{
					*tk.u32++ = '@';
					continue;
				}

				v = 0;

				while (*ln >= '0' && *ln <= '7')
					v = (v << 3) + *ln++ - '0';

				if (*ln == '.')
				{
					if ((*(ln + 1) == 'b') || (*(ln + 1) == 'B'))
					{
						v &= 0x000000FF;
						ln += 2;
					}

					if ((*(ln + 1) == 'w') || (*(ln + 1) == 'W'))
					{
						v &= 0x0000FFFF;
						ln += 2;
					}

					if ((*(ln + 1) == 'l') || (*(ln + 1) == 'L'))
					{
						v &= 0xFFFFFFFF;
						ln += 2;
					}
				}

				*tk.u32++ = CONST;
				*tk.u64++ = v;
				continue;
			case '^':		// ^ or ^^ <operator-name>
				if (*ln != '^')
				{
					*tk.u32++ = '^';
					continue;
				}

				if (((int)chrtab[*++ln] & STSYM) == 0)
				{
					error("invalid symbol following ^^");
					continue;
				}

				p = ln++;

				while ((int)chrtab[*ln] & CTSYM)
					++ln;

				for(state=0; state>=0;)
				{
					// Get char, convert to lowercase
					j = (int)tolowertab[*p++];

					//if (j >= 'A' && j <= 'Z')
					//	j += 0x20;

					j += unarybase[state];

					if (unarycheck[j] != state)
					{
						j = -1;
						break;
					}

					if (*p == EOS || p == ln)
					{
						j = unaryaccept[j];
						break;
					}

					state = unarytab[j];
				}

				if (j < 0 || state < 0)
				{
					error("unknown symbol following ^^");
					continue;
				}

				*tk.u32++ = (TOKEN)j;
				continue;
			default:
				interror(2);	// Bad MULTX entry in chrtab
				continue;
			}
		}

		// Handle decimal constant
		if (c & DIGIT)
		{
			uint8_t * numStart = ln;
			v = 0;

			while ((int)chrtab[*ln] & DIGIT)
				v = (v * 10) + *ln++ - '0';

			// See if there's a .[bwl] after the constant & deal with it if so
			if (*ln == '.')
			{
				if ((*(ln + 1) == 'b') || (*(ln + 1) == 'B'))
				{
					v &= 0x000000FF;
					ln += 2;
					*tk.u32++ = CONST;
					*tk.u64++ = v;
					*tk.u32++ = DOTB;
				}
				else if ((*(ln + 1) == 'w') || (*(ln + 1) == 'W'))
				{
					v &= 0x0000FFFF;
					ln += 2;
					*tk.u32++ = CONST;
					*tk.u64++ = v;
					*tk.u32++ = DOTW;
				}
				else if ((*(ln + 1) == 'l') || (*(ln + 1) == 'L'))
				{
					v &= 0xFFFFFFFF;
					ln += 2;
					*tk.u32++ = CONST;
					*tk.u64++ = v;
					*tk.u32++ = DOTL;
				}
				else if ((int)chrtab[*(ln + 1)] & DIGIT)
				{
					// Hey, more digits after the dot, so we assume it's a
					// floating point number of some kind... numEnd will point
					// to the first non-float character after it's done
					char * numEnd;
					errno = 0;
					double f = strtod(numStart, &numEnd);
					ln = (uint8_t *)numEnd;

					if (errno != 0)
						return error("floating point parse error");

					// N.B.: We use the C compiler's internal double
					//       representation for all internal float calcs and
					//       are reasonably sure that the size of said double
					//       is 8 bytes long (which we check for in fltpoint.c)
					*tk.u32++ = FCONST;
					*tk.dp = f;
					tk.u64++;
					continue;
				}
			}
			else
			{
				*tk.u32++ = CONST;
				*tk.u64++ = v;
			}

//printf("CONST: %i\n", v);
			continue;
		}

		// Handle illegal character
		return error("illegal character $%02X found", *ln);
	}

	// Terminate line of tokens and return "success."

goteol:
	tok = etok;				// Set tok to beginning of line

	if (stuffnull)			// Terminate last SYMBOL
		*nullspot = EOS;

	*tk.u32++ = EOL;

	return OK;
}


//
// .GOTO <label>	goto directive
//
// The label is searched for starting from the first line of the current,
// enclosing macro definition. If no enclosing macro exists, an error is
// generated.
//
// A label is of the form:
//
// :<name><whitespace>
//
// The colon must appear in column 1.  The label is stripped prior to macro
// expansion, and is NOT subject to macro expansion.  The whitespace may also
// be EOL.
//
int d_goto(WORD unused)
{
	// Setup for the search
	if (*tok != SYMBOL)
		return error("missing label");

	char * sym = string[tok[1]];
	tok += 2;

	if (cur_inobj->in_type != SRC_IMACRO)
		return error("goto not in macro");

	IMACRO * imacro = cur_inobj->inobj.imacro;
	LLIST * defln = imacro->im_macro->lineList;

	// Attempt to find the label, starting with the first line.
	for(; defln!=NULL; defln=defln->next)
	{
		// Must start with a colon
		if (defln->line[0] == ':')
		{
			// Compare names (sleazo string compare)
			char * s1 = sym;
			char * s2 = defln->line + 1;

			// Either we will match the strings to EOS on both, or we will
			// match EOS on string 1 to whitespace on string 2. Otherwise, we
			// have no match.
			while ((*s1 == *s2) || ((*s1 == EOS) && (chrtab[*s2] & WHITE)))
			{
				// If we reached the end of string 1 (sym), we're done.
				// Note that we're also checking for the end of string 2 as
				// well, since we've established they're equal above.
				if (*s1 == EOS)
				{
					// Found the label, set new macro next-line and return.
					imacro->im_nextln = defln;
					return 0;
				}

				s1++;
				s2++;
			}
		}
	}

	return error("goto label not found");
}


void DumpToken(TOKEN t)
{
	if (t == COLON)
		printf("[COLON]");
	else if (t == CONST)
		printf("[CONST]");
	else if (t == FCONST)
		printf("[FCONST]");
	else if (t == ACONST)
		printf("[ACONST]");
	else if (t == STRING)
		printf("[STRING]");
	else if (t == SYMBOL)
		printf("[SYMBOL]");
	else if (t == EOS)
		printf("[EOS]");
	else if (t == TKEOF)
		printf("[TKEOF]");
	else if (t == DEQUALS)
		printf("[DEQUALS]");
	else if (t == SET)
		printf("[SET]");
	else if (t == REG)
		printf("[REG]");
	else if (t == DCOLON)
		printf("[DCOLON]");
	else if (t == GE)
		printf("[GE]");
	else if (t == LE)
		printf("[LE]");
	else if (t == NE)
		printf("[NE]");
	else if (t == SHR)
		printf("[SHR]");
	else if (t == SHL)
		printf("[SHL]");
	else if (t == UNMINUS)
		printf("[UNMINUS]");
	else if (t == DOTB)
		printf("[DOTB]");
	else if (t == DOTW)
		printf("[DOTW]");
	else if (t == DOTL)
		printf("[DOTL]");
	else if (t == DOTQ)
		printf("[DOTQ]");
	else if (t == DOTS)
		printf("[DOTS]");
	else if (t == DOTD)
		printf("[DOTD]");
	else if (t == DOTI)
		printf("[DOTI]");
	else if (t == ENDEXPR)
		printf("[ENDEXPR]");
	else if (t == CR_ABSCOUNT)
		printf("[CR_ABSCOUNT]");
	else if (t == CR_FILESIZE)
		printf("[CR_FILESIZE]");
	else if (t == CR_DEFINED)
		printf("[CR_DEFINED]");
	else if (t == CR_REFERENCED)
		printf("[CR_REFERENCED]");
	else if (t == CR_STREQ)
		printf("[CR_STREQ]");
	else if (t == CR_MACDEF)
		printf("[CR_MACDEF]");
	else if (t == CR_TIME)
		printf("[CR_TIME]");
	else if (t == CR_DATE)
		printf("[CR_DATE]");
	else if (t >= 0x20 && t <= 0x2F)
		printf("[%c]", (char)t);
	else if (t >= 0x3A && t <= 0x3F)
		printf("[%c]", (char)t);
	else if (t >= 0x80 && t <= 0x87)
		printf("[D%u]", ((uint32_t)t) - 0x80);
	else if (t >= 0x88 && t <= 0x8F)
		printf("[A%u]", ((uint32_t)t) - 0x88);
	else
		printf("[%X:%c]", (uint32_t)t, (char)t);
}


void DumpTokenBuffer(void)
{
	printf("Tokens [%X]: ", sloc);

	for(TOKEN * t=tokbuf; *t!=EOL; t++)
	{
		if (*t == COLON)
			printf("[COLON]");
		else if (*t == CONST)
		{
			PTR tp;
			tp.u32 = t + 1;
			printf("[CONST: $%llX]", *tp.u64);
			t += 2;
		}
		else if (*t == FCONST)
		{
			PTR tp;
			tp.u32 = t + 1;
			printf("[FCONST: $%llX]", *tp.u64);
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
		else if (*t == DOTQ)
			printf("[DOTQ]");
		else if (*t == DOTS)
			printf("[DOTS]");
		else if (*t == DOTD)
			printf("[DOTD]");
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
