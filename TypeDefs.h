#pragma once
/*
 *   Copyright (C) 2020 by Thomas A. Early N7TAE
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string>

#define CALL_SIZE 8

using SPORTIP = struct portip_tag {
	std::string ip;
	int port;
};

using SMOD = struct aprs_module {
	std::string call;   /* KJ4NHF-B */
	bool defined;
	std::string band;  /* 23cm ... */
	double frequency, offset, latitude, longitude, range, agl;
	std::string desc1, desc2, url, package_version;
};

using SRPTR = struct aprs_info {
	SPORTIP aprs;
	std::string aprs_filter;
	int aprs_hash;
	int aprs_interval;
	SMOD mod;
};

using SBANDTXT = struct band_txt_tag {
	unsigned short streamID;
	unsigned char flags[3];
	char lh_mycall[CALL_SIZE + 1];
	char lh_sfx[5];
	char lh_yrcall[CALL_SIZE + 1];
	char lh_rpt1[CALL_SIZE + 1];
	char lh_rpt2[CALL_SIZE + 1];
	time_t last_time;
	char txt[64];   // Only 20 are used
	unsigned short txt_cnt;
	bool sent_key_on_msg;

	std::string dest_rptr;

	// try to process GPS mode: GPRMC and ID
	char temp_line[256];
	unsigned short temp_line_cnt;
	char gprmc[256];
	char gpid[256];
	time_t gps_last_time;

	int num_dv_frames;
	int num_dv_silent_frames;
	int num_bit_errors;
};
