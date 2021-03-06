/* Tera Term
 Copyright(C) 1994-1998 T. Teranishi
 All rights reserved. */

/* TERATERM.EXE, Clipboard routines */
#include "teraterm.h"
#include "tttypes.h"
#include "vtdisp.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <commctrl.h>

#include "ttwinman.h"
#include "ttcommon.h"
#include "ttlib.h"

#include "clipboar.h"
#include "tt_res.h"

// for clipboard copy
static HGLOBAL CBCopyHandle = NULL;
static PCHAR CBCopyPtr = NULL;
static HGLOBAL CBCopyWideHandle = NULL;
static LPWSTR CBCopyWidePtr = NULL;

#define CB_BRACKET_NONE  0
#define CB_BRACKET_START 1
#define CB_BRACKET_END   2
// for clipboard paste
static HGLOBAL CBMemHandle = NULL;
static PCHAR CBMemPtr = NULL;
static LONG CBMemPtr2 = 0;
static BOOL CBAddCR = FALSE;
static int CBBracketed = CB_BRACKET_NONE;
static BYTE CBByte;
static BOOL CBRetrySend;
static BOOL CBRetryEcho;
static BOOL CBSendCR;
static BOOL CBDDE;
static BOOL CBWIDE;
static BOOL CBEchoOnly;
static BOOL CBInsertDelay = FALSE;

static HFONT DlgClipboardFont;

PCHAR CBOpen(LONG MemSize)
{
	if (MemSize==0) {
		return (NULL);
	}
	if (CBCopyHandle!=NULL) {
		return (NULL);
	}
	CBCopyPtr = NULL;
	CBCopyHandle = GlobalAlloc(GMEM_MOVEABLE, MemSize);
	if (CBCopyHandle == NULL) {
		MessageBeep(0);
	}
	else {
		CBCopyPtr = GlobalLock(CBCopyHandle);
		if (CBCopyPtr == NULL) {
			GlobalFree(CBCopyHandle);
			CBCopyHandle = NULL;
			MessageBeep(0);
		}
	}
	return (CBCopyPtr);
}

void CBClose()
{
	BOOL Empty;
	int WideCharLength;

	if (CBCopyHandle==NULL) {
		return;
	}

	WideCharLength = MultiByteToWideChar(CP_ACP, 0, CBCopyPtr, -1, NULL, 0);
	CBCopyWideHandle = GlobalAlloc(GMEM_MOVEABLE, sizeof(WCHAR) * WideCharLength);
	if (CBCopyWideHandle) {
		CBCopyWidePtr = (LPWSTR)GlobalLock(CBCopyWideHandle);
		MultiByteToWideChar(CP_ACP, 0, CBCopyPtr, -1, CBCopyWidePtr, WideCharLength);
		GlobalUnlock(CBCopyWideHandle);
	}

	Empty = FALSE;
	if (CBCopyPtr!=NULL) {
		Empty = (CBCopyPtr[0]==0);
	}

	GlobalUnlock(CBCopyHandle);
	CBCopyPtr = NULL;

	if (OpenClipboard(HVTWin)) {
		EmptyClipboard();
		if (! Empty) {
			SetClipboardData(CF_TEXT, CBCopyHandle);
			if (CBCopyWidePtr) {
				SetClipboardData(CF_UNICODETEXT, CBCopyWideHandle);
				CBCopyWidePtr = NULL;
			}
		}
		CloseClipboard();
	}
	CBCopyHandle = NULL;
	CBCopyWideHandle = NULL;
}

void CBStartPaste(HWND HWin, BOOL AddCR, BOOL Bracketed,
                  int BuffSize, PCHAR DataPtr, int DataSize)
//
//  DataPtr and DataSize are used only for DDE
//	  For clipboard, BuffSize should be 0
//	  DataSize should be <= BuffSize
{
	UINT Cf;

	if (! cv.Ready) {
		return;
	}
	if (TalkStatus!=IdTalkKeyb) {
		return;
	}

	CBAddCR = AddCR;
	if (Bracketed) {
		CBBracketed = CB_BRACKET_START;
	}

	if (BuffSize==0) { // for clipboar
		if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
			Cf = CF_UNICODETEXT;
		}
		else if (IsClipboardFormatAvailable(CF_TEXT)) {
			Cf = CF_TEXT;
		}
		else if (IsClipboardFormatAvailable(CF_OEMTEXT)) {
			Cf = CF_OEMTEXT;
		}
		else {
			return;
		}
	}

	CBEchoOnly = FALSE;

	CBMemHandle = NULL;
	CBMemPtr = NULL;
	CBMemPtr2 = 0;
	CBDDE = FALSE;
	CBWIDE = FALSE;
	CBInsertDelay = FALSE;

	if (BuffSize==0) { //clipboard
		if (ts.PasteDelayPerLine > 0) {
			CBInsertDelay = TRUE;
		}
		if (OpenClipboard(HWin)) {
			if (Cf == CF_UNICODETEXT) {
				// 貼り付け処理では CBMemHandle ではなく dde と同じように CBMemPtr が使われる
				HGLOBAL TmpHandle = GetClipboardData(Cf);
				CBWIDE = TRUE;
				if (TmpHandle) {
					LPWSTR TmpPtr = (LPWSTR)GlobalLock(TmpHandle);
					int mb_len = WideCharToMultiByte(CP_ACP, 0, TmpPtr, -1, 0, 0, NULL, NULL);

					CBMemHandle = GlobalAlloc(GHND, mb_len);
					if (CBMemHandle != NULL) {
						CBMemPtr = GlobalLock(CBMemHandle);
						if (CBMemPtr != NULL) {
							WideCharToMultiByte(CP_ACP, 0, TmpPtr, -1, CBMemPtr, mb_len, NULL, NULL);

							GlobalUnlock(CBMemHandle);
							CBMemPtr=NULL;
							TalkStatus=IdTalkCB;
						}

						GlobalUnlock(TmpHandle);
						CloseClipboard();
					}
				}
			}
			else {
				CBMemHandle = GetClipboardData(Cf);
				if (CBMemHandle!=NULL) {
					TalkStatus=IdTalkCB;
				}
			}
		}
	}
	else { // dde
		CBMemHandle = GlobalAlloc(GHND,BuffSize);
		if (CBMemHandle != NULL) {
			CBDDE = TRUE;
			CBMemPtr = GlobalLock(CBMemHandle);
			if (CBMemPtr != NULL) {
				memcpy(CBMemPtr,DataPtr,DataSize);
				GlobalUnlock(CBMemHandle);
				CBMemPtr=NULL;
				TalkStatus=IdTalkCB;
			}
		}
	}
	CBRetrySend = FALSE;
	CBRetryEcho = FALSE;
	CBSendCR = FALSE;
	if (TalkStatus != IdTalkCB) {
		CBEndPaste();
	}
}

void CBStartPasteB64(HWND HWin, PCHAR header, PCHAR footer)
{
	HANDLE tmpHandle = NULL;
	char *tmpPtr = NULL;
	int len, hlen, flen, blen;
	UINT Cf;
	LPWSTR tmpPtrWide = NULL;
	int mb_len;

	if (! cv.Ready) {
		return;
	}
	if (TalkStatus!=IdTalkKeyb) {
		return;
	}

	CBEchoOnly = FALSE;

	CBMemHandle = NULL;
	CBMemPtr = NULL;
	CBMemPtr2 = 0;
	CBDDE = TRUE;
	CBWIDE = FALSE;

	if (ts.PasteDelayPerLine > 0) {
		CBInsertDelay = TRUE;
	}
	else {
		CBInsertDelay = FALSE;
	}

	if (IsClipboardFormatAvailable(CF_UNICODETEXT) && OpenClipboard(HWin)) {
		Cf = CF_UNICODETEXT;
		if ((tmpHandle = GetClipboardData(CF_UNICODETEXT)) == NULL) {
			CloseClipboard();
		}
	}
	else if (IsClipboardFormatAvailable(CF_TEXT) && OpenClipboard(HWin)) {
		Cf = CF_TEXT;
		if ((tmpHandle = GetClipboardData(CF_TEXT)) == NULL) {
			CloseClipboard();
		}
	}
	else if (IsClipboardFormatAvailable(CF_OEMTEXT) && OpenClipboard(HWin)) {
		Cf = CF_OEMTEXT;
		if ((tmpHandle = GetClipboardData(CF_OEMTEXT)) == NULL) {
			CloseClipboard();
		}
	}

	if (Cf == CF_UNICODETEXT) {
		if (tmpHandle) {
			if ((tmpPtrWide = GlobalLock(tmpHandle)) != NULL) {
				hlen = strlen(header);
				flen = strlen(footer);
				mb_len = WideCharToMultiByte(CP_ACP, 0, tmpPtrWide, -1, 0, 0, NULL, NULL);
				blen = (mb_len + 2) / 3 * 4 + hlen + flen + 1;
				if ((CBMemHandle = GlobalAlloc(GHND, blen)) != NULL) {
					if ((CBMemPtr = GlobalLock(CBMemHandle)) != NULL) {
						if (hlen > 0) {
							strncpy_s(CBMemPtr, blen, header, _TRUNCATE);
						}
						tmpPtr = (char *)calloc(sizeof(char), mb_len);
						WideCharToMultiByte(CP_ACP, 0, tmpPtrWide, -1, tmpPtr, mb_len, NULL, NULL);
						b64encode(CBMemPtr + hlen, blen - hlen, tmpPtr, mb_len);
						free(tmpPtr);
						if (flen > 0) {
							strncat_s(CBMemPtr, blen, footer, _TRUNCATE);
						}
						TalkStatus=IdTalkCB;
						GlobalUnlock(CBMemPtr);
						CBMemPtr = NULL;
					}
				}
				GlobalUnlock(tmpPtrWide);
			}
			CloseClipboard();
		}
	}
	else {
		if (tmpHandle) {
			if ((tmpPtr = GlobalLock(tmpHandle)) != NULL) {
				hlen = strlen(header);
				flen = strlen(footer);
				len = strlen(tmpPtr);
				blen = (len + 2) / 3 * 4 + hlen + flen + 1;
				if ((CBMemHandle = GlobalAlloc(GHND, blen)) != NULL) {
					if ((CBMemPtr = GlobalLock(CBMemHandle)) != NULL) {
						if (hlen > 0) {
							strncpy_s(CBMemPtr, blen, header, _TRUNCATE);
						}
						b64encode(CBMemPtr + hlen, blen - hlen, tmpPtr, len);
						if (flen > 0) {
							strncat_s(CBMemPtr, blen, footer, _TRUNCATE);
						}
						TalkStatus=IdTalkCB;
						GlobalUnlock(CBMemPtr);
						CBMemPtr = NULL;
					}
				}
				GlobalUnlock(tmpPtr);
			}
			CloseClipboard();
		}
	}

	CBRetrySend = FALSE;
	CBRetryEcho = FALSE;
	CBSendCR = FALSE;
	if (TalkStatus != IdTalkCB) {
		CBEndPaste();
	}
}

void CBStartEcho(PCHAR DataPtr, int DataSize)
{
	if (! cv.Ready) {
		return;
	}
	if (TalkStatus!=IdTalkKeyb) {
		return;
	}

	CBEchoOnly = TRUE;
	CBMemPtr2 = 0;
	CBRetryEcho = FALSE;
	CBSendCR = FALSE;
	CBWIDE = FALSE;

	CBDDE = TRUE;
	if ((CBMemHandle = GlobalAlloc(GHND, DataSize)) != NULL) {
		if ((CBMemPtr = GlobalLock(CBMemHandle)) != NULL) {
			memcpy(CBMemPtr, DataPtr, DataSize);
			GlobalUnlock(CBMemHandle);
			CBMemPtr=NULL;
			TalkStatus=IdTalkCB;
		}
	}

	if (TalkStatus != IdTalkCB) {
		CBEndPaste();
	}
}

// この関数はクリップボードおよびDDEデータを端末へ送り込む。
//
// CBMemHandleハンドルはグローバル変数なので、この関数が終了するまでは、
// 次のクリップボードおよびDDEデータを処理することはできない（破棄される可能性あり）。
// また、データ列で null-terminate されていることを前提としているため、後続のデータ列は
// 無視される。
// (2006.11.6 yutaka)
void CBSend()
{
	int c;
	BOOL EndFlag;
	static DWORD lastcr;
	static char BracketStart[] = "\033[200~";
	static char BracketEnd[] = "\033[201~";
	static int BracketPtr = 0;
	DWORD now;

	if (CBMemHandle==NULL) {
		return;
	}

	if (CBEchoOnly) {
		CBEcho();
		return;
	}

	if (CBInsertDelay) {
		now = GetTickCount();
		if (now - lastcr < (DWORD)ts.PasteDelayPerLine) {
			return;
		}
	}

	if (CBRetrySend) {
		CBRetryEcho = (ts.LocalEcho>0);
		c = CommTextOut(&cv,(PCHAR)&CBByte,1);
		CBRetrySend = (c==0);
		if (CBRetrySend) {
			return;
		}
	}

	if (CBRetryEcho) {
		c = CommTextEcho(&cv,(PCHAR)&CBByte,1);
		CBRetryEcho = (c==0);
		if (CBRetryEcho) {
			return;
		}
	}

	CBMemPtr = GlobalLock(CBMemHandle);
	if (CBMemPtr==NULL) {
		return;
	}

	do {
		if (CBSendCR && (CBMemPtr[CBMemPtr2]==0x0a)) {
			CBMemPtr2++;
			// added PasteDelayPerLine (2009.4.12 maya)
			if (CBInsertDelay) {
				lastcr = now;
				CBSendCR = FALSE;
				SetTimer(HVTWin, IdPasteDelayTimer, ts.PasteDelayPerLine, NULL);
				break;
			}
		}

		EndFlag = (CBMemPtr[CBMemPtr2]==0);
		if (CBBracketed == CB_BRACKET_START) {
			CBByte = BracketStart[BracketPtr++];
			if (BracketPtr >= sizeof(BracketStart) - 1) {
				CBBracketed = CB_BRACKET_END;
				BracketPtr = 0;
			}
		}
		else if (! EndFlag) {
			CBByte = CBMemPtr[CBMemPtr2];
			CBMemPtr2++;
// Decoding characters which are encoded by MACRO
//   to support NUL character sending
//
//  [encoded character] --> [decoded character]
//         01 01        -->     00
//         01 02        -->     01
			if (CBByte==0x01) { /* 0x01 from MACRO */
				CBByte = CBMemPtr[CBMemPtr2];
				CBMemPtr2++;
				CBByte = CBByte - 1; // character just after 0x01
			}
		}
		else if (CBAddCR) {
			EndFlag = FALSE;
			CBAddCR = FALSE;
			CBByte = 0x0d;
		}
		else if (CBBracketed == CB_BRACKET_END) {
			EndFlag = FALSE;
			CBByte = BracketEnd[BracketPtr++];
			if (BracketPtr >= sizeof(BracketEnd) - 1) {
				CBBracketed = CB_BRACKET_NONE;
				BracketPtr = 0;
			}
		}
		else {
			CBEndPaste();
			return;
		}

		if (! EndFlag) {
			c = CommTextOut(&cv,(PCHAR)&CBByte,1);
			CBSendCR = (CBByte==0x0D);
			CBRetrySend = (c==0);
			if ((! CBRetrySend) &&
			    (ts.LocalEcho>0)) {
				c = CommTextEcho(&cv,(PCHAR)&CBByte,1);
				CBRetryEcho = (c==0);
			}
		}
		else {
			c=0;
		}
	}
	while (c>0);

	if (CBMemPtr!=NULL) {
		GlobalUnlock(CBMemHandle);
		CBMemPtr=NULL;
	}
}

void CBEcho()
{
	if (CBMemHandle==NULL) {
		return;
	}

	if (CBRetryEcho && CommTextEcho(&cv,(PCHAR)&CBByte,1) == 0) {
		return;
	}

	if ((CBMemPtr = GlobalLock(CBMemHandle)) == NULL) {
		return;
	}

	do {
		if (CBSendCR && (CBMemPtr[CBMemPtr2]==0x0a)) {
			CBMemPtr2++;
		}

		if (CBMemPtr[CBMemPtr2] == 0) {
			CBRetryEcho = FALSE;
			CBEndPaste();
			return;
		}

		CBByte = CBMemPtr[CBMemPtr2];
		CBMemPtr2++;

		// Decoding characters which are encoded by MACRO
		//   to support NUL character sending
		//
		//  [encoded character] --> [decoded character]
		//         01 01        -->     00
		//         01 02        -->     01
		if (CBByte==0x01) { /* 0x01 from MACRO */
			CBByte = CBMemPtr[CBMemPtr2];
			CBMemPtr2++;
			CBByte = CBByte - 1; // character just after 0x01
		}

		CBSendCR = (CBByte==0x0D);

	} while (CommTextEcho(&cv,(PCHAR)&CBByte,1) > 0);

	CBRetryEcho = TRUE;

	if (CBMemHandle != NULL) {
		GlobalUnlock(CBMemHandle);
		CBMemPtr=NULL;
	}
}

void CBEndPaste()
{
	TalkStatus = IdTalkKeyb;

	if (CBMemHandle!=NULL) {
		if (CBMemPtr!=NULL) {
			GlobalUnlock(CBMemHandle);
		}
		if (CBDDE || CBWIDE) {
			GlobalFree(CBMemHandle);
		}
	}
	if (!CBDDE && !CBWIDE) {
		CloseClipboard();
	}

	CBDDE = FALSE;
	CBWIDE = FALSE;
	CBMemHandle = NULL;
	CBMemPtr = NULL;
	CBMemPtr2 = 0;
	CBAddCR = FALSE;
	CBEchoOnly = FALSE;
	CBInsertDelay = FALSE;
}


static char *ClipboardPtr = NULL;
static int PasteCanceled = 0;

static LRESULT CALLBACK OnClipboardDlgProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	LOGFONT logfont;
	HFONT font;
	char uimsg[MAX_UIMSG];
	//char *p;
	POINT p;
	RECT rc_dsk, rc_dlg;
	int dlg_height, dlg_width;
	OSVERSIONINFO osvi;
	static int ok2right, edit2ok, edit2bottom;
	RECT rc_edit, rc_ok, rc_cancel;
	// for status bar
	static HWND hStatus = NULL;
	static init_width, init_height;

	switch (msg) {
		case WM_INITDIALOG:
#if 0
			for (p = ClipboardPtr; *p ; p++) {
				char buf[20];
				_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%02x ", *p);
				OutputDebugString(buf);
			}
#endif

			font = (HFONT)SendMessage(hDlgWnd, WM_GETFONT, 0, 0);
			GetObject(font, sizeof(LOGFONT), &logfont);
			if (get_lang_font("DLG_TAHOMA_FONT", hDlgWnd, &logfont, &DlgClipboardFont, ts.UILanguageFile)) {
				SendDlgItemMessage(hDlgWnd, IDC_EDIT, WM_SETFONT, (WPARAM)DlgClipboardFont, MAKELPARAM(TRUE,0));
				SendDlgItemMessage(hDlgWnd, IDOK, WM_SETFONT, (WPARAM)DlgClipboardFont, MAKELPARAM(TRUE,0));
				SendDlgItemMessage(hDlgWnd, IDCANCEL, WM_SETFONT, (WPARAM)DlgClipboardFont, MAKELPARAM(TRUE,0));
			}
			else {
				DlgClipboardFont = NULL;
			}

			GetWindowText(hDlgWnd, uimsg, sizeof(uimsg));
			get_lang_msg("DLG_CLIPBOARD_TITLE", ts.UIMsg, sizeof(ts.UIMsg), uimsg, ts.UILanguageFile);
			SetWindowText(hDlgWnd, ts.UIMsg);
			GetDlgItemText(hDlgWnd, IDCANCEL, uimsg, sizeof(uimsg));
			get_lang_msg("BTN_CANCEL", ts.UIMsg, sizeof(ts.UIMsg), uimsg, ts.UILanguageFile);
			SetDlgItemText(hDlgWnd, IDCANCEL, ts.UIMsg);
			GetDlgItemText(hDlgWnd, IDOK, uimsg, sizeof(uimsg));
			get_lang_msg("BTN_OK", ts.UIMsg, sizeof(ts.UIMsg), uimsg, ts.UILanguageFile);
			SetDlgItemText(hDlgWnd, IDOK, ts.UIMsg);

			SendMessage(GetDlgItem(hDlgWnd, IDC_EDIT), WM_SETTEXT, 0, (LPARAM)ClipboardPtr);

			DispConvScreenToWin(CursorX, CursorY, &p.x, &p.y);
			ClientToScreen(HVTWin, &p);

			// キャレットが画面からはみ出しているときに貼り付けをすると
			// 確認ウインドウが見えるところに表示されないことがある。
			// ウインドウからはみ出した場合に調節する (2008.4.24 maya)
			osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			GetVersionEx(&osvi);
			if ( (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion == 4) ||
			     (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS && osvi.dwMinorVersion < 10) ) {
				// NT4.0, 95 はマルチモニタAPIに非対応
				SystemParametersInfo(SPI_GETWORKAREA, 0, &rc_dsk, 0);
			}
			else {
				HMONITOR hm;
				POINT pt;
				MONITORINFO mi;

				pt.x = p.x;
				pt.y = p.y;
				hm = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

				mi.cbSize = sizeof(MONITORINFO);
				GetMonitorInfo(hm, &mi);
				rc_dsk = mi.rcWork;
			}
			GetWindowRect(hDlgWnd, &rc_dlg);
			dlg_height = rc_dlg.bottom-rc_dlg.top;
			dlg_width  = rc_dlg.right-rc_dlg.left;
			if (p.y < rc_dsk.top) {
				p.y = rc_dsk.top;
			}
			else if (p.y + dlg_height > rc_dsk.bottom) {
				p.y = rc_dsk.bottom - dlg_height;
			}
			if (p.x < rc_dsk.left) {
				p.x = rc_dsk.left;
			}
			else if (p.x + dlg_width > rc_dsk.right) {
				p.x = rc_dsk.right - dlg_width;
			}

			SetWindowPos(hDlgWnd, NULL, p.x, p.y,
			             0, 0, SWP_NOSIZE | SWP_NOZORDER);

			// ダイアログの初期サイズを保存
			GetWindowRect(hDlgWnd, &rc_dlg);
			init_width = rc_dlg.right - rc_dlg.left;
			init_height = rc_dlg.bottom - rc_dlg.top;

			// 現在サイズから必要な値を計算
			GetClientRect(hDlgWnd,                                 &rc_dlg);
			GetWindowRect(GetDlgItem(hDlgWnd, IDC_EDIT),           &rc_edit);
			GetWindowRect(GetDlgItem(hDlgWnd, IDOK),               &rc_ok);

			p.x = rc_dlg.right;
			p.y = rc_dlg.bottom;
			ClientToScreen(hDlgWnd, &p);
			ok2right = p.x - rc_ok.left;
			edit2bottom = p.y - rc_edit.bottom;
			edit2ok = rc_ok.left - rc_edit.right;

			// サイズを復元
			SetWindowPos(hDlgWnd, NULL, 0, 0,
			             ts.PasteDialogSize.cx, ts.PasteDialogSize.cy,
			             SWP_NOZORDER | SWP_NOMOVE);

			// リサイズアイコンを右下に表示させたいので、ステータスバーを付ける。
			InitCommonControls();
			hStatus = CreateStatusWindow(
				WS_CHILD | WS_VISIBLE |
				CCS_BOTTOM | SBARS_SIZEGRIP, NULL, hDlgWnd, 1);

			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wp)) {
				case IDOK:
				{
					int len = SendMessage(GetDlgItem(hDlgWnd, IDC_EDIT), WM_GETTEXTLENGTH, 0, 0);
					HGLOBAL hMem;
					char *buf;
					int wide_len;
					HGLOBAL wide_hMem;
					LPWSTR wide_buf;

					if (OpenClipboard(hDlgWnd) != 0) {
						hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
						buf = GlobalLock(hMem);
						SendMessage(GetDlgItem(hDlgWnd, IDC_EDIT), WM_GETTEXT, len + 1, (LPARAM)buf);
						GlobalUnlock(hMem);

						wide_len = MultiByteToWideChar(CP_ACP, 0, buf, -1, NULL, 0);
						wide_hMem = GlobalAlloc(GMEM_MOVEABLE, sizeof(WCHAR) * wide_len);
						if (wide_hMem) {
							wide_buf = (LPWSTR)GlobalLock(wide_hMem);
							MultiByteToWideChar(CP_ACP, 0, buf, -1, wide_buf, wide_len);
							GlobalUnlock(wide_hMem);
						}

						EmptyClipboard();
						SetClipboardData(CF_TEXT, hMem);
						if (wide_buf) {
							SetClipboardData(CF_UNICODETEXT, wide_hMem);
						}
						CloseClipboard();
						// hMemはクリップボードが保持しているので、破棄してはいけない。
					}

					if (DlgClipboardFont != NULL) {
						DeleteObject(DlgClipboardFont);
					}

					DestroyWindow(hStatus);
					EndDialog(hDlgWnd, IDOK);
				}
					break;

				case IDCANCEL:
					PasteCanceled = 1;

					if (DlgClipboardFont != NULL) {
						DeleteObject(DlgClipboardFont);
					}

					DestroyWindow(hStatus);
					EndDialog(hDlgWnd, IDCANCEL);
					break;

				default:
					return FALSE;
			}

#if 0
		case WM_CLOSE:
			PasteCanceled = 1;
			EndDialog(hDlgWnd, 0);
			return TRUE;
#endif

		case WM_SIZE:
			{
				// 再配置
				POINT p;
				int dlg_w, dlg_h;

				GetClientRect(hDlgWnd,                                 &rc_dlg);
				dlg_w = rc_dlg.right;
				dlg_h = rc_dlg.bottom;

				GetWindowRect(GetDlgItem(hDlgWnd, IDC_EDIT),           &rc_edit);
				GetWindowRect(GetDlgItem(hDlgWnd, IDOK),               &rc_ok);
				GetWindowRect(GetDlgItem(hDlgWnd, IDCANCEL),           &rc_cancel);

				// OK
				p.x = rc_ok.left;
				p.y = rc_ok.top;
				ScreenToClient(hDlgWnd, &p);
				SetWindowPos(GetDlgItem(hDlgWnd, IDOK), 0,
				             dlg_w - ok2right, p.y, 0, 0,
				             SWP_NOSIZE | SWP_NOZORDER);

				// CANCEL
				p.x = rc_cancel.left;
				p.y = rc_cancel.top;
				ScreenToClient(hDlgWnd, &p);
				SetWindowPos(GetDlgItem(hDlgWnd, IDCANCEL), 0,
				             dlg_w - ok2right, p.y, 0, 0,
				             SWP_NOSIZE | SWP_NOZORDER);

				// EDIT
				p.x = rc_edit.left;
				p.y = rc_edit.top;
				ScreenToClient(hDlgWnd, &p);
				SetWindowPos(GetDlgItem(hDlgWnd, IDC_EDIT), 0,
				             0, 0, dlg_w - p.x - edit2ok - ok2right, dlg_h - p.y - edit2bottom,
				             SWP_NOMOVE | SWP_NOZORDER);

				// サイズを保存
				GetWindowRect(hDlgWnd, &rc_dlg);
				ts.PasteDialogSize.cx = rc_dlg.right - rc_dlg.left;
				ts.PasteDialogSize.cy = rc_dlg.bottom - rc_dlg.top;

				// status bar
				SendMessage(hStatus , msg , wp , lp);
			}
			return TRUE;

		case WM_GETMINMAXINFO:
			{
				// ダイアログの初期サイズより小さくできないようにする
				LPMINMAXINFO lpmmi;
				lpmmi = (LPMINMAXINFO)lp;
				lpmmi->ptMinTrackSize.x = init_width;
				lpmmi->ptMinTrackSize.y = init_height;
			}
			return FALSE;

		default:
			return FALSE;
	}
	return TRUE;
}

// ファイルに定義された文字列が、textに含まれるかを調べる。
static int search_clipboard(char *filename, char *text)
{
	int ret = 0;  // no hit
	FILE *fp = NULL;
	char buf[256];
	int len;
	
	if (filename == NULL || filename[0] == '\0')
		goto error;

	fp = fopen(filename, "r");
	if (fp == NULL)
		goto error;

	// TODO: 一行が256byteを超えている場合の考慮
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		len = strlen(buf);
		if (buf[len - 1] == '\n') 
			buf[len - 1] = '\0';
		if (buf[0] == '\0')
			continue;
		if (strstr(text, buf)) { // hit
			ret = 1;
			break;
		}
	}

error:
	if (fp)
		fclose(fp);

	return (ret);
}


//
// クリップボードに改行コードが含まれていたら、確認ダイアログを表示する。
// クリップボードの変更も可能。
//
// return 0: Cancel
//        1: Paste OK
//
// (2008.2.3 yutaka)
//
int CBStartPasteConfirmChange(HWND HWin, BOOL AddCR)
{
	UINT Cf;
	HANDLE hText;
	char *pText;
	int pos;
	int ret = 0;
	BOOL confirm = FALSE;
	HANDLE wide_hText;
	LPWSTR wide_buf;
	int mb_len;

	if (ts.ConfirmChangePaste == 0)
		return 1;

	if (! cv.Ready) 
		goto error;
	if (TalkStatus!=IdTalkKeyb)
		goto error;

	if (IsClipboardFormatAvailable(CF_UNICODETEXT))
		Cf = CF_UNICODETEXT;
	else if (IsClipboardFormatAvailable(CF_TEXT))
		Cf = CF_TEXT;
	else if (IsClipboardFormatAvailable(CF_OEMTEXT))
		Cf = CF_OEMTEXT;
	else 
		goto error;

	if (!OpenClipboard(HWin)) 
		goto error;

	if (Cf == CF_UNICODETEXT) {
		wide_hText = GetClipboardData(CF_UNICODETEXT);
		if (wide_hText != NULL) {
			wide_buf = GlobalLock(wide_hText);
			mb_len = WideCharToMultiByte(CP_ACP, 0, wide_buf, -1, NULL, 0, NULL, NULL);
			ClipboardPtr = (char *)calloc(sizeof(char), mb_len);
			WideCharToMultiByte(CP_ACP, 0, wide_buf, -1, ClipboardPtr, mb_len, NULL, NULL);
			GlobalUnlock(wide_hText);
		}
		else {
			CloseClipboard();
			goto error;
		}
	}
	else {
		hText = GetClipboardData(Cf);
		if (hText != NULL) {
			pText = (char *)GlobalLock(hText);
			ClipboardPtr = (char *)calloc(sizeof(char), strlen(pText)+1);
			memcpy(ClipboardPtr, pText, strlen(pText));
			GlobalUnlock(hText);
		}
		else {
			CloseClipboard();
			goto error;
		}
	}
	CloseClipboard();

	if (AddCR) {
		if (ts.ConfirmChangePasteCR) {
			confirm = TRUE;
		}
	}
	else {
		pos = strcspn(ClipboardPtr, "\r\n");  // 改行が含まれていたら
		if (ClipboardPtr[pos] != '\0' || AddCR) {
			confirm = TRUE;
		}
	}

	// 辞書をサーチする
	if (!confirm && search_clipboard(ts.ConfirmChangePasteStringFile, ClipboardPtr)) {
		confirm = TRUE;
	}

	if (confirm) {
		PasteCanceled = 0;
		ret = DialogBox(hInst, MAKEINTRESOURCE(IDD_CLIPBOARD_DIALOG),
		                HVTWin, (DLGPROC)OnClipboardDlgProc);
		if (ret == 0 || ret == -1) {
			ret = GetLastError();
		}

		if (PasteCanceled) {
			ret = 0;
			goto error;
		}
	}

	ret = 1;

error:
	if (ClipboardPtr != NULL) {
		free(ClipboardPtr);
		ClipboardPtr = NULL;
	}
	return (ret);
}
