//
// RMAC - Renamed Macro Assembler for all Atari computers
// RMAC.C - Main Application Code
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "rmac.h"
#include "6502.h"
#include "debug.h"
#include "direct.h"
#include "dsp56k.h"
#include "error.h"
#include "expr.h"
#include "listing.h"
#include "mark.h"
#include "macro.h"
#include "object.h"
#include "procln.h"
#include "riscasm.h"
#include "sect.h"
#include "symbol.h"
#include "token.h"
#include "version.h"

int perm_verb_flag;				// Permanently verbose, interactive mode
int list_flag;					// "-l" listing flag on command line
int list_pag = 1;				// Enable listing pagination by default
int verb_flag;					// Be verbose about what's going on
int m6502;						// 1, assembling 6502 code
int glob_flag;					// Assume undefined symbols are global
int lsym_flag;					// Include local symbols in object file (ALWAYS true)
int dsym_flag;					// Gen debug syms (Requires obj_format = BSD)
int optim_warn_flag;			// Warn about possible short branches
int prg_flag;					// !=0, produce .PRG executable (2=symbols)
int prg_extend;					// !=0, output extended .PRG symbols
int legacy_flag;				// Do stuff like insert code in RISC assembler
int obj_format;					// Object format flag
int debug;						// [1..9] Enable debugging levels
int err_flag;					// '-e' specified
int err_fd;						// File to write error messages to
int rgpu, rdsp;					// Assembling Jaguar GPU or DSP code
int robjproc;					// Assembling Jaguar Object Processor code
int dsp56001;					// Assembling DSP 56001 code
int list_fd;					// File to write listing to
int segpadsize;					// Segment padding size
int endian;						// Host processor endianess (0 = LE, 1 = BE)
int *regbase;					// Points to current DFA register table (base)
int *regtab;					// Points to current DFA register table (tab)
int *regcheck;					// Points to current DFA register table (check)
int *regaccept;					// Points to current DFA register table (accept)
char * objfname;				// Object filename pointer
char * firstfname;				// First source filename
char * cmdlnexec;				// Executable name, pointer to ARGV[0]
char searchpatha[512] = { 0 };	// Buffer to hold searchpath when specified
char * searchpath = NULL;		// Search path for include files
char defname[] = "noname.o";	// Default output filename
int optim_flags[OPT_COUNT_ALL] = { 0 };	// Specific optimisations on/off matrix
int activecpu = CPU_68000;		// Active 68k CPU (68000 by default)
int activefpu = FPU_NONE;		// Active FPU (none by default)
int org68k_active = 0;			// .org switch for 68k (only with RAW output format)
uint32_t org68k_address;		// .org for 68k
int correctMathRules;			// 1, use C operator precedence in expressions
uint32_t used_architectures;	// Bitmask that records exactly which architectures were used during assembly

//
// Convert a string to uppercase
//
void strtoupper(char * s)
{
	while (*s)
		*s++ &= 0xDF;
}

//
// Manipulate file extension.
//
// 'name' must be large enough to hold any possible filename. If 'stripp' is
// nonzero, any old extension is removed. If the file does not already have an
// extension, 'extension' is appended to the filename.
//
char * fext(char * name, char * extension, int stripp)
{
	char * s;

	// Find beginning of "real" name (strip off path)
	char * beg = strrchr(name, SLASHCHAR);

	if (beg == NULL)
		beg = name;

	// Clobber any old extension, if requested
	if (stripp)
	{
		for(s=beg; *s && *s!='.'; ++s)
			;

		*s = '\0';
	}

	if (strrchr(beg, '.') == NULL)
		strcat(beg, extension);

	return name;
}

static int is_sep(char c)
{
    const char *seps = PATH_SEPS;

    for (seps = PATH_SEPS; *seps; seps++) {
        if (*seps == c)
            return 1;
    }

    return 0;
}

//
// Return 'item'nth element of semicolon-seperated pathnames specified in the
// enviroment string 's'. Copy the pathname to 'buf'.  Return 0 if the 'item'
// nth path doesn't exist.
//
// ['item' ranges from 0 to N-1, where N = #elements in search path]
//
int nthpath(char * env_var, int itemno, char * buf)
{
	char * s = searchpath;

	if (s == NULL)
		s = getenv(env_var);

	if (s == NULL)
		return 0;

	while (itemno--)
		while (*s != EOS && !is_sep(*s++))
			;

	if (*s == EOS)
		return 0;

	while (*s != EOS && !is_sep(*s))
		*buf++ = *s++;

	*buf++ = EOS;

	return 1;
}

//
// Display command line help
//
void DisplayHelp(void)
{
	printf("Usage:\n"
		"    %s [options] srcfile\n"
		"\n"
		"Options:\n"
		"  -? or -h          Display usage information\n"
		"  -dsymbol[=value]  Define symbol (with optional value, default=0)\n"
		"  -e[errorfile]     Send error messages to file, not stdout\n"
		"  -f[format]        Output object file format\n"
		"                    a: ALCYON\n"
		"                    b: BSD (use this for Jaguar)\n"
		"                    c: PRG (C64)\n"
		"                    e: ELF\n"
		"                    p: P56 (use this for DSP56001 only)\n"
		"                    l: LOD (use this for DSP56001 only)\n"
		"                    x: com/exe/xex (Atari 800)\n"
		"                    r: absolute address\n"
		"  -g                Output source level debug information (BSD object only)\n"
		"  -i[path]          Directory to search for include files\n"
		"  -l[filename]      Create an output listing file\n"
		"  -l*[filename]     Create an output listing file without pagination\n"
		"  -m[cpu]           Select default CPU. Available options:\n"
		"                    68000, 68020, 68030, 68040, 68060, 6502, tom, jerry, 56001\n"
		"  -n                Don't do things behind your back in RISC assembler\n"
		"  -o file           Output file name\n"
		"  +o[value]         Turn a specific optimisation on\n"
		"                    Available optimisation switches:\n"
		"                    o0: Absolute long addresses to word\n"
		"                    o1: move.l #x,dn/an to moveq\n"
		"                    o2: Word branches to short\n"
		"                    o3: Outer displacement 0(an) to (an)\n"
		"                    o4: lea size(An),An to addq #size,An\n"
		"                    o5: 68020+ Absolute long base/outer disp. to word\n"
		"                    o6: Null branches to NOP\n"
		"                    o7: clr.l Dx to moveq #0,Dx\n"
		"                    o8: adda.w/l #x,Dy to addq.w/l #x,Dy\n"
		"                    o9: adda.w/l #x,Ay to lea x(Dy),Ay\n"
		"                    o10: 56001 Use short format for immediate values if possible\n"
		"                    o11: 56001 Auto convert short addressing mode to long (default: on)\n"
		"                    o30: Enforce PC relative (alternative name: op)\n"
		"  ~o[value]         Turn a specific optimisation off\n"
		"  +oall             Turn all optimisations on\n"
		"  ~oall             Turn all optimisations off\n"
		"  -p                Create an ST .prg (without symbols). Forces -fa\n"
		"  -ps               Create an ST .prg (with symbols). Forces -fa\n"
		"  -px               Create an ST .prg (with exsymbols). Forces -fa\n"
		"  -r[size]          Pad segments to boundary size specified\n"
		"                    w: word (2 bytes, default alignment)\n"
		"                    l: long (4 bytes)\n"
		"                    p: phrase (8 bytes)\n"
		"                    d: double phrase (16 bytes)\n"
		"                    q: quad phrase (32 bytes)\n"
		"  -s                Warn about possible short branches\n"
		"                    and applied optimisations\n"
		"  -u                Force referenced and undefined symbols global\n"
		"  -v                Set verbose mode\n"
		"  -x                Turn on debugging mode\n"
		"  -y[pagelen]       Set page line length (default: 61)\n"
		"  -4                Use C style operator precedence\n"
		"\n", cmdlnexec);
}


//
// Display version information
//
void DisplayVersion(void)
{
	printf("\n"
		" _ __ _ __ ___   __ _  ___ \n"
		"| '__| '_ ` _ \\ / _` |/ __|\n"
		"| |  | | | | | | (_| | (__ \n"
		"|_|  |_| |_| |_|\\__,_|\\___|\n"
		"\nRenamed Macro Assembler\n"
		"Copyright (C) 199x Landon Dyer, 2011-2024 rmac authors\n"
		"V%01i.%01i.%01i-BS42 %s (%s)\n\n", MAJOR, MINOR, PATCH, __DATE__, PLATFORM);
}

//
// Parse optimisation options
//
int ParseOptimization(char * optstring)
{
	int onoff = 0;

	while (*optstring)
	{
		if (*optstring == '+')
			onoff = 1;
		else if (*optstring != '~')
			return ERROR;

        if (optstring[2] == 0)
            return error(".opt called with zero arguments");

		if ((optstring[2] == 'a' || optstring[2] == 'A')
			&& (optstring[3] == 'l' || optstring[3] == 'L')
			&& (optstring[4] == 'l' || optstring[4] == 'L'))
		{
			memset(optim_flags, onoff, OPT_COUNT * sizeof(int));
			optstring += 5;
		}
		else if (optstring[1] == 'o' || optstring[1] == 'O') // Turn on specific optimisation
		{
			if (optstring[2] == 'p' || optstring[2] == 'P')
			{
				optim_flags[OPT_PC_RELATIVE] = onoff;
				optstring += 3;
			}
			else
			{
				int opt_no = atoi(&optstring[2]);

				if ((opt_no >= 0) && (opt_no < OPT_COUNT))
				{
					optim_flags[opt_no] = onoff;
					optstring += 3;

					// If opt_no is 2 digits then consume an extra character.
					// Sounds a bit sleazy but we know we're not going to hit
					// more than 100 optimisation switches so this should be
					// fine. If we do hit that number then it'll probably be
					// time to burn the whole codebase and start from scratch.
					if (opt_no > 9)
						optstring++;
				}
				else
					return ERROR;
			}
		}
		else
		{
			return ERROR;
		}

		if (*optstring == ',')
			optstring++;
	}
	return OK;
}

static void ProcessFile(int fd, char *fname)
{
	char *dbgname = fname;

	if (NULL == fname)
	{
		fname = defname; // Kludge first filename
		dbgname = "(stdin)";
	}

	// First file operations:
	if (firstfname == NULL)
	{
		// Record first filename.
		firstfname = fname;

		// Validate option compatibility
		if (dsym_flag)
		{
			if (obj_format != BSD)
			{
				printf("-g: debug information only supported with BSD object file format\n");
				dsym_flag = 0;
				errcnt++;
			}
			else
			{
				GenMainFileSym(dbgname);
			}
		}
	}

	include(fd, dbgname);
	Assemble();
}

extern int reg68base[53];
extern int reg68tab[222];
extern int reg68check[222];
extern int reg68accept[222];

//
// Process command line arguments and do an assembly
//
int Process(int argc, char ** argv)
{
	int argno;						// Argument number
	SYM * sy;						// Pointer to a symbol record
	char * s;						// String pointer
	int fd;							// File descriptor
	char fnbuf[FNSIZ];				// Filename buffer
	int i;							// Iterator
	int current_path_index = 0;		// Iterator for search paths

	errcnt = 0;						// Initialize error count
	listing = 0;					// Initialize listing level
	list_flag = 0;					// Initialize listing flag
	verb_flag = perm_verb_flag;		// Initialize verbose flag
	glob_flag = 0;					// Initialize .globl flag
	optim_warn_flag = 0;			// Initialize short branch flag
	debug = 0;						// Initialize debug flag
	objfname = NULL;				// Initialize object filename
	list_fname = NULL;				// Initialize listing filename
	err_fname = NULL;				// Initialize error filename
	obj_format = BSD;				// Initialize object format
	firstfname = NULL;				// Initialize first filename
	err_fd = ERROUT;				// Initialize error file descriptor
	err_flag = 0;					// Initialize error flag
	rgpu = 0;						// Initialize GPU assembly flag
	rdsp = 0;						// Initialize DSP assembly flag
	robjproc = 0;					// Initialize OP assembly flag
	lsym_flag = 1;					// Include local symbols in object file
	dsym_flag = 0;					// No debug sym generation by default
	orgactive = 0;					// Not in RISC org section
	orgwarning = 0;					// No ORG warning issued
	segpadsize = 2;					// Initialize segment padding size
    dsp_orgmap[0].start = 0;		// Initialize 56001 org initial address
    dsp_orgmap[0].memtype = ORG_P;	// Initialize 56001 org start segment
	m6502 = 0;						// 6502 mode off by default
	regbase = reg68base;			// Initialise DFA register tables
	regtab = reg68tab;				// Idem
	regcheck = reg68check;			// Idem
	regaccept = reg68accept;		// Idem
    correctMathRules = 0;			// respect operator precedence
	used_architectures = 0;			// Initialise used architectures bitfield
	// Initialize modules
	InitSymbolTable();				// Symbol table
	InitTokenizer();				// Tokenizer
	InitLineProcessor();			// Line processor
	InitExpression();				// Expression analyzer
	InitSection();					// Section manager / code generator
	InitMark();						// Mark tape-recorder
	InitMacro();					// Macro processor
	InitListing();					// Listing generator
	Init6502();						// 6502 assembler

	// Process command line arguments and assemble source files
	for(argno=0; argno<argc; argno++)
	{
		if (*argv[argno] == '-')
		{
			switch (argv[argno][1])
			{
                        case '4':
                          correctMathRules = 1;
                          break;
			case 'd':				// Define symbol
			case 'D':
				for(s=argv[argno]+2; *s!=EOS;)
				{
					if (*s++ == '=')
					{
						s[-1] = EOS;
						break;
					}
				}

				if (argv[argno][2] == EOS)
				{
					printf("-d: empty symbol\n");
					errcnt++;
					return errcnt;
				}

				sy = lookup(argv[argno] + 2, 0, 0);

				if (sy == NULL)
				{
					sy = NewSymbol(argv[argno] + 2, LABEL, 0);
					sy->svalue = 0;
				}

				sy->sattr = DEFINED | EQUATED | ABS;
				sy->svalue = 0;
				if (*s)
				{
					if (*s == '$')
					{
						// Hex number
						s++;
						if (*s)
						{
							sy->svalue = strtol(s, 0, 16);
						}
					}
					else
					{
						sy->svalue = (uint64_t)atoi(s);
					}
				}
				//sy->svalue = (*s ? (uint64_t)atoi(s) : 0);
				break;
			case 'e':				// Redirect error message output
			case 'E':
				err_fname = argv[argno] + 2;
				break;
			case 'f':				// -f<format>
			case 'F':
				switch (argv[argno][2])
				{
				case EOS:
				case 'a':			// -fa = Alcyon [the default]
				case 'A':
					obj_format = ALCYON;
					break;
				case 'b':			// -fb = BSD (Jaguar Recommended: 3 out 4 jaguars recommend it!)
				case 'B':
					obj_format = BSD;
					break;
				case 'c':
				case 'C':
					obj_format = C64PRG;
					break;
				case 'e':			// -fe = ELF
				case 'E':
					obj_format = ELF;
					break;
                case 'l':           // -fl = LOD
                case 'L':
                    obj_format = LOD;
                    break;
                case 'p':           // -fp = P56
                case 'P':
                    obj_format = P56;
					break;
				case 'x':			// -fx = COM/EXE/XEX
				case 'X':
					obj_format = XEX;
					break;
                case 'r':           // -fr = Absolute address
                case 'R':
                    obj_format = RAW;
                    break;
				default:
					printf("-f: unknown object format specified\n");
					errcnt++;
					return errcnt;
				}
				break;
			case 'g':				// Debugging flag
			case 'G':
				dsym_flag = 1;
				break;
			case 'i':				// Set directory search path
			case 'I':
			{
				strcat(searchpatha, argv[argno] + 2);
				strcat(searchpatha, ";");
				searchpath = searchpatha;

				// Check to see if include paths actually exist
				char current_path[256];

				for(i=current_path_index; nthpath("RMACPATH", i, current_path)!=0; i++)
				{
					if (strlen(current_path) > 0)
					{
						DIR * test = opendir(current_path);

						if (test == NULL)
						{
							printf("Invalid include path: %s\n", current_path);
							errcnt++;
							return errcnt;
						}

						closedir(test);
					}
				}

				current_path_index = i - 1;
				break;
			}
			case 'l':				// Produce listing file
			case 'L':
				if (*(argv[argno] + 2) == '*')
				{
					list_fname = argv[argno] + 3;
					list_pag = 0;    // Special case - turn off pagination
				}
				else
				{
					list_fname = argv[argno] + 2;
				}

				listing = 1;
				list_flag = 1;
				lnsave++;
				break;
			case 'm':
			case 'M':
				if (strcmp(argv[argno] + 2, "68000") == 0)
					d_68000();
				else if (strcmp(argv[argno] + 2, "68020") == 0)
					d_68020();
				else if (strcmp(argv[argno] + 2, "68030") == 0)
					d_68030();
				else if (strcmp(argv[argno] + 2, "68040") == 0)
					d_68040();
				else if (strcmp(argv[argno] + 2, "68060") == 0)
					d_68060();
				else if (strcmp(argv[argno] + 2, "68881") == 0)
					d_68881();
				else if (strcmp(argv[argno] + 2, "68882") == 0)
					d_68882();
				else if (strcmp(argv[argno] + 2, "56001") == 0)
					d_56001();
				else if (strcmp(argv[argno] + 2, "6502") == 0)
					d_6502();
				else if (strcmp(argv[argno] + 2, "tom") == 0)
					d_gpu();
				else if (strcmp(argv[argno] + 2, "jerry") == 0)
					d_dsp();
				else
				{
					printf("Unrecognized CPU '%s'\n", argv[argno] + 2);
					errcnt++;
					return errcnt;
				}
				break;
			case 'o':				// Direct object file output
			case 'O':
				if (argv[argno][2] != EOS)
					objfname = argv[argno] + 2;
				else
				{
					if (++argno >= argc)
					{
						printf("Missing argument to -o");
						errcnt++;
						return errcnt;
					}

					objfname = argv[argno];
				}

				break;
			case 'p':		/* -p: generate ".PRG" executable output */
			case 'P':
				/*
				 * -p		.PRG generation w/o symbols
				 * -ps		.PRG generation with symbols
				 * -px		.PRG generation with extended symbols
				 */
				switch (argv[argno][2])
				{
					case EOS:
						prg_flag = 1;
						break;

					case 's':
					case 'S':
						prg_flag = 2;
						break;

					case 'x':
					case 'X':
						prg_flag = 3;
						break;

					default:
						printf("-p: syntax error\n");
						errcnt++;
						return errcnt;
				}

				// Enforce Alcyon object format - kind of silly
				// to ask for .prg output without it!
				obj_format = ALCYON;
				break;
			case 'r':				// Pad seg to requested boundary size
			case 'R':
				switch(argv[argno][2])
				{
				case 'w': case 'W': segpadsize = 2;  break;
				case 'l': case 'L': segpadsize = 4;  break;
				case 'p': case 'P': segpadsize = 8;  break;
				case 'd': case 'D': segpadsize = 16; break;
				case 'q': case 'Q': segpadsize = 32; break;
				default: segpadsize = 2; break;	// Effective autoeven();
				}
				break;
			case 's':				// Warn about possible short branches and applied optimisations
			case 'S':
				optim_warn_flag = 1;
				break;
			case 'u':				// Make undefined symbols .globl
			case 'U':
				glob_flag = 1;
				break;
			case 'v':				// Verbose flag
			case 'V':
				verb_flag++;

				if (verb_flag > 1)
					DisplayVersion();

				break;
			case 'x':				// Turn on debugging
			case 'X':
				debug = 1;
				printf("~ Debugging ON\n");
				break;
			case 'y':				// -y<pagelen>
			case 'Y':
				pagelen = atoi(argv[argno] + 2);

				if (pagelen < 10)
				{
					printf("-y: bad page length\n");
					errcnt++;
					return errcnt;
				}

				break;
			case EOS:				// Input is stdin
				ProcessFile(0, NULL);
				break;
			case 'h':				// Display command line usage
			case 'H':
			case '?':
				DisplayVersion();
				DisplayHelp();
				errcnt++;
				break;
			case 'n':				// Turn off legacy mode
			case 'N':
				legacy_flag = 0;
				printf("Legacy mode OFF\n");
				break;
			default:
				DisplayVersion();
				printf("Unknown switch: %s\n\n", argv[argno]);
				DisplayHelp();
				errcnt++;
				break;
			}
		}
		else if (*argv[argno] == '+' || *argv[argno] == '~')
		{
			if (ParseOptimization(argv[argno]) != OK)
			{
				DisplayVersion();
				printf("Unknown switch: %s\n\n", argv[argno]);
				DisplayHelp();
				errcnt++;
				break;
			}
		}
		else
		{
			strcpy(fnbuf, argv[argno]);
			fext(fnbuf, ".s", 0);
			fd = open(fnbuf, 0);

			if (fd < 0)
			{
				printf("Cannot open: %s\n", fnbuf);
				errcnt++;
				continue;
			}

			ProcessFile(fd, fnbuf);
		}
	}

	// Wind-up processing;
	// o  save current section (no more code generation)
	// o  do auto-even of all sections (or boundary alignment as requested
	//    through '-r')
    // o  save the last pc value for 6502 (if we used 6502 at all) and
    //    detemine if the last section contains anything
	// o  determine name of object file:
	//    -  "foo.o" for linkable output;
	//    -  "foo.prg" for GEMDOS executable (-p flag).
	SaveSection();
	int temp_section = cursect;

	for(i=TEXT; i<=BSS; i<<=1)
	{
		SwitchSection(i);

		switch(segpadsize)
		{
		case 2:  d_even();    break;
		case 4:  d_long();    break;
		case 8:  d_phrase();  break;
		case 16: d_dphrase(); break;
		case 32: d_qphrase(); break;
		}

		SaveSection();
	}

	SwitchSection(M6502);

	if (sloc != currentorg[0])
	{
		currentorg[1] = sloc;
		currentorg += 2;
	}

	// This looks like an awful kludge...  !!! FIX !!!
	if (temp_section & (M56001P | M56001X | M56001Y))
	{
		SwitchSection(temp_section);

		if (chptr != dsp_currentorg->start)
		{
			dsp_currentorg->end = chptr;
			dsp_currentorg++;
		}
	}

	SwitchSection(TEXT);

	if (objfname == NULL)
	{
		if (firstfname == NULL)
			firstfname = defname;

		// It's the size of fnbuf minus 5 because of the possible 4 char suffix
		// + trailing null (added by fext()).
		strncpy(fnbuf, firstfname,sizeof(fnbuf)-5);
		fext(fnbuf, (prg_flag ? ".prg" : ".o"), 1);
		objfname = fnbuf;
	}

	// With one pass finished, go back and:
	// (1)   run through all the fixups and resolve forward references;
	// (1.5) ensure that remaining fixups can be handled by the linker
	//       (`lo68' format, extended (postfix) format....)
	// (2)   generate the output file image and symbol table;
	// (3)   generate relocation information from left-over fixups.
	ResolveAllFixups();						// Do all fixups
	StopMark();								// Stop mark tape-recorder

	if (errcnt == 0)
	{
		if ((fd = open(objfname, _OPEN_FLAGS, _PERM_MODE)) < 0)
			CantCreateFile(objfname);

		if (verb_flag)
		{
			s = (prg_flag ? "executable" : "object");
			printf("[Writing %s file: %s]\n", s, objfname);
		}

		WriteObject(fd);
		close(fd);

		if (errcnt != 0)
			unlink(objfname);
	}

	if (list_flag)
	{
		if (verb_flag)
			printf("[Wrapping-up listing file]\n");

		listing = 1;
		symtable();
		close(list_fd);
	}

	if (err_flag)
		close(err_fd);

	DEBUG dump_everything();

	return errcnt;
}

//
// Determine processor endianess
//
int GetEndianess(void)
{
	int i = 1;
	char * p = (char *)&i;

	if (p[0] == 1)
		return 0;

	return 1;
}

//
// Application entry point
//
int main(int argc, char ** argv)
{
	perm_verb_flag = 0;				// Clobber "permanent" verbose flag
	legacy_flag = 1;				// Default is legacy mode on (:-P)
	optim_flags[OPT_56K_SHORT] = 1; // This ensures compatibilty with Motorola's 56k assembler

	cmdlnexec = argv[0];			// Obtain executable name
	endian = GetEndianess();		// Get processor endianess

	// If commands were passed in, process them
	if (argc > 1)
		return Process(argc - 1, argv + 1);

	DisplayVersion();
	DisplayHelp();

	return 0;
}
