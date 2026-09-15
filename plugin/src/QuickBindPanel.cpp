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
    bodyOutline.addRoundedRectangle(10.0f, 10.0f, 300.0f, 350.0f, 24.0f);

    auto addCircle = [this](int target, float cx, float cy, float r, const juce::String& label)
    {
        Hotspot hs;
        hs.target = target;
        hs.shape.addEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        hs.label = label;
        hotspots.add(hs);
    };
    auto addRect = [this](int target, float x, float y, float w, float h, float corner, const juce::String& label)
    {
        Hotspot hs;
        hs.target = target;
        hs.shape.addRoundedRectangle(x, y, w, h, corner);
        hs.label = label;
        hotspots.add(hs);
    };

    // fret x positions, green..orange -- matches Theme::gemColours / the
    // "GRYBO" order used elsewhere (e.g. GuitarService::announceFrets)
    const float fretX[5] = { 64.0f, 120.0f, 176.0f, 232.0f, 288.0f };
    const char* fretLabel[5] = { "G", "R", "Y", "B", "O" };

    // Rock Band standard guitars only: an upper-fret row (same colours,
    // same order) above the usual lower row. Unlabelled as "upper" -- row
    // position plus colour already says which is which, same idea as the
    // guitar itself.
    for (int i = 0; i < 5; ++i)
        addCircle(GuitarService::LFretUpG + i, fretX[i], 46.0f, 18.0f, fretLabel[i]);
    for (int i = 0; i < 5; ++i)
        addCircle(i, fretX[i], 92.0f, 20.0f, fretLabel[i]);

    // strum bar: one rect hotspot per half
    addRect(GuitarService::LStrumUp,   60.0f, 132.0f, 212.0f, 26.0f, 8.0f, "UP");
    addRect(GuitarService::LStrumDown, 60.0f, 158.0f, 212.0f, 26.0f, 8.0f, "DN");

    // whammy bar
    addRect(GuitarService::LWhammy, 36.0f, 198.0f, 132.0f, 36.0f, 14.0f, "WHAMMY");

    // joystick: two chips -- left/right axis and up/down axis are separate
    // LEARN targets even though they're one physical stick
    addRect(GuitarService::LStickX, 212.0f, 250.0f, 38.0f, 26.0f, 8.0f, "L/R");
    addRect(GuitarService::LStickY, 254.0f, 250.0f, 38.0f, 26.0f, 8.0f, "U/D");

    // bottom row: minus / plus, and (Rock Band only) the tilt sensor
    addCircle(GuitarService::LMinus, 78.0f,  308.0f, 17.0f, "-");
    addCircle(GuitarService::LPlus,  134.0f, 308.0f, 17.0f, "+");
    addRect(GuitarService::LTilt, 172.0f, 292.0f, 128.0f, 32.0f, 10.0f, "TILT");
}

void QuickBindPanel::resized()
{
    layoutHotspots();
}

void QuickBindPanel::layoutHotspots()
{
    constexpr float canvasW = 320.0f, canvasH = 380.0f;
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
    g.setFont(juce::Font(juce::FontOptions(juce::jmin(14.0f, bounds.getHeight() * 0.4f), juce::Font::bold)));
    g.drawText(hs.label, bounds, juce::Justification::centred);
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
