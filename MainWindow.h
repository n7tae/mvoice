/*
 *   Copyright (c) 2019-2022 by Thomas A. Early N7TAE
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

#include <regex>
#include <future>

#include "FLTK-GUI.h"
#include "Configure.h"
#include "M17Gateway.h"
#include "SettingsDlg.h"
#include "AudioManager.h"
#include "M17RouteMap.h"

class CMainWindow
{
public:
	CMainWindow();
	~CMainWindow();

	CConfigure cfg;
	CAudioManager AudioManager;

	bool Init();
	void Run();
	void Receive(bool is_rx);
	// regular expression for testing stuff
	std::regex IPv4RegEx, IPv6RegEx, M17CallRegEx, M17RefRegEx;

private:
	// classes
	CSettingsDlg SettingsDlg;
	CM17RouteMap routeMap;
	CM17Gateway gateM17;

	// widgets
	Fl_Double_Window *pWin;
	Fl_Button *pPTTButton, *pEchoTestButton, *pQuickKeyButton, *pActionButton, *pLinkButton, *pUnlinkButton, *pDashboardButton;
	Fl_Input *pDestCallsignInput, *pDestIPInput;
	Fl_Choice *pDestinationChoice;
	Fl_Box *pModuleLabel;
	Fl_Group *pModuleGroup;
	Fl_Round_Button *pModuleRadioButton[26];
	Fl_Menu_Bar *pMenuBar;
	Fl_Menu_Item *pSettingsMenuItem;
	Fl_Text_Display *pTextDisplay;
	Fl_Text_Buffer  *pTextBuffer;

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
	bool RelayM172AM();
	bool GetLogInput();
	bool TimeoutProcess();

	bool bDestCS, bDestIP, bTransOK;
};
