/* Tera Term
 Copyright(C) 1994-1998 T. Teranishi
 All rights reserved. */

/* TTMACRO.EXE, status dialog box */

#include "stdafx.h"
#include "teraterm.h"
#include "ttlib.h"
#ifdef TERATERM32
#include "ttm_res.h"
#else
#include "ttm_re16.h"
#endif
#include "ttmlib.h"

#include "statdlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// CStatDlg dialog

BEGIN_MESSAGE_MAP(CStatDlg, CDialog)
	//{{AFX_MSG_MAP(CStatDlg)
	ON_MESSAGE(WM_EXITSIZEMOVE, OnExitSizeMove)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

BOOL CStatDlg::Create(PCHAR Text, PCHAR Title, int x, int y)
{
  RestoreNewLine(Text); // (2006.7.29 maya)
  TextStr = Text;
  TitleStr = Title;
  PosX = x;
  PosY = y;
  return CDialog::Create(CStatDlg::IDD, GetDesktopWindow());
}

void CStatDlg::Update(PCHAR Text, PCHAR Title, int x, int y)
{
  RECT R;
  HDC TmpDC;

  if (Title!=NULL) {
    SetWindowText(Title);
	TitleStr = Title;
  }

  GetWindowRect(&R);
  PosX = R.left;
  PosY = R.top;
  WW = R.right-R.left;
  WH = R.bottom-R.top;

  if (Text!=NULL) {
    TmpDC = ::GetDC(GetSafeHwnd());
    CalcTextExtent(TmpDC,Text,&s);
    ::ReleaseDC(GetSafeHwnd(),TmpDC);
    TW = s.cx + s.cx/10;
    TH = s.cy;

    SetDlgItemText(IDC_STATTEXT,Text);
	TextStr = Text;
  }

  if (x!=32767)
  {
    PosX = x;
    PosY = y;
  }

  Relocation(TRUE, WW);
}

// CStatDlg message handler

BOOL CStatDlg::OnInitDialog()
{
#ifdef I18N
  LOGFONT logfont;
  HFONT font;
#endif

  CDialog::OnInitDialog();
  Update(TextStr,TitleStr,PosX,PosY);
#ifdef TERATERM32
  SetForegroundWindow();
#else
  SetActiveWindow();
#endif

#ifdef I18N
  font = (HFONT)SendMessage(WM_GETFONT, 0, 0);
  GetObject(font, sizeof(LOGFONT), &logfont);
  if (get_lang_font("DLG_SYSTEM_FONT", m_hWnd, &logfont, &DlgFont, UILanguageFile)) {
    SendDlgItemMessage(IDC_STATTEXT, WM_SETFONT, (WPARAM)DlgFont, MAKELPARAM(TRUE,0));
  }
#endif

  return TRUE;
}

void CStatDlg::OnCancel()
{
  DestroyWindow();
}

BOOL CStatDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
  switch (LOWORD(wParam)) {
    case IDCANCEL:
      if ((HWND)lParam!=NULL) // ignore ESC key
	DestroyWindow();
      return TRUE;
    default:
      return (CDialog::OnCommand(wParam,lParam));
  }
}

void CStatDlg::PostNcDestroy()
{
  delete this;
}

LONG CStatDlg::OnExitSizeMove(UINT wParam, LONG lParam)
{
  RECT R;

  GetWindowRect(&R);
  if (R.bottom-R.top == WH && R.right-R.left == WW) {
    // �T�C�Y���ς���Ă��Ȃ���Ή������Ȃ�
  }
  else if (R.bottom-R.top != WH || R.right-R.left < init_WW) {
    // �������ύX���ꂽ���A�ŏ���蕝�������Ȃ����ꍇ�͌��ɖ߂�
    SetWindowPos(&wndTop,R.left,R.top,WW,WH,0);
  }
  else {
    // �����łȂ���΍Ĕz�u����
    Relocation(FALSE, R.right-R.left);
  }

  return CDialog::DefWindowProc(WM_EXITSIZEMOVE,wParam,lParam);
}

void CStatDlg::Relocation(BOOL is_init, int new_WW)
{
  RECT R;
  HDC TmpDC;
  HWND HText;
  int CW, CH;

  if (TextStr != NULL) {
    HText = ::GetDlgItem(GetSafeHwnd(), IDC_STATTEXT);

    GetClientRect(&R);
    CW = R.right-R.left;
    CH = R.bottom-R.top;

    // ����̂�
    if (is_init) {
      // �e�L�X�g�R���g���[���T�C�Y��␳
	  if (TW < CW)
        TW = CW;
      // �E�C���h�E�T�C�Y�̌v�Z
      WW = WW + TW - CW;
      WH = WH + 2*TH - CH;
	  init_WW = WW;
    }
    else {
      TW = CW;
      WW = new_WW;
    }

    ::MoveWindow(HText,(TW-s.cx)/2,TH/2,TW,TH,TRUE);
  }

  if (PosX<=-100)
  {
    TmpDC = ::GetDC(GetSafeHwnd());
    PosX = (GetDeviceCaps(TmpDC,HORZRES)-WW) / 2;
    PosY = (GetDeviceCaps(TmpDC,VERTRES)-WH) / 2;
    ::ReleaseDC(GetSafeHwnd(),TmpDC);
  }
  SetWindowPos(&wndTop,PosX,PosY,WW,WH,SWP_NOZORDER);

  InvalidateRect(NULL);
}