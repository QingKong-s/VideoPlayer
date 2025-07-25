﻿#include "pch.h"

#include "CApp.h"
#include "CWndMain.h"

#include "eck\Env.h"


int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
	_In_ PWSTR pszCmdLine, _In_ int nCmdShow)
{
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);

	if (const auto hr = CoInitialize(nullptr); FAILED(hr))
	{
		EckDbgPrintFormatMessage(hr);
		eck::MsgBox(eck::Format(L"CoInitialize failed!\r\nhr = %08X", hr),
			L"Error", MB_ICONERROR);
		return 0;
	}

	eck::INITPARAM ip{};
	ip.uD3dCreateFlags |= D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
	DWORD dwErr;
	if (const auto iRetInit = eck::Init(hInstance, &ip, &dwErr);
		iRetInit != eck::InitStatus::Ok)
	{
		EckDbgPrintFormatMessage(dwErr);
		eck::MsgBox(eck::Format(L"Init failed!\r\nInitStatus = %d\r\nError code = %08X",
			(int)iRetInit, dwErr), L"Error", MB_ICONERROR);
		return 0;
	}

	App = new CApp{};
	CApp::Init();

	const auto pWnd = new CWndMain{};
	const auto hMon = eck::GetOwnerMonitor(nullptr);
	const auto iDpi = eck::GetMonitorDpi(hMon);
	auto size = SIZE{ 640,480 };
	eck::DpiScale(size, iDpi);
	const auto pt = eck::CalcCenterWndPos(nullptr, size.cx, size.cy, FALSE);
	pWnd->SetPresentMode(Dui::PresentMode::DCompositionSurface);
	pWnd->SetUserDpi(iDpi);
	pWnd->SetTransparent(TRUE);
	pWnd->Create(L"示例Win32程序", WS_OVERLAPPEDWINDOW, 0,
		pt.x, pt.y, size.cx, size.cy, nullptr, 0);
	pWnd->Visible = TRUE;

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0))
	{
		if (!eck::PreTranslateMessage(msg))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}
	delete pWnd;
	delete App;
	eck::ThreadUnInit();
	eck::UnInit();
#ifdef _DEBUG
	if (eck::g_pDxgiDebug)
		eck::g_pDxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
#endif
	CoUninitialize();
	return (int)msg.wParam;
}