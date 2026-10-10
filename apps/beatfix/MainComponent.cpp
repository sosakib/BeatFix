#include "MainComponent.h"
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>

namespace bf::gui {

MainComponent::MainComponent() {
    m_player.init();
    setWantsKeyboardFocus(true);

    // --- Header ---
    m_open_button.onClick = [this] { open_file_dialog(); };
    addAndMakeVisible(m_open_button);

    m_save_button.onClick = [this] { save_project_dialog(); };
    addAndMakeVisible(m_save_button);

    m_file_info_label.setText("No file loaded", juce::dontSendNotification);
    m_file_info_label.setColour(juce::Label::textColourId, juce::Colour(0xFFA0AEC0));
    addAndMakeVisible(m_file_info_label);

    m_undo_button.onClick = [this] {
        if (m_session.can_undo()) {
            m_session.undo();
            update_ui_from_session();
            trigger_render();
        }
    };
    addAndMakeVisible(m_undo_button);

    m_redo_button.onClick = [this] {
        if (m_session.can_redo()) {
            m_session.redo();
            update_ui_from_session();
            trigger_render();
        }
    };
    addAndMakeVisible(m_redo_button);

    // --- Control Strip ---
    m_detected_label.setText("DETECTED: --- BPM", juce::dontSendNotification);
    m_detected_label.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(m_detected_label);

    m_confidence_badge.setText("---", juce::dontSendNotification);
    m_confidence_badge.setColour(juce::Label::textColourId, juce::Colour(0xFF68D391));
    addAndMakeVisible(m_confidence_badge);

    m_target_label.setText("TARGET:", juce::dontSendNotification);
    addAndMakeVisible(m_target_label);

    m_target_bpm_slider.setRange(40.0, 300.0, 0.1);
    m_target_bpm_slider.setValue(120.0);
    m_target_bpm_slider.setSliderStyle(juce::Slider::LinearBar);
    m_target_bpm_slider.onValueChange = [this] {
        if (m_session.is_analyzed() && std::abs(m_session.target_bpm() - m_target_bpm_slider.getValue()) > 0.05) {
            m_session.set_target_bpm(m_target_bpm_slider.getValue());
            m_waveform_view.repaint();
            trigger_render();
        }
    };
    addAndMakeVisible(m_target_bpm_slider);

    m_reset_bpm_button.onClick = [this] {
        if (m_session.is_analyzed()) {
            double orig = std::round(m_session.analysis().bpm_global);
            m_target_bpm_slider.setValue(orig);
        }
    };
    addAndMakeVisible(m_reset_bpm_button);

    m_detent_label.setText("CORRECTION: 2 bars (Natural)", juce::dontSendNotification);
    addAndMakeVisible(m_detent_label);

    m_detent_slider.setRange(0, 5, 1);
    m_detent_slider.setValue(2); // 2 = Natural8 (default)
    m_detent_slider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_detent_slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    m_detent_slider.onValueChange = [this] {
        int v = static_cast<int>(m_detent_slider.getValue());
        dsp::WarpDetent det = dsp::WarpDetent::Natural8;
        juce::String name = "2 bars (Natural)";
        if (v == 0) { det = dsp::WarpDetent::Strict; name = "Strict (0 bars)"; }
        else if (v == 1) { det = dsp::WarpDetent::Detent4; name = "1 bar"; }
        else if (v == 2) { det = dsp::WarpDetent::Natural8; name = "2 bars (Natural)"; }
        else if (v == 3) { det = dsp::WarpDetent::Detent16; name = "4 bars"; }
        else if (v == 4) { det = dsp::WarpDetent::Detent32; name = "8 bars"; }
        else if (v == 5) { det = dsp::WarpDetent::Global; name = "Global (Constant Ratio)"; }

        m_detent_label.setText("CORRECTION: " + name, juce::dontSendNotification);
        if (m_session.is_analyzed()) {
            m_session.set_detent(det);
            m_waveform_view.repaint();
            trigger_render();
        }
    };
    addAndMakeVisible(m_detent_slider);

    m_mode_combo.addItem("Smooth (Signalsmith)", 1);
    m_mode_combo.addItem("Resample (Catmull-Rom)", 2);
    m_mode_combo.setSelectedId(1);
    m_mode_combo.onChange = [this] {
        dsp::StretchEngineMode m = (m_mode_combo.getSelectedId() == 2) ? dsp::StretchEngineMode::Resample : dsp::StretchEngineMode::Smooth;
        if (m_session.is_analyzed()) {
            m_session.set_mode(m);
            trigger_render();
        }
    };
    addAndMakeVisible(m_mode_combo);

    m_diag_label.setText("Ratio: [1.00..1.00] | Warnings: 0", juce::dontSendNotification);
    m_diag_label.setColour(juce::Label::textColourId, juce::Colour(0xFFA0AEC0));
    addAndMakeVisible(m_diag_label);

    // --- Waveform View ---
    m_waveform_view.set_session(&m_session);
    m_waveform_view.on_seek = [this](double s) {
        m_player.seek_seconds(s);
        m_waveform_view.set_playhead_seconds(s);
    };
    addAndMakeVisible(m_waveform_view);

    // --- Transport Bar ---
    m_play_pause_button.onClick = [this] {
        if (m_player.is_playing()) {
            m_player.pause();
            m_play_pause_button.setButtonText("▶ Play");
        } else {
            m_player.play();
            m_play_pause_button.setButtonText("⏸ Pause");
        }
    };
    addAndMakeVisible(m_play_pause_button);

    m_stop_button.onClick = [this] {
        m_player.stop();
        m_play_pause_button.setButtonText("▶ Play");
        m_waveform_view.set_playhead_seconds(0.0);
    };
    addAndMakeVisible(m_stop_button);

    m_seek_start_button.onClick = [this] {
        m_player.seek_seconds(0.0);
        m_waveform_view.set_playhead_seconds(0.0);
    };
    addAndMakeVisible(m_seek_start_button);

    m_seek_end_button.onClick = [this] {
        if (m_session.source_audio().total_frames > 0) {
            double end_sec = static_cast<double>(m_session.source_audio().total_frames) / m_session.source_audio().sample_rate;
            m_player.seek_seconds(end_sec);
            m_waveform_view.set_playhead_seconds(end_sec);
        }
    };
    addAndMakeVisible(m_seek_end_button);

    // A/B Segmented Controls
    m_source_a_button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D3748));
    m_source_a_button.onClick = [this] {
        m_player.set_active_source(PlaybackSource::SourceA_Original);
        m_source_a_button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF3182CE));
        m_source_b_button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D3748));
    };
    addAndMakeVisible(m_source_a_button);

    m_source_b_button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF3182CE));
    m_source_b_button.onClick = [this] {
        m_player.set_active_source(PlaybackSource::SourceB_Corrected);
        m_source_b_button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF3182CE));
        m_source_a_button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D3748));
    };
    addAndMakeVisible(m_source_b_button);

    m_metronome_toggle.onStateChange = [this] {
        m_player.set_metronome_enabled(m_metronome_toggle.getToggleState());
    };
    addAndMakeVisible(m_metronome_toggle);

    m_time_display_label.setText("00:00.000", juce::dontSendNotification);
    m_time_display_label.setFont(juce::Font(16.0f, juce::Font::bold));
    addAndMakeVisible(m_time_display_label);

    addAndMakeVisible(m_render_progress);

    m_export_button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF38A169)); // Green
    m_export_button.onClick = [this] {
        if (!m_export_modal) {
            m_export_modal = std::make_unique<ExportModal>(&m_session, [this] {
                removeChildComponent(m_export_modal.get());
                m_export_modal.reset();
            });
            addAndMakeVisible(m_export_modal.get());
            resized();
        }
    };
    addAndMakeVisible(m_export_button);

    setSize(1280, 800);
    startTimerHz(30); // 30 Hz UI timer for time display and progress
}

MainComponent::~MainComponent() {
    stopTimer();
    m_player.shutdown();
}

void MainComponent::timerCallback() {
    double pos = m_player.get_position_seconds();
    m_waveform_view.set_playhead_seconds(pos);

    int mins = static_cast<int>(pos) / 60;
    double secs = std::fmod(pos, 60.0);
    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << mins << ":"
       << std::setfill('0') << std::setw(6) << std::fixed << std::setprecision(3) << secs;
    m_time_display_label.setText(ss.str(), juce::dontSendNotification);

    m_undo_button.setEnabled(m_session.can_undo());
    m_redo_button.setEnabled(m_session.can_redo());
}

bool MainComponent::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::spaceKey) {
        m_play_pause_button.triggerClick();
        return true;
    } else if (key.getTextCharacter() == 't' || key.getTextCharacter() == 'T') {
        if (m_player.get_active_source() == PlaybackSource::SourceA_Original) {
            m_source_b_button.triggerClick();
        } else {
            m_source_a_button.triggerClick();
        }
        return true;
    } else if (key.getTextCharacter() == 'k' || key.getTextCharacter() == 'K') {
        m_metronome_toggle.setToggleState(!m_metronome_toggle.getToggleState(), juce::sendNotification);
        return true;
    }
    return false;
}

void MainComponent::resized() {
    auto area = getLocalBounds();

    // 1. Header (40 px)
    auto header = area.removeFromTop(40).reduced(8, 4);
    m_open_button.setBounds(header.removeFromLeft(110));
    header.removeFromLeft(8);
    m_save_button.setBounds(header.removeFromLeft(100));
    header.removeFromLeft(16);
    m_undo_button.setBounds(header.removeFromRight(70));
    header.removeFromRight(8);
    m_redo_button.setBounds(header.removeFromRight(70));
    header.removeFromRight(16);
    m_file_info_label.setBounds(header);

    // 2. Control Strip (64 px)
    auto ctrl = area.removeFromTop(64).reduced(8, 6);
    auto ctrl_top = ctrl.removeFromTop(26);
    m_detected_label.setBounds(ctrl_top.removeFromLeft(180));
    m_confidence_badge.setBounds(ctrl_top.removeFromLeft(100));
    ctrl_top.removeFromLeft(24);
    m_target_label.setBounds(ctrl_top.removeFromLeft(64));
    m_target_bpm_slider.setBounds(ctrl_top.removeFromLeft(100));
    ctrl_top.removeFromLeft(4);
    m_reset_bpm_button.setBounds(ctrl_top.removeFromLeft(28));
    ctrl_top.removeFromLeft(24);
    m_diag_label.setBounds(ctrl_top);

    auto ctrl_bot = ctrl;
    m_detent_label.setBounds(ctrl_bot.removeFromLeft(200));
    m_detent_slider.setBounds(ctrl_bot.removeFromLeft(160));
    ctrl_bot.removeFromLeft(24);
    m_mode_combo.setBounds(ctrl_bot.removeFromLeft(180));

    // 3. Transport Bar (50 px at bottom)
    auto transport = area.removeFromBottom(50).reduced(8, 6);
    m_seek_start_button.setBounds(transport.removeFromLeft(36));
    transport.removeFromLeft(4);
    m_play_pause_button.setBounds(transport.removeFromLeft(84));
    transport.removeFromLeft(4);
    m_stop_button.setBounds(transport.removeFromLeft(74));
    transport.removeFromLeft(4);
    m_seek_end_button.setBounds(transport.removeFromLeft(36));
    transport.removeFromLeft(20);
    m_time_display_label.setBounds(transport.removeFromLeft(110));
    transport.removeFromLeft(20);
    m_source_a_button.setBounds(transport.removeFromLeft(100));
    transport.removeFromLeft(4);
    m_source_b_button.setBounds(transport.removeFromLeft(110));
    transport.removeFromLeft(16);
    m_metronome_toggle.setBounds(transport.removeFromLeft(90));
    transport.removeFromLeft(20);
    m_export_button.setBounds(transport.removeFromRight(90));
    transport.removeFromRight(16);
    m_render_progress.setBounds(transport.removeFromRight(150));

    // 4. Waveform View (Remaining central area)
    m_waveform_view.setBounds(area);

    if (m_export_modal) {
        m_export_modal->setCentrePosition(getWidth() / 2, getHeight() / 2);
    }
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF1A1D24)); // Main UI Background
}

void MainComponent::open_file_dialog() {
    juce::FileChooser chooser("Select Audio File", {}, "*.wav;*.flac;*.mp3;*.aif;*.aiff");
    if (chooser.browseForFileToOpen()) {
        auto file = chooser.getResult();
        std::string path = file.getFullPathName().toStdString();

        auto open_res = m_session.open_audio_file(path);
        if (!open_res.is_ok()) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon, "Load Error", open_res.error_message());
            return;
        }

        auto ana_res = m_session.run_analysis();
        if (!ana_res.is_ok()) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon, "Analysis Error", ana_res.error_message());
            return;
        }

        m_target_bpm_slider.setValue(m_session.target_bpm(), juce::dontSendNotification);
        update_ui_from_session();
        m_waveform_view.set_session(&m_session);
        m_waveform_view.zoom_fit();

        trigger_render();
    }
}

void MainComponent::save_project_dialog() {
    if (m_session.source_audio().is_empty()) return;
    juce::FileChooser chooser("Save Project", {}, "*.beatfix");
    if (chooser.browseForFileToSave(true)) {
        auto file = chooser.getResult();
        auto save_res = m_session.save_project(file.getFullPathName().toStdString());
        if (!save_res.is_ok()) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon, "Save Error", save_res.error_message());
        }
    }
}

void MainComponent::update_ui_from_session() {
    if (m_session.source_audio().is_empty()) return;

    std::filesystem::path p(m_session.source_audio().source_path);
    std::ostringstream ss;
    ss << p.filename().string() << " | "
       << (m_session.source_audio().channels == 2 ? "Stereo" : "Mono") << " | "
       << static_cast<int>(m_session.source_audio().sample_rate) << " Hz";
    m_file_info_label.setText(ss.str(), juce::dontSendNotification);

    if (m_session.is_analyzed()) {
        std::ostringstream bss;
        bss << "DETECTED: " << std::fixed << std::setprecision(2) << m_session.analysis().bpm_global << " BPM";
        m_detected_label.setText(bss.str(), juce::dontSendNotification);

        std::string conf_str = "●●●○ High";
        if (m_session.analysis().global_confidence == analysis::GlobalConfidence::Medium) conf_str = "●●○○ Med";
        if (m_session.analysis().global_confidence == analysis::GlobalConfidence::Low) conf_str = "●○○○ Low";
        m_confidence_badge.setText(conf_str, juce::dontSendNotification);

        const auto& diag = m_session.warp_map().diagnostics();
        std::ostringstream dss;
        dss << "Ratio: [" << std::fixed << std::setprecision(2) << diag.min_ratio
            << ".." << diag.max_ratio << "] | Warn: " << diag.num_warn_segments;
        m_diag_label.setText(dss.str(), juce::dontSendNotification);
    }

    m_player.set_sources(&m_session.source_audio(), &m_session.corrected_audio(), &m_session.warp_map());
}

void MainComponent::trigger_render() {
    m_render_fraction = 0.0;
    // Launch background thread to render full corrected audio without stalling UI
    std::thread([this] {
        m_session.render_corrected([this](const engine::RenderProgress& prog) {
            m_render_fraction = prog.progress_fraction;
        });
        juce::MessageManager::callAsync([this] {
            m_render_fraction = 1.0;
            m_player.set_sources(&m_session.source_audio(), &m_session.corrected_audio(), &m_session.warp_map());
            m_waveform_view.repaint();
        });
    }).detach();
}

} // namespace bf::gui
