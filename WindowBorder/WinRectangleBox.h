#ifndef WIN_RECTANGEL_BOX_H
#define WIN_RECTANCEL_BOX_H

#include <windows.h>
#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>

#define IDT_TIMER4 (WM_USER + 4)

class RectangleBox
{
public:
	struct BoderColor
	{
		IN BYTE r;
		IN BYTE g;
		IN BYTE b;
	};

public:
	RectangleBox(HWND trackWin);
	~RectangleBox();
public:
	bool CreateWnd();
	void ShowWnd(bool bShow = true);
	void SetBorderColor(BoderColor color = {255,0,0});
	void SetTrackWin(HWND insertAfter = nullptr);
private:
	void OnPaint(HDC hdc);
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	static bool RectangleBox::RegisterWindowClass();
	bool Destroy();
	bool IsColorValid(const BoderColor& color);
	bool IsRectValid(const RECT& m_pos);
	void MessageLoop();
	void ShowWndInternal(bool bShow = true);
	void DoTrackWin();
private:
	HWND m_hWnd{nullptr};
	BoderColor  m_color;
	RECT	m_pos;
	HWND	m_insertAfter;
	ULONG_PTR     m_gdiplusToken;
	std::mutex m_cvM;
	std::condition_variable m_cv;
	std::queue<int>   m_messageQue; //0 -- quit  1 -- show  2 -- hide;
	static ATOM  wnd_class_;

	bool m_isValidRect{false};
};



#endif //RECTANCEL_BOX_H!