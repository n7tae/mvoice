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
	pMainWindow->cfg.CopyTo(data);	// get the saved config data (MainWindow has alread read it)
	SetWidgetStates(data);
	AudioRescanButtonCB(nullptr, nullptr);	// re-read the audio PCM devices
	pDlg->show();
}

void CSettingsDlg::OkayButtonCB(Fl_Widget *, void *This)
{
	((CSettingsDlg *)This)->OkayButton();
}

void CSettingsDlg::OkayButton()
{
	CFGDATA newstate;						// the user clicked okay, time to look at what's changed
	SaveWidgetStates(newstate);				// newstate is now the current contents of the Settings Dialog
	// pMainWindow->cfg.CopyFrom(newstate);	// and it is now in the global cfg object
	// pMainWindow->cfg.WriteData();			// and it's saved in ~/.config/qdv/qdv.cfg

	// reconfigure current environment if anything changed
	// pMainWindow->cfg.CopyTo(data);	// we need to return to the MainWindow a pointer to the new state
	pDlg->hide();
	pMainWindow->NewSettings(&newstate);
}

void CSettingsDlg::CancelButtonCB(Fl_Widget *, void *This)
{
	((CSettingsDlg *)This)->CancelButton();
}

void CSettingsDlg::CancelButton()
{
	pDlg->hide();
}

void CSettingsDlg::SaveWidgetStates(CFGDATA &d)
{
	// M17
	d.sM17SourceCallsign.assign(pSourceCallsignInput->value());
	d.bVoiceOnlyEnable = 1u == pVoiceOnlyRadioButton->value();
	// station
	d.cModule = data.cModule;
	// Internet
	if (pIPv4RadioButton->active())
		d.eNetType = EInternetType::ipv4only;
	else if (pIPv6RadioButton->active())
		d.eNetType = EInternetType::ipv6only;
	else
		d.eNetType = EInternetType::dualstack;
	// audio
	const std::string in(pAudioInputChoice->text());
	auto itin = AudioInMap.find(in);
	if (AudioInMap.end() != itin)
	{
		data.sAudioIn.assign(itin->second.first);
	}
	const std::string out(pAudioInputChoice->text());
	auto itout = AudioOutMap.find(out);
	if (AudioOutMap.end() != itout)
	{
		data.sAudioOut.assign(itout->second.first);
	}
	// interface
	d.bIsPTT = 1u == pPTTRadioButton->value();
}

void CSettingsDlg::SetWidgetStates(const CFGDATA &d)
{
	// M17
	if (d.bVoiceOnlyEnable)
		pVoiceOnlyRadioButton->setonly();
	else
		pVoiceDataRadioButton->setonly();
	pSourceCallsignInput->value(d.sM17SourceCallsign.c_str());
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
	// interface
	if (d.bIsPTT)
		pPTTRadioButton->setonly();
	else
		pTTTRadioButton->setonly();
}

bool CSettingsDlg::Init(Fl_Double_Window */*unused*/, CMainWindow *pMain)
{
	pMainWindow = pMain;
	pDlg = new Fl_Double_Window(450, 345, gettext("Settings"));

	pTabs = new Fl_Tabs(24, 15, 406, 250);
	pTabs->labelsize(20);

	pStationGroup = new Fl_Group(24, 34, 401, 195, gettext("Station"));
	pStationGroup->labelsize(20);
	pStationGroup->hide();

	pSourceCallsignInput = new Fl_Input(218, 51, 127, 30, gettext("My Callsign:"));
	pSourceCallsignInput->tooltip(gettext("Input your callsign, up to 8 characters"));
	pSourceCallsignInput->color(FL_RED);
	pSourceCallsignInput->labelsize(20);
	pSourceCallsignInput->textsize(20);
	pSourceCallsignInput->callback(&CSettingsDlg::SourceCallsignInputCB, this);

	pCodecGroup = new Fl_Group(110, 118, 245, 105, gettext("Codec:"));
	pCodecGroup->box(FL_THIN_UP_BOX);
	pCodecGroup->labelsize(20);

	pVoiceOnlyRadioButton = new Fl_Round_Button(159, 149, 175, 25, gettext("Voice-only"));
	pVoiceOnlyRadioButton->tooltip(gettext("This is the higher quality, 3200 bits/s codec"));
	pVoiceOnlyRadioButton->down_box(FL_ROUND_DOWN_BOX);
	pVoiceOnlyRadioButton->labelsize(20);

	pVoiceDataRadioButton = new Fl_Round_Button(159, 180, 160, 33, gettext("Voice+Data"));
	pVoiceDataRadioButton->tooltip(gettext("This is the 1600 bits/s codec"));
	pVoiceDataRadioButton->down_box(FL_ROUND_DOWN_BOX);
	pVoiceDataRadioButton->labelsize(20);
	pCodecGroup->end();
	pStationGroup->end();

	pAudioGroup = new Fl_Group(24, 35, 401, 226, gettext("Audio"));
	pAudioGroup->tooltip(gettext("Select the audio Input device"));
	pAudioGroup->labelsize(20);
	pAudioGroup->hide();

	pAudioInputChoice = new Fl_Choice(134, 51, 260, 24, gettext("Input:"));
	pAudioInputChoice->tooltip(gettext("Select your audio input device, usually \"default\""));
	pAudioInputChoice->down_box(FL_BORDER_BOX);
	pAudioInputChoice->labelsize(20);
	pAudioInputChoice->textsize(20);
	pAudioInputChoice->callback(&CSettingsDlg::AudioInputChoiceCB, this);

	pAudioInputDescBox = new Fl_Box(45, 82, 355, 28, gettext("input description"));
	pAudioInputDescBox->labelsize(20);

	pAudioOutputChoice = new Fl_Choice(134, 122, 260, 24, gettext("Output:"));
	pAudioOutputChoice->tooltip(gettext("Select the audio output device, usually \"default\""));
	pAudioOutputChoice->down_box(FL_BORDER_BOX);
	pAudioOutputChoice->labelsize(20);
	pAudioOutputChoice->textsize(20);
	pAudioOutputChoice->callback(&CSettingsDlg::AudioOutputChoiceCB, this);

	pAudioOutputDescBox = new Fl_Box(45, 156, 360, 33, gettext("output description"));
	pAudioOutputDescBox->labelsize(20);

	pAudioRescanButton = new Fl_Button(192, 198, 90, 43, gettext("Rescan"));
	pAudioRescanButton->tooltip(gettext("Rescan for new audio devices"));
	pAudioRescanButton->labelsize(20);

	pAudioGroup->end();

	pInternetGroup = new Fl_Group(30, 45, 400, 220, gettext("Internet"));
	pInternetGroup->labelsize(20);
	pInternetGroup->hide();

	pIPv4RadioButton = new Fl_Round_Button(146, 59, 218, 34, gettext("IPv4 Only"));
	pIPv4RadioButton->down_box(FL_ROUND_DOWN_BOX);
	pIPv4RadioButton->labelsize(20);

	pIPv6RadioButton = new Fl_Round_Button(146, 113, 218, 34, gettext("IPv6 Only"));
	pIPv6RadioButton->down_box(FL_ROUND_DOWN_BOX);
	pIPv6RadioButton->labelsize(20);

	pDualStackRadioButton = new Fl_Round_Button(146, 170, 218, 34, gettext("IPv4 && IPv6"));
	pDualStackRadioButton->down_box(FL_ROUND_DOWN_BOX);
	pDualStackRadioButton->labelsize(20);

	pInternetGroup->end();

	pGUIGroup = new Fl_Group(45, 40, 360, 160, gettext("GUI"));
	pGUIGroup->labelsize(20);

	pPTTRadioButton = new Fl_Round_Button(144, 80, 195, 35, gettext("Push to Talk"));
	pPTTRadioButton->down_box(FL_ROUND_DOWN_BOX);
	pPTTRadioButton->labelsize(20);

	pTTTRadioButton = new Fl_Round_Button(144, 133, 195, 35, gettext("Toggle  to Talk"));
	pTTTRadioButton->down_box(FL_ROUND_DOWN_BOX);
	pTTTRadioButton->labelsize(20);

	pGUIGroup->end();

	pOkayButton = new Fl_Return_Button(310, 280, 120, 44, gettext("Okay"));
	pOkayButton->labelsize(20);
	pOkayButton->callback(&CSettingsDlg::OkayButtonCB, this);

	pCancelButton = new Fl_Button(150, 280, 120, 44, gettext("Cancel"));
	pCancelButton->labelsize(20);
	pCancelButton->callback(&CSettingsDlg::CancelButtonCB, this);

	pDlg->set_modal();
	pDlg->end();

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
		pSourceCallsignInput->activate();
	}
	else
	{
		pSourceCallsignInput->color(FL_RED);
		pSourceCallsignInput->deactivate();
	}
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
	if (0 == data.sAudioIn.size())
		data.sAudioIn.assign("default");
	if (0 == data.sAudioOut.size())
		data.sAudioOut.assign("default");
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
					AudioInMap.emplace(short_name, std::pair<std::string, std::string>(name, desc));
					snd_pcm_close(handle);
				}
			}

			if (is_output) {
				snd_pcm_t *handle;
				if (snd_pcm_open(&handle, name, SND_PCM_STREAM_PLAYBACK, 0) == 0) {
					AudioOutMap.emplace(short_name, std::pair<std::string, std::string>(name, desc));
					snd_pcm_close(handle);
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
}
