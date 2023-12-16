#pragma once
#include <Windows.h>

class ShortcutHelper
{
public:
	static BOOL CreateShellShortcutWithAMUID(const std::wstring& aumi, const std::wstring& appName);
	static void DeleteShellShortcutWithAMUID(const std::wstring& appName);
};

