//
// RMAC - Renamed Macro Assembler for all Atari computers
// OBJECT.H - Writing Object Files
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "rmac.h"

#define BSDHDRSIZE  0x20	// Size of BSD header
#define HDRSIZE     0x1C	// Size of Alcyon header

//
// Alcyon symbol flags
//
#define	AL_DEFINED	0x8000
#define	AL_EQUATED	0x4000
#define	AL_GLOBAL	0x2000
#define	AL_EQUREG	0x1000
#define	AL_EXTERN	0x0800
#define	AL_DATA		0x0400
#define	AL_TEXT		0x0200
#define	AL_BSS		0x0100
#define	AL_FILE		0x0080

enum ELFSectionNames
{
	ES_NULL, ES_TEXT, ES_DATA, ES_BSS, ES_RELATEXT, ES_RELADATA, ES_SHSTRTAB,
	ES_SYMTAB, ES_STRTAB
};

//
// ELF special section indices (field st_shndx)
// Lifted from glibc (https://sourceware.org/git/?p=glibc.git;a=blob;f=elf/elf.h)
//
#define SHN_UNDEF       0               /* Undefined section */
#define SHN_ABS         0xFFF1          /* Associated symbol is absolute */
#define SHN_COMMON      0xFFF2          /* Associated symbol is common */

// Exported variables.
extern uint8_t * objImage;
extern int elfHdrNum[];
extern uint32_t extraSyms;

// Exported functions
int WriteObject(int);

#endif // __OBJECT_H__

