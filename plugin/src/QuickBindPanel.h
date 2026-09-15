// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
#pragma once
#include "PluginProcessor.h"

// QuickBind: a vector-drawn guitar diagram with one clickable hotspot per
// mappable control, so binding a controller is "click the picture of the
// button, then press it" instead of working down the text row table one
// control at a time. This is purely a new view over GuitarService's existing
// LEARN engine -- clicking a hotspot calls the exact same startLearn() /
// cancelLearn() the row table's LEARN buttons already call, and mapped/live
// state is read from the exact same describeMapping() / uiButtonBits the row
// table already reads. No engine changes needed for the GH control set this
// draws today.
class QuickBindPanel : public juce::Component
{
public:
    explicit QuickBindPanel(GHMidiProcessor& processorToUse);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    struct Hotspot
    {
        int target = -1;
        juce::Path shape;         // built once, in virtual-canvas space
        juce::Path screenShape;   // resized(): shape transformed into component bounds
        juce::String label;
    };

    void buildHotspots();    // virtual-canvas space, called once from the constructor
    void layoutHotspots();   // resized(): aspect-fit the canvas into the component bounds
    int hotspotAt(juce::Point<float>) const;
    void drawHotspot(juce::Graphics&, const Hotspot&, bool learning, bool mapped, bool live) const;

    GHMidiProcessor& proc;
    juce::Array<Hotspot> hotspots;
    juce::Path bodyOutline, bodyOutlineScreen;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuickBindPanel)
};
