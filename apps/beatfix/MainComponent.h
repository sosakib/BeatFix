#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "AudioPlayer.h"
#include "WaveformView.h"
#include "ExportModal.h"
#include "BeatFixLookAndFeel.h"
#include "../../src/bf_engine/session.h"
#include <memory>
#include <atomic>

namespace bf::gui {

class MainComponent final : public juce::Component,
                            public juce::Timer,
                            public juce::FileDragAndDropTarget {
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Keyboard shortcuts
    bool keyPressed(const juce::KeyPress& key) override;

    // Timer callback for transport time and render status
    void timerCallback() override;

    // File Drag and Drop Target implementation
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void load_audio_file(const std::string& path);

private:
    void open_file_dialog();
    void save_project_dialog();
    void update_ui_from_session();
    void trigger_render();

    BeatFixLookAndFeel m_look_and_feel;

    bool m_is_drag_hovering = false;
    std::atomic<uint64_t> m_render_generation{0};

    engine::Session m_session;
    AudioPlayer m_player;

    // Track metadata
    juce::String m_track_title{"No file loaded"};
    juce::String m_track_specs{"Drag audio here • WAV, FLAC, MP3"};

    // Header Controls
    juce::TextButton m_open_button{"📁 Open"};
    juce::TextButton m_save_button{"💾 Save"};
    juce::TextButton m_menu_button{"⁝"};

    // Dashboard Cards Controls
    juce::TextButton m_card_track_btn; // Overlay button on track card to open file
    juce::Slider m_target_bpm_slider;
    juce::TextButton m_bpm_up_btn{"▲"};
    juce::TextButton m_bpm_down_btn{"▼"};
    juce::ComboBox m_mode_combo;

    // Waveform View
    WaveformView m_waveform_view;

    // Transport Bar Controls
    juce::TextButton m_play_button{"▶ Play"};
    juce::TextButton m_pause_button{"❚❚ Pause"};
    juce::TextButton m_stop_button{"⏹ Stop"};
    juce::Label m_time_display_label;
    juce::TextButton m_source_a_button{"A: Original"};
    juce::TextButton m_source_b_button{"B: Corrected"};
    juce::ToggleButton m_metronome_toggle{"Click (K)"};
    juce::TextButton m_export_button{"⤓ Export ⌵"};
    double m_render_fraction = 0.0;
    juce::ProgressBar m_render_progress{m_render_fraction};

    // Modal Export Component
    std::unique_ptr<ExportModal> m_export_modal;
};

} // namespace bf::gui
