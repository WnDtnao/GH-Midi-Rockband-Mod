// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// One-shot WAV playback for the Standalone app only (see
// PluginProcessor.cpp's processBlock -- a VST3 hosted in a DAW shouldn't
// play sounds into the host's mix on its own, same reasoning as the MIDI
// input guard in GuitarService::applyMidiInput). Decodes each embedded clip
// once, up front; playback resamples on the fly with linear interpolation
// rather than pulling in a resampling AudioSource, since these are all
// short UI/game blips, not anything that needs broadcast-quality
// resampling.
class AudioSfx
{
public:
    enum Id
    {
        BadNote1, BadNote2, BadNote3, BadNote4, BadNote6,
        CheckboxOn, CheckboxOff,
        HighwayRise,
        MusicScroll, MusicSelect,
        NotesRippleUp,
        SongFailed,
        SpAvailable, SpAwarded1, SpAwarded2, SpDeployed,
        StarAvailable, StarRelease,
        SuddenDeath,
        YouRock,
        kNumSfx
    };

    AudioSfx();

    void prepareToPlay(double sampleRate);
    void play(int id, float gain = 1.0f);
    // mixes active voices into `buffer` (adds, doesn't clear); call from the
    // audio thread only, standalone builds only
    void render(juce::AudioBuffer<float>& buffer);

private:
    struct Clip { juce::AudioBuffer<float> data; double sourceRate = 44100.0; };
    struct Voice { int id = -1; double pos = 0.0; float gain = 1.0f; bool active = false; };

    Clip clips[kNumSfx];
    static constexpr int kMaxVoices = 8;
    Voice voices[kMaxVoices];
    double deviceSampleRate = 44100.0;
    juce::CriticalSection voiceLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSfx)
};
