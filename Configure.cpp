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


#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>

#include "Configure.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp/"
#endif

void CConfigure::SetDefaultValues()
{
	// M17
	data.sM17SourceCallsign.clear();
	data.bVoiceOnlyEnable = true;
	// mode and module
	data.eNetType = EInternetType::ipv4only;
	data.cModule = 'D';
	// audio
	data.sAudioIn.assign("default");
	data.sAudioOut.assign("default");
}

void CConfigure::ReadData()
{
	std::string path(CFG_DIR);
	path.append("mvoice.cfg");

	std::ifstream cfg(path.c_str(), std::ifstream::in);
	if (! cfg.is_open()) {
		SetDefaultValues();
		return;
	}

	char line[128];
	while (cfg.getline(line, 128)) {
		char *key = strtok(line, "=");
		if (!key)
			continue;	// skip empty lines
		if (0==strlen(key) || '#'==*key)
			continue;	// skip comments
		char *val = strtok(NULL, "\t\r");
		if (!val)
			continue;
		if (*val == '\'')	// value is a quoted string
			val = strtok(val, "'");
		if (! val)
			continue;

		if (0 == strcmp(key, "InternetType")) {
			if (0 == strcmp(val, "IPv6"))
				data.eNetType = EInternetType::ipv6only;
			else if (0 == strcmp(val, "Dual"))
				data.eNetType = EInternetType::dualstack;
			else
				data.eNetType = EInternetType::ipv4only;
		} else if (0 == strcmp(key, "Module")) {
			data.cModule = *val;
		} else if (0 == strcmp(key, "AudioInput")) {
			data.sAudioIn.assign(val);
		} else if (0 == strcmp(key, "AudioOutput")) {
			data.sAudioOut.assign(val);
		} else if (0 == strcmp(key, "M17SourceCallsign")) {
			data.sM17SourceCallsign.assign(val);
		} else if (0 == strcmp(key, "M17VoiceOnly")) {
			data.bVoiceOnlyEnable = IS_TRUE(*val);
		}
	}
	cfg.close();
}

void CConfigure::WriteData()
{

	std::string path(CFG_DIR);
	path.append("mvoice.cfg");

	// directory exists, now make the file
	std::ofstream file(path.c_str(), std::ofstream::out | std::ofstream::trunc);
	if (! file.is_open()) {
		std::cerr << "ERROR: could not open " << path << '!' << std::endl;
		return;
	}
	file << "#Generated Automatically, DO NOT MANUALLY EDIT!" << std::endl;
	// M17
	file << "M17SourceCallsign='" << data.sM17SourceCallsign << "'" << std::endl;
	file << "M17VoiceOnly=" << (data.bVoiceOnlyEnable ? "true" : "false") << std::endl;
	// mode and module
	file << "InternetType=";
	if (data.eNetType == EInternetType::ipv6only)
		file << "IPv6";
	else if (data.eNetType == EInternetType::dualstack)
		file << "Dual";
	else
		file << "IPv4";
	file << std::endl;
	file << "Module=" << data.cModule << std::endl;
	// audio
	file << "AudioInput='" << data.sAudioIn << "'" << std::endl;
	file << "AudioOutput='" << data.sAudioOut << "'" << std::endl;

	file.close();
}

void CConfigure::CopyFrom(const CFGDATA &from)
{
	// M17
	data.sM17SourceCallsign.assign(from.sM17SourceCallsign);
	data.bVoiceOnlyEnable = from.bVoiceOnlyEnable;
	// mode and module
	data.eNetType = from.eNetType;
	data.cModule = from.cModule;
	// audio
	data.sAudioIn.assign(from.sAudioIn);
	data.sAudioOut.assign(from.sAudioOut);
}

void CConfigure::CopyTo(CFGDATA &to)
{
	// M17
	to.sM17SourceCallsign.assign(data.sM17SourceCallsign);
	to.bVoiceOnlyEnable = data.bVoiceOnlyEnable;
	// mode and module
	to.eNetType = data.eNetType;
	to.cModule = data.cModule;
	// audio
	to.sAudioIn.assign(data.sAudioIn);
	to.sAudioOut.assign(data.sAudioOut);
}

bool CConfigure::IsOkay()
{
	bool audio = (data.sAudioIn.size()>0 && data.sAudioOut.size()>0);
	bool module = isalpha(data.cModule);
	bool src = (data.sM17SourceCallsign.size() > 2);
	return (audio && src && module);
}

const CFGDATA *CConfigure::GetData()
{
	return &data;
}
