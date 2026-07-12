#include "pch.h"
#include "MainWindow.h"
#include "MainWindow.g.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace winrt::CompressorUI::implementation;

namespace winrt::CompressorUI::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
    }

    // TODO(M2): open file picker -> Archive API (.cza) -> populate file list.
    void MainWindow::MyButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
    }
}
