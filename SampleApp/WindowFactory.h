#pragma once

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <winrt/Windows.Foundation.h>
#include <unordered_map>
#include <functional>
#include <CommCtrl.h>

using EventCallback = std::function<void(HWND)>;

class WindowInterface
{
public:
    virtual void Size(int width, int height) const = 0;
    virtual void Position(int x, int y) const = 0;
    virtual void Button(LPCWSTR text, int x, int y, int width, int height, EventCallback cb) = 0;
    virtual void Close() = 0;
    virtual bool Closed() const = 0;
    virtual LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) = 0;
};

class Window : public WindowInterface
{
public:
    Window()
        : mHwnd{ nullptr }
        , mControlId{ 100 }
        , mClosed{ false }
    {
    }

    virtual ~Window()
    {
        SetWindowLongPtr(Handle(), GWLP_USERDATA, (LONG_PTR)nullptr);

        if (!Closed())
        {
            Closed(true);
            Close();
        }
    }

    virtual void Size(int width, int height) const override
    {
        SetWindowPos(mHwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_FRAMECHANGED);
    }

    virtual void Position(int x, int y) const override
    {
        SetWindowPos(mHwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_FRAMECHANGED);
    }

    virtual void SizeAndPosition(int newX, int newY, int newWidth, int newHeight)
    {
        SetWindowPos(mHwnd, nullptr, newX, newY, newWidth, newHeight, SWP_FRAMECHANGED);
    }

    virtual void Button(LPCWSTR text, int x, int y, int width, int height, EventCallback cb)
    {
        // https://docs.microsoft.com/en-us/windows/win32/controls/create-a-button
        auto id = mControlId++;
        mCallbacks.insert(std::make_pair(id, cb));
        (void)CreateWindow(
            WC_BUTTON, // Predefined class; Unicode assumed 
            text,      // Button text 
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
            x,         // x position 
            y,         // y position 
            width,     // Button width
            height,    // Button height
            mHwnd,     // Parent window
            (HMENU)id,
            (HINSTANCE)GetWindowLongPtr(mHwnd, GWLP_HINSTANCE),
            nullptr);  // Pointer not needed.
    }

    virtual void Close() override
    {
        CloseWindow(mHwnd);
    }

    virtual bool Closed() const override
    {
        return mClosed;
    }

protected:

    virtual LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override
    {
        switch (msg)
        {
        case WM_CREATE:
        {
            Handle(hwnd);
            ShowWindow(hwnd, SW_SHOWDEFAULT);
            return (LRESULT)nullptr;
        }

        case WM_CLOSE:
        {
            Closed(true);
            DestroyWindow(hwnd);
            return 0;
        }

        // https://docs.microsoft.com/en-us/windows/win32/menurc/wm-command
        case WM_COMMAND:
        {
            auto code = HIWORD(wparam);

            switch (code)
            {
                // https://docs.microsoft.com/en-us/windows/win32/controls/bn-clicked
            case BN_CLICKED:
            {
                long id = LOWORD(wparam);
                auto button = (HWND)lparam;
                auto cb = mCallbacks.find(id);
                if (cb != mCallbacks.end())
                    cb->second(button);

                break;
            }

            default:
                break;
            }
            break;
        }
        default:
            break;
        }

        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

protected:
    HWND Handle() const
    {
        return mHwnd;
    }

    void Handle(HWND hwnd)
    {
        mHwnd = hwnd;
    }

    void Closed(bool closed)
    {
        mClosed = closed;
    }

private:
    volatile long long mControlId;
    std::unordered_map<long long, EventCallback> mCallbacks;
    bool mClosed;
    HWND mHwnd;

};

class BorderWindow : public Window
{
    const int BorderWidth = 10;

public:
    virtual ~BorderWindow()
    {
        if (!Closed())
        {
            Close();
        }
    }

protected:

    virtual LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override
    {
        switch (msg)
        {
        case WM_CREATE:
            Handle(hwnd);

            // https://docs.microsoft.com/en-us/windows/win32/winmsg/using-windows#using-layered-windows
            // https://docs.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles
            SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_TOPMOST);
            SetLayeredWindowAttributes(
                hwnd,
                RGB(0, 0, 0),
                128,
                LWA_COLORKEY | LWA_ALPHA);

            SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
            SetWindowLong(hwnd, GWL_STYLE, 0);  // Without 1 point border = white rectangle 
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_FRAMECHANGED);
            ShowWindow(hwnd, SW_SHOWDEFAULT);
            break;
        case WM_MOVE:
        case WM_SIZE:
            Repaint();
            break;
        default:
            return Window::WndProc(hwnd, msg, wparam, lparam);
        }

        return (LRESULT)nullptr;
    }

    virtual void Close() override
    {
        Closed(true);
        DestroyWindow(Handle());
    }

private:
    void Repaint()
    {
        RECT rect;
        GetClientRect(Handle(), &rect);

        if (rect.left == 0 &&
            rect.right == 0 &&
            rect.top == 0 &&
            rect.bottom == 0)
        {
            return;
        }

        // https://docs.microsoft.com/en-us/windows/win32/gdi/using-filled-shapes
        PAINTSTRUCT ps;

        BeginPaint(Handle(), &ps);

        SelectObject(ps.hdc, GetStockObject(NULL_PEN));

        HBRUSH eraser = CreateSolidBrush(RGB(0, 0, 0));
        HBRUSH brush = CreateSolidBrush(RGB(255, 0, 0));

        // erase
        SelectObject(ps.hdc, eraser);
        Rectangle(ps.hdc, rect.left, rect.top, rect.right, rect.bottom);

        // borders
        SelectObject(ps.hdc, brush);

        // Top Border
        Rectangle(ps.hdc, rect.left, rect.top, rect.right, BorderWidth);

        // Left Border
        Rectangle(ps.hdc, rect.left, rect.top, BorderWidth, rect.bottom);

        // Bottom Border
        Rectangle(ps.hdc, rect.left, rect.bottom - BorderWidth, rect.right, rect.bottom);

        // Right Border
        Rectangle(ps.hdc, rect.right - BorderWidth, rect.top, rect.right, rect.bottom);

        DeleteObject(brush);
        DeleteObject(eraser);
        EndPaint(Handle(), &ps);
    }
};

template<typename T, class... Args>
class WindowFactory
{
public:

    static WindowFactory Create(HINSTANCE hInstance)
    {
        return WindowFactory{ hInstance };
    }

    std::unique_ptr<T> NewWindow(Args&&... args)
    {
        auto newWindow = std::make_unique<T>(std::forward<Args>(args)...);
        HWND windowHandle = CreateWindowHandle(newWindow.get());
        winrt::check_pointer(windowHandle);
        return std::move(newWindow);
    }

private:

    HWND CreateWindowHandle(WindowInterface* windowInterface)
    {
        return CreateWindowEx(
            0,                              // Optional window styles.
            mWindowClassName.c_str(),     // Window class
            L"Window",    // Window title
            WS_OVERLAPPEDWINDOW,            // Window style

            // Size and position
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

            NULL,       // Parent window    
            NULL,       // Menu
            mInstanceHandle,       // Instance handle
            windowInterface        // Additional application data
        );
    }

    static LRESULT WndProcDispatcher(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        if (msg == WM_CREATE)
        {
            auto createStruct = (LPCREATESTRUCT)lparam;
            winrt::check_pointer(createStruct);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)createStruct->lpCreateParams);
        }

        auto window = (WindowInterface*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (window == nullptr)
        {
            return DefWindowProc(hwnd, msg, wparam, lparam);
        }

        return window->WndProc(hwnd, msg, wparam, lparam);
    }

    WindowFactory(HINSTANCE hInstance)
        : mInstanceHandle{ hInstance }
        , mWindowClass{ }
    {
        mWindowClass.hInstance = hInstance;
        mWindowClass.lpfnWndProc = &WindowFactory::WndProcDispatcher;

        std::wstringstream wss;
        wss << typeid(T).raw_name();
        mWindowClassName = wss.str();
        LPCWSTR lpcClassName = mWindowClassName.c_str();

        mWindowClass.lpszClassName = lpcClassName;
        RegisterClass(&mWindowClass);
    }

    std::wstring mWindowClassName;
    WNDCLASS mWindowClass;
    const HINSTANCE mInstanceHandle;
};
