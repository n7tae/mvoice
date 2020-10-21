/*
 *   Copyright (c) 2019-2020 by Thomas A. Early N7TAE
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

#pragma once

#include <string>

#define IS_TRUE(a) ((a)=='t' || (a)=='T' || (a)=='1')

enum class EInternetType { ipv4only, ipv6only, dualstack };

using CFGDATA = struct CFGData_struct {
	std::string sAudioIn, sAudioOut, sM17SourceCallsign;
	bool bVoiceOnlyEnable;
	EInternetType eNetType;
	char cModule;
};

class CConfigure {
public:
	CConfigure() { ReadData(); }

	void ReadData();
	void WriteData();
	void CopyFrom(const CFGDATA &);
	void CopyTo(CFGDATA &);
	bool IsOkay();
	const CFGDATA *GetData();

private:
	// data
	CFGDATA data;
	// methods
	void SetDefaultValues();
};
