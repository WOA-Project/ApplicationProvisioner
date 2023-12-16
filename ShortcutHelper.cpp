/* * Copyright (C) 2016-2019 Mohammed Boujemaoui <mohabouje@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "pch.h"
#include "ShortcutHelper.h"

#include <Windows.h>
#include <sdkddkver.h>
#include <WinUser.h>
#include <ShObjIdl.h>
#include <wrl/implements.h>
#include <wrl/event.h>
#include <strsafe.h>
#include <Psapi.h>
#include <ShlObj.h>
#include <roapi.h>
#include <propvarutil.h>
#include <functiondiscoverykeys.h>
#include <iostream>
#include <winstring.h>
#include <string.h>
#include <vector>
#include <map>

#pragma comment(lib,"shlwapi")
#pragma comment(lib,"user32")
#pragma comment(lib,"Propsys")

#define DEFAULT_SHELL_LINKS_PATH	L"\\Microsoft\\Windows\\Start Menu\\Programs\\"
#define DEFAULT_LINK_FORMAT			L".lnk"

using namespace Microsoft::WRL;

inline HRESULT defaultExecutablePath(_In_ WCHAR* path, _In_ DWORD nSize = MAX_PATH) {
    DWORD written = GetModuleFileNameExW(GetCurrentProcess(), nullptr, path, nSize);
    return (written > 0) ? S_OK : E_FAIL;
}


inline HRESULT defaultShellLinksDirectory(_In_ WCHAR* path, _In_ DWORD nSize = MAX_PATH) {
    DWORD written = GetEnvironmentVariableW(L"APPDATA", path, nSize);
    HRESULT hr = written > 0 ? S_OK : E_INVALIDARG;
    if (SUCCEEDED(hr)) {
        errno_t result = wcscat_s(path, nSize, DEFAULT_SHELL_LINKS_PATH);
        hr = (result == 0) ? S_OK : E_INVALIDARG;
    }
    return hr;
}

inline HRESULT defaultShellLinkPath(const std::wstring& appname, _In_ WCHAR* path, _In_ DWORD nSize = MAX_PATH) {
    HRESULT hr = defaultShellLinksDirectory(path, nSize);
    if (SUCCEEDED(hr)) {
        const std::wstring appLink(appname + DEFAULT_LINK_FORMAT);
        errno_t result = wcscat_s(path, nSize, appLink.c_str());
        hr = (result == 0) ? S_OK : E_INVALIDARG;
    }
    return hr;
}

void ShortcutHelper::DeleteShellShortcutWithAMUID(const std::wstring& appName) {
    WCHAR	path[MAX_PATH] = { L'\0' };
    defaultShellLinkPath(appName, path);

    char buffer[MAX_PATH];
    wcstombs(buffer, path, sizeof(buffer));

    remove(buffer);
}

BOOL ShortcutHelper::CreateShellShortcutWithAMUID(const std::wstring& aumi, const std::wstring& appName) {
    WCHAR	path[MAX_PATH] = { L'\0' };
    ComPtr<IShellLink> shellLink;
    HRESULT hr;
    defaultShellLinkPath(appName, path);
    // Check if the file exist
    DWORD attr = GetFileAttributesW(path);
    if (attr >= 0xFFFFFFF) {
        goto secondpart;
    }

    // Let's load the file as shell link to validate.
    // - Create a shell link
    // - Create a persistant file
    // - Load the path as data for the persistant file
    // - Read the property AUMI and validate with the current
    // - Review if AUMI is equal.
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
    if (SUCCEEDED(hr)) {
        ComPtr<IPersistFile> persistFile;
        hr = shellLink.As(&persistFile);
        if (SUCCEEDED(hr)) {
            hr = persistFile->Load(path, STGM_READWRITE);
            if (SUCCEEDED(hr)) {
                ComPtr<IPropertyStore> propertyStore;
                hr = shellLink.As(&propertyStore);
                if (SUCCEEDED(hr)) {
                    PROPVARIANT appIdPropVar;
                    hr = propertyStore->GetValue(PKEY_AppUserModel_ID, &appIdPropVar);
                    if (SUCCEEDED(hr)) {
                        WCHAR AUMI[MAX_PATH];
                        hr = PropVariantToString(appIdPropVar, AUMI, MAX_PATH);
                        if (FAILED(hr) || aumi != AUMI) {
                            // AUMI Changed for the same app, let's update the current value! =)
                            PropVariantClear(&appIdPropVar);
                            hr = InitPropVariantFromString(aumi.c_str(), &appIdPropVar);
                            if (SUCCEEDED(hr)) {
                                hr = propertyStore->SetValue(PKEY_AppUserModel_ID, appIdPropVar);
                                if (SUCCEEDED(hr)) {
                                    hr = propertyStore->Commit();
                                    if (SUCCEEDED(hr) && SUCCEEDED(persistFile->IsDirty())) {
                                        hr = persistFile->Save(path, TRUE);
                                    }
                                }
                            }
                        }
                        PropVariantClear(&appIdPropVar);
                    }
                }
            }
        }
    }

    if (SUCCEEDED(hr)) {
        return TRUE;
    }

secondpart:
    WCHAR   exePath[MAX_PATH]{ L'\0' };
    WCHAR	slPath[MAX_PATH]{ L'\0' };
    defaultShellLinkPath(appName, slPath);
    defaultExecutablePath(exePath);
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
    if (SUCCEEDED(hr)) {
        hr = shellLink->SetPath(exePath);
        if (SUCCEEDED(hr)) {
            hr = shellLink->SetArguments(L"");
            if (SUCCEEDED(hr)) {
                hr = shellLink->SetWorkingDirectory(exePath);
                if (SUCCEEDED(hr)) {
                    ComPtr<IPropertyStore> propertyStore;
                    hr = shellLink.As(&propertyStore);
                    if (SUCCEEDED(hr)) {
                        PROPVARIANT appIdPropVar;
                        hr = InitPropVariantFromString(aumi.c_str(), &appIdPropVar);
                        if (SUCCEEDED(hr)) {
                            hr = propertyStore->SetValue(PKEY_AppUserModel_ID, appIdPropVar);
                            if (SUCCEEDED(hr)) {
                                hr = propertyStore->Commit();
                                if (SUCCEEDED(hr)) {
                                    ComPtr<IPersistFile> persistFile;
                                    hr = shellLink.As(&persistFile);
                                    if (SUCCEEDED(hr)) {
                                        hr = persistFile->Save(slPath, TRUE);
                                    }
                                }
                            }
                            PropVariantClear(&appIdPropVar);
                        }
                    }
                }
            }
        }
    }

    return SUCCEEDED(hr);
}