#pragma once
#include <windows.h>
#include "resource.h"

#define A8T_WM_THREADEND (WM_APP+1)


namespace app8tai
{
	const int PROGRESS_MAX = 500;
	struct TPARA {
		bool bStop;
		HWND hWnd;
		HWND hProgress;
		TCHAR szInput[MAX_PATH + 1];
		TCHAR szOutput[MAX_PATH + 1];
		LPCSTR lpBasename;
		LPCSTR lpExt;
	};

	INT_PTR WINAPI DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void OnInit(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void OnCommand(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void OnExit(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
}