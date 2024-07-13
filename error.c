//
// RMAC - Renamed Macro Assembler for all Atari computers
// ERROR.C - Error Handling
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "error.h"
#include <stdarg.h>
#include "token.h"
#include "listing.h"
char * interror_msg[] = {
	"Unknown internal error",	// Error not referenced, should not be displayed
	"Unknown internal error",	// Error not referenced, should not be displayed
	"Bad MULTX entry in chrtab",				// Error #2
	"Unknown internal error",	// Error not referenced, should not be displayed
	"Bad fixup type",							// Error #4
	"Bad operator in expression stream",		// Error #5
	"Can't find generated code in section",		// Error #6
	"Fixup (loc) out of range",					// Error #7
	"Absolute top filename found",				// Error #8
	"The RISC expression evaluator blew up, sorry" // Error #9
};

// Exported variables
int errcnt;						// Error count
char * err_fname;				// Name of error message file

// Internal variables
static long unused;				// For supressing 'write' warnings

//
// Report error if not at EOL
//
// N.B.: Since this should *never* happen, we can feel free to add whatever
//       diagnostics that will help in tracking down a problem to this function.
//
int ErrorIfNotAtEOL(void)
{
	if (*tok != EOL)
	{
		error("syntax error. expected EOL, found $%X ('%c')", *tok, *tok);
		printf("Token = ");
		DumpToken(*tok);
		printf("\n");
		DumpTokenBuffer();
	}

	return 0;
}

//
// Cannot create a file
//
void CantCreateFile(const char * fn)
{
	printf("Cannot create file: '%s'\n", fn);
	exit(1);
}

//
// Setup for error message
//  o  Create error listing file (if necessary)
//  o  Set current filename
//
void err_setup(void)
{
	char fnbuf[FNSIZ];

	if (err_fname != NULL)
	{
		strcpy(fnbuf, err_fname);

		if (*fnbuf == EOS)
			strcpy(fnbuf, firstfname);

		err_fname = NULL;

		if ((err_fd = open(fnbuf, _OPEN_FLAGS, _PERM_MODE)) < 0)
			CantCreateFile(fnbuf);

		err_flag = 1;
	}
}

//
// Display error message (uses printf() style variable arguments)
//
int error(const char * text, ...)
{
	char buf[EBUFSIZ];
	char buf1[EBUFSIZ];

	err_setup();

	va_list arg;
	va_start(arg, text);
	vsprintf(buf, text, arg);
	va_end(arg);

	if (listing > 0)
		ship_ln(buf);

	if (cur_inobj)
	{
		switch (cur_inobj->in_type)
		{
		case SRC_IFILE:
			sprintf(buf1, "%s %d: Error: %s\n", curfname, curlineno, buf);
			break;
		case SRC_IMACRO:
		{
			// This is basically SetFilenameForErrorReporting() but we don't
			// call it here as it will clobber curfname. That function is used
			// during fixups only so it really doesn't matter at that point...
			char * filename;
			FILEREC * fr;
			uint16_t fnum = cur_inobj->inobj.imacro->im_macro->cfileno;

			// Check for absolute top filename (this should never happen)
			if ((int16_t)fnum == -1)
				interror(8);
			else
			{
				fr = filerec;

				// Advance to the correct record...
				while (fr != NULL && fnum != 0)
				{
					fr = fr->frec_next;
					fnum--;
				}
			}
			// Check for file # record not found (this should never happen either)
			if (fr == NULL)
				interror(8);

			filename = fr->frec_name;

			sprintf(buf1, "%s %d: Error: %s\nCalled from: %s %d\n", filename, cur_inobj->inobj.imacro->im_macro->lineList->lineno, buf,
			curfname, curlineno);
		}
			break;
		case SRC_IREPT:
			sprintf(buf1, "%s %d: Error: %s\n", curfname, cur_inobj->inobj.irept->lineno, buf);
			break;
		}
	}
	else
		// No current file so cur_inobj is NULL
		sprintf(buf1, "%s %d: Error: %s\n", curfname, curlineno, buf);

	if (err_flag)
		unused = write(err_fd, buf1, (LONG)strlen(buf1));
	else
		printf("%s", buf1);

	taglist('E');
	errcnt++;

	return ERROR;
}

//
// Display warning message (uses printf() style variable arguments)
//
int warn(const char * text, ...)
{
	char buf[EBUFSIZ];
	char buf1[EBUFSIZ];

	err_setup();
	va_list arg;
	va_start(arg, text);
	vsprintf(buf, text, arg);
	va_end(arg);

	if (listing > 0)
		ship_ln(buf);

	sprintf(buf1, "%s %d: Warning: %s\n", curfname, curlineno, buf);

	if (err_flag)
		unused = write(err_fd, buf1, (LONG)strlen(buf1));
	else
		printf("%s", buf1);

	taglist('W');

	return OK;
}

int fatal(const char * s)
{
	char buf[EBUFSIZ];

	err_setup();

	if (listing > 0)
		ship_ln(s);

	sprintf(buf, "%s %d: Fatal: %s\n", curfname, curlineno, s);

	if (err_flag)
		unused = write(err_fd, buf, (LONG)strlen(buf));
	else
		printf("%s", buf);

	exit(1);
}

int interror(int n)
{
	char buf[EBUFSIZ];

	err_setup();
	sprintf(buf, "%s %d: Internal error #%d: %s\n", curfname, curlineno, n, interror_msg[n]);

	if (listing > 0)
		ship_ln(buf);

	if (err_flag)
		unused = write(err_fd, buf, (LONG)strlen(buf));
	else
		printf("%s", buf);

	exit(1);
}
