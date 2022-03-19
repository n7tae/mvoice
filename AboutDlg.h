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

 #pragma once

 #include "FLTK-GUI.h"

 class CAboutDlg
 {
public:
	CAboutDlg();
	~CAboutDlg();
	bool Init(Fl_PNG_Image *);
	void Show();

protected:
	Fl_Double_Window *pDlg;
	Fl_PNG_Image *pIcon;
	Fl_Box *pIconBox, *pVersionBox, *pCopyrightBox;
	static void WindowCallbackCB(Fl_Widget *, void *);
	void WindowCallback();
 };
