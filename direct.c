//
// RMAC - Renamed Macro Assembler for all Atari computers
// DIRECT.C - Directive Handling
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "direct.h"
#include "6502.h"
#include "amode.h"
#include "dsp56k.h"
#include "error.h"
#include "expr.h"
#include "fltpoint.h"
#include "listing.h"
#include "mach.h"
#include "macro.h"
#include "mark.h"
#include "procln.h"
#include "riscasm.h"
#include "sect.h"
#include "symbol.h"
#include "token.h"

#define DEF_KW
#include "kwtab.h"
#define DEF_REG56
#define DECL_REG56
#include "56kregs.h"
#define DEF_REG68
#define DECL_REG68
#include "68kregs.h"
#define DEF_REGRISC
#define DECL_REGRISC
#include "riscregs.h"

TOKEN exprbuf[128];			// Expression buffer
SYM * symbolPtr[1000000];	// Symbol pointers table
static long unused;			// For supressing 'write' warnings
char buffer[256];			// Scratch buffer for messages
int largestAlign[3] = { 2, 2, 2 };	// Largest alignment value seen per section

// Function prototypes
int d_unimpl(void);
int d_68000(void);
int d_68020(void);
int d_68030(void);
int d_68040(void);
int d_68060(void);
int d_68881(void);
int d_68882(void);
int d_56001(void);
int d_nofpu(void);
int d_bss(void);
int d_data(void);
int d_text(void);
int d_abs(void);
int d_comm(void);
int d_dc(WORD);
int d_ds(WORD);
int d_dsm(WORD);
int d_dcb(WORD);
int d_globl(void);
int d_gpu(void);
int d_dsp(void);
int d_assert(void);
int d_include(void);
int d_list(void);
int d_nlist(void);
int d_error(char *);
int d_warn(char *);
int d_org(void);
int d_init(WORD);
int d_cargs(void);
int d_undmac(void);
int d_regbank0(void);
int d_regbank1(void);
int d_incbin(void);
int d_noclear(void);
int d_equrundef(void);
int d_ccundef(void);
int d_print(void);
int d_gpumain(void);
int d_jpad(void);
int d_nojpad(void);
int d_fail(void);
int d_cstruct(void);
int d_prgflags(void);
int d_opt(void);
int d_dsp(void);
int d_objproc(void);
int d_align(void);
void SetLargestAlignment(int);

// Directive handler table
int (*dirtab[])() = {
	d_org,				// 0 org
	d_even,				// 1 even
	d_6502,				// 2 .6502
	d_68000,			// 3 .68000
	d_bss,				// 4 bss
	d_data,				// 5 data
	d_text,				// 6 text
	d_abs,				// 7 abs
	d_comm,				// 8 comm
	(void *)d_init,		// 9 init
	d_cargs,			// 10 cargs
	(void *)d_goto,		// 11 goto
	(void *)d_dc,		// 12 dc
	(void *)d_ds,		// 13 ds
	d_undmac,			// 14 undefmac
	d_gpu,				// 15 .gpu
	d_dsp,				// 16 .dsp
	(void *)d_dcb,		// 17 dcb
	d_unimpl,			// 18* set
	d_unimpl,			// 19* reg
	d_unimpl,			// 20 dump
	d_incbin,			// 21 .incbin //load
	d_unimpl,			// 22 disable
	d_unimpl,			// 23 enable
	d_globl,			// 24 globl
	d_regbank0,			// 25 .regbank0
	d_regbank1,			// 26 .regbank1
	d_unimpl,			// 27 xdef
	d_assert,			// 28 assert
	d_unimpl,			// 29* if
	d_unimpl,			// 30* endif
	d_unimpl,			// 31* endc
	d_unimpl,			// 32* iif
	d_include,			// 33 include
	fpop,				// 34 end
	d_unimpl,			// 35* macro
	ExitMacro,			// 36* exitm
	d_unimpl,			// 37* endm
	d_list,				// 38 list
	d_nlist,			// 39 nlist
	d_long,				// 40* rept
	d_phrase,			// 41* endr
	d_dphrase,			// 42 struct
	d_qphrase,			// 43 ends
	d_title,			// 44 title
	d_subttl,			// 45 subttl
	eject,				// 46 eject
	d_error,			// 47 error
	d_warn,				// 48 warn
	d_noclear,			// 49 .noclear
	d_equrundef,		// 50 .equrundef/.regundef
	d_ccundef,			// 51 .ccundef
	d_print,			// 52 .print
	d_cstruct,			// 53 .cstruct
	d_jpad,				// 54 .jpad (deprecated)
	d_nojpad,			// 55 .nojpad (deprecated)
	d_gpumain,			// 56 .gpumain (deprecated)
	d_prgflags,			// 57 .prgflags
	d_68020,			// 58 .68020
	d_68030,			// 59 .68030
	d_68040,			// 60 .68040
	d_68060,			// 61 .68060
	d_68881,			// 62 .68881
	d_68882,			// 63 .68882
	d_56001,			// 64 .56001
	d_nofpu,			// 65 nofpu
	d_opt,				// 66 .opt
	d_objproc,			// 67 .objproc
	(void *)d_dsm,			// 68 .dsm
	d_align				// 69 .align
};


//
// Set the largest alignment seen in the current section
//
void SetLargestAlignment(int size)
{
	if ((scattr & TEXT) && (largestAlign[0] < size))
		largestAlign[0] = size;
	else if ((scattr & DATA) && (largestAlign[1] < size))
		largestAlign[1] = size;
	else if ((scattr & BSS) && (largestAlign[2] < size))
		largestAlign[2] = size;
}


//
// .error - Abort compilation, printing an error message
//
int d_error(char *str)
{
	if (*tok == EOL)
		return error("error directive encountered - aborting assembling");
	else
	{
		switch(*tok)
		{
		case STRING:
			return error(string[tok[1]]);
			break;
		default:
			return error("error directive encountered--aborting assembly");
		}
	}
}


//
// .warn - Just display a warning on screen
//
int d_warn(char *str)
{
	if (*tok == EOL)
		return warn("WARNING WARNING WARNING");
	else
	{
		switch(*tok)
		{
		case STRING:
			return warn(string[tok[1]]);
			break;
		default:
			return warn("WARNING WARNING WARNING");
		}
	}
}


//
// .org - Set origin
//
int d_org(void)
{
	uint64_t address;

	if (!rgpu && !rdsp && !robjproc && !m6502 && !dsp56001 && !(obj_format == RAW))
		return error(".org permitted only in GPU/DSP/OP, 56001, 6502 and 68k (with -fr switch) sections");

	// M56K can leave the expression off the org for some reason :-/
	// (It's because the expression is non-standard, and so we have to look at
	// it in isolation)
	if (!dsp56001 && (abs_expr(&address) == ERROR))
	{
		error("cannot determine org'd address");
		return ERROR;
	}

	if (rgpu | rdsp | robjproc)
	{
		orgaddr = address;
		orgactive = 1;
	}
	else if (m6502)
	{
		// 6502.  We also kludge 'lsloc' so the listing generator doesn't try
		// to spew out megabytes.
		if (address > 0xFFFF)
			return error(range_error);

		if (sloc != currentorg[0])
		{
			currentorg[1] = sloc;
			currentorg += 2;
		}

		currentorg[0] = address;
		ch_size = 0;
		lsloc = sloc = address;
		chptr = scode->chptr + address;
		orgaddr = address;
		orgactive = 1;
	}
	else if (dsp56001)
	{
		// Only mark segments we actually wrote something
		if (chptr != dsp_currentorg->start && dsp_written_data_in_current_org)
		{
			dsp_currentorg->end = chptr;
			dsp_currentorg++;
		}

		// Maybe we switched from a non-DSP section (TEXT, DATA, etc) and
		// scode isn't initialised yet. Not that it's going to be a valid
		// scenario, but if we try it anyhow it's going to lead to a crash. So
		// let's fudge a value of 0 and get on with it.
		orgaddr = (scode != NULL ? sloc : 0);
		SaveSection();

		if (tok[1] != ':')
			return error(syntax_error);

		int sectionToSwitch = 0;

		switch (tok[0])
		{
		case REG56_X:
			dsp_currentorg->memtype = ORG_X;
			sectionToSwitch = M56001X;
			break;

		case REG56_Y:
			dsp_currentorg->memtype = ORG_Y;
			sectionToSwitch = M56001Y;
			break;

		case REG56_P:
			dsp_currentorg->memtype = ORG_P;
			sectionToSwitch = M56001P;
			break;

		case REG56_L:
			dsp_currentorg->memtype = ORG_L;
			sectionToSwitch = M56001L;
			break;

		default:
			return error("unknown type in ORG");
		}

		if ((obj_format == LOD) || (obj_format == P56))
			SwitchSection(sectionToSwitch);

		tok += 2;
		chcheck(3); // Ensure we got a valid address to write
		dsp_currentorg->chunk = scode;  // Mark down which chunk this org starts from (will be needed when outputting)

		if (*tok == EOL)
		{
			// Well, the user didn't specify an address at all so we'll have to
			// use the last used address of that section (or 0 if there wasn't one)
			address = orgaddr;
			dsp_currentorg->start = chptr;
			dsp_currentorg->orgadr = orgaddr;
		}
		else
		{
			if (abs_expr(&address) == ERROR)
			{
				error("cannot determine org'd address");
				return ERROR;
			}

			dsp_currentorg->start = chptr;
			dsp_currentorg->orgadr = (uint32_t)address;
			sect[cursect].orgaddr = (uint32_t)address;
		}

		if (address > DSP_MAX_RAM)
		{
			return error(range_error);
		}

		dsp_written_data_in_current_org = 0;

		// Copied from 6502 above: kludge `lsloc' so the listing generator
		// doesn't try to spew out megabytes.
		lsloc = sloc = (int32_t)address;
// N.B.: It seems that by enabling this, even though it works elsewhere, will cause symbols to royally fuck up.  Will have to do some digging to figure out why.
//		orgactive = 1;
	}
	else
	{
		// If we get here we assume it's 68k with RAW output, so this is allowed
		if (orgactive)
		{
			return error("In 68k mode only one .org statement is allowed");
		}

		org68k_address = address;
		org68k_active = 1;
	}

	ErrorIfNotAtEOL();
	return 0;
}


//
// Print directive
//
int d_print(void)
{
	char prntstr[LNSIZ];		// String for PRINT directive
	char format[LNSIZ];			// Format for PRINT directive
	int formatting = 0;			// Formatting on/off
	int wordlong = 0;			// WORD = 0, LONG = 1
	int outtype = 0;			// 0:hex, 1:decimal, 2:unsigned

	uint64_t eval;				// Expression value
	WORD eattr;					// Expression attributes
	SYM * esym;					// External symbol involved in expr.
	TOKEN r_expr[EXPRSIZE];

	while (*tok != EOL)
	{
		switch (*tok)
		{
		case STRING:
			sprintf(prntstr, "%s", string[tok[1]]);
			printf("%s", prntstr);

			if (list_fd)
				unused = write(list_fd, prntstr, (LONG)strlen(prntstr));

			tok += 2;
			break;
		case '/':
			formatting = 1;

			// "X" & "L" get tokenized now... :-/ Probably should look into preventing this kind of thing from happening (was added with DSP56K code)
			// Note (ggn): This is now much less severe as it's localised for 56k only
			if ((tok[1] != SYMBOL) && (tok[1] != REG56_L) && (tok[1] != REG56_X))
				goto token_err;

			if (tok[1] == REG56_L)
			{
				wordlong = 1;
				tok += 2;
			}
			else if (tok[1] == REG56_X)
			{
				outtype = 0;
				tok += 2;
			}
			else
			{
				strcpy(prntstr, string[tok[2]]);

				switch (prntstr[0])
				{
				case 'l': case 'L': wordlong = 1; break;
				case 'w': case 'W': wordlong = 0; break;
				case 'x': case 'X': outtype  = 0; break;
				case 'd': case 'D': outtype  = 1; break;
				case 'u': case 'U': outtype  = 2; break;
				default:
					error("unknown print format flag");
					return ERROR;
				}

				tok += 3;
			}

			break;
		case ',':
			tok++;
			break;
		default:
			if (expr(r_expr, &eval, &eattr, &esym) != OK)
				goto token_err;
			else
			{
				switch(outtype)
				{
				case 0: strcpy(format, "%X"); break;
				case 1: strcpy(format, "%d" ); break;
				case 2: strcpy(format, "%u" ); break;
				}

				if (wordlong)
					sprintf(prntstr, format, eval);
				else
					sprintf(prntstr, format, eval & 0xFFFF);

				printf("%s", prntstr);

				if (list_fd)
					unused = write(list_fd, prntstr, (LONG)strlen(prntstr));

				formatting = 0;
				wordlong = 0;
				outtype = 0;
			}

			break;
		}
	}

	printf("\n");

	return 0;

token_err:
	error("illegal print token [@ '%s']", prntstr);
	return ERROR;
}


//
// Undefine an equated condition code
//
int d_ccundef(void)
{
	SYM * ccname;

	// Check that we are in a RISC section
	if (!rgpu && !rdsp)
	{
		error(".ccundef must be defined in .gpu/.dsp section");
		return ERROR;
	}

	if (*tok != SYMBOL)
	{
		error("syntax error; expected symbol");
		return ERROR;
	}

	ccname = lookup(string[tok[1]], LABEL, 0);

	// Make sure symbol is a valid ccdef
	if (!ccname || !(ccname->sattre & EQUATEDCC))
	{
		error("invalid equated condition name specified");
		return ERROR;
	}

	ccname->sattre |= UNDEF_CC;

	return 0;
}


//
// Undefine an equated register
//
int d_equrundef(void)
{
	SYM * regname;

	// Check that we are in a RISC section
	if (!rgpu && !rdsp)
		return error(".equrundef/.regundef must be defined in .gpu/.dsp section");

	while (*tok != EOL)
	{
		// Skip preceeding or seperating commas (if any)
		if (*tok == ',')
			tok++;

		// Check we are dealing with a symbol
		if (*tok != SYMBOL)
			return error("syntax error; expected symbol");

		// Lookup and undef if equated register
		regname = lookup(string[tok[1]], LABEL, 0);

		if (regname && (regname->sattre & EQUATEDREG))
		{
			// Reset the attributes of this symbol...
			regname->sattr = 0;
			regname->sattre &= ~EQUATEDREG;
			regname->sattre |= UNDEF_EQUR;
		}

		// Skip over symbol token and address
		tok += 2;
	}

	return 0;
}


//
// Do not allow use of the CLR.L opcode
//
int d_noclear(void)
{
	warn("CLR.L opcode ignored...");
	return 0;
}


//
// Include binary file (can add addition size & position params, comma separated)
//
int d_incbin(void)
{
	int fd;
	int bytes = 0;
	uint64_t pos, size, bytesRead;
	char buf1[256];
	int i;

	// Check to see if we're in BSS, and, if so, throw an error
	if (scattr & SBSS)
	{
		error("cannot include binary file \"%s\" in BSS section", string[tok[1]]);
		return ERROR;
	}

	if (*tok != STRING)
	{
		error("syntax error; file to include missing");
		return ERROR;
	}

	// Attempt to open the include file in the current directory, then (if that
	// failed) try list of include files passed in the enviroment string or by
	// the "-i" option.
	TOKEN filename = tok[1];

	if ((fd = open(string[filename], _OPEN_INC)) < 0)
	{
		for(i=0; nthpath("RMACPATH", i, buf1)!=0; i++)
		{
			fd = strlen(buf1);

			// Append path char if necessary
			if (fd > 0 && buf1[fd - 1] != SLASHCHAR)
				strcat(buf1, SLASHSTRING);

			strcat(buf1, string[filename]);

			if ((fd = open(buf1, _OPEN_INC)) >= 0)
				goto allright;
		}

		return error("cannot open: \"%s\"", string[filename]);
	}

allright:

	tok += 2;

	size = lseek(fd, 0L, SEEK_END);
	pos = lseek(fd, 0L, SEEK_SET);

	if (*tok != EOL)
	{
		// Parse size and position parameters
		uint64_t requested_size = -1;	// -1 means "not set" for these two

		if (*tok++ != ',')
		{
			close(fd);
			return error("expected comma after incbin filename");
		}

		if (*tok != EOL)
		{
			if (*tok != ',')
			{
				if (abs_expr(&requested_size) != OK)
				{
					close(fd);
					return ERROR;
				}

				if ((int64_t)requested_size <= 0 || requested_size > size)
				{
					close(fd);
					return error("invalid incbin size requested");
				}
			}

		if (*tok != EOL)
		{
				if (*tok++ != ',')
			{
					close(fd);
					return error("expected comma after size parameter");
				}

				if (*tok != EOL)
				{
					if (abs_expr(&pos) != OK)
					{
						close(fd);
						return ERROR;
					}

					if ((int64_t)pos <= 0 || pos > size)
					{
						close(fd);
						return error("invalid incbin position requested");
					}
					}
				}

			if (*tok != EOL)
				{
				close(fd);
				return error("extra characters following incbin");
				}
			}

		// Adjust size if the user didn't specify it via the parameter
		if (requested_size == -1)
		{
			requested_size = size - pos;
		}

		// Are we going to read past the end of the file?
		if (pos + requested_size > size)
		{
			close(fd);
			return error("invalid combination of incbin position and size");
	}
		size = requested_size;

		// All checks passed, let's seek to where the user requested, otherwise at file start
		lseek(fd, pos, SEEK_SET);
	}

	chcheck(size);

	DEBUG { printf("INCBIN: File '%s' is %lli bytes.\n", string[filename], size); }

	char * fileBuffer = (char *)malloc(size);
	bytesRead = read(fd, fileBuffer, size);

	if (bytesRead != size)
	{
		error("was only able to read %li bytes from binary file (%s, %li bytes)", bytesRead, string[filename], size);
		return ERROR;
	}

	memcpy(chptr, fileBuffer, size);
	chptr += size;
	sloc += size;
	ch_size += size;

	if (orgactive)
		orgaddr += size;

	free(fileBuffer);
	close(fd);
	return 0;
}


//
// Set RISC register banks
//
int d_regbank0(void)
{
	// Deprecated, it's not as if this did anything useful, ever
	warn("regbank0 ignored");
	return 0;
}


int d_regbank1(void)
{
	// Deprecated, it's not as if this did anything useful, ever
	warn("regbank1 ignored");
	return 0;
}


//
// Helper function, to cut down on mistakes & typing
//
static inline void SkipBytes(unsigned bytesToSkip)
{
	if (!bytesToSkip)
		return;

	if ((scattr & SBSS) == 0)
	{
		chcheck(bytesToSkip);
		D_ZEROFILL(bytesToSkip);
	}
	else
	{
		sloc += bytesToSkip;

		if (orgactive)
			orgaddr += bytesToSkip;
	}
}


//
// Adjust location to an EVEN value
//
int d_even(void)
{
	if (m6502)
		return error(in_6502mode);

	unsigned skip = (rgpu || rdsp ? orgaddr : sloc) & 0x01;

	if (skip)
	{
		if ((scattr & SBSS) == 0)
		{
			chcheck(1);
			D_byte(0);
		}
		else
		{
			sloc++;

			if (orgactive)
				orgaddr++;
		}
	}

	return 0;
}


//
// Adjust location to a LONG value
//
int d_long(void)
{
	unsigned lower2Bits = (rgpu || rdsp ? orgaddr : sloc) & 0x03;
	unsigned bytesToSkip = (0x04 - lower2Bits) & 0x03;
	SkipBytes(bytesToSkip);
	SetLargestAlignment(4);

	return 0;
}


//
// Adjust location to a PHRASE value
//
// N.B.: We have to handle the GPU/DSP cases separately because you can embed
//       RISC code in the middle of a regular 68K section. Also note that all
//       of the alignment pseudo-ops will have to be fixed this way.
//
// This *must* behave differently when in a RISC section, as following sloc
// (instead of orgaddr) will fuck things up royally. Note that we do it this
// way because you can embed RISC code in a 68K section, and have the origin
// pointing to a different alignment in the RISC section than the 68K section.
//
int d_phrase(void)
{
	unsigned lower3Bits = (rgpu || rdsp ? orgaddr : sloc) & 0x07;
	unsigned bytesToSkip = (0x08 - lower3Bits) & 0x07;
	SkipBytes(bytesToSkip);
	SetLargestAlignment(8);

	return 0;
}


//
// Adjust location to a DPHRASE value
//
int d_dphrase(void)
{
	unsigned lower4Bits = (rgpu || rdsp ? orgaddr : sloc) & 0x0F;
	unsigned bytesToSkip = (0x10 - lower4Bits) & 0x0F;
	SkipBytes(bytesToSkip);
	SetLargestAlignment(16);

	return 0;
}


//
// Adjust location to a QPHRASE value
//
int d_qphrase(void)
{
	unsigned lower5Bits = (rgpu || rdsp ? orgaddr : sloc) & 0x1F;
	unsigned bytesToSkip = (0x20 - lower5Bits) & 0x1F;
	SkipBytes(bytesToSkip);
	SetLargestAlignment(32);

	return 0;
}


//
// Adjust location to <alignment> bytes
//
int d_align(void)
{
	unsigned bytesToSkip;
	uint64_t eval;

	if (abs_expr(&eval) != OK)
		return 0;

	if (eval < 2)
	{
		return error("Invalid .align value specified");
	}

	if (dsp56001)
	{
		bytesToSkip = eval - sloc % eval;
		D_ZEROFILL(bytesToSkip*3);
		return 0;
	}

	bytesToSkip = eval - (rgpu || rdsp ? orgaddr : sloc) % eval;
	if ( bytesToSkip != eval )
	{
		if ((scattr & SBSS) == 0)
		{
			D_ZEROFILL(bytesToSkip);
		}
		else
		{
			sloc += bytesToSkip;

		if (orgactive)
			orgaddr += bytesToSkip;
		}
	}
	return 0;
}


//
// Do auto-even.  This must be called ONLY if 'sloc' is odd.
//
// This is made hairy because, if there was a label on the line, we also have
// to adjust its value. This won't work with more than one label on the line,
// which is OK since multiple labels are only allowed in AS68 kludge mode, and
// the C compiler is VERY paranoid and uses ".even" whenever it can
//
// N.B.: This probably needs the same fixes as above...
//
void auto_even(void)
{
	if (cursect != M6502)
	{
		if (scattr & SBSS)
			sloc++;				// Bump BSS section
		else
			D_byte(0);			// Deposit 0.b in non-BSS

		if (lab_sym != NULL)	// Bump label if we have to
			lab_sym->svalue++;
	}
}


//
// Unimplemened directive error
//
int d_unimpl(void)
{
	return error("unimplemented directive");
}


//
// Return absolute (not TDB) and defined expression or return an error
//
int abs_expr(uint64_t * a_eval)
{
	WORD eattr;

	if (expr(exprbuf, a_eval, &eattr, NULL) < 0)
		return ERROR;

	if (!(eattr & DEFINED))
		return error(undef_error);

	if (eattr & TDB)
		return error(rel_error);

	return OK;
}


//
// Hand symbols in a symbol-list to a function (kind of like mapcar...)
//
int symlist(int(* func)())
{
	const char * em = "symbol list syntax";

	for(;;)
	{
		if (*tok != SYMBOL)
			return error(em);

		if ((*func)(string[tok[1]]) != OK)
			break;

		tok += 2;

		if (*tok == EOL)
			break;

		if (*tok != ',')
			return error(em);

		tok++;
	}

	return 0;
}


//
// .include "filename"
//
int d_include(void)
{
	int j;
	int i;
	char * fn;
	char buf[128];
	char buf1[128];

	if (*tok == STRING)			// Leave strings ALONE
		fn = string[*++tok];
	else if (*tok == SYMBOL)	// Try to append ".s" to symbols
	{
		strcpy(buf, string[*++tok]);
		fext(buf, ".s", 0);
		fn = &buf[0];
	}
	else						// Punt if no STRING or SYMBOL
		return error("missing filename");

	// Make sure the user didn't try anything like:
	// .include equates.s
	if (*++tok != EOL)
		return error("extra stuff after filename--enclose it in quotes");

	// Attempt to open the include file in the current directory, then (if that
	// failed) try list of include files passed in the enviroment string or by
	// the "-i" option.
	if ((j = open(fn, 0)) < 0)
	{
		for(i=0; nthpath("RMACPATH", i, buf1)!=0; i++)
		{
			j = strlen(buf1);

			// Append path char if necessary
			if (j > 0 && buf1[j - 1] != SLASHCHAR)
				strcat(buf1, SLASHSTRING);

			strcat(buf1, fn);

			if ((j = open(buf1, 0)) >= 0)
				goto allright;
		}

		return error("cannot open: \"%s\"", fn);
	}

allright:
	include(j, fn);
	return 0;
}


//
// .assert expression [, expression...]
//
int d_assert(void)
{
	WORD eattr;
	uint64_t eval;

	for(; expr(exprbuf, &eval, &eattr, NULL)==OK; ++tok)
	{
		if (!(eattr & DEFINED))
			return error("forward or undefined .assert");

		if (!eval)
			return error("assert failure");

		if (*tok != ',')
			break;
	}

	ErrorIfNotAtEOL();
	return 0;
}


//
// .globl symbol [, symbol] <<<cannot make local symbols global>>>
//
int globl1(char * p)
{
	SYM * sy;

	if (*p == '.')
		return error("cannot .globl local symbol");

	if ((sy = lookup(p, LABEL, 0)) == NULL)
	{
		sy = NewSymbol(p, LABEL, 0);
		sy->svalue = 0;
		sy->sattr = GLOBAL;
//printf("glob1: Making global symbol: attr=%04X, eattr=%08X, %s\n", sy->sattr, sy->sattre, sy->sname);
	}
	else
		sy->sattr |= GLOBAL;

	return OK;
}


int d_globl(void)
{
	if (m6502)
		return error(in_6502mode);

	symlist(globl1);
	return 0;
}


//
// .prgflags expression
//
int d_prgflags(void)
{
	uint64_t eval;

	if (*tok == EOL)
		return error("PRGFLAGS requires value");
	else if (abs_expr(&eval) == OK)
	{
		PRGFLAGS = (uint32_t)eval;
		return 0;
	}
	else
	{
		return error("PRGFLAGS requires value");
	}
}


//
// .abs [expression]
//
int d_abs(void)
{
	uint64_t eval;

	if (m6502)
		return error(in_6502mode);

	SaveSection();

	if (*tok == EOL)
		eval = 0;
	else if (abs_expr(&eval) != OK)
		return 0;

	SwitchSection(ABS);
	sloc = (uint32_t)eval;
	return 0;
}


//
// Switch segments
//
int d_text(void)
{
	if (rgpu || rdsp)
		return error("directive forbidden in gpu/dsp mode");
	else if (m6502)
		return error(in_6502mode);

	if (cursect != TEXT)
	{
		SaveSection();
		SwitchSection(TEXT);
	}

	return 0;
}


int d_data(void)
{
	if (rgpu || rdsp)
		return error("directive forbidden in gpu/dsp mode");
	else if (m6502)
		return error(in_6502mode);

	if (cursect != DATA)
	{
		SaveSection();
		SwitchSection(DATA);
	}

	return 0;
}


int d_bss(void)
{
	if (rgpu || rdsp)
		return error("directive forbidden in gpu/dsp mode");
	else if (m6502)
		return error(in_6502mode);

	if (cursect != BSS)
	{
		SaveSection();
		SwitchSection(BSS);
	}

	return 0;
}


//
// .ds[.size] expression
//
int d_ds(WORD siz)
{
	DEBUG { printf("Directive: .ds.[size] = %u, sloc = $%X\n", siz, sloc); }

	uint64_t eval;
    WORD eattr;

	if ((cursect & (M6502 | M56KPXYL)) == 0)
	{
		if ((siz != SIZB) && (sloc & 1))	// Automatic .even
			auto_even();
	}

	if (expr(exprbuf, &eval, &eattr, NULL) < 0)
		return ERROR;

	// Check to see if the value being passed in is negative (who the hell does
	// that?--nobody does; it's the code gremlins, or rum, what does it)
	// N.B.: Since 'eval' is of type uint64_t, if it goes negative, it will
	//       have its high bit set.
	if (eval & 0x8000000000000000)
		return error("negative sizes not allowed in DS");

	// In non-TDB section (BSS, ABS and M6502) just advance the location
	// counter appropriately. In TDB sections, deposit (possibly large) chunks
	// of zeroed memory....
	if ((scattr & SBSS) || cursect == M6502)
	{
		listvalue((uint32_t)eval);
		eval *= siz;
		sloc += (uint32_t)eval;

		if (cursect == M6502)
			chptr += eval;

		just_bss = 1;					// No data deposited (8-bit CPU mode)
	}
	else if (cursect & M56KPXYL)
	{
		// Change segment instead of marking blanks.
		// Only mark segments we actually wrote something
		if (chptr != dsp_currentorg->start && dsp_written_data_in_current_org)
		{
			dsp_currentorg->end = chptr;
			dsp_currentorg++;
			dsp_currentorg->memtype = dsp_currentorg[-1].memtype;
		}

		listvalue((uint32_t)eval);
		sloc += (uint32_t)eval;

		// And now let's create a new segment
		dsp_currentorg->start = chptr;
		dsp_currentorg->chunk = scode;  // Mark down which chunk this org starts from (will be needed when outputting)
		sect[cursect].orgaddr = sloc;
		dsp_currentorg->orgadr = sloc;
		dsp_written_data_in_current_org = 0;

		just_bss = 1;					// No data deposited
	}
	else
	{
		dep_block(eval, siz, 0, (DEFINED | ABS), NULL);
	}

	ErrorIfNotAtEOL();
	return OK;
}


//
// dsm[.siz] expression
// Define modulo storage
// Quoting the Motorola assembler manual:
// "The DSM directive reserves a block of memory the length of which in words is equal to
// the value of <expression>.If the runtime location counter is not zero, this directive first
// advances the runtime location counter to a base address that is a multiple of 2k, where
// 2k >= <expression>."
// The kicker of course is written a few sentences after:
// "<label>, if present, will be assigned the value of the runtime location counter after a valid
// base address has been established."
//
int d_dsm(WORD siz)
{
	TOKEN * tok_current = tok;  // Keep track of where tok was when we entered this procedure
	uint64_t eval;

	if (abs_expr(&eval) != OK)
 		return 0;

	// Round up to the next highest power of 2
	// Nicked from https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	eval--;
	eval |= eval >> 1;
	eval |= eval >> 2;
	eval |= eval >> 4;
	eval |= eval >> 8;
	eval |= eval >> 16;

	int units_to_skip;
	units_to_skip = eval + 1 - sloc;
	sloc += units_to_skip;		// Bump up sloc - TODO: check if this goes over the RAM limits?

	// If a label has been defined in the same line as dsm, its value also needs to be adjusted
	if (label_defined)
	{
		SYM * label = lookup(label_defined, LABEL, 0);
		label->svalue += units_to_skip;
	}

	tok = tok_current;		// Rewind tok back to where it was
	return d_ds(siz);		// And let d_ds take over from here
}


//
// dc.b, dc.w / dc, dc.l, dc.i, dc.q, dc.d, dc.s, dc.x
//
int d_dc(WORD siz)
{
	WORD eattr;
	uint64_t eval;
	uint8_t * p;

	if ((scattr & SBSS) != 0)
		return error("illegal initialization of section");

	// Do an auto_even if it's not BYTE sized (hmm, should we be doing this???)
	if ((cursect != M6502) && (cursect != M56001P) && (cursect != M56001X)
		&& (cursect != M56001Y) && (cursect != M56001L)
		&& (siz != SIZB) && (sloc & 1))
		auto_even();

	// Check to see if we're trying to set LONGS on a non 32-bit aligned
	// address in a GPU or DSP section, in their local RAM
	if ((siz == SIZL) && (orgaddr & 0x03)
		&& ((rgpu && (orgaddr >= 0xF03000) && (orgaddr <= 0xF03FFFF))
		|| (rdsp && (orgaddr >= 0xF1B000) && (orgaddr <= 0xF1CFFFF))))
		warn("depositing LONGs on a non-long address in local RAM");

	for(;; tok++)
	{
		// dc.b 'string' [,] ...
		if (siz == SIZB && (*tok == STRING || *tok == STRINGA8) && (tok[2] == ',' || tok[2] == EOL))
		{
			uint32_t i = strlen(string[tok[1]]);

			if ((challoc - ch_size) < i)
				chcheck(i);

			if (*tok == STRING)
			{
				for(p=string[tok[1]]; *p!=EOS; p++)
					D_byte(*p);
			}
			else if (*tok == STRINGA8)
			{
				for(p=string[tok[1]]; *p!=EOS; p++)
					D_byte(strtoa8[*p]);
			}
			else
			{
				error("String format not supported... yet");
			}

			tok += 2;
			goto comma;
		}

		int movei = 0; // MOVEI flag for dc.i

		if (*tok == DOTI)
		{
			movei = 1;
			tok++;
			siz = SIZL;
		}

		// dc.x <expression>
		SYM * esym = 0;

		if (expr(exprbuf, &eval, &eattr, &esym) != OK)
			return 0;

		uint16_t tdb = eattr & TDB;
		uint16_t defined = eattr & DEFINED;

// N.B.: This is awful.  This needs better handling, rather than just bodging something in that, while works, is basically an ugly wart on the assembler.  !!! FIX !!!
		if (dsp56001)
		{
			if (cursect != M56001L)
			{
				if (!defined)
				{
					AddFixup(FU_DSPIMM24 | FU_SEXT, sloc, exprbuf);
					D_dsp(0);
				}
				else
				{
					if (eattr & FLOAT)
					{
						double fval = *(double *)&eval;
						eval = DoubleToDSPFloat(fval);
					}
					else
					{
						if ((uint32_t)eval + 0x1000000 >= 0x2000000)
							return error(range_error);
					}

					// Deposit DSP word (24-bit)
					D_dsp(eval);
				}
			}
			else
			{
				// In L: we deposit stuff to both X: and Y: instead
				// We will be a bit lazy and require that there is a 2nd value
				// in the same source line. (Motorola's assembler can parse
				// 12-digit hex values, which we can't do at the moment) This
				// of course requires to parse 2 values in one pass. If there
				// isn't another value in this line, assume X: value is 0.
				int secondword = 0;
				uint32_t evaly;
l_parse_loop:

				if (!defined)
				{
					AddFixup(FU_DSPIMM24 | FU_SEXT, sloc, exprbuf);
					D_dsp(0);
				}
				else
				{
					if (eattr & FLOAT)
					{
						float fval = *(float *)&eval;
						eval = DoubleToDSPFloat(fval);
					}
					else
					{
						if (eval + 0x1000000 >= 0x2000000)
							return error(range_error);
					}

					// Parse 2nd value if we didn't do this yet
					if (secondword == 0)
					{
						evaly = (uint32_t)eval;
						secondword = 1;

						if (*tok != ':')
						{
							// If we don't have a : then we're probably at EOL,
							// which means the X: value will be 0
							eval = 0;
							ErrorIfNotAtEOL();
						}
						else
						{
							tok++; // Eat the comma;

							if (expr(exprbuf, &eval, &eattr, NULL) != OK)
								return 0;

							defined = (WORD)(eattr & DEFINED);
							goto l_parse_loop;
						}
					}

					// Deposit DSP words (24-bit)
					D_dsp(eval);
					D_dsp(evaly);
					sloc--; // We do write 2 DSP words but as far as L: space is concerned we actually advance our counter by one
				}

			}

			goto comma;
		}

		switch (siz)
		{
		case SIZB:
			if (!defined)
			{
				AddFixup(FU_BYTE | FU_SEXT, sloc, exprbuf);
				D_byte(0);
			}
			else
			{
				if (tdb)
					return error("non-absolute byte value");

				if (eval + 0x100 >= 0x200)
					return error("%s (value = $%X)", range_error, eval);

				D_byte(eval);
			}

			break;

		case SIZW:
		case SIZN:
			if (!defined)
			{
				AddFixup(FU_WORD | FU_SEXT, sloc, exprbuf);
				D_word(0);
			}
			else
			{
				if (eval + 0x10000 >= 0x20000)
					return error(range_error);

				if (tdb)
					MarkRelocatable(cursect, sloc, tdb, MWORD, NULL);

				// Deposit 68000 or 6502 (byte-reversed) word
				if (cursect != M6502)
					D_word(eval)
				else
					D_rword(eval)
			}

			break;

		case SIZL:
			// Shamus: Why can't we do longs in 6502 mode?
			if (m6502)
				return error(in_6502mode);

			if (!defined)
			{
				AddFixup(FU_LONG | (movei ? FU_MOVEI : 0), sloc, exprbuf);
				D_long(0);
			}
			else
			{
				if (tdb)
					MarkRelocatable(cursect, sloc, tdb, MLONG, NULL);

				if (movei)
					eval = WORDSWAP32(eval);

				D_long(eval);
			}

			break;

		case SIZQ:
			// 64-bit size
			if (m6502)
				return error(in_6502mode);

			// DEFINITELY NEED FIXUPS HERE!
			if (!defined)
			{
				AddFixup(FU_QUAD, sloc, exprbuf);
				eval = 0;
			}

			D_quad(eval);
			break;

		case SIZS:
			// 32-bit float size
			if (m6502)
				return error(in_6502mode);

/* Seems to me that if something is undefined here, then that should be an error.  Likewise for the D & X variants. */
			if (!defined)
			{
//				AddFixup(FU_FLOATSING, sloc, exprbuf);
//				D_long(0);
				return error("labels not allowed in floating point expressions");
			}
			else
			{
//Would this *ever* happen?
//				if (tdb)
//					MarkRelocatable(cursect, sloc, tdb, MSINGLE, NULL);

				PTR ptr;
				ptr.u64 = &eval;
				uint32_t ieee754 = FloatToIEEE754((float)*ptr.dp);
				D_long(ieee754);
			}

			break;

		case SIZD:
			// 64-bit double size
			if (m6502)
				return error(in_6502mode);

			if (!defined)
			{
//				AddFixup(FU_FLOATDOUB, sloc, exprbuf);
//				D_quad(0LL);
				return error("labels not allowed in floating point expressions");
			}
			else
			{
//Would this *ever* happen?
//				if (tdb)
//					MarkRelocatable(cursect, sloc, tdb, MDOUBLE, NULL);

				PTR ptr;
				ptr.u64 = &eval;
				uint64_t ieee754 = DoubleToIEEE754(*ptr.dp);
				D_quad(ieee754);
			}

			break;

		case SIZX:
			if (m6502)
				return error(in_6502mode);

			uint8_t extDbl[12];
			memset(extDbl, 0, 12);

			if (!defined)
			{
//				AddFixup(FU_FLOATEXT, sloc, exprbuf);
//				D_extend(extDbl);
				return error("labels not allowed in floating point expressions");
			}
			else
			{
//Would this *ever* happen?
//				if (tdb)
//					MarkRelocatable(cursect, sloc, tdb, MEXTEND, NULL);

				PTR ptr;
				ptr.u64 = &eval;
				DoubleToExtended(*ptr.dp, extDbl);
				D_extend(extDbl);
			}

			break;
		}

comma:
		if (*tok != ',')
			break;
	}

	ErrorIfNotAtEOL();
	return 0;
}


//
// dcb[.siz] expr1,expr2 - Make 'expr1' copies of 'expr2'
//
int d_dcb(WORD siz)
{
	uint64_t evalc, eval;
	WORD eattr;

	DEBUG { printf("dcb: section is %s%s%s (scattr=$%X)\n", (cursect & TEXT ? "TEXT" : ""), (cursect & DATA ? " DATA" : ""), (cursect & BSS ? "BSS" : ""), scattr); }

	if ((scattr & SBSS) != 0)
		return error("illegal initialization of section");

	if (abs_expr(&evalc) != OK)
		return 0;

	if (*tok++ != ',')
		return error("missing comma");

	if (expr(exprbuf, &eval, &eattr, NULL) < 0)
		return 0;

	if (cursect != M6502 && (siz != SIZB) && (sloc & 1))
		auto_even();

	dep_block((uint32_t)evalc, siz, (uint32_t)eval, eattr, exprbuf);

    ErrorIfNotAtEOL();
	return 0;
}


//
// Generalized initialization directive
//
// .init[.siz] [#count,] expression [.size] , ...
//
// The size suffix on the ".init" directive becomes the default size of the
// objects to deposit. If an item is preceeded with a sharp (immediate) sign
// and an expression, it specifies a repeat count. The value to be deposited
// may be followed by a size suffix, which overrides the default size.
//
int d_init(WORD def_siz)
{
	uint64_t count;
	uint64_t eval;
	WORD eattr;
	WORD siz;

	if ((scattr & SBSS) != 0)
		return error(".init not permitted in BSS or ABS");

	if (rgpu || rdsp)
		return error("directive forbidden in gpu/dsp mode");

	for(;;)
	{
		// Get repeat count (defaults to 1)
		if (*tok == '#')
		{
			tok++;

			if (abs_expr(&count) != OK)
				return 0;

			if (*tok++ != ',')
				return error(comma_error);
		}
		else
			count = 1;

		// Evaluate expression to deposit
		if (expr(exprbuf, &eval, &eattr, NULL) < 0)
			return 0;

		switch (*tok++)
		{                                 // Determine size of object to deposit
		case DOTB: siz = SIZB; break;
		case DOTW: siz = SIZB; break;
		case DOTL: siz = SIZL; break;
		default:
			siz = def_siz;
			tok--;
			break;
		}

		dep_block((uint32_t)count, siz, (uint32_t)eval, eattr, exprbuf);

		switch (*tok)
		{
		case EOL:
			return 0;
		case ',':
			tok++;
			continue;
		default:
			return error(comma_error);
		}
	}
}


//
// Deposit 'count' values of size 'siz' in the current (non-BSS) segment
//
int dep_block(uint32_t count, WORD siz, uint32_t eval, WORD eattr, TOKEN * exprbuf)
{
	WORD tdb = eattr & TDB;
	WORD defined = eattr & DEFINED;

	while (count--)
	{
		if ((challoc - ch_size) < 4)
			chcheck(4L);

		switch(siz)
		{
		case SIZB:
			if (!defined)
			{
				AddFixup(FU_BYTE | FU_SEXT, sloc, exprbuf);
				D_byte(0);
			}
			else
			{
				if (tdb)
					return error("non-absolute byte value");

				if (eval + 0x100 >= 0x200)
					return error(range_error);

				D_byte(eval);
			}

			break;
		case SIZW:
		case SIZN:
			if (!defined)
			{
				AddFixup(FU_WORD | FU_SEXT, sloc, exprbuf);
				D_word(0);
			}
			else
			{
				if (tdb)
					MarkRelocatable(cursect, sloc, tdb, MWORD, NULL);

				if (eval + 0x10000 >= 0x20000)
					return error(range_error);

				// Deposit 68000 or 6502 (byte-reversed) word
				if (cursect != M6502)
					D_word(eval)
				else
					D_rword(eval)

			}

			break;
		case SIZL:
			if (m6502)
				return error(in_6502mode);

			if (!defined)
			{
				AddFixup(FU_LONG, sloc, exprbuf);
				D_long(0);
			}
			else
			{
				if (tdb)
					MarkRelocatable(cursect, sloc, tdb, MLONG, NULL);

				D_long(eval);
			}

			break;
		}
	}

	return 0;
}


//
// .comm symbol, size
//
int d_comm(void)
{
	SYM * sym;
	char * p;
	uint64_t eval;

	if (m6502)
		return error(in_6502mode);

	if (*tok != SYMBOL)
		return error("missing symbol");

	p = string[tok[1]];
	tok += 2;

	if (*p == '.')						// Cannot .comm a local symbol
		return error(locgl_error);

	if ((sym = lookup(p, LABEL, 0)) == NULL)
		sym = NewSymbol(p, LABEL, 0);
	else
	{
		if (sym->sattr & DEFINED)
			return error(".comm symbol already defined");
	}

	sym->sattr = GLOBAL | COMMON | BSS;

	if (*tok++ != ',')
		return error(comma_error);

	if (abs_expr(&eval) != OK)			// Parse size of common region
		return 0;

	sym->svalue = eval;					// Install common symbol's size
	ErrorIfNotAtEOL();
	return 0;
}


//
// .list - Turn listing on
//
int d_list(void)
{
	if (list_flag)
		listing++;

	return 0;
}


//
// .nlist - Turn listing off
//
int d_nlist(void)
{
	if (list_flag)
		listing--;

	return 0;
}


//
// .68000 - Back to 68000 TEXT segment
//
int d_68000(void)
{
	rgpu = rdsp = robjproc = dsp56001 = 0;
	// Switching from gpu/dsp sections should reset any ORG'd Address
	orgactive = 0;
	orgwarning = 0;
	SaveSection();
	SwitchSection(TEXT);
	activecpu = CPU_68000;
	regbase = reg68base;	// Update register DFA tables
	regtab = reg68tab;
	regcheck = reg68check;
	regaccept = reg68accept;
	return 0;
}


//
// .68020 - Back to 68000 TEXT segment and select 68020
//
int d_68020(void)
{
	d_68000();
	activecpu = CPU_68020;
	return 0;
}


//
// .68030 - Back to 68000 TEXT segment and select 68030
//
int d_68030(void)
{
	d_68000();
	activecpu = CPU_68030;
	return 0;
}


//
// .68040 - Back to 68000 TEXT segment and select 68040
//
int d_68040(void)
{
	d_68000();
	activecpu = CPU_68040;
	activefpu = FPU_68040;
	return 0;
}


//
// .68060 - Back to 68000 TEXT segment and select 68060
//
int d_68060(void)
{
	d_68000();
	activecpu = CPU_68060;
	activefpu = FPU_68060;
	return 0;
}


//
// .68881 - Back to 680x0 TEXT segment and select 68881 FPU
//
int d_68881(void)
{
	activefpu = FPU_68881;
	regbase = reg68base;	// Update register DFA tables
	regtab = reg68tab;
	regcheck = reg68check;
	regaccept = reg68accept;
	return 0;
}


//
// .68882 - Back to 680x0 TEXT segment and select 68882 FPU
//
int d_68882(void)
{
	activefpu = FPU_68882;
	regbase = reg68base;	// Update register DFA tables
	regtab = reg68tab;
	regcheck = reg68check;
	regaccept = reg68accept;
	return 0;
}


//
// nofpu - Deselect FPUs.
//
int d_nofpu(void)
{
	activefpu = FPU_NONE;
	return 0;
}


//
// .56001 - Switch to DSP56001 assembler
//
int d_56001(void)
{
	dsp56001 = 1;
	rgpu = rdsp = robjproc = 0;
	SaveSection();

	if ((obj_format == LOD) || (obj_format == P56))
		SwitchSection(M56001P);

	regbase = reg56base;	// Update register DFA tables
	regtab = reg56tab;
	regcheck = reg56check;
	regaccept = reg56accept;
	used_architectures |= M56001P | M56001X | M56001Y | M56001L;
	return 0;
}


//
// .gpu - Switch to GPU assembler
//
int d_gpu(void)
{
	if ((cursect != TEXT) && (cursect != DATA))
	{
		error(".gpu can only be used in the TEXT or DATA segments");
		return ERROR;
	}

	// If previous section was DSP or 68000 then we need to reset ORG'd Addresses
	if (!rgpu)
	{
		orgactive = 0;
		orgwarning = 0;
	}

	rgpu = 1;			// Set GPU assembly
	rdsp = 0;			// Unset DSP assembly
	robjproc = 0;		// Unset OP assembly
	dsp56001 = 0;		// Unset 56001 assembly

	regbase = regriscbase;	// Update register DFA tables
	regtab = regrisctab;
	regcheck = regrisccheck;
	regaccept = regriscaccept;
	//used_architectures |= MGPU;	// TODO: Should GPU/DSP have their own dedicated sections in the long run?
	return 0;
}


//
// .dsp - Switch to DSP assembler
//
int d_dsp(void)
{
	if ((cursect != TEXT) && (cursect != DATA))
	{
		error(".dsp can only be used in the TEXT or DATA segments");
		return ERROR;
	}

	// If previous section was gpu or 68000 then we need to reset ORG'd Addresses
	if (!rdsp)
	{
		orgactive = 0;
		orgwarning = 0;
	}

	rdsp = 1;			// Set DSP assembly
	rgpu = 0;			// Unset GPU assembly
	robjproc = 0;		// Unset OP assembly
	dsp56001 = 0;		// Unset 56001 assembly

	regbase = regriscbase;	// Update register DFA tables
	regtab = regrisctab;
	regcheck = regrisccheck;
	regaccept = regriscaccept;
	//used_architectures |= MDSP;	// TODO: Should GPU/DSP have their own dedicated sections in the long run?
	return 0;
}


//
// .cargs [#offset], symbol[.size], ...
//
// Lists of registers may also be mentioned; they just take up space. Good for
// "documentation" purposes:
//
// .cargs a6, .arg1, .arg2, .arg3...
//
// Symbols thus created are ABS and EQUATED.
//
int d_cargs(void)
{
	uint64_t eval = 4;	// Default to 4 if no offset specified (to account for
						// return address)
	WORD rlist;
	SYM * symbol;
	char * p;
	int env;
	int i;

	if (rgpu || rdsp)
		return error("directive forbidden in gpu/dsp mode");

	if (*tok == '#')
	{
		tok++;

		if (abs_expr(&eval) != OK)
			return 0;

		// Eat the comma, if it's there
		if (*tok == ',')
			tok++;
	}

	for(;;)
	{
		if (*tok == SYMBOL)
		{
			p = string[tok[1]];

			// Set env to either local (dot prefixed) or global scope
			env = (*p == '.' ? curenv : 0);
			symbol = lookup(p, LABEL, env);

			if (symbol == NULL)
			{
				symbol = NewSymbol(p, LABEL, env);
				symbol->sattr = 0;
			}
			else if (symbol->sattr & DEFINED)
				return error("multiply-defined label '%s'", p);

			// Put symbol in "order of definition" list
			AddToSymbolDeclarationList(symbol);

			symbol->sattr |= (ABS | DEFINED | EQUATED);
			symbol->svalue = eval;
			tok += 2;

			// What this does is eat any dot suffixes attached to a symbol. If
			// it's a .L, it adds 4 to eval; if it's .W or .B, it adds 2. If
			// there is no dot suffix, it assumes a size of 2.
			switch ((int)*tok)
			{
			case DOTL:
				eval += 2;
			case DOTB:
			case DOTW:
				tok++;
			}

			eval += 2;
		}
		else if (*tok >= REG68_D0 && *tok <= REG68_A7)
		{
			if (reglist(&rlist) < 0)
				return 0;

			for(i=0; i<16; i++, rlist>>=1)
			{
				if (rlist & 1)
					eval += 4;
			}
		}
		else
		{
			switch ((int)*tok)
			{
			case REG68_USP:
			case REG68_SSP:
			case REG68_PC:
				eval += 2;
				// FALLTHROUGH
			case REG68_SR:
			case REG68_CCR:
				eval += 2;
				tok++;
				break;
			case EOL:
				return 0;
			default:
				return error(".cargs syntax");
			}
		}

		// Eat commas in between each argument, if they exist
		if (*tok == ',')
			tok++;
	}
}


//
// .cstruct [#offset], symbol[.size], ...
//
// Lists of registers may also be mentioned; they just take up space. Good for
// "documentation" purposes:
//
// .cstruct a6, .arg1, .arg2, .arg3...
//
// Symbols thus created are ABS and EQUATED. Note that this is for
// compatibility with VBCC and the Remover's library. Thanks to GroovyBee for
// the suggestion.
//
int d_cstruct(void)
{
	uint64_t eval = 0;	// Default, if no offset specified, is zero
	WORD rlist;
	SYM * symbol;
	char * symbolName;
	int env;
	int i;

	if (rgpu || rdsp)
		return error("directive forbidden in gpu/dsp mode");

	if (*tok == '#')
	{
		tok++;

		if (abs_expr(&eval) != OK)
			return 0;

		// Eat the comma, if it's there
		if (*tok == ',')
			tok++;
	}

	for(;;)
	{
		if (*tok == SYMBOL)
		{
			symbolName = string[tok[1]];

			// Set env to either local (dot prefixed) or global scope
			env = (symbolName[0] == '.' ? curenv : 0);
			symbol = lookup(symbolName, LABEL, env);

			// If the symbol wasn't found, then define it. Otherwise, throw an
			// error.
			if (symbol == NULL)
			{
				symbol = NewSymbol(symbolName, LABEL, env);
				symbol->sattr = 0;
			}
			else if (symbol->sattr & DEFINED)
				return error("multiply-defined label '%s'", symbolName);

			// Put symbol in "order of definition" list
			AddToSymbolDeclarationList(symbol);

			tok += 2;

			// Adjust label start address if it's a word or a long, as a byte
			// label might have left us on an odd address.
			switch ((int)*tok)
			{
			case DOTW:
			case DOTL:
				eval += eval & 0x01;
			}

			symbol->sattr |= (ABS | DEFINED | EQUATED);
			symbol->svalue = eval;

			// Check for dot suffixes and adjust space accordingly (longs and
			// words on an odd boundary get bumped to the next word aligned
			// address). If no suffix, then throw an error.
			switch ((int)*tok)
			{
			case DOTL:
				eval += 4;
				break;
			case DOTW:
				eval += 2;
				break;
			case DOTB:
				eval += 1;
				break;
			default:
				return error("Symbol missing dot suffix in .cstruct construct");
			}

			tok++;
		}
		else if (*tok >= REG68_D0 && *tok <= REG68_A7)
		{
			if (reglist(&rlist) < 0)
				return 0;

			for(i=0; i<16; i++, rlist>>=1)
			{
				if (rlist & 1)
					eval += 4;
			}
		}
		else
		{
			switch ((int)*tok)
			{
			case REG68_USP:
			case REG68_SSP:
			case REG68_PC:
				eval += 2;
				// FALLTHROUGH
			case REG68_SR:
			case REG68_CCR:
				eval += 2;
				tok++;
				break;
			case EOL:
				return 0;
			default:
				return error(".cstruct syntax");
			}
		}

		// Eat commas in between each argument, if they exist
		if (*tok == ',')
			tok++;
	}
}


//
// Define start of OP object list (allows the use of ORG)
//
int d_objproc(void)
{
	if ((cursect != TEXT) && (cursect != DATA))
	{
		error(".objproc can only be used in the TEXT or DATA segments");
		return ERROR;
	}

	// If previous section was DSP or 68000 then we need to reset ORG'd
	// Addresses
	if (!robjproc)
	{
		orgactive = 0;
		orgwarning = 0;
	}

	robjproc = 1;		// Set OP assembly
	rgpu = 0;			// Unset GPU assembly
	rdsp = 0;			// Unset DSP assembly
	dsp56001 = 0;		// Unset 56001 assembly
	//used_architectures |= MOP;	// TODO: Should OP have its own dedicated section in the long run?
	return OK;
}


//
// Undefine a macro - .undefmac macname [, macname...]
//
int undmac1(char * p)
{
	SYM * symbol = lookup(p, MACRO, 0);

	// If the macro symbol exists, cause it to disappear
	if (symbol != NULL)
		symbol->stype = (BYTE)SY_UNDEF;

	return OK;
}


int d_undmac(void)
{
	symlist(undmac1);
	return 0;
}


int d_jpad(void)
{
	warn("JPAD directive is deprecated/non-functional");
	return OK;
}


int d_nojpad(void)
{
	warn("NOJPAD directive is deprecated/non-functional");
	return OK;
}


int d_gpumain(void)
{
	return error("What the hell? Do you think we adhere to the Goof standard?");
}


//
// .opt - turn a specific (or all) optimisation on or off
//
int d_opt(void)
{
	while (*tok != EOL)
	{
		if (*tok == STRING)
		{
			tok++;
			char * tmpstr = string[*tok++];

			if (ParseOptimization(tmpstr) != OK)
				return error("unknown optimization flag '%s'", tmpstr);
		}
		else
			return error(".opt directive needs every switch enclosed inside quotation marks");
	}

	return OK;
}


//
// .if, Start conditional assembly
//
int d_if(void)
{
	WORD eattr;
	uint64_t eval;
	SYM * esym;
	IFENT * rif = f_ifent;

	// Alloc an IFENTRY
	if (rif == NULL)
		rif = (IFENT *)malloc(sizeof(IFENT));
	else
		f_ifent = rif->if_prev;

	rif->if_prev = ifent;
	ifent = rif;

	if (!disabled)
	{
		if (expr(exprbuf, &eval, &eattr, &esym) != OK)
			return 0;

		if ((eattr & DEFINED) == 0)
			return error(undef_error);

		disabled = !eval;
	}

	rif->if_state = (WORD)disabled;
	return 0;
}


//
// .else, Do alternate case for .if
//
int d_else(void)
{
	IFENT * rif = ifent;

	if (rif->if_prev == NULL)
		return error("mismatched .else");

	if (disabled)
		disabled = rif->if_prev->if_state;
	else
		disabled = 1;

	rif->if_state = (WORD)disabled;
	return 0;
}


//
// .endif, End of conditional assembly block
// This is also called by fpop() to pop levels of IFENTs in case a macro or
// include file exits early with `exitm' or `end'.
//
int d_endif(void)
{
	IFENT * rif = ifent;

	if (rif->if_prev == NULL)
		return error("mismatched .endif");

	ifent = rif->if_prev;
	disabled = rif->if_prev->if_state;
	rif->if_prev = f_ifent;
	f_ifent = rif;
	return 0;
}
