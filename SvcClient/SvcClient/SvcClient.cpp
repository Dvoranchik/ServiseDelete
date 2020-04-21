#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <stdio.h>
#include <conio.h>
#include <iostream>

VOID DisplayUsage(void);
VOID DoDeleteFile(LPTSTR filename);

LPCTSTR lpszPipeName = _T("\\\\.\\pipe\\MyPipeName");

TCHAR szCommand[10];
TCHAR szSvcName[80];
TCHAR szFileName[100];

void __cdecl _tmain(int argc, TCHAR* argv[])
{
	printf("\n");
	if (argc != 4)
	{
		printf("ERROR:\tIncorrect number of arguments\n\n");
		DisplayUsage();
		return;
	}

	StringCchCopy(szCommand, 10, argv[1]);
	StringCchCopy(szSvcName, 80, argv[2]);
	StringCchCopy(szFileName, 80, argv[3]);
	if (lstrcmpi(szCommand, TEXT("delete")) == 0)
		DoDeleteFile(szFileName);
	else
	{
		_tprintf(TEXT("Unknown command (%s)\n\n"), szCommand);
		DisplayUsage();
	}
}


VOID DisplayUsage()
{
	printf("Description:\n");
	printf("\tCommand-line tool that configures a service.\n\n");
	printf("Usage:\n");
	printf("\tsvcconfig [command] [service_name]\n\n");
	printf("\t[command]\n");
	printf("\t  query\n");
	printf("\t  describe\n");
	printf("\t  disable\n");
	printf("\t  enable\n");
	printf("\t  delete\n");
}




VOID DoDeleteFile(LPTSTR filename)
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	HANDLE hPipe;
	SERVICE_STATUS ssStatus;
	BOOL fSuccess = TRUE;
	TCHAR chReadBuf[1024]{ 0 };
	DWORD cbRead;

	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager)
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	schService = OpenService(
		schSCManager,       // SCM database 
		szSvcName,          // name of service 
		DELETE);            // need delete access 

	if (schService == NULL)
	{
		printf("OpenService failed (%d)\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return;
	}
	fSuccess = CallNamedPipe(
		lpszPipeName,
		szFileName,           // message to server 
		(lstrlen(szFileName) + 1) * sizeof(TCHAR), // message length 
		chReadBuf,              // buffer to receive reply 
		1024 * sizeof(TCHAR),  // size of read buffer 
		&cbRead,                // number of bytes read 
		20000);                 // waits for 20 seconds 
	if (fSuccess || GetLastError() == ERROR_MORE_DATA)
	{

		printf("%s\n", chReadBuf);

		// The pipe is closed; no more data can be read. 

		if (!fSuccess)
		{
			printf("\nExtra data in message was lost\n");
		}
	}

}