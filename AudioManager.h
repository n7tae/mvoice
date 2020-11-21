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

using M17PacketQueue = CTQueue<SM17Frame>;
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
	void M17_2AudioMgr(const SM17Frame &m17);
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
	std::mutex audio_mutex, data_mutex;
	std::future<void> r1, r2, r3, p1, p2;
	bool link_open;

	// Unix sockets
	CUnixDgramWriter AM2M17, LogInput;
	// helpers
	CMainWindow *pMainWindow;
	CRandom random;
	std::vector<unsigned long> speak;
	CCRC crc;
	// methods
	bool audio_is_empty();
	bool codec_is_empty();
	void microphone2audioqueue();
	void audio2codec(const bool is_3200);
	void codec2audio(const bool is_3200);
	void codec2m17gateway(const std::string &dest, const std::string &sour, bool voiceonly);
	void play_audio_queue();
	void calc_audio_stats(const short int *audio = nullptr);
};
