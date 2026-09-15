// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
//
// Shared look for GHMidiEditor and QuickBindPanel: gem colours, the gold
// accent, and the Metal Mania display font, extracted out of PluginEditor.cpp
// so QuickBind's diagram matches the rest of the UI with no drift between
// the two.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace Theme
{
    extern const juce::Colour gemColours[5];   // green, red, yellow, blue, orange
    extern const juce::Colour openBarColour;
    extern const juce::Colour gold;

    juce::Typeface::Ptr ghTypeface();
    juce::Font ghFont(float height);
}
