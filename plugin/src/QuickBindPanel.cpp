// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
#include "QuickBindPanel.h"
#include "Theme.h"
#include <BinaryData.h>
#include <cmath>

namespace {
struct SpriteEntry { int target; const char* data; int size; };
// the 16 controls with unambiguous 1:1 art in the sprite sheet -- see
// README's QuickBind section for why the other 3 in-scope targets
// (joystick X/Y, the solo modifier) aren't here
const SpriteEntry kSprites[] = {
    { GuitarService::LFretG,     BinaryData::Fret_green_PNG,       BinaryData::Fret_green_PNGSize },
    { GuitarService::LFretR,     BinaryData::Fret_red_PNG,         BinaryData::Fret_red_PNGSize },
    { GuitarService::LFretY,     BinaryData::Fret_yellow_PNG,      BinaryData::Fret_yellow_PNGSize },
    { GuitarService::LFretB,     BinaryData::Fret_blue_PNG,        BinaryData::Fret_blue_PNGSize },
    { GuitarService::LFretO,     BinaryData::Fret_orange_PNG,      BinaryData::Fret_orange_PNGSize },
    { GuitarService::LFretUpG,   BinaryData::Fret_solo_green_PNG,  BinaryData::Fret_solo_green_PNGSize },
    { GuitarService::LFretUpR,   BinaryData::Fret_solo_red_PNG,    BinaryData::Fret_solo_red_PNGSize },
    { GuitarService::LFretUpY,   BinaryData::Fret_solo_yellow_PNG, BinaryData::Fret_solo_yellow_PNGSize },
    { GuitarService::LFretUpB,   BinaryData::Fret_solo_blue_PNG,   BinaryData::Fret_solo_blue_PNGSize },
    { GuitarService::LFretUpO,   BinaryData::Fret_solo_orange_PNG, BinaryData::Fret_solo_orange_PNGSize },
    { GuitarService::LStrumDown, BinaryData::Strum_down_PNG,       BinaryData::Strum_down_PNGSize },
    { GuitarService::LStrumUp,   BinaryData::Strum_up_PNG,         BinaryData::Strum_up_PNGSize },
    { GuitarService::LTilt,      BinaryData::Tilt_PNG,             BinaryData::Tilt_PNGSize },
    { GuitarService::LWhammy,    BinaryData::WammyBar_PNG,         BinaryData::WammyBar_PNGSize },
    // Rock Band guitars label these Pause/Select; this plugin calls the same
    // two physical buttons Plus/Minus (mode-cycle), after the original GH
    // controller's own +/- labelling -- same controls, different name
    { GuitarService::LPlus,      BinaryData::Pause_PNG,            BinaryData::Pause_PNGSize },
    { GuitarService::LMinus,     BinaryData::Select_PNG,           BinaryData::Select_PNGSize },
};
// no natural spot on a guitar photo for these: a continuous stick axis, and
// an internal calibration bit (see GuitarService::LSoloModifier) -- small
// text-pill fallbacks in a row under the diagram instead
const int kFallbackTargets[] = { GuitarService::LStickX, GuitarService::LStickY, GuitarService::LSoloModifier };
constexpr int kNumFallback = 3;
}

QuickBindPanel::QuickBindPanel(GHMidiProcessor& processorToUse) : proc(processorToUse)
{
    setInterceptsMouseClicks(true, false);
    buildHotspots();
}

// ---------- layout ----------

namespace {
// the sprite sheet keys every control in the same flat highlight green
// (confirmed by inspecting rock-band-4.png, the reference composite) rather
// than each fret's real colour -- recolour the 10 fret sprites to match
// gemColours (everything else keeps its native green/neutral look, that
// isn't part of the 5-colour gem language anyway) so the diagram carries
// the same colour information the row table and highway already do.
juce::Image recoloured(const juce::Image& src, juce::Colour target)
{
    if (! src.isValid())
        return src;
    juce::Image out(juce::Image::ARGB, src.getWidth(), src.getHeight(), true);
    const juce::Image::BitmapData srcBd(src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData outBd(out, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < src.getHeight(); ++y)
        for (int x = 0; x < src.getWidth(); ++x)
        {
            const auto a = srcBd.getPixelColour(x, y).getAlpha();
            outBd.setPixelColour(x, y, target.withAlpha(a));
        }
    return out;
}
}

juce::Rectangle<float> QuickBindPanel::opaqueBounds(const juce::Image& img)
{
    if (! img.isValid())
        return {};
    const juce::Image::BitmapData bd(img, juce::Image::BitmapData::readOnly);
    constexpr int stride = 2;   // a coarse scan is plenty for a click hitbox -- 4x fewer pixel reads
    int minX = img.getWidth(), minY = img.getHeight(), maxX = -1, maxY = -1;
    for (int y = 0; y < img.getHeight(); y += stride)
        for (int x = 0; x < img.getWidth(); x += stride)
            if (bd.getPixelColour(x, y).getAlpha() > 24)
            {
                minX = juce::jmin(minX, x); maxX = juce::jmax(maxX, x);
                minY = juce::jmin(minY, y); maxY = juce::jmax(maxY, y);
            }
    if (maxX < minX || maxY < minY)   // fully transparent: shouldn't happen, fail open to the whole canvas
        return { 0.0f, 0.0f, (float) img.getWidth(), (float) img.getHeight() };
    return { (float) minX, (float) minY, (float) (maxX - minX + stride), (float) (maxY - minY + stride) };
}

void QuickBindPanel::buildHotspots()
{
    background = juce::ImageCache::getFromMemory(BinaryData::Background_PNG, BinaryData::Background_PNGSize);
    canvasW = (float) juce::jmax(1, background.getWidth());
    canvasH = (float) juce::jmax(1, background.getHeight());

    for (auto& s : kSprites)
    {
        Hotspot hs;
        hs.target = s.target;
        hs.label = GuitarService::targetName(s.target);
        hs.sprite = juce::ImageCache::getFromMemory(s.data, s.size);
        hs.bounds = opaqueBounds(hs.sprite);

        const bool isLowerFret = s.target >= GuitarService::LFretG && s.target <= GuitarService::LFretO;
        const bool isUpperFret = s.target >= GuitarService::LFretUpG && s.target <= GuitarService::LFretUpO;
        if (isLowerFret || isUpperFret)
        {
            const int gemIndex = isLowerFret ? s.target : s.target - GuitarService::LFretUpG;
            hs.sprite = recoloured(hs.sprite, Theme::gemColours[gemIndex]);
        }
        hotspots.add(hs);
    }

    constexpr float pillW = 150.0f, pillH = 28.0f, gap = 10.0f;
    const float totalW = (float) kNumFallback * pillW + (float) (kNumFallback - 1) * gap;
    float x = (canvasW - totalW) * 0.5f;
    const float y = canvasH + 14.0f;
    for (int t : kFallbackTargets)
    {
        Hotspot hs;
        hs.target = t;
        hs.label = GuitarService::targetName(t);
        hs.bounds = { x, y, pillW, pillH };
        hotspots.add(hs);
        x += pillW + gap;
    }
    canvasH += 14.0f + pillH + 6.0f;   // grow the virtual canvas to include the fallback row
}

void QuickBindPanel::resized()
{
    layoutHotspots();
}

void QuickBindPanel::layoutHotspots()
{
    const float w = (float) getWidth(), h = (float) getHeight();
    if (w <= 0.0f || h <= 0.0f || canvasW <= 0.0f || canvasH <= 0.0f)
        return;
    // aspect-fit the virtual canvas into whatever space the settings panel
    // actually gives this component, centred
    const float scale = juce::jmin(w / canvasW, h / canvasH);
    const float offX = (w - canvasW * scale) * 0.5f;
    const float offY = (h - canvasH * scale) * 0.5f;

    canvasScreenBounds = { offX, offY, canvasW * scale, canvasH * scale };
    for (auto& hs : hotspots)
        hs.screenBounds = { offX + hs.bounds.getX() * scale, offY + hs.bounds.getY() * scale,
                            hs.bounds.getWidth() * scale, hs.bounds.getHeight() * scale };
}

int QuickBindPanel::hotspotAt(juce::Point<float> p) const
{
    for (auto& hs : hotspots)
        if (hs.screenBounds.contains(p))
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
    if (hs.sprite.isValid())
    {
        float alpha = mapped ? 0.55f : 0.16f;
        if (live)
            alpha = 1.0f;
        if (learning)
        {
            // breathing pulse while this is the control actively being learned
            const float pulse = 0.5f + 0.5f * (float) std::sin(juce::Time::getMillisecondCounterHiRes() * 0.006);
            alpha = 0.35f + 0.65f * pulse;
        }
        // the sprite is full-canvas-sized (same as background, just this one
        // control opaque) -- draw it at the canvas's own screen rect, not
        // hs.screenBounds (that's only the click region, a crop of this image)
        g.setOpacity(alpha);
        g.drawImage(hs.sprite, canvasScreenBounds);
        g.setOpacity(1.0f);
        if (learning)
        {
            g.setColour(Theme::gold.withAlpha(0.9f));
            g.drawRect(hs.screenBounds, 2.0f);
        }
        return;
    }

    // fallback text pill (joystick X/Y, solo modifier -- no natural spot on a guitar photo)
    float fillAlpha = 0.10f, strokeAlpha = 0.35f, strokeW = 1.4f;
    if (mapped) { fillAlpha = 0.30f; strokeAlpha = 0.85f; strokeW = 1.8f; }
    if (live)   { fillAlpha = 0.55f; strokeAlpha = 1.0f;  strokeW = 2.2f; }
    juce::Colour drawColour = Theme::gold;
    if (learning)
    {
        const float pulse = 0.5f + 0.5f * (float) std::sin(juce::Time::getMillisecondCounterHiRes() * 0.006);
        fillAlpha = 0.25f + 0.45f * pulse;
        strokeAlpha = 1.0f;
        strokeW = 2.4f;
    }
    g.setColour(drawColour.withAlpha(fillAlpha));
    g.fillRoundedRectangle(hs.screenBounds, 6.0f);
    g.setColour(drawColour.withAlpha(strokeAlpha));
    g.drawRoundedRectangle(hs.screenBounds, 6.0f, strokeW);
    g.setColour(juce::Colours::white.withAlpha(mapped || learning || live ? 0.95f : 0.6f));
    g.setFont(juce::Font(juce::FontOptions(juce::jmin(11.0f, hs.screenBounds.getHeight() * 0.4f), juce::Font::bold)));
    g.drawText(hs.label, hs.screenBounds.reduced(3.0f, 0.0f), juce::Justification::centred);
}

void QuickBindPanel::paint(juce::Graphics& g)
{
    auto& svc = proc.guitar();

    if (background.isValid())
        g.drawImage(background, canvasScreenBounds);

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
    const auto hintArea = getLocalBounds().removeFromBottom(24).toFloat();
    g.setColour(Theme::panelBg.withAlpha(0.75f));
    g.fillRect(hintArea);
    g.setColour(Theme::gold);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText(hint, hintArea.toNearestInt(), juce::Justification::centred);
}
