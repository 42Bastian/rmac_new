//
// RMAC - Renamed Macro Assembler for the Atari Jaguar Console System
// DSP56L_MACH.C - Code Generation for Motorola DSP56001
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __DSP56KMACH_H__
#define __DSP56KMACH_H__

#include "rmac.h"
#include "dsp56k_amode.h"

// Exported variables
extern MNTABDSP dsp56k_machtab[];
extern unsigned int dsp_orgaddr;
extern unsigned int dsp_orgseg;

// Exported functions
extern int dsp_mult(LONG inst);

#endif // __DSP56KMACH_H__

