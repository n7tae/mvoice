/*
 *   Copyright (c) 2019-2021 by Thomas A. Early N7TAE
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
#include <fstream>
#include <sstream>
#include <alsa/asoundlib.h>

#include "AudioManager.h"
#include "SettingsDlg.h"
#include "MainWindow.h"

CSettingsDlg::CSettingsDlg() : pMainWindow(nullptr), pDlg(nullptr)
{
}

CSettingsDlg::~CSettingsDlg()
{
	if (pDlg)
		delete pDlg;
}

void CSettingsDlg::Show()
{
	CConfigure cfg;
	pMainWindow->cfg.CopyTo(data);	// get the saved config data (MainWindow has alread read it)
	SetWidgetStates(data);
	pDlg->show();
	pTabs->value(pStationGroup);
	AudioRescanButton();	// re-read the audio PCM devices
}

void CSettingsDlg::OkayButtonCB(Fl_Widget *, void *This)
{
	((CSettingsDlg *)This)->OkayButton();
}

void CSettingsDlg::OkayButton()
{
	CFGDATA newstate;						// the user clicked okay, time to look at what's changed
	SaveWidgetStates(newstate);				// newstate is now the current contents of the Settings Dialog
	pMainWindow->cfg.CopyFrom(newstate);	// and it is now in the global cfg object
	pMainWindow->cfg.WriteData();			// and it's saved in the config dir

	// reconfigure current environment if anything changed
	pMainWindow->cfg.CopyTo(data);
	pDlg->hide();
	pMainWindow->NewSettings(&newstate);
}

void CSettingsDlg::SaveWidgetStates(CFGDATA &d)
{
	// M17
	d.sM17SourceCallsign.assign(pSourceCallsignInput->value());
	d.bVoiceOnlyEnable = 1u == pVoiceOnlyRadioButton->value();
	// station
	d.cModule = data.cModule;
	// Internet
	if (pIPv4RadioButton->value())
		d.eNetType = EInternetType::ipv4only;
	else if (pIPv6RadioButton->value())
		d.eNetType = EInternetType::ipv6only;
	else
		d.eNetType = EInternetType::dualstack;
	// audio
	const std::string in(pAudioInputChoice->text());
	auto itin = AudioInMap.find(in);
	if (AudioInMap.end() != itin)
	{
		d.sAudioIn.assign(itin->second.first);
	}
	const std::string out(pAudioInputChoice->text());
	auto itout = AudioOutMap.find(out);
	if (AudioOutMap.end() != itout)
	{
		d.sAudioOut.assign(itout->second.first);
	}
}

void CSettingsDlg::SetWidgetStates(const CFGDATA &d)
{
	// M17
	if (d.bVoiceOnlyEnable)
		pVoiceOnlyRadioButton->setonly();
	else
		pVoiceDataRadioButton->setonly();
	pSourceCallsignInput->value(d.sM17SourceCallsign.c_str());
	SourceCallsignInput();
	// internet
	switch (d.eNetType) {
		case EInternetType::ipv6only:
			pIPv6RadioButton->setonly();
			break;
		case EInternetType::dualstack:
			pDualStackRadioButton->setonly();
			break;
		default:
			pIPv4RadioButton->setonly();
			break;
	}
}

bool CSettingsDlg::Init(CMainWindow *pMain)
{
	pMainWindow = pMain;
	pDlg = new Fl_Double_Window(450, 350, "Settings");

	pTabs = new Fl_Tabs(10, 10, 430, 240);
	pTabs->labelsize(16);

	//////////////////////////////////////////////////////////////////////////
	pStationGroup = new Fl_Group(20, 30, 410, 210, "Station");
	pStationGroup->labelsize(16);

	pSourceCallsignInput = new Fl_Input(218, 51, 127, 30, "My Callsign:");
	pSourceCallsignInput->tooltip("Input your callsign, up to 8 characters");
	pSourceCallsignInput->color(FL_RED);
	pSourceCallsignInput->labelsize(16);
	pSourceCallsignInput->textsize(16);
	pSourceCallsignInput->when(FL_WHEN_CHANGED);
	pSourceCallsignInput->callback(&CSettingsDlg::SourceCallsignInputCB, this);

	pCodecGroup = new Fl_Group(110, 118, 245, 105, "Codec:");
	pCodecGroup->box(FL_THIN_UP_BOX);
	pCodecGroup->labelsize(16);

	pVoiceOnlyRadioButton = new Fl_Radio_Round_Button(160, 135, 150, 30, "Voice-only");
	pVoiceOnlyRadioButton->tooltip("This is the higher quality, 3200 bits/s codec");
	pVoiceOnlyRadioButton->labelsize(16);

	pVoiceDataRadioButton = new Fl_Radio_Round_Button(160, 175, 150, 30, "Voice+Data");
	pVoiceDataRadioButton->tooltip("This is the 1600 bits/s codec");
	pVoiceDataRadioButton->labelsize(16);
	pCodecGroup->end();
	pStationGroup->end();
	pTabs->add(pStationGroup);

	//////////////////////////////////////////////////////////////////////////
	pAudioGroup = new Fl_Group(20, 30, 410, 210, "Audio");
	pAudioGroup->tooltip("Select the audio Input device");
	pAudioGroup->labelsize(16);

	pAudioInputChoice = new Fl_Choice(135, 50, 260, 24, "Input:");
	pAudioInputChoice->tooltip("Select your audio input device, usually \"default\"");
	pAudioInputChoice->down_box(FL_BORDER_BOX);
	pAudioInputChoice->labelsize(16);
	pAudioInputChoice->textsize(16);
	pAudioInputChoice->callback(&CSettingsDlg::AudioInputChoiceCB, this);

	pAudioInputDescBox = new Fl_Box(30, 80, 390, 25, "input description");
	pAudioInputDescBox->labelsize(12);

	pAudioOutputChoice = new Fl_Choice(135, 120, 260, 24, "Output:");
	pAudioOutputChoice->tooltip("Select the audio output device, usually \"default\"");
	pAudioOutputChoice->down_box(FL_BORDER_BOX);
	pAudioOutputChoice->labelsize(16);
	pAudioOutputChoice->textsize(16);
	pAudioOutputChoice->callback(&CSettingsDlg::AudioOutputChoiceCB, this);

	pAudioOutputDescBox = new Fl_Box(30, 150, 390, 25, "output description");
	pAudioOutputDescBox->labelsize(12);

	pAudioRescanButton = new Fl_Button(192, 186, 90, 40, "Rescan");
	pAudioRescanButton->tooltip("Rescan for new audio devices");
	pAudioRescanButton->labelsize(16);

	pAudioGroup->end();
	pTabs->add(pAudioGroup);

	//////////////////////////////////////////////////////////////////////////
	pInternetGroup = new Fl_Group(20, 30, 410, 210, "Internet");
	pInternetGroup->labelsize(16);

	pIPv4RadioButton = new Fl_Radio_Round_Button(145, 60, 200, 30, "IPv4 Only");
	pIPv4RadioButton->labelsize(16);

	pIPv6RadioButton = new Fl_Radio_Round_Button(145, 110, 200, 30, "IPv6 Only");
	pIPv6RadioButton->labelsize(16);

	pDualStackRadioButton = new Fl_Radio_Round_Button(145, 160, 200, 30, "IPv4 && IPv6");
	pDualStackRadioButton->labelsize(16);

	pInternetGroup->end();
	pTabs->add(pInternetGroup);

	pTabs->end();

	pOkayButton = new Fl_Return_Button(310, 280, 120, 44, "Update");
	pOkayButton->labelsize(16);
	pOkayButton->callback(&CSettingsDlg::OkayButtonCB, this);

	pDlg->end();
	pDlg->set_modal();

	return false;
}

void CSettingsDlg::SourceCallsignInputCB(Fl_Widget *, void *This)
{
	((CSettingsDlg *)This)->SourceCallsignInput();
}

void CSettingsDlg::SourceCallsignInput()
{
	auto pos = pSourceCallsignInput->position();
	std::string s(pSourceCallsignInput->value());
	if (pMainWindow->ToUpper(s))
	{
		pSourceCallsignInput->value(s.c_str());
		pSourceCallsignInput->position(pos);
	}
	bM17Source = std::regex_match(s.c_str(), pMainWindow->M17CallRegEx);
	if (bM17Source)
	{
		pSourceCallsignInput->color(FL_GREEN);
		pOkayButton->activate();
	}
	else
	{
		pSourceCallsignInput->color(FL_RED);
		pOkayButton->deactivate();
	}
	pSourceCallsignInput->damage(FL_DAMAGE_ALL);
}

void CSettingsDlg::AudioInputChoiceCB(Fl_Widget *, void *This)
{
	((CSettingsDlg *)This)->AudioInputChoice();
}

void CSettingsDlg::AudioInputChoice()
{
	const std::string selected(pAudioInputChoice->text());
	auto it = AudioInMap.find(selected);
	if (AudioInMap.end() == it)
	{
		data.sAudioIn.assign("ERROR");
		pAudioInputDescBox->label(std::string(selected+" not found!").c_str());

	}
	else
	{
		data.sAudioIn.assign(it->second.first);
		pAudioInputDescBox->label(it->second.second.c_str());
	}
}

void CSettingsDlg::AudioOutputChoiceCB(Fl_Widget *, void *This)
{
	((CSettingsDlg *)This)->AudioOutputChoice();
}

void CSettingsDlg::AudioOutputChoice()
{
	const std::string selected(pAudioOutputChoice->text());
	auto it = AudioOutMap.find(selected);
	if (AudioOutMap.end() == it)
	{
		data.sAudioOut.assign("ERROR");
		pAudioOutputDescBox->label(std::string(selected+" not found!").c_str());

	}
	else
	{
		data.sAudioOut.assign(it->second.first);
		pAudioOutputDescBox->label(it->second.second.c_str());
	}
}

void CSettingsDlg::AudioRescanButtonCB(Fl_Widget *, void *This)
{
	((CSettingsDlg *)This)->AudioRescanButton();
}

void CSettingsDlg::AudioRescanButton()
{
	auto inchoice = pAudioInputChoice->value();
	if (inchoice < 0)
		inchoice = 0;
	auto outchoice = pAudioOutputChoice->value();
	if (outchoice < 0)
		outchoice = 0;
	void **hints;
	if (snd_device_name_hint(-1, "pcm", &hints) < 0)
		return;
	void **n = hints;
	pAudioInputChoice->clear();
	pAudioOutputChoice->clear();
	AudioInMap.clear();
	AudioOutMap.clear();
	while (*n != NULL) {
		char *name = snd_device_name_get_hint(*n, "NAME");
		if (NULL == name) {
			n++;
			continue;
		}
		char *desc = snd_device_name_get_hint(*n, "DESC");
		if (NULL == desc) {
			free(name);
			n++;
			continue;
		}
		if ((0==strcmp(name, "default") || strstr(name, "plughw")) && NULL==strstr(desc, "without any conversions")) {

			char *io = snd_device_name_get_hint(*n, "IOID");
			bool is_input = true, is_output = true;

			if (io) {	// io == NULL means it's for both input and output
				if (0 == strcasecmp(io, "Input")) {
					is_output = false;
				} else if (0 == strcasecmp(io, "Output")) {
					is_input = false;
				} else {
					std::cerr << "ERROR: unexpected IOID=" << io << std::endl;
				}
				free(io);
			}

			std::string short_name(name);
			auto pos = short_name.find("plughw:CARD=");
			if (short_name.npos != pos) {
				short_name = short_name.replace(pos, 12, "");
				pos = short_name.find(",DEV=0");
				if (short_name.npos != pos)
					short_name = short_name.replace(pos, 6, "");
				if (0 == short_name.size())
					short_name.assign(name);
			}
			if (is_input) {
				snd_pcm_t *handle;
				if (snd_pcm_open(&handle, name, SND_PCM_STREAM_CAPTURE, 0) == 0) {
					AudioInMap[short_name] = std::pair<std::string, std::string>(name, desc);
					snd_pcm_close(handle);
					pAudioInputChoice->add(short_name.c_str());
				}
			}

			if (is_output) {
				snd_pcm_t *handle;
				if (snd_pcm_open(&handle, name, SND_PCM_STREAM_PLAYBACK, 0) == 0) {
					AudioOutMap[short_name] = std::pair<std::string, std::string>(name, desc);
					snd_pcm_close(handle);
					pAudioOutputChoice->add(short_name.c_str());
				}
			}

		}

	    if (name) {
	      	free(name);
		}
		if (desc) {
			free(desc);
		}
		n++;
	}
	snd_device_name_free_hint(hints);
	pAudioInputChoice->value(inchoice);
	AudioInputChoice();
	pAudioOutputChoice->value(outchoice);
	AudioOutputChoice();
}
