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
#include <fstream>
#include <sstream>
#include <alsa/asoundlib.h>

#include "AudioManager.h"
#include "WaitCursor.h"
#include "SettingsDlg.h"
#include "QnetDB.h"
#include "MainWindow.h"

CSettingsDlg::CSettingsDlg() : pMainWindow(nullptr), pDlg(nullptr)
{
}

CSettingsDlg::~CSettingsDlg()
{
	if (pDlg)
		delete pDlg;
}

CFGDATA *CSettingsDlg::Show()
{
	pMainWindow->cfg.CopyTo(data);	// get the saved config data (MainWindow has alread read it)
	SetWidgetStates(data);
	on_AudioRescanButton_clicked();	// re-read the audio PCM devices

	if (Gtk::RESPONSE_OK == pDlg->run()) {
		CFGDATA newstate;						// the user clicked okay, time to look at what's changed
		SaveWidgetStates(newstate);				// newstate is now the current contents of the Settings Dialog
		pMainWindow->cfg.CopyFrom(newstate);	// and it is now in the global cfg object
		pMainWindow->cfg.WriteData();			// and it's saved in ~/.config/qdv/qdv.cfg

		// reconfigure current environment if anything changed
		pMainWindow->cfg.CopyTo(data);	// we need to return to the MainWindow a pointer to the new state
		pDlg->hide();
		return &data;					// changes to the irc client will be done in the MainWindow
	}
	pDlg->hide();
	return nullptr;
}

void CSettingsDlg::SaveWidgetStates(CFGDATA &d)
{
	// M17
	d.sM17SourceCallsign.assign(pM17SourceCallsignEntry->get_text());
	d.bVoiceOnlyEnable = pM17VoiceOnlyRadioButton->get_active();
	// station
	d.cModule = data.cModule;
	// Internet
	if (pIPv4RadioButton->get_active())
		d.eNetType = EInternetType::ipv4only;
	else if (pIPv6RadioButton->get_active())
		d.eNetType = EInternetType::ipv6only;
	else
		d.eNetType = EInternetType::dualstack;
	// audio
	Gtk::ListStore::iterator it = pAudioInputComboBox->get_active();
	Gtk::ListStore::Row row = *it;
	Glib::ustring s = row[audio_columns.name];
	d.sAudioIn.assign(s.c_str());
	it = pAudioOutputComboBox->get_active();
	row = *it;
	s = row[audio_columns.name];
	d.sAudioOut.assign(s.c_str());
}

void CSettingsDlg::SetWidgetStates(const CFGDATA &d)
{
	// M17
	if (d.bVoiceOnlyEnable)
		pM17VoiceOnlyRadioButton->clicked();
	else
		pM17VoiceDataRadioButton->clicked();
	pM17SourceCallsignEntry->set_text(d.sM17SourceCallsign);
	//quadnet
	switch (d.eNetType) {
		case EInternetType::ipv6only:
			pIPv6RadioButton->clicked();
			break;
		case EInternetType::dualstack:
			pDualStackRadioButton->clicked();
			break;
		default:
			pIPv4RadioButton->clicked();
			break;
	}
}

bool CSettingsDlg::Init(const Glib::RefPtr<Gtk::Builder> builder, const Glib::ustring &name, Gtk::Window *pWin, CMainWindow *pMain)
{
	pMainWindow = pMain;
	builder->get_widget(name, pDlg);
	if (nullptr == pDlg) {
		std::cerr << "Failed to initialize SettingsDialog!" << std::endl;
		return true;
	}
	pDlg->set_transient_for(*pWin);
	pDlg->add_button("Cancel", Gtk::RESPONSE_CANCEL);
	pOkayButton = pDlg->add_button("Okay", Gtk::RESPONSE_OK);
	pOkayButton->set_sensitive(false);

	// mode && notebook
	builder->get_widget("SettingsNotebook", pSettingsNotebook);
	// M17
	builder->get_widget("M17SourceCallsignEntry", pM17SourceCallsignEntry);
	builder->get_widget("M17VoiceOnlyRadioButton", pM17VoiceOnlyRadioButton);
	builder->get_widget("M17VoiceDataRadioButton", pM17VoiceDataRadioButton);
	// Internet
	builder->get_widget("IPv4RadioButton", pIPv4RadioButton);
	builder->get_widget("IPv6RadioButton", pIPv6RadioButton);
	builder->get_widget("DualStackRadioButton", pDualStackRadioButton);
	// Audio
	builder->get_widget("AudioInputComboBox", pAudioInputComboBox);
	refAudioInListModel = Gtk::ListStore::create(audio_columns);
	pAudioInputComboBox->set_model(refAudioInListModel);
	pAudioInputComboBox->pack_start(audio_columns.short_name);

	builder->get_widget("AudioOutputComboBox", pAudioOutputComboBox);
	refAudioOutListModel = Gtk::ListStore::create(audio_columns);
	pAudioOutputComboBox->set_model(refAudioOutListModel);
	pAudioOutputComboBox->pack_start(audio_columns.short_name);

	builder->get_widget("InputDescLabel", pInputDescLabel);
	builder->get_widget("OutputDescLabel", pOutputDescLabel);
	builder->get_widget("AudioRescanButton", pAudioRescanButton);

	pAudioRescanButton->signal_clicked().connect(sigc::mem_fun(*this, &CSettingsDlg::on_AudioRescanButton_clicked));
	pAudioInputComboBox->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_AudioInputComboBox_changed));
	pAudioOutputComboBox->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_AudioOutputComboBox_changed));
	pM17SourceCallsignEntry->signal_changed().connect(sigc::mem_fun(*this, &CSettingsDlg::on_M17SourceCallsignEntry_changed));

	return false;
}

void CSettingsDlg::on_M17SourceCallsignEntry_changed()
{
	int pos = pM17SourceCallsignEntry->get_position();
	Glib::ustring s = pM17SourceCallsignEntry->get_text().uppercase();
	pM17SourceCallsignEntry->set_text(s);
	pM17SourceCallsignEntry->set_position(pos);
	bM17Source = std::regex_match(s.c_str(), pMainWindow->M17CallRegEx);
	pM17SourceCallsignEntry->set_icon_from_icon_name(bM17Source ? "gtk-ok" : "gtk-cancel");
	pOkayButton->set_sensitive(bM17Source);
}

void CSettingsDlg::on_AudioInputComboBox_changed()
{
	Gtk::ListStore::iterator iter = pAudioInputComboBox->get_active();
	if (iter) {
		Gtk::ListStore::Row row = *iter;
		if (row) {
			Glib::ustring name = row[audio_columns.name];
			Glib::ustring desc = row[audio_columns.desc];

			data.sAudioIn.assign(name.c_str());
			pInputDescLabel->set_text(desc);
		}
	}
}

void CSettingsDlg::on_AudioOutputComboBox_changed()
{
	Gtk::ListStore::iterator iter = pAudioOutputComboBox->get_active();
	if (iter) {
		Gtk::ListStore::Row row = *iter;
		if (row) {
			Glib::ustring name = row[audio_columns.name];
			Glib::ustring desc = row[audio_columns.desc];

			data.sAudioOut.assign(name.c_str());
			pOutputDescLabel->set_text(desc);
		}
	}
}

void CSettingsDlg::on_AudioRescanButton_clicked()
{
	if (0 == data.sAudioIn.size())
		data.sAudioIn.assign("default");
	if (0 == data.sAudioOut.size())
		data.sAudioOut.assign("default");
	void **hints;
	if (snd_device_name_hint(-1, "pcm", &hints) < 0)
		return;
	void **n = hints;
	refAudioInListModel->clear();
	refAudioOutListModel->clear();
	while (*n != NULL) {
		char *name = snd_device_name_get_hint(*n, "NAME");
		if (NULL == name)
			continue;
		char *desc = snd_device_name_get_hint(*n, "DESC");
		if (NULL == desc) {
			free(name);
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

			Glib::ustring short_name(name);
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
					Gtk::ListStore::Row row = *(refAudioInListModel->append());
					row[audio_columns.short_name] = short_name;
					row[audio_columns.name] = name;
					row[audio_columns.desc] = desc;
					if (0==data.sAudioIn.compare(name))
						pAudioInputComboBox->set_active(row);
					snd_pcm_close(handle);
				}
			}

			if (is_output) {
				snd_pcm_t *handle;
				if (snd_pcm_open(&handle, name, SND_PCM_STREAM_PLAYBACK, 0) == 0) {
					Gtk::ListStore::Row row = *(refAudioOutListModel->append());
					row[audio_columns.short_name] = short_name;
					row[audio_columns.name] = name;
					row[audio_columns.desc] = desc;
					if (0==data.sAudioOut.compare(name))
						pAudioOutputComboBox->set_active(row);
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
