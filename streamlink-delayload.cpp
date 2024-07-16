#include "utils.hpp"

#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <delayimp.h>

FARPROC WINAPI obs_streamlink_delay_load(unsigned dliNotify, PDelayLoadInfo pdli);
ExternC const PfnDliHook __pfnDliNotifyHook2 = obs_streamlink_delay_load;

FARPROC WINAPI obs_streamlink_delay_load(unsigned dliNotify, PDelayLoadInfo pdli)
{
	FF_LOG(LOG_WARNING, "delay_load: [%u, %s]", dliNotify, pdli->szDll);

    if (dliNotify == dliNotePreLoadLibrary)
    {
#if  0
        auto pathFFmpeg = obs_streamlink_data_path / "ffmpeg" / pdli->szDll;
        auto pathPython = obs_streamlink_data_path / obs_streamlink_python_ver / pdli->szDll;
        if (exists(pathFFmpeg))
        {
            auto directLoad = LoadLibraryA(pdli->szDll);
            if (!directLoad)
                return reinterpret_cast<FARPROC>(LoadLibraryW(pathFFmpeg.wstring().c_str()));
            return reinterpret_cast<FARPROC>(directLoad);
        }
        if (exists(pathPython))
            return reinterpret_cast<FARPROC>(LoadLibraryW(pathPython.wstring().c_str()));
#else
        auto pathPython = std::filesystem::path{R"(A:\python-3.8.0-embed-amd64)"} / pdli->szDll;
        const auto handle = LoadLibraryW(pathPython.wstring().c_str());
        if (handle)
            return reinterpret_cast<FARPROC>(handle);
        FF_LOG(LOG_ERROR, "delay_load failed [%s]: %d", pathPython.string(), GetLastError());
#endif
    }

    return nullptr;
}
