#include "flutter_window.h"

#include <iostream>
#include <optional>

#include "flutter/generated_plugin_registrant.h"
#include "resource.h"

NOTIFYICONDATA icon;
static bool built = false;
constexpr int WM_TRAY = 0x1E05;

void BuildIcon(HWND hwnd) {
  if (built || !hwnd) return;
  built = true;
  icon.cbSize = sizeof(NOTIFYICONDATA);
  icon.hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_APP_ICON));
  icon.hWnd = hwnd;
  icon.uID = IDI_APP_ICON;
  icon.uCallbackMessage = WM_TRAY;
  icon.dwInfoFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  std::wstring name = L"Application";
  wcscpy_s(icon.szTip, name.c_str());
  std::cerr << "Built icon\n";
}

FlutterWindow::FlutterWindow(const flutter::DartProject& project)
    : project_(project) {}

FlutterWindow::~FlutterWindow() {}

bool FlutterWindow::OnCreate() {
  if (!Win32Window::OnCreate()) {
    return false;
  }

  RECT frame = GetClientArea();

  // The size here must match the window dimensions to avoid unnecessary surface
  // creation / destruction in the startup path.
  flutter_controller_ = std::make_unique<flutter::FlutterViewController>(
      frame.right - frame.left, frame.bottom - frame.top, project_);
  // Ensure that basic setup of the controller was successful.
  if (!flutter_controller_->engine() || !flutter_controller_->view()) {
    return false;
  }
  RegisterPlugins(flutter_controller_->engine());
  SetChildContent(flutter_controller_->view()->GetNativeWindow());

  flutter_controller_->engine()->SetNextFrameCallback([&]() {
    this->Show();
  });

  BuildIcon(GetHandle());
  if (!Shell_NotifyIcon(NIM_ADD, &icon)) {
    std::cout << "Notifying Icon failed";
  }

  return true;
}

void FlutterWindow::OnDestroy() {
  if (flutter_controller_) {
    flutter_controller_ = nullptr;
  }

  Win32Window::OnDestroy();
}

LRESULT
FlutterWindow::MessageHandler(HWND hwnd, UINT const message,
                              WPARAM const wparam,
                              LPARAM const lparam) noexcept {
  // Give Flutter, including plugins, an opportunity to handle window messages.
  if (flutter_controller_) {
    std::optional<LRESULT> result =
        flutter_controller_->HandleTopLevelWindowProc(hwnd, message, wparam,
                                                      lparam);
    if (result) {
      return *result;
    }
  }

  switch (message) {
    case WM_FONTCHANGE:
      flutter_controller_->engine()->ReloadSystemFonts();
      break;
    case WM_CLOSE:
      BuildIcon(hwnd);
      std::cerr << "Closing (to tray)\n";
      Shell_NotifyIcon(NIM_ADD, &icon);
      ShowWindow(hwnd, SW_HIDE);
      return Win32Window::MessageHandler(hwnd, WM_SIZE, SIZE_MINIMIZED, lparam);
    case WM_DESTROY:
      std::cerr << "Window destroyed\n";
      Shell_NotifyIcon(NIM_DELETE, &icon);
      PostQuitMessage(0);
      break;
    case WM_TRAY:
      std::cerr << "Menu action\n";
      switch (lparam) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
          ShowWindow(hwnd, SW_SHOW);
          SetForegroundWindow(hwnd);
          break;
      }
      break;
  }

  return Win32Window::MessageHandler(hwnd, message, wparam, lparam);
}
