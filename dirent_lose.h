#ifndef __DIRENT_LOSE_H__
#define __DIRENT_LOSE_H__

#if defined(WIN32) || defined(WIN64)

// Microsoft™ Windows™ dependent code

// This is only needed to keep the compiler from throwing a fit, it's not meant
// to be used except as a sentinel (NULL/non-NULL).
typedef struct { int i; /* dummy member */ } DIR;

// These are the only two functions used from <dirent.h>
DIR * opendir(const char *);
int closedir(DIR *);

#endif

#endif	// __DIRENT_LOSE_H__
