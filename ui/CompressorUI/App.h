#pragma once
#include "App.g.h"

namespace winrt::CompressorUI::implementation
{
    struct App : AppT<App>
    {
        App();
        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);
    private:
        winrt::Microsoft::UI::Xaml::Window window{ nullptr };
    };
}

namespace winrt::CompressorUI::factory_implementation
{
    struct App : AppT<App, implementation::App>
    {
    };
}
