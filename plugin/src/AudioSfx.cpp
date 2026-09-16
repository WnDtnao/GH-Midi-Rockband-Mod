// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
#include "AudioSfx.h"
#include <BinaryData.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace {
struct SfxSource { const char* data; int size; };
const SfxSource kSources[AudioSfx::kNumSfx] = {
    { BinaryData::bad_note1_wav,      BinaryData::bad_note1_wavSize },
    { BinaryData::bad_note2_wav,      BinaryData::bad_note2_wavSize },
    { BinaryData::bad_note3_wav,      BinaryData::bad_note3_wavSize },
    { BinaryData::bad_note4_wav,      BinaryData::bad_note4_wavSize },
    { BinaryData::bad_note6_wav,      BinaryData::bad_note6_wavSize },
    { BinaryData::checkbox_on_wav,    BinaryData::checkbox_on_wavSize },
    { BinaryData::checkbox_off_wav,   BinaryData::checkbox_off_wavSize },
    { BinaryData::highway_rise_wav,   BinaryData::highway_rise_wavSize },
    { BinaryData::music_scroll_wav,   BinaryData::music_scroll_wavSize },
    { BinaryData::music_select_wav,   BinaryData::music_select_wavSize },
    { BinaryData::notes_ripple_up_wav,BinaryData::notes_ripple_up_wavSize },
    { BinaryData::song_failed_wav,    BinaryData::song_failed_wavSize },
    { BinaryData::sp_available_wav,   BinaryData::sp_available_wavSize },
    { BinaryData::sp_awarded1_wav,    BinaryData::sp_awarded1_wavSize },
    { BinaryData::sp_awarded2_wav,    BinaryData::sp_awarded2_wavSize },
    { BinaryData::sp_deployed_wav,    BinaryData::sp_deployed_wavSize },
    { BinaryData::star_available_wav, BinaryData::star_available_wavSize },
    { BinaryData::star_release_wav,   BinaryData::star_release_wavSize },
    { BinaryData::sudden_death_wav,   BinaryData::sudden_death_wavSize },
    { BinaryData::you_rock_wav,       BinaryData::you_rock_wavSize },
};
}

AudioSfx::AudioSfx()
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    for (int i = 0; i < (int) kNumSfx; ++i)
    {
        auto stream = std::make_unique<juce::MemoryInputStream>(kSources[i].data, (size_t) kSources[i].size, false);
        std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(std::move(stream)));
        if (reader == nullptr || reader->lengthInSamples <= 0)
            continue;
        auto& clip = clips[i];
        clip.sourceRate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
        clip.data.setSize((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read(&clip.data, 0, (int) reader->lengthInSamples, 0, true, true);
    }
}

void AudioSfx::prepareToPlay(double sampleRate)
{
    deviceSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void AudioSfx::play(int id, float gain)
{
    if (id < 0 || id >= (int) kNumSfx || clips[id].data.getNumSamples() == 0)
        return;
    const juce::ScopedLock sl(voiceLock);
    for (auto& v : voices)
        if (! v.active)
        {
            v = { id, 0.0, gain, true };
            return;
        }
    voices[0] = { id, 0.0, gain, true };   // all busy: steal the oldest slot
}

void AudioSfx::render(juce::AudioBuffer<float>& buffer)
{
    const juce::ScopedLock sl(voiceLock);
    const int numOut = buffer.getNumSamples();
    const int outChans = buffer.getNumChannels();
    for (auto& v : voices)
    {
        if (! v.active)
            continue;
        auto& clip = clips[v.id];
        const int srcChans = clip.data.getNumChannels();
        const int srcLen = clip.data.getNumSamples();
        if (srcChans == 0 || srcLen < 2)
        {
            v.active = false;
            continue;
        }
        const double step = clip.sourceRate / deviceSampleRate;
        for (int i = 0; i < numOut; ++i)
        {
            const int p0 = (int) v.pos;
            if (p0 >= srcLen - 1)
            {
                v.active = false;
                break;
            }
            const float frac = (float) (v.pos - (double) p0);
            for (int ch = 0; ch < outChans; ++ch)
            {
                const int srcCh = juce::jmin(ch, srcChans - 1);
                const float s0 = clip.data.getSample(srcCh, p0);
                const float s1 = clip.data.getSample(srcCh, p0 + 1);
                buffer.addSample(ch, i, (s0 + (s1 - s0) * frac) * v.gain);
            }
            v.pos += step;
        }
    }
}
