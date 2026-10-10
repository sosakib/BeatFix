#include "ExportModal.h"
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace bf::gui {

ExportModal::ExportModal(engine::Session* session, std::function<void()> on_close)
    : m_session(session), m_on_close(std::move(on_close)) {

    m_title_label.setText("Export Corrected Audio", juce::dontSendNotification);
    m_title_label.setFont(juce::Font(18.0f, juce::Font::bold));
    m_title_label.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(m_title_label);

    m_path_label.setText("Output File:", juce::dontSendNotification);
    m_path_label.setColour(juce::Label::textColourId, juce::Colour(0xFFA0AEC0));
    addAndMakeVisible(m_path_label);

    std::string default_out = "output_beatfixed.wav";
    if (m_session && !m_session->source_audio().source_path.empty()) {
        std::filesystem::path sp(m_session->source_audio().source_path);
        std::string stem = sp.stem().string();
        default_out = (sp.parent_path() / (stem + "_fixed.wav")).string();
    }
    m_path_editor.setText(default_out);
    addAndMakeVisible(m_path_editor);

    m_browse_button.setButtonText("Browse...");
    m_browse_button.onClick = [this] {
        juce::FileChooser chooser("Select export location", juce::File(m_path_editor.getText()), "*.wav");
        if (chooser.browseForFileToSave(true)) {
            m_path_editor.setText(chooser.getResult().getFullPathName());
        }
    };
    addAndMakeVisible(m_browse_button);

    m_format_label.setText("Bit Depth:", juce::dontSendNotification);
    m_format_label.setColour(juce::Label::textColourId, juce::Colour(0xFFA0AEC0));
    addAndMakeVisible(m_format_label);

    m_format_combo.addItem("16-bit PCM (TPDF Dither)", 1);
    m_format_combo.addItem("24-bit PCM (TPDF Dither)", 2);
    m_format_combo.addItem("32-bit Float (Lossless)", 3);
    m_format_combo.setSelectedId(1);
    addAndMakeVisible(m_format_combo);

    m_peak_gate_toggle.setButtonText("Lower gain if peaking (> 0 dBFS / -1 dBTP)");
    m_peak_gate_toggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(m_peak_gate_toggle);

    m_status_label.setText("Ready to export.", juce::dontSendNotification);
    m_status_label.setColour(juce::Label::textColourId, juce::Colour(0xFFCBD5E0));
    addAndMakeVisible(m_status_label);

    m_export_button.setButtonText("Export Now");
    m_export_button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF3182CE));
    m_export_button.onClick = [this] { perform_export(); };
    addAndMakeVisible(m_export_button);

    m_cancel_button.setButtonText("Close");
    m_cancel_button.onClick = [this] {
        if (m_on_close) m_on_close();
    };
    addAndMakeVisible(m_cancel_button);

    setSize(520, 320);
}

void ExportModal::resized() {
    m_title_label.setBounds(24, 20, 470, 24);

    m_path_label.setBounds(24, 60, 100, 24);
    m_path_editor.setBounds(124, 60, 280, 24);
    m_browse_button.setBounds(410, 60, 80, 24);

    m_format_label.setBounds(24, 100, 100, 24);
    m_format_combo.setBounds(124, 100, 280, 24);

    m_peak_gate_toggle.setBounds(24, 140, 460, 24);

    m_status_label.setBounds(24, 180, 460, 60);

    m_export_button.setBounds(280, 260, 110, 32);
    m_cancel_button.setBounds(400, 260, 90, 32);
}

void ExportModal::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF222630));
    g.setColour(juce::Colour(0xFF4A5568));
    g.drawRect(getLocalBounds(), 2);
}

void ExportModal::perform_export() {
    if (!m_session) return;

    m_status_label.setText("Exporting and validating...", juce::dontSendNotification);

    io::ExportSettings settings;
    settings.output_path = m_path_editor.getText().toStdString();
    int sel = m_format_combo.getSelectedId();
    settings.bit_depth = (sel == 3) ? 32 : ((sel == 2) ? 24 : 16);
    settings.lower_gain_if_peaking = m_peak_gate_toggle.getToggleState();

    auto exp_res = m_session->export_corrected(settings);
    if (!exp_res.is_ok()) {
        m_status_label.setText("Error: " + juce::String(exp_res.error_message()), juce::dontSendNotification);
        m_status_label.setColour(juce::Label::textColourId, juce::Colour(0xFFE53E3E));
        return;
    }

    const auto& rep = exp_res.value();
    std::ostringstream ss;
    ss << "Export Succeeded!\n"
       << "Sample Peak: " << std::fixed << std::setprecision(2) << rep.sample_peak_dbfs << " dBFS | "
       << "True Peak: " << rep.true_peak_dbtp << " dBTP\n"
       << "Gain reduction: " << rep.applied_gain_reduction_db << " dB | "
       << "Re-decode Validation: " << (rep.re_decode_verified ? "PASSED" : "FAILED");

    m_status_label.setText(ss.str(), juce::dontSendNotification);
    m_status_label.setColour(juce::Label::textColourId, juce::Colour(0xFF48BB78)); // Green
}

} // namespace bf::gui
