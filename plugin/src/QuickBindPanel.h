// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
#pragma once
#include "PluginProcessor.h"

// QuickBind: the RB4 Stratocaster diagram (assets/sprites/rockband4, this
// mod's own art -- see README) with one clickable hotspot per mappable
// control, so binding a controller is "click the picture of the button,
// then press it" instead of working down the text row table one control at
// a time. This is purely a new view over GuitarService's existing LEARN
// engine -- clicking a hotspot calls the exact same startLearn()/
// cancelLearn() the row table's LEARN buttons already call, and mapped/live
// state is read from the exact same describeMapping()/uiButtonBits the row
// table already reads.
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
        juce::Image sprite;                  // full-canvas overlay PNG; invalid = no art, draw the fallback pill instead
        juce::Rectangle<float> bounds;        // native canvas space: opaque-pixel bbox for a sprite, or a laid-out pill rect for the fallback
        juce::Rectangle<float> screenBounds;  // resized(): bounds transformed into component space
        juce::String label;                   // fallback pill text only -- sprite targets don't need a label to draw
    };

    void buildHotspots();    // loads images, computes bboxes; called once from the constructor
    void layoutHotspots();   // resized(): aspect-fit the canvas into the component bounds
    int hotspotAt(juce::Point<float>) const;
    void drawHotspot(juce::Graphics&, const Hotspot&, bool learning, bool mapped, bool live) const;
    static juce::Rectangle<float> opaqueBounds(const juce::Image&);   // one-time pixel scan per sprite, for its click region

    GHMidiProcessor& proc;
    juce::Array<Hotspot> hotspots;
    juce::Image background;
    float canvasW = 1.0f, canvasH = 1.0f;              // background's native size, plus room for the fallback pill row below it
    juce::Rectangle<float> canvasScreenBounds;          // resized(): the whole canvas's transformed rect

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuickBindPanel)
};
