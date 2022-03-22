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

#include <curl/curl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <chrono>
#include <cmath>

#include "MainWindow.h"
#include "Utilities.h"
#include "TemplateClasses.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp/"
#endif

static void MyIdleProcess(void *p)
{
	CMainWindow *pMainWindow = (CMainWindow *)p;
	pMainWindow->UpdateGUI();

	Fl::repeat_timeout(1.0, MyIdleProcess, pMainWindow);
}

CMainWindow::CMainWindow() :
	pWin(nullptr),
	pSettingsMenuItem(nullptr),
	bDestCS(false),
	bDestIP(false),
	bTransOK(true)
{
	cfg.CopyTo(cfgdata);
	// allowed M17 " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."
	IPv4RegEx = std::regex("^((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])\\.){3,3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9]){1,1}$", std::regex::extended);
	IPv6RegEx = std::regex("^(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}(:[0-9a-fA-F]{1,4}){1,1}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|([0-9a-fA-F]{1,4}:){1,1}(:[0-9a-fA-F]{1,4}){1,6}|:((:[0-9a-fA-F]{1,4}){1,7}|:))$", std::regex::extended);
	M17RefRegEx = std::regex("^(M17-|URF)([A-Z0-9]){3,3}$", std::regex::extended);
	M17CallRegEx = std::regex("^[0-9]{0,1}[A-Z]{1,2}[0-9][A-Z]{1,4}(()|[ ]*[A-Z]|([-/\\.][A-Z0-9]*))$", std::regex::extended);
}

CMainWindow::~CMainWindow()
{
	if (futReadThread.valid())
	{
		keep_running = false;
		futReadThread.get();
	}
	StopM17();
	if (pWin)
		delete pWin;
}

void CMainWindow::RunM17()
{
	std::cout << "Starting M17 Gateway..." << std::endl;
	if (! gateM17.Init(cfgdata))
		gateM17.Process();
	std::cout << "M17 Gateway has stopped." << std::endl;
}

void CMainWindow::SetState()
{
	if (cfg.IsOkay() && false == gateM17.keep_running)
		futM17 = std::async(std::launch::async, &CMainWindow::RunM17, this);

	// set up the destination combo box
	const std::string current(pDestCallsignInput->value());
	pDestinationChoice->clear();
	for (const auto &cs : routeMap.GetKeys()) {
		const auto host = routeMap.Find(cs);
		if (host) {
			switch (cfgdata.eNetType) {
				case EInternetType::ipv6only:
					if (! host->ip6addr.empty())
						pDestinationChoice->add(cs.c_str());
					break;
				case EInternetType::ipv4only:
					if (! host->ip4addr.empty())
						pDestinationChoice->add(cs.c_str());
					break;
				default:
					pDestinationChoice->add(cs.c_str());
					break;
			}
		}
	}
	if (routeMap.Find(current))
		pDestinationChoice->value(pDestinationChoice->find_index(current.c_str()));
}

void CMainWindow::CloseAll()
{
	M172AM.Close();
	LogInput.Close();
}

bool CMainWindow::Init()
{
	keep_running = true;
	futReadThread = std::async(std::launch::async, &CMainWindow::ReadThread, this);

	bool menu__i18n_done = false;
	Fl_Menu_Item theMenu[] = {
 		{ "Settings...", 0, &CMainWindow::ShowSettingsDialogCB, this, 0, 0, 0, 0, 0 },
		{ "About...",    0, &CMainWindow::ShowAboutDialogCB,    this, 0, 0, 0, 0, 0 },
		{ 0,             0, 0,                                     0, 0, 0, 0, 0, 0 }
	};

	std::string iconpath(CFG_DIR);
	iconpath.append("mvoice48.png");
	pIcon = new Fl_PNG_Image(iconpath.c_str());

	switch( pIcon->fail())
	{
	case Fl_Image::ERR_NO_IMAGE:
	case Fl_Image::ERR_FILE_ACCESS:
		fl_alert("%s: %s", iconpath.c_str(), strerror(errno));    // shows actual os error to user
		return true;
	case Fl_Image::ERR_FORMAT:
		fl_alert("%s: couldn't decode image", iconpath.c_str());
		return true;
	}

	if (M172AM.Open("m172am")) {
		CloseAll();
		return true;
	}

	if (LogInput.Open("log_input")) {
		CloseAll();
		return true;
	}

	if (AudioManager.Init(this)) {
		CloseAll();
		return true;
	}

	pWin = new Fl_Double_Window(900, 640, "mvoice");
	pWin->icon(pIcon);
	pWin->box(FL_BORDER_BOX);
	pWin->size_range(718, 440);
	pWin->callback(&CMainWindow::QuitCB, this);
	//Fl::visual(FL_DOUBLE|FL_INDEX);

	pMenuBar = new Fl_Menu_Bar(0, 0, 900, 30);
	pMenuBar->labelsize(16);
	if (! menu__i18n_done)
	{
		int i = 0;
		while (theMenu[i].label())
		{
			theMenu[i].label(theMenu[i].label());
			i++;
		}
		menu__i18n_done = true;
	}
	pMenuBar->copy(theMenu);

	pTextBuffer = new Fl_Text_Buffer();
	pTextDisplay = new Fl_Text_Display(16, 30, 872, 314);
	pTextDisplay->buffer(pTextBuffer);
	pTextDisplay->end();

	pWin->resizable(pTextDisplay);

	pDestCallsignInput = new Fl_Input(245, 360, 149, 30, "Destination Callsign:");
	pDestCallsignInput->tooltip("A reflector or user callsign");
	pDestCallsignInput->color(FL_RED);
	pDestCallsignInput->labelsize(16);
	pDestCallsignInput->textsize(16);
	pDestCallsignInput->when(FL_WHEN_CHANGED);
	pDestCallsignInput->callback(&CMainWindow::DestCallsignInputCB, this);

	pDestIPInput = new Fl_Input(460, 360, 424, 30, "IP:");
	pDestIPInput->tooltip("The IP of the reflector or user");
	pDestIPInput->color(FL_RED);
	pDestIPInput->labelsize(16);
	pDestIPInput->textsize(16);
	pDestIPInput->when(FL_WHEN_CHANGED);
	pDestIPInput->callback(&CMainWindow::DestIPInputCB, this);

	pModuleGroup = new Fl_Group(177, 400, 546, 60, "Module:");
	pModuleGroup->tooltip("Select a module for the reflector or repeater");
	pModuleGroup->labelsize(16);
	pModuleGroup->align(FL_ALIGN_LEFT);
	pModuleGroup->begin();
	static const char *modlabel[26] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z" };
	for (int y=0; y<2; y++)
	{
		for (int x=0; x<13; x++)
		{
			const int i = 13 * y + x;
			pModuleRadioButton[i] = new Fl_Radio_Round_Button(x*42+177, y*30+400, 30, 25, modlabel[i]);
			pModuleRadioButton[i]->down_box(FL_ROUND_DOWN_BOX);
			pModuleRadioButton[i]->labelsize(16);
		}
	}
	pModuleGroup->end();
	pModuleRadioButton[0]->setonly();

	pDestinationChoice = new Fl_Choice(276, 474, 164, 30, "Destination:");
	pDestinationChoice->tooltip("Select a saved contact (reflector or user)");
	pDestinationChoice->down_box(FL_BORDER_BOX);
	pDestinationChoice->labelsize(16);
	pDestinationChoice->textsize(16);
	pDestinationChoice->callback(&CMainWindow::DestChoiceCB, this);

	pActionButton = new Fl_Button(455, 472, 80, 30, "Action");
	pActionButton->tooltip("Update or delete an existing contact,,or save a new contact");
	pActionButton->labelsize(16);
	pActionButton->deactivate();
	pActionButton->callback(&CMainWindow::ActionButtonCB, this);

	pDashboardButton = new Fl_Button(600, 474, 201, 30, "Open Dashboard");
	pDashboardButton->tooltip("Open a reflector dashboard, if available");
	pDashboardButton->labelsize(16);
	pDashboardButton->deactivate();
	pDashboardButton->callback(&CMainWindow::DashboardButtonCB, this);

	pLinkButton = new Fl_Button(358, 514, 80, 30, "Link");
	pLinkButton->tooltip("Connect to an M17 Reflector");
	pLinkButton->labelsize(16);
	pLinkButton->deactivate();
	pLinkButton->callback(&CMainWindow::LinkButtonCB, this);

	pUnlinkButton = new Fl_Button(456, 514, 80, 30, "Unlink");
	pUnlinkButton->tooltip("Disconnected from a reflector");
	pUnlinkButton->labelsize(16);
	pUnlinkButton->deactivate();
	pUnlinkButton->callback(&CMainWindow::UnlinkButtonCB, this);

	pEchoTestButton = new CTransmitButton(50, 574, 144, 40, "Echo Test");
	pEchoTestButton->tooltip("Push to record a test that will be played back");
	pEchoTestButton->labelsize(16);
	pEchoTestButton->selection_color(FL_YELLOW);
	pEchoTestButton->callback(&CMainWindow::EchoButtonCB, this);

	pPTTButton = new CTransmitButton(250, 557, 400, 60, "PTT");
	pPTTButton->tooltip("Push to talk");
	pPTTButton->labelsize(22);
	pPTTButton->deactivate();
	pPTTButton->selection_color(FL_YELLOW);
	pPTTButton->callback(&CMainWindow::PTTButtonCB, this);

	pQuickKeyButton = new Fl_Button(700, 574, 150, 40, "Quick Key");
	pQuickKeyButton->tooltip("Send a short, silent voice stream");
	pQuickKeyButton->labelsize(16);
	pQuickKeyButton->deactivate();
	pQuickKeyButton->callback(QuickKeyButttonCB, this);

	pWin->end();

	if (SettingsDlg.Init(this))
	{
		CloseAll();
		return true;
	}

	if (AboutDlg.Init(pIcon))
	{
		CloseAll();
		return true;
	}

	routeMap.ReadAll();
	Receive(false);
	SetState();
	DestChoice();

	// idle processing
	Fl::add_timeout(1.0, MyIdleProcess, this);

	return false;
}

void CMainWindow::Run(int argc, char *argv[])
{
	pWin->show(argc, argv);
}

void CMainWindow::QuitCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->Quit();
}

void CMainWindow::Quit()
{
	AudioManager.KeyOff();
	StopM17();

	if (pWin)
		pWin->hide();
}

void CMainWindow::ShowAboutDialogCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->ShowAboutDialog();
}

void CMainWindow::ShowAboutDialog()
{
	AboutDlg.Show();
}

void CMainWindow::ShowSettingsDialogCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->ShowSettingsDialog();
}

void CMainWindow::ShowSettingsDialog()
{
	SettingsDlg.Show();
}

void CMainWindow::NewSettings(CFGDATA *newdata)
{
	if (newdata) {	// the user clicked okay so if anything changed. We'll shut things down and let SetState start things up again
		if (newdata->sM17SourceCallsign.compare(cfgdata.sM17SourceCallsign) || newdata->eNetType!=cfgdata.eNetType) {
			StopM17();
		}
		cfg.CopyTo(cfgdata);
	}
	SetState();
}

void CMainWindow::DestChoiceCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->DestChoice();
}

void CMainWindow::DestChoice()
{
	auto i = pDestinationChoice->value();
	if (i >= 0)
	{
		auto cs = pDestinationChoice->text(i);
		pDestCallsignInput->value(cs);
		auto host = routeMap.Find(cs);
		if (host) {
			if (EInternetType::ipv4only!=cfgdata.eNetType && !host->ip6addr.empty())
				// if we're not in IPv4-only mode && there is an IPv6 address for this host
				pDestIPInput->value(host->ip6addr.c_str());
			else
				pDestIPInput->value(host->ip4addr.c_str());
		}
	}
}

void CMainWindow::ActionButtonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->ActionButton();
}

void CMainWindow::ActionButton()
{
	const std::string label(pActionButton->label());
	auto cs = pDestCallsignInput->value();
	if (0 == label.compare("Save")) {
		const std::string a(pDestIPInput->value());
		if (std::string::npos == a.find(':'))
			routeMap.Update(cs, "", a, "");
		else
			routeMap.Update(cs, "", "", a);
		pDestinationChoice->clear();
		for (const auto &member : routeMap.GetKeys())
			pDestinationChoice->add(member.c_str());
		auto i = pDestinationChoice->find_index(cs);
		if (i >= 0)
			pDestinationChoice->value(i);
	} else if (0 == label.compare("Delete")) {
		auto index = pDestinationChoice->value();
		pDestinationChoice->remove(index);
		routeMap.Erase(cs);
		if (index >= int(routeMap.Size()))
			index--;
		if (index < 0) {
			pDestinationChoice->value(-1);
			pDestIPInput->value("");
		} else
			pDestinationChoice->value(index);
	} else if (0 == label.compare("Update")) {
		std::string a(pDestIPInput->value());
		if (std::string::npos == a.find(':'))
			routeMap.Update(cs, "", a, "");
		else
			routeMap.Update(cs, "", "", a);
	}
	FixDestActionButton();
	routeMap.Save();
}

void CMainWindow::AudioSummary(const char *title)
{
		char line[640];
		double t = AudioManager.volStats.count * 0.000125;	// 0.000125 = 1 / 8000
		// we only do the sums of squares on every other point, so 0.5 mult in denominator
		// 25 db subtration for "ambient quiet", an arbitrary reference point
		double d = 20.0 * log10(sqrt(AudioManager.volStats.ss/(0.5 * AudioManager.volStats.count))) - 25.0;
		double c = 100.0 * AudioManager.volStats.clip / AudioManager.volStats.count;
		snprintf(line, 64, "%s Time=%.1fs Vol=%.0fdB Clip=%.0f%%\n", title, t, d, c);
		insertLogText(line);
}

void CMainWindow::EchoButtonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->EchoButton();
}

void CMainWindow::EchoButton()
{
	pEchoTestButton->toggle();
	if (pEchoTestButton->value()) {
		// record the mic to a queue
		AudioManager.RecordMicThread(E_PTT_Type::echo, "ECHOTEST");
	} else {
		// pEchoTestButton->deactivate();	// don't allow user to try to start a new test when it's still playing the current test
		AudioSummary("Echo");
		// play back the queue
		AudioManager.PlayEchoDataThread();
		// pEchoTestButton->activate();
	}
}

void CMainWindow::Receive(bool is_rx)
{
	bTransOK = ! is_rx;
	if (bTransOK && bDestCS && bDestIP)
	{
		pPTTButton->activate();
		pQuickKeyButton->activate();
	}
	else
	{
		pPTTButton->deactivate();
		pQuickKeyButton->deactivate();
	}

	if (bTransOK)
		pEchoTestButton->activate();
	else
		pEchoTestButton->deactivate();

	if (bTransOK && AudioManager.volStats.count)
		AudioSummary("RX Audio");
}

void CMainWindow::SetDestinationAddress(std::string &cs)
{
	cs.assign(pDestCallsignInput->value());
	const std::string ip(pDestIPInput->value());
	uint16_t port = 17000;	// we need to get the port from the routeMap in case it's not 17000
	auto host = routeMap.Find(cs);
	if (host)
		port = host->port;
	gateM17.SetDestAddress(ip, port);
	if (0==cs.compare(0, 4, "M17-") || 0==cs.compare(0, 3, "URF")) {
		cs.resize(8, ' ');
		cs.append(1, GetDestinationModule());
	}
}

void CMainWindow::PTTButtonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->PTTButton();
}

void CMainWindow::PTTButton()
{
	pPTTButton->toggle();
	if (pPTTButton->value()) {
		if (gateM17.TryLock())
		{
			std::string cs;
			SetDestinationAddress(cs);
			AudioManager.RecordMicThread(E_PTT_Type::m17, cs);
		}
		else
		{
			pPTTButton->value(0);
		}
	}
	else
	{
		AudioManager.KeyOff();
		AudioSummary("PTT");
		gateM17.ReleaseLock();
	}
}

void CMainWindow::QuickKeyButton()
{
	std::string cs;
	SetDestinationAddress(cs);
	AudioManager.QuickKey(cs, cfgdata.sM17SourceCallsign);
}

void CMainWindow::QuickKeyButttonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->QuickKeyButton();
}

void CMainWindow::ReadThread()
{
	while (keep_running)
	{
		auto gatefd = M172AM.GetFD();
		auto logfd  = LogInput.GetFD();
		auto fdmax = gatefd;
		if (logfd > fdmax)
			fdmax = logfd;
		fd_set fdset;
		FD_ZERO(&fdset);
		FD_SET(gatefd, &fdset);
		FD_SET(logfd, &fdset);
		timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 100000;	// wait up to 100 ms for something

		auto ret = select(fdmax+1, &fdset, 0, 0, &tv);
		if (ret < 0)
		{
			std::cout << "M17Relay select() error - " << strerror(errno) << std::endl;
		}
		else if (ret > 0)
		{
			if (FD_ISSET(gatefd, &fdset))
			{
				SM17Frame frame;
				M172AM.Read(frame.magic, sizeof(SM17Frame));
				if (0 == memcmp(frame.magic, "M17 ", 4))
					AudioManager.M17_2AudioMgr(frame);
			}
			if (FD_ISSET(logfd, &fdset))
			{
				char line[256] = { 0 };
				LogInput.Read(line, 256);
				insertLogText(line);
			}
		}
	}
}

void CMainWindow::insertLogText(const char *line)
{
	pTextBuffer->append(line);
	pTextDisplay->insert_position(pTextBuffer->length());
	pTextDisplay->show_insert_position();
}

void CMainWindow::UpdateGUI()
{
	if (ELinkState::linked != gateM17.GetLinkState()) {
		pUnlinkButton->deactivate();
		std::string s(pDestCallsignInput->value());
		if (std::regex_match(s, M17RefRegEx) && bDestIP)
			pLinkButton->activate();
		else
			pLinkButton->deactivate();
	} else {
		pLinkButton->deactivate();
		if (bDestIP)
			pUnlinkButton->activate();
		else
			pUnlinkButton->deactivate();
	}
	pPTTButton->UpdateLabel();
	pEchoTestButton->UpdateLabel();
}

void CMainWindow::SetModuleSensitive(const std::string &dest)
{
	const bool state = (0==dest.compare(0, 4, "M17-") || 0==dest.compare(0, 3, "URF")) ? true : false;
	for (unsigned i=0; i<26; i++)
	{
		if (state)
			pModuleRadioButton[i]->activate();
		else
			pModuleRadioButton[i]->deactivate();
	}
}

bool CMainWindow::ToUpper(std::string &s)
{
	bool rval = false;
	for (auto it=s.begin(); it!=s.end(); it++)
	{
		if (islower(*it))
		{
			rval = true;
			*it = toupper(*it);
		}
	}
	return rval;
}

void CMainWindow::DestCallsignInputCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->DestCallsignInput();
}

void CMainWindow::DestCallsignInput()
{
	auto pos = pDestCallsignInput->position();
	std::string s(pDestCallsignInput->value());
	if (ToUpper(s))
	{
		pDestCallsignInput->value(s.c_str());
		pDestCallsignInput->position(pos);
	}
	SetModuleSensitive(s.c_str());
	auto is_valid_reflector = std::regex_match(s.c_str(), M17RefRegEx);
	bDestCS = std::regex_match(s.c_str(), M17CallRegEx) || is_valid_reflector;
	const auto host = routeMap.Find(s);
	if (host)
	{
		if (EInternetType::ipv4only!=cfgdata.eNetType && !host->ip6addr.empty())
			pDestIPInput->value(host->ip6addr.c_str());
		else if (!host->ip4addr.empty())
			pDestIPInput->value(host->ip4addr.c_str());
	}
	if (is_valid_reflector && host && !host->url.empty())
		pDashboardButton->activate();
	else
		pDashboardButton->deactivate();
	pDestCallsignInput->color(bDestCS ? 2 : 1);
	FixDestActionButton();
	pDestCallsignInput->damage(FL_DAMAGE_ALL);
	DestIpInput();
}

void CMainWindow::DestIPInputCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->DestIpInput();
}

void CMainWindow::DestIpInput()
{
	auto bIP4 = std::regex_match(pDestIPInput->value(), IPv4RegEx);
	auto bIP6 = std::regex_match(pDestIPInput->value(), IPv6RegEx);
	switch (cfgdata.eNetType) {
		case EInternetType::ipv4only:
			bDestIP = bIP4;
			break;
		case EInternetType::ipv6only:
			bDestIP = bIP6;
			break;
		default:
			bDestIP = (bIP4 || bIP6);
	}
	pDestIPInput->color(bDestIP ? 2 : 1);
	FixDestActionButton();
	pDestIPInput->damage(FL_DAMAGE_ALL);
}

void CMainWindow::SetDestActionButton(const bool sensitive, const char *label)
{
	if (sensitive)
		pActionButton->activate();
	else
		pActionButton->deactivate();
	pActionButton->label(label);
}

void CMainWindow::LinkButtonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->LinkButton();
}

void CMainWindow::LinkButton()
{
	while (cfgdata.sM17SourceCallsign.empty()) {
		insertLogText("ERROR: Your system is not yet configured!\n");
		insertLogText("Be sure to save your callsign and internet access in Settings\n");
		insertLogText("You can usually leave the audio devices as 'default'\n");
		return;
	}
	std::string cmd("M17L");
	std::string cs;
	SetDestinationAddress(cs);
	cmd.append(cs);
	AudioManager.Link(cmd);
}

void CMainWindow::UnlinkButtonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->UnlinkButton();
}

void CMainWindow::UnlinkButton()
{
	std::string cmd("M17U");
	AudioManager.Link(cmd);
}

void CMainWindow::DashboardButtonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->DashboardButton();
}

void CMainWindow::DashboardButton()
{
	auto host = routeMap.Find(pDestCallsignInput->value());
	if (host && ! host->url.empty()) {
		std::string opencmd("xdg-open ");
		opencmd.append(host->url);
		system(opencmd.c_str());
	}
}

void CMainWindow::FixDestActionButton()
{
	const std::string cs(pDestCallsignInput->value());
	const std::string ip(pDestIPInput->value());
	if (bDestCS) {	// is the destination c/s valid?
		auto host = routeMap.Find(cs);	// look for it
		if (host) {
			// cs is found in map
			if (bDestIP && host->url.empty()) { // is the IP okay and is this not from the csv file?
				if (ip.compare(host->ip4addr) && ip.compare(host->ip6addr)) {
					// the ip in the IPEntry is different
					SetDestActionButton(true, "Update");
				} else {
					// perfect match
					SetDestActionButton(true, "Delete");
					auto index = pDestinationChoice->find_index(cs.c_str());
					if (index >= 0)
						pDestinationChoice->value(index);
				}
			} else {
				SetDestActionButton(false, "");
			}
		} else {
			// cs is not found in map
			if (bDestIP) { // is the IP okay and is the not from the csv file?
				SetDestActionButton(true, "Save");
			} else {
				SetDestActionButton(false, "");
			}
		}
	} else {
		SetDestActionButton(false, "");
	}
	if (bTransOK && bDestCS && bDestIP)
	{
		pPTTButton->activate();
		pQuickKeyButton->activate();
	}
	else
	{
		pPTTButton->deactivate();
		pQuickKeyButton->deactivate();
	}
}

void CMainWindow::StopM17()
{
	keep_running = false;
	if (futReadThread.valid())
		futReadThread.get();
	if (gateM17.keep_running) {
		gateM17.keep_running = false;
		futM17.get();
	}
}

char CMainWindow::GetDestinationModule()
{
	for (unsigned i=0; i<26; i++) {
		if (pModuleRadioButton[i]->value())
			return 'A' + i;
	}
	return '!';	// ERROR!
}

// callback function writes data to a std::ostream
static size_t data_write(void* buf, size_t size, size_t nmemb, void* userp)
{
	if(userp)
	{
		std::ostream& os = *static_cast<std::ostream*>(userp);
		std::streamsize len = size * nmemb;
		if(os.write(static_cast<char*>(buf), len))
			return len;
	}

	return 0;
}

/**
 * timeout is in seconds
 **/
static CURLcode curl_read(const std::string& url, std::ostream& os, long timeout = 30)
{
	CURLcode code(CURLE_FAILED_INIT);
	CURL* curl = curl_easy_init();

	if(curl)
	{
		if(CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &data_write))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_FILE, &os))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout))
		&& CURLE_OK == (code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str())))
		{
			code = curl_easy_perform(curl);
		}
		curl_easy_cleanup(curl);
	}
	return code;
}

static void ReadM17Json()
{
	curl_global_init(CURL_GLOBAL_ALL);

	std::string path(CFG_DIR);
	path.append("m17refl.json");
	std::ofstream ofs(path);
	if (ofs.is_open()) {
		const std::string url("https://reflectors.m17.link/ref-list/json");
		if(CURLE_OK == curl_read(url, ofs)) {
			std::cout << url << " copied to " << path << std::endl;
		} else {
			std::cerr << "Could not read " << url << std::endl;
		}
	} else {
		std::cerr << "Could not open " << path << " for writing" << std::endl;
	}

	curl_global_cleanup();
}

int main (int argc, char **argv)
{
	ReadM17Json();

	CMainWindow MainWindow;
	if (MainWindow.Init())
		return 1;

	//Fl::lock();
	MainWindow.Run(argc, argv);
	Fl::run();
	return 0;
}
