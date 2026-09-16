// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
#include "QuickBindPanel.h"
#include "Theme.h"
#include <cmath>

QuickBindPanel::QuickBindPanel(GHMidiProcessor& processorToUse) : proc(processorToUse)
{
    setInterceptsMouseClicks(true, false);
    buildHotspots();
}

// ---------- layout (virtual 320x460 canvas, aspect-fit into the component) ----------

void QuickBindPanel::buildHotspots()
{
    // Rockband Mod (2026): plain text-label pills, an explicit placeholder
    // until real sprite art exists (see README.md's QuickBind section) --
    // this replaced a guitar-shaped diagram of coloured vector shapes
    // (circles for frets, a whammy-bar pill, etc.) with a simple uniform
    // grid of labelled buttons showing each control's full name. Same
    // hotspotAt()/mouseDown()/LEARN wiring as before, only the drawing and
    // layout changed.
    constexpr int cols = 3;
    constexpr float cellW = 112.0f, cellH = 30.0f, gapX = 6.0f, gapY = 6.0f;
    constexpr float startX = 6.0f, startY = 6.0f;

    int col = 0, row = 0;
    // QuickBind covers the guitar-shaped controls only (frets, strum,
    // whammy, joystick, plus/minus, Rock Band upper frets + tilt); pedal
    // targets are row-table only (see README.md) -- LPedalModeFwd is where
    // the guitar/Rock Band targets end
    for (int t = 0; t < GuitarService::LPedalModeFwd; ++t)
    {
        Hotspot hs;
        hs.target = t;
        hs.label = GuitarService::targetName(t);
        const float x = startX + (float) col * (cellW + gapX);
        const float y = startY + (float) row * (cellH + gapY);
        hs.shape.addRoundedRectangle(x, y, cellW, cellH, 6.0f);
        hotspots.add(hs);
        if (++col >= cols) { col = 0; ++row; }
    }

    const float gridW = startX * 2.0f + (float) cols * cellW + (float) (cols - 1) * gapX;
    const int rows = (GuitarService::LPedalModeFwd + cols - 1) / cols;
    const float gridH = startY * 2.0f + (float) rows * cellH + (float) (rows - 1) * gapY;
    bodyOutline.addRoundedRectangle(0.0f, 0.0f, gridW, gridH, 10.0f);
}

void QuickBindPanel::resized()
{
    layoutHotspots();
}

void QuickBindPanel::layoutHotspots()
{
    constexpr float canvasW = 360.0f, canvasH = 222.0f;   // matches buildHotspots()'s grid bounds
    const float w = (float) getWidth(), h = (float) getHeight();
    if (w <= 0.0f || h <= 0.0f)
        return;
    // aspect-fit the virtual canvas into whatever space the settings panel
    // actually gives this component, centred
    const float scale = juce::jmin(w / canvasW, h / canvasH);
    const float offX = (w - canvasW * scale) * 0.5f;
    const float offY = (h - canvasH * scale) * 0.5f;
    const auto xform = juce::AffineTransform::scale(scale).translated(offX, offY);

    bodyOutlineScreen = bodyOutline;
    bodyOutlineScreen.applyTransform(xform);
    for (auto& hs : hotspots)
    {
        hs.screenShape = hs.shape;
        hs.screenShape.applyTransform(xform);
    }
}

int QuickBindPanel::hotspotAt(juce::Point<float> p) const
{
    for (auto& hs : hotspots)
        if (hs.screenShape.contains(p))
            return hs.target;
    return -1;
}

// ---------- interaction ----------

void QuickBindPanel::mouseDown(const juce::MouseEvent& e)
{
    const int t = hotspotAt(e.position);
    if (t < 0)
        return;
    // exactly what the row table's LEARN button already does for this target
    auto& svc = proc.guitar();
    if (svc.getLearnTarget() == t)
        svc.cancelLearn();
    else
        svc.startLearn(t);
    repaint();
}

// ---------- drawing ----------

void QuickBindPanel::drawHotspot(juce::Graphics& g, const Hotspot& hs, bool learning, bool mapped, bool live) const
{
    const bool isLowerFret = hs.target >= 0 && hs.target <= GuitarService::LFretO;
    const bool isUpperFret = hs.target >= GuitarService::LFretUpG && hs.target <= GuitarService::LFretUpO;
    const int gemIndex = isLowerFret ? hs.target : (isUpperFret ? hs.target - GuitarService::LFretUpG : -1);
    const juce::Colour baseColour = gemIndex >= 0 ? Theme::gemColours[gemIndex] : Theme::gold;

    float fillAlpha = 0.10f, strokeAlpha = 0.35f, strokeW = 1.4f;
    if (mapped) { fillAlpha = 0.30f; strokeAlpha = 0.85f; strokeW = 1.8f; }
    if (live)   { fillAlpha = 0.55f; strokeAlpha = 1.0f;  strokeW = 2.2f; }
    juce::Colour drawColour = baseColour;
    if (learning)
    {
        // breathing pulse while this is the control actively being learned
        const float pulse = 0.5f + 0.5f * (float) std::sin(juce::Time::getMillisecondCounterHiRes() * 0.006);
        fillAlpha = 0.25f + 0.45f * pulse;
        strokeAlpha = 1.0f;
        strokeW = 2.4f;
        drawColour = Theme::gold;
    }

    g.setColour(drawColour.withAlpha(fillAlpha));
    g.fillPath(hs.screenShape);
    g.setColour(drawColour.withAlpha(strokeAlpha));
    g.strokePath(hs.screenShape, juce::PathStrokeType(strokeW));

    const auto bounds = hs.screenShape.getBounds();
    g.setColour(juce::Colours::white.withAlpha(mapped || learning || live ? 0.95f : 0.55f));
    // full control names (e.g. "JOYSTICK LEFT/RIGHT") need a smaller cap
    // than the old single-letter/short labels did to fit the pill width
    g.setFont(juce::Font(juce::FontOptions(juce::jmin(11.0f, bounds.getHeight() * 0.4f), juce::Font::bold)));
    g.drawText(hs.label, bounds.reduced(3.0f, 0.0f), juce::Justification::centred);
}

void QuickBindPanel::paint(juce::Graphics& g)
{
    auto& svc = proc.guitar();

    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.fillPath(bodyOutlineScreen);
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.strokePath(bodyOutlineScreen, juce::PathStrokeType(1.2f));

    const int learnTarget = svc.getLearnTarget();
    const int liveBits = svc.uiButtonBits.load();
    for (auto& hs : hotspots)
    {
        const bool learning = hs.target == learnTarget;
        const bool mapped = svc.describeMapping(hs.target) != "-";
        const bool live = (liveBits & (1 << hs.target)) != 0;
        drawHotspot(g, hs, learning, mapped, live);
    }

    juce::String hint;
    if (learnTarget >= 0)
        hint = (GuitarService::isAxisTarget(learnTarget) ? juce::String("Now SWEEP the ")
                                                           : juce::String("Now PRESS the "))
             + GuitarService::targetName(learnTarget);
    else
        hint = "Click a button on the diagram, then press it on your guitar.";
    g.setColour(Theme::gold);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText(hint, getLocalBounds().removeFromBottom(24), juce::Justification::centred);
}
