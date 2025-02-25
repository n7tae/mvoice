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
#include <future>
#include <atomic>
#include <mutex>
#include <vector>

#include "TemplateClasses.h"
#include "Packet.h"
#include "Random.h"
#include "UnixDgramSocket.h"
#include "CRC.h"

#ifdef USE44100
#include "Resampler.h"
#endif

using M17PacketQueue = CTQueue<SSMFrame>;
using SVolStats = struct volstats_tag
{
	int count, clip;
	double ss;
};

enum class E_PTT_Type { echo, m17 };

class CMainWindow;

class CAudioManager
{
public:
	CAudioManager();
	bool Init(CMainWindow *);

	void RecordMicThread(E_PTT_Type for_who, const std::string &urcall);
	void PlayEchoDataThread();	// for Echo
	void M17_2AudioMgr(const SSMFrame &m17);
	void KeyOff();
	void QuickKey(const std::string &dest, const std::string &sour);
	void Link(const std::string &linkcmd);

	// for volume stats
	SVolStats volStats;

private:
	// data
	std::atomic<bool> hot_mic, play_file;
	std::atomic<unsigned short> m17_sid_in;
	CAudioQueue audio_queue;
	CC2DataQueue c2_queue;
	std::future<void> mic2audio_fut, audio2codec_fut, codec2gateway_fut, codec2audio_fut, play_audio_fut;
	bool link_open;
#ifdef USE44100
	SDATA expand, shrink;
	float expand_in[160], expand_out[882];
	float shrink_in[882], shrink_out[160];
#endif

	// Unix sockets
	CUnixDgramWriter AM2M17, LogInput;
	// helpers
	CMainWindow *pMainWindow;
	CRandom random;
	std::vector<unsigned long> speak;
	CCRC crc;
#ifdef USE44100
	// the Rational Resamplers
	CResampler RSExpand, RSShrink;
#endif

	// methods
	void mic2audio();
	void audio2codec(const bool is_3200);
	void codec2audio(const bool is_3200);
	void codec2gateway(const std::string &dest, const std::string &sour, bool voiceonly);
	void play_audio();
	void calc_audio_stats(const short int *audio = nullptr);
};
