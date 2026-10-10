#include "MainComponent.h"
#include "../../src/bf_io/path_utf8.h"
#include "../../src/bf_dsp/stretch.h"
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>

namespace bf::gui {

MainComponent::MainComponent() {
    setLookAndFeel(&m_look_and_feel);
    m_player.init();
    setWantsKeyboardFocus(true);

    // --- Header ---
    m_open_button.onClick = [this] { open_file_dialog(); };
    addAndMakeVisible(m_open_button);

    m_save_button.onClick = [this] { save_project_dialog(); };
    addAndMakeVisible(m_save_button);

    m_menu_button.onClick = [this] { open_file_dialog(); };
    addAndMakeVisible(m_menu_button);

    // --- Dashboard Cards Controls ---
    m_card_track_btn.onClick = [this] { open_file_dialog(); };
    m_card_track_btn.setAlpha(0.0f); // Invisible click overlay on card 1
    addAndMakeVisible(m_card_track_btn);

    m_target_bpm_slider.setRange(40.0, 300.0, 0.1);
    m_target_bpm_slider.setValue(120.0);
    m_target_bpm_slider.setSliderStyle(juce::Slider::LinearBar);
    m_target_bpm_slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
    m_target_bpm_slider.onValueChange = [this] {
        if (m_session.is_analyzed() && std::abs(m_session.target_bpm() - m_target_bpm_slider.getValue()) > 0.05) {
            m_session.set_target_bpm(m_target_bpm_slider.getValue());
            m_waveform_view.repaint();
            trigger_render();
        }
    };
    addAndMakeVisible(m_target_bpm_slider);

    m_bpm_up_btn.onClick = [this] {
        m_target_bpm_slider.setValue(m_target_bpm_slider.getValue() + 1.0);
    };
    addAndMakeVisible(m_bpm_up_btn);

    m_bpm_down_btn.onClick = [this] {
        m_target_bpm_slider.setValue(m_target_bpm_slider.getValue() - 1.0);
    };
    addAndMakeVisible(m_bpm_down_btn);

    m_mode_combo.addItem("Smooth (Signalsmith)", 1);
    m_mode_combo.addItem("Resample (Varispeed)", 2);
    m_mode_combo.setSelectedId(1, juce::dontSendNotification);
    m_mode_combo.onChange = [this] {
        if (m_session.is_analyzed()) {
            bf::dsp::StretchEngineMode sm = (m_mode_combo.getSelectedId() == 2) ?
                                  bf::dsp::StretchEngineMode::Resample : bf::dsp::StretchEngineMode::Smooth;
            m_session.set_mode(sm);
            trigger_render();
        }
    };
    addAndMakeVisible(m_mode_combo);

    // --- Waveform View ---
    addAndMakeVisible(m_waveform_view);
    m_waveform_view.on_seek = [this](double seek_sec) {
        m_player.seek_seconds(seek_sec);
    };

    // --- Transport Bar ---
    m_play_button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF161B26));
    m_play_button.onClick = [this] { m_player.play(); };
    addAndMakeVisible(m_play_button);

    m_pause_button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF161B26));
    m_pause_button.onClick = [this] { m_player.pause(); };
    addAndMakeVisible(m_pause_button);

    m_stop_button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF161B26));
    m_stop_button.onClick = [this] { m_player.stop(); };
    addAndMakeVisible(m_stop_button);

    m_time_display_label.setText("00:00.000 / 00:00.000", juce::dontSendNotification);
    m_time_display_label.setFont(juce::Font(15.0f, juce::Font::bold));
    m_time_display_label.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(m_time_display_label);

    m_source_a_button.onClick = [this] {
        m_player.set_active_source(PlaybackSource::SourceA_Original);
        m_source_a_button.getProperties().set("accentRed", true);
        m_source_b_button.getProperties().remove("accentRed");
        m_waveform_view.set_active_source(false);
        m_source_a_button.repaint();
        m_source_b_button.repaint();
    };
    addAndMakeVisible(m_source_a_button);

    m_source_b_button.onClick = [this] {
        m_player.set_active_source(PlaybackSource::SourceB_Corrected);
        m_source_b_button.getProperties().set("accentRed", true);
        m_source_a_button.getProperties().remove("accentRed");
        m_waveform_view.set_active_source(true);
        m_source_a_button.repaint();
        m_source_b_button.repaint();
    };
    // Default to B: Corrected as accented
    m_source_b_button.getProperties().set("accentRed", true);
    addAndMakeVisible(m_source_b_button);

    m_metronome_toggle.onStateChange = [this] {
        m_player.set_metronome_enabled(m_metronome_toggle.getToggleState());
    };
    m_metronome_toggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xFFE2E8F0));
    addAndMakeVisible(m_metronome_toggle);

    addAndMakeVisible(m_render_progress);

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
    startTimerHz(30); // 30 Hz UI refresh timer
}

MainComponent::~MainComponent() {
    stopTimer();
    m_player.shutdown();
    setLookAndFeel(nullptr);
}

void MainComponent::timerCallback() {
    double pos = m_player.get_position_seconds();
    m_waveform_view.set_playhead_seconds(pos);

    // Time display: current / total
    int mins = static_cast<int>(pos) / 60;
    double secs = std::fmod(pos, 60.0);

    double total_dur = 0.0;
    if (!m_session.source_audio().is_empty() && m_session.source_audio().sample_rate > 0) {
        total_dur = static_cast<double>(m_session.source_audio().total_frames) / m_session.source_audio().sample_rate;
    }
    int t_mins = static_cast<int>(total_dur) / 60;
    double t_secs = std::fmod(total_dur, 60.0);

    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << mins << ":"
       << std::setfill('0') << std::setw(6) << std::fixed << std::setprecision(3) << secs
       << "  /  "
       << std::setfill('0') << std::setw(2) << t_mins << ":"
       << std::setfill('0') << std::setw(6) << std::fixed << std::setprecision(3) << t_secs;
    m_time_display_label.setText(ss.str(), juce::dontSendNotification);
}

bool MainComponent::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::spaceKey) {
        if (m_player.is_playing()) m_player.pause();
        else m_player.play();
        return true;
    } else if (key.getTextCharacter() == 'a' || key.getTextCharacter() == 'A') {
        m_source_a_button.triggerClick();
        return true;
    } else if (key.getTextCharacter() == 'b' || key.getTextCharacter() == 'B') {
        m_source_b_button.triggerClick();
        return true;
    } else if (key.getTextCharacter() == 'k' || key.getTextCharacter() == 'K') {
        m_metronome_toggle.setToggleState(!m_metronome_toggle.getToggleState(), juce::sendNotification);
        return true;
    }
    return false;
}

void MainComponent::resized() {
    auto area = getLocalBounds();

    // 1. Top Header Bar (44 px)
    auto header = area.removeFromTop(44).reduced(16, 6);
    header.removeFromLeft(160); // Space for Brand logo
    m_menu_button.setBounds(header.removeFromRight(28));
    header.removeFromRight(8);
    m_save_button.setBounds(header.removeFromRight(84));
    header.removeFromRight(8);
    m_open_button.setBounds(header.removeFromRight(84));

    // 2. Dashboard Cards Strip (72 px)
    auto cards_strip = area.removeFromTop(72).reduced(14, 4);
    int total_w = cards_strip.getWidth();
    int card_gap = 8;
    int num_cards = 6;
    int card_w = (total_w - (num_cards - 1) * card_gap) / num_cards;

    // Card 1: Track info
    auto c1 = cards_strip.removeFromLeft(card_w);
    m_card_track_btn.setBounds(c1);
    cards_strip.removeFromLeft(card_gap);

    // Card 2: Detected BPM
    auto c2 = cards_strip.removeFromLeft(card_w);
    cards_strip.removeFromLeft(card_gap);

    // Card 3: Target BPM
    auto c3 = cards_strip.removeFromLeft(card_w);
    auto c3_ctrl = c3.reduced(10, 8);
    c3_ctrl.removeFromTop(20); // Label height
    m_bpm_down_btn.setBounds(c3_ctrl.removeFromLeft(20).withHeight(24));
    m_bpm_up_btn.setBounds(c3_ctrl.removeFromRight(20).withHeight(24));
    m_target_bpm_slider.setBounds(c3_ctrl.withHeight(24));
    cards_strip.removeFromLeft(card_gap);

    // Card 4: Ratio
    auto c4 = cards_strip.removeFromLeft(card_w);
    cards_strip.removeFromLeft(card_gap);

    // Card 5: Warnings
    auto c5 = cards_strip.removeFromLeft(card_w);
    cards_strip.removeFromLeft(card_gap);

    // Card 6: Smoothing
    auto c6 = cards_strip;
    auto c6_ctrl = c6.reduced(10, 8);
    c6_ctrl.removeFromTop(20);
    m_mode_combo.setBounds(c6_ctrl.withHeight(24));

    // 3. Bottom Transport Bar (56 px)
    auto transport = area.removeFromBottom(56).reduced(14, 8);
    m_play_button.setBounds(transport.removeFromLeft(70));
    transport.removeFromLeft(6);
    m_pause_button.setBounds(transport.removeFromLeft(76));
    transport.removeFromLeft(6);
    m_stop_button.setBounds(transport.removeFromLeft(70));
    transport.removeFromLeft(16);

    m_time_display_label.setBounds(transport.removeFromLeft(220));
    transport.removeFromLeft(16);

    m_source_a_button.setBounds(transport.removeFromLeft(96));
    transport.removeFromLeft(4);
    m_source_b_button.setBounds(transport.removeFromLeft(104));
    transport.removeFromLeft(16);

    m_metronome_toggle.setBounds(transport.removeFromLeft(86));
    transport.removeFromLeft(16);

    m_render_progress.setBounds(transport.removeFromRight(130));
    transport.removeFromRight(12);
    m_export_button.setBounds(transport.removeFromRight(96));

    // 4. Waveform Area (remaining space)
    area.removeFromTop(6);
    area.removeFromBottom(6);
    m_waveform_view.setBounds(area.reduced(14, 0));

    if (m_export_modal) {
        m_export_modal->setBounds(getLocalBounds());
    }
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF0B0E14)); // Deep matte obsidian background

    // --- Brand Logo (Top-Left) ---
    float lx = 16.0f, ly = 13.0f, lw = 16.0f, lh = 16.0f;
    g.setColour(juce::Colour(0xFFFF334B));
    juce::Path p1, p2;
    p1.addLineSegment(juce::Line<float>(lx, ly, lx + lw, ly + lh), 3.5f);
    p2.addLineSegment(juce::Line<float>(lx, ly + lh, lx + lw, ly), 3.5f);
    g.strokePath(p1, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.strokePath(p2, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("BeatFix", lx + lw + 8, 10, 100, 24, juce::Justification::left);

    // --- Dashboard Cards Rendering ---
    auto cards_area = juce::Rectangle<int>(14, 48, getWidth() - 28, 64);
    int total_w = cards_area.getWidth();
    int card_gap = 8;
    int num_cards = 6;
    int card_w = (total_w - (num_cards - 1) * card_gap) / num_cards;

    for (int i = 0; i < num_cards; ++i) {
        auto card_rect = juce::Rectangle<int>(cards_area.getX() + i * (card_w + card_gap), cards_area.getY(), card_w, cards_area.getHeight());
        g.setColour(juce::Colour(0xFF121620));
        g.fillRoundedRectangle(card_rect.toFloat(), 8.0f);
        g.setColour(juce::Colour(0xFF1F2633));
        g.drawRoundedRectangle(card_rect.toFloat(), 8.0f, 1.0f);

        auto content = card_rect.reduced(10, 8);

        if (i == 0) {
            // Card 1: Track Details
            g.setColour(juce::Colour(0xFF1A2230));
            g.fillEllipse(static_cast<float>(content.getX()), static_cast<float>(content.getY() + 4), 28.0f, 28.0f);
            g.setColour(juce::Colour(0xFFFF334B));
            g.setFont(juce::Font(14.0f, juce::Font::bold));
            g.drawText("♫", content.getX(), content.getY() + 4, 28, 28, juce::Justification::centred);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(12.5f, juce::Font::bold));
            g.drawText(m_track_title, content.getX() + 34, content.getY() + 2, content.getWidth() - 44, 16, juce::Justification::left, true);

            g.setColour(juce::Colour(0xFF718096));
            g.setFont(juce::Font(10.5f, juce::Font::plain));
            g.drawText(m_track_specs, content.getX() + 34, content.getY() + 20, content.getWidth() - 44, 14, juce::Justification::left, true);

            g.drawText("⌵", content.getRight() - 10, content.getY() + 10, 10, 14, juce::Justification::right);
        } else if (i == 1) {
            // Card 2: Detected BPM
            g.setColour(juce::Colour(0xFF718096));
            g.setFont(juce::Font(11.0f, juce::Font::plain));
            g.drawText("Detected BPM", content.getX(), content.getY(), content.getWidth(), 14, juce::Justification::left);

            g.setColour(juce::Colour(0xFFFF334B));
            g.drawText("◎", content.getX(), content.getY() + 18, 16, 20, juce::Justification::left);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(16.0f, juce::Font::bold));
            juce::String bpmStr = m_session.is_analyzed() ?
                                  juce::String::formatted("%.2f", m_session.analysis().bpm_global) : "---";
            g.drawText(bpmStr, content.getX() + 20, content.getY() + 18, 60, 20, juce::Justification::left);

            g.setColour(juce::Colour(0xFF718096));
            g.setFont(juce::Font(11.0f, juce::Font::plain));
            g.drawText("BPM", content.getX() + 82, content.getY() + 20, 30, 16, juce::Justification::left);
        } else if (i == 2) {
            // Card 3: Target BPM Label
            g.setColour(juce::Colour(0xFF718096));
            g.setFont(juce::Font(11.0f, juce::Font::plain));
            g.drawText("Target BPM", content.getX(), content.getY(), content.getWidth(), 14, juce::Justification::left);
        } else if (i == 3) {
            // Card 4: Ratio
            g.setColour(juce::Colour(0xFF718096));
            g.setFont(juce::Font(11.0f, juce::Font::plain));
            g.drawText("Ratio ⓘ", content.getX(), content.getY(), content.getWidth(), 14, juce::Justification::left);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(15.0f, juce::Font::bold));
            juce::String ratioStr = m_session.is_analyzed() ?
                                    juce::String::formatted("%.2f..%.2f", m_session.warp_map().diagnostics().min_ratio, m_session.warp_map().diagnostics().max_ratio) : "1.00..1.00";
            g.drawText(ratioStr, content.getX(), content.getY() + 18, content.getWidth(), 20, juce::Justification::left);
        } else if (i == 4) {
            // Card 5: Warnings
            g.setColour(juce::Colour(0xFF718096));
            g.setFont(juce::Font(11.0f, juce::Font::plain));
            g.drawText("Warnings", content.getX(), content.getY(), content.getWidth(), 14, juce::Justification::left);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(15.0f, juce::Font::bold));
            int warnCount = m_session.is_analyzed() ? static_cast<int>(m_session.warp_map().diagnostics().num_extreme_segments) : 0;
            g.drawText(juce::String(warnCount), content.getX(), content.getY() + 18, 20, 20, juce::Justification::left);

            // Green/Red status dot
            juce::Colour dotCol = (warnCount == 0) ? juce::Colour(0xFF2ECC71) : juce::Colour(0xFFFF334B);
            g.setColour(dotCol);
            g.fillEllipse(static_cast<float>(content.getX() + 24), static_cast<float>(content.getY() + 25), 6.0f, 6.0f);
        } else if (i == 5) {
            // Card 6: Smoothing Label
            g.setColour(juce::Colour(0xFF718096));
            g.setFont(juce::Font(11.0f, juce::Font::plain));
            g.drawText("📈 Smoothing", content.getX(), content.getY(), content.getWidth(), 14, juce::Justification::left);
        }
    }

    // --- Bottom Transport Container Bar ---
    auto transport_rect = juce::Rectangle<int>(0, getHeight() - 56, getWidth(), 56);
    g.setColour(juce::Colour(0xFF121620));
    g.fillRect(transport_rect);
    g.setColour(juce::Colour(0xFF1F2633));
    g.drawHorizontalLine(transport_rect.getY(), 0.0f, static_cast<float>(getWidth()));

    // --- Drag Hover Overlay ---
    if (m_is_drag_hovering) {
        g.setColour(juce::Colour(0xD00B0E14));
        g.fillAll();

        auto inner = getLocalBounds().reduced(32);
        g.setColour(juce::Colour(0xFFFF334B));
        g.drawRoundedRectangle(inner.toFloat(), 12.0f, 2.0f);

        auto top_half = inner.removeFromTop(inner.getHeight() / 2);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(24.0f, juce::Font::bold));
        g.drawText("Drop Audio File to Analyze & Fix Timing",
                   top_half, juce::Justification::centredBottom, true);

        g.setColour(juce::Colour(0xFF8A99AD));
        g.setFont(juce::Font(15.0f, juce::Font::plain));
        g.drawText("Supported formats: WAV, FLAC, MP3, AIFF, or .beatfix projects",
                   inner, juce::Justification::centredTop, true);
    }
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files) {
    for (const auto& f : files) {
        auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".wave" || ext == ".flac" || ext == ".mp3" || ext == ".aif" || ext == ".aiff" || ext == ".beatfix") {
            return true;
        }
    }
    return false;
}

void MainComponent::fileDragEnter(const juce::StringArray& /*files*/, int /*x*/, int /*y*/) {
    m_is_drag_hovering = true;
    repaint();
}

void MainComponent::fileDragMove(const juce::StringArray& /*files*/, int /*x*/, int /*y*/) {}

void MainComponent::fileDragExit(const juce::StringArray& /*files*/) {
    m_is_drag_hovering = false;
    repaint();
}

void MainComponent::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/) {
    m_is_drag_hovering = false;
    repaint();

    if (!files.isEmpty()) {
        load_audio_file(files[0].toStdString());
    }
}

void MainComponent::load_audio_file(const std::string& path) {
    m_player.stop();

    std::string ext = bf::io::get_extension_lower(path);

    if (ext == ".beatfix") {
        auto load_res = m_session.load_project(path);
        if (!load_res.is_ok()) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon, "Load Error", load_res.error_message());
            return;
        }
    } else {
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
    }

    m_target_bpm_slider.setValue(m_session.target_bpm(), juce::dontSendNotification);
    update_ui_from_session();
    m_waveform_view.set_session(&m_session);
    m_waveform_view.zoom_fit();

    trigger_render();
}

void MainComponent::open_file_dialog() {
    juce::FileChooser chooser("Select Audio File", {}, "*.wav;*.flac;*.mp3;*.aif;*.aiff;*.beatfix");
    if (chooser.browseForFileToOpen()) {
        auto file = chooser.getResult();
        load_audio_file(file.getFullPathName().toStdString());
    }
}

void MainComponent::save_project_dialog() {
    if (!m_session.is_analyzed()) {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon, "Save Project", "No analyzed project to save.");
        return;
    }
    juce::FileChooser chooser("Save BeatFix Project", {}, "*.beatfix");
    if (chooser.browseForFileToSave(true)) {
        auto file = chooser.getResult().withFileExtension(".beatfix");
        auto save_res = m_session.save_project(file.getFullPathName().toStdString());
        if (!save_res.is_ok()) {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon, "Save Error", save_res.error_message());
        }
    }
}

void MainComponent::update_ui_from_session() {
    if (m_session.source_audio().sample_rate > 0) {
        auto p = bf::io::path_from_utf8(m_session.source_audio().source_path);
        m_track_title = bf::io::wide_to_utf8(p.stem().wstring());

        std::string ext = bf::io::get_extension_lower(m_session.source_audio().source_path);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
        if (ext.rfind(".", 0) == 0) ext = ext.substr(1);

        double sr_khz = m_session.source_audio().sample_rate / 1000.0;
        juce::String chStr = (m_session.source_audio().channels == 2) ? "Stereo" : "Mono";
        m_track_specs = juce::String::formatted("%s • %.1f kHz • %s", ext.c_str(), sr_khz, chStr.toRawUTF8());

        m_target_bpm_slider.setValue(m_session.target_bpm(), juce::dontSendNotification);
        m_player.set_sources(m_session.source_audio_shared(), m_session.corrected_audio_shared(), &m_session.warp_map());
    }
    repaint();
}

void MainComponent::trigger_render() {
    uint64_t gen = ++m_render_generation;
    m_render_fraction = 0.05;

    std::thread([this, gen] {
        m_session.render_corrected([this, gen](const engine::RenderProgress& prog) {
            if (gen == m_render_generation.load()) {
                m_render_fraction = prog.progress_fraction;
            }
        });

        if (gen == m_render_generation.load()) {
            juce::MessageManager::callAsync([this, gen] {
                if (gen == m_render_generation.load()) {
                    m_render_fraction = 1.0;
                    m_player.set_sources(m_session.source_audio_shared(), m_session.corrected_audio_shared(), &m_session.warp_map());
                    m_waveform_view.repaint();
                    repaint();
                }
            });
        }
    }).detach();
}

} // namespace bf::gui
