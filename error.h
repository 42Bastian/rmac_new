//
// RMAC - Renamed Macro Assembler for all Atari computers
// ERROR.H - Error Handling
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __ERROR_H__
#define __ERROR_H__

#include "rmac.h"

#define EBUFSIZ     256		// Max size of an error message

// Exported variables
extern int errcnt;
extern char * err_fname;

// Exported functions
int error(const char *, ...);
int warn(const char *, ...);
int fatal(const char *);
int interror(int);
void CantCreateFile(const char *);
void err_setup(void);
int ErrorIfNotAtEOL(void);

#endif // __ERROR_H__

