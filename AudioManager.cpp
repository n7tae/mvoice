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

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#include <netinet/in.h>

#include <iostream>
#include <fstream>

#include "MainWindow.h"
#include "AudioManager.h"
#include "Configure.h"
#include "codec2.h"
#include "Callsign.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp/"
#endif

CAudioManager::CAudioManager() : hot_mic(false), play_file(false), m17_sid_in(0U)
{
	link_open = true;
	volStats.count = 0;
}

bool CAudioManager::Init(CMainWindow *pMain)
{
	pMainWindow = pMain;
	std::string index(CFG_DIR);

	AM2M17.SetUp("am2m17");
	LogInput.SetUp("log_input");
	return false;
}


void CAudioManager::RecordMicThread(E_PTT_Type for_who, const std::string &urcall)
{
	auto data = pMainWindow->cfg.GetData();
	hot_mic = true;

	r1 = std::async(std::launch::async, &CAudioManager::microphone2audioqueue, this);

	r2 = std::async(std::launch::async, &CAudioManager::audio2codec, this, data->bVoiceOnlyEnable);

	if (for_who == E_PTT_Type::m17) {
		r3 = std::async(std::launch::async, &CAudioManager::codec2m17gateway, this, urcall, data->sM17SourceCallsign, data->bVoiceOnlyEnable);
	}
}

void CAudioManager::audio2codec(const bool is_3200)
{
	CCodec2 c2(is_3200);
	bool last;
	calc_audio_stats();  // initialize volume statistics
	bool is_odd = false; // true if we've processed an odd number of audio frames
	do {
		while ( audio_is_empty() )
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
		audio_mutex.lock();
		CAudioFrame audioframe = audio_queue.Pop();
		audio_mutex.unlock();
		calc_audio_stats(audioframe.GetData());
		last = audioframe.GetFlag();
		if ( is_3200 ) {
			is_odd = ! is_odd;
			unsigned char data[8];
			c2.codec2_encode(data, audioframe.GetData());
			CC2DataFrame dataframe(data);
			dataframe.SetFlag(is_odd ? false : last);
			data_mutex.lock();
			c2_queue.Push(dataframe);
			data_mutex.unlock();
			if (is_odd && last) { // we need an even number of data frame for 3200
				// add one more quite frame
				const short quiet[160] = { 0 };
				c2.codec2_encode(data, quiet);
				CC2DataFrame frame2(data);
				frame2.SetFlag(true);
				data_mutex.lock();
				c2_queue.Push(frame2);
				data_mutex.unlock();
			}
		} else { // 1600 - we need 40 ms of audio
			short audio[320] = { 0 }; // initialize to 40 ms of silence
			unsigned char data[8];
			memcpy(audio, audioframe.GetData(), 160*sizeof(short)); // we'll put 20 ms of audio at the beginning
			if (last) { // get another frame, if available
				volStats.count += 160; // a quite frame will only contribute to the total count
			} else {
				while (audio_is_empty()) // now we'll get another 160 sample frame
					std::this_thread::sleep_for(std::chrono::milliseconds(3));
				audio_mutex.lock();
				audioframe = audio_queue.Pop();
				audio_mutex.unlock();
				calc_audio_stats(audioframe.GetData());
				memcpy(audio+160, audioframe.GetData(), 160*sizeof(short));	// now we have 40 ms total
				last = audioframe.GetFlag();
			}
			c2.codec2_encode(data, audio);
			CC2DataFrame dataframe(data);
			dataframe.SetFlag(last);
			data_mutex.lock();
			c2_queue.Push(dataframe);
			data_mutex.unlock();
		}
	} while (! last);
}

void CAudioManager::QuickKey(const std::string &d, const std::string &s)
{
	hot_mic = true;
	SM17Frame frame;
	CCallsign dest(d), sour(s);
	memcpy(frame.magic, "M17 ", 4);
	frame.streamid = random.NewStreamID();
	dest.CodeOut(frame.lich.addr_dst);
	sour.CodeOut(frame.lich.addr_src);
	frame.SetFrameType(0x5u);
	memset(frame.lich.nonce, 0, 14);
	const uint8_t quiet[] = { 0x01u, 0x00u, 0x09u, 0x43u, 0x9cu, 0xe4u, 0x21u, 0x08u };
	memcpy(frame.payload,     quiet, 8);
	memcpy(frame.payload + 8, quiet, 8);
	for (uint16_t i=0; i<5; i++) {
		frame.SetFrameNumber((i < 4) ? i : i & 0x8000u);
		frame.SetCRC(crc.CalcCRC(frame));
		AM2M17.Write(frame.magic, sizeof(SM17Frame));
	}
	hot_mic = false;
}

void CAudioManager::codec2m17gateway(const std::string &dest, const std::string &sour, bool voiceonly)
{
	CCallsign destination(dest);
	CCallsign source(sour);

	// make most of the M17 IP frame
	// TODO: nonce and encryption and more TODOs mentioned later...
	SM17Frame ipframe;
	memcpy(ipframe.magic, "M17 ", 4);
	ipframe.streamid = random.NewStreamID(); // no need to htons because it's just a random id
	ipframe.SetFrameType(voiceonly ? 0x5U : 0x7U);
	destination.CodeOut(ipframe.lich.addr_dst);
	source.CodeOut(ipframe.lich.addr_src);

	unsigned int count = 0;
	bool last;
	do {
		while (codec_is_empty())
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		data_mutex.lock();
		CC2DataFrame cframe = c2_queue.Pop();
		data_mutex.unlock();
		last = cframe.GetFlag();
		memcpy(ipframe.payload, cframe.GetData(), 8);
		if (voiceonly) {
			if (last) {
				// we should never get here, but just in case...
				std::cerr << "WARNING: unexpected end of 3200 voice stream!" << std::endl;
				const uint8_t quiet[] = { 0x00u, 0x01u, 0x43u, 0x09u, 0xe4u, 0x9cu, 0x08u, 0x21u };	// for 3200 only!
				memcpy(ipframe.payload+8, quiet, 8);
			} else {
				// fill in the second part of the payload for C2 3200
				while (codec_is_empty())
					std::this_thread::sleep_for(std::chrono::milliseconds(2));
				data_mutex.lock();
				cframe = c2_queue.Pop();
				data_mutex.unlock();
				last = cframe.GetFlag();
				memcpy(ipframe.payload+8, cframe.GetData(), 8);
			}
		}
		// TODO: do something with the 2nd half of the payload when it's voice + "data"

		uint16_t fn = count++ % 0x8000u;
		if (last)
			fn |= 0x8000u;
		ipframe.SetFrameNumber(fn);

		// TODO: calculate crc

		AM2M17.Write(ipframe.magic, sizeof(SM17Frame));
	} while (! last);
}

void CAudioManager::microphone2audioqueue()
{
	auto data = pMainWindow->cfg.GetData();
	// Open PCM device for recording (capture).
	snd_pcm_t *handle;
	int rc = snd_pcm_open(&handle, data->sAudioIn.c_str(), SND_PCM_STREAM_CAPTURE, 0);
	if (rc < 0) {
		std::cerr << "unable to open pcm device: " << snd_strerror(rc) << std::endl;
		return;
	}
	// Allocate a hardware parameters object.
	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);

	// Fill it in with default values.
	snd_pcm_hw_params_any(handle, params);

	// Set the desired hardware parameters.

	// Interleaved mode
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

	// Signed 16-bit little-endian format
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

	// One channels (mono)
	snd_pcm_hw_params_set_channels(handle, params, 1);

	// 8000 samples/second
	snd_pcm_hw_params_set_rate(handle, params, 8000, 0);

	// Set period size to 40 ms for M17, 20 ms for AMBE.
	snd_pcm_uframes_t frames = 160;
	snd_pcm_hw_params_set_period_size(handle, params, frames, 0);

	// Write the parameters to the driver
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0) {
		std::cerr << "unable to set hw parameters: " << snd_strerror(rc) << std::endl;
		return;
	}

	bool keep_running;
	do {
		short int audio_buffer[frames];
		rc = snd_pcm_readi(handle, audio_buffer, frames);
		if (rc == -EPIPE) {
			// EPIPE means overrun
			std::cerr << "overrun occurred" << std::endl;
			snd_pcm_prepare(handle);
		} else if (rc < 0) {
			std::cerr << "error from readi: " << snd_strerror(rc) << std::endl;
		} else if (rc != int(frames)) {
			std::cerr << "short readi, read " << rc << " frames" << std::endl;
		}
		keep_running = hot_mic;
		CAudioFrame frame(audio_buffer);
		frame.SetFlag(! keep_running);
		audio_mutex.lock();
		audio_queue.Push(frame);
		audio_mutex.unlock();
	} while (keep_running);
	snd_pcm_drop(handle);
	snd_pcm_close(handle);
}

void CAudioManager::codec2audio(const bool is_3200)
{
	CCodec2 c2(is_3200);
	bool last;
	calc_audio_stats(); // init volume stats
	do {
		while (codec_is_empty())
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
		data_mutex.lock();
		CC2DataFrame dataframe = c2_queue.Pop();
		data_mutex.unlock();
		last = dataframe.GetFlag();
		if (is_3200) {
			short audio[160];
			c2.codec2_decode(audio, dataframe.GetData());
			CAudioFrame audioframe(audio);
			audioframe.SetFlag(last);
			audio_mutex.lock();
			audio_queue.Push(audioframe);
			audio_mutex.unlock();
			calc_audio_stats(audio);
		} else {
			short audio[320];	// C2 1600 is 40 ms audio
			c2.codec2_decode(audio, dataframe.GetData());
			CAudioFrame audio1(audio), audio2(audio+160);
			audio1.SetFlag(false);
			audio2.SetFlag(last);
			audio_mutex.lock();
			audio_queue.Push(audio1);
			audio_queue.Push(audio2);
			audio_mutex.unlock();
			calc_audio_stats(audio);
			calc_audio_stats(audio+160);
		}
	} while (! last);
}

void CAudioManager::PlayEchoDataThread()
{
	auto data = pMainWindow->cfg.GetData();
	hot_mic = false;
	r1.get();
	r2.get();

	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	p1 = std::async(std::launch::async, &CAudioManager::codec2audio, this, data->bVoiceOnlyEnable);
	p2 = std::async(std::launch::async, &CAudioManager::play_audio_queue, this);
	p1.get();
	p2.get();
}

void CAudioManager::M17_2AudioMgr(const SM17Frame &m17)
{
	static bool is_3200;
	if (! play_file) {
		if (0U==m17_sid_in && 0U==(m17.GetFrameNumber() & 0x8000u)) {	// don't start if it's the last audio frame
			// here comes a new stream
			m17_sid_in = m17.streamid;
			is_3200 = ((m17.GetFrameType() & 0x6u) == 0x4u);
			pMainWindow->Receive(true);
			// launch the audio processing threads
			p1 = std::async(std::launch::async, &CAudioManager::codec2audio, this, is_3200);
			p2 = std::async(std::launch::async, &CAudioManager::play_audio_queue, this);
		}
		if (m17.streamid != m17_sid_in)
			return;
		auto payload = m17.payload;
		CC2DataFrame dataframe(payload);
		auto last = (0x8000u == (m17.GetFrameNumber() & 0x8000u));
		dataframe.SetFlag(is_3200 ? false : last);
		data_mutex.lock();
		c2_queue.Push(dataframe);
		if (is_3200) {
			CC2DataFrame frame2(payload+8);
			frame2.SetFlag(last);
			c2_queue.Push(frame2);
		}
		data_mutex.unlock();
		if (last) {
			p1.get();	// we're done, get the finished threads and reset the current stream id
			p2.get();
			m17_sid_in = 0U;
			pMainWindow->Receive(false);
		}
	}
}

void CAudioManager::play_audio_queue()
{
	auto data = pMainWindow->cfg.GetData();
	std::this_thread::sleep_for(std::chrono::milliseconds(300));
	// Open PCM device for playback.
	snd_pcm_t *handle;
	int rc = snd_pcm_open(&handle, data->sAudioOut.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0) {
		std::cerr << "unable to open pcm device: " << snd_strerror(rc) << std::endl;
		return;
	}

	// Allocate a hardware parameters object.
	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	snd_pcm_hw_params_any(handle, params);

	// Set the desired hardware parameters.

	// Interleaved mode
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

	// Signed 16-bit little-endian format
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

	// One channels (mono)
	snd_pcm_hw_params_set_channels(handle, params, 1);

	// samples/second sampling rate
	snd_pcm_hw_params_set_rate(handle, params, 8000, 0);

	// Set period size to 160 frames.
	snd_pcm_uframes_t frames = 160;
	snd_pcm_hw_params_set_period_size(handle, params, frames, 0);
	//snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

	// Write the parameters to the driver
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0) {
		std::cerr << "unable to set hw parameters: " << snd_strerror(rc) << std::endl;
		return;
	}

	// Use a buffer large enough to hold one period
	snd_pcm_hw_params_get_period_size(params, &frames, 0);

	bool last;
	do {
		while (audio_is_empty())
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
		audio_mutex.lock();
		CAudioFrame frame(audio_queue.Pop());
		audio_mutex.unlock();
		last = frame.GetFlag();
		rc = snd_pcm_writei(handle, frame.GetData(), frames);
		if (rc == -EPIPE) {
			// EPIPE means underrun
			// std::cerr << "underrun occurred" << std::endl;
			snd_pcm_prepare(handle);
		} else if (rc < 0) {
			std::cerr <<  "error from writei: " << snd_strerror(rc) << std::endl;
		}  else if (rc != int(frames)) {
			std::cerr << "short write, write " << rc << " frames" << std::endl;
		}
	} while (! last);

	snd_pcm_drain(handle);
	snd_pcm_close(handle);
}

bool CAudioManager::audio_is_empty()
{
	audio_mutex.lock();
	bool ret = audio_queue.Empty();
	audio_mutex.unlock();
	return ret;
}

bool CAudioManager::codec_is_empty()
{
	data_mutex.lock();
	bool ret = c2_queue.Empty();
	data_mutex.unlock();
	return ret;
}

void CAudioManager::KeyOff()
{
	if (hot_mic) {
		hot_mic = false;
		r1.get();
		r2.get();
		r3.get();
	}
}

void CAudioManager::Link(const std::string &linkcmd)
{
	if (0 == linkcmd.compare(0, 3, "M17")) { //it's an M17 link/unlink command
		//std::cout << "got Link cmd '" << linkcmd << "'" << std::endl;
		SM17Frame frame;
		if ('L' == linkcmd.at(3)) {
			if (13 == linkcmd.size()) {
				std::string sDest(linkcmd.substr(4));
				sDest[7] = 'L';
				CCallsign dest(sDest);
				dest.CodeOut(frame.lich.addr_dst);
				//printf("dest=%s=0x%02x%02x%02x%02x%02x%02x\n", dest.GetCS().c_str(), frame.lich.addr_dst[0], frame.lich.addr_dst[1], frame.lich.addr_dst[2], frame.lich.addr_dst[3], frame.lich.addr_dst[4], frame.lich.addr_dst[5]);
				AM2M17.Write(frame.magic, sizeof(SM17Frame));
			}
		} else if ('U' == linkcmd.at(3)) {
			CCallsign dest("U");
			dest.CodeOut(frame.lich.addr_dst);
			AM2M17.Write(frame.magic, sizeof(SM17Frame));
		}
	}
}

void CAudioManager::calc_audio_stats(const short int *wave)
{
	if (wave) {
		double ss = 0.0;
		for (unsigned int i=0; i<160; i++) {
			auto a = (unsigned int)abs(wave[i]);
			if (a > 16383) volStats.clip++;
			if (i % 2) ss += double(a) * double(a); // every other point will do
		}
		volStats.count += 160;
		volStats.ss += ss;
	} else {
		volStats.count = volStats.clip = 0;
		volStats.ss = 0.0;
	}
}
