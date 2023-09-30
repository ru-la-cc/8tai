#include "MainDlg.h"
#include <tchar.h>

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPTSTR lpCmdLine, int nShowCmd)
{
	return static_cast<int>(::DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_MAINDLG), nullptr, app8tai::DlgProc, 0));
}