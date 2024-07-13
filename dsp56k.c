//
// RMAC - Renamed Macro Assembler for all Atari computers
// DSP56K.C - General DSP56001 routines
// Copyright (C) 199x Landon Dyer, 2011-2024 Reboot and Friends
// RMAC derived from MADMAC v1.07 Written by Landon Dyer, 1986
// Source utilised with the kind permission of Landon Dyer
//

#include "rmac.h"
#include "dsp56k.h"

DSP_ORG dsp_orgmap[1024];		// Mark all 56001 org changes
DSP_ORG * dsp_currentorg = &dsp_orgmap[0];
int dsp_written_data_in_current_org = 0;

