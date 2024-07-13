//
// RMAC - Renamed Macro Assembler for all Atari computers
// SYMBOL.C - Symbol Handling
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "symbol.h"
#include "dsp56k.h"
#include "error.h"
#include "listing.h"
#include "object.h"
#include "procln.h"


// Macros
#define NBUCKETS 256				// Number of hash buckets (power of 2)

static SYM * symbolTable[NBUCKETS];	// User symbol-table header
int curenv;							// Current enviroment number
static SYM * sorder;				// * -> Symbols, in order of reference
static SYM * sordtail;				// * -> Last symbol in sorder list
static SYM * sdecl;					// * -> Symbols, in order of declaration
static SYM * sdecltail;				// * -> Last symbol in sdecl list
static uint32_t currentUID;			// Symbol UID tracking (done by NewSymbol())
uint32_t firstglobal; // Index of the first global symbol in an ELF object.

// Tags for marking symbol spaces:
// a = absolute
// t = text
// d = data
// ! = "impossible!"
// b = BSS
static uint8_t tdb_text[8] = {
   'a', 't', 'd', '!', 'b', SPACE, SPACE, SPACE
};

// Internal function prototypes
static uint16_t WriteLODSection(int, uint16_t);

//
// Initialize symbol table
//
void InitSymbolTable(void)
{
	for(int i=0; i<NBUCKETS; i++)			// Initialise symbol hash table
		symbolTable[i] = NULL;

	curenv = 1;								// Init local symbol enviroment
	sorder = NULL;							// Init symbol-reference list
	sordtail = NULL;
	sdecl = NULL;							// Init symbol-decl list
	sdecltail = NULL;
	currentUID = 0;
}

//
// Hash the ASCII name and enviroment number
//
int HashSymbol(const uint8_t * name, int envno)
{
	int sum = envno, k = 0;

	for(; *name; name++)
	{
		if (k++ == 1)
			sum += *name << 2;
		else
			sum += *name;
	}

	return sum & (NBUCKETS - 1);
}

//
// Make a new symbol of type 'type' in enviroment 'envno'
//
SYM * NewSymbol(const uint8_t * name, int type, int envno)
{
	// Allocate the symbol
	SYM * symbol = malloc(sizeof(SYM));

	if (symbol == NULL)
	{
		printf("NewSymbol: MALLOC ERROR (symbol=\"%s\")\n", name);
		return NULL;
	}

	// Fill-in the symbol
	symbol->sname  = name ? strdup(name) : NULL;
	symbol->stype  = (uint8_t)type;
	symbol->senv   = (uint16_t)envno;
	// We don't set this as DEFINED, as it could be a forward reference!
	symbol->sattr  = 0;
	// We don't set RISCSYM here as not every symbol first seen in a RISC
	// section is a RISC symbol!
	symbol->sattre = 0;
	symbol->svalue = 0;
	symbol->sorder = NULL;
	symbol->uid    = currentUID++;
	// We don't set st_type, st_desc, or st_other here because they are only
	// used by stabs debug symbols, which are always initialized by
	// NewDebugSymbol(), which always sets these fields. Hence, initializing
	// them here would be redundant.

	// Record filename the symbol is defined (Used by macro error reporting and some debug symbols)
	symbol->cfileno = cfileno;

	// Don't hash debug symbols: they are never looked up and may have no name.
	if (type != DBGSYM)
	{
	// Install symbol in the symbol table
	int hash = HashSymbol(name, envno);
	symbol->snext = symbolTable[hash];
	symbolTable[hash] = symbol;
	}

	// Append symbol to the symbol-order list
	if (sorder == NULL)
		sorder = symbol;					// Add first symbol
	else
		sordtail->sorder = symbol;			// Or append to tail of list

	sordtail = symbol;
	return symbol;
}

//
// Look up the symbol name by its UID and return the pointer to the name.
// If it's not found, return NULL.
//
uint8_t * GetSymbolNameByUID(uint32_t uid)
{
	//problem is with string lookup, that's why we're writing this
	//so once this is written, we can put the uid in the token stream

	// A much better approach to the symbol order list would be to make an
	// array--that way you can do away with the UIDs and all the rest, and
	// simply do an array lookup based on position. But meh, let's do this for
	// now until we can rewrite things so they make sense.
	SYM * symbol = sorder;

	for(; symbol; symbol=symbol->sorder)
	{
		if (symbol->uid == uid)
			return symbol->sname;
	}

	return NULL;
}

//
// Lookup the symbol 'name', of the specified type, with the specified
// enviroment level
//
SYM * lookup(uint8_t * name, int type, int envno)
{
	SYM * symbol = symbolTable[HashSymbol(name, envno)];

	// Do linear-search for symbol in bucket
	while (symbol != NULL)
	{
		if (symbol->stype == type			// Type, envno and name must match
			&& symbol->senv  == envno
			&& *name == *symbol->sname		// Fast check for first character
			&& !strcmp(name, symbol->sname))	// More expensive check
			break;

		symbol = symbol->snext;
	}

	// Return NULL or matching symbol
	return symbol;
}

//
// Put symbol on "order-of-declaration" list of symbols
//
void AddToSymbolDeclarationList(SYM * symbol)
{
	// Don't add if already on list, or it's an equated register/CC
	if ((symbol->sattr & SDECLLIST)
		|| (symbol->sattre & (EQUATEDREG | UNDEF_EQUR | EQUATEDCC | UNDEF_CC)))
		return;

	// Mark as "on .sdecl list"
	symbol->sattr |= SDECLLIST;

	if (sdecl == NULL)
		sdecl = symbol;				// First on decl-list
	else
		sdecltail->sdecl = symbol;	// Add to end of list

	// Fix up list's tail
	symbol->sdecl = NULL;
	sdecltail = symbol;
}

//
// Make all referenced, undefined symbols global
//
void ForceUndefinedSymbolsGlobal(void)
{
	SYM * sy;

	DEBUG printf("~ForceUndefinedSymbolsGlobal()\n");

	// Scan through all symbols; if a symbol is REFERENCED but not DEFINED,
	// then make it global.
	for(sy=sorder; sy!=NULL; sy=sy->sorder)
	{
		if (sy->stype == LABEL && sy->senv == 0
			&& ((sy->sattr & (REFERENCED | DEFINED)) == REFERENCED))
			sy->sattr |= GLOBAL;
	}
}

//
// Assign numbers to symbols that are to be exported or imported. The symbol
// number is put in 'senv'. Returns the number of symbols that will be in the
// symbol table.
//
// N.B.: This is usually called twice; first time with NULL parameters and the
//       second time with real ones. The first one is typically done to get a
//       count of the # of symbols in the symbol table, and the second is to
//       actually create it.
//
uint32_t AssignSymbolNos(uint8_t * buf, uint8_t *(* construct)(uint8_t * buf, SYM * sym, int globflag))
{
	uint16_t scount = 0;

	// Done only on first pass...
	if (buf == NULL)
	{
		// Append all symbols not appearing on the .sdecl list to the end of
		// the .sdecl list
		for(SYM * sy=sorder; sy!=NULL; sy=sy->sorder)
			AddToSymbolDeclarationList(sy);
	}

	// Run through all symbols (now on the .sdecl list) and assign numbers to
	// them. We also pick which symbols should be global or not here.
	for(SYM * sy=sdecl; sy!=NULL; sy=sy->sdecl)
	{
		// Always export debug symbols. Don't force them global.
		if (DBGSYM == sy->stype) {
			sy->senv = scount++;

			if (buf != NULL)
				buf = construct(buf, sy, 0);
			continue;
		}

		// Skip non-labels.
		if (sy->stype != LABEL)
			continue;

		// Nuke equated register/CC symbols from orbit:
		if (sy->sattre & (EQUATEDREG | UNDEF_EQUR | EQUATEDCC | UNDEF_CC))
			continue;

		// Export or import external references, and export COMMON blocks.
		// N.B.: This says to mark the symbol as global if either 1) the symbol
		//       is global AND the symbol is defined OR referenced, or 2) this
		//       symbol is a common symbol.
		if (((sy->sattr & GLOBAL) && (sy->sattr & (DEFINED | REFERENCED)))
			|| (sy->sattr & COMMON))
		{
			sy->senv = scount++;

			if (buf != NULL)
				buf = construct(buf, sy, 1);
		}
		// Export vanilla labels (but don't make them global). An exception is
		// made for equates, which are not exported unless they are referenced.
		// ^^^ The above just might be bullshit. ^^^
		// N.B.: This says if the symbol is either defined OR referenced (but
		//       because of the above we know it *won't* be GLOBAL).  And
		//       lsym_flag is always set true in Process() in rmac.c.
		else if (lsym_flag && (sy->sattr & (DEFINED | REFERENCED)))
		{
			sy->senv = scount++;

			if (buf != NULL)
				buf = construct(buf, sy, 0);
		}
	}

	return scount;
}

//
// Custom version of AssignSymbolNos for ELF .o files.
// The order that the symbols should be dumped is different.
// (globals must be explicitly at the end of the table)
//
// N.B.: It should be possible to merge this with AssignSymbolNos, as there's
//       nothing really ELF specific in here, other than the "globals go at the
//       end of the queue" thing, which doesn't break the others. :-P
uint32_t AssignSymbolNosELF(uint8_t * buf, uint8_t *(* construct)(uint8_t * buf, SYM * sym, int globflag))
{
	uint16_t scount = 0;

	// Append all symbols not appearing on the .sdecl list to the end of
	// the .sdecl list
	for(SYM * sy=sorder; sy!=NULL; sy=sy->sorder)
		AddToSymbolDeclarationList(sy);

	// Run through all symbols (now on the .sdecl list) and assign numbers to
	// them. We also pick which symbols should be global or not here.
	for(SYM * sy=sdecl; sy!=NULL; sy=sy->sdecl)
	{
		// Export vanilla labels (but don't make them global). An exception is
		// made for equates, which are not exported unless they are referenced.
		if (sy->stype == LABEL && lsym_flag
			&& (sy->sattr & (DEFINED | REFERENCED)) != 0
			&& (*sy->sname != '.')
			&& (sy->sattr & GLOBAL) == 0
			&& (sy->sattre & (EQUATEDREG | UNDEF_EQUR | EQUATEDCC | UNDEF_CC)) == 0)
		{
			sy->senv = scount++;

			if (buf != NULL)
				buf = construct(buf, sy, 0);
		}
	}

	firstglobal = scount;

	// For ELF object mode run through all symbols in reference order
	// and export all global-referenced labels. Not sure if this is
	// required but it's here nonetheless

	for(SYM * sy=sdecl; sy!=NULL; sy=sy->sdecl)
	{
		if ((sy->stype == LABEL)
			&& (sy->sattre & (EQUATEDREG | UNDEF_EQUR | EQUATEDCC | UNDEF_CC)) == 0
			&& ((sy->sattr & (GLOBAL | DEFINED)) == (GLOBAL | DEFINED)
			|| (sy->sattr & (GLOBAL | REFERENCED)) == (GLOBAL | REFERENCED))
			|| (sy->sattr & COMMON))
		{
			sy->senv = scount++;

			if (buf != NULL)
				buf = construct(buf, sy, 1);
		}
		else if ((sy->sattr == (GLOBAL | REFERENCED)) &&  (buf != NULL) && (sy->sattre & (EQUATEDREG | UNDEF_EQUR | EQUATEDCC | UNDEF_CC)) == 0)
		{
			buf = construct(buf, sy, 0); // <-- this creates a NON-global symbol...
			scount++;
		}
	}

	return scount;
}

//
// Helper function for dsp_lod_symbols
//
static uint16_t WriteLODSection(int section, uint16_t symbolCount)
{
	for(SYM * sy=sdecl; sy!=NULL; sy=sy->sdecl)
	{
		// Export vanilla labels (but don't make them global). An exception is
		// made for equates, which are not exported unless they are referenced.
		if (sy->stype == LABEL && lsym_flag
			&& (sy->sattr & (DEFINED | REFERENCED)) != 0
			&& (*sy->sname != '.')
			&& (sy->sattr & GLOBAL) == 0
			&& (sy->sattr & (section)))
		{
			sy->senv = symbolCount++;
			D_printf("%-19s   I %.6" PRIX64 "\n", sy->sname, sy->svalue);
		}
	}

	return symbolCount;
}

//
// Dump LOD style symbols into the passed in buffer
//
void DumpLODSymbols(void)
{
	D_printf("_SYMBOL P\n");
	uint16_t count = WriteLODSection(M56001P, 0);

	D_printf("_SYMBOL X\n");
	count = WriteLODSection(M56001X, count);

	D_printf("_SYMBOL Y\n");
	count = WriteLODSection(M56001Y, count);

	D_printf("_SYMBOL L\n");
	count = WriteLODSection(M56001L, count);

	// TODO: I've seen _SYMBOL N in there but no idea what symbols it needs...
	//D_printf("_SYMBOL N\n");
	//WriteLODSection(M56001?, count);
}

//
// Convert string to uppercase
//
void ToUppercase(uint8_t * s)
{
	for(; *s; s++)
	{
		if (*s >= 'a' && *s <= 'z')
			*s -= 0x20;
	}
}

//
// Generate symbol table for listing file
//
int symtable(void)
{
	int i;
	int j;
	SYM * q = NULL;
	SYM * p;
	SYM * r;
	SYM * k;
	SYM * colptr[4];
	char ln[1024];
	char ln1[1024];
	char ln2[20];
	char c, c1;
	WORD w;
	int ww;
	int colhei = pagelen - 5;

	// Allocate storage for list headers and partition all labels. Throw away
	// macros and macro arguments.
	SYM ** sy = (SYM **)malloc(128 * sizeof(SYM **));

	for(i=0; i<128; i++)
		sy[i] = NULL;

	for(i=0; i<NBUCKETS; i++)
	{
		for(p=symbolTable[i]; p!=NULL; p=k)
		{
			k = p->snext;
			j = *p->sname;
			r = NULL;

			// Ignore non-labels
			if ((p->stype != LABEL) || (p->sattre & UNDEF_EQUR))
				continue;

			for(q=sy[j]; q!=NULL; q=q->snext)
			{
				if (strcmp(p->sname, q->sname) < 0)
					break;

				r = q;
			}

			if (r == NULL)
			{
				// Insert at front of list
				p->snext = sy[j];
				sy[j] = p;
			}
			else
			{
				// Insert in middle or append to list
				p->snext = r->snext;
				r->snext = p;
			}
		}
	}

	// Link all symbols onto one list again
	p = NULL;

	for(i=0; i<128; ++i)
	{
		if ((r = sy[i]) != NULL)
		{
			if (p == NULL)
				q = r;
			else
				q->snext = r;

			while (q->snext != NULL)
				q = q->snext;

			if (p == NULL)
				p = r;
		}
	}

	eject();
	strcpy(subttl, "Symbol Table");

	while (p != NULL)
	{
		for(i=0; i<4; i++)
		{
			colptr[i] = p;

			for(j=0; j<colhei; j++)
			{
				if (p == NULL)
					break;
				else
					p = p->snext;
			}
		}

		for(i=0; i<colhei; i++)
		{
			*ln = EOS;

			if (colptr[0] == NULL)
				break;

			for(j=0; j<4; j++)
			{
				if ((q = colptr[j]) == NULL)
					break;

				colptr[j] = q->snext;
				w = q->sattr;
				ww = q->sattre;
				// Pick a tag:
				// c	common
				// x	external reference
				// g	global (export)
				// space	nothing special
				c1 = SPACE;
				c = SPACE;

				if (w & COMMON)
					c = 'c';
				else if ((w & (DEFINED | GLOBAL)) == GLOBAL)
					c = 'x';
				else if (w & GLOBAL)
					c = 'g';

				c1 = tdb_text[w & TDB];

				if (c == 'x')
					strcpy(ln2, "external");
				else
				{
					sprintf(ln2, "%016llX", q->svalue);
					ToUppercase(ln2);
				}

				sprintf(ln1, "  %16s %s %c%c%c", q->sname, ln2, (ww & EQUATEDREG) ? 'e' : SPACE, c1, c);
				strcat(ln, ln1);
			}

			ship_ln(ln);
		}

		eject();
	}

	return 0;
}

SYM * NewDebugSymbol(const uint8_t * str, uint8_t type, uint8_t other, uint16_t desc)
{
	SYM * symbol = NewSymbol(str, DBGSYM, 0);

	if (NULL == symbol)
		fatal("Could not allocate space for debug symbol");

	AddToSymbolDeclarationList(symbol);

	symbol->st_type = type;
	symbol->st_other = other;
	symbol->st_desc = desc;

	return symbol;
}

char *FilePath(const char * fname)
{
	char buf1[256];
	char * fpath;
	int i, j;

	if ((fpath = realpath(fname, NULL)) != NULL)
		return fpath;

	for(i=0; nthpath("RMACPATH", i, buf1)!=0; i++)
	{
		j = strlen(buf1);

		// Append path char if necessary
		if (j > 0 && buf1[j - 1] != SLASHCHAR)
			strcat(buf1, SLASHSTRING);

		strcat(buf1, fname);

		if ((fpath = realpath(buf1, NULL)) != NULL)
			return fpath;
	}

	return NULL;
}

static void GenFileSym(const char * fname, uint8_t type, uint32_t addr, uint32_t sattr)
{
	char *fpath;

	if (!(fpath = FilePath(fname)))
	{
		// Don't treat this as an error. Any file rmac can read is valid enough.
		// Just use the relative filename in place of an absolute path for the
		// debug information.
		fpath = strdup(fname);

		if (!fpath)
			fatal("Could not allocate memory for fake path name");
	}

	SYM * symbol = NewDebugSymbol(fpath, type, 0, 0);

	free(fpath);

	symbol->svalue = addr;
	symbol->sattr |= sattr;
}

void GenMainFileSym(const char * fname)
{
	GenFileSym(fname, 0x64 /* N_SO */, 0, DEFINED | TEXT);
}

void GenLineNoSym(void)
{
	uint32_t addr;
	uint32_t sattr;
	uint8_t type;
	SYM * symbol;

	static uint16_t prevlineno = -1;
	static uint32_t prevaddr = -1;
	static uint16_t prevfileno = 0;

	if (orgactive)
	{
		addr = orgaddr;
		sattr = ABS | DEFINED | EQUATED;
		// 0x4c is N_FLINE, function start/body/end line number, repurposed by
		// MADMAC/ALN for ABS line numbers.
		type = 0x4c;
	}
	else
	{
		addr = pcloc;
		sattr = DEFINED | cursect;
		type = 0x44; // N_SLINE, text section line number
	}

	if ((addr == prevaddr) || ((curlineno == prevlineno) && (prevfileno == cfileno)))
		return;

	prevaddr = addr;
	prevlineno = curlineno;

	if (prevfileno != cfileno)
		GenFileSym(curfname, 0x84 /* N_SOL */, addr, sattr);

	prevfileno = cfileno;

	/* MADMAC counts lines starting at 0. Offset curlineno accordingly */
	symbol = NewDebugSymbol(NULL, type, 0, curlineno - 1);

	symbol->svalue = addr;
	symbol->sattr |= sattr;
}
