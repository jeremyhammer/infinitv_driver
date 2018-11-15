#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include <strsafe.h>
#include <wchar.h>
//#include <ResApi.h>

//#if DBG
//#define DbgOut(Text) OutputDebugString(TEXT(Text))
#define DbgOut(fmt, ...) { TCHAR __temp_buffer[1024]; __temp_buffer[1023] = 0; StringCbPrintf(__temp_buffer, 1023, TEXT("Ceton MOCUR Coinstaller: ") TEXT( fmt), __VA_ARGS__); OutputDebugString(__temp_buffer); }
//#else
//#define DbgOut(Text)
//#endif

#include <difxapi.h>

int
FileExists(const TCHAR *fileName)
{
    DWORD       fileAttr;

    fileAttr = GetFileAttributes(fileName);
    if (0xFFFFFFFF == fileAttr)
        return 0;
    return 1;
}

static int
FinishInstallActionsNeeded()
{
    return 1;
}

static void
thasp_install()
{
    TCHAR SetupPackagePath_temp[1024];
    PTCHAR SetupPackagePath = TEXT("%ProgramFiles%\\Ceton Corp\\Setup\\setup.exe");
    ExpandEnvironmentStrings(SetupPackagePath, SetupPackagePath_temp, 1023);
    
    DbgOut("thasp_install\n");
    
    SetupPackagePath = SetupPackagePath_temp;
    
    if (!FileExists(SetupPackagePath)) {
        DbgOut("\"%s\" doesn't exist\n", SetupPackagePath);
        return;
    }
    
    {
        STARTUPINFO StartupInfo;
        PROCESS_INFORMATION ProcessInformation;
        TCHAR args[1024];
        
        ZeroMemory( &StartupInfo, sizeof(StartupInfo) );
        StartupInfo.cb = sizeof(StartupInfo);
        ZeroMemory( &ProcessInformation, sizeof(ProcessInformation) );
        ZeroMemory( args, 1024 );
        
        //StringCbCat( args, 1024, SetupPackagePath );
        StringCbCat( args, 1024, TEXT(" /passive") );
        
        DbgOut("CreateProcess \"%s\"\n", args);
        
        CreateProcess(
            SetupPackagePath,
            args,
            NULL,
            NULL,
            0,
            0,
            NULL,
            NULL,
            &StartupInfo,
            &ProcessInformation);
            
        // Wait until child process exits.
        WaitForSingleObject( ProcessInformation.hProcess, INFINITE );
    
        // Close process and thread handles. 
        CloseHandle( ProcessInformation.hProcess );
        CloseHandle( ProcessInformation.hThread );
    }
}

DWORD CALLBACK
CoInstallerEntry(
        __in    DI_FUNCTION InstallFunction,
        __in    HDEVINFO    DeviceInfoSet,
        __in    PSP_DEVINFO_DATA    DeviceInfoData,
        __inout PCOINSTALLER_CONTEXT_DATA   Context
        )
{
    DbgOut("Enter CoInstallerEntry\n");
    
    //MessageBox(NULL, TEXT( "Calling all cars" ), TEXT( "Code ran" ), MB_OK );
    
    switch( InstallFunction ) {
        case DIF_INSTALLDEVICE:
            DbgOut("DIF_INSTALLDEVICE\n");
            break;
        case DIF_REMOVE:
            DbgOut("DIF_REMOVE\n");
            break;
        case DIF_FINISHINSTALL_ACTION:
            DbgOut("DIF_FINISHINSTALL_ACTION\n");
            thasp_install();
            break;
        case DIF_NEWDEVICEWIZARD_FINISHINSTALL:
        {
            SP_DEVINSTALL_PARAMS DeviceInstallParams;
            
            DbgOut("DIF_NEWDEVICEWIZARD_FINISHINSTALL\n");
            if (FinishInstallActionsNeeded()) {
                // Obtain the device install parameters for the device
                // and set the DI_FLAGSEX_FINISHINSTALL_ACTION flag
                DeviceInstallParams.cbSize = sizeof(DeviceInstallParams);
                if (SetupDiGetDeviceInstallParams(
                    DeviceInfoSet,
                    DeviceInfoData,
                    &DeviceInstallParams)) 
                {
                    DeviceInstallParams.FlagsEx |= DI_FLAGSEX_FINISHINSTALL_ACTION;
                    
                    SetupDiSetDeviceInstallParams(
                        DeviceInfoSet,
                        DeviceInfoData,
                        &DeviceInstallParams);
                }
            }
            break;
        }
        default:
            DbgOut("InstallFunction %d\n", InstallFunction);
            break;
    }

    return NO_ERROR;
}
