// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
#pragma once
#include <juce_core/juce_core.h>

// Practice mode (see README): parses a Moonscraper/Clone Hero .chart or a
// .mid file's 5-fret guitar track into a flat, time-stamped note list the
// highway can scroll. MVP scope only -- no scoring, Star Power, or audio-
// track sync (see the plan/README for why); this only needs to answer
// "what note is at what time", not judge how well it was played.
struct ChartNote
{
    double timeSecs = 0.0;
    double lengthSecs = 0.0;
    int lane = 0;   // 0-4 = green..orange, 5 = open
};

class ChartFile
{
public:
    // true on success (notes may legitimately be empty for a silent intro);
    // false only if the file couldn't be read/parsed as this format at all
    bool loadAuto(const juce::File& file);   // picks .chart vs .mid by extension

    const juce::Array<ChartNote>& getNotes() const { return notes; }
    const juce::String& getTitle() const { return title; }
    double getLengthSecs() const { return lengthSecs; }

private:
    bool loadChart(const juce::File& file);
    bool loadMidi(const juce::File& file);

    juce::Array<ChartNote> notes;
    juce::String title;
    double lengthSecs = 0.0;
};
