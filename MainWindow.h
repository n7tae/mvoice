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
#include <atomic>
#include <mutex>
#ifndef NO_DHT
#include <opendht.h>
#endif

#include "FLTK-GUI.h"
#include "Configure.h"
#include "M17Gateway.h"
#include "M17RouteMap.h"
#include "SMSDlg.h"
#include "SettingsDlg.h"
#include "AboutDlg.h"
#include "AudioManager.h"
#include "TransmitButton.h"

class CMainWindow
{
public:
	CMainWindow();
	~CMainWindow();

	CConfigure cfg;
	CAudioManager AudioManager;

	bool Init();
	void Run(int argc, char *argv[]);
	void Receive(bool is_rx);
	void NewSettings(CFGDATA *newdata);
	void UpdateGUI();

	// helpers
	bool ToUpper(std::string &s);

	// regular expression for testing stuff
	std::regex IPv4RegEx, IPv6RegEx, M17CallRegEx, M17RefRegEx;

private:
	// classes
	CSMSDlg SMSDlg;
	CSettingsDlg SettingsDlg;
	CAboutDlg AboutDlg;
	CM17Gateway gateM17;

#ifndef NO_DHT
	// Distributed Hash Table
	dht::DhtRunner node;
	dht::Value nodevalue;
	const std::string exportNodeFilename;
#endif

	// widgets
	Fl_Double_Window *pWin;
	CTransmitButton *pPTTButton, *pEchoTestButton;
	Fl_Button *pQuickKeyButton, *pActionButton, *pConnectButton, *pDisconnectButton, *pDashboardButton;
	Fl_Input *pTargetCSInput, *pTargetIpInput;
	Fl_Input *pDSTCallsignInput;
	Fl_Int_Input *pTargetPortInput;
	Fl_Group *pModuleGroup;
	Fl_Radio_Round_Button *pModuleRadioButton[26];
	Fl_Menu_Bar *pMenuBar;
	Fl_Text_Display *pTextDisplay;
	Fl_Text_Buffer  *pTextBuffer;
	Fl_RGB_Image *pIcon;

	// state data
	CFGDATA cfgdata;
	std::mutex logmux;

	// helpers
	void BuildTargetMenuButton();
	void FixTargetMenuButton();
	void SetTargetMenuButton(const bool sensitive, const char *label);
	void TransmitterButtonControl();
	std::future<void> futM17;
	std::future<void> futReadThread;
	void SetState();
	void RunM17();
	void StopM17();
	void ReadThread();
	CUnixDgramReader M172AM, LogInput;
	void CloseAll();
	void insertLogText(const char *line);
	void AudioSummary(const char *title);
	char GetTargetModule();
	void SetTargetAddress(std::string &cs);
#ifndef NO_DHT
	void Get(const std::string &cs);
#endif
	void ActivateModules(const std::string &modules = "ABCDEFGHIJKLMNOPQRSTUVWXYZ");

	// Actual Callbacks
	void Quit();
	void ShowSMSDialog();
	void ShowSettingsDialog();
	void ShowAboutDialog();
	void EchoButton();
	void PTTButton();
	void QuickKeyButton();
	void TargetCSInput();
	void TargetIPInput();
	void DestinationCSInput();
	void TargetPortInput();
	void TargetMenuButton();
	void ActionButton();
	void LinkButton();
	void UnlinkButton();
	void DashboardButton();
	// Static wrapper for callbacks
	static void QuitCB(Fl_Widget *p, void *v);
	static void ShowSMSDialogCB(Fl_Widget *p, void *v);
	static void ShowSettingsDialogCB(Fl_Widget *p, void *v);
	static void ShowAboutDialogCB(Fl_Widget *p, void *v);
	static void EchoButtonCB(Fl_Widget *p, void *v);
	static void PTTButtonCB(Fl_Widget *p, void *v);
	static void QuickKeyButttonCB(Fl_Widget *, void *);
	static void TargetCSInputCB(Fl_Widget *p, void *v);
	static void TargetIPInputCB(Fl_Widget *p, void *v);
	static void TargetPortInputCB(Fl_Widget *p, void *v);
	static void TargetMenuButtonCB(Fl_Widget *p, void *v);
	static void DestinationCSInputCB(Fl_Widget *p, void *v);
	static void ActionButtonCB(Fl_Widget *p, void *v);
	static void LinkButtonCB(Fl_Widget *p, void *v);
	static void UnlinkButtonCB(Fl_Widget *p, void *v);
	static void DashboardButtonCB(Fl_Widget *p, void *v);

	bool bDestCS, bTargetCS, bTargetIP, bTargetPort, bTransOK;
	std::atomic<bool> keep_running;
};
