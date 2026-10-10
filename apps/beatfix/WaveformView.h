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

    std::function<void(double seek_sec)> on_seek;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // Timer override for smooth playhead animation
    void timerCallback() override;

    void zoom_in();
    void zoom_out();
    void zoom_fit();

private:
    void draw_original_lane(juce::Graphics& g, juce::Rectangle<int> bounds);
    void draw_gutter(juce::Graphics& g, juce::Rectangle<int> bounds);
    void draw_corrected_lane(juce::Graphics& g, juce::Rectangle<int> bounds);
    void draw_playhead(juce::Graphics& g);

    juce::Colour get_correction_class_colour(int correction_class) const;

    engine::Session* m_session = nullptr;

    double m_playhead_sec = 0.0;
    double m_view_start_sec = 0.0;
    double m_view_duration_sec = 10.0; // Visible window duration

    int m_lane_height = 120;
    int m_gutter_height = 28;
    int m_ruler_height = 20;

    juce::Rectangle<int> m_orig_rect;
    juce::Rectangle<int> m_gutter_rect;
    juce::Rectangle<int> m_corr_rect;
};

} // namespace bf::gui
