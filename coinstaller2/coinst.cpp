#include "../coinstaller/coinst.cpp"


static INSTALLERINFO installerInfo = {
    /*.pApplicationId = */L"{D5D7E85E-5707-40b3-B4F2-0CAE8F16023B}",
    /*.pDisplayName = */L"Ceton Tuning Adapter Driver",
    /*.pProductName = */L"Ceton Tuning Adapter Driver",
    /*.pMfgName = */L"Ceton",
};

void  difLogCallback(
    DIFXAPI_LOG Event,
    DWORD Error,
    PCTSTR EventDescription,
    PVOID CallbackContext
)
{
    PTCHAR type = L"";
    switch (Event) {
        case DIFXAPI_SUCCESS:
            type = L"Success";
            break;
        case DIFXAPI_INFO:
            type = L"Info";
            break;
        case DIFXAPI_WARNING:
            type = L"Warning";
            break;
        case DIFXAPI_ERROR:
            type = L"Error";
            break;
    }
    DbgOut("%s %s (code %.08x)", type, EventDescription, Error);
}

void
install_trif_bulkusb(WCHAR * InPath) {
    PTCHAR DriverPackageInfPath;
    TCHAR DriverPackageInfPath_temp[1024];
    
    DWORD Flags = DRIVER_PACKAGE_FORCE; //DRIVER_PACKAGE_SILENT | DRIVER_PACKAGE_REPAIR
    DWORD ReturnCode = ERROR_SUCCESS;
    BOOL needReboot = FALSE;
    
    if (InPath) {
        StringCbPrintf(DriverPackageInfPath_temp, 1023, TEXT("%s\\ceton_trif_bulkusb.inf"), InPath);
        
    } else {
        if (sizeof(void*) != 4) {
            //our helper is 64 but our driver was installed to the x86 program files folder because of TAHSP being x86
            DriverPackageInfPath = TEXT("%ProgramFiles(x86)%\\Ceton Corp\\Ceton Tuning Adapter Host Driver\\ceton_trif_bulkusb.inf");
        } else {
            DriverPackageInfPath = TEXT("%ProgramFiles%\\Ceton Corp\\Ceton Tuning Adapter Host Driver\\ceton_trif_bulkusb.inf");
        }
    
        ExpandEnvironmentStrings(DriverPackageInfPath, DriverPackageInfPath_temp, 1023);
    }
    DriverPackageInfPath = DriverPackageInfPath_temp;
    
    
    
    if (!FileExists(DriverPackageInfPath)) {
        DbgOut("\"%s\" doesn't exist\n", DriverPackageInfPath);
        return;
    }
    
    SetDifxLogCallback(difLogCallback, NULL);

    ReturnCode = DriverPackagePreinstall( DriverPackageInfPath, Flags );
    DbgOut("DriverPackagePreinstall %#.08x!\n", ReturnCode);
    if (ReturnCode == ERROR_SUCCESS) {
        DbgOut("Pre-Installed\n");
    } else {
        DbgOut("Pre-Installing failed!\n");
    }
    
    
    ReturnCode = DriverPackageInstall( DriverPackageInfPath, Flags, &installerInfo, &needReboot );
    DbgOut("DriverPackageInstall %#.08x (reboot %d)!\n", ReturnCode, needReboot);
    if (ReturnCode == ERROR_SUCCESS) {
        DbgOut("Installed\n");
    } else {
        DbgOut("Installing failed!\n");
    }
}

void
uninstall_trif_bulkusb(WCHAR * InPath) {
    PTCHAR DriverPackageInfPath;
    TCHAR DriverPackageInfPath_temp[1024];
    
    DWORD Flags = DRIVER_PACKAGE_DELETE_FILES | DRIVER_PACKAGE_FORCE; //DRIVER_PACKAGE_SILENT
    DWORD ReturnCode = ERROR_SUCCESS;
    BOOL needReboot = FALSE;
    
    if (InPath) {
        StringCbPrintf(DriverPackageInfPath_temp, 1023, TEXT("%s\\ceton_trif_bulkusb.inf"), InPath);
        
    } else {
        if (sizeof(void*) != 4) {
            //our helper is 64 but our driver was installed to the x86 program files folder because of TAHSP being x86
            DriverPackageInfPath = TEXT("%ProgramFiles(x86)%\\Ceton Corp\\Ceton Tuning Adapter Host Driver\\ceton_trif_bulkusb.inf");
        } else {
            DriverPackageInfPath = TEXT("%ProgramFiles%\\Ceton Corp\\Ceton Tuning Adapter Host Driver\\ceton_trif_bulkusb.inf");
        }
    
        ExpandEnvironmentStrings(DriverPackageInfPath, DriverPackageInfPath_temp, 1023);
    }
    DriverPackageInfPath = DriverPackageInfPath_temp;
    
    SetDifxLogCallback(difLogCallback, NULL);
    
    ReturnCode = DriverPackageUninstall( DriverPackageInfPath, Flags, &installerInfo, &needReboot );
    DbgOut("DriverPackageInstall %#.08x (reboot %d)!\n", ReturnCode, needReboot);
    if (ReturnCode == ERROR_SUCCESS) {
        DbgOut("Uninstalled\n");
    } else {
        DbgOut("Uninstall failed!\n");
    }
    
    
    
    if (!FileExists(DriverPackageInfPath)) {
        DbgOut("\"%s\" doesn't exist\n", DriverPackageInfPath);
        return;
    }
}

int __cdecl	
wmain( int argc, WCHAR* argv[] )
{
    if (argc > 1) {
        if (wcscmp(argv[1], L"uninstall") == 0) {
            if (argc > 2) {
                uninstall_trif_bulkusb(argv[2]);
            } else {
                uninstall_trif_bulkusb(NULL);
            }
        } else {
            install_trif_bulkusb(argv[1]);
        }
    } else {
        install_trif_bulkusb(NULL);
    }
}