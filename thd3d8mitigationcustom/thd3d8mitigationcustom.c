#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "resource.h"

int myvasprintf(char** strp, const char* fmt, va_list ap)
{
	va_list ap2;
	int len;
	_locale_t loc;

	loc = _get_current_locale();

	va_copy(ap2, ap);
	len = _vscprintf_l(fmt, loc, ap);
	va_end(ap2);

	if (len < 0)
		return -1;

	if ((*strp = malloc(len + 1)) == NULL)
		return -1;

	if ((len = _vsprintf_s_l(*strp, len + 1, fmt, loc, ap)) < 0)
	{
		free(*strp);
		return -1;
	}

	return len;
}

int myasprintf(char** strp, const char* fmt, ...)
{
	va_list ap;
	int len;

	va_start(ap, fmt);
	len = myvasprintf(strp, fmt, ap);
	va_end(ap);
	return len;
}

int AllocateDataDirPath(char** strp)
{
	char exepath[MAX_PATH];
	char exedrivepath[_MAX_DRIVE];
	char exedirpath[_MAX_DIR];
	DWORD len;

	if ((len = GetModuleFileNameA(NULL, exepath, sizeof(exepath))) == 0)
		return -1;

	if (len >= sizeof(exepath))
		return -1;

	if (_splitpath_s(exepath, exedrivepath, sizeof(exedrivepath), exedirpath, sizeof(exedirpath), NULL, 0, NULL, 0) !=
		0)
		return -1;

	return myasprintf(strp, "%s%s", exedrivepath, exedirpath);
}

int AllocateDataFilePath(char** strp, const char* filename)
{
	char* datadirpath;
	int ret;

	if (AllocateDataDirPath(&datadirpath) < 0)
		return -1;

	ret = myasprintf(strp, "%s%s", datadirpath, filename);
	free(datadirpath);
	return ret;
}

BOOL ExistsFile(char* path)
{
	return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

const char* section_present = "presentation";
const char* key_wait_for = "wait_for";
const char* value_wait_for_default = "auto";
const int radioid_wait_for_default = IDC_RADIO_WAIT_FOR_AUTO;
const char* cfgfilename = "thd3d8mitigationcfg.ini";
const WCHAR* title = L"thd3d8mitigation config";

BOOL LoadConfig(HWND hwndDlg)
{
	char* path = NULL;
	char buf[32];
	BOOL ret = FALSE;

	if (AllocateDataFilePath(&path, cfgfilename) < 0)
		goto cleanup;

	if (!ExistsFile(path))
		if (WritePrivateProfileStringA(section_present, key_wait_for, value_wait_for_default, path) == 0)
			goto cleanup;

	if (GetPrivateProfileStringA(section_present, key_wait_for, value_wait_for_default, buf, sizeof(buf), path) >=
		sizeof(buf) - 1)
		SendMessageW(GetDlgItem(hwndDlg, radioid_wait_for_default), BM_SETCHECK, BST_CHECKED, 0);
	else if (strcmp(buf, "auto") == 0)
		SendMessageW(GetDlgItem(hwndDlg, IDC_RADIO_WAIT_FOR_AUTO), BM_SETCHECK, BST_CHECKED, 0);
	else if (strcmp(buf, "vsync") == 0)
		SendMessageW(GetDlgItem(hwndDlg, IDC_RADIO_WAIT_FOR_VSYNC), BM_SETCHECK, BST_CHECKED, 0);
	else if (strcmp(buf, "normal") == 0)
		SendMessageW(GetDlgItem(hwndDlg, IDC_RADIO_WAIT_FOR_NORMAL), BM_SETCHECK, BST_CHECKED, 0);
	else
		SendMessageW(GetDlgItem(hwndDlg, radioid_wait_for_default), BM_SETCHECK, BST_CHECKED, 0);

	ret = TRUE;
cleanup:
	free(path);
	return ret;
}

BOOL StoreConfig(HWND hwndDlg)
{
	const char* value_wait_for = value_wait_for_default;
	char* path = NULL;
	BOOL ret = FALSE;

	if (AllocateDataFilePath(&path, cfgfilename) < 0)
		goto cleanup;

	if (SendMessageW(GetDlgItem(hwndDlg, IDC_RADIO_WAIT_FOR_AUTO), BM_GETCHECK, 0, 0) == BST_CHECKED)
		value_wait_for = "auto";
	else if (SendMessageW(GetDlgItem(hwndDlg, IDC_RADIO_WAIT_FOR_VSYNC), BM_GETCHECK, 0, 0) == BST_CHECKED)
		value_wait_for = "vsync";
	else if (SendMessageW(GetDlgItem(hwndDlg, IDC_RADIO_WAIT_FOR_NORMAL), BM_GETCHECK, 0, 0) == BST_CHECKED)
		value_wait_for = "normal";

	if (WritePrivateProfileStringA(section_present, key_wait_for, value_wait_for, path) == 0)
		goto cleanup;

	ret = TRUE;
cleanup:
	free(path);
	return ret;
}

/* EndDialogの第二引数で1を返したら成功、2を返したら失敗。0や-1を使わないのは、DialogBoxParamW元来の返り値との重複を避けるため。 */
BOOL CALLBACK DlgProc(HWND hwndDlg,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		if (!LoadConfig(hwndDlg))
		{
			MessageBoxW(hwndDlg, L"設定ファイルを作成もしくは開くことができませんでした。", title, MB_OK | MB_ICONERROR);
			EndDialog(hwndDlg, 2);
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			if (!StoreConfig(hwndDlg))
			{
				MessageBoxW(hwndDlg, L"設定ファイルを保存できませんでした。", title, MB_OK | MB_ICONERROR);
				EndDialog(hwndDlg, 2);
			}
			else
			{
				MessageBoxW(hwndDlg, L"設定を書き換えました。", title, MB_OK);
				EndDialog(hwndDlg, 1);
			}
			return TRUE;
		case IDCANCEL:
			EndDialog(hwndDlg, 1);
			return TRUE;
		}
	}

	return FALSE;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow)
{
	return DialogBoxW(hInstance, L"IDD_THD3D8MITIGATIONCUSTOM_DIALOG", NULL, DlgProc) == 1 ? 0 : 1;
}
