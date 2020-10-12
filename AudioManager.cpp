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

CAudioManager::CAudioManager() : hot_mic(false), play_file(false), gate_sid_in(0U), link_sid_in(0U), m17_sid_in(0U)
{
	link_open = true;
}

bool CAudioManager::Init(CMainWindow *pMain)
{
	pMainWindow = pMain;
	std::string index(CFG_DIR);

	index.append("announce/index.dat");
	std::ifstream indexfile(index.c_str(), std::ifstream::in);
	if (indexfile) {
		for (int i=0; i<65; i++) {
			std::string name, offset, size;
			indexfile >> name >> offset >> size;
			if (name.size() && offset.size() && size.size()) {
				unsigned long of = std::stoul(offset);
				unsigned long sz = std::stoul(size);
				speak.push_back(1000U * of + sz);
			}
		}
		indexfile.close();
	}
	if (65 == speak.size()) {
		std::cout << "read " << speak.size() << " indicies from " << index << std::endl;
	} else {
		std::cerr << "read unexpected (" << speak.size() << " number of indices from " << index << std::endl;
		speak.clear();
	}
	AM2M17.SetUp("am2m17");
	AM2Gate.SetUp("am2gate");
	AM2Link.SetUp("am2link");
	LogInput.SetUp("log_input");
	return false;
}


void CAudioManager::RecordMicThread(E_PTT_Type for_who, const std::string &urcall)
{
	auto data = pMainWindow->cfg.GetData();
	hot_mic = true;

	r1 = std::async(std::launch::async, &CAudioManager::microphone2audioqueue, this);

	if (data->bCodec2Enable)
		r2 = std::async(std::launch::async, &CAudioManager::audio2codec, this, data->bVoiceOnlyEnable);
	else
		r2 = std::async(std::launch::async, &CAudioManager::audioqueue2ambedevice, this);

	switch (for_who) {
		case E_PTT_Type::echo:
			if (! data->bCodec2Enable)
				r3 = std::async(std::launch::async, &CAudioManager::ambedevice2ambequeue, this);
			break;
		case E_PTT_Type::gateway:
			r3 = std::async(std::launch::async, &CAudioManager::ambedevice2packetqueue, this, std::ref(gateway_queue), std::ref(gateway_mutex), urcall);
			r4 = std::async(std::launch::async, &CAudioManager::packetqueue2gate, this);
			break;
		case E_PTT_Type::link:
			r3 = std::async(std::launch::async, &CAudioManager::ambedevice2packetqueue, this, std::ref(link_queue), std::ref(link_mutex), urcall);
			r4 = std::async(std::launch::async, &CAudioManager::packetqueue2link, this);
			break;
		case E_PTT_Type::m17:
			r3 = std::async(std::launch::async, &CAudioManager::codec2m17gateway, this, urcall, data->sM17SourceCallsign, data->bVoiceOnlyEnable);
			break;
	}
}

void CAudioManager::audio2codec(const bool is_3200)
{
	CCodec2 c2(is_3200);
	bool last;
	bool is_odd = false; // true if we've processed an odd number of audio frames
	do {
		while ( audio_is_empty() )
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
		audio_mutex.lock();
		CAudioFrame audioframe = audio_queue.Pop();
		audio_mutex.unlock();
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
			short audio[320] = { 0 }; // 40 ms of silence
			unsigned char data[8];
			memcpy(audio, audioframe.GetData(), 160*sizeof(short)); // we have 20 ms of audio at the beginning
			if (! last) { // get another frame, if available
				while (audio_is_empty())
					std::this_thread::sleep_for(std::chrono::milliseconds(3));
				audio_mutex.lock();
				audioframe = audio_queue.Pop();
				audio_mutex.unlock();
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

void CAudioManager::makeheader(SDSVT &c, const std::string &urcall, unsigned char *ut, unsigned char *uh)
{
	// this also makes the scrambled text message and header for streaming into the slow data
	// only the 1-byte header need to be interleaved before and between each 5 byte set
	const unsigned char scramble[5] = { 0x4FU, 0x93U, 0x70U, 0x4FU, 0x93U };
	auto cfgdata = pMainWindow->cfg.GetData();
	std::string msg(cfgdata->sMessage);
	msg.resize(20, ' ');
	for (int i=0; i<20; i++) {
		ut[i] = scramble[i%5] ^ msg.at(i);
	}
	memset(c.title, 0, sizeof(SDSVT));
	memcpy(c.title, "DSVT", 4);
	c.config = 0x10U;
	c.id = 0x20U;
	c.flagb[2] = 1U;
	c.streamid = random.NewStreamID();
	c.ctrl = 0x80U;
	memset(c.hdr.flag+3, ' ', 36);
	memcpy(c.hdr.rpt1, cfgdata->sStation.c_str(), cfgdata->sStation.size());
	memcpy(c.hdr.rpt2, c.hdr.rpt1, 8);
	c.hdr.rpt1[7] = cfgdata->cModule;
	c.hdr.rpt2[7] = 'G';
	memcpy(c.hdr.urcall, urcall.c_str(), urcall.size());
	memcpy(c.hdr.mycall, cfgdata->sCallsign.c_str(), cfgdata->sCallsign.size());
	memcpy(c.hdr.sfx, cfgdata->sName.c_str(), cfgdata->sName.size());
	calcPFCS(c.hdr.flag, c.hdr.pfcs);
	for (int i=0; i<41; i++) {
		uh[i] = scramble[i%5] ^ *(c.hdr.flag + i);
	}
}

void CAudioManager::QuickKey(const std::string &urcall)
{
	hot_mic = true;
	const unsigned char silence[9] = { 0x9EU, 0x8DU, 0x32U, 0x88U, 0x26U, 0x1AU, 0x3FU, 0x61U, 0xE8U };
	unsigned char ut[20];
	unsigned char uh[41];
	SDSVT h;
	makeheader(h, urcall, ut, uh);
	bool islink = (0 == urcall.compare(0, 6, "CQCQCQ"));
	if (islink)
		AM2Link.Write(h.title, 56);
	else
		AM2Gate.Write(h.title, 56);

	h.config = 0x20U;
	memcpy(h.vasd.voice, silence, 9);
	for (unsigned int i=0; i<10; i++) {
		h.ctrl = i;
		if (9U == i)
			h.ctrl |= 0x40U;
		SlowData(i, ut, uh, h);
		std::this_thread::sleep_for(std::chrono::microseconds(19));
		if (islink)
			AM2Link.Write(h.title, 27);
		else
			AM2Gate.Write(h.title, 27);
	}
	hot_mic = false;
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

void CAudioManager::ambedevice2packetqueue(DSVTPacketQueue &queue, std::mutex &mtx, const std::string &urcall)
{
	unsigned count = 0;
	// add a header;
	unsigned char ut[20];
	unsigned char uh[41];
	SDSVT h;
	makeheader(h, urcall, ut, uh);
	SDSVT v(h);
	v.config = 0x20U;
	bool header_not_sent = true;
	do {
		if (! AMBEDevice.IsOpen())
			return;
		if (AMBEDevice.GetData(v.vasd.voice))
			break;
		a2d_mutex.lock();
		auto last = a2d_queue.Pop();
		a2d_mutex.unlock();
		v.ctrl = count % 21;
		if (last) v.ctrl |= 0x40U;
		SlowData(count++, ut, uh, v);
		mtx.lock();
		if (header_not_sent) {
			queue.Push(h);
			header_not_sent = false;
		}
		queue.Push(v);
		mtx.unlock();
	} while (0U == (v.ctrl & 0x40U));
}

void CAudioManager::packetqueue2link()
{
	//unsigned count = 0U;
	unsigned char ctrl = 0U;
	std::this_thread::sleep_for(std::chrono::milliseconds(300));
	do {
		link_mutex.lock();
		if (link_queue.Empty()) {
			link_mutex.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
		} else {
			SDSVT dsvt = link_queue.Pop();
			link_mutex.unlock();
			AM2Link.Write(dsvt.title, (dsvt.config==0x10) ? 56 : 27);
			ctrl = dsvt.ctrl;
	//		count++;
		}
	} while (0U == (ctrl & 0x40U));
	//std::cout << count << " packets sent to link\n";
}

void CAudioManager::packetqueue2gate()
{
	unsigned char ctrl = 0U;
	std::this_thread::sleep_for(std::chrono::milliseconds(300));
	do {
		gateway_mutex.lock();
		if (gateway_queue.Empty()) {
			gateway_mutex.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
		} else {
			SDSVT dsvt = gateway_queue.Pop();
			gateway_mutex.unlock();
			AM2Gate.Write(dsvt.title, (dsvt.config==0x10) ? 56 : 27);
			ctrl = dsvt.ctrl;
		}
	} while (0U == (ctrl & 0x40U));
}

void CAudioManager::SlowData(const unsigned count, const unsigned char *ut, const unsigned char *uh, SDSVT &d)
{
	const unsigned char sync[3] = { 0x55U, 0x2DU, 0x16U };
	const unsigned char empty[3] = { 0x16U, 0x29U, 0xF5U };
	unsigned ctrl = d.ctrl & 0x1FU;
	if (ctrl) {
		const unsigned sf = count / 21;	// superframe count
		const unsigned cd2 = ctrl / 2;
		if (sf % 70) {
			// header
			switch (ctrl) {
				case 1:
				case 3:
				case 5:
				case 7:
				case 9:
				case 11:
				case 13:
				case 15:
					d.vasd.text[0] = 0x70U ^ 0x55U;
					d.vasd.text[1] = uh[5*cd2];
					d.vasd.text[2] = uh[5*cd2+1];
					break;
				case 2:
				case 4:
				case 6:
				case 8:
				case 10:
				case 12:
				case 14:
				case 16:
					d.vasd.text[0] = uh[5*cd2-3];
					d.vasd.text[1] = uh[5*cd2-2];
					d.vasd.text[2] = uh[5*cd2-1];
					break;
				case 17:
					d.vasd.text[0] = 0x70U ^ 0x51U;
					d.vasd.text[1] = uh[40];
					d.vasd.text[2] = empty[2];
					break;
				default:
					memcpy(d.vasd.text, empty, 3);
					break;
			}
		} else {
			// text message
			switch (ctrl) {
				case 1:
				case 3:
				case 5:
				case 7:
					d.vasd.text[0] = 0x70U ^ ('@'+cd2);
					d.vasd.text[1] = ut[5*cd2];
					d.vasd.text[2] = ut[5*cd2+1];
					break;
				case 2:
				case 4:
				case 6:
				case 8:
					d.vasd.text[0] = ut[5*cd2-3];
					d.vasd.text[1] = ut[5*cd2-2];
					d.vasd.text[2] = ut[5*cd2-1];
					break;
				default:
					memcpy(d.vasd.text, empty, 3);
					break;
			}
		}
	} else {
		memcpy(d.vasd.text, sync, 3);
	}
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
		//std::cout << "audio:" << count << " hot_mic:" << hot_mic << std::endl;
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
	//std::cout << count << " frames by microphone2audioqueue\n";
	snd_pcm_drop(handle);
	snd_pcm_close(handle);
}

void CAudioManager::audioqueue2ambedevice()
{
	bool last;
	do {
		while (audio_is_empty())
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
		audio_mutex.lock();
		CAudioFrame frame(audio_queue.Pop());
		audio_mutex.unlock();
		if (! AMBEDevice.IsOpen())
			return;
		if(AMBEDevice.SendAudio(frame.GetData()))
			break;
		last = frame.GetFlag();
		a2d_mutex.lock();
		a2d_queue.Push(frame.GetFlag());
		a2d_mutex.unlock();
	} while (! last);
}

void CAudioManager::ambedevice2ambequeue()
{
	bool last;
	do {
		unsigned char ambe[9];
		if (! AMBEDevice.IsOpen())
			return;
		if (AMBEDevice.GetData(ambe))
			break;
		CAmbeDataFrame frame(ambe);
		a2d_mutex.lock();
		last = a2d_queue.Pop();
		a2d_mutex.unlock();
		frame.SetFlag(last);
		data_mutex.lock();
		ambe_queue.Push(frame);
		data_mutex.unlock();
	} while (! last);
	return;
}

void CAudioManager::codec2audio(const bool is_3200)
{
	CCodec2 c2(is_3200);
	bool last;
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
		}
	} while (! last);
}

void CAudioManager::PlayEchoDataThread()
{
	auto data = pMainWindow->cfg.GetData();
	hot_mic = false;
	r1.get();
	r2.get();
	if (! data->bCodec2Enable)
		r3.get();

	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	if (data->bCodec2Enable) {
		p1 = std::async(std::launch::async, &CAudioManager::codec2audio, this, data->bVoiceOnlyEnable);
		p2 = std::async(std::launch::async, &CAudioManager::play_audio_queue, this);
		p1.get();
		p2.get();
	} else {
		p1 = std::async(std::launch::async, &CAudioManager::ambequeue2ambedevice, this);
		p2 = std::async(std::launch::async, &CAudioManager::ambedevice2audioqueue, this);
		p3 = std::async(std::launch::async, &CAudioManager::play_audio_queue, this);
		p1.get();
		p2.get();
		p3.get();
	}
}

void CAudioManager::Link2AudioMgr(const SDSVT &dsvt)
{
	l2am_mutex.lock();
	l2am(dsvt, false);
	l2am_mutex.unlock();
}

void CAudioManager::l2am(const SDSVT &dsvt, const bool shutoff) {
	if (link_open && AMBEDevice.IsOpen() && 0U==gate_sid_in && ! play_file) {	// don't do anythings if the gateway is currently providing audio

		if (0U==link_sid_in && 0U==(dsvt.ctrl & 0x40U)) {	// don't start if it's the last audio frame
			// here comes a new stream
			link_sid_in = dsvt.streamid;
			pMainWindow->Receive(true);
			// launch the audio processing threads
			p1 = std::async(std::launch::async, &CAudioManager::ambequeue2ambedevice, this);
			p2 = std::async(std::launch::async, &CAudioManager::ambedevice2audioqueue, this);
			p3 = std::async(std::launch::async, &CAudioManager::play_audio_queue, this);
		}
		if (dsvt.streamid != link_sid_in)
			return;
		if (0x20U != dsvt.config)
			return;	// we only need audio frames at this point
		CAmbeDataFrame frame(dsvt.vasd.voice);
		frame.SetFlag((dsvt.ctrl & 0x40U) == 0x40U);
		data_mutex.lock();
		ambe_queue.Push(frame);
		data_mutex.unlock();
		if (shutoff)
			link_open = false;	// slam the door shut. it will open again when pLink is relinked.
		if (dsvt.ctrl & 0x40U) {
			p1.get();	// we're done, get the finished threads and reset the current stream id
			p2.get();
			p3.get();
			link_sid_in = 0U;
			pMainWindow->Receive(false);
		}
	}
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

void CAudioManager::Gateway2AudioMgr(const SDSVT &dsvt)
{
	if (AMBEDevice.IsOpen() && 0U==link_sid_in && ! play_file) {	// don't do anythings if the link is currently providing audio

		if (0U==gate_sid_in && 0U==(dsvt.ctrl & 0x40U)) {	// don't start if it's the last audio frame
			// here comes a new stream
			gate_sid_in = dsvt.streamid;
			pMainWindow->Receive(true);
			// launch the audio processing threads
			p1 = std::async(std::launch::async, &CAudioManager::ambequeue2ambedevice, this);
			p2 = std::async(std::launch::async, &CAudioManager::ambedevice2audioqueue, this);
			p3 = std::async(std::launch::async, &CAudioManager::play_audio_queue, this);
		}
		if (dsvt.streamid != gate_sid_in)
			return;
		if (0x20U != dsvt.config)
			return;	// we only need audio frames at this point
		CAmbeDataFrame frame(dsvt.vasd.voice);
		bool last = (dsvt.ctrl & 0x40U) == 0x40U;
		frame.SetFlag(last);
		data_mutex.lock();
		ambe_queue.Push(frame);
		data_mutex.unlock();
		if (last) {
			p1.get();	// we're done, get the finished threads and reset the current stream id
			p2.get();
			p3.get();
			gate_sid_in = 0U;
			pMainWindow->Receive(false);
		}
	}
}

void CAudioManager::ambequeue2ambedevice()
{
	bool last;
	do {
		while (ambe_is_empty())
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
		data_mutex.lock();
		CAmbeDataFrame frame(ambe_queue.Pop());
		data_mutex.unlock();
		last = frame.GetFlag();
		d2a_mutex.lock();
		d2a_queue.Push(last);
		d2a_mutex.unlock();
		if (! AMBEDevice.IsOpen())
			return;
		if (AMBEDevice.SendData(frame.GetData()))
			return;
	} while (! last);
}

void CAudioManager::ambedevice2audioqueue()
{
	bool last;
	do {
		if (! AMBEDevice.IsOpen())
			return;
		short audio[160];
		if (AMBEDevice.GetAudio(audio))
			return;
		CAudioFrame frame(audio);
		d2a_mutex.lock();
		last = d2a_queue.Pop();
		d2a_mutex.unlock();
		frame.SetFlag(last);
		audio_mutex.lock();
		audio_queue.Push(frame);
		audio_mutex.unlock();
	} while (! last);
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

bool CAudioManager::ambe_is_empty()
{
	data_mutex.lock();
	bool ret = ambe_queue.Empty();
	data_mutex.unlock();
	return ret;
}

bool CAudioManager::codec_is_empty()
{
	data_mutex.lock();
	bool ret = c2_queue.Empty();
	data_mutex.unlock();
	return ret;
}

void CAudioManager::calcPFCS(const unsigned char *packet, unsigned char *pfcs)
{
	unsigned short crc_dstar_ffff = 0xffff;
	unsigned short tmp, short_c;
	unsigned short crc_tabccitt[256] = {
		0x0000,0x1189,0x2312,0x329b,0x4624,0x57ad,0x6536,0x74bf,0x8c48,0x9dc1,0xaf5a,0xbed3,0xca6c,0xdbe5,0xe97e,0xf8f7,
		0x1081,0x0108,0x3393,0x221a,0x56a5,0x472c,0x75b7,0x643e,0x9cc9,0x8d40,0xbfdb,0xae52,0xdaed,0xcb64,0xf9ff,0xe876,
		0x2102,0x308b,0x0210,0x1399,0x6726,0x76af,0x4434,0x55bd,0xad4a,0xbcc3,0x8e58,0x9fd1,0xeb6e,0xfae7,0xc87c,0xd9f5,
		0x3183,0x200a,0x1291,0x0318,0x77a7,0x662e,0x54b5,0x453c,0xbdcb,0xac42,0x9ed9,0x8f50,0xfbef,0xea66,0xd8fd,0xc974,
		0x4204,0x538d,0x6116,0x709f,0x0420,0x15a9,0x2732,0x36bb,0xce4c,0xdfc5,0xed5e,0xfcd7,0x8868,0x99e1,0xab7a,0xbaf3,
		0x5285,0x430c,0x7197,0x601e,0x14a1,0x0528,0x37b3,0x263a,0xdecd,0xcf44,0xfddf,0xec56,0x98e9,0x8960,0xbbfb,0xaa72,
		0x6306,0x728f,0x4014,0x519d,0x2522,0x34ab,0x0630,0x17b9,0xef4e,0xfec7,0xcc5c,0xddd5,0xa96a,0xb8e3,0x8a78,0x9bf1,
		0x7387,0x620e,0x5095,0x411c,0x35a3,0x242a,0x16b1,0x0738,0xffcf,0xee46,0xdcdd,0xcd54,0xb9eb,0xa862,0x9af9,0x8b70,
		0x8408,0x9581,0xa71a,0xb693,0xc22c,0xd3a5,0xe13e,0xf0b7,0x0840,0x19c9,0x2b52,0x3adb,0x4e64,0x5fed,0x6d76,0x7cff,
		0x9489,0x8500,0xb79b,0xa612,0xd2ad,0xc324,0xf1bf,0xe036,0x18c1,0x0948,0x3bd3,0x2a5a,0x5ee5,0x4f6c,0x7df7,0x6c7e,
		0xa50a,0xb483,0x8618,0x9791,0xe32e,0xf2a7,0xc03c,0xd1b5,0x2942,0x38cb,0x0a50,0x1bd9,0x6f66,0x7eef,0x4c74,0x5dfd,
		0xb58b,0xa402,0x9699,0x8710,0xf3af,0xe226,0xd0bd,0xc134,0x39c3,0x284a,0x1ad1,0x0b58,0x7fe7,0x6e6e,0x5cf5,0x4d7c,
		0xc60c,0xd785,0xe51e,0xf497,0x8028,0x91a1,0xa33a,0xb2b3,0x4a44,0x5bcd,0x6956,0x78df,0x0c60,0x1de9,0x2f72,0x3efb,
		0xd68d,0xc704,0xf59f,0xe416,0x90a9,0x8120,0xb3bb,0xa232,0x5ac5,0x4b4c,0x79d7,0x685e,0x1ce1,0x0d68,0x3ff3,0x2e7a,
		0xe70e,0xf687,0xc41c,0xd595,0xa12a,0xb0a3,0x8238,0x93b1,0x6b46,0x7acf,0x4854,0x59dd,0x2d62,0x3ceb,0x0e70,0x1ff9,
		0xf78f,0xe606,0xd49d,0xc514,0xb1ab,0xa022,0x92b9,0x8330,0x7bc7,0x6a4e,0x58d5,0x495c,0x3de3,0x2c6a,0x1ef1,0x0f78
	};

	for (int i = 0; i < 39 ; i++) {
		short_c = 0x00ff & (unsigned short)packet[i];
		tmp = (crc_dstar_ffff & 0x00ff) ^ short_c;
		crc_dstar_ffff = (crc_dstar_ffff >> 8) ^ crc_tabccitt[tmp];
	}
	crc_dstar_ffff =  ~crc_dstar_ffff;
	tmp = crc_dstar_ffff;

	pfcs[0] = (unsigned char)(crc_dstar_ffff & 0xff);
	pfcs[1] = (unsigned char)((tmp >> 8) & 0xff);

	return;
}

void CAudioManager::KeyOff(bool isAMBEmode)
{
	if (hot_mic) {
		hot_mic = false;
		r1.get();
		r2.get();
		r3.get();
		if (isAMBEmode)
			r4.get();
	}
}

void CAudioManager::Link(const std::string &linkcmd)
{
	if (0 == linkcmd.compare(0, 4, "LINK")) {
		AM2Link.Write(linkcmd.c_str(), linkcmd.size() + 1);
		if (linkcmd.size() < 5) {
			// ON UNLINK: if there is an open link stream, push a closing ambe frame onto the stream
			if (link_sid_in) {
				l2am_mutex.lock();
				const unsigned char quiet[9] = { 0x9EU, 0x8DU, 0x32U, 0x88U, 0x26U, 0x1AU, 0x3FU, 0x61U, 0xE8U };
				SDSVT dsvt;		// we only need to set up a few things to keep l2am() happy
				dsvt.config = 0x20U;				// it's a voice packet
				dsvt.ctrl = 0x40U;					// it's the last packet
				dsvt.streamid = link_sid_in;		// it has the correct streamid
				memcpy(dsvt.vasd.voice, quiet, 9);	// it noiseless
				l2am(dsvt, true);
				l2am_mutex.unlock();
			}
		} else {
			link_open = true;
		}
	} else if (0 == linkcmd.compare(0, 3, "M17")) { //it's an M17 link/unlink command
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

void CAudioManager::PlayFile(const char *filetoplay)
{
	if (gate_sid_in || link_sid_in || m17_sid_in) {
		std::cout << "Too busy to play " << filetoplay << std::endl;
		return;
	}
	play_file = true;
	auto cfgdata = pMainWindow->cfg.GetData();
	std::string msg(filetoplay);
	if (cfgdata->cModule != msg.at(0)) {
		std::cerr << "Improper module in msg " << msg << std::endl;
		play_file = false;
		return;
	}

	auto pos = msg.find("_linked.dat_LINKED_");
	bool is_linked = (std::string::npos == pos) ? false : true;

	pos = msg.find(".dat");
	if (std::string::npos == pos) {
		std::cerr << "Improper AMBE data file in msg " << msg << std::endl;
		play_file = false;
		return;
	}

	std::string message;
	if (msg.size() > pos+4) {
		message.assign(msg.substr(pos+5));
		message.append("\n");
		LogInput.Write(message.c_str(), message.size()+1);
	}

	std::string cfgdir(CFG_DIR);
	cfgdir.append("announce/");

	std::string path = cfgdir + msg.substr(2, pos+2);

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	std::cout << "sending File:" << path << " mod:" << cfgdata->cModule << " RADIO_ID=" << message << std::endl;

	struct stat sbuf;
	if (stat(path.c_str(), &sbuf)) {
		std::cerr << "can't stat " << path << std::endl;
		play_file = false;
		return;
	}

	if (sbuf.st_size % 8)
		std::cerr << "Warning " << path << " file size is " << sbuf.st_size << " (not a multiple of 8)!" << std::endl;
	int ambeblocks = (int)sbuf.st_size / 8;


	FILE *fp = fopen(path.c_str(), "rb");
	if (!fp) {
		std::cerr << "Failed to open file " << path << " for reading" << std::endl;
		play_file = false;
		return;
	}

	p1 = std::async(std::launch::async, &CAudioManager::codec2audio, this, true);
	p2 = std::async(std::launch::async, &CAudioManager::play_audio_queue, this);

	int count;
	for (count=0; count<ambeblocks; count++) {
		unsigned char voice[9];
		int nread = fread(voice, 8, 1, fp);
		if (nread == 1) {
			CC2DataFrame frame(voice);
			bool last = false;
			if (count+1 == ambeblocks && ! is_linked)
				last = true;
			frame.SetFlag(last);
			data_mutex.lock();
			c2_queue.Push(frame);
			data_mutex.unlock();
		}
	}
	fclose(fp);

	if (is_linked) {
		// open the speak file
		std::string speakfile(cfgdir);
		speakfile.append("/speak.dat");
		fp = fopen(speakfile.c_str(), "rb");
		if (fp) {
			// create the speak sentence
			std::string say("2");
			say.append(message.substr(7));
			auto rit = say.rbegin();
			while (isspace(*rit)) {
				say.resize(say.size()-1);
				rit = say.rbegin();
			}

			// play it
			for (auto it=say.begin(); it!=say.end(); it++) {
				bool lastch = (it+1 == say.end());
				unsigned long offset = 0;
				int size = 0;
				if ('A' <= *it && *it <= 'Z')
					offset = speak[*it - 'A' + (lastch ? 26 : 0)];
				else if ('0' <= *it && *it <= '9')
					offset = speak[*it - '0' + 52];
				else if ('.' == *it)
					offset = speak[62];
				else if ('-' == *it)
					offset = speak[63];
				else if ('/' == *it)
					offset = speak[64];
				if (offset) {
					size = (int)(offset % 1000UL);
					offset = (offset / 1000UL) * 8UL;
				}
				if (0 == size)
					continue;
				if (fseek(fp, offset, SEEK_SET)) {
					std::cerr << "fseek to " << offset << " error!" << std::endl;
				} else {
					for (int i=0; i<size; i++) {
						uint8_t voice[8];
						int nread = fread(voice, 8, 1, fp);
						if (nread == 1) {
							CC2DataFrame frame(voice);
							bool last = false;
							if (i+1==size && lastch)
								last = true;	// signal the last voiceframe (of the last character)
							frame.SetFlag(last);
							data_mutex.lock();
							c2_queue.Push(frame);
							data_mutex.unlock();
						}
					}
				}
			}
			fclose(fp);
		}
	}
	p1.get();
	p2.get();
	play_file = false;
}
