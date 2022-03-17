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

#pragma once

#include <map>

#include "FLTK-GUI.h"
#include "Configure.h"

class CMainWindow;

class CSettingsDlg
{
public:
    CSettingsDlg();
    ~CSettingsDlg();
    bool Init(Fl_Double_Window *pWin, CMainWindow *pMain);
    void Show();

private:
	std::map<std::string, std::pair<std::string, std::string>> AudioInMap, AudioOutMap;
	// persistance
	void SaveWidgetStates(CFGDATA &d);
	void SetWidgetStates(const CFGDATA &d);
	// data classes
	CFGDATA data;
	// other data
	bool bM17Source;
	// Windows
	CMainWindow *pMainWindow;
    Fl_Double_Window *pDlg;
	// widgets
	Fl_Tabs *pTabs;
	Fl_Button *pOkayButton, *pCancelButton, *pAudioRescanButton;
	Fl_Choice *pAudioInputChoice, *pAudioOutputChoice;
	Fl_Input *pSourceCallsignInput;
	Fl_Group *pStationGroup, *pAudioGroup, *pInternetGroup, *pGUIGroup, *pCodecGroup;
	Fl_Round_Button *pVoiceOnlyRadioButton, *pVoiceDataRadioButton;
	Fl_Round_Button *pIPv4RadioButton, *pIPv6RadioButton, *pDualStackRadioButton;
	Fl_Round_Button *pPTTRadioButton, *pTTTRadioButton;
	Fl_Box *pAudioInputDescBox, *pAudioOutputDescBox;
	// Callback wrapper
	static void SourceCallsignInputCB(Fl_Widget *p, void *v);
	static void AudioRescanButtonCB(Fl_Widget *p, void *v);
	static void AudioInputChoiceCB(Fl_Widget *p, void *v);
	static void AudioOutputChoiceCB(Fl_Widget *p, void *v);
	static void OkayButtonCB(Fl_Widget *p, void *v);
	static void CancelButtonCB(Fl_Widget *p, void *v);
	// the actual callbacks
	void SourceCallsignInput();
	void AudioRescanButton();
	void AudioInputChoice();
	void AudioOutputChoice();
	void OkayButton();
	void CancelButton();
};
