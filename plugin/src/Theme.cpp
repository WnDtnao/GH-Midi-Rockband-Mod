// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a). Extracted from PluginEditor.cpp, unchanged.
#include "Theme.h"
#include <BinaryData.h>

namespace Theme
{

const juce::Colour gemColours[5] = {
    juce::Colour(0xff33cc3d), juce::Colour(0xffe63232), juce::Colour(0xfff2d02a),
    juce::Colour(0xff3378e6), juce::Colour(0xfff2921f),
};
const juce::Colour openBarColour(0xffa05ff0);
const juce::Colour gold(0xfff2d02a);
const juce::Colour panelBg(0xf1202024);
const juce::Colour panelBorder = juce::Colours::white.withAlpha(0.16f);

juce::Typeface::Ptr ghTypeface()
{
    static juce::Typeface::Ptr t = juce::Typeface::createSystemTypefaceFor(
        BinaryData::MetalManiaRegular_ttf, (size_t) BinaryData::MetalManiaRegular_ttfSize);
    return t;
}

juce::Font ghFont(float h)
{
    return juce::Font(juce::FontOptions(h, juce::Font::bold));
}

} // namespace Theme
