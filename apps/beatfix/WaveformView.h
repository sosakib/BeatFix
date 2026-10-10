#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_events/juce_events.h>
#include "../../src/bf_dsp/peaks.h"
#include "../../src/bf_dsp/warp.h"
#include "../../src/bf_engine/session.h"
#include <functional>

namespace bf::gui {

class WaveformView final : public juce::Component, public juce::Timer {
public:
    WaveformView();
    ~WaveformView() override;

    void set_session(engine::Session* session);
    void set_playhead_seconds(double pos_sec);
    void set_active_source(bool is_corrected);

    std::function<void(double seek_sec)> on_seek;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // Timer override for smooth playhead animation
    void timerCallback() override;

    void zoom_in();
    void zoom_out();
    void zoom_fit();

    double get_view_start_seconds() const noexcept { return m_view_start_sec; }
    double get_view_duration_seconds() const noexcept { return m_view_duration_sec; }
    void set_view_window(double start_sec, double duration_sec);

private:
    void draw_overview_lane(juce::Graphics& g, juce::Rectangle<int> bounds);
    void draw_detail_lane(juce::Graphics& g, juce::Rectangle<int> bounds);
    void draw_playhead(juce::Graphics& g);

    juce::Colour get_correction_class_colour(int correction_class) const;

    engine::Session* m_session = nullptr;

    double m_playhead_sec = 0.0;
    double m_view_start_sec = 0.0;
    double m_view_duration_sec = 30.0; // Visible window duration in zoomed detail view

    bool m_is_corrected_source = true;

    // Layout areas
    juce::Rectangle<int> m_overview_rect;
    juce::Rectangle<int> m_detail_rect;
    juce::Rectangle<int> m_detail_header_rect;
    juce::Rectangle<int> m_detail_wave_rect;

    // Interaction state for overview selection window
    enum class DragMode { None, OverviewPan, OverviewLeftHandle, OverviewRightHandle, DetailSeek };
    DragMode m_drag_mode = DragMode::None;
    float m_drag_start_mouse_x = 0.0f;
    double m_drag_initial_start_sec = 0.0;
    double m_drag_initial_dur_sec = 30.0;

    // Detail lane zoom UI buttons
    juce::TextButton m_zoom_out_btn{"−"};
    juce::TextButton m_zoom_reset_btn{"100%"};
    juce::TextButton m_zoom_in_btn{"+"};
};

} // namespace bf::gui
