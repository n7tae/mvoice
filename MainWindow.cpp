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
	bDestCS(false),
	bDestIP(false),
	bDestPort(false),
	bTransOK(true)
{
	cfg.CopyTo(cfgdata);
	// allowed M17 " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."
	IPv4RegEx = std::regex("^((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])\\.){3,3}(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9]){1,1}$", std::regex::extended);
	IPv6RegEx = std::regex("^(([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}(:[0-9a-fA-F]{1,4}){1,1}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|([0-9a-fA-F]{1,4}:){1,1}(:[0-9a-fA-F]{1,4}){1,6}|:((:[0-9a-fA-F]{1,4}){1,7}|:))$", std::regex::extended);
	M17RefRegEx = std::regex("^(M17-|URF)([A-Z0-9]){3,3}$", std::regex::extended);
	M17CallRegEx = std::regex("^[0-9]?[A-Z]{1,2}[0-9]{1,2}[A-Z]{1,4}([ -/\\.].*)?$", std::regex::extended);
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

void CMainWindow::BuildDestMenuButton()
{
	auto index = pMenuBar->find_index(_("Destination"));
	if (index >= 0)
	{
		pMenuBar->clear_submenu(index);
		for (const auto &cs : routeMap.GetKeys()) {
			const auto host = routeMap.Find(cs);
			if (host) {
				std::string name(_("Destination"));
				name.append("/");
				name.append(cs);
				switch (cfgdata.eNetType) {
					case EInternetType::ipv6only:
						if (! host->ip6addr.empty())
							pMenuBar->add(name.c_str(), 0, &CMainWindow::DestMenuButtonCB, this);
						break;
					case EInternetType::ipv4only:
						if (! host->ip4addr.empty())
							pMenuBar->add(name.c_str(), 0, &CMainWindow::DestMenuButtonCB, this);
						break;
					default:
						pMenuBar->add(name.c_str(), 0, &CMainWindow::DestMenuButtonCB, this);
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

	BuildDestMenuButton();
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
	pMenuBar->add(_("Destination"), 0, 0, 0, FL_SUBMENU);
	pMenuBar->add(_("Settings..."), 0, &CMainWindow::ShowSettingsDialogCB, this, 0);
	pMenuBar->add(_("About..."),    0, &CMainWindow::ShowAboutDialogCB,    this, 0);


	pTextBuffer = new Fl_Text_Buffer();
	pTextDisplay = new Fl_Text_Display(16, 30, 872, 314);
	pTextDisplay->buffer(pTextBuffer);
	pTextDisplay->end();

	pWin->resizable(pTextDisplay);

	pDestCallsignInput = new Fl_Input(195, 360, 130, 30, _("Destination Callsign:"));
	pDestCallsignInput->tooltip(_("A reflector or user callsign"));
	pDestCallsignInput->color(FL_RED);
	pDestCallsignInput->labelsize(16);
	pDestCallsignInput->textsize(16);
	pDestCallsignInput->when(FL_WHEN_CHANGED);
	pDestCallsignInput->callback(&CMainWindow::DestCallsignInputCB, this);

	pDestIPInput = new Fl_Input(360, 360, 380, 30, _("IP:"));
	pDestIPInput->tooltip(_("The IP of the reflector or user"));
	pDestIPInput->color(FL_RED);
	pDestIPInput->labelsize(16);
	pDestIPInput->textsize(16);
	pDestIPInput->when(FL_WHEN_CHANGED);
	pDestIPInput->callback(&CMainWindow::DestIPInputCB, this);

	pDestPortInput = new Fl_Int_Input(790, 360, 80, 30, _("Port:"));
	pDestPortInput->tooltip(_("The comm port of the reflector or user"));
	pDestPortInput->color(FL_RED);
	pDestPortInput->labelsize(16);
	pDestPortInput->textsize(16);
	pDestPortInput->when(FL_WHEN_CHANGED);
	pDestPortInput->callback(&CMainWindow::DestPortInputCB, this);

	pModuleGroup = new Fl_Group(170, 400, 560, 60, _("Module:"));
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
			pModuleRadioButton[i] = new Fl_Radio_Round_Button(x*42+177, y*30+402, 40, 25, modlabel[i]);
			pModuleRadioButton[i]->labelsize(16);
		}
	}
	pModuleGroup->end();
	pModuleRadioButton[0]->setonly();

	pActionButton = new Fl_Button(50, 475, 100, 30, _("Action"));
	pActionButton->tooltip(_("Update or delete an existing contact, or save a new contact"));
	pActionButton->labelsize(16);
	pActionButton->deactivate();
	pActionButton->callback(&CMainWindow::ActionButtonCB, this);

	pDashboardButton = new Fl_Button(690, 475, 160, 30, _("Open Dashboard"));
	pDashboardButton->tooltip(_("Open a reflector dashboard, if available"));
	pDashboardButton->labelsize(16);
	pDashboardButton->deactivate();
	pDashboardButton->callback(&CMainWindow::DashboardButtonCB, this);

	pConnectButton = new Fl_Button(338, 475, 100, 30, _("Connect"));
	pConnectButton->tooltip(_("Connect to an M17 Reflector"));
	pConnectButton->labelsize(16);
	pConnectButton->deactivate();
	pConnectButton->callback(&CMainWindow::LinkButtonCB, this);

	pDisconnectButton = new Fl_Button(455, 475, 100, 30, _("Disconnect"));
	pDisconnectButton->tooltip(_("Disconnect from an M17 reflector"));
	pDisconnectButton->labelsize(16);
	pDisconnectButton->deactivate();
	pDisconnectButton->callback(&CMainWindow::UnlinkButtonCB, this);

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
	node.run(17171, dht::crypto::generateIdentity(idstr), true, 59973);

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
		std::cout << "ERROR: not connected to any DHT network!" << std::endl;
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

void CMainWindow::DestMenuButtonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->DestMenuButton();
}

void CMainWindow::DestMenuButton()
{
	auto item = pMenuBar->mvalue();
	if (item)
	{
		auto cs = item->label();
		pDestCallsignInput->value(cs);
		DestCallsignInput();
		auto host = routeMap.Find(cs);
		if (host) {
			// first the IP
			if (EInternetType::ipv4only!=cfgdata.eNetType && !host->ip6addr.empty())
				// if we're not in IPv4-only mode && there is an IPv6 address for this host
				pDestIPInput->value(host->ip6addr.c_str());
			else
				pDestIPInput->value(host->ip4addr.c_str());
			DestIPInput();
			// then the port
			pDestPortInput->value(std::to_string(host->port).c_str());
			DestPortInput();
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
	if (0 == label.compare(savestr)) {
		const std::string a(pDestIPInput->value());
		auto p = uint16_t(std::atoi(pDestPortInput->value()));
		if (std::string::npos == a.find(':'))
			routeMap.Update(false, cs, a, "", "", "", p);
		else
			routeMap.Update(false, cs, "", a, "", "", p);
		BuildDestMenuButton();
	} else if (0 == label.compare(deletestr)) {
		routeMap.Erase(cs);
		BuildDestMenuButton();
	} else if (0 == label.compare(updatestr)) {
		std::string a(pDestIPInput->value());
		const auto p = uint16_t(std::atoi(pDestPortInput->value()));
		if (std::string::npos == a.find(':'))
			routeMap.Update(false, cs, a, "", "", "", p);
		else
			routeMap.Update(false, cs, "", a, "", "", p);
	}
	FixDestActionButton();
	routeMap.Save();
}

void CMainWindow::AudioSummary(const char *title)
{
		char line[640];
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

void CMainWindow::SetDestinationAddress(std::string &cs)
{
	cs.assign(pDestCallsignInput->value());
	const std::string ip(pDestIPInput->value());
	uint16_t port = std::stoul(pDestPortInput->value());
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
	auto onchar = pPTTButton->value();
	if (onchar) {
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
		AudioSummary(pttstr);
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
		std::string dest(pDestCallsignInput->value());
		switch (gateM17.GetLinkState())
		{
			case ELinkState::unlinked:
				pDisconnectButton->deactivate();
				if (std::regex_match(dest, M17RefRegEx) && bDestIP && bDestPort)
					pConnectButton->activate();
				else
					pConnectButton->deactivate();
				break;
			case ELinkState::linking:
				pDisconnectButton->deactivate();
				pConnectButton->deactivate();
				break;
			case ELinkState::linked:
				pDisconnectButton->activate();
				pConnectButton->deactivate();
				break;
		}
		pPTTButton->UpdateLabel();
		pEchoTestButton->UpdateLabel();
		TransmitterButtonControl();
		if (bTransOK)
		{
			auto host = routeMap.Find(dest);
			if (host)
			{
				if (host->updated)
				{
					if (EInternetType::ipv4only!=cfgdata.eNetType && ! host->ip6addr.empty())
						pDestIPInput->value(host->ip6addr.c_str());
					else
						pDestIPInput->value(host->ip4addr.c_str());
					DestIPInput();
					pDestPortInput->value(std::to_string(host->port).c_str());
					DestPortInput();

					if (host->modules.size())
						ActivateModules(host->modules);
					else
						ActivateModules();

					host->updated = false;
				}
			}
		}
	}
	Fl::unlock();
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
					routeMap.Update(false, rdat.callsign, rdat.ipv4addr, rdat.ipv6addr, rdat.url, rdat.modules, rdat.port);
				}
			}
			else if (0 == v->user_type.compare(URFD_CONFIG_1))
			{
				auto rdat = dht::Value::unpack<SUrfdConfig1>(*v);
				if (rdat.timestamp > ts)
				{
					ts = rdat.timestamp;
					routeMap.Update(false, rdat.callsign, rdat.ipv4addr, rdat.ipv6addr, rdat.url, rdat.modules, rdat.port[toUType(EUrfdPorts::m17)]);
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

void CMainWindow::DestCallsignInputCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->DestCallsignInput();
}

void CMainWindow::DestCallsignInput()
{
	// Convert to uppercase
	auto pos = pDestCallsignInput->position();
	std::string dest(pDestCallsignInput->value());
	if (ToUpper(dest))
	{
		pDestCallsignInput->value(dest.c_str());
		pDestCallsignInput->position(pos);
	}

	// the destination either has to be a reflector or a legal callsign
	bDestCS = std::regex_match(dest, M17CallRegEx) || std::regex_match(dest, M17RefRegEx);

	if (bDestCS)
	{
		auto host = routeMap.Find(dest); // is it already in the routeMap?
		if (host)
		{
#ifndef NO_DHT
			Get(host->cs);
#endif
			// let's try to come up with a destination IP
			if (EInternetType::ipv4only!=cfgdata.eNetType && !host->ip6addr.empty())
				// if we aren't in IPv4-only mode and there is an IPv6 address, use it
				pDestIPInput->value(host->ip6addr.c_str());
			else if (!host->ip4addr.empty())
				// otherwise, if there is an IPv4 address, use it
				pDestIPInput->value(host->ip4addr.c_str());

			pDestPortInput->value(std::to_string(host->port).c_str());

			// activate the configure modules
			// if there aren't any confgured modules, activate all modules
			if (host->modules.size())
				ActivateModules(host->modules);
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
		// bDestCS is false
		pDestIPInput->value("");
		pDestPortInput->value("");
	}

	DestIPInput();
	pDestCallsignInput->color(bDestCS ? 2 : 1);
	pDestCallsignInput->damage(FL_DAMAGE_ALL);

	DestPortInput();
	pDestPortInput->color(bDestPort ? 2 : 1);
	pDestPortInput->damage(FL_DAMAGE_ALL);
}

void CMainWindow::DestIPInputCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->DestIPInput();
}

void CMainWindow::DestIPInput()
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

void CMainWindow::DestPortInputCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->DestPortInput();
}

void CMainWindow::DestPortInput()
{
	auto port = std::atoi(pDestPortInput->value());
	bDestPort = (1023 < port && port < 49000);
	pDestPortInput->color(bDestPort ? 2 : 1);
	FixDestActionButton();
	pDestPortInput->damage(FL_DAMAGE_ALL);
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
		SetDestinationAddress(cs);
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
}

void CMainWindow::DashboardButtonCB(Fl_Widget *, void *This)
{
	((CMainWindow *)This)->DashboardButton();
}

void CMainWindow::DashboardButton()
{
	auto dciv = pDestCallsignInput->value();
	auto host = routeMap.Find(dciv);
	if (host && ! host->url.empty()) {
		fl_open_uri(host->url.c_str());
	}
}

void CMainWindow::FixDestActionButton()
{
	if (bDestCS) {	// is the destination c/s valid?
		const std::string cs(pDestCallsignInput->value());
		auto host = routeMap.Find(cs);	// look for it
		if (host) {
			// cs is found in map
			if (bDestIP && bDestPort && host->modules.empty()) { // is the IP and port okay and is this not from the csv file?
				const std::string ip(pDestIPInput->value());
				const std::string port(pDestPortInput->value());
				if ((ip.compare(host->ip4addr) && ip.compare(host->ip6addr)) || (port.compare(std::to_string(host->port)))) {
					// the ip in the IPEntry is different, or the port is different
					SetDestActionButton(true, updatestr);
				} else {
					// perfect match
					if (host->from_json)
						SetDestActionButton(false, "");
					else
						SetDestActionButton(true, deletestr);
				}
			} else {
				SetDestActionButton(false, "");
			}
		} else {
			// cs is not found in map
			if (bDestIP && bDestPort) { // is the IP okay and is the not from the csv file?
				SetDestActionButton(true, savestr);
			} else {
				SetDestActionButton(false, "");
			}
		}
	} else {
		SetDestActionButton(false, "");
	}
	TransmitterButtonControl();
}

void CMainWindow::TransmitterButtonControl()
{
	if (bTransOK && bDestCS && bDestIP && bDestPort && (0 == pConnectButton->active()))
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

	std::string path(CFGDIR);
	path.append("/m17refl.json");
	std::ofstream ofs(path);
	if (ofs.is_open()) {
		const std::string url("https://dvref.com/mrefd/json/?format=json");
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

	ReadM17Json();

	CMainWindow MainWindow;
	if (MainWindow.Init())
		return 1;

	Fl::lock();	// "start" the FLTK lock mechanism

	MainWindow.Run(argc, argv);
	Fl::run();
	return 0;
}
