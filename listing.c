//
// RMAC - Renamed Macro Assembler for all Atari computers
// LISTING.C - Listing Output
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//
// --------------------------------------------------------------------------
// 0    0    1    1    2    2    3    3    4    4    5    5    6    6    7    7
// 012345678901234567890123456789012345678901234567890123456789012345678901234567
// filename....                         Renamed Macro Assembler N.N.NN (Unknown)
// nnnnn  aaaaaaaa  dddddddddddddddddddd T source code
// nnnnn  aaaaaaaa  dddddddddddddddd
// nnnnn           =vvvvvvvv

#include "listing.h"
#include "error.h"
#include "procln.h"
#include "sect.h"
#include "token.h"
#include "version.h"

char * list_fname;					// Listing filename
uint8_t subttl[TITLESIZ];			// Current subtitle
int listing;						// Listing level
int pagelen = 61;					// Lines on a page
int nlines;							// #lines on page so far
LONG lsloc;							// `sloc' at start of line

// Private
static int lcursect;				// `cursect' at start of line
static int llineno;					// `curlineno' at start of line
static int pageno;					// Current page number
static int pagewidth;				// #columns on a page
static int subflag;					// 0, don't do .eject on subttl (set 1)
static char lnimage[IMAGESIZ];		// Image of output line
static char title[TITLESIZ];		// Current title
static char datestr[20];			// Current date dd-mon-yyyy
static char timestr[20];			// Current time hh:mm:ss [am|pm]
static char buf[IMAGESIZ];			// Buffer for numbers
static long unused;					// For supressing 'write' warnings

static char * month[16] = {
	"",    "Jan", "Feb", "Mar",
	"Apr", "May", "Jun", "Jul",
	"Aug", "Sep", "Oct", "Nov",
	"Dec", "",    "",    ""
};


//
// Eject the page, reset the line count and bump the page number
//
int eject(void)
{
	if (listing > 0 && list_pag)
	{
		println("\f");
		nlines = 0;
	}

	return 0;
}


//
// Return GEMDOS format date
//
uint32_t dos_date(void)
{
	uint32_t v;
	struct tm * tm;
	time_t tloc;

	time(&tloc);
	tm = localtime(&tloc);
	v = ((tm->tm_year - 80) << 9) | ((tm->tm_mon + 1) << 5) | tm->tm_mday;

	return v;
}


//
// Return GEMDOS format time
//
uint32_t dos_time(void)
{
	uint32_t v;
	struct tm * tm;
	time_t tloc;

	time(&tloc);
	tm = localtime(&tloc);
	v = (tm->tm_hour << 11) | (tm->tm_min) << 5 | tm->tm_sec;

	return v;
}


//
// Generate a time string
//
void time_string(char * buf, uint32_t time)
{
	int hour;
	char * ampm;

	hour = (time >> 11);

	if (hour > 12)
	{
		hour -= 12;
		ampm = "pm";
	}
	else
		ampm = "am";

	sprintf(buf, "%d:%02d:%02d %s",
		hour, (int)((time >> 5) & 0x3F), (int)((time & 0x1F) << 1), ampm);
}


//
// Generate a date string
//
void date_string(char * buf, uint32_t date)
{
	sprintf(buf, "%d-%s-%d",
		(int)(date & 0x1F), month[(date >> 5) & 0xF], (int)((date >> 9) + 1980));
}


//
// Transform letters a-f in the address and data columns of the listing to
// uppercase. (People seem  to like uppercase hex better in assembly-language
// listings....)
//
void uc_ln(char * ln)
{
	int i;
	char j;

	for(i=LOC_COL; i<SRC_COL; ++i)
	{
		if ((j = ln[i]) >= 'a' && j <= 'f')
			ln[i] = (char)(j - 0x20);
	}
}


//
// Fill region 'dest' with 'len' characters 'c' and null terminate the region
//
void lnfill(char * dest, int len, char chr)
{
	while (len--)
		*dest++ = chr;

	*dest = EOS;
}


//
// Create listing file with the appropriate name
//
void list_setup(void)
{
	char fnbuf[FNSIZ];

	strcpy(fnbuf, list_fname);

	if (*fnbuf == EOS)
	{
		strcpy(fnbuf, firstfname);
		fext(fnbuf, ".prn", 1);
	}

	list_fname = NULL;

	if ((list_fd = open(fnbuf, _OPEN_FLAGS, _PERM_MODE)) < 0)
		CantCreateFile(fnbuf);
}


//
// Tag listing with a character, typically for errors or warnings
//
void taglist(char chr)
{
	lnimage[TAG_COL + 1] = chr;
}


//
// Print a line to the listing file
//
void println(const char * ln)
{
	unsigned int length;

	//  Create listing file, if necessary
	if (list_fname != NULL)
		list_setup();

	length = strlen(ln);
	unused = write(list_fd, ln, length);
	unused = write(list_fd, "\n", 1L);
}


//
// Ship line 'ln' out; do page breaks and title stuff
//
void ship_ln(const char * ln)
{
	// If listing level is <= 0, then don't print anything
	if (listing <= 0)
		return;

	if (list_pag)
	{
		// Notice bottom of page
		if (nlines >= pagelen - BOT_MAR)
			eject();

		// Print title, boilerplate, and subtitle at top of page
		if (nlines == 0)
		{
			pageno++;
			println("");
			date_string(datestr, dos_date());
			time_string(timestr, dos_time());
			sprintf(buf,
				"%-40s%-20s Page %-4d    %s %s        RMAC %01i.%01i.%02i (%s)",
				title, curfname, pageno, timestr, datestr, MAJOR, MINOR, PATCH,
				PLATFORM);
			println(buf);
			sprintf(buf, "%s", subttl);
			println(buf);
			println("");
			nlines = 4;
		}
	}

	println(ln);
	nlines++;
}


//
// Initialize listing generator
//
void InitListing(void)
{
	subflag = 0;
	pageno = 0;
	nlines = 0;
	pagelen = 61;
	pagewidth = 132;
	strcpy(title, "");
	strcpy(subttl, "");
	date_string(datestr, dos_date());
	time_string(timestr, dos_time());
}


//
// Listing EOL
//
void listeol(void)
{
	CHUNK * ch;
	char * p;
	int col;
	LONG count;
	int fixcount;

	DEBUG printf("~list: lsloc=$%X sloc=$%X\n", lsloc, sloc);

	if (lsloc != sloc)
	{
		sprintf(buf, "%08X", lsloc);
		strncpy(lnimage + LOC_COL, buf, 8);
	}

	if (llineno != curlineno)
	{
		sprintf(buf, "%5d", llineno);
		strncpy(lnimage + LN_COL, buf, 5);
	}

	// List bytes only when section stayed the same and the section is not a
	// "no-data" (SBSS) section. An extra annoyance is caused by "ds.b" in a
	// microprocessor mode, which prints out bytes of zero as if they had been
	// deposited with dcb. The fix (kludge) is an extra variable which records
	// the fact that a 'ds.x' directive generated all the data, and it
	// shouldn't be listed
	SaveSection();		// Update section variables

	if (lcursect == cursect && (sect[lcursect].scattr & SBSS) == 0
		&& lsloc != sloc && just_bss == 0)
	{
		ch = sect[lcursect].sfcode;

		if (lcursect != M6502)
        {
		for(; ch!=NULL; ch=ch->chnext)
		{
			if (lsloc >= ch->chloc && lsloc < (ch->chloc + ch->ch_size))
				break;
		}
        }

		// Fatal: Can't find chunk holding code
		if (ch == NULL)
		{
nochunk:
			interror(6);	// Can't find generated code in section
		}

		p =  ch->chptr + (lsloc - ch->chloc);
		col = DATA_COL;
		fixcount = 0;

		for(count=sloc-lsloc; count--; col+=2, ++lsloc)
		{
			if (col >= DATA_END)
			{
				// Ship the line
				col = DATA_COL;
				uc_ln(lnimage);
				ship_ln(lnimage);
				lnfill(lnimage, SRC_COL, SPACE);	// Fill with spaces
				sprintf(buf, "%08X", lsloc);
				strncpy(lnimage + LOC_COL, buf, 8);
			}

			if (lcursect != M6502 &&
                    lsloc >= (ch->chloc + ch->ch_size))
			{
				if ((ch = ch->chnext) == NULL)
					goto nochunk;

				p = ch->chptr;
			}

			if (!fixcount)
				fixcount = fixtest(lcursect, lsloc);

			if (fixcount)
			{
				fixcount--;
				strcpy(buf, "xx");
				p++;		// Advance anyway
			}
			else
				sprintf(buf, "%02x", *p++ & 0xff);

			strncpy(lnimage + col, buf, 2);
		}

		// Flush partial line
		if (col > DATA_COL)
		{
			uc_ln(lnimage);
			ship_ln(lnimage);
		}
	}
	else
	{
		uc_ln(lnimage);
		ship_ln(lnimage);
	}
}


//
// Copy current (saved) line to output buffer and tag it with a character
//
void lstout(char tag)
{
	char * s;
	char * d;

	lsloc = sloc;
	lcursect = cursect;
	llineno = curlineno;

	lnfill(lnimage, SRC_COL, SPACE);	// Fill with spaces
	lnimage[TAG_COL] = tag;

	// Copy line image and handle control characters
	d = lnimage + SRC_COL;

	for(s=lnbuf; *s; ++s)
	{
		if (*s >= 0x20 || *s == '\t')
			*d++ = *s;
		else
		{
			*d++ = '^';
			*d++ = (char)(*s + 0x40);
		}
	}

	*d++ = EOS;
}


//
// Output a value to listing
//
int listvalue(uint32_t v)
{
	sprintf(buf, "=%08X", v);
	strncpy(lnimage + DATA_COL - 1, buf, 9);
	return 0;
}


/*
 *  .subttl [-] "string"
 *
 *  Set subtitle;
 *    o  leading '-' supresses page eject
 *    o  don't .eject on first .subttl, but do so on all other ones,
 *    o  arrange not to print the .subttl directive
 *
 */
int d_subttl(void)
{
	int ejectok;
	ejectok = 1;

	if (*tok == '-')
	{
		ejectok = 0;
		++tok;
	}

	if (*tok != STRING)
		return error("missing string");

//	strcpy(subttl, (char *)tok[1]);
	strcpy(subttl, string[tok[1]]);

	tok += 2;

	// Always eject on pages 2+
	if (ejectok && (subflag || pageno > 1))
		eject();

	subflag = 1;

	return 0;
}


//
// Set title on titles not on the first page, do an eject and clobber the
// subtitle
//
int d_title(void)
{
	if (*tok != STRING)
		return error("missing string");

//	strcpy(title, (char*)tok[1]);
	strcpy(title, string[tok[1]]);
	tok += 2;

	if (pageno > 1)
	{
		strcpy(subttl, "");
		eject();
	}

	return 0;
}

