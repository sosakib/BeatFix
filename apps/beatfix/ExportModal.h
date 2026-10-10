#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../src/bf_engine/session.h"
#include <functional>

namespace bf::gui {

class ExportModal final : public juce::Component {
public:
    explicit ExportModal(engine::Session* session, std::function<void()> on_close);
    ~ExportModal() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void perform_export();

    engine::Session* m_session = nullptr;
    std::function<void()> m_on_close;

    juce::Label m_title_label;
    juce::Label m_path_label;
    juce::TextEditor m_path_editor;
    juce::TextButton m_browse_button;

    juce::Label m_format_label;
    juce::ComboBox m_format_combo;

    juce::ToggleButton m_peak_gate_toggle;
    juce::Label m_status_label;

    juce::TextButton m_export_button;
    juce::TextButton m_cancel_button;
};

} // namespace bf::gui
