//
// RMAC - Renamed Macro Assembler for all Atari computers
// LISTING.H - Listing Output
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __LISTING_H__
#define __LISTING_H__

#include <time.h>
#include "rmac.h"

#define BOT_MAR         1				// #blank lines on bottom of page
#define IMAGESIZ        1024			// Max size of a line of text
#define TITLESIZ        1024			// Max size of a title
#define LN_COL          0				// Column for line numbers
#define LOC_COL         7				// Location ptr
#define DATA_COL        17				// Data start (for 20 chars, usually 16)
#define DATA_END        (DATA_COL+20)	// End+1th data column
#define TAG_COL         38				// Tag character
#define SRC_COL         40				// Source start

// Exported variables
extern char * list_fname;
extern int listing;
extern int pagelen;
extern int nlines;
extern LONG lsloc;
extern uint8_t subttl[];

// Exported functions
int eject(void);
uint32_t dos_date(void);
uint32_t dos_time(void);
void taglist(char);
void println(const char *);
void ship_ln(const char *);
void InitListing(void);
void listeol(void);
void lstout(char);
int listvalue(uint32_t);
int d_subttl(void);
int d_title(void);

#endif // __LISTING_H__

