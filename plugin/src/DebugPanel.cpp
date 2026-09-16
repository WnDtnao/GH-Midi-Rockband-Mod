// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
#include "DebugPanel.h"
#include "Theme.h"

DebugPanel::DebugPanel(GHMidiProcessor& processorToUse, HighwayRenderer& highwayToUse)
    : proc(processorToUse), highway(highwayToUse)
{
    addAndMakeVisible(wireframeToggle);
    wireframeToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.85f));
    wireframeToggle.onClick = [this]
    {
        proc.guitar().debugWireframe = wireframeToggle.getToggleState();
    };
}

void DebugPanel::resized()
{
    wireframeToggle.setBounds(getLocalBounds().removeFromBottom(24).removeFromLeft(120).translated(4, -4));
}

void DebugPanel::drawByteGrid(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title,
                              const uint8_t* buf, int len) const
{
    g.setColour(Theme::gold);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawText(title, area.removeFromTop(16), juce::Justification::left);

    if (len <= 0)
    {
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText("(no device)", area, juce::Justification::topLeft);
        return;
    }

    constexpr int cols = 16;
    const int rows = (len + cols - 1) / cols;
    const float cellW = (float) area.getWidth() / (float) cols;
    const float cellH = juce::jmin(16.0f, (float) area.getHeight() / (float) juce::jmax(1, rows));
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    for (int i = 0; i < len; ++i)
    {
        const int col = i % cols, row = i / cols;
        const juce::Rectangle<float> cell((float) area.getX() + (float) col * cellW,
                                          (float) area.getY() + (float) row * cellH, cellW, cellH);
        g.setColour(juce::Colours::white.withAlpha(buf[i] != 0 ? 0.9f : 0.35f));
        g.drawText(juce::String::toHexString(buf[i]).paddedLeft('0', 2).toUpperCase(),
                   cell.toNearestInt(), juce::Justification::centred);
    }
}

void DebugPanel::paint(juce::Graphics& g)
{
    g.setColour(Theme::panelBg);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), Theme::panelCornerRadius);
    g.setColour(Theme::panelBorder);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), Theme::panelCornerRadius, 1.0f);

    auto& svc = proc.guitar();
    auto r = getLocalBounds().reduced(10);

    g.setColour(Theme::gold);
    g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    g.drawText("DEBUG", r.removeFromTop(22), juce::Justification::left);
    r.removeFromTop(4);

    // stat row: mode/key/octave/whammy/BPM/FPS
    {
        static const char* modeNames[4] = { "CHORDS", "NOTES", "SOLO", "CHART" };
        static const char* keyNames[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        const int mode = svc.uiMode.load() % 4;
        const int key = svc.uiKey.load() % 12;
        const juce::String stats = "mode " + juce::String(modeNames[mode])
            + "  key " + juce::String(keyNames[key])
            + "  oct " + juce::String(mode == 0 ? svc.uiEasyOct.load() : svc.uiOctave.load())
            + "  whammy " + juce::String(svc.uiWhammy.load(), 2)
            + "  bpm " + (svc.uiBpm.load() > 0.0f ? juce::String(svc.uiBpm.load(), 1) : juce::String("--"))
            + "  fps " + juce::String(highway.currentFps.load(), 0);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText(stats, r.removeFromTop(18), juce::Justification::left);
    }

    // which guitar/Rock Band controls are live right now, by name (covers
    // lower + upper frets, strum, plus/minus, whammy, stick, tilt)
    {
        const int bits = svc.uiButtonBits.load();
        juce::StringArray active;
        for (int t = 0; t < GuitarService::LPedalModeFwd; ++t)
            if (bits & (1 << t))
                active.add(GuitarService::targetName(t));
        const juce::String s = "active: " + (active.isEmpty() ? juce::String("(none)") : active.joinIntoString(", "));
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(10.5f)));
        g.drawText(s, r.removeFromTop(16), juce::Justification::left);
    }
    r.removeFromTop(6);

    const auto snap = svc.getDebugSnapshot();
    r.removeFromBottom(24);   // wireframeToggle lives here; its own bounds are set in resized()
    const int halfW = (r.getWidth() - 10) / 2;
    drawByteGrid(g, r.removeFromLeft(halfW), "GUITAR (raw HID bytes)", snap.guitar, snap.guitarLen);
    r.removeFromLeft(10);
    drawByteGrid(g, r, "PEDAL (raw HID bytes)", snap.pedal, snap.pedalLen);
}
