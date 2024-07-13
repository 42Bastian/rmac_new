//
// RMAC - Renamed Macro Assembler for all Atari computers
// EXPR.H - Expression Analyzer
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __EXPR_H__
#define __EXPR_H__

#include "rmac.h"

// Tunable definitions
#define STKSIZE      64		// Size of expression stacks
#define EVSTACKSIZE  64		// Expression evaluator stack size

// Token classes in order of precedence
#define END          0		// End/beginning of input
#define ID           1		// Symbol or constant
#define OPAR         2		// (
#define CPAR         3		// )
#define SUNARY       4		// Special unary (^^defined, etc.)
#define UNARY        5		// Unary class: ! ~ -
#define MULT         6		// Multiplicative class: * / %
#define ADD          7		// Additive class: + -
#define SHIFT        8		// Shift class: << >>
#define REL          9		// Relational class: <= >= < > <> = !=
#define AND          10		// Bitwise and: &
#define XOR          11		// Bitwise xor: ^
#define OR           12		// Bitwise or: |

// Exported functions
void InitExpression(void);
int expr1(void);
int expr2(void);
int expr(TOKEN *, uint64_t *, WORD *, SYM **);
int evexpr(TOKEN *, uint64_t *, WORD *, SYM **);
uint16_t ExpressionLength(TOKEN *);

#endif // __EXPR_H__

