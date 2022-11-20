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
#include <opendht.h>

#include "FLTK-GUI.h"
#include "Configure.h"
#include "M17Gateway.h"
#include "M17RouteMap.h"
#include "SettingsDlg.h"
#include "AboutDlg.h"
#include "AudioManager.h"
#include "TransmitButton.h"

struct SReflectorData0
{
	std::string cs, ipv4;
	std::string ipv6, mods, url, email;
	uint16_t port;
	std::vector<std::pair<std::string, std::string>> peers;

	MSGPACK_DEFINE(cs, ipv4, ipv6, mods, url, email, port, peers);
};

struct SReflectorData1
{
	std::string cs, ipv4;
	std::string ipv6, mods, emods, url, email;
	std::string sponsor, country;
	uint16_t port;
	std::vector<std::pair<std::string, std::string>> peers;

	MSGPACK_DEFINE(cs, ipv4, ipv6, mods, emods, url, email, sponsor, country, port, peers);
};

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
	CSettingsDlg SettingsDlg;
	CAboutDlg AboutDlg;
	CM17Gateway gateM17;

	// Distributed Hash Table
	dht::DhtRunner node;
	dht::Value nodevalue;

	// widgets
	Fl_Double_Window *pWin;
	CTransmitButton *pPTTButton, *pEchoTestButton;
	Fl_Button *pQuickKeyButton, *pActionButton, *pConnectButton, *pDisconnectButton, *pDashboardButton;
	Fl_Input *pDestCallsignInput, *pDestIPInput;
	Fl_Int_Input *pDestPortInput;
	Fl_Group *pModuleGroup;
	Fl_Radio_Round_Button *pModuleRadioButton[26];
	Fl_Menu_Bar *pMenuBar;
	Fl_Text_Display *pTextDisplay;
	Fl_Text_Buffer  *pTextBuffer;
	Fl_PNG_Image *pIcon;

	// state data
	CFGDATA cfgdata;
	std::mutex logmux;

	// helpers
	void BuildDestMenuButton();
	void FixDestActionButton();
	void SetDestActionButton(const bool sensitive, const char *label);
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
	char GetDestinationModule();
	void SetDestinationAddress(std::string &cs);
	void Get(const std::string &cs);
	void ActivateModules(const std::string &modules = "ABCDEFGHIJKLMNOPQRSTUVWXYZ");

	// Actual Callbacks
	void Quit();
	void ShowSettingsDialog();
	void ShowAboutDialog();
	void EchoButton();
	void PTTButton();
	void QuickKeyButton();
	void DestCallsignInput();
	void DestIPInput();
	void DestPortInput();
	void DestMenuButton();
	void ActionButton();
	void LinkButton();
	void UnlinkButton();
	void DashboardButton();
	// Static wrapper for callbacks
	static void QuitCB(Fl_Widget *p, void *v);
	static void ShowSettingsDialogCB(Fl_Widget *p, void *v);
	static void ShowAboutDialogCB(Fl_Widget *p, void *v);
	static void EchoButtonCB(Fl_Widget *p, void *v);
	static void PTTButtonCB(Fl_Widget *p, void *v);
	static void QuickKeyButttonCB(Fl_Widget *, void *);
	static void DestCallsignInputCB(Fl_Widget *p, void *v);
	static void DestIPInputCB(Fl_Widget *p, void *v);
	static void DestPortInputCB(Fl_Widget *p, void *v);
	static void DestMenuButtonCB(Fl_Widget *p, void *v);
	static void ActionButtonCB(Fl_Widget *p, void *v);
	static void LinkButtonCB(Fl_Widget *p, void *v);
	static void UnlinkButtonCB(Fl_Widget *p, void *v);
	static void DashboardButtonCB(Fl_Widget *p, void *v);

	bool bDestCS, bDestIP, bDestPort, bTransOK;
	std::atomic<bool> keep_running;
};
