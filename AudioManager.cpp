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
#include <thread>

#include "MainWindow.h"
#include "AudioManager.h"
#include "Configure.h"
#include "FrameType.h"
#include "Callsign.h"
#include "codec2.h"

CAudioManager::CAudioManager() : hot_mic(false), play_file(false), m17_sid_in(0U)
{
	link_open = true;
	volStats.count = 0;

#ifdef USE44100
	expand.data_in = expand_in;
	expand.data_out = expand_out;
	expand.input_frames = 160;
	expand.output_frames = 882;
	expand.end_of_input = false;
	RSExpand.SetRatio(expand, 44100.0/8000.0);

	shrink.data_in = shrink_in;
	shrink.data_out = expand_out;
	shrink.input_frames = 882;
	shrink.output_frames = 160;
	shrink.end_of_input = false;
	RSShrink.SetRatio(shrink, 8000.0/44100.0);
#endif
}

bool CAudioManager::Init(CMainWindow *pMain)
{
	pMainWindow = pMain;
	AM2M17.SetUp("am2m17");
	LogInput.SetUp("log_input");
	return false;
}

void CAudioManager::BuildMetaBlocks()
{
	auto pcfg = pMainWindow->cfg.GetData();
	if (pcfg->dLatitude or pcfg->dLongitude) {
		gnss.SetDataStationTypes(EGnssSourceType::Client, EGnssStationType::Fixed);
		gnss.Set(pcfg->dLatitude, pcfg->dLongitude);
	}
	CMessage::MakeBlocks(pcfg->sMessage, msgblks);
}

void CAudioManager::RecordMicThread(E_PTT_Type for_who, const std::string &urcall)
{
	// wait on audio queue to empty

	unsigned int count = 0u;
	while (! audio_queue.IsEmpty()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		count++;
	}
	if (count > 0)
		std::cout << "Tailgating detected! Waited " << count*20 << " ms for audio queue to clear." << std::endl;

	auto data = pMainWindow->cfg.GetData();
	hot_mic = true;

	mic2audio_fut = std::async(std::launch::async, &CAudioManager::mic2audio, this);

	audio2codec_fut = std::async(std::launch::async, &CAudioManager::audio2codec, this, data->bVoiceOnlyEnable);

	if (for_who == E_PTT_Type::m17) {
		codec2gateway_fut = std::async(std::launch::async, &CAudioManager::codec2gateway, this, urcall, data->sM17SourceCallsign, data->bVoiceOnlyEnable);
	}
}

void CAudioManager::audio2codec(const bool is_3200)
{
	CCodec2 c2(is_3200);
	bool last;
	calc_audio_stats();  // initialize volume statistics
	bool is_odd = false; // true if we've processed an odd number of audio frames
	do {
		// we'll wait until there is something
		CAudioFrame audioframe = audio_queue.WaitPop();
		calc_audio_stats(audioframe.GetData());
		last = audioframe.GetFlag();
		if ( is_3200 ) {
			is_odd = ! is_odd;
			unsigned char data[8];
			c2.codec2_encode(data, audioframe.GetData());
			CC2DataFrame dataframe(data);
			dataframe.SetFlag(is_odd ? false : last);
			c2_queue.Push(dataframe);
			if (is_odd && last) { // we need an even number of data frame for 3200
				// add one more quite frame
				const short quiet[160] = { 0 };
				c2.codec2_encode(data, quiet);
				CC2DataFrame frame2(data);
				frame2.SetFlag(true);
				c2_queue.Push(frame2);
			}
		} else { // 1600 - we need 40 ms of audio
			short audio[320] = { 0 }; // initialize to 40 ms of silence
			unsigned char data[8];
			memcpy(audio, audioframe.GetData(), 160*sizeof(short)); // we'll put 20 ms of audio at the beginning
			if (last) { // get another frame, if available
				volStats.count += 160; // a quite frame will only contribute to the total count
			} else {
				//we'll wait until there is something
				audioframe = audio_queue.WaitPop();
				calc_audio_stats(audioframe.GetData());
				memcpy(audio+160, audioframe.GetData(), 160*sizeof(short));	// now we have 40 ms total
				last = audioframe.GetFlag();
			}
			c2.codec2_encode(data, audio);
			CC2DataFrame dataframe(data);
			dataframe.SetFlag(last);
			c2_queue.Push(dataframe);
		}
	} while (! last);
}

void CAudioManager::QuickKey(const std::string &d, const std::string &s)
{
	hot_mic = true;
	CPacket pack;
	pack.Initialize(54u, true);
	CCallsign dst(d), src(s);
	pack.SetStreamId(random.NewStreamID());
	dst.CodeOut(pack.GetDstAddress());
	src.CodeOut(pack.GetSrcAddress());
	pack.SetFrameType(0x5u);
	const uint8_t quiet[] = { 0x01u, 0x00u, 0x09u, 0x43u, 0x9cu, 0xe4u, 0x21u, 0x08u };
	memcpy(pack.GetVoiceData(true),  quiet, 8);
	memcpy(pack.GetVoiceData(false), quiet, 8);
	for (uint16_t i=0; i<5; i++) {
		pack.SetFrameNumber((i < 4) ? i : i | 0x8000u);
		pack.CalcCRC();
		AM2M17.Write(pack.GetCData(), pack.GetSize());
	}
	hot_mic = false;
}

void CAudioManager::codec2gateway(const std::string &dst, const std::string &src, bool voiceonly)
{
	CCallsign destination(dst);
	CCallsign source(src);

	// make most of the M17 IP frame
	// TODO: metadata and encryption and more TODOs mentioned later...
	CPacket pack;
	pack.Initialize(54u, true);
	pack.SetStreamId(random.NewStreamID());
	pack.SetFrameType(voiceonly ? 0x5U : 0x7U);
	CFrameType TYPE(pack.GetFrameType());
	const auto pMeta = pack.GetMetaData();
	destination.CodeOut(pack.GetDstAddress());
	source.CodeOut(pack.GetSrcAddress());

	unsigned int count = 0;
	bool last;
	do {
		// we'll wait until there is something
		CC2DataFrame cframe = c2_queue.WaitPop();
		last = cframe.GetFlag();
		memcpy(pack.GetVoiceData(), cframe.GetData(), 8);
		if (voiceonly) {
			if (last) {
				// we should never get here, but just in case...
				std::cerr << "WARNING: unexpected end of 3200 voice stream!" << std::endl;
				const uint8_t quiet[] = { 0x00u, 0x01u, 0x43u, 0x09u, 0xe4u, 0x9cu, 0x08u, 0x21u };	// for 3200 only!
				memcpy(pack.GetVoiceData(false), quiet, 8);
			} else {
				// fill in the second part of the payload for C2 3200
				cframe = c2_queue.WaitPop();
				last = cframe.GetFlag();
				memcpy(pack.GetVoiceData(false), cframe.GetData(), 8);
			}
		}
		// TODO: do something with the 2nd half of the payload when it's voice + "data"

		uint16_t fn = count % 0x8000u;
		if (last)
			fn |= 0x8000u;
		pack.SetFrameNumber(fn);

		// add meta data:
		// GPS data first then the message
		const auto size = msgblks.size();
		switch (count)
		{
			case 6u:	// GPS data starts a FN 6, the second superframe
				if (gnss.IsValid())
				{
					TYPE.SetMetaDataType(EMetaDatType::gnss);
					gnss.CopyGnssMetaData(pMeta);
					pack.SetFrameType(TYPE.GetFrameType(EVersionType::legacy));
				}
				break;
			case 12u:	// Message data starts at the third superframe
				if (size > 0u) {
					TYPE.SetMetaDataType(EMetaDatType::text);
					pack.SetFrameType(TYPE.GetFrameType(EVersionType::legacy));
					memcpy(pMeta, msgblks[0].data, 14);
				} else if (gnss.IsValid()) {
					memset(pMeta, 0, 14);
					TYPE.SetMetaDataType(EMetaDatType::none);
					pack.SetFrameType(TYPE.GetFrameType(EVersionType::legacy));
				}
				break;
			case 18u:
				if (size > 1u)
					memcpy(pMeta, msgblks[1].data, 14);
				else if (1u == size) {
					memset(pMeta, 0, 14);
					TYPE.SetMetaDataType(EMetaDatType::none);
					pack.SetFrameType(TYPE.GetFrameType(EVersionType::legacy));
				}
				break;
			case 24u:
				if (size > 2u)
					memcpy(pMeta, msgblks[2].data, 14);
				else if (2u == size) {
					memset(pMeta, 0, 14);
					TYPE.SetMetaDataType(EMetaDatType::none);
					pack.SetFrameType(TYPE.GetFrameType(EVersionType::legacy));
				}
				break;
			case 30u:
				if (size > 3u)
					memcpy(pMeta, msgblks[3].data, 14);
				else if (3u == size)
					memset(pMeta, 0, 14);
				break;
			case 36u:	// and a largest message will end at the seventh superframe
				if (size == 4u) {
					memset(pMeta, 0, 14);
					TYPE.SetMetaDataType(EMetaDatType::none);
					pack.SetFrameType(TYPE.GetFrameType(EVersionType::legacy));
				}
				break;
			default:
				break;
		}

		pack.CalcCRC();

		AM2M17.Write(pack.GetCData(), pack.GetSize());

		count++;
	} while (! last);
}

void CAudioManager::mic2audio()
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

#ifdef USE44100
	// 44100 samples/second
	snd_pcm_hw_params_set_rate(handle, params, 44100, 0);
	snd_pcm_uframes_t frames = shrink.input_frames;
#else
	// 8000 samples/second
	snd_pcm_hw_params_set_rate(handle, params, 8000, 0);
	snd_pcm_uframes_t frames = 160;
#endif
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
#ifdef USE44100
		short int audio_frame[160];
#endif
		rc = snd_pcm_readi(handle, audio_buffer, frames);
		if (rc == -EPIPE) {
			// EPIPE means overrun
			std::cerr << "overrun occurred" << std::endl;
			snd_pcm_prepare(handle);
		} else if (rc < 0) {
			std::cerr << "error from readi: " << snd_strerror(rc) << std::endl;
#ifdef USE44100
		} else if (rc != int(shrink.input_frames)) {
#else
		} else if (rc != int(frames)) {
#endif
			std::cerr << "short readi, read " << rc << " frames" << std::endl;
		}
		keep_running = hot_mic;
#ifdef USE44100
		RSShrink.Short2Float(audio_buffer, shrink.data_in, shrink.input_frames);
		if (RSShrink.Process(shrink))
			keep_running = hot_mic = false;
		RSShrink.Float2Short(shrink.data_out, audio_frame, shrink.output_frames);
		CAudioFrame frame(audio_frame);
#else
		CAudioFrame frame(audio_buffer);
#endif
		frame.SetFlag(! keep_running);
		audio_queue.Push(frame);
	} while (keep_running);
#ifdef USE44100
	RSShrink.Reset();
#endif
	snd_pcm_drop(handle);
	snd_pcm_close(handle);
}

void CAudioManager::codec2audio(const bool is_3200)
{
	CCodec2 c2(is_3200);
	bool last;
	calc_audio_stats(); // init volume stats
	do {
		// we'll wait until there is something
		CC2DataFrame dataframe = c2_queue.WaitPop();
		last = dataframe.GetFlag();
		if (is_3200) {
			short audio[160];
			c2.codec2_decode(audio, dataframe.GetData());
			CAudioFrame audioframe(audio);
			audioframe.SetFlag(last);
			audio_queue.Push(audioframe);
			calc_audio_stats(audio);
		} else {
			short audio[320];	// C2 1600 is 40 ms audio
			c2.codec2_decode(audio, dataframe.GetData());
			CAudioFrame audio1(audio), audio2(audio+160);
			audio1.SetFlag(false);
			audio2.SetFlag(last);
			audio_queue.Push(audio1);
			audio_queue.Push(audio2);
			calc_audio_stats(audio);
			calc_audio_stats(audio+160);
		}
	} while (! last);
}

void CAudioManager::PlayEchoDataThread()
{
	auto data = pMainWindow->cfg.GetData();
	hot_mic = false;
	mic2audio_fut.get();
	audio2codec_fut.get();

	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	codec2audio_fut = std::async(std::launch::async, &CAudioManager::codec2audio, this, data->bVoiceOnlyEnable);
	play_audio_fut = std::async(std::launch::async, &CAudioManager::play_audio, this);
	codec2audio_fut.get();
	play_audio_fut.get();
}

void CAudioManager::M17_2AudioMgr(const CPacket &pack)
{
	static bool is_3200;
	if (! play_file) {
		if (0U==m17_sid_in && 0U==(pack.GetFrameNumber() & 0x8000u)) {	// don't start if it's the last audio frame
			// here comes a new stream
			m17_sid_in = pack.GetStreamId();
			is_3200 = ((pack.GetFrameType() & 0x6u) == 0x4u);
			pMainWindow->Receive(true);
			// launch the audio processing threads
			codec2audio_fut = std::async(std::launch::async, &CAudioManager::codec2audio, this, is_3200);
			play_audio_fut = std::async(std::launch::async, &CAudioManager::play_audio, this);
		}
		if (pack.GetStreamId() != m17_sid_in)
			return;
		CC2DataFrame dataframe(pack.GetCVoiceData());
		auto last = (0x8000u == (pack.GetFrameNumber() & 0x8000u));
		dataframe.SetFlag(is_3200 ? false : last);
		c2_queue.Push(dataframe);
		if (is_3200) {
			CC2DataFrame frame2(pack.GetCVoiceData(false));
			frame2.SetFlag(last);
			c2_queue.Push(frame2);
		}
		if (last) {
			codec2audio_fut.get();	// we're done, get the finished threads and reset the current stream id
			play_audio_fut.get();
			m17_sid_in = 0U;
			pMainWindow->Receive(false);
		}
	}
}

void CAudioManager::play_audio()
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

#ifdef USE44100
	// samples/second sampling rate
	snd_pcm_hw_params_set_rate(handle, params, 44100, 0);
	// Set period size to 160 frames.
	snd_pcm_uframes_t frames = expand.input_frames;
#else
	// samples/second sampling rate
	snd_pcm_hw_params_set_rate(handle, params, 8000, 0);
	// Set period size to 160 frames.
	snd_pcm_uframes_t frames = 160;
#endif
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
#ifdef USE44100
		short short_out[882];
#endif
		CAudioFrame frame(audio_queue.WaitPop());	// wait for a packet
		last = frame.GetFlag();
#ifdef USE44100
		RSExpand.Short2Float(frame.GetData(), expand.data_in, expand.input_frames);
		if (RSExpand.Process(expand))
			last = true;
		RSExpand.Float2Short(expand.data_out, short_out, expand.output_frames);
		rc = snd_pcm_writei(handle, short_out, expand.output_frames);
#else
		rc = snd_pcm_writei(handle, frame.GetData(), frames);
#endif
		if (rc == -EPIPE) {
			// EPIPE means underrun
			// std::cerr << "underrun occurred" << std::endl;
			snd_pcm_prepare(handle);
		} else if (rc < 0) {
			std::cerr <<  "error from writei: " << snd_strerror(rc) << std::endl;
#ifdef USE44100
		}  else if (rc != int(expand.output_frames)) {
#else
		}  else if (rc != int(frames)) {
#endif
			std::cerr << "short write, wrote " << rc << " frames" << std::endl;
		}
	} while (! last);

	snd_pcm_drain(handle);
	snd_pcm_close(handle);
#ifdef USE44100
	RSExpand.Reset();
#endif
}

void CAudioManager::KeyOff()
{
	if (hot_mic) {
		hot_mic = false;
		mic2audio_fut.get();
		audio2codec_fut.get();
		codec2gateway_fut.get();
	}
}

void CAudioManager::Link(const std::string &linkcmd)
{
	if (0 == linkcmd.compare(0, 3, "M17")) { //it's an M17 link/unlink command
		CPacket pack;
		pack.Initialize(54, true);
		if ('L' == linkcmd.at(3)) {
			if (13 == linkcmd.size()) {
				std::string sDest(linkcmd.substr(4));
				sDest[7] = 'L';
				CCallsign dest(sDest);
				dest.CodeOut(pack.GetDstAddress());
				//printf("Link Packet: dest=%s=0x%02x%02x%02x%02x%02x%02x\n", dest.GetCS().c_str(), pack.GetCDstAddress()[0], pack.GetCDstAddress()[1], pack.GetCDstAddress()[2], pack.GetCDstAddress()[3], pack.GetCDstAddress()[4], pack.GetCDstAddress()[5]);
				AM2M17.Write(pack.GetCData(), pack.GetSize());
			}
		} else if ('U' == linkcmd.at(3)) {
			CCallsign dest("U");
			dest.CodeOut(pack.GetDstAddress());
			AM2M17.Write(pack.GetCData(), pack.GetSize());
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
