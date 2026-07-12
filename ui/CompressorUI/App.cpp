#include "pch.h"
#include "App.h"
#include "App.g.h"
#include "MainWindow.h"
#include "MainWindow.g.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::CompressorUI::implementation;

App::App()
{
#if defined(_DEBUG) && !defined(DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION)
    UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
    {
        if (IsDebuggerPresent())
        {
            OutputDebugStringW(e.Message().c_str());
            DebugBreak();
        }
    });
#endif
}

void App::OnLaunched(LaunchActivatedEventArgs const&)
{
    window = make<MainWindow>();
    window.Activate();
}
