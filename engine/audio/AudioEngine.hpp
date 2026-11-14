#pragma once
#include "../core/Types.hpp"
#include <SDL3/SDL.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace vg {

enum class SfxCue : u8 {
    SwingLight = 0,
    SwingHeavy,
    HitImpact,
    ParryClang,
    DodgeWhoosh,
    StaggerBreak
};

// Lightweight procedural PCM audio synthesizer over SDL3 AudioStream
class AudioEngine {
public:
    bool init() {
        SDL_AudioSpec spec{ SDL_AUDIO_F32, 1, 44100 };
        m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!m_stream) return false;
        SDL_ResumeAudioStreamDevice(m_stream);
        m_initialized = true;
        return true;
    }

    void shutdown() {
        if (m_stream) {
            SDL_DestroyAudioStream(m_stream);
            m_stream = nullptr;
        }
        m_initialized = false;
    }

    void play(SfxCue cue, f32 gain = 0.8f) {
        if (!m_initialized || !m_stream) return;

        f32 freq = 220.0f;
        f32 dur = 0.08f;
        switch (cue) {
            case SfxCue::SwingLight:   freq = 280.0f; dur = 0.07f; break;
            case SfxCue::SwingHeavy:   freq = 150.0f; dur = 0.13f; break;
            case SfxCue::HitImpact:    freq = 110.0f; dur = 0.10f; break;
            case SfxCue::ParryClang:   freq = 920.0f; dur = 0.18f; break;
            case SfxCue::DodgeWhoosh:  freq = 360.0f; dur = 0.09f; break;
            case SfxCue::StaggerBreak: freq = 85.0f;  dur = 0.25f; break;
        }

        const std::size_t samples = static_cast<std::size_t>(44100.0f * dur);
        std::vector<f32> pcm(samples);
        for (std::size_t i = 0; i < samples; ++i) {
            const f32 t = static_cast<f32>(i) / 44100.0f;
            const f32 env = std::exp(-t * 28.0f);
            pcm[i] = std::sin(6.2831853f * freq * t) * env * gain * 0.25f;
        }
        SDL_PutAudioStreamData(m_stream, pcm.data(), static_cast<int>(pcm.size() * sizeof(f32)));
    }

private:
    SDL_AudioStream* m_stream = nullptr;
    bool m_initialized = false;
};

} // namespace vg
