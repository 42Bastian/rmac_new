//
// RMAC - Renamed Macro Assembler for all Atari computers
// OBJECT.C - Writing Object Files
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "object.h"
#include "6502.h"
#include "direct.h"
#include "dsp56k.h"
#include "error.h"
#include "mark.h"
#include "riscasm.h"
#include "sect.h"
#include "symbol.h"
#include "version.h"

//#define DEBUG_ELF

uint32_t symsize = 0;			// Size of BSD/ELF symbol table
uint32_t strindx = 0x00000004;	// BSD/ELF string table index
uint8_t * strtable;				// Pointer to the symbol string table
uint8_t * objImage;				// Global object image pointer
int elfHdrNum[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
uint32_t extraSyms;

static uint16_t tdb_tab[] = {
	0,				// absolute
	AL_TEXT,		// TEXT segment based
	AL_DATA, 0,		// DATA segment based
	AL_BSS			// BSS segment based
};

uint32_t PRGFLAGS;	/* PRGFLAGS as defined in Atari Compendium Chapter 2
Definition		Bit(s)	Meaning
--------------- ------- --------------------------------------------------------
PF_FASTLOAD		0		If set, clear only the BSS area on program load,
						otherwise clear the entire heap.
PF_TTRAMLOAD	1		If set, the program may be loaded into alternative RAM,
						otherwise it must be loaded into standard RAM.
PF_TTRAMMEM		2		If set, the program's Malloc() requests may be satisfied
						from alternative RAM, otherwise they must be satisfied
						from standard RAM.
-				3		Currently unused
See left.		4 & 5	If these bits are set to 0 (PF_PRIVATE), the processes'
						entire memory space will be considered private
						(when memory protection is enabled).If these bits are
						set to 1 (PF_GLOBAL), the processes' entire memory space
						will be readable and writable by any process (i.e.
						global). If these bits are set to 2 (PF_SUPERVISOR), the
						processes' entire memory space will only be readable and
						writable by itself and any other process in supervisor
						mode.If these bits are set to 3 (PF_READABLE), the
						processes' entire memory space will be readable by any
						application but only writable by itself.
-				6-15 	Currently unused
*/

// Internal function prototypes
static void WriteLOD(void);
static void WriteP56(void);

//
// Add entry to symbol table (in ALCYON mode)
// If 'globflag' is 1, make the symbol global
// If in .PRG mode, adjust symbol values for fake link
//
uint8_t * AddSymEntry(register uint8_t * buf, SYM * sym, int globflag)
{
	// Copy symbol name to buffer (first 8 chars or less)
	register uint8_t * s = sym->sname;
	register int i;
	uint32_t extra = 0;

	for(i=0; i<8 && *s; i++)
		*buf++ = *s++;

	while (i++ < 8)
		*buf++ = '\0';

	register uint16_t w1 = sym->sattr;
	register uint16_t w = AL_DEFINED | tdb_tab[w1 & TDB];

	if (prg_flag == 3)
	{
		// Extended symbol - Check to see if symbol is larger than 8 characters
		// and write an extra 14 characters where the next symbol would be.
		// Modify the flag word for this
		if (*s)
		{
			//printf("%s '%i' - will write extended symbol\n", sym->sname,s[0]);
			uint8_t *buf2 = buf + 6;

			for(i=8; i<8+14 && *s; i++)
				*buf2++ = *s++;

			while (i++ < 8 + 14)
				*buf2++ = '\0';

			symsize += 14;
			w |= 0x48;
			extra = 14;
		}
	}

	//
	// Construct and deposit flag word
	//
	// o  all symbols are AL_DEFINED
	// o  install T/D/B/A base
	//   o  install 'equated'
	//   o  commons (COMMON) are AL_EXTERN, but not BSS
	// o  exports (DEFINED) are AL_GLOBAL
	// o  imports (~DEFINED) are AL_EXTERN
	//
	if (w1 & EQUATED)		// Equated
		w |= AL_EQUATED;

	if (w1 & COMMON)
	{
		w |= AL_EXTERN | AL_GLOBAL;	// Common symbol
		w &= ~AL_BSS;		// They're not BSS in Alcyon object files
	}

	if (w1 & DEFINED)
	{
		if (globflag)		// Export the symbol
			w |= AL_GLOBAL;
	}
	else
		w |= AL_EXTERN;		// Imported symbol

	SETBE16(buf, 0, w);
	buf += 2;
	register uint32_t z = (uint32_t)sym->svalue;

	if (prg_flag)			// Relocate value in .PRG segment
	{
		w1 &= DATA | BSS;

		if (w1)
			z += sect[TEXT].sloc;

		if (w1 & BSS)
			z += sect[DATA].sloc;
	}

	SETBE32(buf, 0, z);		// Deposit symbol value
	buf += 4;

	symsize += 14;
	buf += extra;

	return buf;
}

//
// Add an entry to the BSD symbol table
//
// From stab.def (https://sites.uclouvain.be/SystInfo/usr/include/bits/stab.def.html):
/*
_________________________________________________
| 00 - 1F are not dbx stab symbols              |
| In most cases, the low bit is the EXTernal bit|

| 00 UNDEF  | 02 ABS    | 04 TEXT   | 06 DATA   |
| 01  |EXT  | 03  |EXT  | 05  |EXT  | 07  |EXT  |

| 08 BSS    | 0A INDR   | 0C FN_SEQ | 0E WEAKA  |
| 09  |EXT  | 0B        | 0D WEAKU  | 0F WEAKT  |

| 10 WEAKD  | 12 COMM   | 14 SETA   | 16 SETT   |
| 11 WEAKB  | 13        | 15        | 17        |

| 18 SETD   | 1A SETB   | 1C SETV   | 1E WARNING|
| 19        | 1B        | 1D        | 1F FN     |
*/
uint8_t * AddBSDSymEntry(uint8_t * buf, SYM * sym, int globflag)
{
	chptr = buf;						// Point to buffer for depositing longs
	if (sym->sname)
	{
	D_long(strindx);					// Deposit the symbol string index
	}
    else
	{
		D_long(0);						// Deposit special NULL string index
	}

	uint16_t w1 = sym->sattr;			// Obtain symbol attributes
	uint32_t z = 0;						// Initialize resulting symbol flags

	if (sym->stype == DBGSYM)
	{
		// Debug symbols hard-code the a.out symbol type in the st_type field
		// and can include additional type-specific data in the a.out symbol
		// "other" and "description" fields, both packed into this same dword.
		z = sym->st_type << 24;
		z |= sym->st_other << 16;
		z |= sym->st_desc;
	}
	else
	{
		// Translate rmac symbol attributes to an a.out symbol type.
	if (w1 & EQUATED)
	{
		z = 0x02000000;					// Set equated flag
	}

		// If a symbol is both EQUd and flagged as TBD then we let the latter
		// take precedence. Otherwise the linker will not even bother trying to
		// relocate the address during link time.

	switch (w1 & TDB)
	{
	case TEXT: z = 0x04000000; break;	// Set TEXT segment flag
	case DATA: z = 0x06000000; break;	// Set DATA segment flag
	case BSS : z = 0x08000000; break;	// Set BSS segment flag
	}

	if (globflag)
		z |= 0x01000000;				// Set global flag if requested
	}

	D_long(z);							// Deposit symbol attribute
	z = sym->svalue;					// Obtain symbol value

	if (w1 & (DATA | BSS))
		z += sect[TEXT].sloc;			// If DATA or BSS add TEXT segment size

	if (w1 & BSS)
		z += sect[DATA].sloc;			// If BSS add DATA segment size

	D_long(z);							// Deposit symbol value
	if (sym->sname)
	{
	strcpy(strtable + strindx, sym->sname);
	strindx += strlen(sym->sname) + 1;	// Incr string index incl null terminate
	}
	buf += 12;							// Increment buffer to next record
	symsize += 12;						// Increment symbol table size

	return buf;
}

//
// Add entry to ELF symbol table; if `globflag' is 1, make the symbol global
//
uint8_t * AddELFSymEntry(uint8_t * buf, SYM * sym, int globflag)
{
	chptr = buf;
	ch_size = 0;
	D_long(strindx);		// st_name
	D_long(sym->svalue);	// st_value
	D_long(0);				// st_size
	uint8_t st_info = 0;

	register WORD w1 = sym->sattr;

	if (w1 & DEFINED)
	{
		if (globflag)		// Export the symbol
			st_info |= 16;  // STB_GLOBAL (1<<4)
	}
	else if (w1 & (GLOBAL | REFERENCED))
		st_info |= 16;

	D_byte(st_info);
	D_byte(0);				// st_other

	uint16_t st_shndx = SHN_ABS;	// Assume absolute (equated) number

	if (w1 & TEXT)
		st_shndx = elfHdrNum[ES_TEXT];
	else if (w1 & DATA)
		st_shndx = elfHdrNum[ES_DATA];
	else if (w1 & BSS)
		st_shndx = elfHdrNum[ES_BSS];
	else if (globflag && !(w1 & DEFINED) && (w1 & REFERENCED))
	{
		st_shndx = SHN_UNDEF;
	}		                       // If the symbol is global then probably we
                                    // don't need to do anything (probably)
                                    // since we set STB_GLOBAL in st_info above.
                                    // Unless we need to set it to SHN_COMMON?

	D_word(st_shndx);

	strcpy(strtable + strindx, sym->sname);
	strindx += strlen(sym->sname) + 1;	// Incr string index incl null terminate
	symsize += 0x10;					// Increment symbol table size

	return buf + 0x10;
}

//
// Helper function for ELF output
//
int DepositELFSectionHeader(uint8_t * ptr, uint32_t name, uint32_t type, uint32_t flags, uint32_t addr, uint32_t offset, uint32_t size, uint32_t link, uint32_t info, uint32_t addralign, uint32_t entsize)
{
	chptr = ptr;
	ch_size = 0;
	D_long(name);
	D_long(type);
	D_long(flags);
	D_long(addr);
	D_long(offset);
	D_long(size);
	D_long(link);
	D_long(info);
	D_long(addralign);
	D_long(entsize);
	return 40;
}

//
// Deposit an entry in the Section Header string table
//
uint32_t DepositELFSHSTEntry(uint8_t ** pTable, const uint8_t * s)
{
#ifdef DEBUG_ELF
printf("DepositELFSHSTEntry: s = \"%s\"\n", s);
#endif
	uint32_t strSize = strlen(s);
	strcpy(*pTable, s);
	*pTable += strSize + 1;
	return strSize + 1;
}

//
// Deposit a symbol table entry in the ELF Symbol Table
//
uint32_t DepositELFSymbol(uint8_t * ptr, uint32_t name, uint32_t addr, uint32_t size, uint8_t info, uint8_t other, uint16_t shndx)
{
	chptr = ptr;
	ch_size = 0;
	D_long(name);
	D_long(addr);
	D_long(size);
	*chptr++ = info;
	*chptr++ = other;
	D_word(shndx);
	return 16;
}

//
// Write an object file to the passed in file descriptor
// N.B.: Return value is ignored...
//
int WriteObject(int fd)
{
	LONG tds;				// TEXT & DATA segment size
	int i;					// Temporary int
	CHUNK * cp;				// Chunk (for gather)
	uint8_t * buf;			// Scratch area
	uint8_t * p;			// Temporary ptr
	LONG trsize, drsize;	// Size of relocations
	uint32_t unused;		// For supressing 'write' warnings

	if (verb_flag)
	{
		printf("TEXT segment: %d bytes\n", sect[TEXT].sloc);
		printf("DATA segment: %d bytes\n", sect[DATA].sloc);
		printf("BSS  segment: %d bytes\n", sect[BSS].sloc);
	}

	// Write requested object file...
	if ((obj_format == BSD) || ((obj_format == ALCYON) && (prg_flag == 0)))
	{
		ch_size = 0;

		// Force BSD format (if it was ALCYON format)
		obj_format = BSD;

		if (verb_flag)
		{
			printf("Total       : %d bytes\n", sect[TEXT].sloc + sect[DATA].sloc + sect[BSS].sloc);
		}

		AssignSymbolNos(NULL, NULL);	// Assign index numbers to the symbols
		tds = sect[TEXT].sloc + sect[DATA].sloc;	// Get size of TEXT and DATA segment
		buf = malloc(0x800000);			// Allocate 8MB object file image memory

		if (buf == NULL)
		{
			error("cannot allocate object file memory (in BSD mode)");
			return ERROR;
		}

		memset(buf, 0, 0x800000);		// Clear allocated memory
		objImage = buf;					// Set global object image pointer
		strtable = malloc(0x200000);	// Allocate 2MB string table buffer

		if (strtable == NULL)
		{
			free(buf);
			error("cannot allocate string table memory (in BSD mode)");
			return ERROR;
		}

		memset(strtable, 0, 0x200000);	// Clear allocated memory

		// Build object file header
		chptr = buf;					// Base of header (for D_foo macros)
		ch_size = 0;
		challoc = 0x800000;
		D_long(0x00000107);				// Magic number
		D_long(sect[TEXT].sloc);		// TEXT size
		D_long(sect[DATA].sloc);		// DATA size
		D_long(sect[BSS].sloc);			// BSS size
		D_long(0x00000000);				// Symbol size
		D_long(0x00000000);				// First entry (0L)
		D_long(0x00000000);				// TEXT relocation size
		D_long(0x00000000);				// DATA relocation size

		// Construct TEXT and DATA segments (without relocation changes)
		p = buf + BSDHDRSIZE;

		for(i=TEXT; i<=DATA; i++)
		{
			for(cp=sect[i].sfcode; cp!=NULL; cp=cp->chnext)
			{
				memcpy(p, cp->chptr, cp->ch_size);
				p += cp->ch_size;
			}
		}

		// Do relocation tables (and make changes to segment data)
		p = buf + BSDHDRSIZE + tds;		// Move obj image ptr to reloc info
		trsize = MarkBSDImage(p, tds, sect[TEXT].sloc, TEXT);// Do TEXT relocation table
		chptr = buf + 0x18;				// Point to relocation hdr entry
		D_long(trsize);					// Write the relocation table size

		// Move obj image ptr to reloc info
		p = buf + BSDHDRSIZE + tds + trsize;
		drsize = MarkBSDImage(p, tds, sect[TEXT].sloc, DATA);// Do DATA relocation table
		chptr = buf + 0x1C;				// Point to relocation hdr entry
		D_long(drsize);					// Write the relocation table size

		// Point to start of symbol table
		p = buf + BSDHDRSIZE + tds + trsize + drsize;
		AssignSymbolNos(p, AddBSDSymEntry);	// Build symbol and string tables
		chptr = buf + 0x10;				// Point to sym table size hdr entry
		D_long(symsize);				// Write the symbol table size

		// Point to string table
		p = buf + BSDHDRSIZE + tds + trsize + drsize + symsize;
		memcpy(p, strtable, strindx);	// Copy string table to object image
		chptr = p;						// Point to string table size long
		D_long(strindx);				// Write string table size

		// Write the BSD object file from the object image buffer
		unused = write(fd, buf, BSDHDRSIZE + tds + trsize + drsize + symsize + strindx + 4);

		if (verb_flag)
		{
			printf("TextRel size: %d bytes\n", trsize);
			printf("DataRel size: %d bytes\n", drsize);
		}

		if (buf)
		{
			free(strtable);				// Free allocated memory
			free(buf);					// Free allocated memory
		}
	}
	else if (obj_format == ALCYON)
	{
		ch_size = 0;

		if (verb_flag)
		{
			if (prg_flag)
				printf("TOS header  : 28 bytes\n");

			printf("Total       : %d bytes\n", sect[TEXT].sloc + sect[DATA].sloc + sect[BSS].sloc + (prg_flag ? 28 : 0));
		}

		// Assign index numbers to the symbols, get # of symbols (we assume
		// that all symbols can potentially be extended, hence the x28)
		// (To clarify: 28 bytes is the size of an extended symbol)
		uint32_t symbolMaxSize = AssignSymbolNos(NULL, NULL) * 28;

		// Alloc memory for header + text + data, symbol and relocation
		// information construction.
		tds = sect[TEXT].sloc + sect[DATA].sloc;
		buf = malloc(HDRSIZE + tds + symbolMaxSize);

		// Build object file header just before the text+data image
		chptr = buf;				// -> base of header
		ch_size = 0;
		challoc = HDRSIZE + tds + symbolMaxSize;
		D_word(0x601A);				// 00 - magic number
		D_long(sect[TEXT].sloc);	// 02 - TEXT size
		D_long(sect[DATA].sloc);	// 06 - DATA size
		D_long(sect[BSS].sloc); 	// 0A - BSS size
		D_long(0);					// 0E - symbol table size (filled later)
		D_long(0);					// 12 - stack size (unused)
		D_long(PRGFLAGS);			// 16 - PRGFLAGS
		D_word(0);					// 1A - relocation information exists

		// Construct text and data segments; fixup relocatable longs in .PRG
		// mode; finally write the header + text + data
		p = buf + HDRSIZE;

		for(i=TEXT; i<=DATA; i++)
		{
			for(cp=sect[i].sfcode; cp!=NULL; cp=cp->chnext)
			{
				memcpy(p, cp->chptr, cp->ch_size);
				p += cp->ch_size;
			}
		}

		// Do a first pass on the Alcyon image, if in PRG mode
		if (prg_flag)
			MarkImage(buf + HDRSIZE, tds, sect[TEXT].sloc, 0);

		// Construct symbol table and update the header entry, if necessary
		if (prg_flag > 1)
		{
			// AssignSymbolNos with AddSymEntry updates symsize (stays 0 otherwise)
			AssignSymbolNos(buf + HDRSIZE + tds, AddSymEntry);
			chptr = buf + 0x0E;			// Point to symbol table size entry
			D_long(symsize);

			if (verb_flag)
				printf("Symbol table: %d bytes\n", symsize);
		}

		// Write out the header + text & data + symbol table (if any)
		unused = write(fd, buf, HDRSIZE + tds + symsize);

		// Construct and write relocation information; the size of it changes if
		// we're writing a RELMODed executable. N.B.: Destroys buffer!
		tds = MarkImage(buf, tds, sect[TEXT].sloc, 1);
		unused = write(fd, buf, tds);
	}
	else if (obj_format == ELF)
	{
		// Allocate 6MB object file image memory
		buf = malloc(0x600000);

		if (buf == NULL)
		{
			error("cannot allocate object file memory (in ELF mode)");
			return ERROR;
		}

		memset(buf, 0, 0x600000);
		objImage = buf;					// Set global object image pointer
		strtable = malloc(0x200000);	// Allocate 2MB string table buffer

		if (strtable == NULL)
		{
			error("cannot allocate string table memory (in ELF mode)");
			return ERROR;
		}

		memset(strtable, 0, 0x200000);

		// This is pretty much a first pass at this shite, so there's room for
		// improvement. :-P
		uint8_t headers[4 * 10 * 10];	// (DWORD * 10) = 1 hdr, 10 entries
		int headerSize = 0;
		uint8_t shstrtab[128];			// The section header string table proper
		uint32_t shstTab[9];			// Index into shstrtab for strings
		uint8_t * shstPtr = shstrtab;	// Temp pointer
		uint32_t shstSize = 0;
		int numEntries = 4;				// There are always at *least* 4 sections
		int shstIndex = 1;				// The section where the shstrtab lives
		int elfSize = 0;				// Size of the ELF object
		// Clear the header numbers
		memset(elfHdrNum, 0, 9 * sizeof(int));

		//
		// First step is to see what sections need to be made; we also
		// construct the section header string table here at the same time.
		//
		shstTab[ES_NULL] = shstSize;
		shstSize += DepositELFSHSTEntry(&shstPtr, "");
		shstTab[ES_SHSTRTAB] = shstSize;
		shstSize += DepositELFSHSTEntry(&shstPtr, ".shstrtab");
		shstTab[ES_SYMTAB] = shstSize;
		shstSize += DepositELFSHSTEntry(&shstPtr, ".symtab");
		shstTab[ES_STRTAB] = shstSize;
		shstSize += DepositELFSHSTEntry(&shstPtr, ".strtab");

		if (sect[TEXT].sloc > 0)
		{
			elfHdrNum[ES_TEXT] = shstIndex;
			shstTab[ES_TEXT] = shstSize;
			shstSize += DepositELFSHSTEntry(&shstPtr, ".text");
			shstIndex++;
			numEntries++;
		}

		if (sect[DATA].sloc > 0)
		{
			elfHdrNum[ES_DATA] = shstIndex;
			shstTab[ES_DATA] = shstSize;
			shstSize += DepositELFSHSTEntry(&shstPtr, ".data");
			shstIndex++;
			numEntries++;
		}

		if (sect[BSS].sloc > 0)
		{
			elfHdrNum[ES_BSS] = shstIndex;
			shstTab[ES_BSS] = shstSize;
			shstSize += DepositELFSHSTEntry(&shstPtr, ".bss");
			shstIndex++;
			numEntries++;
		}

		if (sect[TEXT].relocs > 0)
		{
			elfHdrNum[ES_RELATEXT] = shstIndex;
			shstTab[ES_RELATEXT] = shstSize;
			shstSize += DepositELFSHSTEntry(&shstPtr, ".relaTEXT");
			shstIndex++;
			numEntries++;
		}

		if (sect[DATA].relocs > 0)
		{
			elfHdrNum[ES_RELADATA] = shstIndex;
			shstTab[ES_RELADATA] = shstSize;
			shstSize += DepositELFSHSTEntry(&shstPtr, ".relaDATA");
			shstIndex++;
			numEntries++;
		}

		elfHdrNum[ES_SHSTRTAB] = shstIndex + 0;
		elfHdrNum[ES_SYMTAB]   = shstIndex + 1;
		elfHdrNum[ES_STRTAB]   = shstIndex + 2;

#ifdef DEBUG_ELF
printf("ELF shstrtab size: %i bytes. Entries:\n", shstSize);
for(int j=0; j<i; j++)
	printf("\"%s\"\n", shstrtab + shstTab[j]);
#endif

		// Construct ELF header
		// If you want to make any sense out of this you'd better take a look
		// at Executable and Linkable Format on Wikipedia.
		chptr = buf;
		ch_size = 0;
		challoc = 0x600000;
		D_long(0x7F454C46); // 00 - "<7F>ELF" Magic Number
		D_byte(0x01); // 04 - 32 vs 64 (1 = 32, 2 = 64)
		D_byte(0x02); // 05 - Endianness (1 = LE, 2 = BE)
		D_byte(0x01); // 06 - Original version of ELF (set to 1)
		D_byte(0x00); // 07 - Target OS ABI (0 = System V)
		D_byte(0x00); // 08 - ABI Extra (unneeded)
		D_byte(0x00); // 09 - Pad bytes
		D_word(0x00);
		D_long(0x00);
		D_word(0x01); // 10 - ELF Type (1 = relocatable)
		D_word(0x04); // 12 - Architecture (EM_68K = 4, Motorola M68K family)
		D_long(0x01); // 14 - Version (1 = original ELF)
		D_long(0x00); // 18 - Entry point virtual address (unneeded)
		D_long(0x00); // 1C - Program header table offset (unneeded)
		D_long(0x00); // 20 - Section header table offset (to be determined)

		if (0)
		{
			// Specifically for 68000 CPU
			D_long(0x01000000) // 24 - Processor-specific flags - EF_M68K_M68000
		}
		else
		{
			// CPUs other than 68000 (68020...)
			D_long(0); // 24 - Processor-specific flags (ISA dependent)
		}

		D_word(0x0034); // 28 - ELF header size in bytes
		D_word(0); // 2A - Program header table entry size
		D_word(0); // 2C - Program header table entry count
		D_word(0x0028); // 2E - Section header entry size - 40 bytes for ELF32
		D_word(numEntries); // 30 - Section header table entry count
		D_word(shstIndex); // 32 - Section header string table index

		elfSize += 0x34;

		// Deposit section header 0 (NULL)
		headerSize += DepositELFSectionHeader(headers + headerSize, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

		int textLoc = elfSize;

		// Construct TEXT section, if any
		if (sect[TEXT].sloc > 0)
		{
			headerSize += DepositELFSectionHeader(headers + headerSize, shstTab[ES_TEXT], 1, 6, 0, elfSize, sect[TEXT].sloc, 0, 0, largestAlign[0], 0);

			for(CHUNK * cp=sect[TEXT].sfcode; cp!=NULL; cp=cp->chnext)
			{
				memcpy(buf + elfSize, cp->chptr, cp->ch_size);
				elfSize += cp->ch_size;
			}

			// Pad for next section (LONG boundary)
			elfSize = (elfSize + 3) & ~3;
		}

		int dataLoc = elfSize;

		// Construct DATA section, if any
		if (sect[DATA].sloc > 0)
		{
			headerSize += DepositELFSectionHeader(headers + headerSize, shstTab[ES_DATA], 1, 3, 0, elfSize, sect[DATA].sloc, 0, 0, largestAlign[1], 0);

			for(CHUNK * cp=sect[DATA].sfcode; cp!=NULL; cp=cp->chnext)
			{
				memcpy(buf + elfSize, cp->chptr, cp->ch_size);
				elfSize += cp->ch_size;
			}

			// Pad for next section (LONG boundary)
			elfSize = (elfSize + 3) & ~3;
		}

		// Construct BSS section, if any
		if (sect[BSS].sloc > 0)
		{
			headerSize += DepositELFSectionHeader(headers + headerSize, shstTab[ES_BSS], 8, 3, 0, elfSize, sect[BSS].sloc, 0, 0, largestAlign[2], 0);
		}

		int textrelLoc = headerSize;

		// Add headers for relocated sections, if any...
		if (sect[TEXT].relocs > 0)
			headerSize += DepositELFSectionHeader(headers + headerSize, shstTab[ES_RELATEXT], 4, 0x00, 0, 0, 0, elfHdrNum[ES_SYMTAB], elfHdrNum[ES_TEXT], 4, 0x0C);

		int datarelLoc = headerSize;

		if (sect[DATA].relocs > 0)
			headerSize += DepositELFSectionHeader(headers + headerSize, shstTab[ES_RELADATA], 4, 0x40, 0, 0, 0, elfHdrNum[ES_SYMTAB], elfHdrNum[ES_DATA], 4, 0x0C);

		// Add shstrtab
		headerSize += DepositELFSectionHeader(headers + headerSize, shstTab[ES_SHSTRTAB], 3, 0, 0, elfSize, shstSize, 0, 0, 1, 0);
		memcpy(buf + elfSize, shstrtab, shstSize);
		elfSize += shstSize;
		// Pad for next section (LONG boundary)
		elfSize = (elfSize + 3) & ~3;

		// Add section headers
		int headerLoc = elfSize;
		chptr = buf + 0x20;		// Set section header offset in ELF header
		D_long(headerLoc);
		elfSize += (4 * 10) * numEntries;

		// Add symbol table & string table
		int symtabLoc = elfSize;
		strindx = 0;	// Make sure we start at the beginning...
		elfSize += DepositELFSymbol(buf + elfSize, 0, 0, 0, 0, 0, 0);
		*strtable = 0;
		strindx++;
		extraSyms = 1;

		if (sect[TEXT].sloc > 0)
		{
			elfSize += DepositELFSymbol(buf + elfSize, 0, 0, 0, 3, 0, elfHdrNum[ES_TEXT]);
			extraSyms++;
		}

		if (sect[DATA].sloc > 0)
		{
			elfSize += DepositELFSymbol(buf + elfSize, 0, 0, 0, 3, 0, elfHdrNum[ES_DATA]);
			extraSyms++;
		}

		if (sect[BSS].sloc > 0)
		{
			elfSize += DepositELFSymbol(buf + elfSize, 0, 0, 0, 3, 0, elfHdrNum[ES_BSS]);
			extraSyms++;
		}

		int numSymbols = AssignSymbolNosELF(buf + elfSize, AddELFSymEntry);
		elfSize += numSymbols * 0x10;

		// String table
		int strtabLoc = elfSize;
		memcpy(buf + elfSize, strtable, strindx);
		elfSize += strindx;
		// Pad for next section (LONG boundary)
		elfSize = (elfSize + 3) & ~3;

		headerSize += DepositELFSectionHeader(headers + headerSize, shstTab[ES_SYMTAB], 2, 0, 0, symtabLoc, (numSymbols + extraSyms) * 0x10, shstIndex + 2, firstglobal + extraSyms, 4, 0x10);
		headerSize += DepositELFSectionHeader(headers + headerSize, shstTab[ES_STRTAB], 3, 0, 0, strtabLoc, strindx, 0, 0, 1, 0);

		// Add relocation tables, if any (no need to align after these, they're
		// already on DWORD boundaries)
		if (sect[TEXT].relocs > 0)
		{
			uint32_t textrelSize = CreateELFRelocationRecord(buf + elfSize, buf + textLoc, TEXT);
			// Deposit offset & size, now that we know them
			chptr = headers + textrelLoc + 0x10;
			D_long(elfSize);
			D_long(textrelSize);
			elfSize += textrelSize;
		}

		if (sect[DATA].relocs > 0)
		{
			uint32_t datarelSize = CreateELFRelocationRecord(buf + elfSize, buf + dataLoc, DATA);
			// Deposit offset & size, now that we know them
			chptr = headers + datarelLoc + 0x10;
			D_long(elfSize);
			D_long(datarelSize);
			elfSize += datarelSize;
		}

		// Copy headers into the object
		memcpy(buf + headerLoc, headers, headerSize);

		// Finally, write out the object
		unused = write(fd, buf, elfSize);

		// Free allocated memory
		if (buf)
		{
			free(buf);
			free(strtable);
		}
	}
	else if (obj_format == XEX)
	{
		// Just write the object file
		m6502obj(fd);
	}
	else if (obj_format == C64PRG)
	{
		// Just write the object file
		m6502c64(fd);
	}
	else if (obj_format == P56 || obj_format == LOD)
	{
		// Allocate 6MB object file image memory
		uint8_t * buf = malloc(0x600000);

		if (buf == NULL)
			return error("cannot allocate object file memory (in P56/LOD mode)");

		memset(buf, 0, 0x600000);		// Clear allocated memory

		// Iterate through DSP ram buffers
		chptr = buf;					// -> base of header
		ch_size = 0;
		challoc = 0x600000;

		if (obj_format == LOD)
			WriteLOD();
		else
			WriteP56();

		// Write all the things \o/
		unused = write(fd, buf, chptr - buf);

		if (buf)
			free(buf);
	}
	else if (obj_format == RAW)
	{
		if (!org68k_active && used_architectures & (!(M6502 | M56001P | M56001X | M56001Y | M56001L)))
			return error("cannot output absolute binary without a starting address (.org or command line)");

		if (used_architectures & M6502)
		{
			// Okay, this is not the best. But it'll have to do until we revamp things a bit with sections.
			// Basically we assume that if raw output is requested and 6502 mode was switched on, nobody
			// switched to other architectures. The combination doesn't make much sense anyway for now.
			m6502raw(fd);
			return 0;
		}

		// Alloc memory for text + data construction.
		tds = sect[TEXT].sloc + sect[DATA].sloc;
		buf = malloc(tds);
		chptr = buf;

		// Construct text and data segments; fixup relocatable longs;
		// finally write the text + data

		p = buf;
		objImage = buf;					// Set global object image pointer

		for(i=TEXT; i<=DATA; i++)
		{
			for(cp=sect[i].sfcode; cp!=NULL; cp=cp->chnext)
			{
				memcpy(p, cp->chptr, cp->ch_size);
				p += cp->ch_size;
			}
		}

		if (MarkABSImage(buf, tds, sect[TEXT].sloc, TEXT) != OK)  // Do TEXT relocation table
		{
			return ERROR;
		}
		if (MarkABSImage(buf, tds, sect[TEXT].sloc, DATA) != OK) // Do DATA relocation table
		{
			return ERROR;
		}

		// Write out the header + text & data + symbol table (if any)
		unused = write(fd, buf, tds);

	}
	return 0;
}

static void WriteLOD(void)
{
	D_printf("_START %s 0000 0000 0000 RMAC %01i.%01i.%01i\n\n", firstfname, MAJOR, MINOR, PATCH);

	for(DSP_ORG * l=&dsp_orgmap[0]; l<dsp_currentorg; l++)
	{
		if (l->end != l->start)
		{
			switch (l->memtype)
			{
			case ORG_P: D_printf("_DATA P %.4X\n", l->orgadr); break;
			case ORG_X: D_printf("_DATA X %.4X\n", l->orgadr); break;
			case ORG_Y: D_printf("_DATA Y %.4X\n", l->orgadr); break;
			case ORG_L: D_printf("_DATA L %.4X\n", l->orgadr); break;
			default:
				error("Internal error: unknown DSP56001 org'd section");
				return;
			}

			CHUNK * cp = l->chunk;
			uint8_t * p_chunk = l->start;
			uint8_t * p_chunk_end = p_chunk;
			uint32_t j = 0;

			while (p_chunk_end != l->end)
			{
				if (l->end < (cp->chptr + cp->ch_size) && l->end > cp->chptr)
				{
					// If the end of the section is inside the current chunk, just dump everything and stop
					p_chunk_end = l->end;
				}
				else
				{
					// If the end of the section is not inside the current chunk, just dump everything from the current chunk and move on to the next
					p_chunk_end = cp->chptr + cp->ch_size;
				}

				uint32_t count = (uint32_t)(p_chunk_end - p_chunk);

				for(uint32_t i=0; i<count; i+=3)
				{
					if ((j & 7) != 7)
					{
						D_printf("%.6X ", (((p_chunk[0] << 8) | p_chunk[1]) << 8) | p_chunk[2]);
					}
					else
					{
						D_printf("%.6X\n", (((p_chunk[0] << 8) | p_chunk[1]) << 8) | p_chunk[2]);
					}

					p_chunk += 3;
					j++;
				}

				cp = cp->chnext;        // Advance chunk

				if (cp != NULL)
					p_chunk = cp->chptr;    // Set dump pointer to start of this chunk
			}

			if ((j & 7) != 0)
				D_printf("\n");
		}
	}

	// Dump the symbol table into the buf
	DumpLODSymbols();

	D_printf("\n_END %.4X\n", dsp_orgmap[0].orgadr);
}

static void WriteP56(void)
{
	for(DSP_ORG * l=&dsp_orgmap[0]; l<dsp_currentorg; l++)
	{
		if (l->end == l->start)
			continue;

		if ((l->memtype < ORG_P) || (l->memtype > ORG_L))
		{
			error("Internal error: unknown DSP56001 org'd section");
			return;
		}

		CHUNK * cp = l->chunk;
		uint8_t * p_chunk = l->start;
		uint8_t * p_chunk_end = p_chunk;

		// Memory type (P, X, Y or L)
		D_dsp(l->memtype);

		// Chunk start address (in DSP words)
		D_dsp(l->orgadr);

		// Chunk length (in DSP words)
		// We'll fill this field after we write the chunk so we can calculate
		// how long it is (so if the chunk is split into different CHUNKs we
		// can deal with this during copy)
		uint8_t * p_buf_len = chptr;
		chptr += 3;

		// The chunk itself
		uint32_t chunk_size = 0;

		while (p_chunk_end != l->end)
		{
			if (l->end < (cp->chptr + cp->ch_size) && l->end > cp->chptr)
			{
				// If the end of the section is inside the current chunk, just
				// dump everything and stop
				p_chunk_end = l->end;
			}
			else
			{
				// If the end of the section is not inside the current chunk,
				// just dump everything from the current chunk and move on to
				// the next
				p_chunk_end = cp->chptr + cp->ch_size;
			}

			uint32_t current_chunk_size = p_chunk_end - p_chunk;
			chunk_size += current_chunk_size;
			memcpy(chptr, p_chunk, current_chunk_size);
			chptr += current_chunk_size;

			cp = cp->chnext;        // Advance chunk

			if (cp != NULL)
				p_chunk = cp->chptr;    // Set dump pointer to start of this chunk
		}

		// Now we can mark the chunk's length (DSP word size is 24-bits, so
		// the byte count needs to be divided by 3)
		SETBE24(p_buf_len, chunk_size / 3);
	}
}
