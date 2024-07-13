//
// RMAC - Renamed Macro Assembler for all Atari computers
// EAGEN.C - Effective Address Code Generation
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "eagen.h"
#include "amode.h"
#include "error.h"
#include "fltpoint.h"
#include "mach.h"
#include "mark.h"
#include "riscasm.h"
#include "sect.h"
#include "token.h"

#define eaNgen    ea0gen
#define amN       am0
#define aNexattr  a0exattr
#define aNexval   a0exval
#define aNexpr    a0expr
#define aNixreg   a0ixreg
#define aNixsiz   a0ixsiz
#define AnESYM    a0esym
#define aNexten   a0extension
#define aNbexpr   a0bexpr
#define aNbdexval a0bexval
#define aNbdexattr a0bexattr
#include "eagen0.c"

#define eaNgen    ea1gen
#define amN       am1
#define aNexattr  a1exattr
#define aNexval   a1exval
#define aNexpr    a1expr
#define aNixreg   a1ixreg
#define aNixsiz   a1ixsiz
#define AnESYM    a1esym
#define aNexten   a1extension
#define aNbexpr   a1bexpr
#define aNbdexval a1bexval
#define aNbdexattr a1bexattr
#include "eagen0.c"
