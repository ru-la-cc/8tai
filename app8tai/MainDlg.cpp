#include "MainDlg.h"
#include <tchar.h>
#include <shlobj.h>
#include <process.h>
#include <string>
#include <opencv2/opencv.hpp>

#pragma comment(lib, "opencv_world480.lib")

using namespace std;

namespace app8tai
{
	static bool isCoInit = false;
	static WNDPROC lpEditInputProc = nullptr, lpEditOutputProc = nullptr;

	static HRESULT CoInit()
	{
		if (!isCoInit) {
			isCoInit = true;
			return ::CoInitialize(NULL);
		}
		return S_FALSE;
	}

	void CoUninit()
	{
		if (isCoInit) {
			::CoUninitialize();
			isCoInit = false;
		}
	}

	static void EnableControls(HWND hWnd, BOOL bEnable = TRUE)
	{
		::EnableWindow(::GetDlgItem(hWnd, IDC_EDIT_INPUTFILE), bEnable);
		::EnableWindow(::GetDlgItem(hWnd, IDC_EDIT_OUTPUTDIR), bEnable);
		::EnableWindow(::GetDlgItem(hWnd, IDC_BUTTON_FILE), bEnable);
		::EnableWindow(::GetDlgItem(hWnd, IDC_BUTTON_DIR), bEnable);
		::EnableWindow(::GetDlgItem(hWnd, IDC_BUTTON_START), bEnable);
		::EnableWindow(::GetDlgItem(hWnd, IDC_BUTTON_STOP), bEnable ? FALSE : TRUE);
		::EnableWindow(::GetDlgItem(hWnd, IDC_BUTTON_EXIT), bEnable);
	}

	static BOOL OpenFileName(HWND hWnd, LPTSTR lpFileName, DWORD dwLen, LPCTSTR lpTitle, DWORD dwFlg = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY)
	{
		OPENFILENAME ofn;
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = hWnd;
		ofn.hInstance = nullptr;
		ofn.lpstrFilter = TEXT("どうがファイル(*.mp4;*.m4v;*.wmv;*.avi;*.mov;*.mpeg;*.mpg;*.divx)\0*.mp4;*.m4v;*.wmv;*.avi;*.mov;*.mpeg;*.mpg;*.divx\0\0");
		ofn.lpstrCustomFilter = nullptr;
		ofn.nMaxCustFilter = 0;
		ofn.nFilterIndex = 1;
		ofn.lpstrFile = lpFileName;
		ofn.nMaxFile = dwLen;
		ofn.lpstrFileTitle = nullptr;
		ofn.nMaxFileTitle = MAX_PATH;
		ofn.lpstrInitialDir = nullptr;
		ofn.lpstrTitle = lpTitle;
		ofn.Flags = dwFlg;
		ofn.nFileOffset = 0;
		ofn.nFileExtension = 0;
		ofn.lpstrDefExt = nullptr;
		ofn.lCustData = 0;
		ofn.lpfnHook = nullptr;
		ofn.lpTemplateName = nullptr;

		if (::GetOpenFileName(&ofn)) {
			lpFileName[dwLen - 1] = TEXT('\0');
			return TRUE;
		}
		return FALSE;
	}

	static bool SelectFolder(HWND hWnd, LPTSTR lpDir, int nLen, LPCTSTR lpTitle, UINT uFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE)
	{
		BROWSEINFO bi;
		LPITEMIDLIST idl;
		// LPMALLOC lpMalloc;
		TCHAR szDir[MAX_PATH * 4 + 1];
		bool ret;

		ret = false;

		CoInit();

		// ::SHGetMalloc(&lpMalloc);

		bi.hwndOwner = hWnd;
		bi.pidlRoot = NULL;
		bi.pszDisplayName = szDir;
		bi.lpszTitle = lpTitle;
		bi.ulFlags = uFlags;
		bi.lpfn = NULL;
		bi.lParam = 0;
		bi.iImage = 0;

		idl = ::SHBrowseForFolder(&bi);
		if (idl != NULL) {
			if (!::SHGetPathFromIDList(idl, szDir)) szDir[0] = TEXT('\0');
			::CoTaskMemFree(idl);
			::lstrcpyn(lpDir, szDir, nLen);
			ret = true;
		}
		// ::CoUninitialize();

		return ret;
	}

	static void OnDropFiles(HWND hWnd, WPARAM wParam, int id)
	{
		HDROP hDrop = reinterpret_cast<HDROP>(wParam);
		HANDLE hFind;
		TCHAR szFileName[MAX_PATH + 1];
		WIN32_FIND_DATA w32fd;
		::DragQueryFile(hDrop, 0, szFileName, MAX_PATH);
		::DragFinish(hDrop);
		szFileName[MAX_PATH] = TEXT('\0');
		hFind = ::FindFirstFile(szFileName, &w32fd);
		if (hFind != INVALID_HANDLE_VALUE) {
			::FindClose(hFind);
			switch (id) {
			case IDC_EDIT_INPUTFILE:
				if (!(w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
					::SetWindowText(hWnd, szFileName);
				}
				break;
			case IDC_EDIT_OUTPUTDIR:
				if (w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
					::SetWindowText(hWnd, szFileName);
				}
				break;
			}
		}
	}


	static LRESULT WINAPI EditInputProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg) {
		case WM_DROPFILES:
			OnDropFiles(hWnd, wParam, IDC_EDIT_INPUTFILE);
			return 0;
		}
		return ::CallWindowProc(lpEditInputProc, hWnd, msg, wParam, lParam);
	}

	static LRESULT WINAPI EditOutputProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg) {
		case WM_DROPFILES:
			OnDropFiles(hWnd, wParam, IDC_EDIT_OUTPUTDIR);
			return 0;
		}
		return ::CallWindowProc(lpEditOutputProc, hWnd, msg, wParam, lParam);
	}

	static HANDLE hThread = 0;
	static bool bStop = false;
	static TPARA para;

	// とりあえず今回は手抜き。Unicodeパスつかうな。
	static string ToString(LPCTSTR lpString)
	{
#ifndef UNICODE
		return move(string(lpString));
#else
		auto bufsize = ::lstrlen(lpString) * 2;
		char* buf = new char[bufsize + 1];
		::WideCharToMultiByte(CP_ACP, 0, lpString, -1, buf, bufsize, NULL, NULL);
		string result(buf);
		delete[] buf;
		return move(result);
#endif
	}

	static unsigned __stdcall CutThread(void* param)
	{
		TPARA* para = reinterpret_cast<TPARA*>(param);
		auto cap = cv::VideoCapture(ToString(para->szInput), cv::CAP_FFMPEG);
		string outdir = ToString(para->szOutput);
		cv::Mat frame;

		int result = cap.isOpened() ? 0 : 2;
		int fc = 0;
		int totalframe = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
		char outfilename[MAX_PATH + 1];

		if (outdir.length() > 0 && outdir[outdir.length() - 1] != '\\') outdir += '\\';
		while (!result && cap.read(frame)) {
			if (para->bStop) {
				if (::MessageBox(para->hWnd, TEXT("もうやめとくか？"), TEXT("ちゅうだん"), MB_ICONWARNING | MB_YESNO) == IDYES) {
					result = 1;
					break;
				}
				else {
					para->bStop = false;
				}
			}
			snprintf(outfilename, MAX_PATH, "%s%s%06u.%s", outdir.c_str(), para->lpBasename, ++fc, para->lpExt);
			::SendMessage(para->hProgress, PBM_SETPOS, fc * PROGRESS_MAX / totalframe, 0);
			cv::imwrite(outfilename, frame);
		}
		::SendMessage(para->hProgress, PBM_SETPOS, PROGRESS_MAX, 0);
		int cnt = 0;
		while (!::PostMessage(para->hWnd, A8T_WM_THREADEND, result, 0)) {
			++cnt;
			if (cnt > 10) break;
			::Sleep(100);
		}
		return 0;
	}

	INT_PTR CALLBACK DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg) {
		case WM_INITDIALOG:
			OnInit(hWnd, msg, wParam, lParam);
			return TRUE;

		case WM_COMMAND:
			OnCommand(hWnd, msg, wParam, lParam);
			return TRUE;

		case WM_CLOSE:
			if (!::IsWindowEnabled(::GetDlgItem(hWnd, IDC_BUTTON_EXIT))) {
				::MessageBox(hWnd, TEXT("まだおわってない"), TEXT("むり"), MB_ICONWARNING | MB_OK);
				return TRUE;
			}
			OnExit(hWnd, msg, wParam, lParam);
			::EndDialog(hWnd, 0);
			CoUninit();
			return TRUE;

		case A8T_WM_THREADEND:
			if (hThread) {
				::WaitForSingleObject(hThread, INFINITE);
				::CloseHandle(hThread);
				hThread = 0;
				EnableControls(hWnd);
			}
			if (wParam == 1) {
				::MessageBox(hWnd, TEXT("しょりやめた"), TEXT("ちゅうだん"), MB_ICONWARNING | MB_OK);
			}
			else if (wParam == 2) {
				::MessageBox(hWnd, TEXT("なんかえらーでた"), TEXT("ちゅうだん"), MB_ICONERROR | MB_OK);
			}
			else {
				::MessageBox(hWnd, TEXT("しょりおわった"), TEXT("かんぺき"), MB_ICONINFORMATION | MB_OK);
			}
			::SendDlgItemMessage(hWnd, IDC_PROGRESS1, PBM_SETPOS, 0, 0);
			return TRUE;
		}
		return FALSE;
	}

	void OnInit(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		EnableControls(hWnd);
		::SendDlgItemMessage(hWnd, IDC_PROGRESS1, PBM_SETRANGE32, 0, PROGRESS_MAX);
		::SendDlgItemMessage(hWnd, IDC_PROGRESS1, PBM_SETPOS, 0, 0);
		lpEditInputProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(::GetDlgItem(hWnd, IDC_EDIT_INPUTFILE), GWLP_WNDPROC));
		lpEditOutputProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(::GetDlgItem(hWnd, IDC_EDIT_OUTPUTDIR), GWLP_WNDPROC));
		::SetWindowLongPtr(::GetDlgItem(hWnd, IDC_EDIT_INPUTFILE), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditInputProc));
		::SetWindowLongPtr(::GetDlgItem(hWnd, IDC_EDIT_OUTPUTDIR), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditOutputProc));
	}

	void OnCommand(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (LOWORD(wParam))
		{
		case IDC_BUTTON_FILE:
			TCHAR szInFile[MAX_PATH + 1];
			::GetDlgItemText(hWnd, IDC_EDIT_INPUTFILE, szInFile, MAX_PATH);
			szInFile[MAX_PATH] = TEXT('\0');
			if (OpenFileName(hWnd, szInFile, sizeof(szInFile) / sizeof(TCHAR), TEXT("どうがふぁいるをえらべ"))) {
				::SetDlgItemText(hWnd, IDC_EDIT_INPUTFILE, szInFile);
			}
			break;

		case IDC_BUTTON_DIR:
			TCHAR szOutDir[MAX_PATH + 1];
			::GetDlgItemText(hWnd, IDC_EDIT_OUTPUTDIR, szOutDir, MAX_PATH);
			szOutDir[MAX_PATH];
			if (SelectFolder(hWnd, szOutDir, sizeof(szOutDir) / sizeof(TCHAR), TEXT("ふれーむしゅつりょくさきをえらべ"))) {
				::SetDlgItemText(hWnd, IDC_EDIT_OUTPUTDIR, szOutDir);
			}
			break;

		case IDC_BUTTON_START:
			EnableControls(hWnd, FALSE);
			para.bStop = false;
			para.hProgress = ::GetDlgItem(hWnd, IDC_PROGRESS1);
			para.hWnd = hWnd;
			para.lpBasename = "IMG_";
			para.lpExt = "png";
			::GetDlgItemText(hWnd, IDC_EDIT_INPUTFILE, para.szInput, MAX_PATH);
			para.szInput[MAX_PATH] = TEXT('\0');
			::GetDlgItemText(hWnd, IDC_EDIT_OUTPUTDIR, para.szOutput, MAX_PATH);
			para.szOutput[MAX_PATH] = TEXT('\0');
			if (!para.szInput[0]) {
				::MessageBox(hWnd, TEXT("どのどうがかえらべ"), TEXT("むり"), MB_ICONERROR | MB_OK);
				EnableControls(hWnd);
				break;
			}
			if (!para.szOutput[0]) {
				::MessageBox(hWnd, TEXT("どこにだせばいいのかえらべ"), TEXT("むり"), MB_ICONERROR | MB_OK);
				EnableControls(hWnd);
				break;
			}
			if (::MessageBox(hWnd, TEXT("きりだしするます？"), TEXT("かくにん"), MB_ICONQUESTION | MB_YESNO) != IDYES) {
				EnableControls(hWnd);
				break;
			}
			hThread = reinterpret_cast<HANDLE>(::_beginthreadex(NULL, 0, CutThread, &para, 0, nullptr));
			if (!hThread) EnableControls(hWnd);
			break;

		case IDC_BUTTON_STOP:
			para.bStop = true;
			break;

		case IDC_BUTTON_EXIT:
			::SendMessage(hWnd, WM_CLOSE, 0, 0);
			break;
		}

	}

	void OnExit(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (lpEditInputProc == nullptr) {
			::SetWindowLongPtr(::GetDlgItem(hWnd, IDC_EDIT_INPUTFILE), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(lpEditInputProc));
		}
		if (lpEditOutputProc == nullptr) {
			::SetWindowLongPtr(::GetDlgItem(hWnd, IDC_EDIT_OUTPUTDIR), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(lpEditOutputProc));
		}
	}
}