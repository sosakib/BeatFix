#include "AudioPlayer.h"
#include <numbers>

namespace bf::gui {

AudioPlayer::AudioPlayer() = default;

AudioPlayer::~AudioPlayer() {
    shutdown();
}

bool AudioPlayer::init() {
    auto err = m_device_manager.initialiseWithDefaultDevices(0, 2);
    if (!err.isEmpty()) {
        return false;
    }
    m_device_manager.addAudioCallback(this);
    return true;
}

void AudioPlayer::shutdown() {
    m_device_manager.removeAudioCallback(this);
    m_device_manager.closeAudioDevice();
}

void AudioPlayer::set_sources(
    std::shared_ptr<const io::AudioData> source_a,
    std::shared_ptr<const io::AudioData> source_b,
    const dsp::WarpMap* warp
) {
    std::lock_guard<std::mutex> lock(m_source_mutex);
    m_source_a = std::move(source_a);
    m_source_b = std::move(source_b);
    m_warp = warp;
}

void AudioPlayer::play() {
    m_playing.store(true, std::memory_order_release);
}

void AudioPlayer::pause() {
    m_playing.store(false, std::memory_order_release);
}

void AudioPlayer::stop() {
    m_playing.store(false, std::memory_order_release);
    m_current_pos_sec.store(0.0, std::memory_order_release);
}

void AudioPlayer::seek_seconds(double pos_sec) {
    if (pos_sec < 0.0) pos_sec = 0.0;
    m_current_pos_sec.store(pos_sec, std::memory_order_release);
}

void AudioPlayer::set_active_source(PlaybackSource src) {
    if (m_active_source.load(std::memory_order_relaxed) == src) return;

    if (m_playing.load(std::memory_order_relaxed)) {
        // Trigger 20ms equal-power crossfade
        m_crossfade_from = m_active_source.load(std::memory_order_relaxed);
        m_crossfade_to = src;
        m_crossfade_sample = 0;
        m_crossfade_duration = std::max(1, static_cast<int>(0.020 * m_device_sample_rate));
        m_crossfading = true;
    }
    m_active_source.store(src, std::memory_order_release);
}

void AudioPlayer::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    if (device) {
        m_device_sample_rate = device->getCurrentSampleRate();
    }
}

void AudioPlayer::audioDeviceStopped() {}

void AudioPlayer::audioDeviceIOCallbackWithContext(
    const float* const* /*inputChannelData*/,
    int /*numInputChannels*/,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& /*context*/
) {
    if (numOutputChannels <= 0 || numSamples <= 0) return;

    for (int ch = 0; ch < numOutputChannels; ++ch) {
        if (outputChannelData[ch]) {
            std::fill_n(outputChannelData[ch], numSamples, 0.0f);
        }
    }

    if (!m_playing.load(std::memory_order_acquire)) {
        return;
    }

    std::shared_ptr<const io::AudioData> src_a;
    std::shared_ptr<const io::AudioData> src_b;
    const dsp::WarpMap* warp = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_source_mutex);
        src_a = m_source_a;
        src_b = m_source_b;
        warp = m_warp;
    }

    if (!src_a && !src_b) {
        return;
    }

    double pos = m_current_pos_sec.load(std::memory_order_relaxed);
    PlaybackSource active_src = m_active_source.load(std::memory_order_relaxed);

    float* out_l = outputChannelData[0];
    float* out_r = (numOutputChannels > 1) ? outputChannelData[1] : nullptr;

    for (int i = 0; i < numSamples; ++i) {
        double current_sample_time = pos + (static_cast<double>(i) / m_device_sample_rate);

        auto get_sample = [&](PlaybackSource src, float& l, float& r) {
            l = 0.0f;
            r = 0.0f;
            if (src == PlaybackSource::SourceA_Original && src_a && !src_a->left.empty()) {
                int64_t s_idx = static_cast<int64_t>(current_sample_time * src_a->sample_rate);
                if (s_idx >= 0 && s_idx < static_cast<int64_t>(src_a->left.size())) {
                    l = src_a->left[static_cast<size_t>(s_idx)];
                    r = (src_a->channels == 2 && !src_a->right.empty()) ? src_a->right[static_cast<size_t>(s_idx)] : l;
                }
            } else if (src == PlaybackSource::SourceB_Corrected && src_b && !src_b->left.empty()) {
                int64_t t_idx = static_cast<int64_t>(current_sample_time * src_b->sample_rate);
                if (t_idx >= 0 && t_idx < static_cast<int64_t>(src_b->left.size())) {
                    l = src_b->left[static_cast<size_t>(t_idx)];
                    r = (src_b->channels == 2 && !src_b->right.empty()) ? src_b->right[static_cast<size_t>(t_idx)] : l;
                }
            } else if (src_a && !src_a->left.empty()) {
                int64_t s_idx = static_cast<int64_t>(current_sample_time * src_a->sample_rate);
                if (s_idx >= 0 && s_idx < static_cast<int64_t>(src_a->left.size())) {
                    l = src_a->left[static_cast<size_t>(s_idx)];
                    r = (src_a->channels == 2 && !src_a->right.empty()) ? src_a->right[static_cast<size_t>(s_idx)] : l;
                }
            }
        };

        float samp_l = 0.0f, samp_r = 0.0f;

        if (m_crossfading) {
            float from_l = 0.0f, from_r = 0.0f;
            float to_l = 0.0f, to_r = 0.0f;
            get_sample(m_crossfade_from, from_l, from_r);
            get_sample(m_crossfade_to, to_l, to_r);

            double frac = static_cast<double>(m_crossfade_sample) / static_cast<double>(m_crossfade_duration);
            // Equal-power crossfade gains: g_from^2 + g_to^2 = 1
            float g_from = static_cast<float>(std::cos(frac * 0.5 * std::numbers::pi));
            float g_to = static_cast<float>(std::sin(frac * 0.5 * std::numbers::pi));

            samp_l = (from_l * g_from) + (to_l * g_to);
            samp_r = (from_r * g_from) + (to_r * g_to);

            m_crossfade_sample++;
            if (m_crossfade_sample >= m_crossfade_duration) {
                m_crossfading = false;
            }
        } else {
            get_sample(active_src, samp_l, samp_r);
        }

        // Metronome click track (REQ-PLY-004)
        if (m_metronome_enabled.load(std::memory_order_relaxed) && warp && warp->target_bpm() > 0.0) {
            double click_period = 60.0 / warp->target_bpm();
            double beat_phase = std::fmod(current_sample_time, click_period);
            if (beat_phase >= 0.0 && beat_phase < 0.010) {
                double t_click = beat_phase;
                double freq = 1200.0;
                float click_val = static_cast<float>(0.3 * std::sin(2.0 * std::numbers::pi * freq * t_click) * std::exp(-t_click * 300.0));
                samp_l += click_val;
                samp_r += click_val;
            }
        }

        if (out_l) out_l[i] = samp_l;
        if (out_r) out_r[i] = samp_r;
    }

    double new_pos = pos + (static_cast<double>(numSamples) / m_device_sample_rate);
    m_current_pos_sec.store(new_pos, std::memory_order_release);
}

} // namespace bf::gui
