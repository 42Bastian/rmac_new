//
// RMAC - Renamed Macro Assembler for all Atari computers
// DSP56K.H - General DSP56001 routines
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#ifndef __DSP56K_H__
#define __DSP56K_H__

#include "rmac.h"
#include "sect.h"

// Exported variables.
#define DSP_MAX_RAM (32*3*1024)			// 32k 24-bit words
#define DSP_ORG struct dsp56001_orgentry

enum MEMTYPES
{
	ORG_P,
	ORG_X,
	ORG_Y,
	ORG_L
} ;

DSP_ORG
{
	enum MEMTYPES memtype;
	uint8_t * start;
	uint8_t * end;
	uint32_t orgadr;
	CHUNK * chunk;
};

extern DSP_ORG dsp_orgmap[1024];		// Mark all 56001 org changes
extern DSP_ORG * dsp_currentorg;
extern int dsp_written_data_in_current_org;

#define D_printf(...) chptr += sprintf(chptr, __VA_ARGS__)

// Exported functions

#endif	// __DSP56K_H__
