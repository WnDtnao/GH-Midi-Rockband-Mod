// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
#pragma once
#include "PluginProcessor.h"
#include "HighwayRenderer.h"

// Live diagnostics: raw HID bytes for the guitar and pedal, parsed
// performance state, a strum-timing BPM estimate, an FPS counter, and a
// highway wireframe toggle. Purely a read-only view plus one checkbox --
// no LEARN/mapping interaction here, that's QuickBind/the row table's job.
// Reads GuitarService::getDebugSnapshot() and live ui* atomics, same
// "read live state fresh every paint()" pattern QuickBindPanel already uses
// -- no separate timer, relies on GHMidiEditor's existing 30Hz repaint.
class DebugPanel : public juce::Component
{
public:
    DebugPanel(GHMidiProcessor& processorToUse, HighwayRenderer& highwayToUse);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void drawByteGrid(juce::Graphics&, juce::Rectangle<int> area, const juce::String& title,
                      const uint8_t* buf, int len) const;

    GHMidiProcessor& proc;
    HighwayRenderer& highway;
    juce::ToggleButton wireframeToggle { "Wireframe" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DebugPanel)
};
