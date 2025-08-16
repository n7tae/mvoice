/*
 *   Copyright (c) 2022 by Thomas A. Early N7TAE
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

#include "AboutDlg.h"

#include <libintl.h>
#define _(STRING) gettext(STRING)

#define VERSION "1.3.9"

CAboutDlg::CAboutDlg() {}

CAboutDlg::~CAboutDlg() {}

bool CAboutDlg::Init(Fl_RGB_Image *pIcon)
{
	pDlg = new Fl_Double_Window(400, 200, _("About MVoice"));

	pIconBox = new Fl_Box(176, 30, 48, 48);
	pIconBox->image(pIcon);

	snprintf(version, sizeof(version), _("MVoice version # %s"), VERSION);

	pVersionBox = new Fl_Box(0, 100, 400, 30, version);

	pCopyrightBox = new Fl_Box(0, 150, 400, 30, _("Copyright (c) 2025 by Thomas A. Early N7TAE"));

	pDlg->end();
	pDlg->callback(&CAboutDlg::WindowCallbackCB, this);
	pDlg->set_modal();

	return false;
}

void CAboutDlg::Show()
{
	pDlg->show();
}

void CAboutDlg::WindowCallbackCB(Fl_Widget *, void *ptr)
{
	((CAboutDlg *)ptr)->WindowCallback();
}

void CAboutDlg::WindowCallback()
{
	pDlg->hide();
}
