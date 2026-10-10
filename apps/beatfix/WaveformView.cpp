#include "WaveformView.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace bf::gui {

WaveformView::WaveformView() {
    startTimerHz(60); // 60 FPS playhead refresh
}

WaveformView::~WaveformView() {
    stopTimer();
}

void WaveformView::set_session(engine::Session* session) {
    m_session = session;
    if (m_session && m_session->source_audio().sample_rate > 0 && m_session->source_audio().total_frames > 0) {
        double dur = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
        m_view_duration_sec = std::min(dur, 10.0);
    }
    repaint();
}

void WaveformView::set_playhead_seconds(double pos_sec) {
    m_playhead_sec = pos_sec;
}

void WaveformView::timerCallback() {
    // If playing past visible window, auto-scroll to follow playhead (Follow: Page)
    if (m_playhead_sec > (m_view_start_sec + m_view_duration_sec)) {
        m_view_start_sec = m_playhead_sec;
    } else if (m_playhead_sec < m_view_start_sec) {
        m_view_start_sec = std::max(0.0, m_playhead_sec);
    }
    repaint();
}

void WaveformView::resized() {
    auto area = getLocalBounds();
    int total_h = area.getHeight();
    int header_gutter_h = m_ruler_height * 2 + m_gutter_height;
    int available_h = std::max(100, total_h - header_gutter_h);
    m_lane_height = available_h / 2;

    int y = 0;
    y += m_ruler_height; // Top ruler
    m_orig_rect = juce::Rectangle<int>(0, y, area.getWidth(), m_lane_height);
    y += m_lane_height;

    m_gutter_rect = juce::Rectangle<int>(0, y, area.getWidth(), m_gutter_height);
    y += m_gutter_height;

    m_corr_rect = juce::Rectangle<int>(0, y, area.getWidth(), m_lane_height);
}

void WaveformView::zoom_in() {
    m_view_duration_sec = std::max(1.0, m_view_duration_sec * 0.75);
    repaint();
}

void WaveformView::zoom_out() {
    if (m_session && m_session->source_audio().total_frames > 0) {
        double max_dur = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
        m_view_duration_sec = std::min(max_dur, m_view_duration_sec * 1.33);
    } else {
        m_view_duration_sec *= 1.33;
    }
    repaint();
}

void WaveformView::zoom_fit() {
    if (m_session && m_session->source_audio().total_frames > 0) {
        m_view_start_sec = 0.0;
        m_view_duration_sec = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
        repaint();
    }
}

void WaveformView::mouseDown(const juce::MouseEvent& e) {
    if (e.x < 0 || getWidth() <= 0) return;
    double frac = static_cast<double>(e.x) / static_cast<double>(getWidth());
    double click_time = m_view_start_sec + (frac * m_view_duration_sec);

    if (e.y >= m_corr_rect.getY()) {
        if (on_seek) on_seek(click_time);
    } else {
        if (on_seek) on_seek(click_time);
    }
}

void WaveformView::mouseDrag(const juce::MouseEvent& e) {
    mouseDown(e);
}

void WaveformView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    if (e.mods.isCtrlDown()) {
        if (wheel.deltaY > 0) zoom_in();
        else if (wheel.deltaY < 0) zoom_out();
    } else {
        double scroll_amt = (-wheel.deltaY * 0.1) * m_view_duration_sec;
        m_view_start_sec = std::max(0.0, m_view_start_sec + scroll_amt);
        repaint();
    }
}

juce::Colour WaveformView::get_correction_class_colour(int correction_class) const {
    switch (correction_class) {
        case 0: return juce::Colour(0xFF718096); // Neutral Grey
        case 1: return juce::Colour(0xFFD69E2E); // Amber (moderate)
        case 2: return juce::Colour(0xFFDD6B20); // Orange (significant)
        case 3:
        default: return juce::Colour(0xFFE53E3E); // Red (extreme)
    }
}

void WaveformView::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF14171E)); // Dark background

    if (!m_session || m_session->source_audio().is_empty()) {
        g.setColour(juce::Colours::grey);
        g.setFont(18.0f);
        g.drawText("Drop audio here or click [Open Audio...] to begin analysis",
                   getLocalBounds(), juce::Justification::centred, true);
        return;
    }

    draw_original_lane(g, m_orig_rect);
    draw_gutter(g, m_gutter_rect);
    draw_corrected_lane(g, m_corr_rect);
    draw_playhead(g);
}

void WaveformView::draw_original_lane(juce::Graphics& g, juce::Rectangle<int> bounds) {
    g.setColour(juce::Colour(0xFF1B1F27));
    g.fillRect(bounds);

    // Border
    g.setColour(juce::Colour(0xFF2D3748));
    g.drawRect(bounds, 1);

    // Lane Label
    g.setColour(juce::Colour(0xFFA0AEC0));
    g.setFont(13.0f);
    g.drawText("ORIGINAL (Source)", bounds.getX() + 8, bounds.getY() + 6, 200, 16, juce::Justification::left);

    // Draw Waveform Peak Envelope
    const auto& pyramid = m_session->source_pyramid();
    if (!pyramid.empty() && bounds.getWidth() > 0) {
        float mid_y = bounds.getY() + (bounds.getHeight() * 0.5f);
        float half_h = (bounds.getHeight() * 0.45f);

        g.setColour(juce::Colour(0xFF4A5568)); // Neutral envelope

        double sec_per_pixel = m_view_duration_sec / bounds.getWidth();
        double sample_rate = pyramid.sample_rate();

        for (int x = 0; x < bounds.getWidth(); ++x) {
            double t0 = m_view_start_sec + (x * sec_per_pixel);
            int64_t s_idx = static_cast<int64_t>(t0 * sample_rate);

            if (s_idx >= 0 && s_idx < static_cast<int64_t>(m_session->source_audio().left.size())) {
                float sample = m_session->source_audio().left[static_cast<size_t>(s_idx)];
                float y1 = mid_y - (std::abs(sample) * half_h);
                float y2 = mid_y + (std::abs(sample) * half_h);
                g.drawVerticalLine(bounds.getX() + x, y1, y2);
            }
        }
    }

    // Draw detected beats
    const auto& analysis = m_session->analysis();
    g.setFont(10.0f);
    for (const auto& beat : analysis.beats) {
        if (beat.time_sec >= m_view_start_sec && beat.time_sec <= (m_view_start_sec + m_view_duration_sec)) {
            double frac = (beat.time_sec - m_view_start_sec) / m_view_duration_sec;
            int x = bounds.getX() + static_cast<int>(frac * bounds.getWidth());

            if (beat.is_downbeat) {
                g.setColour(juce::Colour(0xFF63B3ED)); // Downbeat Cyan
                g.drawVerticalLine(x, static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));
                // Triangle ▼
                juce::Path p;
                p.addTriangle(static_cast<float>(x - 4), static_cast<float>(bounds.getY()),
                              static_cast<float>(x + 4), static_cast<float>(bounds.getY()),
                              static_cast<float>(x), static_cast<float>(bounds.getY() + 7));
                g.fillPath(p);
                g.drawText(std::to_string(beat.bar_index + 1), x + 2, bounds.getY() + 8, 24, 12, juce::Justification::left);
            } else {
                g.setColour(juce::Colour(0x66A0AEC0)); // Beat line 40% alpha
                g.drawVerticalLine(x, static_cast<float>(bounds.getY() + 10), static_cast<float>(bounds.getBottom()));
                // Diamond ◆
                juce::Path p;
                p.startNewSubPath(static_cast<float>(x), static_cast<float>(bounds.getY() + 2));
                p.lineTo(static_cast<float>(x + 3), static_cast<float>(bounds.getY() + 5));
                p.lineTo(static_cast<float>(x), static_cast<float>(bounds.getY() + 8));
                p.lineTo(static_cast<float>(x - 3), static_cast<float>(bounds.getY() + 5));
                p.closeSubPath();
                g.fillPath(p);
            }
        }
    }
}

void WaveformView::draw_gutter(juce::Graphics& g, juce::Rectangle<int> bounds) {
    g.setColour(juce::Colour(0xFF14171E));
    g.fillRect(bounds);

    const auto& warp = m_session->warp_map();
    if (warp.anchors().empty()) return;

    // Draw slanting connectors from original beats (top) to landed beats (bottom)
    for (const auto& seg : warp.segments()) {
        double src_t = seg.src_start_sec;
        double tgt_t = seg.tgt_start_sec;

        if (src_t >= m_view_start_sec && src_t <= (m_view_start_sec + m_view_duration_sec)) {
            double frac_src = (src_t - m_view_start_sec) / m_view_duration_sec;
            double frac_tgt = (tgt_t - m_view_start_sec) / m_view_duration_sec;

            float x_top = bounds.getX() + static_cast<float>(frac_src * bounds.getWidth());
            float x_bot = bounds.getX() + static_cast<float>(frac_tgt * bounds.getWidth());

            g.setColour(get_correction_class_colour(seg.correction_class));
            g.drawLine(x_top, static_cast<float>(bounds.getY()),
                       x_bot, static_cast<float>(bounds.getBottom()), 1.5f);
        }
    }
}

void WaveformView::draw_corrected_lane(juce::Graphics& g, juce::Rectangle<int> bounds) {
    g.setColour(juce::Colour(0xFF1B1F27));
    g.fillRect(bounds);

    g.setColour(juce::Colour(0xFF2D3748));
    g.drawRect(bounds, 1);

    // Lane Label & Status Badge
    g.setColour(juce::Colour(0xFFA0AEC0));
    g.setFont(13.0f);
    g.drawText("CORRECTED (Warped Grid)", bounds.getX() + 8, bounds.getY() + 6, 200, 16, juce::Justification::left);

    if (!m_session->is_rendered()) {
        // Show [ESTIMATE] badge
        g.setColour(juce::Colour(0xFFD69E2E));
        g.drawRoundedRectangle(static_cast<float>(bounds.getX() + 200), static_cast<float>(bounds.getY() + 5), 80.0f, 18.0f, 4.0f, 1.0f);
        g.setFont(11.0f);
        g.drawText("ESTIMATE", bounds.getX() + 200, bounds.getY() + 5, 80, 18, juce::Justification::centred);
    }

    // Draw Target Grid vertical lines
    const auto& warp = m_session->warp_map();
    if (!warp.anchors().empty()) {
        double target_bpm = warp.target_bpm();
        double beat_period = 60.0 / target_bpm;

        double grid_start = std::floor(m_view_start_sec / beat_period) * beat_period;
        for (double gt = grid_start; gt <= (m_view_start_sec + m_view_duration_sec); gt += beat_period) {
            double frac = (gt - m_view_start_sec) / m_view_duration_sec;
            int x = bounds.getX() + static_cast<int>(frac * bounds.getWidth());

            g.setColour(juce::Colour(0x22FFFFFF)); // 13% alpha grid lines
            g.drawVerticalLine(x, static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));
        }
    }

    // Draw Class-Tinted Waveform
    const auto& corrected = m_session->corrected_audio();
    float mid_y = bounds.getY() + (bounds.getHeight() * 0.5f);
    float half_h = (bounds.getHeight() * 0.45f);

    if (!corrected.is_empty() && bounds.getWidth() > 0) {
        double sec_per_pixel = m_view_duration_sec / bounds.getWidth();
        double sample_rate = corrected.sample_rate;

        for (int x = 0; x < bounds.getWidth(); ++x) {
            double t0 = m_view_start_sec + (x * sec_per_pixel);
            int64_t t_idx = static_cast<int64_t>(t0 * sample_rate);

            if (t_idx >= 0 && t_idx < static_cast<int64_t>(corrected.left.size())) {
                float sample = corrected.left[static_cast<size_t>(t_idx)];
                float y1 = mid_y - (std::abs(sample) * half_h);
                float y2 = mid_y + (std::abs(sample) * half_h);

                // Heatmap color based on warp ratio at this position
                int c_class = 0;
                for (const auto& seg : warp.segments()) {
                    if (t0 >= seg.tgt_start_sec && t0 <= seg.tgt_end_sec) {
                        c_class = seg.correction_class;
                        break;
                    }
                }
                g.setColour(get_correction_class_colour(c_class));
                g.drawVerticalLine(bounds.getX() + x, y1, y2);
            }
        }
    }
}

void WaveformView::draw_playhead(juce::Graphics& g) {
    if (m_playhead_sec >= m_view_start_sec && m_playhead_sec <= (m_view_start_sec + m_view_duration_sec)) {
        double frac = (m_playhead_sec - m_view_start_sec) / m_view_duration_sec;
        int x = getLocalBounds().getX() + static_cast<int>(frac * getLocalBounds().getWidth());

        g.setColour(juce::Colour(0xFFFC8181)); // Playhead coral red
        g.drawVerticalLine(x, 0.0f, static_cast<float>(getHeight()));

        // Playhead triangle at top
        juce::Path p;
        p.addTriangle(static_cast<float>(x - 5), 0.0f,
                      static_cast<float>(x + 5), 0.0f,
                      static_cast<float>(x), 10.0f);
        g.fillPath(p);
    }
}

} // namespace bf::gui
