/* Tera Term
 Copyright(C) 1994-1998 T. Teranishi
 All rights reserved. */

/* proto types */
#ifdef __cplusplus
extern "C" {
#endif

/* KeyDown return type */
#define KEYDOWN_COMMOUT	1	/* �����[�g�ɑ��M�iBS Enter Space�Ȃǁj */
#define KEYDOWN_CONTROL	2	/* Ctrl,Shift�Ȃ� */
#define KEYDOWN_OTHER	0	/* ���̑� */

#define DEBUG_FLAG_NONE  0
#define DEBUG_FLAG_NORM  1
#define DEBUG_FLAG_HEXD  2
#define DEBUG_FLAG_NOUT  3
#define DEBUG_FLAG_MAXD  4

void SetKeyMap();
void ClearUserKey();
void DefineUserKey(int NewKeyId, PCHAR NewKeyStr, int NewKeyLen);
int KeyDown(HWND HWin, WORD VKey, WORD Count, WORD Scan);
void KeyCodeSend(WORD KCode, WORD Count);
void KeyUp(WORD VKey);
BOOL ShiftKey();
BOOL ControlKey();
BOOL AltKey();
void InitKeyboard();
void EndKeyboard();

#define FuncKeyStrMax 32

extern BOOL AutoRepeatMode;
extern BOOL AppliKeyMode, AppliCursorMode;
extern BOOL Send8BitMode;
extern BYTE DebugFlag;

#ifdef __cplusplus
}
#endif