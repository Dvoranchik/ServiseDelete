#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <conio.h>
#include <iostream>
#include <strsafe.h>
#include <sys/stat.h>

#define SERVICE_NAME TEXT("SvcName")
#define PIPE_NAME TEXT("\\\\.\\pipe\\MyPipeName")

SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = NULL;
BOOL fConnected;
DWORD dwThreadId;

VOID SvcInstall(void);
VOID WINAPI SvcCtrlHandler(DWORD);
VOID WINAPI SvcMain(DWORD, LPTSTR*);

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPTSTR*);
VOID SvcReportEvent(LPTSTR);


DWORD WINAPI ProcessingMessageThread(LPVOID lpvParam);

void __cdecl _tmain(int argc, TCHAR* argv[])
{
	if (lstrcmpi(argv[1], TEXT("install")) == 0)
	{
		SvcInstall();
		return;
	}

	SERVICE_TABLE_ENTRY DispatchTable[] =
	{
		{ (LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
		{ NULL, NULL }
	};

	if (!StartServiceCtrlDispatcher(DispatchTable))
	{
		SvcReportEvent((LPWSTR)TEXT("StartServiceCtrlDispatcher"));
	}
}

VOID SvcInstall()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	TCHAR szPath[MAX_PATH];

	if (!GetModuleFileName(NULL, szPath, MAX_PATH))
	{
		printf("Cannot install service (%d)\n", GetLastError());
		return;
	}

	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	schService = CreateService(
		schSCManager,              // SCM database 
		SERVICE_NAME,                   // name of service 
		SERVICE_NAME,                   // service name to display 
		SERVICE_ALL_ACCESS,        // desired access 
		SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS, // service type 
		SERVICE_DEMAND_START,      // start type 
		SERVICE_ERROR_NORMAL,      // error control type 
		szPath,                    // path to service's binary 
		NULL,                      // no load ordering group 
		NULL,                      // no tag identifier 
		NULL,                      // no dependencies 
		NULL,                      // LocalSystem account 
		NULL);                     // no password 

	if (schService == NULL)
	{
		printf("CreateService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}
	else printf("Service installed successfully\n");

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}

VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
	HANDLE hThread;

	gSvcStatusHandle = RegisterServiceCtrlHandler(
		SERVICE_NAME,
		SvcCtrlHandler);

	if (!gSvcStatusHandle)
	{
		SvcReportEvent((LPTSTR)TEXT("RegisterServiceCtrlHandler"));
		return;
	}

	gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	gSvcStatus.dwServiceSpecificExitCode = 0;

	ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

	hThread = CreateThread(
		NULL,              // no security attribute 
		0,                 // default stack size 
		ProcessingMessageThread,    // thread proc
		NULL,    // thread parameter 
		0,                 // not suspended 
		&dwThreadId);      // returns thread ID 

	if (hThread == NULL)
	{
		printf("CreateThread failed");
		return;
	}

	SvcInit(dwArgc, lpszArgv);
}

VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv)
{
	ghSvcStopEvent = CreateEvent(
		NULL,    // default security attributes
		TRUE,    // manual reset event
		FALSE,   // not signaled
		NULL);   // no name

	if (ghSvcStopEvent == NULL)
	{
		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		return;
	}

	ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

	WaitForSingleObject(ghSvcStopEvent, INFINITE);
	ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
}


VOID ReportSvcStatus(DWORD dwCurrentState,
	DWORD dwWin32ExitCode,
	DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;

	gSvcStatus.dwCurrentState = dwCurrentState;
	gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
	gSvcStatus.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		gSvcStatus.dwControlsAccepted = 0;
	else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if ((dwCurrentState == SERVICE_RUNNING) ||
		(dwCurrentState == SERVICE_STOPPED))
		gSvcStatus.dwCheckPoint = 0;
	else gSvcStatus.dwCheckPoint = dwCheckPoint++;

	SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
	switch (dwCtrl)
	{
	case SERVICE_CONTROL_STOP:
		ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

		SetEvent(ghSvcStopEvent);

		return;

	case SERVICE_CONTROL_INTERROGATE:
		break;

	default:
		break;
	}

	ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
}

VOID SvcReportEvent(LPTSTR szFunction)
{
	HANDLE hEventSource;
	LPCTSTR lpszStrings[2];
	TCHAR Buffer[80];

	hEventSource = RegisterEventSource(NULL, SERVICE_NAME);

	if (NULL != hEventSource)
	{
		StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

		lpszStrings[0] = SERVICE_NAME;
		lpszStrings[1] = Buffer;

		ReportEvent(hEventSource,        // event log handle
			EVENTLOG_ERROR_TYPE, // event type
			0,                   // event category
			0,           // event identifier
			NULL,                // no security identifier
			2,                   // size of lpszStrings array
			0,                   // no binary data
			lpszStrings,         // array of strings
			NULL);               // no binary data

		DeregisterEventSource(hEventSource);
	}
}

DWORD WINAPI ProcessingMessageThread(LPVOID lpvParam)
{
	SECURITY_ATTRIBUTES saPipe;

	HANDLE hPipe;
	HANDLE hThread;

	saPipe.lpSecurityDescriptor = (PSECURITY_DESCRIPTOR)malloc(SECURITY_DESCRIPTOR_MIN_LENGTH);
	InitializeSecurityDescriptor(saPipe.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(saPipe.lpSecurityDescriptor, TRUE, (PACL)NULL, NULL);
	saPipe.nLength = sizeof(saPipe);
	saPipe.bInheritHandle = TRUE;


	HANDLE hNamedPipe;
	BOOL   fConnected;
	TCHAR   szBuf[512]{ 0 };
	DWORD  cbWritten;
	HANDLE    hToken;
	DWORD  cbRead;

	ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);
	fConnected = false;
	hNamedPipe = CreateNamedPipe(
		PIPE_NAME,
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
		PIPE_UNLIMITED_INSTANCES,
		512, 512, NMPWAIT_USE_DEFAULT_WAIT, NULL);

	if (hNamedPipe == INVALID_HANDLE_VALUE)
	{
		fprintf(stdout, "CreateNamedPipe: Error %ld\n",
			GetLastError());
		return 0;
	}

	while (true)
	{
		fConnected = ConnectNamedPipe(hNamedPipe, NULL);
		if (ReadFile(hNamedPipe, szBuf, 512, &cbRead, NULL))
		{
			bool del;
			del = DeleteFile(szBuf);
			if (!del)
			{
				fprintf(stdout, "DeleteFile: %ld\n",
					GetLastError());
				if (!WriteFile(hNamedPipe, "Error of delete file\n", strlen("Error of delete file\n") + 1,
					&cbWritten, NULL))
					fprintf(stdout, "DeleteFile: Error %ld\n",
						GetLastError());
			}
			else {
				if (!WriteFile(hNamedPipe, "File was deleted sucessfully\n", strlen("File was deleted sucessfully\n") + 1,
					&cbWritten, NULL))
					fprintf(stdout, "DeleteFile: Error %ld\n",
						GetLastError());
			}
			DisconnectNamedPipe(hNamedPipe);
		}
	}
	CloseHandle(hNamedPipe);
}

