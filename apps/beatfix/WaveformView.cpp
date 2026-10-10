#include "WaveformView.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace bf::gui {

WaveformView::WaveformView() {
    startTimerHz(60); // 60 FPS playhead refresh

    m_zoom_out_btn.setButtonText("−");
    m_zoom_out_btn.onClick = [this] { zoom_out(); };
    addAndMakeVisible(m_zoom_out_btn);

    m_zoom_reset_btn.setButtonText("100%");
    m_zoom_reset_btn.onClick = [this] { zoom_fit(); };
    addAndMakeVisible(m_zoom_reset_btn);

    m_zoom_in_btn.setButtonText("+");
    m_zoom_in_btn.onClick = [this] { zoom_in(); };
    addAndMakeVisible(m_zoom_in_btn);
}

WaveformView::~WaveformView() {
    stopTimer();
}

void WaveformView::set_session(engine::Session* session) {
    m_session = session;
    if (m_session && m_session->source_audio().sample_rate > 0 && m_session->source_audio().total_frames > 0) {
        double dur = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
        m_view_start_sec = 0.0;
        m_view_duration_sec = std::min(dur, 30.0);
    }
    repaint();
}

void WaveformView::set_playhead_seconds(double pos_sec) {
    m_playhead_sec = pos_sec;
}

void WaveformView::set_active_source(bool is_corrected) {
    m_is_corrected_source = is_corrected;
    repaint();
}

void WaveformView::set_view_window(double start_sec, double duration_sec) {
    if (!m_session || m_session->source_audio().is_empty()) return;
    double total_dur = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
    if (total_dur <= 0.0) return;

    m_view_duration_sec = std::clamp(duration_sec, 2.0, total_dur);
    m_view_start_sec = std::clamp(start_sec, 0.0, total_dur - m_view_duration_sec);
    repaint();
}

void WaveformView::timerCallback() {
    // Follow playhead if playing outside the visible window
    if (m_session && !m_session->source_audio().is_empty()) {
        double total_dur = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
        if (m_playhead_sec > (m_view_start_sec + m_view_duration_sec)) {
            m_view_start_sec = std::min(m_playhead_sec, std::max(0.0, total_dur - m_view_duration_sec));
        } else if (m_playhead_sec < m_view_start_sec) {
            m_view_start_sec = std::max(0.0, m_playhead_sec);
        }
    }
    repaint();
}

void WaveformView::resized() {
    auto area = getLocalBounds();

    // Upper Lane: Full-Track Overview (130 px)
    m_overview_rect = area.removeFromTop(130);

    // Separator space (8 px)
    area.removeFromTop(8);

    // Lower Lane: High-Detail Zoomed Inspector
    m_detail_rect = area;

    // Detail Header Strip (32 px)
    m_detail_header_rect = m_detail_rect.removeFromTop(32);
    m_detail_wave_rect = m_detail_rect;

    // Position Zoom Controls in detail header top-right
    int btn_w = 32;
    int btn_h = 24;
    int right_x = m_detail_header_rect.getRight() - 12;
    int btn_y = m_detail_header_rect.getY() + (m_detail_header_rect.getHeight() - btn_h) / 2;

    m_zoom_in_btn.setBounds(right_x - btn_w, btn_y, btn_w, btn_h);
    right_x -= (btn_w + 4);
    m_zoom_reset_btn.setBounds(right_x - 48, btn_y, 48, btn_h);
    right_x -= (48 + 4);
    m_zoom_out_btn.setBounds(right_x - btn_w, btn_y, btn_w, btn_h);
}

void WaveformView::zoom_in() {
    double center = m_view_start_sec + m_view_duration_sec * 0.5;
    double new_dur = std::max(2.0, m_view_duration_sec * 0.7);
    set_view_window(center - new_dur * 0.5, new_dur);
}

void WaveformView::zoom_out() {
    double center = m_view_start_sec + m_view_duration_sec * 0.5;
    double new_dur = m_view_duration_sec * 1.4;
    set_view_window(center - new_dur * 0.5, new_dur);
}

void WaveformView::zoom_fit() {
    if (m_session && m_session->source_audio().total_frames > 0) {
        double total_dur = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
        set_view_window(0.0, total_dur);
    }
}

void WaveformView::mouseMove(const juce::MouseEvent& e) {
    if (!m_session || m_session->source_audio().is_empty()) return;

    double total_dur = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
    if (total_dur <= 0.0) return;

    if (m_overview_rect.contains(e.getPosition())) {
        int w = m_overview_rect.getWidth();
        int left_handle_x = m_overview_rect.getX() + static_cast<int>((m_view_start_sec / total_dur) * w);
        int right_handle_x = m_overview_rect.getX() + static_cast<int>(((m_view_start_sec + m_view_duration_sec) / total_dur) * w);

        if (std::abs(e.x - left_handle_x) <= 6 || std::abs(e.x - right_handle_x) <= 6) {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        } else if (e.x > left_handle_x && e.x < right_handle_x) {
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            return;
        }
    }

    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void WaveformView::mouseDown(const juce::MouseEvent& e) {
    if (!m_session || m_session->source_audio().is_empty()) return;

    double total_dur = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
    if (total_dur <= 0.0) return;

    m_drag_start_mouse_x = static_cast<float>(e.x);
    m_drag_initial_start_sec = m_view_start_sec;
    m_drag_initial_dur_sec = m_view_duration_sec;

    // Overview Lane Interaction
    if (m_overview_rect.contains(e.getPosition())) {
        int w = m_overview_rect.getWidth();
        int left_handle_x = m_overview_rect.getX() + static_cast<int>((m_view_start_sec / total_dur) * w);
        int right_handle_x = m_overview_rect.getX() + static_cast<int>(((m_view_start_sec + m_view_duration_sec) / total_dur) * w);

        if (std::abs(e.x - left_handle_x) <= 6) {
            m_drag_mode = DragMode::OverviewLeftHandle;
        } else if (std::abs(e.x - right_handle_x) <= 6) {
            m_drag_mode = DragMode::OverviewRightHandle;
        } else if (e.x > left_handle_x && e.x < right_handle_x) {
            m_drag_mode = DragMode::OverviewPan;
        } else {
            // Click outside jumps view window center to click position
            double click_sec = (static_cast<double>(e.x - m_overview_rect.getX()) / w) * total_dur;
            set_view_window(click_sec - m_view_duration_sec * 0.5, m_view_duration_sec);
            m_drag_mode = DragMode::OverviewPan;
            m_drag_initial_start_sec = m_view_start_sec;
        }
        return;
    }

    // Detail Lane Interaction (Seeking & Scrubbing)
    if (m_detail_wave_rect.contains(e.getPosition())) {
        m_drag_mode = DragMode::DetailSeek;
        double frac = static_cast<double>(e.x - m_detail_wave_rect.getX()) / static_cast<double>(m_detail_wave_rect.getWidth());
        double click_sec = m_view_start_sec + (frac * m_view_duration_sec);
        if (on_seek) on_seek(std::clamp(click_sec, 0.0, total_dur));
    }
}

void WaveformView::mouseDrag(const juce::MouseEvent& e) {
    if (!m_session || m_session->source_audio().is_empty()) return;

    double total_dur = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
    if (total_dur <= 0.0) return;

    float delta_px = static_cast<float>(e.x) - m_drag_start_mouse_x;

    if (m_drag_mode == DragMode::OverviewPan) {
        double delta_sec = (static_cast<double>(delta_px) / m_overview_rect.getWidth()) * total_dur;
        set_view_window(m_drag_initial_start_sec + delta_sec, m_view_duration_sec);
    } else if (m_drag_mode == DragMode::OverviewLeftHandle) {
        double delta_sec = (static_cast<double>(delta_px) / m_overview_rect.getWidth()) * total_dur;
        double new_start = std::clamp(m_drag_initial_start_sec + delta_sec, 0.0, m_drag_initial_start_sec + m_drag_initial_dur_sec - 2.0);
        double new_dur = (m_drag_initial_start_sec + m_drag_initial_dur_sec) - new_start;
        set_view_window(new_start, new_dur);
    } else if (m_drag_mode == DragMode::OverviewRightHandle) {
        double delta_sec = (static_cast<double>(delta_px) / m_overview_rect.getWidth()) * total_dur;
        double new_dur = std::clamp(m_drag_initial_dur_sec + delta_sec, 2.0, total_dur - m_drag_initial_start_sec);
        set_view_window(m_drag_initial_start_sec, new_dur);
    } else if (m_drag_mode == DragMode::DetailSeek) {
        double frac = static_cast<double>(e.x - m_detail_wave_rect.getX()) / static_cast<double>(m_detail_wave_rect.getWidth());
        double scrub_sec = m_view_start_sec + (frac * m_view_duration_sec);
        if (on_seek) on_seek(std::clamp(scrub_sec, 0.0, total_dur));
    }
}

void WaveformView::mouseUp(const juce::MouseEvent& /*e*/) {
    m_drag_mode = DragMode::None;
}

void WaveformView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    if (!m_session || m_session->source_audio().is_empty()) return;
    double total_dur = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
    if (total_dur <= 0.0) return;

    if (e.mods.isCtrlDown() || e.mods.isAltDown()) {
        // Zoom centered around mouse cursor position
        double mouse_frac = std::clamp(static_cast<double>(e.x - m_detail_wave_rect.getX()) / m_detail_wave_rect.getWidth(), 0.0, 1.0);
        double mouse_time = m_view_start_sec + mouse_frac * m_view_duration_sec;
        double zoom_factor = (wheel.deltaY > 0) ? 0.85 : 1.18;
        double new_dur = std::clamp(m_view_duration_sec * zoom_factor, 2.0, total_dur);
        double new_start = mouse_time - mouse_frac * new_dur;
        set_view_window(new_start, new_dur);
    } else {
        // Horizontal pan
        double pan_sec = (-wheel.deltaY * 0.15) * m_view_duration_sec;
        set_view_window(m_view_start_sec + pan_sec, m_view_duration_sec);
    }
}

juce::Colour WaveformView::get_correction_class_colour(int correction_class) const {
    switch (correction_class) {
        case 0: return juce::Colour(0xFF4A5568); // Neutral Grey-Blue
        case 1: return juce::Colour(0xFFD69E2E); // Moderate Amber
        case 2: return juce::Colour(0xFFED8936); // Significant Warm Orange
        case 3:
        default: return juce::Colour(0xFFFF334B); // Vibrant Fiery Coral Red
    }
}

void WaveformView::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xFF0B0E14)); // Deep dark matte background

    if (!m_session || m_session->source_audio().is_empty()) {
        auto area = getLocalBounds().reduced(24);
        g.setColour(juce::Colour(0xFF161B26));
        g.fillRoundedRectangle(area.toFloat(), 12.0f);
        g.setColour(juce::Colour(0xFF252D3D));
        g.drawRoundedRectangle(area.toFloat(), 12.0f, 1.5f);

        g.setColour(juce::Colour(0xFFE2E8F0));
        g.setFont(juce::Font(22.0f, juce::Font::bold));
        g.drawText("Drop Audio File to Analyze & Fix Timing",
                   area.removeFromTop(area.getHeight() / 2 + 10), juce::Justification::centredBottom, true);
        g.setFont(juce::Font(14.0f, juce::Font::plain));
        g.setColour(juce::Colour(0xFF718096));
        g.drawText("Supported formats: WAV, FLAC, MP3, AIFF (.beatfix projects)",
                   area, juce::Justification::centredTop, true);
        return;
    }

    draw_overview_lane(g, m_overview_rect);
    draw_detail_lane(g, m_detail_rect);
    draw_playhead(g);
}

void WaveformView::draw_overview_lane(juce::Graphics& g, juce::Rectangle<int> bounds) {
    // Card background
    g.setColour(juce::Colour(0xFF121620));
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
    g.setColour(juce::Colour(0xFF1F2633));
    g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);

    auto inner = bounds.reduced(8, 6);
    int total_w = inner.getWidth();
    if (total_w <= 0) return;

    double total_dur = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
    if (total_dur <= 0.0) return;

    // 1. Time Ruler (Top 18 px)
    auto ruler_bounds = inner.removeFromTop(18);
    g.setColour(juce::Colour(0xFF718096));
    g.setFont(juce::Font(10.0f, juce::Font::plain));

    double interval_sec = (total_dur > 300.0) ? 60.0 : (total_dur > 90.0 ? 30.0 : 15.0);
    for (double t = 0.0; t <= total_dur; t += interval_sec) {
        int x = ruler_bounds.getX() + static_cast<int>((t / total_dur) * total_w);
        g.setColour(juce::Colour(0xFF2D3748));
        g.drawVerticalLine(x, static_cast<float>(ruler_bounds.getBottom() - 4), static_cast<float>(ruler_bounds.getBottom()));

        int m = static_cast<int>(t) / 60;
        int s = static_cast<int>(t) % 60;
        juce::String timeStr = juce::String::formatted("%d:%02d", m, s);
        g.setColour(juce::Colour(0xFF8A99AD));
        g.drawText(timeStr, x - 20, ruler_bounds.getY(), 40, 14, juce::Justification::centred);
    }

    // Mini Warp Heatmap Strip (Bottom 12 px)
    auto heatmap_bounds = inner.removeFromBottom(12);

    // Overview Waveform Area (Remaining middle)
    auto wave_bounds = inner;
    float mid_y = wave_bounds.getY() + wave_bounds.getHeight() * 0.5f;
    float half_h = wave_bounds.getHeight() * 0.44f;

    auto src = m_session->source_audio_shared();
    if (src && !src->left.empty()) {
        g.setColour(juce::Colour(0xFF4A586E)); // Calm slate-blue base envelope
        int64_t total_samples = static_cast<int64_t>(src->left.size());
        double samples_per_pixel = static_cast<double>(total_samples) / total_w;

        for (int x = 0; x < total_w; ++x) {
            int64_t s_start = static_cast<int64_t>(x * samples_per_pixel);
            int64_t s_end = std::min(total_samples, static_cast<int64_t>((x + 1) * samples_per_pixel));
            float max_val = 0.0f;
            for (int64_t s = s_start; s < s_end; s += std::max<int64_t>(1, (s_end - s_start) / 8)) {
                max_val = std::max(max_val, std::abs(src->left[static_cast<size_t>(s)]));
            }
            if (max_val > 0.001f) {
                g.drawVerticalLine(wave_bounds.getX() + x, mid_y - max_val * half_h, mid_y + max_val * half_h);
            }
        }
    }

    // Draw Mini Warp Heatmap Strip
    g.setColour(juce::Colour(0xFF0E121B));
    g.fillRect(heatmap_bounds);
    g.setColour(juce::Colour(0xFF1D2433));
    g.drawRect(heatmap_bounds, 1);

    const auto& warp = m_session->warp_map();
    if (!warp.segments().empty()) {
        for (const auto& seg : warp.segments()) {
            int x1 = heatmap_bounds.getX() + static_cast<int>((seg.src_start_sec / total_dur) * total_w);
            int x2 = heatmap_bounds.getX() + static_cast<int>((seg.src_end_sec / total_dur) * total_w);
            x2 = std::max(x1 + 1, x2);

            g.setColour(get_correction_class_colour(seg.correction_class));
            g.fillRect(x1, heatmap_bounds.getY() + 1, x2 - x1, heatmap_bounds.getHeight() - 2);
        }
    }

    // Zoom Range Selection Highlight Window
    int sel_x1 = inner.getX() + static_cast<int>((m_view_start_sec / total_dur) * total_w);
    int sel_x2 = inner.getX() + static_cast<int>(((m_view_start_sec + m_view_duration_sec) / total_dur) * total_w);
    sel_x2 = std::clamp(sel_x2, sel_x1 + 6, inner.getRight());

    // Dim mask outside selection
    g.setColour(juce::Colour(0x600B0E14));
    g.fillRect(inner.getX(), inner.getY(), sel_x1 - inner.getX(), inner.getHeight() + heatmap_bounds.getHeight() + 2);
    g.fillRect(sel_x2, inner.getY(), inner.getRight() - sel_x2, inner.getHeight() + heatmap_bounds.getHeight() + 2);

    // Translucent glowing red selection box
    juce::Rectangle<int> sel_rect(sel_x1, inner.getY(), sel_x2 - sel_x1, inner.getHeight() + heatmap_bounds.getHeight() + 2);
    g.setColour(juce::Colour(0x28FF334B));
    g.fillRect(sel_rect);

    // Glowing boundary lines & handles
    g.setColour(juce::Colour(0xFFFF334B));
    g.drawVerticalLine(sel_x1, static_cast<float>(sel_rect.getY()), static_cast<float>(sel_rect.getBottom()));
    g.drawVerticalLine(sel_x2, static_cast<float>(sel_rect.getY()), static_cast<float>(sel_rect.getBottom()));

    // Left and right ▼ triangle handle flags
    juce::Path left_flag, right_flag;
    left_flag.addTriangle(static_cast<float>(sel_x1 - 4), static_cast<float>(sel_rect.getY()),
                          static_cast<float>(sel_x1 + 4), static_cast<float>(sel_rect.getY()),
                          static_cast<float>(sel_x1), static_cast<float>(sel_rect.getY() + 7));
    right_flag.addTriangle(static_cast<float>(sel_x2 - 4), static_cast<float>(sel_rect.getY()),
                           static_cast<float>(sel_x2 + 4), static_cast<float>(sel_rect.getY()),
                           static_cast<float>(sel_x2), static_cast<float>(sel_rect.getY() + 7));
    g.fillPath(left_flag);
    g.fillPath(right_flag);
}

void WaveformView::draw_detail_lane(juce::Graphics& g, juce::Rectangle<int> bounds) {
    // Card background
    g.setColour(juce::Colour(0xFF121620));
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
    g.setColour(juce::Colour(0xFF1F2633));
    g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);

    // Detail Header Title Tag
    g.setColour(juce::Colour(0xFFFF334B));
    g.setFont(juce::Font(14.0f, juce::Font::bold));

    juce::String tagText = m_is_corrected_source ? "  📈  Warped (Stretched)" : "  📈  Original (Source)";
    g.drawText(tagText, bounds.getX() + 12, bounds.getY() + 6, 220, 20, juce::Justification::left);

    // Update zoom reset button text with zoom percentage
    if (m_session && m_session->source_audio().total_frames > 0) {
        double total_dur = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
        if (total_dur > 0.0) {
            int pct = juce::roundToInt((total_dur / m_view_duration_sec) * 100.0);
            m_zoom_reset_btn.setButtonText(juce::String(pct) + "%");
        }
    }

    auto wave_area = m_detail_wave_rect.reduced(10, 6);
    int total_w = wave_area.getWidth();
    if (total_w <= 0) return;

    // Time ticks on zoomed detail lane
    g.setColour(juce::Colour(0xFF4A5568));
    g.setFont(juce::Font(10.0f, juce::Font::plain));

    double detail_interval = (m_view_duration_sec > 60.0) ? 10.0 : (m_view_duration_sec > 15.0 ? 5.0 : 1.0);
    double tick_start = std::floor(m_view_start_sec / detail_interval) * detail_interval;
    for (double t = tick_start; t <= (m_view_start_sec + m_view_duration_sec); t += detail_interval) {
        if (t < m_view_start_sec) continue;
        int x = wave_area.getX() + static_cast<int>(((t - m_view_start_sec) / m_view_duration_sec) * total_w);
        g.setColour(juce::Colour(0x20A0AEC0));
        g.drawVerticalLine(x, static_cast<float>(wave_area.getY()), static_cast<float>(wave_area.getBottom() - 16));

        int m = static_cast<int>(t) / 60;
        int s = static_cast<int>(t) % 60;
        juce::String timeStr = juce::String::formatted("%d:%02d", m, s);
        g.setColour(juce::Colour(0xFF5A6678));
        g.drawText(timeStr, x - 18, wave_area.getY(), 36, 12, juce::Justification::centred);
    }

    // Draw Vertical Beat Grid Lines
    const auto& analysis = m_session->analysis();
    const auto& warp = m_session->warp_map();

    for (const auto& beat : analysis.beats) {
        if (beat.time_sec >= m_view_start_sec && beat.time_sec <= (m_view_start_sec + m_view_duration_sec)) {
            int x = wave_area.getX() + static_cast<int>(((beat.time_sec - m_view_start_sec) / m_view_duration_sec) * total_w);

            if (beat.is_downbeat) {
                g.setColour(juce::Colour(0x40FF334B)); // Glowing red downbeat line
                g.drawVerticalLine(x, static_cast<float>(wave_area.getY() + 12), static_cast<float>(wave_area.getBottom() - 16));
            } else {
                g.setColour(juce::Colour(0x15FFFFFF)); // Subtle beat line
                g.drawVerticalLine(x, static_cast<float>(wave_area.getY() + 12), static_cast<float>(wave_area.getBottom() - 16));
            }
        }
    }

    // High-Detail Waveform with Glowing Stretched Gradient
    float mid_y = wave_area.getY() + (wave_area.getHeight() - 16) * 0.5f + 6.0f;
    float half_h = (wave_area.getHeight() - 28) * 0.45f;

    auto display_audio = (m_is_corrected_source && m_session->is_rendered()) ?
                          m_session->corrected_audio_shared() : m_session->source_audio_shared();

    if (display_audio && !display_audio->left.empty()) {
        double sample_rate = (display_audio->sample_rate > 0) ? display_audio->sample_rate : 44100.0;
        int64_t total_samples = static_cast<int64_t>(display_audio->left.size());
        double sec_per_pixel = m_view_duration_sec / total_w;

        for (int x = 0; x < total_w; ++x) {
            double t0 = m_view_start_sec + (x * sec_per_pixel);
            int64_t s_idx = static_cast<int64_t>(t0 * sample_rate);

            if (s_idx >= 0 && s_idx < total_samples) {
                float amp = std::abs(display_audio->left[static_cast<size_t>(s_idx)]);

                // Check warp status at this time
                int c_class = 0;
                for (const auto& seg : warp.segments()) {
                    if (t0 >= seg.tgt_start_sec && t0 <= seg.tgt_end_sec) {
                        c_class = seg.correction_class;
                        break;
                    }
                }

                float y1 = mid_y - amp * half_h;
                float y2 = mid_y + amp * half_h;

                if (c_class > 0) {
                    // Warped / Fixed section: Radiant Fiery Gradient Glow
                    juce::Colour topGlow = (c_class >= 2) ? juce::Colour(0xFFFF334B) : juce::Colour(0xFFFF7A45);
                    g.setColour(topGlow);
                    g.drawVerticalLine(wave_area.getX() + x, y1, y2);
                } else {
                    // Neutral section: Calm Slate-Blue
                    g.setColour(juce::Colour(0xFF5B6E8A));
                    g.drawVerticalLine(wave_area.getX() + x, y1, y2);
                }
            }
        }
    }

    // Beat Markers & Numbers along bottom baseline
    int baseline_y = wave_area.getBottom() - 14;
    g.setColour(juce::Colour(0xFF1F2633));
    g.drawHorizontalLine(baseline_y, static_cast<float>(wave_area.getX()), static_cast<float>(wave_area.getRight()));

    g.setFont(juce::Font(10.0f, juce::Font::bold));
    for (const auto& beat : analysis.beats) {
        if (beat.time_sec >= m_view_start_sec && beat.time_sec <= (m_view_start_sec + m_view_duration_sec)) {
            int x = wave_area.getX() + static_cast<int>(((beat.time_sec - m_view_start_sec) / m_view_duration_sec) * total_w);

            int beat_in_bar = (beat.bar_index >= 0) ? ((int)std::round(std::fmod(beat.time_sec * (m_session->target_bpm() / 60.0), 4.0)) + 1) : 1;
            if (beat_in_bar < 1 || beat_in_bar > 4) beat_in_bar = 1;

            if (beat.is_downbeat) {
                // Large Glowing Red Dot & Bold '1'
                g.setColour(juce::Colour(0xFFFF334B));
                g.fillEllipse(static_cast<float>(x - 3), static_cast<float>(baseline_y - 3), 6.0f, 6.0f);
                g.setColour(juce::Colours::white);
                g.drawText("1", x - 6, baseline_y + 3, 12, 12, juce::Justification::centred);
            } else {
                // Small Red Dot & Beat Number
                g.setColour(juce::Colour(0xCCFF334B));
                g.fillEllipse(static_cast<float>(x - 2), static_cast<float>(baseline_y - 2), 4.0f, 4.0f);
                g.setColour(juce::Colour(0xFF8A99AD));
                g.drawText(juce::String(beat_in_bar), x - 6, baseline_y + 3, 12, 12, juce::Justification::centred);
            }
        }
    }
}

void WaveformView::draw_playhead(juce::Graphics& g) {
    if (!m_session || m_session->source_audio().is_empty()) return;

    double total_dur = static_cast<double>(m_session->source_audio().total_frames) / m_session->source_audio().sample_rate;
    if (total_dur <= 0.0) return;

    // 1. Playhead in Overview Lane
    if (m_playhead_sec >= 0.0 && m_playhead_sec <= total_dur) {
        auto inner = m_overview_rect.reduced(8, 6);
        int ox = inner.getX() + static_cast<int>((m_playhead_sec / total_dur) * inner.getWidth());
        g.setColour(juce::Colour(0xFFFF334B));
        g.drawVerticalLine(ox, static_cast<float>(inner.getY()), static_cast<float>(inner.getBottom()));
    }

    // 2. Playhead in Detail Lane
    if (m_playhead_sec >= m_view_start_sec && m_playhead_sec <= (m_view_start_sec + m_view_duration_sec)) {
        auto wave_area = m_detail_wave_rect.reduced(10, 6);
        int dx = wave_area.getX() + static_cast<int>(((m_playhead_sec - m_view_start_sec) / m_view_duration_sec) * wave_area.getWidth());

        g.setColour(juce::Colour(0xFFFF334B));
        g.drawVerticalLine(dx, static_cast<float>(wave_area.getY() + 10), static_cast<float>(wave_area.getBottom() - 14));

        // Indicator triangle at top of detail lane
        juce::Path p;
        p.addTriangle(static_cast<float>(dx - 5), static_cast<float>(wave_area.getY() + 10),
                      static_cast<float>(dx + 5), static_cast<float>(wave_area.getY() + 10),
                      static_cast<float>(dx), static_cast<float>(wave_area.getY() + 18));
        g.fillPath(p);
    }
}

} // namespace bf::gui
