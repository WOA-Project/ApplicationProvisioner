#include "pch.h"
#include "ShortcutHelper.h"
#include <Windows.h>
#include <ShObjIdl_core.h>
#include <iostream>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::UI::Notifications;
using namespace Windows::Data::Xml::Dom;
using namespace Windows::Management::Deployment;
using namespace Windows::Storage;
using namespace Windows::ApplicationModel;

#pragma comment(lib,"shell32")

auto tag = L"installtag";
auto group = L"notificationgroup";
auto appName = L"Second Party Applications Provisioner", appUserModelID = L"Device Installation";
//auto appUserModelID = L"windows.immersivecontrolpanel_cw5n1h2txyewy!microsoft.windows.immersivecontrolpanel";

int c = 1;

void PopUpToast()
{
	auto result = ShortcutHelper::CreateShellShortcutWithAMUID(appUserModelID, appName);
	auto res = SetCurrentProcessExplicitAppUserModelID(appUserModelID);

	Sleep(4000);

	XmlDocument toastXml;
	toastXml.LoadXml(L"<toast scenario=\"reminder\">"
		"  <visual>"
		"    <binding template=\"ToastGeneric\">"
		"      <text>Device Installation</text>"
		"      <text>Please do not log off your device just yet while we are trying to install applications onto your device. We will notify you once done!</text>"
		"      <progress"
		"        title=\"Installing applications...\""
		"        value=\"{progressValue}\""
		"        valueStringOverride=\"{progressValueString}\""
		"        status=\"{progressStatus}\"/>"
		"    </binding>"
		"  </visual>"
		"</toast>");

	ToastNotification toast(toastXml);
	NotificationData data;
	data.Values().Insert(L"progressValue", L"0");
	data.Values().Insert(L"progressValueString", L"Pending...");
	data.Values().Insert(L"progressStatus", L"Unknown");
	data.SequenceNumber(c);
	//toast.Data(data);
	toast.Tag(tag);
	toast.Group(group);

	ToastNotificationManager::CreateToastNotifier(appUserModelID).Show(toast);
}

void PopUpFinalToast()
{
	XmlDocument toastXml;
	toastXml.LoadXml(L"<toast>"
		"  <visual>"
		"    <binding template=\"ToastGeneric\">"
		"      <text>Device Installation</text>"
		"      <text>We finished installing applications onto your device. You can now logoff or power off your device if you need to.</text>"
		"    </binding>"
		"  </visual>"
		"</toast>");

	ToastNotification toast(toastXml);
	ToastNotificationManager::CreateToastNotifier(appUserModelID).Show(toast);

	ShortcutHelper::DeleteShellShortcutWithAMUID(appName);
}

void PopUpFailureToast()
{
	XmlDocument toastXml;
	toastXml.LoadXml(L"<toast>"
		"  <visual>"
		"    <binding template=\"ToastGeneric\">"
		"      <text>Device Installation</text>"
		"      <text>We failed to install applications onto your device. Something might be wrong with your installation.</text>"
		"    </binding>"
		"  </visual>"
		"</toast>");

	ToastNotification toast(toastXml);
	ToastNotificationManager::CreateToastNotifier(appUserModelID).Show(toast);

	ShortcutHelper::DeleteShellShortcutWithAMUID(appName);
}

int total = 0;
int current = 0;

void UpdateToast(float progressValue, const wchar_t* progressStatus)
{
	NotificationData newdata;
	newdata.SequenceNumber(++c);
	if (progressValue < 1)
		newdata.Values().Insert(L"progressValue", std::to_wstring(progressValue));
	else
		newdata.Values().Insert(L"progressValue", L"1");
	newdata.Values().Insert(L"progressStatus", progressStatus);
	wchar_t valueStr[256];
	swprintf_s(valueStr, 256, L"%d/%d", current, total);
	newdata.Values().Insert(L"progressValueString", valueStr);
	ToastNotificationManager::CreateToastNotifier(appUserModelID).Update(newdata, tag, group);
}

void SuppressToast()
{
	ToastNotificationManager::History().Clear(appUserModelID);
}

int main()
{
	try
	{
		init_apartment();
		PopUpToast();

		PackageManager pacman;

		const wchar_t* dependencyARMDirectory = L"\\OEM\\Applications\\Dependencies\\arm";
		const wchar_t* dependencyARM64Directory = L"\\OEM\\Applications\\Dependencies\\arm64";
		const wchar_t* ApplicationsDirectory = L"\\OEM\\Applications";

		wchar_t windowsDirectory[MAX_PATH] = { 0 };
		wchar_t Directory[MAX_PATH] = { 0 };
		UINT winlength = GetWindowsDirectory(windowsDirectory, MAX_PATH);
		if (winlength == 0)
		{
			return 1;
		}
		else if (winlength > MAX_PATH)
		{
			return 2;
		}

		RtlZeroMemory(Directory, sizeof(Directory));
		swprintf_s(Directory, L"%s%s", windowsDirectory, L"\\OEM\\Applications\\Dependencies\\arm");

		StorageFolder armDependencyFolder = StorageFolder::GetFolderFromPathAsync(Directory).get();
		auto armDependencyFiles = armDependencyFolder.GetFilesAsync().get();

		RtlZeroMemory(Directory, sizeof(Directory));
		swprintf_s(Directory, L"%s%s", windowsDirectory, L"\\OEM\\Applications\\Dependencies\\arm64");

		StorageFolder arm64DependencyFolder = StorageFolder::GetFolderFromPathAsync(Directory).get();
		auto arm64DependencyFiles = arm64DependencyFolder.GetFilesAsync().get();

		RtlZeroMemory(Directory, sizeof(Directory));
		swprintf_s(Directory, L"%s%s", windowsDirectory, L"\\OEM\\Applications");

		StorageFolder applicationFolder = StorageFolder::GetFolderFromPathAsync(Directory).get();
		auto applicationFiles = applicationFolder.GetFilesAsync().get();

		total = armDependencyFiles.Size() + arm64DependencyFiles.Size() + applicationFiles.Size();
		current = 0;

		for (auto const& file : armDependencyFiles)
		{
			current++;
			Uri uri = Uri(file.Path());

			auto op = pacman.StagePackageAsync(uri, NULL);
			op.Progress([](
				IAsyncOperationWithProgress<DeploymentResult, DeploymentProgress> const& /* sender */,
				DeploymentProgress const& progress)
				{
					if (progress.state == DeploymentProgressState::Processing)
					{
						UpdateToast((float)progress.percentage / (float)100, L"Staging");
					}
				});
			auto depresult = op.get();
			UpdateToast(1, L"Provisioning");
			pacman.RegisterPackageByFamilyNameAsync(file.DisplayName(), NULL, DeploymentOptions::None, pacman.GetDefaultPackageVolume(), NULL).get();
			//pacman.ProvisionPackageForAllUsersAsync(file.DisplayName()).get();
		}

		for (auto const& file : arm64DependencyFiles)
		{
			current++;
			Uri uri = Uri(file.Path());

			auto op = pacman.StagePackageAsync(uri, NULL);
			op.Progress([](
				IAsyncOperationWithProgress<DeploymentResult, DeploymentProgress> const& /* sender */,
				DeploymentProgress const& progress)
				{
					if (progress.state == DeploymentProgressState::Processing)
					{
						UpdateToast((float)progress.percentage / (float)100, L"Staging");
					}
				});
			auto depresult = op.get();
			UpdateToast(1, L"Provisioning");
			pacman.RegisterPackageByFamilyNameAsync(file.DisplayName(), NULL, DeploymentOptions::None, pacman.GetDefaultPackageVolume(), NULL).get();
			//pacman.ProvisionPackageForAllUsersAsync(file.DisplayName()).get();
		}

		for (auto const& file : applicationFiles)
		{
			current++;
			Uri uri = Uri(file.Path());

			auto op = pacman.StagePackageAsync(uri, NULL);
			op.Progress([](
				IAsyncOperationWithProgress<DeploymentResult, DeploymentProgress> const& /* sender */,
				DeploymentProgress const& progress)
				{
					if (progress.state == DeploymentProgressState::Processing)
					{
						UpdateToast((float)progress.percentage / (float)100, L"Staging");
					}
				});
			auto depresult = op.get();
			UpdateToast(1, L"Provisioning");
			pacman.RegisterPackageByFamilyNameAsync(file.DisplayName(), NULL, DeploymentOptions::None, pacman.GetDefaultPackageVolume(), NULL).get();
			//pacman.ProvisionPackageForAllUsersAsync(file.DisplayName()).get();
		}

		SuppressToast();
		PopUpFinalToast();
	}
	catch (...)
	{
		SuppressToast();
		PopUpFailureToast();
	}
}
