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

#define _(STRING) gettext(STRING)

static const char *notfoundstr = _(" not found");

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

void CSettingsDlg::UpdateButtonCB(Fl_Widget *, void *This)
{
	((CSettingsDlg *)This)->UpdateButton();
}

void CSettingsDlg::UpdateButton()
{
	pDlg->hide();
	CFGDATA newstate;						// the user clicked okay, time to look at what's changed
	SaveWidgetStates(newstate);				// newstate is now the current contents of the Settings Dialog
#ifndef NO_DHT
	if (newstate.sBootstrap.compare(pMainWindow->cfg.GetData()->sBootstrap))
	{
		fl_message("Please restart to use new bootstrap");
	}
#endif
	pMainWindow->cfg.CopyFrom(newstate);	// and it is now in the global cfg object
	pMainWindow->cfg.WriteData();			// and it's saved in the config dir

	// reconfigure current environment if anything changed
	pMainWindow->cfg.CopyTo(data);
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
	const char *p_in = pAudioInputChoice->text();
	if (p_in != NULL)
	{
		const std::string in(p_in);
		auto itin = AudioInMap.find(in);
		if (AudioInMap.end() != itin)
		{
			d.sAudioIn.assign(itin->second.first);
		}
	}
	const char *p_out = pAudioOutputChoice->text();
	if (p_out != NULL)
	{
		const std::string out(p_out);
		auto itout = AudioOutMap.find(out);
		if (AudioOutMap.end() != itout)
		{
			d.sAudioOut.assign(itout->second.first);
		}
	}
#ifndef NO_DHT
	d.sBootstrap.assign(pBootstrapInput->value());
#endif
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
	pModuleChoice->value(d.cModule - 'A');
#ifndef NO_DHT
	pBootstrapInput->value(d.sBootstrap.c_str());
#endif
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
	pDlg = new Fl_Double_Window(450, 350, _("Settings"));

	pTabs = new Fl_Tabs(10, 10, 430, 240);
	pTabs->labelsize(16);

	//////////////////////////////////////////////////////////////////////////
	pStationGroup = new Fl_Group(20, 30, 410, 210, _("Station"));
	pStationGroup->labelsize(16);

	pSourceCallsignInput = new Fl_Input(218, 45, 127, 30, _("My Callsign:"));
	pSourceCallsignInput->tooltip(_("Input your callsign, up to 8 characters"));
	pSourceCallsignInput->color(FL_RED);
	pSourceCallsignInput->labelsize(16);
	pSourceCallsignInput->textsize(16);
	pSourceCallsignInput->when(FL_WHEN_CHANGED);
	pSourceCallsignInput->callback(&CSettingsDlg::SourceCallsignInputCB, this);

	pModuleChoice = new Fl_Choice(218, 85, 50, 30, _("Using Module:"));
	pModuleChoice->down_box(FL_BORDER_BOX);
	pModuleChoice->tooltip(_("Assign the transceiver module"));
	pModuleChoice->labelsize(16);
	pModuleChoice->textsize(16);
	pModuleChoice->callback(&CSettingsDlg::ModuleChoiceCB, this);
	for (char c='A'; c<='Z'; c++)
	{
		const std::string l(1, c);
		pModuleChoice->add(l.c_str());
	}

	pCodecGroup = new Fl_Group(110, 138, 245, 105, "Codec:");
	pCodecGroup->box(FL_THIN_UP_BOX);
	pCodecGroup->labelsize(16);

	pVoiceOnlyRadioButton = new Fl_Radio_Round_Button(160, 155, 150, 30, _("Voice-only"));
	pVoiceOnlyRadioButton->tooltip(_("This is the higher quality, 3200 bits/s codec"));
	pVoiceOnlyRadioButton->labelsize(16);

	pVoiceDataRadioButton = new Fl_Radio_Round_Button(160, 195, 150, 30, _("Voice+Data"));
	pVoiceDataRadioButton->tooltip(_("This is the 1600 bits/s codec"));
	pVoiceDataRadioButton->labelsize(16);
	pCodecGroup->end();
	pStationGroup->end();
	pTabs->add(pStationGroup);

	//////////////////////////////////////////////////////////////////////////
	pInternetGroup = new Fl_Group(20, 30, 410, 210, _("Network"));
	pInternetGroup->labelsize(16);

	pIPv4RadioButton = new Fl_Radio_Round_Button(145, 60, 200, 30, _("IPv4 Only"));
	pIPv4RadioButton->labelsize(16);

	pIPv6RadioButton = new Fl_Radio_Round_Button(145, 110, 200, 30, _("IPv6 Only"));
	pIPv6RadioButton->labelsize(16);

	pDualStackRadioButton = new Fl_Radio_Round_Button(145, 160, 200, 30, _("IPv4 && IPv6"));
	pDualStackRadioButton->labelsize(16);

	pInternetGroup->end();
	pTabs->add(pInternetGroup);

	///////////////////////////////////////////////////////////////////////////
#ifndef NO_DHT
	pDHTGroup = new Fl_Group(20, 30, 410, 210, _("DHT"));

	pBootstrapInput = new Fl_Input(170, 120, 227, 30, _("DHT Bootstrap:"));
	pBootstrapInput->tooltip(_("An existing node on the DHT Network"));

	pDHTGroup->end();
	pTabs->add(pDHTGroup);
#endif

	//////////////////////////////////////////////////////////////////////////
	pAudioGroup = new Fl_Group(20, 30, 410, 210, _("Audio"));
	pAudioGroup->tooltip(_("Select the audio Input and Output devices"));
	pAudioGroup->labelsize(16);

	pAudioInputChoice = new Fl_Choice(135, 50, 260, 24, _("Input:"));
	pAudioInputChoice->tooltip(_("Select your audio input device, usually \"default\""));
	pAudioInputChoice->down_box(FL_BORDER_BOX);
	pAudioInputChoice->labelsize(16);
	pAudioInputChoice->textsize(16);
	pAudioInputChoice->callback(&CSettingsDlg::AudioInputChoiceCB, this);

	pAudioInputDescBox = new Fl_Box(30, 80, 390, 25, "input description");
	pAudioInputDescBox->labelsize(12);

	pAudioOutputChoice = new Fl_Choice(135, 120, 260, 24, _("Output:"));
	pAudioOutputChoice->tooltip(_("Select the audio output device, usually \"default\""));
	pAudioOutputChoice->down_box(FL_BORDER_BOX);
	pAudioOutputChoice->labelsize(16);
	pAudioOutputChoice->textsize(16);
	pAudioOutputChoice->callback(&CSettingsDlg::AudioOutputChoiceCB, this);

	pAudioOutputDescBox = new Fl_Box(30, 150, 390, 25, "output description");
	pAudioOutputDescBox->labelsize(12);

	pAudioRescanButton = new Fl_Button(192, 186, 90, 40, _("Rescan"));
	pAudioRescanButton->tooltip(_("Rescan for new audio devices"));
	pAudioRescanButton->labelsize(16);

	pAudioGroup->end();
	pTabs->add(pAudioGroup);

	pTabs->end();

	pOkayButton = new Fl_Return_Button(310, 280, 120, 44, _("Update"));
	pOkayButton->labelsize(16);
	pOkayButton->callback(&CSettingsDlg::UpdateButtonCB, this);

	pDlg->end();
	pDlg->set_modal();

	return false;
}

void CSettingsDlg::ModuleChoiceCB(Fl_Widget *, void *This)
{
	((CSettingsDlg *)This)->ModuleChoice();
}

void CSettingsDlg::ModuleChoice()
{
	const std::string selected(pModuleChoice->text());
	data.cModule = selected.at(0);
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
	const char *p_in = pAudioInputChoice->text();
	if (p_in == NULL)
	{
		pAudioInputDescBox->label(NULL);
		return;
	}
	const std::string selected(p_in);
	auto it = AudioInMap.find(selected);
	if (AudioInMap.end() == it)
	{
		data.sAudioIn.assign(_("ERROR"));
		pAudioInputDescBox->label(std::string(selected+notfoundstr).c_str());

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
	const char *p_out = pAudioOutputChoice->text();
	if (p_out == NULL)
	{
		pAudioOutputDescBox->label(NULL);
		return;
	}
	const std::string selected(p_out);
	auto it = AudioOutMap.find(selected);
	if (AudioOutMap.end() == it)
	{
		data.sAudioOut.assign(_("ERROR"));
		pAudioOutputDescBox->label(std::string(selected+notfoundstr).c_str());

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
					std::cerr << _("ERROR: unexpected IOID=") << io << std::endl;
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
