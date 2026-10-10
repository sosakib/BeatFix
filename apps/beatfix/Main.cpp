#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

namespace bf::gui {

class MainWindow final : public juce::DocumentWindow {
public:
    explicit MainWindow(juce::String name)
        : DocumentWindow(name,
                         juce::Colour(0xFF14171E),
                         DocumentWindow::allButtons) {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(), true);
        setResizable(true, true);
        setResizeLimits(1024, 640, 3840, 2160);
        centreWithSize(1280, 800);
        setVisible(true);
    }

    void closeButtonPressed() override {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

class BeatFixApplication final : public juce::JUCEApplication {
public:
    BeatFixApplication() = default;

    const juce::String getApplicationName() override { return "BeatFix"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& /*commandLine*/) override {
        m_main_window = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override {
        m_main_window.reset();
    }

    void systemRequestedQuit() override {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override {}

private:
    std::unique_ptr<MainWindow> m_main_window;
};

} // namespace bf::gui

START_JUCE_APPLICATION(bf::gui::BeatFixApplication)
