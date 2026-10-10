#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../../src/bf_dsp/units.h"
#include "../../src/bf_dsp/warp.h"
#include "../../src/bf_io/pcm_cache.h"
#include <atomic>
#include <mutex>
#include <vector>
#include <cmath>

namespace bf::gui {

enum class PlaybackSource {
    SourceA_Original = 0,
    SourceB_Corrected = 1
};

class AudioPlayer final : public juce::AudioIODeviceCallback {
public:
    AudioPlayer();
    ~AudioPlayer() override;

    bool init();
    void shutdown();

    void set_sources(
        std::shared_ptr<const io::AudioData> source_a,
        std::shared_ptr<const io::AudioData> source_b,
        const dsp::WarpMap* warp
    );

    void play();
    void pause();
    void stop();
    bool is_playing() const noexcept { return m_playing.load(std::memory_order_relaxed); }

    void seek_seconds(double pos_sec);
    double get_position_seconds() const noexcept { return m_current_pos_sec.load(std::memory_order_relaxed); }

    void set_active_source(PlaybackSource src);
    PlaybackSource get_active_source() const noexcept { return m_active_source.load(std::memory_order_relaxed); }

    void set_metronome_enabled(bool enabled) noexcept { m_metronome_enabled.store(enabled, std::memory_order_relaxed); }
    bool is_metronome_enabled() const noexcept { return m_metronome_enabled.load(std::memory_order_relaxed); }

    // AudioIODeviceCallback implementation
    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context
    ) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    juce::AudioDeviceManager m_device_manager;

    std::shared_ptr<const io::AudioData> m_source_a;
    std::shared_ptr<const io::AudioData> m_source_b;
    const dsp::WarpMap* m_warp = nullptr;
    std::mutex m_source_mutex;

    std::atomic<bool> m_playing{false};
    std::atomic<double> m_current_pos_sec{0.0};
    std::atomic<PlaybackSource> m_active_source{PlaybackSource::SourceB_Corrected};
    std::atomic<bool> m_metronome_enabled{false};

    // Crossfading state (20 ms equal power)
    bool m_crossfading = false;
    PlaybackSource m_crossfade_from = PlaybackSource::SourceA_Original;
    PlaybackSource m_crossfade_to = PlaybackSource::SourceB_Corrected;
    int m_crossfade_sample = 0;
    int m_crossfade_duration = 882; // 20ms @ 44.1kHz

    double m_device_sample_rate = 44100.0;
};

} // namespace bf::gui
