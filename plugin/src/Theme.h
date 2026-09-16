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
    // flat Windows-style panel chrome (Round 3): a neutral dark grey instead
    // of the original's navy-tinted glass, with a plain visible border
    // instead of a soft blurred edge
    extern const juce::Colour panelBg, panelBorder;
    constexpr float panelCornerRadius = 4.0f;

    // Metal Mania is kept embedded (still available via ghTypeface() below)
    // but ghFont() itself now returns the OS default UI font -- "make the
    // font generic too" per the user's own change request -- which also
    // means it renders as Segoe UI on Windows / the real system font on
    // Linux instead of the original's rock-poster display face.
    juce::Typeface::Ptr ghTypeface();
    juce::Font ghFont(float height);
}
