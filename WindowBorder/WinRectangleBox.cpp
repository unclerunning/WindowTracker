#include "stdafx.h"
#include "WinRectangleBox.h"
#include <thread>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;
using namespace std;

#ifndef ASSERT
#define ASSERT(expr)  _ASSERTE(expr)
#endif

ATOM RectangleBox::wnd_class_ = 0;
RectangleBox::RectangleBox(HWND trackWin)
{
	// Initialize GDI+.
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

	m_insertAfter = trackWin;
	// Init box pos and color
	HWND desk_hWnd = GetDesktopWindow();
	RECT deskScreenRect;
	GetWindowRect(desk_hWnd, &deskScreenRect);
	int left = (deskScreenRect.right - deskScreenRect.left) / 4;
	int top = (deskScreenRect.bottom - deskScreenRect.top) / 4;
	int width = (deskScreenRect.right - deskScreenRect.left) / 2;
	int height = (deskScreenRect.bottom - deskScreenRect.top) / 2;
	m_pos = { left, top, left + width,top+height};
	m_color = {255,0,0};
}
RectangleBox::~RectangleBox()
{
	// quit;	
	std::unique_lock<std::mutex> lock(m_cvM);
	m_messageQue.push(0);
	while (!m_messageQue.empty())
	{
		m_cv.wait(lock);
	}

	// Uninitialize GDI+
	GdiplusShutdown(m_gdiplusToken);
}

// DO it in the gui thead.
bool RectangleBox::Destroy() {
	BOOL ret = FALSE;

	if (m_hWnd&&::IsWindow(m_hWnd)) 
	{
		ret = ::DestroyWindow(m_hWnd);
	}

	return ret != FALSE;
}

void RectangleBox::OnPaint(HDC hdc)
{
	Graphics graphics(hdc);

	// Create a Pen object.
	Gdiplus::Pen pen(Gdiplus::Color(m_color.r, m_color.g, m_color.b), 6);

	// Draw rect.
	graphics.DrawRectangle(&pen, 0, 0, m_pos.right - m_pos.left, m_pos.bottom - m_pos.top);

	//// Draw line
	//int width = (m_pos.right - m_pos.left);
	//int height = (m_pos.bottom - m_pos.top);
	//if (width > height)
	//{
	//	width = height;
	//}
	//else 
	//{
	//	height = width;
	//}

	//int len = width / 6;
	//width = (m_pos.right - m_pos.left);
	//height = (m_pos.bottom - m_pos.top);

	//HWND desk_hWnd = GetDesktopWindow();
	//RECT deskScreenRect;
	//GetWindowRect(desk_hWnd, &deskScreenRect);

	//// up
	//if (m_pos.top>deskScreenRect.top)
	//{
	//	Gdiplus::Point p1(0, 0);
	//	Gdiplus::Point p2(len,0);
	//	graphics.DrawLine(&redPen,p1,p2);

	//	p1 = {width,0 };
	//	p2 = {width - len,0 };
	//	graphics.DrawLine(&redPen, p1, p2);
	//}

	//// bottom
	//if (m_pos.bottom<deskScreenRect.bottom)
	//{
	//	Gdiplus::Point p1(0, height);
	//	Gdiplus::Point p2(len,height);
	//	graphics.DrawLine(&redPen, p1, p2);

	//	p1 = { width, height };
	//	p2 = { width - len, height};
	//	graphics.DrawLine(&redPen, p1, p2);
	//}

	//// left
	//if (m_pos.left>deskScreenRect.left)
	//{
	//	Gdiplus::Point p1(0, 0);
	//	Gdiplus::Point p2(0, len);
	//	graphics.DrawLine(&redPen, p1, p2);

	//	p1 = { 0, height};
	//	p2 = { 0, height - len };
	//	graphics.DrawLine(&redPen, p1, p2);
	//}

	//// right
	//if (m_pos.right<deskScreenRect.right)
	//{
	//	Gdiplus::Point p1(width,0);
	//	Gdiplus::Point p2(width,len);
	//	graphics.DrawLine(&redPen, p1, p2);

	//	p1 = { width, height };
	//	p2 = { width,height - len };
	//	graphics.DrawLine(&redPen, p1, p2);
	//}
}

void RectangleBox::MessageLoop()
{
	MSG msg;
	// keep a message loop
	while (::GetMessage(&msg, NULL, 0, 0))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);

		// This used be efficient enough to use.
		std::unique_lock<std::mutex> lock(m_cvM);
		while(!m_messageQue.empty())
		{
			auto queMsg = m_messageQue.front();
			m_messageQue.pop();
			switch (queMsg)
			{	
			case 0: 
				Destroy();
				m_messageQue.swap(std::queue<int>()); //clear up
				break;
			case 1:
				ShowWndInternal(true);
				break;
			case 2:
				ShowWndInternal(false);
			default:
				break;
			}
		}
		m_cv.notify_all();
	}
}

bool RectangleBox::CreateWnd()
{
	std::mutex cvM;
	std::condition_variable cv;
	int finished = -1;

	auto borderGUITask = [&]
	{
		// 注册窗口类
		if (!RegisterWindowClass())
		{
			finished = 1;
			cv.notify_one();
			return;
		}

		// WS_POPUP实现无边框窗口
		DWORD ws = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_POPUP;
		// 实现透明必须设置WS_EX_LAYERED标志
		DWORD tranLayer = WS_EX_LAYERED;
		DWORD ws_ex = WS_EX_NOACTIVATE | tranLayer; //| WS_EX_TOPMOST I don't want it keep in the topmost layer.

		m_hWnd = CreateWindowEx(ws_ex,
			L"RectangleBox",
			L"WindowRectangle",
			ws,
			m_pos.left, m_pos.top, m_pos.right - m_pos.left, m_pos.bottom - m_pos.top,
			NULL,
			NULL,
			NULL,
			this);
		{
			::SetLayeredWindowAttributes(m_hWnd, RGB(255, 255, 255), 0, LWA_COLORKEY);    // 设置透明: 白色部分透明
		}

		DoTrackWin();
		ShowWndInternal(true);
		

		SetTimer(m_hWnd,             // handle to main window 
			IDT_TIMER4,             // timer identifier 
			10,						// 10ms interval 
			(TIMERPROC)NULL);       // no timer callback 

		finished = 0;
		cv.notify_one();

		// Has right to access private property of RectangleBox
		MessageLoop();
	};

	//note: Window must have a message loop
	thread borderGUIThread(borderGUITask);
	borderGUIThread.detach();

	std::unique_lock<std::mutex> lock(cvM);
	while (finished==-1)
	{	
		cv.wait(lock);
	}

	return finished==0;
}

void RectangleBox::ShowWnd(bool bShow /*= true*/)
{
	std::unique_lock<std::mutex> lock(m_cvM);
	if (bShow)
	{
		m_messageQue.push(1);
	}
	else
	{
		m_messageQue.push(2);
	}
}

void RectangleBox::ShowWndInternal(bool bShow /*= true*/)
{
	ASSERT(::IsWindow(m_hWnd));
	if (!::IsWindow(m_hWnd)) return;

	::ShowWindow(m_hWnd, bShow ? SW_SHOWNORMAL : SW_HIDE);
	::UpdateWindow(m_hWnd);
}

bool RectangleBox::IsColorValid(const BoderColor& color)
{
	bool ret = true;
	if (color.r < 0 || color.r>255)
		ret = false;
	if (color.g < 0 || color.g>255)
		ret = false;
	if (color.b < 0 || color.b>255)
		ret = false;

	return ret;
}

void RectangleBox::SetBorderColor(BoderColor color/*= { 255, 0, 0 }*/)
{
	if (IsColorValid(color))
		m_color = color;
}

bool RectangleBox::IsRectValid(const RECT& m_pos)
{
	bool ret = true;

	HWND desk_hWnd = GetDesktopWindow();
	RECT deskScreenRect;
	GetWindowRect(desk_hWnd, &deskScreenRect);

	if (m_pos.top >= m_pos.bottom || m_pos.left >= m_pos.right)
		ret = false;

	if (m_pos.top<deskScreenRect.top || m_pos.left<deskScreenRect.left 
		|| m_pos.bottom>deskScreenRect.bottom || m_pos.right>deskScreenRect.right)
		ret = false;

	return ret;
}

void RectangleBox::SetTrackWin(HWND insertAfter /*= nullptr*/)
{
	m_insertAfter = insertAfter;
	if (!m_insertAfter)
	{
		m_insertAfter = GetForegroundWindow();
	}
}

void RectangleBox::DoTrackWin()
{
	if (m_insertAfter&&::IsWindow(m_insertAfter))
	{
		RECT windowRect;
		::GetWindowRect(m_insertAfter, &windowRect);

		RECT borderRect = { windowRect.left - 5, windowRect.top - 5, windowRect.right+5, windowRect.bottom+5};
		//HWND desk_hWnd = GetDesktopWindow();
		//RECT deskScreenRect;
		//GetWindowRect(desk_hWnd, &deskScreenRect);
		//if (borderRect.left < deskScreenRect.left)
		//{
		//	borderRect.left = deskScreenRect.left;
		//}
		//if (borderRect.top < deskScreenRect.top)
		//{
		//	borderRect.top = deskScreenRect.top;
		//}
		//if (borderRect.right > deskScreenRect.right)
		//{
		//	borderRect.right = deskScreenRect.right;
		//}
		//if (borderRect.bottom > deskScreenRect.bottom)
		//{
		//	borderRect.bottom = deskScreenRect.bottom;
		//}
		m_pos = borderRect;
		//::MoveWindow(m_hWnd, m_pos.left, m_pos.top, m_pos.right - m_pos.left, m_pos.bottom - m_pos.top, TRUE);
		::SetWindowPos(m_hWnd, m_insertAfter, m_pos.left, m_pos.top, m_pos.right - m_pos.left, m_pos.bottom - m_pos.top, SWP_SHOWWINDOW);
		if (IsRectValid(m_pos))
		{
			if (!m_isValidRect)
			{
				InvalidateRect(m_hWnd, NULL, TRUE);
			}
			m_isValidRect = true; 
		}
		else
		{
			m_isValidRect = false;
		}
	}
}

// static
LRESULT CALLBACK RectangleBox::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	HDC          hdc;
	PAINTSTRUCT  ps;
	static int delX = 0;
	static int delY = 0;

	RectangleBox* me = reinterpret_cast<RectangleBox*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (msg)
	{
	case WM_CREATE:
	{
		if (!me)
		{
			CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
			me = reinterpret_cast<RectangleBox*>(cs->lpCreateParams);
			me->m_hWnd = hwnd;
			::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(me));
		}
	}
	break;
	case WM_PAINT:
	{
		hdc = BeginPaint(me->m_hWnd, &ps);
		me->OnPaint(hdc);
		EndPaint(me->m_hWnd, &ps);
	}
	break;
	case WM_TIMER:
	{
		me->DoTrackWin();
		KillTimer(me->m_hWnd, IDT_TIMER4);
		//reset this timer(IDT_TIMER1) so that it won't mess up with the older one(IDT_TIMER4);
		SetTimer(me->m_hWnd, IDT_TIMER4, 10, (TIMERPROC)NULL);
	}
	break;
	case WM_SETCURSOR:
	{
		HCURSOR hCur = LoadCursor(NULL, IDC_ARROW);
		::SetCursor(hCur);
	}
	break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		break;
	default:
		return ::DefWindowProc(hwnd, msg, wparam, lparam);
	}

	return 0;
}

// static
bool RectangleBox::RegisterWindowClass() {
	if (wnd_class_)
		return true;

	TCHAR lpszClassName[] = TEXT("RectangleBox");
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	//wc.hInstance = hInstance;
	wc.hInstance = ::GetModuleHandle(NULL);
	wc.hIcon = ::LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)::GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = lpszClassName;

	wnd_class_ = RegisterClass(&wc);

	return wnd_class_ != 0;
}