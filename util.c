#include "thd3d8mitigation.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int myvasprintf(char** strp, const char* fmt, va_list ap)
{
	va_list ap2;
	int len;

	va_copy(ap2, ap);
	len = vsnprintf(NULL, 0, fmt, ap2);
	va_end(ap2);
	if (len < 0)
		return -1;

	if ((*strp = malloc(len + 1)) == NULL)
		return -1;

	if ((len = vsnprintf(*strp, len + 1, fmt, ap)) < 0)
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

int AllocateErrorMessageA(DWORD code, char** strp)
{
	char* errmsg;
	int len;

	// XXX TODO review
	if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errmsg, 0, NULL) == 0)
		return -1;
	len = myasprintf(strp, "%s", errmsg);
	LocalFree(errmsg);
	return len;
}

void ThfVLog(const char* fmt, va_list ap)
{
	char* origmsg = NULL, * msg = NULL;

	if (myvasprintf(&origmsg, fmt, ap) < 0)
	{
		OutputDebugStringA(THF_LOG_PREFIX __FUNCTION__ ": myvasprintf failed.\n");
		return;
	}

	if (myasprintf(&msg, "%s%s\n", THF_LOG_PREFIX, origmsg) < 0)
	{
		OutputDebugStringA(THF_LOG_PREFIX __FUNCTION__ ": myasprintf failed.\n");
		goto cleanup;
	}

	OutputDebugStringA(msg);

cleanup:
	free(msg);
	free(origmsg);
}

void ThfLog(const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ThfVLog(fmt, ap);
	va_end(ap);
}

void ThfVError(DWORD err, const char* fmt, va_list ap)
{
	char* origmsg = NULL, * msg = NULL, * errmsg = NULL;

	if (AllocateErrorMessageA(err, &errmsg) < 0)
	{
		OutputDebugStringA(THF_LOG_PREFIX __FUNCTION__ ": AllocateErrorMessage failed.\n");
		return;
	}

	if (myvasprintf(&origmsg, fmt, ap) < 0)
	{
		OutputDebugStringA(THF_LOG_PREFIX __FUNCTION__ ": myvasprintf failed.\n");
		goto cleanup;
	}

	if (myasprintf(&msg, "%s%s: %s\n", THF_LOG_PREFIX, origmsg, errmsg) < 0)
	{
		OutputDebugStringA(THF_LOG_PREFIX __FUNCTION__ ": myasprintf failed.\n");
		goto cleanup;
	}

	OutputDebugStringA(msg);

cleanup:
	free(msg);
	free(origmsg);
	free(errmsg);
}

void ThfError(DWORD err, const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ThfVError(err, fmt, ap);
	va_end(ap);
}
