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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <chrono>

#include "MainWindow.h"
#include "WaitCursor.h"
#include "DPlusAuthenticator.h"
#include "Utilities.h"
#include "TemplateClasses.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp/"
#endif

static Glib::RefPtr<Gtk::Application> theApp;

CMainWindow::CMainWindow() :
	pWin(nullptr),
	pQuitButton(nullptr),
	pSettingsButton(nullptr),
	pGate(nullptr),
	pLink(nullptr),
	pM17Gate(nullptr),
	bDestCS(false),
	bDestIP(false),
	bTransOK(true)
{
	cfg.CopyTo(cfgdata);
	if (! AudioManager.AMBEDevice.IsOpen()) {
		AudioManager.AMBEDevice.FindandOpen(cfgdata.iBaudRate, Encoding::dstar);
	}
	// allowed M17 " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."
	CallRegEx = std::regex("^(([1-9][A-Z])|([A-PR-Z][0-9])|([A-PR-Z][A-Z][0-9]))[0-9A-Z]*[A-Z][ ]*[ A-RT-Z]$", std::regex::extended);
	IPv4RegEx = std::regex("^((25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])\\.){3,3}(25[0-5]|(2[0-4]|1{0,1}[0-9]){0,1}[0-9])$", std::regex::extended);
	IPv6RegEx = std::regex("([0-9a-fA-F]{1,4}:){7,7}[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,7}:|([0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|([0-9a-fA-F]{1,4}:){1,5}(:[0-9a-fA-F]{1,4}){1,2}|([0-9a-fA-F]{1,4}:){1,4}(:[0-9a-fA-F]{1,4}){1,3}|([0-9a-fA-F]{1,4}:){1,3}(:[0-9a-fA-F]{1,4}){1,4}|([0-9a-fA-F]{1,4}:){1,2}(:[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:((:[0-9a-fA-F]{1,4}){1,6})|:((:[0-9a-fA-F]{1,4}){1,7}|:)$", std::regex::extended);
	M17RefRegEx = std::regex("^M17([A-Z0-9-]){4,4}[ ][A-Z]$", std::regex::extended);
	//M17CallRegEx = std::regex("^[A-Z0-9/\\.-]([ A-Z0-9/\\.-]){0,8}$", std::regex::extended);
	M17CallRegEx = std::regex("^(([1-9][A-Z])|([A-PR-Z][0-9])|([A-PR-Z][A-Z][0-9]))[0-9A-Z]*[A-Z][- /\\.]*[ A-Z]$", std::regex::extended);
}

CMainWindow::~CMainWindow()
{
	if (pWin)
		delete pWin;
	StopLink();
	StopGate();
	StopM17();
}

void CMainWindow::RunLink()
{
	pLink = new CQnetLink;
	if (! pLink->Init(&cfgdata))
		pLink->Process();
	delete pLink;
	pLink = nullptr;
}

void CMainWindow::RunGate()
{
	pGate = new CQnetGateway;
	if (! pGate->Init(&cfgdata))
		pGate->Process();
	delete pGate;
	pGate = nullptr;
}

void CMainWindow::RunM17()
{
	pM17Gate = new CM17Gateway;
	std::cout << "Starting M17 Gateway..." << std::endl;
	if (! pM17Gate->Init(cfgdata, &routeMap))
		pM17Gate->Process();
	delete pM17Gate;
	std::cout << "M17 Gateway has stopped." << std::endl;
	pM17Gate = nullptr;
}

void CMainWindow::SetState(const CFGDATA &data)
{
	if (data.bCodec2Enable) {
		pMainStack->set_visible_child("page1");
		StopGate();
		StopLink();
		pM17DestCallsignEntry->set_text(data.sM17DestCallsign);
		pM17DestIPEntry->set_text(data.sM17DestIp);
		if (nullptr == pM17Gate && cfg.IsOkay())
			futM17 = std::async(std::launch::async, &CMainWindow::RunM17, this);
	} else {
		pMainStack->set_visible_child("page0");
		StopM17();
		if (data.bRouteEnable) {
			if (nullptr == pGate && cfg.IsOkay())
				futGate = std::async(std::launch::async, &CMainWindow::RunGate, this);
			pRouteComboBox->set_sensitive(true);
			pRouteActionButton->set_sensitive(true);
		} else {
			StopGate();
			pRouteEntry->set_text("CQCQCQ");
			pRouteEntry->set_sensitive(false);
			pRouteComboBox->set_sensitive(false);
			pRouteActionButton->set_sensitive(false);
		}

		if (data.bLinkEnable) { // if data.bLinkEnable==true, then the TimeoutProcess() will handle the link frame widgets
			if (nullptr == pGate && cfg.IsOkay())
				futLink = std::async(std::launch::async, &CMainWindow::RunLink, this);
		} else {
			StopLink();
			pLinkButton->set_sensitive(false);
			pLinkEntry->set_sensitive(false);
			pUnlinkButton->set_sensitive(false);
			pLinkEntry->set_text("");
		}
	}
}

void CMainWindow::CloseAll()
{
	Gate2AM.Close();
	Link2AM.Close();
	M172AM.Close();
	LogInput.Close();
}

bool CMainWindow::Init(const Glib::RefPtr<Gtk::Builder> builder, const Glib::ustring &name)
{
	std::string dbname(CFG_DIR);
	dbname.append("qn.db");
	if (qnDB.Open(dbname.c_str()))
		return true;
	qnDB.ClearLH();
	qnDB.ClearLS();
	RebuildGateways(cfgdata.bDPlusEnable);

	if (Gate2AM.Open("gate2am"))
		return true;

	if (Link2AM.Open("link2am")) {
		CloseAll();
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

 	builder->get_widget(name, pWin);
	if (nullptr == pWin) {
		CloseAll();
		std::cerr << "Failed to Initialize MainWindow!" << std::endl;
		return true;
	}

	if (cfgdata.bAPRSEnable)
		aprs.Init();

	//setup our css context and provider
	Glib::RefPtr<Gtk::CssProvider> css = Gtk::CssProvider::create();
	Glib::RefPtr<Gtk::StyleContext> style = Gtk::StyleContext::create();

	//load our red clicked style (applies to Gtk::ToggleButton)
	if (css->load_from_data("button:checked { background: red; }")) {
		style->add_provider_for_screen(pWin->get_screen(), css, GTK_STYLE_PROVIDER_PRIORITY_USER);
	}

	if (SettingsDlg.Init(builder, "SettingsDialog", pWin, this)) {
		CloseAll();
		return true;
	}

	if (AboutDlg.Init(builder, pWin)) {
		CloseAll();
		return true;
	}

	builder->get_widget("QuitButton", pQuitButton);
	builder->get_widget("SettingsButton", pSettingsButton);
	builder->get_widget("LinkButton", pLinkButton);
	builder->get_widget("UnlinkButton", pUnlinkButton);
	builder->get_widget("RouteActionButton", pRouteActionButton);
	builder->get_widget("RouteComboBox", pRouteComboBox);
	builder->get_widget("RouteEntry", pRouteEntry);
	builder->get_widget("LinkEntry", pLinkEntry);
	builder->get_widget("EchoTestButton", pEchoTestButton);
	builder->get_widget("PTTButton", pPTTButton);
	builder->get_widget("QuickKeyButton", pQuickKeyButton);
	builder->get_widget("ScrolledWindow", pScrolledWindow);
	builder->get_widget("LogTextView", pLogTextView);
	builder->get_widget("AboutMenuItem", pAboutMenuItem);
	builder->get_widget("MainStack", pMainStack);
	builder->get_widget("M17Stack", pM17Stack);
	builder->get_widget("DStarStack", pDStarStack);
	builder->get_widget("M17DestActionButton", pM17DestActionButton);
	builder->get_widget("M17DestCallsignEntry", pM17DestCallsignEntry);
	builder->get_widget("M17DestIPEntry", pM17DestIPEntry);
	builder->get_widget("M17DestCallsignComboBox", pM17DestCallsignComboBox);
	builder->get_widget("M17LinkButton", pM17LinkButton);
	builder->get_widget("M17UnlinkButton", pM17UnlinkButton);

	pLogTextBuffer = pLogTextView->get_buffer();

	// events
	pM17DestCallsignEntry->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_M17DestCallsignEntry_changed));
	pM17DestIPEntry->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_M17DestIPEntry_changed));
	pM17DestCallsignComboBox->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_M17DestCallsignComboBox_changed));
	pM17DestActionButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_M17DestActionButton_clicked));
	pM17LinkButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_M17LinkButton_clicked));
	pM17UnlinkButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_M17UnlinkButton_clicked));
	pSettingsButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_SettingsButton_clicked));
	pQuitButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_QuitButton_clicked));
	pRouteActionButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteActionButton_clicked));
	pRouteComboBox->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteComboBox_changed));
	pRouteEntry->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_RouteEntry_changed));
	pEchoTestButton->signal_toggled().connect(sigc::mem_fun(*this, &CMainWindow::on_EchoTestButton_toggled));
	pPTTButton->signal_toggled().connect(sigc::mem_fun(*this, &CMainWindow::on_PTTButton_toggled));
	pQuickKeyButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_QuickKeyButton_clicked));
	pLinkButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_LinkButton_clicked));
	pUnlinkButton->signal_clicked().connect(sigc::mem_fun(*this, &CMainWindow::on_UnlinkButton_clicked));
	pLinkEntry->signal_changed().connect(sigc::mem_fun(*this, &CMainWindow::on_LinkEntry_changed));
	pAboutMenuItem->signal_activate().connect(sigc::mem_fun(*this, &CMainWindow::on_AboutMenuItem_activate));

	ReadRoutes();
	routeMap.Open();
	for (const auto &item : routeMap.GetKeys()) {
		// std::cout << "Addding " << item << " to M17 ComboBox" << std::endl;
		pM17DestCallsignComboBox->append(item);
	}
	if (routeMap.Size())
		pM17DestCallsignComboBox->set_active(0);
	Receive(false);
	SetState(cfgdata);
	on_M17DestCallsignComboBox_changed();

	// i/o events
	Glib::signal_io().connect(sigc::mem_fun(*this, &CMainWindow::RelayGate2AM), Gate2AM.GetFD(), Glib::IO_IN);
	Glib::signal_io().connect(sigc::mem_fun(*this, &CMainWindow::RelayLink2AM), Link2AM.GetFD(), Glib::IO_IN);
	Glib::signal_io().connect(sigc::mem_fun(*this, &CMainWindow::RelayM172AM),  M172AM.GetFD(), Glib::IO_IN);
	Glib::signal_io().connect(sigc::mem_fun(*this, &CMainWindow::GetLogInput), LogInput.GetFD(), Glib::IO_IN);
	// idle processing
	Glib::signal_timeout().connect(sigc::mem_fun(*this, &CMainWindow::TimeoutProcess), 1000);

	return false;
}

void CMainWindow::Run()
{
	theApp->run(*pWin);
}

void CMainWindow::on_QuitButton_clicked()
{
	CWaitCursor wait;
	aprs.Close();
	AudioManager.KeyOff(! cfgdata.bCodec2Enable);
	StopGate();
	StopLink();
	StopM17();

	if (pWin)
		pWin->hide();
}

void CMainWindow::on_AboutMenuItem_activate()
{
	AboutDlg.Show();
}

void CMainWindow::on_SettingsButton_clicked()
{
	auto newdata = SettingsDlg.Show();
	if (newdata) {	// the user clicked okay so we need to see if anything changed. We'll shut things down and let SetState start things up again
		CWaitCursor wait;
		if (newdata->sStation.compare(cfgdata.sCallsign) || newdata->cModule!=cfgdata.cModule) {	// the station callsign has changed
			StopGate();
			StopLink();
		} else if (newdata->eNetType != cfgdata.eNetType) {
			StopM17();
			StopGate();
		}

		if (! newdata->bCodec2Enable)
			StopM17();

		if (! newdata->bLinkEnable)
			StopLink();

		if (! newdata->bRouteEnable)
			StopGate();

		SetState(*newdata);
		cfg.CopyTo(cfgdata);
	}
}

void CMainWindow::WriteRoutes()
{
	std::string path(CFG_DIR);
	path.append("routes.cfg");
	std::ofstream file(path.c_str(), std::ofstream::out | std::ofstream::trunc);
	if (! file.is_open())
		return;
	for (auto it=routeset.begin(); it!=routeset.end(); it++) {
		file << *it << std::endl;
	}
	file.close();
}

void CMainWindow::ReadRoutes()
{
	std::string path(CFG_DIR);

	path.append("routes.cfg");
	std::ifstream file(path.c_str(), std::ifstream::in);
	if (file.is_open()) {
		char line[128];
		while (file.getline(line, 128)) {
			if ('#' != *line) {
				routeset.insert(line);
			}
		}
		file.close();
		for (auto it=routeset.begin(); it!=routeset.end(); it++) {
			pRouteComboBox->append(*it);
		}
		pRouteComboBox->set_active(0);
		return;
	}

	routeset.insert("CQCQCQ");
	routeset.insert("DSTAR1");
	routeset.insert("DSTAR1 T");
	routeset.insert("DSTAR2");
	routeset.insert("DSTAR2 T");
	routeset.insert("QNET20 C");
	routeset.insert("QNET20 Z");
	for (auto it=routeset.begin(); it!=routeset.end(); it++)
		pRouteComboBox->append(*it);
	pRouteComboBox->set_active(0);
}

void CMainWindow::on_RouteEntry_changed()
{
	int pos = pRouteEntry->get_position();
	Glib::ustring s = pRouteEntry->get_text().uppercase();
	const Glib::ustring good("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ");
	Glib::ustring n;
	for (auto it=s.begin(); it!=s.end(); it++) {
		if (Glib::ustring::npos != good.find(*it)) {
			n.append(1, *it);
		}
	}
	pRouteEntry->set_text(n);
	pRouteEntry->set_position(pos);
	pRouteActionButton->set_sensitive(n.size() ? true : false);
	pRouteActionButton->set_label((routeset.end() == routeset.find(s)) ? "Save" : "Delete");
}

void CMainWindow::on_M17DestCallsignComboBox_changed()
{
	auto cs = pM17DestCallsignComboBox->get_active_text();
	pM17DestCallsignEntry->set_text(cs);
	auto address = routeMap.Find(cs.c_str());
	if (address)
		pM17DestIPEntry->set_text(address->GetAddress());
}

void CMainWindow::on_M17DestActionButton_clicked()
{
	auto label = pM17DestActionButton->get_label();
	auto cs = pM17DestCallsignEntry->get_text();
	if (0 == label.compare("Save")) {
		routeMap.Update(cs, pM17DestIPEntry->get_text());
		pM17DestCallsignComboBox->remove_all();
		for (const auto &member : routeMap.GetKeys())
			pM17DestCallsignComboBox->append(member);
		pM17DestCallsignComboBox->set_active_text(cs);
		SetDestActionButton(true, "Delete");
	} else if (0 == label.compare("Delete")) {
		int index = pM17DestCallsignComboBox->get_active_row_number();
		pM17DestCallsignComboBox->remove_text(index);
		routeMap.Erase(cs);
		if (index >= int(routeMap.Size()))
			index--;
		if (index < 0) {
			pM17DestCallsignComboBox->unset_active();
			pM17DestIPEntry->set_text("");
		} else
			pM17DestCallsignComboBox->set_active(index);
	} else if (0 == label.compare("Update")) {
		routeMap.Update(pM17DestIPEntry->get_text(), pM17DestIPEntry->get_text());
	}
	routeMap.Save();
}

void CMainWindow::on_RouteComboBox_changed()
{
	pRouteEntry->set_text(pRouteComboBox->get_active_text());
}

void CMainWindow::on_RouteActionButton_clicked()
{
	if (pRouteActionButton->get_label().compare("Save")) {
		// deleting an entry
		auto todelete = pRouteEntry->get_text();
		int index = pRouteComboBox->get_active_row_number();
		pRouteComboBox->remove_text(index);
		routeset.erase(todelete);
		if (index >= int(routeset.size()))
			index--;
		if (index >= 0)
			pRouteComboBox->set_active(index);
	} else {
		// adding an entry
		auto toadd = pRouteEntry->get_text();
		routeset.insert(toadd);
		pRouteComboBox->remove_all();
		for (const auto &item : routeset)
			pRouteComboBox->append(item);
		pRouteComboBox->set_active_text(toadd);
	}
	WriteRoutes();
}

void CMainWindow::on_EchoTestButton_toggled()
{
	if (pEchoTestButton->get_active()) {
		// record the mic to a queue
		AudioManager.RecordMicThread(E_PTT_Type::echo, "CQCQCQ  ");
		//std::cout << "AM.RecordMicThread() returned\n";
	} else {
		// play back the queue
		AudioManager.PlayEchoDataThread();
		//std::cout << "AM.PlayEchoDataThread() returned\n";
	}
}

void CMainWindow::Receive(bool is_rx)
{
	bool ppt_okay;
	bTransOK = ! is_rx;
	if (cfgdata.bCodec2Enable) {
		ppt_okay = bTransOK && bDestCS && bDestIP;
	} else {
		ppt_okay = bTransOK && (pRouteEntry->get_text().size() > 0);
	}
	pPTTButton->set_sensitive(ppt_okay);
	pEchoTestButton->set_sensitive(bTransOK);
	pQuickKeyButton->set_sensitive(ppt_okay);
}

void CMainWindow::on_PTTButton_toggled()
{
	if (cfgdata.bCodec2Enable) {
		const std::string dest(pM17DestCallsignEntry->get_text().c_str());
		if (pPTTButton->get_active()) {
			AudioManager.RecordMicThread(E_PTT_Type::m17, dest);
		} else
			AudioManager.KeyOff(! cfgdata.bCodec2Enable);
	} else {
		const std::string urcall(pRouteEntry->get_text().c_str());
		bool is_cqcqcq = (0 == urcall.compare(0, 6, "CQCQCQ"));

		if ((! is_cqcqcq && cfgdata.bRouteEnable) || (is_cqcqcq && cfgdata.bLinkEnable)) {
			if (pPTTButton->get_active()) {
				if (is_cqcqcq) {
					AudioManager.RecordMicThread(E_PTT_Type::link, "CQCQCQ");
				} else {
					AudioManager.RecordMicThread(E_PTT_Type::gateway, urcall);
				}
				if (cfgdata.bAPRSEnable)
					aprs.UpdateUser();
			} else
				AudioManager.KeyOff(! cfgdata.bCodec2Enable);
		}
	}
}

void CMainWindow::on_QuickKeyButton_clicked()
{
	if (cfgdata.bCodec2Enable)
		AudioManager.QuickKey(pM17DestCallsignEntry->get_text().c_str(), cfgdata.sM17SourceCallsign);
	else
		AudioManager.QuickKey(pRouteEntry->get_text().c_str());
}

bool CMainWindow::RelayLink2AM(Glib::IOCondition condition)
{
	if (condition & Glib::IO_IN) {
		SDSVT dsvt;
		Link2AM.Read(dsvt.title, 56);
		if (0 == memcmp(dsvt.title, "DSVT", 4))
			AudioManager.Link2AudioMgr(dsvt);
		else if (0 == memcmp(dsvt.title, "PLAY", 4))
			AudioManager.PlayFile((char *)&dsvt.config);
	} else {
		std::cerr << "RelayLink2AM not a read event!" << std::endl;
	}
	return true;
}

bool CMainWindow::RelayM172AM(Glib::IOCondition condition)
{
	if (condition & Glib::IO_IN) {
		SM17Frame m17;
		M172AM.Read(m17.magic, sizeof(SM17Frame));
		if (0 == memcmp(m17.magic, "M17 ", 4))
			AudioManager.M17_2AudioMgr(m17);
		else if (0 == memcmp(m17.magic, "PLAY", 4))
			AudioManager.PlayFile((char *)&m17.streamid);
	} else {
		std::cerr << "RelayM17_2AM not a read event!" << std::endl;
	}
	return true;
}

bool CMainWindow::RelayGate2AM(Glib::IOCondition condition)
{
	if (condition & Glib::IO_IN) {
		SDSVT dsvt;
		Gate2AM.Read(dsvt.title, 56);
		if (0 == memcmp(dsvt.title, "DSVT", 4))
			AudioManager.Gateway2AudioMgr(dsvt);
		else if (0 == memcmp(dsvt.title, "PLAY", 4))
			AudioManager.PlayFile((char *)&dsvt.config);
	} else {
		std::cerr << "RelayGate2AM not a read event!" << std::endl;
	}
	return true;
}

bool CMainWindow::GetLogInput(Glib::IOCondition condition)
{
	static auto it = pLogTextBuffer->begin();
	if (condition & Glib::IO_IN) {
		char line[256] = { 0 };
		LogInput.Read(line, 256);
		//const char *p;
		//if (g_utf8_validate(line, length, &p))
		//	std::cout << "bogus charater '" << int(*p) << "' at " << int(p-line) << std::endl;
		it = pLogTextBuffer->insert(it, line);
		pLogTextView->scroll_to(it, 0.0, 0.0, 1.0);
	} else {
		std::cerr << "GetLogInput is not a read event!" << std::endl;
	}
	return true;
}

bool CMainWindow::TimeoutProcess()
{
	if (!cfgdata.bCodec2Enable) { // don't do this test if we are in M17 mode
		// this is all about syncing the LinkFrame widgets to the actual link state of the module
		// so if pLink is not up, then we don't need to do anything
		if ((! cfgdata.bLinkEnable) || (nullptr == pLink))
			return true;
	}

	std::list<CLink> linkstatus;
	if (qnDB.FindLS(cfgdata.cModule, linkstatus))	// get the link status list of our module (there should only be one, or none if it's not linked)
		return true;

	std::string call;
	if (linkstatus.size()) {	// extract the linked module from the returned list, if the list is empty, it means our module is not linked!
		CLink ls(linkstatus.front());
		call.assign(ls.callsign);
	}

	if (call.empty()) {
		// DStar
		pLinkEntry->set_sensitive(true);
		pUnlinkButton->set_sensitive(false);
		std::string s(pLinkEntry->get_text().c_str());
		pLinkButton->set_sensitive((8==s.size() && isalpha(s.at(7)) && qnDB.FindGW(s.c_str())) ? true : false);
		// M17
		// pM17DestCallsignEntry->set_sensitive(true);
		// pM17DestIPEntry->set_sensitive(true);
		pM17UnlinkButton->set_sensitive(false);
		s.assign(pM17DestCallsignEntry->get_text().c_str());
		pM17LinkButton->set_sensitive(std::regex_match(s, M17RefRegEx));
	} else {
		// DStar
		pLinkEntry->set_sensitive(false);
		pLinkButton->set_sensitive(false);
		pUnlinkButton->set_sensitive(true);
		// M17
		// pM17DestCallsignEntry->set_sensitive(false);
		// pM17DestIPEntry->set_sensitive(false);
		pM17LinkButton->set_sensitive(false);
		pM17UnlinkButton->set_sensitive(true);
	}
	return true;
}

void CMainWindow::on_M17DestCallsignEntry_changed()
{
	int pos = pM17DestCallsignEntry->get_position();
	Glib::ustring s = pM17DestCallsignEntry->get_text().uppercase();
	pM17DestCallsignEntry->set_text(s);
	pM17DestCallsignEntry->set_position(pos);
	bDestCS = std::regex_match(s.c_str(), M17CallRegEx) || std::regex_match(s.c_str(), M17RefRegEx);
	const auto addr = routeMap.FindBase(s);
	if (addr)
		pM17DestIPEntry->set_text(addr->GetAddress());
	pM17DestCallsignEntry->set_icon_from_icon_name(bDestCS ? "gtk-ok" : "gtk-cancel");
	FixM17DestActionButton();
}

void CMainWindow::on_M17DestIPEntry_changed()
{
	auto bIP4 = std::regex_match(pM17DestIPEntry->get_text().c_str(), IPv4RegEx);
	auto bIP6 = std::regex_match(pM17DestIPEntry->get_text().c_str(), IPv6RegEx);
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
	pM17DestIPEntry->set_icon_from_icon_name(bDestIP ? "gtk-ok" : "gtk-cancel");
	FixM17DestActionButton();
}

void CMainWindow::SetDestActionButton(const bool sensitive, const char *label)
{
	pM17DestActionButton->set_sensitive(sensitive);
	pM17DestActionButton->set_label(label);
}

void CMainWindow::on_M17LinkButton_clicked()
{
	if (pM17Gate) {
		//std::cout << "Pushed the Link button for " << pM17DestCallsignEntry->get_text().c_str() << '.' << std::endl;
		std::string cmd("M17L");
		cmd.append(pM17DestCallsignEntry->get_text().c_str());
		AudioManager.Link(cmd);
	} else
		std::cout << "Pushed the link button, but the gateway is not running" << std::endl;
}

void CMainWindow::on_M17UnlinkButton_clicked()
{
	if (pM17Gate) {
		std::string cmd("M17U");
		AudioManager.Link(cmd);
	}
}

void CMainWindow::FixM17DestActionButton()
{
	const std::string cs(pM17DestCallsignEntry->get_text().c_str());
	const std::string ip(pM17DestIPEntry->get_text().c_str());
	if (bDestCS) {
		auto addr = routeMap.Find(cs);
		if (addr) {
			// cs is found in map
			if (bDestIP) { // is the IP okay?
				if (ip.compare(addr->GetAddress())) {
					// the ip in the IPEntry is different
					SetDestActionButton(true, "Update");
				} else {
					// perfect match
					SetDestActionButton(true, "Delete");
					pM17DestCallsignComboBox->set_active_text(cs);
				}
			} else {
				SetDestActionButton(false, "");
			}
		} else {
			// cs is not found in map
			if (bDestIP) { // is the IP okay?
				SetDestActionButton(true, "Save");
			} else {
				SetDestActionButton(false, "");
			}
		}
	} else {
		SetDestActionButton(false, "");
	}
	bool all = (bTransOK && bDestCS && bDestIP);
	pPTTButton->set_sensitive(all);
	pQuickKeyButton->set_sensitive(all);
}

void CMainWindow::on_LinkEntry_changed()
{
	int pos = pLinkEntry->get_position();
	Glib::ustring s = pLinkEntry->get_text().uppercase();
	const Glib::ustring good("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ");
	Glib::ustring n;
	for (auto it=s.begin(); it!=s.end(); it++) {
		if (Glib::ustring::npos != good.find(*it)) {
			n.append(1, *it);
		}
	}
	pLinkEntry->set_text(n);
	pLinkEntry->set_position(pos);
	if (8==n.size() && isalpha(n.at(7)) && qnDB.FindGW(n.c_str())) {
		pLinkEntry->set_icon_from_icon_name("gtk-ok");
		pLinkButton->set_sensitive(true);
	} else {
	 	pLinkEntry->set_icon_from_icon_name("gtk-cancel");
		pLinkButton->set_sensitive(false);
	}
}

void CMainWindow::on_LinkButton_clicked()
{
	if (pLink) {
		//std::cout << "Pushed the Link button for " << pLinkEntry->get_text().c_str() << '.' << std::endl;
		std::string cmd("LINK");
		cmd.append(pLinkEntry->get_text().c_str());
		AudioManager.Link(cmd);
	}
}

void CMainWindow::on_UnlinkButton_clicked()
{
	if (pLink) {
		std::string cmd("LINK");
		AudioManager.Link(cmd);
	}
}

void CMainWindow::RebuildGateways(bool includelegacy)
{
	CWaitCursor WaitCursor;
	qnDB.ClearGW();
	CHostQueue qhost;

	std::string filename(CFG_DIR);	// now open the gateways text file
	filename.append("gwys.txt");
	int count = 0;
	std::ifstream hostfile(filename);
	if (hostfile.is_open()) {
		std::string line;
		while (std::getline(hostfile, line)) {
			trim(line);
			if (! line.empty() && ('#' != line.at(0))) {
				std::istringstream iss(line);
				std::string name, addr;
				unsigned short port;
				iss >> name >> addr >> port;
				CHost host(name, addr, port);
				qhost.Push(host);
				count++;
			}
		}
		hostfile.close();
		qnDB.UpdateGW(qhost);
	}

	if (includelegacy && ! cfgdata.sStation.empty()) {
		const std::string website("auth.dstargateway.org");
		CDPlusAuthenticator auth(cfgdata.sStation, website);
		int dplus = auth.Process(qnDB, true, false);
		if (0 == dplus) {
			fprintf(stdout, "DPlus Authorization failed.\n");
			printf("# of Gateways: %s=%d\n", filename.c_str(), count);
		} else {
			fprintf(stderr, "DPlus Authorization completed!\n");
			printf("# of Gateways %s=%d %s=%d Total=%d\n", filename.c_str(), count, website.c_str(), dplus, qnDB.Count("GATEWAYS"));
		}
	} else {
		printf("#Gateways: %s=%d\n", filename.c_str(), count);
	}
}

int main (int argc, char **argv)
{
	theApp = Gtk::Application::create(argc, argv, "net.openquad.QnetDV");

	//Load the GtkBuilder file and instantiate its widgets:
	Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create();
	try
	{
		std::string path(CFG_DIR);
		builder->add_from_file(path + "DigitalVoice.glade");
	}
	catch (const Glib::FileError& ex)
	{
		std::cerr << "FileError: " << ex.what() << std::endl;
		return 1;
	}
	catch (const Glib::MarkupError& ex)
	{
		std::cerr << "MarkupError: " << ex.what() << std::endl;
		return 1;
	}
	catch (const Gtk::BuilderError& ex)
	{
		std::cerr << "BuilderError: " << ex.what() << std::endl;
		return 1;
	}

	CMainWindow MainWindow;
	if (MainWindow.Init(builder, "AppWindow"))
		return 1;

	MainWindow.Run();

	return 0;
}

void CMainWindow::StopLink()
{
	if (nullptr != pLink) {
		pLink->keep_running = false;
		futLink.get();
		pLink = nullptr;
	}
}

void CMainWindow::StopGate()
{
	if(nullptr != pGate) {
		pGate->keep_running = false;
		futGate.get();
		pGate = nullptr;
	}
}

void CMainWindow::StopM17()
{
	if (nullptr != pM17Gate) {
		pM17Gate->keep_running = false;
		futM17.get();
		pM17Gate = nullptr;
	}
}
