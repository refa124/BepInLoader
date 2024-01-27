#include <Windows.h>
#include <iostream>
#include <direct.h>
#include <sstream>

#include "BepInLoader.h"

#include "Detours/detours.h"
#pragma comment(lib, "Detours/detours.lib")

static bool Initialized = false;

inline static int(*il2cpp_init)(const char*) = nullptr;

int il2cpp_init_detour(const char *domain_name)
{
    int orig_result = il2cpp_init(domain_name);

    if(!Initialized)
    {
        std::stringstream DOORSTOP_MANAGED_FOLDER_DIR;
        DOORSTOP_MANAGED_FOLDER_DIR << _getcwd(0, 0) << "\\" << BepInEx::Dotnet_Directory;

        SetEnvironmentVariableA("DOORSTOP_MANAGED_FOLDER_DIR", DOORSTOP_MANAGED_FOLDER_DIR.str().c_str());
        
        std::stringstream DOORSTOP_INVOKE_DLL_PATH;
        DOORSTOP_INVOKE_DLL_PATH << _getcwd(0, 0) << "\\" << BepInEx::BepInEx_Directory << "\\core\\BepInEx.Unity.IL2CPP.dll";
        
        SetEnvironmentVariableA("DOORSTOP_INVOKE_DLL_PATH", DOORSTOP_INVOKE_DLL_PATH.str().c_str());

        std::stringstream CORECLR_PATH;
        CORECLR_PATH << _getcwd(0, 0) << "\\" << BepInEx::Dotnet_Directory << "\\coreclr.dll";

        HMODULE hCoreClr = LoadLibraryA(CORECLR_PATH.str().c_str());

        if(!hCoreClr) return orig_result;

        auto coreclr_initialize = reinterpret_cast<int(*)(const char*, const char*, int, const char**, const char**, void**, unsigned int*)>(GetProcAddress(hCoreClr, "coreclr_initialize"));
        auto coreclr_create_delegate = reinterpret_cast<int(*)(void*, unsigned int, const char*, const char*, const char*, void**)>(GetProcAddress(hCoreClr, "coreclr_create_delegate"));

        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);

        SetEnvironmentVariableA("DOORSTOP_PROCESS_PATH", exePath);

        const char *property_keys[] = {
            "APP_PATHS"
        };

        std::stringstream paths;
        paths << _getcwd(0, 0) << "\\" << BepInEx::Dotnet_Directory;
        paths << ";";
        paths << _getcwd(0, 0) << "\\" << BepInEx::BepInEx_Directory << "\\core";

        SetEnvironmentVariableA("DOORSTOP_DLL_SEARCH_DIRS", paths.str().c_str());
        
        const char *property_values[] = {
            paths.str().c_str()
        };

        void* coreclr_handle = 0;
        unsigned int domain_id = 0;

        int ret = coreclr_initialize(exePath, "BepInExHost", sizeof(property_values) / sizeof(char*), property_keys, property_values, &coreclr_handle, &domain_id);

        if(ret < 0)
        {
            MessageBoxA(NULL, "Error while coreclr_initialize :(", "BepInLoader", MB_ICONERROR);

            return orig_result;
        }

        void (*startup)() = NULL;

        ret = coreclr_create_delegate(coreclr_handle, domain_id, "BepInEx.Unity.IL2CPP", "Doorstop.Entrypoint", "Start", (void**)&startup);

        if(ret < 0)
        {
            MessageBoxA(NULL, "Error while coreclr_create_delegate :(", "BepInLoader", MB_ICONERROR);

            return orig_result;
        }

        startup();
    }
    
    return orig_result;
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    HMODULE hUnityPlayer = LoadLibraryA("UnityPlayer.dll");

    if(!hUnityPlayer)
    {
        MessageBoxA(NULL, "UnityPlayer module not found!", "BepInLoader", MB_ICONERROR);
        return 0;
    }

    HMODULE hGameAssembly = LoadLibraryA("GameAssembly.dll");

    if(!hGameAssembly)
    {
        MessageBoxA(NULL, "GameAssembly module not found!", "BepInLoader", MB_ICONERROR);
        return 0;
    }

    il2cpp_init = reinterpret_cast<decltype(il2cpp_init)>(GetProcAddress(hGameAssembly, "il2cpp_init"));

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(LPVOID&)il2cpp_init, (PBYTE)il2cpp_init_detour);
    DetourTransactionCommit();
    
    return reinterpret_cast<int(*)(HINSTANCE, HINSTANCE, PSTR, int)>(GetProcAddress(hUnityPlayer, "UnityMain"))(hInst, hInstPrev, cmdline, cmdshow);
}
