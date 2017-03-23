/* ----------------------------------------------------------------------------
 * nmakehlp.c --
 *
 *	This is used to fix limitations within nmake and the environment.
 *
 * Copyright (c) 2002 by David Gravereaux.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 * ----------------------------------------------------------------------------
 */
#include <windows.h>
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "kernel32.lib")
#include <stdio.h>
#include <math.h>

/* protos */
int CheckForCompilerFeature (const char *option);
int CheckForLinkerFeature (const char *option);
int IsIn (const char *string, const char *substring);
int GrepForDefine (const char *file, const char *string);
DWORD WINAPI ReadFromPipe (LPVOID args);

/* globals */
#define CHUNK	25
#define STATICBUFFERSIZE    1000
typedef struct {
    HANDLE pipe;
    char buffer[STATICBUFFERSIZE];
} pipeinfo;

pipeinfo Out = {INVALID_HANDLE_VALUE, '\0'};
pipeinfo Err = {INVALID_HANDLE_VALUE, '\0'};



/* exitcodes: 0 == no, 1 == yes, 2 == error */
int
main (int argc, char *argv[])
{
    char msg[300];
    DWORD dwWritten;
    int chars;

    /* make sure children (cl.exe and link.exe) are kept quiet. */
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    /* Make sure the compiler and linker aren't effected by the outside world. */
    SetEnvironmentVariable("CL", "");
    SetEnvironmentVariable("LINK", "");

    if (argc > 1 && *argv[1] == '-') {
	switch (*(argv[1]+1)) {
	case 'c':
	    if (argc != 3) {
		chars = wsprintf(msg, "usage: %s -c <compiler option>\n"
			"Tests for whether cl.exe supports an option\n"
			"exitcodes: 0 == no, 1 == yes, 2 == error\n", argv[0]);
		WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, chars, &dwWritten, NULL);
		return 2;
	    }
	    return CheckForCompilerFeature(argv[2]);
	case 'l':
	    if (argc != 3) {
		chars = wsprintf(msg, "usage: %s -l <linker option>\n"
			"Tests for whether link.exe supports an option\n"
			"exitcodes: 0 == no, 1 == yes, 2 == error\n", argv[0]);
		WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, chars, &dwWritten, NULL);
		return 2;
	    }
	    return CheckForLinkerFeature(argv[2]);
	case 'f':
	    if (argc == 2) {
		chars = wsprintf(msg, "usage: %s -f <string> <substring>\n"
		    "Find a substring within another\n"
		    "exitcodes: 0 == no, 1 == yes, 2 == error\n", argv[0]);
		WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, chars, &dwWritten, NULL);
		return 2;
	    } else if (argc == 3) {
		/* if the string is blank, there is no match */
		return 0;
	    } else {
		return IsIn(argv[2], argv[3]);
	    }
	case 'g':
	    if (argc == 2) {
		chars = wsprintf(msg, "usage: %s -g <file> <string>\n"
		    "grep for a #define\n"
		    "exitcodes: integer of the found string (no decimals)\n", argv[0]);
		WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, chars, &dwWritten, NULL);
		return 2;
	    }
	    return GrepForDefine(argv[2], argv[3]);
	}
    }
    chars = wsprintf(msg, "usage: %s -c|-l|-f ...\n"
	    "This is a little helper app to equalize shell differences between WinNT and\n"
	    "Win9x and get nmake.exe to accomplish its job.\n",
	    argv[0]);
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, chars, &dwWritten, NULL);
    return 2;
}

int
CheckForCompilerFeature (const char *option)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa;
    DWORD threadID;
    char msg[300];
    BOOL ok;
    HANDLE hProcess, h, pipeThreads[2];
    char cmdline[100];

    hProcess = GetCurrentProcess();

    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags   = STARTF_USESTDHANDLES;
    si.hStdInput = INVALID_HANDLE_VALUE;

    ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = FALSE;

    /* create a non-inheritible pipe. */
    CreatePipe(&Out.pipe, &h, &sa, 0);

    /* dupe the write side, make it inheritible, and close the original. */
    DuplicateHandle(hProcess, h, hProcess, &si.hStdOutput, 
	    0, TRUE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);

    /* Same as above, but for the error side. */
    CreatePipe(&Err.pipe, &h, &sa, 0);
    DuplicateHandle(hProcess, h, hProcess, &si.hStdError, 
	    0, TRUE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);

    /* base command line */
    strcpy(cmdline, "cl.exe -nologo -c -TC -Zs -X ");
    /* append our option for testing */
    strcat(cmdline, option);
    /* filename to compile, which exists, but is nothing and empty. */
    strcat(cmdline, " .\\nul");

    ok = CreateProcess(
	    NULL,	    /* Module name. */
	    cmdline,	    /* Command line. */
	    NULL,	    /* Process handle not inheritable. */
	    NULL,	    /* Thread handle not inheritable. */
	    TRUE,	    /* yes, inherit handles. */
	    DETACHED_PROCESS, /* No console for you. */
	    NULL,	    /* Use parent's environment block. */
	    NULL,	    /* Use parent's starting directory. */
	    &si,	    /* Pointer to STARTUPINFO structure. */
	    &pi);	    /* Pointer to PROCESS_INFORMATION structure. */

    if (!ok) {
	DWORD err = GetLastError();
	int chars = wsprintf(msg, "Tried to launch: \"%s\", but got error [%u]: ", cmdline, err);

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_MAX_WIDTH_MASK, 0L, err, 0, (LPVOID) &msg[chars],
		(300-chars), 0);
	WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, strlen(msg), &err, NULL);
	return 2;
    }

    /* close our references to the write handles that have now been inherited. */
    CloseHandle(si.hStdOutput);
    CloseHandle(si.hStdError);

    WaitForInputIdle(pi.hProcess, 5000);
    CloseHandle(pi.hThread);

    /* start the pipe reader threads. */
    pipeThreads[0] = CreateThread(NULL, 0, ReadFromPipe, &Out, 0, &threadID);
    pipeThreads[1] = CreateThread(NULL, 0, ReadFromPipe, &Err, 0, &threadID);

    /* block waiting for the process to end. */
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);

    /* wait for our pipe to get done reading, should it be a little slow. */
    WaitForMultipleObjects(2, pipeThreads, TRUE, 500);
    CloseHandle(pipeThreads[0]);
    CloseHandle(pipeThreads[1]);

    /* look for the commandline warning code in both streams. */
    return !(strstr(Out.buffer, "D4002") != NULL || strstr(Err.buffer, "D4002") != NULL);
}

int
CheckForLinkerFeature (const char *option)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa;
    DWORD threadID;
    char msg[300];
    BOOL ok;
    HANDLE hProcess, h, pipeThreads[2];
    char cmdline[100];

    hProcess = GetCurrentProcess();

    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags   = STARTF_USESTDHANDLES;
    si.hStdInput = INVALID_HANDLE_VALUE;

    ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    /* create a non-inheritible pipe. */
    CreatePipe(&Out.pipe, &h, &sa, 0);

    /* dupe the write side, make it inheritible, and close the original. */
    DuplicateHandle(hProcess, h, hProcess, &si.hStdOutput, 
	    0, TRUE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);

    /* Same as above, but for the error side. */
    CreatePipe(&Err.pipe, &h, &sa, 0);
    DuplicateHandle(hProcess, h, hProcess, &si.hStdError, 
	    0, TRUE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);

    /* base command line */
    strcpy(cmdline, "link.exe -nologo ");
    /* append our option for testing */
    strcat(cmdline, option);

    ok = CreateProcess(
	    NULL,	    /* Module name. */
	    cmdline,	    /* Command line. */
	    NULL,	    /* Process handle not inheritable. */
	    NULL,	    /* Thread handle not inheritable. */
	    TRUE,	    /* yes, inherit handles. */
	    DETACHED_PROCESS, /* No console for you. */
	    NULL,	    /* Use parent's environment block. */
	    NULL,	    /* Use parent's starting directory. */
	    &si,	    /* Pointer to STARTUPINFO structure. */
	    &pi);	    /* Pointer to PROCESS_INFORMATION structure. */

    if (!ok) {
	DWORD err = GetLastError();
	int chars = wsprintf(msg, "Tried to launch: \"%s\", but got error [%u]: ", cmdline, err);

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_MAX_WIDTH_MASK, 0L, err, 0, (LPVOID) &msg[chars],
		(300-chars), 0);
	WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, strlen(msg), &err, NULL);
	return 2;
    }

    /* close our references to the write handles that have now been inherited. */
    CloseHandle(si.hStdOutput);
    CloseHandle(si.hStdError);

    WaitForInputIdle(pi.hProcess, 5000);
    CloseHandle(pi.hThread);

    /* start the pipe reader threads. */
    pipeThreads[0] = CreateThread(NULL, 0, ReadFromPipe, &Out, 0, &threadID);
    pipeThreads[1] = CreateThread(NULL, 0, ReadFromPipe, &Err, 0, &threadID);

    /* block waiting for the process to end. */
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);

    /* wait for our pipe to get done reading, should it be a little slow. */
    WaitForMultipleObjects(2, pipeThreads, TRUE, 500);
    CloseHandle(pipeThreads[0]);
    CloseHandle(pipeThreads[1]);

    /* look for the commandline warning code in the stderr stream. */
    return !(strstr(Out.buffer, "LNK1117") != NULL || strstr(Err.buffer, "LNK1117") != NULL);
}

DWORD WINAPI
ReadFromPipe (LPVOID args)
{
    pipeinfo *pi = (pipeinfo *) args;
    char *lastBuf = pi->buffer;
    DWORD dwRead;
    BOOL ok;

again:
    if (lastBuf - pi->buffer + CHUNK > STATICBUFFERSIZE) {
	CloseHandle(pi->pipe);
	return -1;
    }
    ok = ReadFile(pi->pipe, lastBuf, CHUNK, &dwRead, 0L);
    if (!ok || dwRead == 0) {
	CloseHandle(pi->pipe);
	return 0;
    }
    lastBuf += dwRead;
    goto again;

    return 0;  /* makes the compiler happy */
}

int
IsIn (const char *string, const char *substring)
{
    return (strstr(string, substring) != NULL);
}

/*
 *  Find a specified #define by name.
 *
 *  If the line is '#define TCL_VERSION "8.5"', it returns
 *  85 as the result.
 */

int
GrepForDefine (const char *file, const char *string)
{
    FILE *f;
    char s1[51], s2[51], s3[51];
    int r = 0;
    double d1;

    f = fopen(file, "rt");
    if (f == NULL) {
	return 0;
    }

    do {
	r = fscanf(f, "%50s", s1);
	if (r == 1 && !strcmp(s1, "#define")) {
	    /* get next two words */
	    r = fscanf(f, "%50s %50s", s2, s3);
	    if (r != 2) continue;
	    /* is the first word what we're looking for? */
	    if (!strcmp(s2, string)) {
		fclose(f);
		/* add 1 past first double quote char. "8.5" */
		d1 = atof(s3 + 1);		  /*    8.5  */
		while (floor(d1) != d1) {
		    d1 *= 10.0;
		}
		return ((int) d1);		  /*    85   */
	    }
	}
    } while (!feof(f));

    fclose(f);
    return 0;
}
