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

#include <regex>
#include <future>

#include "Configure.h"
#include "M17Gateway.h"
#include "QnetDB.h"
#include "SettingsDlg.h"
#include "AboutDlg.h"
#include "AudioManager.h"
#include "M17RouteMap.h"

class CMainWindow
{
public:
	CMainWindow();
	~CMainWindow();

	CConfigure cfg;
	CAudioManager AudioManager;

	bool Init(const Glib::RefPtr<Gtk::Builder>, const Glib::ustring &);
	void Run();
	void Receive(bool is_rx);
	// regular expression for testing stuff
	std::regex IPv4RegEx, IPv6RegEx, M17CallRegEx, M17RefRegEx;

private:
	// classes
	CSettingsDlg SettingsDlg;
	CAboutDlg AboutDlg;
	CQnetDB qnDB;
	CM17RouteMap routeMap;
	CM17Gateway gateM17;

	// widgets
	Gtk::Window *pWin;
	Gtk::Button *pQuitButton, *pSettingsButton, *pQuickKeyButton, *pM17DestActionButton, *pM17LinkButton, *pM17UnlinkButton, *pDashboardButton;
	Gtk::ComboBoxText *pM17DestCallsignComboBox;
	Gtk::RadioButton *pModuleRadioButton[26];
	Gtk::Entry *pM17DestCallsignEntry, *pM17DestIPEntry;
	Gtk::ToggleButton *pEchoTestButton, *pPTTButton;
	Gtk::MenuItem *pAboutMenuItem;
	Glib::RefPtr<Gtk::TextBuffer> pLogTextBuffer;
	Gtk::ScrolledWindow *pScrolledWindow;
	Gtk::TextView *pLogTextView;

	// state data
	CFGDATA cfgdata;

	// helpers
	void FixM17DestActionButton();
	void SetDestActionButton(const bool sensitive, const char *label);
	std::future<void> futM17;
	void SetState();
	void RunM17();
	void StopM17();
	CUnixDgramReader Gate2AM, Link2AM, M172AM, LogInput;
	void CloseAll();
	void insertLogText(const char *line);
	void AudioSummary(const char *title);
	char GetDestinationModule();
	void SetDestinationAddress(std::string &cs);
	void SetModuleSensitive(const std::string &dest);

	// events
	void on_QuitButton_clicked();
	void on_SettingsButton_clicked();
	void on_EchoTestButton_toggled();
	void on_PTTButton_toggled();
	void on_QuickKeyButton_clicked();
	void on_AboutMenuItem_activate();
	void on_M17DestCallsignEntry_changed();
	void on_M17DestIPEntry_changed();
	void on_M17DestCallsignComboBox_changed();
	void on_M17DestActionButton_clicked();
	void on_M17LinkButton_clicked();
	void on_M17UnlinkButton_clicked();
	void on_DashboardButton_clicked();
	bool RelayM172AM(Glib::IOCondition condition);
	bool GetLogInput(Glib::IOCondition condition);
	bool TimeoutProcess();

	bool bDestCS, bDestIP, bTransOK;
};
