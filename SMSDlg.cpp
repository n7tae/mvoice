/*
 *   Copyright (c) 2025 by Thomas A. Early N7TAE
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

#include <locale>
#include <string>

#include "MainWindow.h"
#include "SMSDlg.h"
#include "Utilities.h"

#include <libintl.h>
#define _(STRING) gettext(STRING)

CSMSDlg::CSMSDlg() : pMainWindow(nullptr), pDlg(nullptr) {}

CSMSDlg::~CSMSDlg()
{
	if (pDlg)
		delete pDlg;
}

bool CSMSDlg::Init(CMainWindow *pMain)
{
	pMainWindow = pMain;
	pDlg = new Fl_Double_Window(600, 240, _("SMS Texting"));

	pDSTCallsignInput = new Fl_Input(170, 10, 130, 30, _("Destination Callsign:"));
	pDSTCallsignInput->tooltip(_("Packet Mode destination callsign"));
	pDSTCallsignInput->color(FL_RED);
	pDSTCallsignInput->labelsize(16);
	pDSTCallsignInput->textsize(16);
	pDSTCallsignInput->when(FL_WHEN_CHANGED);
	pDSTCallsignInput->callback(&CSMSDlg::DestinationCSInputCB, this);
	pDSTCallsignInput->value("@ALL");

	pSendButton = new Fl_Button(320, 10, 130, 30, _("Send"));
	pSendButton->tooltip(_("Send this message"));
	pSendButton->labelsize(18);
	pSendButton->deactivate();
	pSendButton->callback(&CSMSDlg::SendButtonCB, this);

	pClearButton = new Fl_Button(460, 10, 130, 32, _("Clear"));
	pClearButton->tooltip(_("Clear the outgoing message"));
	pClearButton->labelsize(18);
	pClearButton->deactivate();
	pClearButton->callback(&CSMSDlg::ClearButtonCB, this);

	pMsgBuffer = new Fl_Text_Buffer();
	pMessage = new Fl_Text_Editor(10, 70, 580, 160, _("Outgoing Message"));
	pMessage->tooltip(_("Message to send"));
	pMessage->labelsize(16);
	pMessage->textsize(16);
	pMessage->buffer(pMsgBuffer);

	pDlg->end();
	pDlg->callback(&CSMSDlg::WindowCallbackCB, this);
	return false;
}

void CSMSDlg::Show()
{
	pDlg->show();
}

void CSMSDlg::WindowCallbackCB(Fl_Widget *, void *dlg)
{
	((CSMSDlg *)dlg)->Hide();
}

void CSMSDlg::Hide()
{
	pDlg->hide();
}

void CSMSDlg::DestinationCSInputCB(Fl_Widget *, void *dlg)
{
	((CSMSDlg *)dlg)->DestinationCSInput();
}

void CSMSDlg::DestinationCSInput()
{
	// Convert to uppercase
	auto pos = pDSTCallsignInput->position();
	std::string dest(pDSTCallsignInput->value());
	if (pMainWindow->ToUpper(dest))
	{
		pDSTCallsignInput->value(dest.c_str());
		pDSTCallsignInput->position(pos);
	}

	// the destination either has to be @ALL or a legal callsign
	bDestCS = 0==dest.compare("@ALL") or 0==dest.compare("#PARROT") or std::regex_match(dest, pMainWindow->M17CallRegEx);
	pDSTCallsignInput->color(bDestCS ? 2 : 1);
	pDSTCallsignInput->damage(FL_DAMAGE_ALL);
}

void CSMSDlg::SendButtonCB(Fl_Widget *, void *dlg)
{
	((CSMSDlg *)dlg)->SendButton();
}

void CSMSDlg::SendButton()
{
	auto txt = pMsgBuffer->text();
	const std::string dst(pDSTCallsignInput->value());
	std::string msg(txt);
	trim(msg);
	free(txt);
	if (pMainWindow->SendMessage(dst, msg))
		ClearButton();
}

void CSMSDlg::ClearButtonCB(Fl_Widget *, void *dlg)
{
	((CSMSDlg *)dlg)->ClearButton();
}

void CSMSDlg::ClearButton()
{
	pMsgBuffer->text("");
}

void CSMSDlg::UpdateSMS(bool cansend)
{
	DestinationCSInput();
	if (pMsgBuffer->length() > 0)
	{
		pClearButton->activate();
		if (bDestCS and cansend)
			pSendButton->activate();
		else
			pSendButton->deactivate();
	}
	else
	{
		pClearButton->deactivate();
		pSendButton->deactivate();
	}
}
