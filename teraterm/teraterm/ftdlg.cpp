/* Tera Term
 Copyright(C) 1994-1998 T. Teranishi
 All rights reserved. */

/* TERATERM.EXE, file transfer dialog box */
#include "stdafx.h"
#include "teraterm.h"
#include "tttypes.h"
#include "ttftypes.h"
#include "ttlib.h"
#include "tt_res.h"
#include "ftdlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CFileTransDlg dialog

BEGIN_MESSAGE_MAP(CFileTransDlg, CDialog)
	//{{AFX_MSG_MAP(CFileTransDlg)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CFileTransDlg::Create(PFileVar pfv, PComVar pcv, PTTSet pts)
{
	BOOL Ok;
	WNDCLASS wc;
	int fuLoad = LR_DEFAULTCOLOR;
	HWND hwnd;

	fv = pfv;
	cv = pcv;
	cv->FilePause &= ~fv->OpId;
	ts = pts;
	LOGFONT logfont;
	HFONT font;

	wc.style = CS_PARENTDC;
	wc.lpfnWndProc = AfxWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = DLGWINDOWEXTRA;
	wc.hInstance = AfxGetInstanceHandle();
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL,IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "FTDlg32";
	RegisterClass(&wc);

	Pause = FALSE;
	hwnd = GetForegroundWindow()->GetSafeHwnd();
	if (fv->OpId == OpLog) { // parent window is desktop
		Ok = CDialog::Create(CFileTransDlg::IDD, GetDesktopWindow());
	}
	else { // parent window is VT window
		Ok = CDialog::Create(CFileTransDlg::IDD, NULL);
	}

	if (!fv->HideDialog) {
		// Visible = False のダイアログを表示する
		ShowWindow(SW_SHOWNORMAL);
		if (fv->OpId == OpLog) {
			ShowWindow(SW_MINIMIZE);
		}
	}
	else {
		// 直前にフォアグラウンドだったウィンドウにフォーカスを戻す。
		// メニューからログをスタートした時に VTWin にフォーカスが戻らないので必要っぽい。
		::SetForegroundWindow(hwnd);
	}

	fv->HWin = GetSafeHwnd();

	font = (HFONT)SendMessage(WM_GETFONT, 0, 0);
	GetObject(font, sizeof(LOGFONT), &logfont);
	if (get_lang_font("DLG_SYSTEM_FONT", fv->HWin, &logfont, &DlgFont, ts->UILanguageFile)) {
		SendDlgItemMessage(IDC_TRANS_FILENAME, WM_SETFONT, (WPARAM)DlgFont, MAKELPARAM(TRUE,0));
		SendDlgItemMessage(IDC_TRANSFNAME, WM_SETFONT, (WPARAM)DlgFont, MAKELPARAM(TRUE,0));
		SendDlgItemMessage(IDC_FULLPATH_LABEL, WM_SETFONT, (WPARAM)DlgFont, MAKELPARAM(TRUE,0));
		SendDlgItemMessage(IDC_EDIT_FULLPATH, WM_SETFONT, (WPARAM)DlgFont, MAKELPARAM(TRUE,0));
		SendDlgItemMessage(IDC_TRANS_TRANS, WM_SETFONT, (WPARAM)DlgFont, MAKELPARAM(TRUE,0));
		SendDlgItemMessage(IDC_TRANSBYTES, WM_SETFONT, (WPARAM)DlgFont, MAKELPARAM(TRUE,0));
		SendDlgItemMessage(IDC_TRANS_ELAPSED, WM_SETFONT, (WPARAM)DlgFont, MAKELPARAM(TRUE,0));
		SendDlgItemMessage(IDC_TRANS_ETIME, WM_SETFONT, (WPARAM)DlgFont, MAKELPARAM(TRUE,0));
		SendDlgItemMessage(IDC_TRANSPAUSESTART, WM_SETFONT, (WPARAM)DlgFont, MAKELPARAM(TRUE,0));
		SendDlgItemMessage(IDCANCEL, WM_SETFONT, (WPARAM)DlgFont, MAKELPARAM(TRUE,0));
		SendDlgItemMessage(IDC_TRANSHELP, WM_SETFONT, (WPARAM)DlgFont, MAKELPARAM(TRUE,0));
	}

	return Ok;
}

void CFileTransDlg::ChangeButton(BOOL PauseFlag)
{
	Pause = PauseFlag;
	if (Pause) {
		get_lang_msg("DLG_FILETRANS_START", ts->UIMsg, sizeof(ts->UIMsg), "&Start", ts->UILanguageFile);
		SetDlgItemText(IDC_TRANSPAUSESTART, ts->UIMsg);
		cv->FilePause |= fv->OpId;
	}
	else {
		get_lang_msg("DLG_FILETRANS_PAUSE", ts->UIMsg, sizeof(ts->UIMsg), "Pau&se", ts->UILanguageFile);
		SetDlgItemText(IDC_TRANSPAUSESTART, ts->UIMsg);
		cv->FilePause &= ~fv->OpId;
	}
}

void CFileTransDlg::RefreshNum()
{
	char NumStr[24];
	double rate;
	int rate2;
	static DWORD prev_elapsed;
	DWORD elapsed;

	if (fv->OpId == OpSendFile) {
		if (fv->StartTime == 0) {
			SetDlgItemText(IDC_TRANS_ETIME, "0:00");
			prev_elapsed = 0;
		}
		else {
			elapsed = (GetTickCount() - fv->StartTime) / 1000;
			if (elapsed != prev_elapsed && elapsed != 0) {
				rate2 = fv->ByteCount / elapsed;
				if (rate2 < 1200) {
					_snprintf_s(NumStr, sizeof(NumStr), _TRUNCATE, "%d:%02d (%dBytes/s)", elapsed / 60, elapsed % 60, rate2);
				}
				else if (rate2 < 1200000) {
					_snprintf_s(NumStr, sizeof(NumStr), _TRUNCATE, "%d:%02d (%d.%02dKB/s)", elapsed / 60, elapsed % 60, rate2 / 1000, rate2 / 10 % 100);
				}
				else {
					_snprintf_s(NumStr, sizeof(NumStr), _TRUNCATE, "%d:%02d (%d.%02dMB/s)", elapsed / 60, elapsed % 60, rate2 / (1000*1000), rate2 / 10000 % 100);
				}
				SetDlgItemText(IDC_TRANS_ETIME, NumStr);
				prev_elapsed = elapsed;
			}
		}
	}

	if (fv->OpId == OpSendFile && fv->FileSize > 0) {
		rate = 100.0 * (double)fv->ByteCount / (double)fv->FileSize;
		if (fv->ProgStat < (int)rate) {
			fv->ProgStat = (int)rate;
			SendDlgItemMessage(IDC_TRANSPROGRESS, PBM_SETPOS, (WPARAM)fv->ProgStat, 0);
		}
		_snprintf_s(NumStr,sizeof(NumStr),_TRUNCATE,"%u (%3.1f%%)",fv->ByteCount, rate);
	}
	else {
		_snprintf_s(NumStr,sizeof(NumStr),_TRUNCATE,"%u",fv->ByteCount);
	}
	SetDlgItemText(IDC_TRANSBYTES, NumStr);
}

/////////////////////////////////////////////////////////////////////////////
// CFileTransDlg message handler

BOOL CFileTransDlg::OnInitDialog()
{
	int fuLoad = LR_DEFAULTCOLOR;

	if (fv->HideDialog) {
		// Visible = False でもフォアグラウンドに来てしまうので、そうならない
		// ように拡張スタイル WS_EX_NOACTIVATE を指定する。
		// (Windows 2000 以上で有効)
		// WS_EX_NOACTIVATE を指定すると表示されている時もタスクバーに現れない
		// ので WS_EX_APPWINDOW も指定する。
		ModifyStyleEx(0, WS_EX_NOACTIVATE | WS_EX_APPWINDOW);
	}

	SetWindowText(fv->DlgCaption);
	SetDlgItemText(IDC_TRANSFNAME, &(fv->FullName[fv->DirLen]));

	// ログファイルはフルパス表示にする(2004.8.6 yutaka)
	SetDlgItemText(IDC_EDIT_FULLPATH, &(fv->FullName[0]));

	if (is_NT4()) {
		fuLoad = LR_VGACOLOR;
	}
	::PostMessage(GetSafeHwnd(),WM_SETICON,ICON_SMALL,
	              (LPARAM)LoadImage(AfxGetInstanceHandle(),
	                                MAKEINTRESOURCE(IDI_TTERM),
	                                IMAGE_ICON,16,16,fuLoad));
	::PostMessage(GetSafeHwnd(),WM_SETICON,ICON_BIG,
	              (LPARAM)LoadImage(AfxGetInstanceHandle(),
	                                MAKEINTRESOURCE(IDI_TTERM),
	                                IMAGE_ICON, 0, 0, fuLoad));

	return 1;
}

void CFileTransDlg::OnCancel( )
{
	::PostMessage(fv->HMainWin,WM_USER_FTCANCEL,fv->OpId,0);
}

BOOL CFileTransDlg::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	switch (LOWORD(wParam)) {
		case IDCANCEL:
			::PostMessage(fv->HMainWin,WM_USER_FTCANCEL,fv->OpId,0);
			return TRUE;
		case IDC_TRANSPAUSESTART:
			ChangeButton(! Pause);
			return TRUE;
		case IDC_TRANSHELP:
			::PostMessage(fv->HMainWin,WM_USER_DLGHELP2,0,0);
			return TRUE;
		default:
			return (CDialog::OnCommand(wParam,lParam));
	}
}

void CFileTransDlg::PostNcDestroy()
{
	delete this;
}

LRESULT CFileTransDlg::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	return DefDlgProc(GetSafeHwnd(),message,wParam,lParam);
}
