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

#include "TransmitButton.h"

CTransmitButton::CTransmitButton(int X, int Y, int W, int H, const char *L) : Fl_Toggle_Button(X, Y, W, H, L), defaultlabel(L)
{
}

void CTransmitButton::toggle()
{
	if (Fl_Button::value())
	{
		timer.start();
	}
	UpdateLabel();
}

void CTransmitButton::UpdateLabel()
{
	if (Fl_Button::value())
	{
		auto t = int(timer.time());
		auto s = t % 60;
		auto m = t / 60;
		snprintf(tlabel, 16, "%d:%02d", m, s);
		Fl_Toggle_Button::label(tlabel);
	}
	else
	{
		Fl_Toggle_Button::label(defaultlabel);
	}
}
