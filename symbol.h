//
// RMAC - Renamed Macro Assembler for all Atari computers
// SYMBOL.H - Symbol Handling
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __SYMBOL_H__
#define __SYMBOL_H__

#include <inttypes.h>

// Line lists
#define LLIST struct _llist
LLIST
{
	LLIST * next;
	uint8_t * line;
	int lineno;
};

// Symbols
#define SYM struct _sym
SYM
{
	SYM * snext;			// * -> Next symbol on hash-chain
	SYM * sorder;			// * -> Next sym in order of reference
	SYM * sdecl;			// * -> Next sym in order of declaration
	uint8_t stype;			// Symbol type
	uint16_t sattr;			// Attribute bits
	uint32_t sattre;		// Extended attribute bits
	uint16_t senv;			// Enviroment number
	uint64_t svalue;		// Symbol value (now 64-bit)
	uint8_t * sname;		// * -> Symbol's print-name
	LLIST * lineList;		// * -> Macro's linked list of lines
	LLIST * last;			// * -> end of macro linked list
	uint16_t cfileno;		// File the macro is defined in
	uint32_t uid;			// Symbol's unique ID
	uint8_t st_type;		// stabs debug symbol's "type" field
	uint8_t st_other;		// stabs debug symbol's "other" field
	uint16_t st_desc;		// stabs debug symbol's "description" field
};

// Exported variables
extern int curenv;
extern uint32_t firstglobal;// Index of the fist global symbol in an ELF object.

// Exported functions
SYM * lookup(uint8_t *, int, int);
void InitSymbolTable(void);
SYM * NewSymbol(const uint8_t *, int, int);
void AddToSymbolDeclarationList(SYM *);
void ForceUndefinedSymbolsGlobal(void);
int symtable(void);
uint32_t AssignSymbolNos(uint8_t *, uint8_t *(*)());
uint32_t AssignSymbolNosELF(uint8_t *, uint8_t *(*)());
void DumpLODSymbols(void);
uint8_t * GetSymbolNameByUID(uint32_t);
SYM * NewDebugSymbol(const uint8_t *, uint8_t, uint8_t, uint16_t);
void GenMainFileSym(const char *);
void GenLineNoSym(void);

// Helper to avoid unnecessary branches:
#define GENLINENOSYM() if (dsym_flag) GenLineNoSym()

#endif // __SYMBOL_H__

