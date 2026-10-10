#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "AudioPlayer.h"
#include "WaveformView.h"
#include "ExportModal.h"
#include "../../src/bf_engine/session.h"
#include <memory>

namespace bf::gui {

class MainComponent final : public juce::Component, public juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Keyboard shortcuts
    bool keyPressed(const juce::KeyPress& key) override;

    // Timer callback for transport time and render status
    void timerCallback() override;

private:
    void open_file_dialog();
    void save_project_dialog();
    void update_ui_from_session();
    void trigger_render();

    engine::Session m_session;
    AudioPlayer m_player;

    // Header Controls
    juce::TextButton m_open_button{"Open Audio..."};
    juce::TextButton m_save_button{"Save Project"};
    juce::Label m_file_info_label;
    juce::TextButton m_undo_button{"↶ Undo"};
    juce::TextButton m_redo_button{"↷ Redo"};

    // Control Strip
    juce::Label m_detected_label;
    juce::Label m_confidence_badge;
    juce::Label m_target_label;
    juce::Slider m_target_bpm_slider;
    juce::TextButton m_reset_bpm_button{"↺"};
    juce::Label m_detent_label;
    juce::Slider m_detent_slider;
    juce::ComboBox m_mode_combo;
    juce::Label m_diag_label;

    // Central Waveform
    WaveformView m_waveform_view;

    // Transport Bar
    juce::TextButton m_play_pause_button{"▶ Play"};
    juce::TextButton m_stop_button{"⏹ Stop"};
    juce::TextButton m_seek_start_button{"⏮"};
    juce::TextButton m_seek_end_button{"⏭"};
    juce::TextButton m_source_a_button{"A: Original"};
    juce::TextButton m_source_b_button{"B: Corrected"};
    juce::ToggleButton m_metronome_toggle{"Click (K)"};
    juce::Label m_time_display_label;
    juce::ProgressBar m_render_progress{m_render_fraction};
    double m_render_fraction = 0.0;
    juce::TextButton m_export_button{"Export..."};

    // Modal Export Component
    std::unique_ptr<ExportModal> m_export_modal;
};

} // namespace bf::gui
