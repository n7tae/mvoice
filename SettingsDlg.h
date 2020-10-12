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

#include <gtkmm.h>

#include "Configure.h"

class CMainWindow;

class CSettingsDlg
{
public:
    CSettingsDlg();
    ~CSettingsDlg();
    bool Init(const Glib::RefPtr<Gtk::Builder>, const Glib::ustring &, Gtk::Window *, CMainWindow *);
    CFGDATA *Show();	// returns a pointer to the private CFGDATA if okay is pressed, otherwise a nullptr

protected:
	class AudioColumns : public Gtk::ListStore::ColumnRecord
	{
	public:
		AudioColumns() { add(short_name); add(name); add(desc); };
		Gtk::TreeModelColumn<Glib::ustring> short_name;
		Gtk::TreeModelColumn<Glib::ustring> name;
		Gtk::TreeModelColumn<Glib::ustring> desc;
	};
	AudioColumns audio_columns;
	Glib::RefPtr<Gtk::ListStore> refAudioInListModel, refAudioOutListModel;
private:
	// persistance
	void SaveWidgetStates(CFGDATA &d);
	void SetWidgetStates(const CFGDATA &d);
	// data classes
	CFGDATA data;
	// other data
	bool bCallsign, bStation, bM17Source;
	// Windows
	CMainWindow *pMainWindow;
    Gtk::Dialog *pDlg;
	// widgets
	Gtk::Button *pAMBERescanButton, *pOkayButton, *pAudioRescanButton;
	Gtk::ComboBox *pAudioInputComboBox, *pAudioOutputComboBox;
	Gtk::CheckButton *pUseMyCallCheckButton, *pDPlusEnableCheckButton, *pAPRSEnableCheckButton, *pGPSDEnableCheckButton, *pLinkingCheckButton, *pRoutingCheckbutton, *pIPv4CheckButton, *pIPv6CheckButton;
	Gtk::Entry *pStationCallsignEntry, *pMyCallsignEntry, *pMyNameEntry, *pMessageEntry, *pLocationEntry[2], *pURLEntry, *pLatitudeEntry, *pLongitudeEntry, *pLinkAtStartEntry, *pAPRSServerEntry, *pAPRSPortEntry, *pAPRSIntervalEntry, *pGPSDServerEntry, *pGPSDPortEntry, *pM17SourceCallsignEntry;
	Gtk::RadioButton *p230kRadioButton, *p460kRadioButton, *pCodec2RadioButton, *pAMBERadioButton, *pM17VoiceOnlyRadioButton, *pM17VoiceDataRadioButton;
	Gtk::Label *pDevicePathLabel, *pProductIDLabel, *pVersionLabel, *pInputDescLabel, *pOutputDescLabel;
	Gtk::Notebook *pSettingsNotebook;
	// events
	void on_M17SourceCallsignEntry_changed();
	void on_Codec2RadioButton_clicked();
	void on_AMBERadioButton_clicked();
	void on_LinkingCheckButton_toggled();
	void on_RoutingCheckButton_toggled();
	void on_UseMyCallsignCheckButton_toggled();
	void on_AudioRescanButton_clicked();
	void on_MyCallsignEntry_changed();
	void on_MyNameEntry_changed();
	void on_StationCallsignEntry_changed();
	void On20CharMsgChanged(Gtk::Entry *pEntry);
	void on_MessageEntry_changed();
	void on_Location1Entry_changed();
	void on_Location2Entry_changed();
	void OnLatLongChanged(Gtk::Entry *pEntry);
	void on_LatitudeEntry_changed();
	void on_LongitudeEntry_changed();
	void on_URLEntry_changed();
	void OnServerChanged(Gtk::Entry *pEntry);
	void on_AMBERescanButton_clicked();
	void on_IPv4CheckButton_clicked();
	void on_IPv6CheckButton_clicked();
	void on_LinkAtStartEntry_changed();
	void on_AudioInputComboBox_changed();
	void on_AudioOutputComboBox_changed();
	void on_APRSServerEntry_changed();
	void on_GPSDServerEntry_changed();
	void OnIntegerChanged(Gtk::Entry *pEntry);
	void on_APRSPortEntry_changed();
	void on_APRSIntervalEntry_changed();
	void on_GPSDPortEntry_changed();
	void on_APRSEnableCheckButton_toggled();
	void on_GPSDEnableCheckButton_toggled();
	// state changed
	void BaudrateChanged(int newBaudrate);
};
