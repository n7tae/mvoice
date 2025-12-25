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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <chrono>
#include <cmath>

#include <FL/filename.H>

#include "MainWindow.h"
#include "Utilities.h"
#include "IconData.h"
#include "TemplateClasses.h"
#ifndef NO_DHT
#include "dht-values.h"
#endif

#define _(STRING) gettext(STRING)
//#define BOOTFILENAME "DHTNodes.bin"

static const char *pttstr    = _("PTT");
static const char *savestr   = _("Save");
static const char *deletestr = _("Delete");
static const char *updatestr = _("Delete");

static CM17RouteMap routeMap;

static void MyIdleProcess(void *p)
{
	CMainWindow *pMainWindow = (CMainWindow *)p;
	pMainWindow->UpdateGUI();

	Fl::repeat_timeout(1.0, MyIdleProcess, pMainWindow);
}

CMainWindow::CMainWindow() :
#ifndef NO_DHT
	exportNodeFilename("/exNodes.bin"),
#endif
	pWin(nullptr),
	bTargetCS(false),
	bTargetIP(false),
	bTargetPort(false),
	bDestCS(false),
	bTransOK(true)
{
	cfg.CopyTo(cfgdata);
	// allowed M17 " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."
	IPv4RegEx = std::regex("^((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])\\.){3,3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9]){1,1}$", std::regex::extended);
	IPv6RegEx = std::regex("^(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}(:[0-9a-fA-F]{1,4}){1,1}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|([0-9a-fA-F]{1,4}:){1,1}(:[0-9a-fA-F]{1,4}){1,6}|:((:[0-9a-fA-F]{1,4}){1,7}|:))$", std::regex::extended);
	ReflTarRegEx = std::regex("^(M17-|URF)[A-Z0-9]{3,3}$", std::regex::extended);
	ReflDstRegEx = std::regex("^(M17-[A-Z0-9]{3,3} [A-Z])|(URF[A-Z0-9]{3,3}  [A-Z])$", std::regex::extended);
	M17CallRegEx = std::regex("^[0-9]?[A-Z]{1,2}[0-9]{1,2}[A-Z]{1,4}([-/\\.][A-Z0-9]{1,2})? *[A-Z]?$", std::regex::extended);
}

CMainWindow::~CMainWindow()
{
#ifndef NO_DHT
	// save the dht network state
	auto exnodes = node.exportNodes();
	if (exnodes.size() > 1)
	{
		// Export nodes to binary file
		std::string path(CFGDIR);
		path.append(exportNodeFilename);
		std::ofstream myfile(path, std::ios::binary | std::ios::trunc);
		if (myfile.is_open())
		{
			std::cout << "Saving " << exnodes.size() << " nodes to " << path << std::endl;
			msgpack::pack(myfile, exnodes);
			myfile.close();
		}
		else
			std::cerr << "Trouble opening " << path << std::endl;
	}

	node.join();
#endif

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

void CMainWindow::BuildTargetMenuButton()
{
	const char *base = _("Target");
	auto index = pMenuBar->find_index(base);
	if (index >= 0)
	{
		pMenuBar->clear_submenu(index);
		for (const auto &cs : routeMap.GetKeys()) {
			const auto host = routeMap.Find(cs);
			if (host) {
				std::stringstream name;
				name << base << '/';
				if (0 == cs.compare(0, 4, "M17-"))
				{
					auto c1 = routeMap.CountKeysThatBeginsWith(cs.substr(0, 4));
					if (c1 > 1u)
					{
						name << "M17-/";
						auto c2 = routeMap.CountKeysThatBeginsWith(cs.substr(0, 5));
						if (c2 > 1u)
						{
							if (c2 < c1)
								name << cs.substr(0, 5) << "/";
							auto c3 = routeMap.CountKeysThatBeginsWith(cs.substr(0, 6));
							if (c3 > 1)
								name << cs.substr(0, 6) << "/" << cs;
							else
								name << cs;
						}
						else // c1 > 1u and 1u == c2
							name << cs;
					}
					else if (1u == c1)
					{
						name << cs;
					}
				}
				else if (0 == cs.compare(0, 3, "URF"))
				{
					auto c1 = routeMap.CountKeysThatBeginsWith(cs.substr(0, 3));
					if (c1 > 1u)
					{
						name << "URF/";
						auto c2 = routeMap.CountKeysThatBeginsWith(cs.substr(0, 4));
						if (c2 > 1u)
						{
							if (c2 < c1)
								name << cs.substr(0, 4) << "/";
							auto c3 = routeMap.CountKeysThatBeginsWith(cs.substr(0, 5));
							if (c3 > 1)
								name << cs.substr(0, 5) << "/" << cs;
							else
								name << cs;
						}
						else // c1 > 1u and 1u == c2
							name << cs;
					}
					else if (1u == c1)
					{
						name << cs;
					}
				}
				else
					name << '/' << cs;
				switch (cfgdata.eNetType) {
					case EInternetType::ipv6only:
						if (! host->ip6addr.empty())
							pMenuBar->add(name.str().c_str(), 0, &CMainWindow::TargetMenuButtonCB, this);
						break;
					case EInternetType::ipv4only:
						if (! host->ip4addr.empty())
							pMenuBar->add(name.str().c_str(), 0, &CMainWindow::TargetMenuButtonCB, this);
						break;
					default:
						pMenuBar->add(name.str().c_str(), 0, &CMainWindow::TargetMenuButtonCB, this);
						break;
				}
			}
		}
	}
}

void CMainWindow::SetState()
{
	if (cfg.IsOkay() && false == gateM17.keep_running)
		futM17 = std::async(std::launch::async, &CMainWindow::RunM17, this);

	BuildTargetMenuButton();
}

void CMainWindow::CloseAll()
{
	M172AM.Close();
	LogInput.Close();
}

bool CMainWindow::Init()
{
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

	keep_running = true;
	futReadThread = std::async(std::launch::async, &CMainWindow::ReadThread, this);

	pIcon = new Fl_RGB_Image(icon_image.pixel_data, icon_image.width, icon_image.height, icon_image.bytes_per_pixel);
	pWin = new Fl_Double_Window(900, 600, "MVoice");
	pWin->icon(pIcon);
	pWin->box(FL_BORDER_BOX);
	pWin->size_range(760, 440);
	pWin->callback(&CMainWindow::QuitCB, this);
	//Fl::visual(FL_DOUBLE|FL_INDEX);

	pMenuBar = new Fl_Menu_Bar(0, 0, 900, 30);
	pMenuBar->labelsize(16);
	pMenuBar->add(_("Target"), 0, 0, 0, FL_SUBMENU);
	pMenuBar->add(_("Texting..."), 0, &CMainWindow::ShowSMSDialogCB, this, 0);
	pMenuBar->add(_("Settings..."), 0, &CMainWindow::ShowSettingsDialogCB, this, 0);
	pMenuBar->add(_("About..."),    0, &CMainWindow::ShowAboutDialogCB,    this, 0);


	pTextBuffer = new Fl_Text_Buffer();
	pTextDisplay = new Fl_Text_Display(16, 30, 872, 314);
	pTextDisplay->buffer(pTextBuffer);
	pTextDisplay->end();

	pWin->resizable(pTextDisplay);

	pTargetCSInput = new Fl_Input(140, 360, 130, 30, _("Target Callsign:"));
	pTargetCSInput->tooltip(_("A reflector or user callsign"));
	pTargetCSInput->color(FL_RED);
	pTargetCSInput->labelsize(16);
	pTargetCSInput->textsize(16);
	pTargetCSInput->when(FL_WHEN_CHANGED);
	pTargetCSInput->callback(&CMainWindow::TargetCSInputCB, this);

	pIsLegacyCheck = new Fl_Check_Button(280, 360, 100, 30, _("Is Legacy"));
	pIsLegacyCheck->tooltip(_("Is the M17 reflector version 0.x.y?"));
	pIsLegacyCheck->labelsize(16);
	pIsLegacyCheck->when(FL_WHEN_CHANGED);

	pTargetIpInput = new Fl_Input(420, 360, 350, 30, _("IP:"));
	pTargetIpInput->tooltip(_("The IP of the reflector or user"));
	pTargetIpInput->color(FL_RED);
	pTargetIpInput->labelsize(16);
	pTargetIpInput->textsize(16);
	pTargetIpInput->when(FL_WHEN_CHANGED);
	pTargetIpInput->callback(&CMainWindow::TargetIPInputCB, this);

	pTargetPortInput = new Fl_Int_Input(820, 360, 60, 30, _("Port:"));
	pTargetPortInput->tooltip(_("The comm port of the reflector or user"));
	pTargetPortInput->color(FL_RED);
	pTargetPortInput->labelsize(16);
	pTargetPortInput->textsize(16);
	pTargetPortInput->when(FL_WHEN_CHANGED);
	pTargetPortInput->callback(&CMainWindow::TargetPortInputCB, this);

	pModuleGroup = new Fl_Group(140, 400, 560, 64, _("Module:"));
	pModuleGroup->tooltip(_("Select a module for the reflector or repeater"));
	pModuleGroup->labelsize(16);
	pModuleGroup->align(FL_ALIGN_LEFT);
	pModuleGroup->box(FL_THIN_UP_BOX);
	pModuleGroup->begin();
	static const char *modlabel[26] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z" };
	for (int y=0; y<2; y++)
	{
		for (int x=0; x<13; x++)
		{
			const int i = 13 * y + x;
			pModuleRadioButton[i] = new Fl_Radio_Round_Button(x*42+147, y*31+403, 40, 25, modlabel[i]);
			pModuleRadioButton[i]->labelsize(16);
		}
	}
	pModuleGroup->end();
	pModuleRadioButton[0]->setonly();

	pConnectButton = new Fl_Button(740, 400, 100, 30, _("Connect"));
	pConnectButton->tooltip(_("Connect to an M17 Reflector"));
	pConnectButton->labelsize(16);
	pConnectButton->deactivate();
	pConnectButton->callback(&CMainWindow::LinkButtonCB, this);

	pDisconnectButton = new Fl_Button(740, 434, 100, 30, _("Disconnect"));
	pDisconnectButton->tooltip(_("Disconnect from an M17 reflector"));
	pDisconnectButton->labelsize(16);
	pDisconnectButton->deactivate();
	pDisconnectButton->callback(&CMainWindow::UnlinkButtonCB, this);

	pActionButton = new Fl_Button(50, 478, 100, 30, _("Action"));
	pActionButton->tooltip(_("Update or delete an existing contact, or save a new contact"));
	pActionButton->labelsize(16);
	pActionButton->deactivate();
	pActionButton->callback(&CMainWindow::ActionButtonCB, this);

	pDSTCallsignInput = new Fl_Input(400, 478, 130, 30, _("Destination Callsign:"));
	pDSTCallsignInput->tooltip(_("A destination callsign"));
	pDSTCallsignInput->color(FL_RED);
	pDSTCallsignInput->labelsize(16);
	pDSTCallsignInput->textsize(16);
	pDSTCallsignInput->when(FL_WHEN_CHANGED);
	pDSTCallsignInput->callback(&CMainWindow::DestinationCSInputCB, this);
	pDSTCallsignInput->value("@ALL");

	pDashboardButton = new Fl_Button(690, 478, 160, 30, _("Open Dashboard"));
	pDashboardButton->tooltip(_("Open a reflector dashboard, if available"));
	pDashboardButton->labelsize(16);
	pDashboardButton->deactivate();
	pDashboardButton->callback(&CMainWindow::DashboardButtonCB, this);

	pEchoTestButton = new CTransmitButton(50, 540, 144, 40, _("Echo Test"));
	pEchoTestButton->tooltip(_("Push to record a test that will be played back"));
	pEchoTestButton->labelsize(16);
	pEchoTestButton->selection_color(FL_YELLOW);
	pEchoTestButton->callback(&CMainWindow::EchoButtonCB, this);

	pPTTButton = new CTransmitButton(250, 520, 400, 60, pttstr);
	pPTTButton->tooltip(_("Push to talk. This is actually a toggle button"));
	pPTTButton->labelsize(22);
	pPTTButton->deactivate();
	pPTTButton->selection_color(FL_YELLOW);
	pPTTButton->callback(&CMainWindow::PTTButtonCB, this);

	pQuickKeyButton = new Fl_Button(700, 540, 150, 40, _("Quick Key"));
	pQuickKeyButton->tooltip(_("Send a short, silent voice stream"));
	pQuickKeyButton->labelsize(16);
	pQuickKeyButton->deactivate();
	pQuickKeyButton->callback(QuickKeyButttonCB, this);

	pWin->end();

	if (SMSDlg.Init(this))
	{
		CloseAll();
		return true;
	}

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

	// idle processing
	Fl::add_timeout(1.0, MyIdleProcess, this);

#ifndef NO_DHT
	// start the dht instance
	std::string idstr(cfgdata.sM17SourceCallsign);
	if (idstr.empty()) {
		idstr.assign("MyNode");
		idstr.append(std::to_string(getpid()));
		std::cout << "Using " << idstr << " for identity" << std::endl;
	}
	try {
		node.run(17171, dht::crypto::generateIdentity(idstr), true, 59973);
	} catch (const std::exception &e) {
		std::cout << "MVoice could not start the Ham-network! " << e.what() << std::endl;
		return true;
	}

	// bootstrap the DHT from either saved nodes from a previous run,
	// or from the configured node
	std::string path(CFGDIR);
	path.append(exportNodeFilename);
	// Try to import nodes from binary file
	std::ifstream myfile(path, std::ios::binary|std::ios::ate);
	if (myfile.is_open())
	{
		msgpack::unpacker pac;
		auto size = myfile.tellg();
		myfile.seekg (0, std::ios::beg);
		pac.reserve_buffer(size);
		myfile.read (pac.buffer(), size);
		pac.buffer_consumed(size);
		// Import nodes
		msgpack::object_handle oh;
		while (pac.next(oh)) {
			auto imported_nodes = oh.get().as<std::vector<dht::NodeExport>>();
			std::cout << "Importing " << imported_nodes.size() << " ham-dht nodes from " << path << std::endl;
			node.bootstrap(imported_nodes);
		}
		myfile.close();
	}
	else if (cfgdata.sBootstrap.length())
	{
		std::cout << "Bootstrapping from " << cfgdata.sBootstrap << std::endl;
		node.bootstrap(cfgdata.sBootstrap, "17171");
	}
	else
	{
		std::cout << "ERROR: MVoice did not bootstrap the Ham-DHT network!" << std::endl;
	}
#endif

	return false;
}

void CMainWindow::ActivateModules(const std::string &modules)
{
	for (unsigned i=0; i<26; i++)
	if (std::string::npos == modules.find('A'+i))
		pModuleRadioButton[i]->deactivate();
	else
		pModuleRadioButton[i]->activate();
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
	SMSDlg.Hide();
	AudioManager.KeyOff();
	StopM17();

	if (pWin)
		pWin->hide();
}

void CMainWindow::ShowSMSDialogCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->ShowSMSDialog();
}

void CMainWindow::ShowSMSDialog()
{
	if (pIsLegacyCheck->value())
	{
		std::lock_guard<std::mutex> lock(logmux);
		insertLogText("Sorry! You can't send a message to a legacy reflector.\n");
	}
	else
		SMSDlg.Show();
	
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

void CMainWindow::TargetMenuButtonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->TargetMenuButton();
}

void CMainWindow::TargetMenuButton()
{
	auto item = pMenuBar->mvalue();
	if (item)
	{
		auto cs = item->label();
		pTargetCSInput->value(cs);
		TargetCSInput();
		auto host = routeMap.Find(cs);
		if (host) {
			// first the IP
			if (EInternetType::ipv4only!=cfgdata.eNetType && !host->ip6addr.empty())
				// if we're not in IPv4-only mode && there is an IPv6 address for this host
				pTargetIpInput->value(host->ip6addr.c_str());
			else
				pTargetIpInput->value(host->ip4addr.c_str());
			TargetIPInput();
			// then the port
			pTargetPortInput->value(std::to_string(host->port).c_str());
			TargetPortInput();
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
	auto cs = pTargetCSInput->value();
	bool islegacy = pIsLegacyCheck->value();
	if (0 == label.compare(savestr)) {
		const std::string a(pTargetIpInput->value());
		auto p = uint16_t(std::atoi(pTargetPortInput->value()));
		if (std::string::npos == a.find(':'))
			routeMap.Update(EFrom::user, cs, islegacy, "", a, "", "", "", p, "");
		else
			routeMap.Update(EFrom::user, cs, islegacy, "", "", a, "", "", p, "");
		BuildTargetMenuButton();
	} else if (0 == label.compare(deletestr)) {
		routeMap.Erase(cs);
		BuildTargetMenuButton();
	} else if (0 == label.compare(updatestr)) {
		std::string a(pTargetIpInput->value());
		const auto p = uint16_t(std::atoi(pTargetPortInput->value()));
		if (std::string::npos == a.find(':'))
			routeMap.Update(EFrom::user, cs, islegacy, "", a, "", "", "", p, "");
		else
			routeMap.Update(EFrom::user, cs, islegacy, "", "", a, "", "", p, "");
	}
	FixTargetMenuButton();
	routeMap.Save();
}

void CMainWindow::AudioSummary(const char *title)
{
		char line[64];
		double t = AudioManager.volStats.count * 0.000125;	// 0.000125 = 1 / 8000
		// we only do the sums of squares on every other point, so 0.5 mult in denominator
		// 65 db subtration for "reasonable volume", an arbitrary reference point
		double d = 20.0 * log10(sqrt(AudioManager.volStats.ss/(0.5 * AudioManager.volStats.count))) - 65.0;
		double c = 100.0 * AudioManager.volStats.clip / AudioManager.volStats.count;
		snprintf(line, 64, _("%s Time=%.1fs Vol=%.0fdB Clip=%.0f%%\n"), title, t, d, c);
		std::lock_guard<std::mutex> lck(logmux);
		insertLogText(line);
}

void CMainWindow::EchoButtonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->EchoButton();
}

void CMainWindow::EchoButton()
{
	pEchoTestButton->toggle();
	auto onchar = pEchoTestButton->value();
	if (onchar) {
		bTransOK = false;
		// record the mic to a queue
		AudioManager.RecordMicThread(E_PTT_Type::echo, "ECHOTEST");
	} else {
		AudioSummary(_("Echo"));
		// play back the queue
		AudioManager.PlayEchoDataThread();
		bTransOK = true;
	}
}

void CMainWindow::Receive(bool is_rx)
{
	bTransOK = ! is_rx;
	TransmitterButtonControl();

	if (bTransOK)
		pEchoTestButton->activate();
	else
		pEchoTestButton->deactivate();

	if (bTransOK && AudioManager.volStats.count)
		AudioSummary(_("RX Audio"));
}

void CMainWindow::SetTargetAddress(std::string &cs)
{
	cs.assign(pTargetCSInput->value());
	const std::string ip(pTargetIpInput->value());
	uint16_t port = std::stoul(pTargetPortInput->value());
	gateM17.SetDestAddress(ip, port);
	if (0==cs.compare(0, 4, "M17-") || 0==cs.compare(0, 3, "URF")) {
		cs.resize(8, ' ');
		cs.append(1, GetTargetModule());
	}
	if (pIsLegacyCheck->value())
	{
		pDSTCallsignInput->value(cs.c_str());
	}
}

void CMainWindow::PTTButtonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->PTTButton();
}

void CMainWindow::PTTButton()
{
	pPTTButton->toggle();
	auto onchar = pPTTButton->value();
	if (onchar) {
		if (gateM17.TryLock())
		{
			const std::string cs(pDSTCallsignInput->value());
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
		AudioSummary(pttstr);
		gateM17.ReleaseLock();
	}
}

void CMainWindow::QuickKeyButton()
{
	std::string cs(pDSTCallsignInput->value());
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
				CPacket pack;
				M172AM.Read(pack.GetData(), MAX_PACKET_SIZE);
				if (0 == memcmp(pack.GetCData(), "M17 ", 4))
				{
					pack.Initialize(54u, true);
					AudioManager.M17_2AudioMgr(pack);
				}
			}
			if (FD_ISSET(logfd, &fdset))
			{
				char line[256] = { 0 };
				LogInput.Read(line, 256);
				std::lock_guard<std::mutex> lok(logmux);
				insertLogText(line);
			}
		}
	}
}

void CMainWindow::insertLogText(const char *line)
{
	Fl::lock();
	pTextBuffer->append(line);
	pTextDisplay->insert_position(pTextBuffer->length());
	pTextDisplay->show_insert_position();
	Fl::unlock();
}

void CMainWindow::UpdateGUI()
{
	Fl::lock();
	if (cfgdata.sM17SourceCallsign.empty())
	{
		pPTTButton->deactivate();
		pQuickKeyButton->deactivate();
		pConnectButton->deactivate();
	}
	else
	{
		bool isLegacy = pIsLegacyCheck->value() ? true : false;
		std::string target(pTargetCSInput->value());
		const auto linkState = gateM17.GetLinkState();
		switch (linkState)
		{
			case ELinkState::unlinked:
				pDisconnectButton->deactivate();
				if (std::regex_match(target, ReflTarRegEx) && bTargetIP && bTargetPort)
					pConnectButton->activate();
				else
					pConnectButton->deactivate();
				pTargetCSInput->activate();
				pIsLegacyCheck->activate();
				pTargetIpInput->activate();
				pTargetPortInput->activate();
				pDSTCallsignInput->activate();
				break;
			case ELinkState::linking:
				pDisconnectButton->deactivate();
				pConnectButton->deactivate();
				if (isLegacy) pDSTCallsignInput->deactivate();
				break;
			case ELinkState::linked:
				pDisconnectButton->activate();
				pConnectButton->deactivate();
				pTargetCSInput->deactivate();
				pIsLegacyCheck->deactivate();
				pTargetIpInput->deactivate();
				pTargetPortInput->deactivate();
				if (isLegacy)
				{
					pDSTCallsignInput->deactivate();
					SMSDlg.Hide();
				}
				break;
		}
		pPTTButton->UpdateLabel();
		pEchoTestButton->UpdateLabel();
		TransmitterButtonControl();
		if (bTransOK)
		{
			auto host = routeMap.Find(target);
			if (host)
			{
				if (host->updated)
				{
					if (EInternetType::ipv4only!=cfgdata.eNetType && ! host->ip6addr.empty())
						pTargetIpInput->value(host->ip6addr.c_str());
					else
						pTargetIpInput->value(host->ip4addr.c_str());
					TargetIPInput();
					pIsLegacyCheck->value(host->is_legacy ? 1 : 0);
					pTargetPortInput->value(std::to_string(host->port).c_str());
					TargetPortInput();

					host->updated = false;
				}

				if (ELinkState::unlinked != linkState)
				{
					ActivateModules("#"); // this will turn off all modules
				}
				else
				{
					if (host->mods.size())
						ActivateModules(host->mods);
					else
						ActivateModules();
				}
			}
		}
	}
	Fl::unlock();
}

bool CMainWindow::SendMessage(const std::string &dst, const std::string &msg)
{
	auto l = gateM17.TryLock();
	if (l)
	{
		CPacket pack;
		pack.Initialize(38u+msg.length(), false);
		CCallsign cs(dst);
		cs.CodeOut(pack.GetDstAddress());
		cs.CSIn(cfgdata.sM17SourceCallsign);
		cs.CodeOut(pack.GetSrcAddress());
		pack.SetFrameType(0);
		pack.GetData()[34] = 0x5u;
		auto len = msg.length();
		if (len > (MAX_PACKET_SIZE - 38u))
		{
			insertLogText("Message is too long, it will be truncated.\n");
			len = MAX_PACKET_SIZE -38u;
		}
		memcpy(pack.GetData()+35, msg.c_str(), len);
		pack.CalcCRC();
		gateM17.SendMessage(pack);
		gateM17.ReleaseLock();
		std::stringstream ss;
		ss << "Sent an SMS text msg to " << dst << ":\n" << msg << "\n";
		insertLogText(ss.str().c_str());
	}
	else
	{
		insertLogText("Could not set the message because the gateway was locked!\n");
	}
	return l;
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

#ifndef NO_DHT
void CMainWindow::Get(const std::string &cs)
{
	static std::time_t ts;
	ts = 0;
	dht::Where w;
	if (0 == cs.compare(0, 4, "M17-"))
		w.id(toUType(EMrefdValueID::Config));
	else if (0 == cs.compare(0, 3, "URF"))
		w.id(toUType(EUrfdValueID::Config));
	else
	{
		// std::cerr << "Unknown callsign '" << cs << "' for node.get()" << std::endl;
		return;
	}
	node.get(
		dht::InfoHash::get(cs),
		[](const std::shared_ptr<dht::Value> &v) {
			if (0 == v->user_type.compare(MREFD_CONFIG_1))
			{
				auto rdat = dht::Value::unpack<SMrefdConfig1>(*v);
				if (rdat.timestamp > ts)
				{
					ts = rdat.timestamp;
					routeMap.Update(EFrom::dht, rdat.callsign, '0'==rdat.version[0], "", rdat.ipv4addr, rdat.ipv6addr, rdat.modules, rdat.encryptedmods, rdat.port, rdat.url);
				}
			}
			else if (0 == v->user_type.compare(URFD_CONFIG_1))
			{
				auto rdat = dht::Value::unpack<SUrfdConfig1>(*v);
				if (rdat.timestamp > ts)
				{
					ts = rdat.timestamp;
					routeMap.Update(EFrom::dht, rdat.callsign, true, "", rdat.ipv4addr, rdat.ipv6addr, rdat.modules, rdat.transcodedmods, rdat.port[toUType(EUrfdPorts::m17)], rdat.url);
				}
			}
			else
			{
				std::cerr << "Found the data, but it has an unknown user_type: " << v->user_type << std::endl;
			}
			return true;
		},
		[](bool success) {
			if (! success)
				std::cout << "node.get() was unsuccessful!" << std::endl;
		},
		{}, // empty filter
		w
	);
}
#endif

void CMainWindow::DestinationCSInputCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->DestinationCSInput();
}

void CMainWindow::DestinationCSInput()
{
	// Convert to uppercase
	auto pos = pDSTCallsignInput->position();
	std::string dest(pDSTCallsignInput->value());
	if (ToUpper(dest))
	{
		pDSTCallsignInput->value(dest.c_str());
		pDSTCallsignInput->position(pos);
	}

	// the destination either has to be @ALL, PARROT or a legal callsign
	bDestCS = 0==dest.compare("@ALL") or std::regex_match(dest, ReflDstRegEx) or 0==dest.compare("#PARROT") or std::regex_match(dest, M17CallRegEx);
	pDSTCallsignInput->color(bDestCS ? 2 : 1);
	pDSTCallsignInput->damage(FL_DAMAGE_ALL);
}

void CMainWindow::TargetCSInputCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->TargetCSInput();
}

void CMainWindow::TargetCSInput()
{
	// Convert to uppercase
	auto pos = pTargetCSInput->position();
	std::string dest(pTargetCSInput->value());
	if (ToUpper(dest))
	{
		pTargetCSInput->value(dest.c_str());
		pTargetCSInput->position(pos);
	}

	// the target either has to be a reflector or a legal callsign
	bTargetCS = std::regex_match(dest, M17CallRegEx) || std::regex_match(dest, ReflTarRegEx);

	if (bTargetCS)
	{
		auto host = routeMap.Find(dest); // is it already in the routeMap?
		if (host)
		{
#ifndef NO_DHT
			Get(host->cs);
#endif
			// let's try to come up with a destination IP
			if (EInternetType::ipv4only!=cfgdata.eNetType and not host->ip6addr.empty())
				// if we aren't in IPv4-only mode and there is an IPv6 address, use it
				pTargetIpInput->value(host->ip6addr.c_str());
			else if (EInternetType::ipv6only!=cfgdata.eNetType and not host->ip4addr.empty())
				// otherwise, if there is an IPv4 address, use it
				pTargetIpInput->value(host->ip4addr.c_str());
			else if (not host->dn.empty())	// is there a domain name specified
			{
				// then we'll try to resolve the domain name to a preferred IP address
				struct addrinfo *res, hints;
			
				memset(&hints, 0, sizeof hints);
				switch (cfgdata.eNetType)
				{
					case EInternetType::ipv4only:
						hints.ai_family = AF_INET;
						break;
					case EInternetType::ipv6only:
						hints.ai_family = AF_INET6;
						break;
					default:
						hints.ai_family = AF_UNSPEC;
						break;
				}
				hints.ai_socktype = SOCK_DGRAM;

				int status = getaddrinfo(host->dn.c_str(), std::to_string(host->port).c_str(), &hints, &res);
				if (status) {
					insertLogText(gai_strerror(status));
					insertLogText("\n");
				} else {
					if (res) {
						void *addr = nullptr;
						// get the pointer to the address itself,
						// different fields in IPv4 and IPv6:
						if (res->ai_family == AF_INET) {
							struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
							addr = &(ipv4->sin_addr);
						} else if (res->ai_family == AF_INET6) {
							struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res->ai_addr;
							addr = &(ipv6->sin6_addr);
						}

						if (addr) {
							char ipstr[INET6_ADDRSTRLEN];
							// convert the IP to a string and print it:
							inet_ntop(res->ai_family, addr, ipstr, sizeof ipstr);
							pTargetIpInput->value(ipstr);
							std::string msg("Resolved domain name ");
							msg.append(host->dn + " to " + ipstr + "\n");
							insertLogText(msg.c_str());
						}
					}
					freeaddrinfo(res); // free the linked list
				}
			
			}

			pTargetPortInput->value(std::to_string(host->port).c_str());

			// activate the configure modules
			// if there aren't any confgured modules, activate all modules
			if (host->mods.size())
				ActivateModules(host->mods);
			else
				ActivateModules();

			if (host && !host->url.empty())
				pDashboardButton->activate();
			else
				pDashboardButton->deactivate();
		}
		else
		{
			ActivateModules();
#ifndef NO_DHT
			Get(dest);
#endif
			SetState();
		}
	}
	else
	{
		// bTargetCS is false
		pTargetIpInput->value("");
		pTargetPortInput->value("");
		pIsLegacyCheck->value(0);
	}

	TargetIPInput();
	pTargetCSInput->color(bTargetCS ? 2 : 1);
	pTargetCSInput->damage(FL_DAMAGE_ALL);

	TargetPortInput();
	pTargetPortInput->color(bTargetPort ? 2 : 1);
	pTargetPortInput->damage(FL_DAMAGE_ALL);
}

void CMainWindow::TargetIPInputCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->TargetIPInput();
}

void CMainWindow::TargetIPInput()
{
	auto bIP4 = std::regex_match(pTargetIpInput->value(), IPv4RegEx);
	auto bIP6 = std::regex_match(pTargetIpInput->value(), IPv6RegEx);
	switch (cfgdata.eNetType) {
		case EInternetType::ipv4only:
			bTargetIP = bIP4;
			break;
		case EInternetType::ipv6only:
			bTargetIP = bIP6;
			break;
		default:
			bTargetIP = (bIP4 || bIP6);
	}
	pTargetIpInput->color(bTargetIP ? 2 : 1);
	FixTargetMenuButton();
	pTargetIpInput->damage(FL_DAMAGE_ALL);
}

void CMainWindow::TargetPortInputCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->TargetPortInput();
}

void CMainWindow::TargetPortInput()
{
	auto port = std::atoi(pTargetPortInput->value());
	bTargetPort = (1023 < port && port < 49000);
	pTargetPortInput->color(bTargetPort ? 2 : 1);
	FixTargetMenuButton();
	pTargetPortInput->damage(FL_DAMAGE_ALL);
}

void CMainWindow::SetTargetMenuButton(const char *label)
{
	if (*label)
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
	if (cfgdata.sM17SourceCallsign.empty()) {
		std::lock_guard<std::mutex> lck(logmux);
		insertLogText(_("ERROR: Your system is not yet configured!\n"));
		insertLogText(_("Be sure to save your callsign and internet access in Settings\n"));
		insertLogText(_("You can usually leave the audio devices as 'default'\n"));
	}
	else
	{
		std::string cmd("M17L");
		std::string cs;
		SetTargetAddress(cs);
		cmd.append(cs);
		AudioManager.Link(cmd);
	}
}

void CMainWindow::UnlinkButtonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->UnlinkButton();
}

void CMainWindow::UnlinkButton()
{
	std::string cmd("M17U");
	AudioManager.Link(cmd);
	Fl::lock();
	pDSTCallsignInput->value("@ALL");
	Fl::unlock();
}

void CMainWindow::DashboardButtonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->DashboardButton();
}

void CMainWindow::DashboardButton()
{
	auto dciv = pTargetCSInput->value();
	auto host = routeMap.Find(dciv);
	if (host && ! host->url.empty()) {
		fl_open_uri(host->url.c_str());
	}
}

void CMainWindow::FixTargetMenuButton()
{
	if (bTargetCS) {	// is the destination c/s valid?
		const std::string cs(pTargetCSInput->value());
		auto host = routeMap.Find(cs);	// look for it
		if (host) {
			// cs is found in map
			if (bTargetIP && bTargetPort && host->mods.empty()) { // is the IP and port okay and is this not from the csv file?
				const std::string ip(pTargetIpInput->value());
				const std::string port(pTargetPortInput->value());
				if ((ip.compare(host->ip4addr) and ip.compare(host->ip6addr)) or (port.compare(std::to_string(host->port))) and ((pIsLegacyCheck->value()?true:false)!=host->is_legacy)) {
					// the ip in the IPEntry is different, or the port is different
					SetTargetMenuButton(updatestr);
				} else {
					// perfect match
					if (EFrom::user != host->from)
						SetTargetMenuButton();
					else
						SetTargetMenuButton(deletestr);
				}
			} else {
				SetTargetMenuButton();
			}
		} else {
			// cs is not found in map
			if (bTargetIP && bTargetPort) { // is the IP okay and is the not from the csv file?
				SetTargetMenuButton(savestr);
			} else {
				SetTargetMenuButton();
			}
		}
	} else {
		SetTargetMenuButton();
	}
	TransmitterButtonControl();
}

void CMainWindow::TransmitterButtonControl()
{
	DestinationCSInput();
	if (bTransOK && bDestCS && bTargetCS && bTargetIP && bTargetPort && (0 == pConnectButton->active()))
	{
		pPTTButton->activate();
		pQuickKeyButton->activate();
		SMSDlg.UpdateSMS(true);
	}
	else
	{
		pPTTButton->deactivate();
		pQuickKeyButton->deactivate();
		SMSDlg.UpdateSMS(false);
	}
}

void CMainWindow::StopM17()
{
	if (gateM17.keep_running) {
		gateM17.keep_running = false;
		futM17.get();
	}
}

char CMainWindow::GetTargetModule()
{
	for (unsigned i=0; i<26; i++) {
		if (pModuleRadioButton[i]->value())
			return 'A' + i;
	}
	return '!';	// ERROR!
}

#define MKDIR(PATH) ::mkdir(PATH, 0755)

static bool do_mkdir(const std::string& path)
{
    struct stat st;
    if (::stat(path.c_str(), &st) != 0)
	{
        if (MKDIR(path.c_str()) != 0 && errno != EEXIST)
		{
            return false;
        }
    } else if (!S_ISDIR(st.st_mode))
	{
        errno = ENOTDIR;
        return false;
    }
    return true;
}

void mkpath(std::string path)
{
    std::string build;
    for (size_t pos = 0; (pos = path.find('/')) != std::string::npos; )
	{
        build += path.substr(0, pos + 1);
        do_mkdir(build);
        path.erase(0, pos + 1);
    }
    if (!path.empty())
	{
        build += path;
        do_mkdir(build);
    }
}

int main (int argc, char **argv)
{
	// internationalization
	setlocale(LC_ALL, "");
	std::string localedir(BASEDIR);
	localedir.append("/share/locale");
	bindtextdomain("mvoice", localedir.c_str());
	textdomain("mvoice");

	// make the user's config directory
	auto home = getenv("HOME");
	if (home)
	{
		if (chdir(home))
		{
			std::cerr << "ERROR: Can't cd to '" << home << "': " << strerror(errno) << std::endl;
			return EXIT_FAILURE;
		}
		mkpath(CFGDIR);
	}
	else
	{
		std::cerr << "ERROR: HOME enviromental variable not found" << std::endl;
		return EXIT_FAILURE;
	}

	CMainWindow MainWindow;
	if (MainWindow.Init())
		return 1;

	Fl::lock();	// "start" the FLTK lock mechanism

	MainWindow.Run(argc, argv);
	Fl::run();
	return 0;
}
