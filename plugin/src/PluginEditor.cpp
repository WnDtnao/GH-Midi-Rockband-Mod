// Rockband Mod (2026) -- modified from the original GH MIDI by Michael Loya
// (michaelloya.studio), AGPL-3.0. See README.md for the full credit / changes
// note (AGPL-3.0 §5a). This file adds: a DIAGRAM/TABLE toggle wiring in the
// QuickBind panel, and a MIDI-output picker for platforms with no virtual
// MIDI port (Windows).
#include "PluginEditor.h"
#include "Theme.h"
#include <BinaryData.h>
#include <cstdlib>

namespace {
// gemColours / openBarColour / gold / ghTypeface / ghFont now live in
// Theme.h so QuickBindPanel.cpp shares them without drift; brought in here
// unqualified so every existing call site below is unchanged.
using Theme::gemColours;
using Theme::openBarColour;
using Theme::gold;
using Theme::ghTypeface;
using Theme::ghFont;

const char* modeNames[4] = { "CHORDS", "NOTES", "SOLO", "CHART" };
const char* keyNames[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

const juce::String arrowL(juce::CharPointer_UTF8("\xE2\x97\x80"));   // left-pointing triangle
const juce::String arrowR(juce::CharPointer_UTF8("\xE2\x96\xB6"));   // right-pointing triangle
const juce::String arrowU(juce::CharPointer_UTF8("\xE2\x96\xB2"));   // up-pointing triangle
const juce::String arrowD(juce::CharPointer_UTF8("\xE2\x96\xBC"));   // down-pointing triangle
const juce::String arrowNE(juce::CharPointer_UTF8("\xE2\x86\x97"));  // north-east arrow

juce::Rectangle<int> panelBounds(int W, int H)
{
    // +180 tall vs. the original 500x480: the QuickBind DIAGRAM/TABLE toggle
    // row, a fixed-height bind area (diagram aspect-fits, table scrolls --
    // neither needs to grow with LTargetCount, so this height doesn't
    // either), the HOPO toggle, the Windows real-MIDI-output row, the two
    // pedal rows (HID device + MIDI input), and the 3D model style row.
    // (This panel's row count has grown across every milestone so far --
    // Phase G's settings reorganization is where this stops being "just add
    // another row".)
    return { W / 2 - 250, H / 2 - 330, 500, 660 };
}
juce::Rectangle<int> helpBounds(int W, int H)
{
    return { W / 2 - 230, H / 2 - 205, 460, 410 };
}
// bottom row: MODE / STRUM / KEY / OCTAVE plate — label row, value row with
// arrows, then a badge naming the guitar control (minus and plus side by side)
juce::Rectangle<int> hudPlate(int W, int H)
{
    return { 16, H - 72, W - 32, 60 };
}
} // namespace

GHMidiEditor::GHMidiEditor(GHMidiProcessor& p)
    : AudioProcessorEditor(p), proc(p), highway(p, *this), quickBind(p), debugPanel(p, highway)
{
    setOpaque(true);
    highway.setContext(&context);
    context.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
    context.setRenderer(&highway);
    context.setContinuousRepainting(true);
    context.setComponentPaintingEnabled(true);
    context.attachTo(*this);

    addAndMakeVisible(gearBtn);
    gearBtn.onClick = [this]
    {
        setHelpVisible(false);
        setPanelVisible(! panelOpen);
        proc.guitar().playSfx(AudioSfx::MusicSelect);
    };
    addAndMakeVisible(helpBtn);
    helpBtn.onClick = [this] { setPanelVisible(false); setHelpVisible(! helpOpen); };
    addAndMakeVisible(debugBtn);
    debugBtn.onClick = [this]
    {
        debugOpen = ! debugOpen;
        debugPanel.setVisible(debugOpen);
    };
    addChildComponent(debugPanel);

    // sustain selector: a radio pair, the active side in gold
    sustainLabel.setText("SUSTAIN", juce::dontSendNotification);
    sustainLabel.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    sustainLabel.setJustificationType(juce::Justification::centredRight);
    sustainLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.38f));
    addAndMakeVisible(sustainLabel);
    for (auto* b : { &sustainFretBtn, &sustainStrumBtn })
    {
        addAndMakeVisible(*b);
        b->setWantsKeyboardFocus(false);
        b->setClickingTogglesState(true);
        b->setRadioGroupId(1001);
        b->setColour(juce::TextButton::buttonOnColourId, gold);
        b->setColour(juce::TextButton::textColourOnId, juce::Colour(0xff15151f));
    }
    sustainFretBtn.setConnectedEdges(juce::Button::ConnectedOnRight);
    sustainStrumBtn.setConnectedEdges(juce::Button::ConnectedOnLeft);
    sustainFretBtn.onClick  = [this] { proc.guitar().strumSustain = false; proc.guitar().requestSave(); };
    sustainStrumBtn.onClick = [this] { proc.guitar().strumSustain = true;  proc.guitar().requestSave(); };
    syncSustainButtons();

    auto initArrow = [this](juce::TextButton& b, const juce::String& glyph, std::function<void()> fn)
    {
        addAndMakeVisible(b);
        b.setButtonText(glyph);
        b.setWantsKeyboardFocus(false);
        b.onClick = std::move(fn);
    };
    initArrow(modePrev, arrowL, [this] { proc.guitar().nudgeMode(-1); });
    initArrow(modeNext, arrowR, [this] { proc.guitar().nudgeMode(1); });
    initArrow(strumPrev, arrowL, [this] { proc.guitar().nudgeStrum(-1); });
    initArrow(strumNext, arrowR, [this] { proc.guitar().nudgeStrum(1); });
    initArrow(keyPrev,  arrowL, [this] { proc.guitar().nudgeKey(-1); });
    initArrow(keyNext,  arrowR, [this] { proc.guitar().nudgeKey(1); });
    initArrow(octPrev,  arrowL, [this] { proc.guitar().nudgeOctave(-1); });
    initArrow(octNext,  arrowR, [this] { proc.guitar().nudgeOctave(1); });

    auto initLabel = [this](juce::Label& l, const juce::String& text, float alpha = 0.85f)
    {
        addChildComponent(l);
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(alpha));
    };
    initLabel(deviceLabel, "Controller");
    initLabel(learnHint, "");
    learnHint.setColour(juce::Label::textColourId, gold);

    addChildComponent(deviceBox);
    deviceBox.setTextWhenNothingSelected("(pick your controller)");
    deviceBox.onChange = [this]
    {
        const int idx = deviceBox.getSelectedId() - 1;
        if (idx >= 0 && idx < shownDevices.size())
            proc.guitar().selectDevice(shownDevices[idx].vid, shownDevices[idx].pid);
    };
    addChildComponent(rescanBtn);
    rescanBtn.onClick = [this] { proc.guitar().requestDeviceScan(); };

    // mapping table: one row per control, LEARN / CLEAR each. Rows live in
    // rowTableContent, scrolled by rowTableViewport, so the list can keep
    // growing (Rock Band added 6 rows; pedals will add more) without
    // outgrowing the settings panel.
    addChildComponent(rowTableViewport);
    rowTableViewport.setViewedComponent(&rowTableContent, false);
    rowTableViewport.setScrollBarsShown(true, false);   // vertical only, shown only when needed
    for (auto* headerText : { "GUITAR", "ROCK BAND", "PEDAL (HID)", "PEDAL (MIDI)" })
    {
        auto* h = sectionHeaders.add(new juce::Label());
        rowTableContent.addAndMakeVisible(h);
        h->setText(headerText, juce::dontSendNotification);
        h->setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        h->setColour(juce::Label::textColourId, gold);
    }
    for (int t = 0; t < GuitarService::LTargetCount; ++t)
    {
        auto* name = rowNames.add(new juce::Label());
        rowTableContent.addAndMakeVisible(name);
        name->setText(GuitarService::targetName(t), juce::dontSendNotification);
        name->setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        const bool fretColoured = t < 5 || (t >= GuitarService::LFretUpG && t <= GuitarService::LFretUpO);
        const int gemIdx = t < 5 ? t : t - GuitarService::LFretUpG;
        name->setColour(juce::Label::textColourId,
                        fretColoured ? gemColours[gemIdx] : juce::Colours::white.withAlpha(0.85f));

        auto* desc = rowDescs.add(new juce::Label());
        rowTableContent.addAndMakeVisible(desc);
        desc->setFont(juce::Font(juce::FontOptions(12.0f)));
        desc->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.55f));

        auto* learn = rowLearn.add(new juce::TextButton("LEARN"));
        rowTableContent.addAndMakeVisible(learn);
        learn->onClick = [this, t]
        {
            auto& svc = proc.guitar();
            if (svc.getLearnTarget() == t)
                svc.cancelLearn();
            else
                svc.startLearn(t);
        };

        auto* clear = rowClear.add(new juce::TextButton("X"));
        rowTableContent.addAndMakeVisible(clear);
        clear->onClick = [this, t] { proc.guitar().clearMapping(t); };
    }

    // QuickBind diagram (default) vs. the row table above, a radio pair like
    // the FRET/STRUM sustain selector
    addChildComponent(quickBind);
    for (auto* b : { &diagramBtn, &tableBtn })
    {
        addChildComponent(*b);
        b->setWantsKeyboardFocus(false);
        b->setClickingTogglesState(true);
        b->setRadioGroupId(1002);
        b->setColour(juce::TextButton::buttonOnColourId, gold);
        b->setColour(juce::TextButton::textColourOnId, juce::Colour(0xff15151f));
    }
    diagramBtn.setConnectedEdges(juce::Button::ConnectedOnRight);
    tableBtn.setConnectedEdges(juce::Button::ConnectedOnLeft);
    diagramBtn.setToggleState(true, juce::dontSendNotification);
    diagramBtn.onClick = [this] { quickBindMode = true;  updateBindModeVisibility(); repaint(); };
    tableBtn.onClick   = [this] { quickBindMode = false; updateBindModeVisibility(); repaint(); };

    // Windows (or anywhere with no OS-level virtual MIDI port): pick a real
    // output to send to instead, e.g. a loopMIDI port
    initLabel(midiOutLabel, "MIDI out");
    addChildComponent(midiOutBox);
    midiOutBox.setTextWhenNothingSelected("(pick a MIDI output)");
    midiOutBox.onChange = [this]
    {
        const int idx = midiOutBox.getSelectedId() - 1;
        if (idx >= 0 && idx < shownMidiOuts.size())
            proc.guitar().selectMidiOutput(shownMidiOuts[idx].identifier);
    };

    // pedal-as-controller: a second HID device (shares the guitar's device
    // list/RESCAN above) and/or a MIDI input, each independently optional
    initLabel(pedalLabel, "Pedal (HID)");
    addChildComponent(pedalBox);
    pedalBox.setTextWhenNothingSelected("(none)");
    pedalBox.onChange = [this]
    {
        const int idx = pedalBox.getSelectedId() - 2;   // id 1 = "(none)"
        if (idx < 0)
            proc.guitar().selectPedalDevice(0, 0);
        else if (idx < shownDevices.size())
            proc.guitar().selectPedalDevice(shownDevices[idx].vid, shownDevices[idx].pid);
    };
    initLabel(midiInLabel, "Pedal (MIDI in)");
    addChildComponent(midiInBox);
    midiInBox.setTextWhenNothingSelected("(none)");
    midiInBox.onChange = [this]
    {
        const int idx = midiInBox.getSelectedId() - 2;   // id 1 = "(none)"
        if (idx < 0)
            proc.guitar().selectMidiInput({});
        else if (idx < shownMidiIns.size())
            proc.guitar().selectMidiInput(shownMidiIns[idx].identifier);
    };

    // 3D highway model style: hand-built (default) or YARG-derived textured
    initLabel(modelStyleLabel, "3D model style");
    for (auto* b : { &modelClassicBtn, &modelYargBtn })
    {
        addChildComponent(*b);
        b->setWantsKeyboardFocus(false);
        b->setClickingTogglesState(true);
        b->setRadioGroupId(1003);
        b->setColour(juce::TextButton::buttonOnColourId, gold);
        b->setColour(juce::TextButton::textColourOnId, juce::Colour(0xff15151f));
    }
    modelClassicBtn.setConnectedEdges(juce::Button::ConnectedOnRight);
    modelYargBtn.setConnectedEdges(juce::Button::ConnectedOnLeft);
    modelClassicBtn.setToggleState(proc.guitar().modelStyle.load() == GuitarService::ModelClassic, juce::dontSendNotification);
    modelYargBtn.setToggleState(proc.guitar().modelStyle.load() == GuitarService::ModelYarg, juce::dontSendNotification);
    modelClassicBtn.onClick = [this] { proc.guitar().modelStyle = GuitarService::ModelClassic; proc.guitar().requestSave(); };
    modelYargBtn.onClick   = [this] { proc.guitar().modelStyle = GuitarService::ModelYarg;    proc.guitar().requestSave(); };

    addChildComponent(vmidiToggle);
    vmidiToggle.onClick = [this]
    {
        proc.guitar().virtualMidiOn = vmidiToggle.getToggleState();
        proc.guitar().requestSave();
        proc.guitar().playSfx(vmidiToggle.getToggleState() ? AudioSfx::CheckboxOn : AudioSfx::CheckboxOff);
    };
    addChildComponent(hopoToggle);
    hopoToggle.onClick = [this]
    {
        proc.guitar().hopoEnabled = hopoToggle.getToggleState();
        proc.guitar().requestSave();
        proc.guitar().playSfx(hopoToggle.getToggleState() ? AudioSfx::CheckboxOn : AudioSfx::CheckboxOff);
    };
    // Practice mode (Rockband Mod, MVP scope -- see README)
    initLabel(practiceLabel, "(no song loaded)", 0.55f);
    practiceLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    addChildComponent(loadSongBtn);
    loadSongBtn.onClick = [this]
    {
        songChooser = std::make_unique<juce::FileChooser>(
            "Load a song for Practice mode", juce::File(), "*.chart;*.mid;*.midi");
        songChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    proc.guitar().loadPracticeSong(f);
                    refreshPracticeLabel();
                }
            });
    };
    addChildComponent(practicePlayBtn);
    practicePlayBtn.onClick = [this]
    {
        auto& svc = proc.guitar();
        svc.setPracticePlaying(! svc.isPracticePlaying());
        proc.guitar().playSfx(AudioSfx::MusicSelect);
    };

    addChildComponent(closeBtn);
    closeBtn.onClick = [this]
    {
        proc.guitar().cancelLearn();
        setPanelVisible(false);
    };

    addChildComponent(helpCloseBtn);
    helpCloseBtn.onClick = [this] { setHelpVisible(false); };
    addChildComponent(moreBtn);
    moreBtn.setButtonText("MORE HELP  " + arrowNE);
    moreBtn.setColour(juce::TextButton::buttonColourId, gold);
    moreBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff15151f));
    moreBtn.onClick = []
    {
        juce::URL("https://michaelloya.studio/gh-midi").launchInDefaultBrowser();
    };

    if (auto* env = std::getenv("GHMIDI_HUDSNAP"); env != nullptr && juce::JUCEApplicationBase::isStandaloneApp())
        hudSnapDir = env;

    setSize(1280, 720);   // 16:9 -- was a near-square 720x730
    startTimerHz(30);
}

void GHMidiEditor::saveHudSnapshot(const juce::String& name)
{
    juce::Image img(juce::Image::ARGB, getWidth(), getHeight(), true);   // transparent: composite over a GL frame
    {
        juce::Graphics g(img);
        paintEntireComponent(g, false);
    }
    const auto file = juce::File(hudSnapDir).getChildFile("hud_" + name + ".png");
    file.deleteFile();
    juce::FileOutputStream os(file);
    if (os.openedOk())
        juce::PNGImageFormat().writeImageToStream(img, os);
}

GHMidiEditor::~GHMidiEditor()
{
    context.detach();
}

void GHMidiEditor::setPanelVisible(bool visible)
{
    panelOpen = visible;
    for (auto* c : { (juce::Component*) &sustainLabel, (juce::Component*) &sustainFretBtn, (juce::Component*) &sustainStrumBtn })
        c->setVisible(! visible && ! helpOpen);
    for (auto* c : std::initializer_list<juce::Component*> {
             &deviceLabel, &deviceBox, &rescanBtn, &vmidiToggle, &hopoToggle,
             &closeBtn, &diagramBtn, &tableBtn,
             &pedalLabel, &pedalBox, &midiInLabel, &midiInBox,
             &modelStyleLabel, &modelClassicBtn, &modelYargBtn,
             &practiceLabel, &loadSongBtn, &practicePlayBtn })
        c->setVisible(visible);
    // Windows (or anywhere else a virtual MIDI port couldn't be created):
    // show the real-output picker. Runtime check, not #ifdef.
    const bool showMidiOut = visible && ! proc.guitar().hasVirtualPort.load();
    midiOutLabel.setVisible(showMidiOut);
    midiOutBox.setVisible(showMidiOut);
    updateBindModeVisibility();
    if (visible)
    {
        proc.guitar().requestDeviceScan();
        vmidiToggle.setToggleState(proc.guitar().virtualMidiOn.load(), juce::dontSendNotification);
        hopoToggle.setToggleState(proc.guitar().hopoEnabled.load(), juce::dontSendNotification);
        refreshMappingRows();
        refreshPedalBox();
        refreshMidiInBox();
        if (showMidiOut)
            refreshMidiOutBox();
        refreshPracticeLabel();
    }
    else
        proc.guitar().cancelLearn();
    updateHudButtons();
    repaint();
}

void GHMidiEditor::updateBindModeVisibility()
{
    quickBind.setVisible(panelOpen && quickBindMode);
    const bool showRow = panelOpen && ! quickBindMode;
    learnHint.setVisible(showRow);   // QuickBind draws its own equivalent hint
    rowTableViewport.setVisible(showRow);   // rows themselves stay visible inside rowTableContent
}

void GHMidiEditor::syncSustainButtons()
{
    const bool strum = proc.guitar().strumSustain.load();
    sustainStrumBtn.setToggleState(strum,  juce::dontSendNotification);
    sustainFretBtn.setToggleState(! strum, juce::dontSendNotification);
}

void GHMidiEditor::setHelpVisible(bool visible)
{
    helpOpen = visible;
    for (auto* c : { (juce::Component*) &sustainLabel, (juce::Component*) &sustainFretBtn, (juce::Component*) &sustainStrumBtn })
        c->setVisible(! visible && ! panelOpen);
    helpCloseBtn.setVisible(visible);
    moreBtn.setVisible(visible);
    updateHudButtons();
    repaint();
}

void GHMidiEditor::updateHudButtons()
{
    const bool show = ! panelOpen && ! helpOpen;
    for (auto* c : std::initializer_list<juce::Component*> {
             &modePrev, &modeNext, &strumPrev, &strumNext, &keyPrev, &keyNext, &octPrev, &octNext })
        c->setVisible(show);
    // octave and strum arrows grey out at the ends of their ranges; in CHART
    // mode strum, key and octave don't apply at all
    const int mode = proc.guitar().uiMode.load() % 4;
    const bool chart = mode == GuitarService::Chart;
    const int oct = mode == 0 ? proc.guitar().uiEasyOct.load() : proc.guitar().uiOctave.load();
    keyPrev.setEnabled(! chart);
    keyNext.setEnabled(! chart);
    octPrev.setEnabled(! chart && oct > GuitarService::kOctaveMin);
    octNext.setEnabled(! chart && oct < GuitarService::kOctaveMax);
    const int ms = proc.guitar().strumRollMs.load();
    strumPrev.setEnabled(! chart && ms > 0);
    strumNext.setEnabled(! chart && ms < GuitarService::kStrumMaxMs);
}

void GHMidiEditor::refreshDeviceBox()
{
    shownDevices = proc.guitar().getDevices();
    deviceBox.clear(juce::dontSendNotification);
    int selected = 0;
    for (int i = 0; i < shownDevices.size(); ++i)
    {
        deviceBox.addItem(shownDevices[i].label, i + 1);
        if (shownDevices[i].vid == proc.guitar().currentVid()
            && shownDevices[i].pid == proc.guitar().currentPid())
            selected = i + 1;
    }
    if (selected > 0)
        deviceBox.setSelectedId(selected, juce::dontSendNotification);
    refreshPedalBox();   // shares this same device list
}

void GHMidiEditor::refreshMidiOutBox()
{
    shownMidiOuts = proc.guitar().getMidiOutputs();
    midiOutBox.clear(juce::dontSendNotification);
    const auto currentId = proc.guitar().currentMidiOutId();
    int selected = 0;
    for (int i = 0; i < shownMidiOuts.size(); ++i)
    {
        midiOutBox.addItem(shownMidiOuts[i].name, i + 1);
        if (shownMidiOuts[i].identifier == currentId)
            selected = i + 1;
    }
    if (selected > 0)
        midiOutBox.setSelectedId(selected, juce::dontSendNotification);
}

void GHMidiEditor::refreshPedalBox()
{
    // shownDevices is refreshed by refreshDeviceBox() (shared RESCAN/scan
    // with the guitar); this just re-populates the pedal's own combo from it
    pedalBox.clear(juce::dontSendNotification);
    pedalBox.addItem("(none)", 1);
    int selected = 1;
    for (int i = 0; i < shownDevices.size(); ++i)
    {
        pedalBox.addItem(shownDevices[i].label, i + 2);
        if (shownDevices[i].vid == proc.guitar().currentPedalVid()
            && shownDevices[i].pid == proc.guitar().currentPedalPid()
            && proc.guitar().currentPedalVid() != 0)
            selected = i + 2;
    }
    pedalBox.setSelectedId(selected, juce::dontSendNotification);
}

void GHMidiEditor::refreshMidiInBox()
{
    shownMidiIns = proc.guitar().getMidiInputs();
    midiInBox.clear(juce::dontSendNotification);
    midiInBox.addItem("(none)", 1);
    int selected = 1;
    const auto currentId = proc.guitar().currentMidiInId();
    for (int i = 0; i < shownMidiIns.size(); ++i)
    {
        midiInBox.addItem(shownMidiIns[i].name, i + 2);
        if (shownMidiIns[i].identifier == currentId && currentId.isNotEmpty())
            selected = i + 2;
    }
    midiInBox.setSelectedId(selected, juce::dontSendNotification);
}

void GHMidiEditor::refreshMappingRows()
{
    for (int t = 0; t < rowDescs.size(); ++t)
        rowDescs[t]->setText(proc.guitar().describeMapping(t), juce::dontSendNotification);
}

void GHMidiEditor::refreshPracticeLabel()
{
    auto& svc = proc.guitar();
    const bool playing = svc.isPracticePlaying();
    practicePlayBtn.setButtonText(playing ? "STOP" : "PLAY");
    practicePlayBtn.setEnabled(svc.hasPracticeSong());
    if (! svc.hasPracticeSong())
    {
        practiceLabel.setText("(no song loaded)", juce::dontSendNotification);
        return;
    }
    juce::String s = svc.practiceSongTitle();
    if (playing)
    {
        const int elapsed = juce::jmax(0, (int) svc.practicePlayheadSecs());
        const int total = (int) svc.practiceSongLengthSecs();
        s << juce::String::formatted("  %d:%02d / %d:%02d", elapsed / 60, elapsed % 60, total / 60, total % 60);
    }
    practiceLabel.setText(s, juce::dontSendNotification);
}

void GHMidiEditor::timerCallback()
{
    syncSustainButtons();   // cheap, and keeps the pair honest after a state restore
    if (! isShowing())
        return;
    if (hudSnapDir.isNotEmpty())
    {
        // scripted tour of the HUD states (dev only, standalone only)
        auto& svc = proc.guitar();
        switch (++hudSnapTick)
        {
            case 45:  saveHudSnapshot("chords"); break;
            case 50:  svc.uiMode = 1; svc.uiKey = 7; svc.uiOctave = 12; break;
            case 60:  saveHudSnapshot("notes"); break;
            case 65:  svc.uiMode = 2; svc.uiOctave = 36;
                      svc.uiButtonBits = (1 << GuitarService::LMinus) | (1 << GuitarService::LStickY)
                                       | (1 << GuitarService::LPlus);
                      break;
            case 75:  saveHudSnapshot("solo_maxoct"); break;
            case 77:  svc.uiMode = 3; svc.uiButtonBits = 0; svc.announceFrets(0x03); break;
            case 79:  saveHudSnapshot("chart"); break;
            case 82:  setHelpVisible(true); break;
            case 90:  saveHudSnapshot("help"); break;
            case 95:  setHelpVisible(false); setPanelVisible(true); break;
            case 105: saveHudSnapshot("settings"); break;
            case 135: juce::JUCEApplicationBase::quit(); break;
            default: break;
        }
    }
    updateHudButtons();
    if (panelOpen)
    {
        refreshPracticeLabel();
        auto& svc = proc.guitar();
        if (svc.getDeviceListVersion() != lastDevVersion)
        {
            lastDevVersion = svc.getDeviceListVersion();
            refreshDeviceBox();
        }
        if (svc.getMapVersion() != lastMapVersion)
        {
            lastMapVersion = svc.getMapVersion();
            refreshMappingRows();
        }
        const int liveBits = svc.uiButtonBits.load();
        for (int t = 0; t < rowNames.size(); ++t)
        {
            // uiButtonBits only ever populates guitar/Rock Band target bits
            // (t < 31); guard the shift so LTargetCount growing past 32
            // (e.g. LSoloModifier) can never shift by >= the int's width
            const bool lit = t < 31 && (liveBits & (1 << t)) != 0;
            rowNames[t]->setColour(juce::Label::backgroundColourId,
                                   lit ? gold.withAlpha(0.22f) : juce::Colours::transparentBlack);
            rowDescs[t]->setColour(juce::Label::textColourId,
                                   juce::Colours::white.withAlpha(lit ? 0.95f : 0.55f));
        }
        const int lt = svc.getLearnTarget();
        if (lt != lastLearnTarget)
        {
            lastLearnTarget = lt;
            for (int t = 0; t < rowLearn.size(); ++t)
                rowLearn[t]->setButtonText(t == lt ? "..." : "LEARN");
            learnHint.setText(lt < 0 ? juce::String()
                                     : (GuitarService::isAxisTarget(lt)
                                            ? "Now SWEEP the " + GuitarService::targetName(lt)
                                            : "Now PRESS the " + GuitarService::targetName(lt)),
                              juce::dontSendNotification);
        }
    }
    repaint();
}

void GHMidiEditor::resized()
{
    highway.setViewSize(getWidth(), getHeight());
    gearBtn.setBounds(getWidth() - 104, 16, 92, 28);
    helpBtn.setBounds(getWidth() - 140, 16, 30, 28);
    debugBtn.setBounds(getWidth() - 214, 16, 68, 28);
    debugPanel.setBounds(16, 56, juce::jmin(460, getWidth() - 32), 170);
    // SUSTAIN  [FRET][STRUM]  -- flush right with SETTINGS, one row below it
    sustainStrumBtn.setBounds(getWidth() - 104 + 46, 50, 46, 22);
    sustainFretBtn.setBounds(getWidth() - 104, 50, 46, 22);
    sustainLabel.setBounds(getWidth() - 200, 50, 90, 22);

    // MODE / STRUM / KEY / OCTAVE plate: arrows either side of each value
    {
        const auto plate = hudPlate(getWidth(), getHeight());
        const int colW = plate.getWidth() / 4;
        juce::TextButton* arrows[4][2] = { { &modePrev, &modeNext },
                                           { &strumPrev, &strumNext },
                                           { &keyPrev, &keyNext },
                                           { &octPrev, &octNext } };
        for (int i = 0; i < 4; ++i)
        {
            auto row = juce::Rectangle<int>(plate.getX() + i * colW, plate.getY() + 19, colW, 22).reduced(6, 0);
            arrows[i][0]->setBounds(row.removeFromLeft(24));
            arrows[i][1]->setBounds(row.removeFromRight(24));
        }
    }

    // settings panel
    const auto pb = panelBounds(getWidth(), getHeight());
    closeBtn.setBounds(pb.getRight() - 40, pb.getY() + 12, 28, 26);
    auto r = pb.reduced(20);
    r.removeFromTop(34);  // painted title
    auto devRow = r.removeFromTop(24);
    deviceLabel.setBounds(devRow.removeFromLeft(76));
    rescanBtn.setBounds(devRow.removeFromRight(72));
    devRow.removeFromRight(6);
    deviceBox.setBounds(devRow);
    r.removeFromTop(10);
    // DIAGRAM/TABLE toggle, right-aligned
    {
        auto toggleRow = r.removeFromTop(24);
        auto toggleGroup = toggleRow.removeFromRight(168);
        diagramBtn.setBounds(toggleGroup.removeFromLeft(84).reduced(1, 0));
        tableBtn.setBounds(toggleGroup.reduced(1, 0));
    }
    r.removeFromTop(6);
    learnHint.setBounds(r.removeFromTop(18));
    r.removeFromTop(4);
    // QuickBind's diagram and the row table share this same region -- only
    // one is visible at a time (see updateBindModeVisibility()). Fixed
    // height, independent of LTargetCount: the table scrolls, the diagram
    // aspect-fits, so neither needs the panel to grow as more targets
    // (Rock Band, then pedals) are added.
    auto bindArea = r.removeFromTop(240);
    quickBind.setBounds(bindArea);
    rowTableViewport.setBounds(bindArea);
    {
        const int rowH = 24;
        // section headers ahead of GUITAR/ROCK BAND/PEDAL (HID)/PEDAL (MIDI)
        // groups -- see the constructor for where sectionHeaders is built
        const int contentW = bindArea.getWidth() - rowTableViewport.getScrollBarThickness();
        constexpr int headerH = 20;
        int y = 0, headerIdx = 0;
        auto maybeHeader = [&](int t)
        {
            if (t == 0 || t == GuitarService::LFretUpG || t == GuitarService::LPedalModeFwd
                || t == GuitarService::LMidiPedalModeFwd)
            {
                if (headerIdx < sectionHeaders.size())
                    sectionHeaders[headerIdx]->setBounds(2, y, contentW, headerH);
                ++headerIdx;
                y += headerH;
            }
        };
        for (int t = 0; t < rowNames.size(); ++t)
        {
            maybeHeader(t);
            auto row = juce::Rectangle<int>(0, y, contentW, 22);
            rowNames[t]->setBounds(row.removeFromLeft(136));
            rowClear[t]->setBounds(row.removeFromRight(30).reduced(0, 1));
            row.removeFromRight(4);
            rowLearn[t]->setBounds(row.removeFromRight(62).reduced(0, 1));
            rowDescs[t]->setBounds(row);
            y += rowH;
        }
        rowTableContent.setSize(contentW, y);
    }
    r.removeFromTop(8);
    vmidiToggle.setBounds(r.removeFromTop(24));
    r.removeFromTop(6);
    hopoToggle.setBounds(r.removeFromTop(24));
    r.removeFromTop(6);
    {
        auto midiRow = r.removeFromTop(24);
        midiOutLabel.setBounds(midiRow.removeFromLeft(76));
        midiOutBox.setBounds(midiRow);
    }
    r.removeFromTop(6);
    {
        auto pedalRow = r.removeFromTop(24);
        pedalLabel.setBounds(pedalRow.removeFromLeft(96));
        pedalBox.setBounds(pedalRow);
    }
    r.removeFromTop(6);
    {
        auto midiInRow = r.removeFromTop(24);
        midiInLabel.setBounds(midiInRow.removeFromLeft(96));
        midiInBox.setBounds(midiInRow);
    }
    r.removeFromTop(6);
    {
        auto modelRow = r.removeFromTop(24);
        modelStyleLabel.setBounds(modelRow.removeFromLeft(96));
        modelClassicBtn.setBounds(modelRow.removeFromLeft(84).reduced(1, 0));
        modelYargBtn.setBounds(modelRow.removeFromLeft(84).reduced(1, 0));
    }
    r.removeFromTop(6);
    {
        auto songRow = r.removeFromTop(24);
        practicePlayBtn.setBounds(songRow.removeFromRight(72));
        songRow.removeFromRight(6);
        loadSongBtn.setBounds(songRow.removeFromLeft(120));
    }
    r.removeFromTop(4);
    practiceLabel.setBounds(r.removeFromTop(16));

    // help overlay
    auto hb = helpBounds(getWidth(), getHeight());
    moreBtn.setBounds(hb.getCentreX() - 90, hb.getBottom() - 78, 180, 32);
    helpCloseBtn.setBounds(hb.getRight() - 40, hb.getY() + 12, 28, 26);
}

void GHMidiEditor::paint(juce::Graphics& g)
{
    const float W = (float) getWidth();
    const float H = (float) getHeight();
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(ghFont(28.0f));
    g.drawText("GH MIDI", 20, 14, 124, 32, juce::Justification::left);

    // small gold pill naming the guitar control that changes a setting;
    // fills solid while that control is being touched
    const int liveBits = proc.guitar().uiButtonBits.load();
    auto badge = [&](juce::Rectangle<float> r, const juce::String& text, bool lit)
    {
        g.setColour(gold.withAlpha(lit ? 0.95f : 0.10f));
        g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
        g.setColour(gold.withAlpha(lit ? 1.0f : 0.45f));
        g.drawRoundedRectangle(r, r.getHeight() * 0.5f, 1.0f);
        g.setColour(lit ? juce::Colour(0xff15151f) : gold.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.drawText(text, r.toNearestInt(), juce::Justification::centred);
    };

    // chord/note name pop
    {
        const float age = (float) (now - proc.guitar().lastPlayedAt.load());
        if (age >= 0.0f && age < 1.6f)
        {
            const float pop = 1.0f + 0.35f * std::exp(-age * 9.0f);
            const float a = juce::jlimit(0.0f, 1.0f, 1.8f - age * 1.2f);
            g.setFont(ghFont(48.0f * pop));
            const int mask = proc.guitar().lastPlayedFrets.load();
            if (mask != 0)   // CHART: one letter per held fret, in that fret's colour
            {
                const float adv = 40.0f * pop;
                int n = 0;
                for (int i = 0; i < 5; ++i)
                    n += (mask >> i) & 1;
                float x = W * 0.5f - adv * (float) n * 0.5f;
                for (int i = 0; i < 5; ++i)
                    if (mask & (1 << i))
                    {
                        g.setColour(gemColours[i].withAlpha(a));
                        g.drawText(juce::String::charToString((juce::juce_wchar) "GRYBO"[i]),
                                   (int) x, (int) (H * 0.08f), (int) adv, 58, juce::Justification::centred);
                        x += adv;
                    }
            }
            else
            {
                g.setColour(gold.withAlpha(a));
                g.drawText(proc.guitar().getLastPlayed(), 0, (int) (H * 0.08f), getWidth(), 58,
                           juce::Justification::centred);
            }
        }
    }

    // MODE / STRUM / KEY / OCTAVE plate: current settings, arrows either side (buttons)
    {
        const auto plate = hudPlate(getWidth(), getHeight());
        g.setColour(Theme::panelBg.withAlpha(0.85f));
        g.fillRoundedRectangle(plate.toFloat(), Theme::panelCornerRadius);
        g.setColour(Theme::panelBorder);
        g.drawRoundedRectangle(plate.toFloat(), Theme::panelCornerRadius, 1.0f);
        const int mode = proc.guitar().uiMode.load() % 4;
        const int uiK = proc.guitar().uiKey.load() % 12;
        const bool notes = mode == 1;
        const bool chart = mode == GuitarService::Chart;
        const int octSemis = mode == 0 ? proc.guitar().uiEasyOct.load() : proc.guitar().uiOctave.load();
        const int oct = octSemis / 12;
        juce::String vals[4] = { modeNames[mode],
                                 juce::String(proc.guitar().strumRollMs.load()) + " ms",
                                 keyNames[uiK],
                                 oct > 0 ? "+" + juce::String(oct) : juce::String(oct) };
        if (notes)   // NOTES: show the lowest playable note, key and octave folded in
        {
            const int base = 40 + uiK + proc.guitar().uiOctave.load();
            vals[2] = juce::String(keyNames[base % 12]) + juce::String(base / 12 - 1);
        }
        const char* labels[4] = { "MODE", "STRUM SPREAD", notes ? "BASE" : "KEY", "OCTAVE" };
        // the guitar control for each column, mirrored on the on-screen arrows
        const juce::String guitarCtl[4] = { "MINUS", "PLUS",
                                            "JOYSTICK " + arrowL + " " + arrowR,
                                            "JOYSTICK " + arrowU + " " + arrowD };
        const float badgeW[4] = { 56.0f, 56.0f, 98.0f, 98.0f };
        const int badgeBit[4] = { GuitarService::LMinus, GuitarService::LPlus,
                                  GuitarService::LStickX, GuitarService::LStickY };
        const int colW = plate.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            const int cx = plate.getX() + i * colW;
            const bool dimmed = chart && i > 0;   // CHART: strum, key and octave don't apply
            if (dimmed)
                g.beginTransparencyLayer(0.3f);
            g.setColour(juce::Colours::white.withAlpha(0.38f));
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawText(labels[i], cx, plate.getY() + 6, colW, 10, juce::Justification::centred);
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.setFont(ghFont(18.0f));
            g.drawText(vals[i], cx + 30, plate.getY() + 20, colW - 60, 20, juce::Justification::centred);
            badge({ (float) cx + ((float) colW - badgeW[i]) * 0.5f, (float) plate.getY() + 43.0f,
                    badgeW[i], 14.0f },
                  guitarCtl[i], (liveBits & (1 << badgeBit[i])) != 0);
            if (dimmed)
                g.endTransparencyLayer();
        }
    }

    // status banner
    const bool found = proc.guitar().guitarFound.load();
    const bool empty = proc.guitar().adapterEmpty.load();
    if ((! found || empty) && ! panelOpen && ! helpOpen)
    {
        g.setColour(juce::Colour(0xd0e23b3b));
        g.fillRoundedRectangle(24.0f, H * 0.46f, W - 48.0f, 28.0f, 6.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.drawText(! found ? "CONTROLLER NOT FOUND - plug it in, or open SETTINGS to pick a device"
                           : "ADAPTER CAN'T SEE THE GUITAR - unplug USB, reseat plug, replug",
                   0, (int) (H * 0.46f), getWidth(), 28, juce::Justification::centred);
    }
    else if (! proc.guitar().introRevealed.load() && ! panelOpen && ! helpOpen)
    {
        // Rockband Mod: GH3-style intro -- the highway stays hidden (see
        // HighwayRenderer) until this first press
        g.setColour(Theme::panelBg);
        g.fillRoundedRectangle(24.0f, H * 0.46f, W - 48.0f, 28.0f, 6.0f);
        g.setColour(gold);
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.drawText("PRESS PLUS (OR PAUSE) ON YOUR GUITAR TO START",
                   0, (int) (H * 0.46f), getWidth(), 28, juce::Justification::centred);
    }

    auto drawOverlayFrame = [&](juce::Rectangle<float> pb, const juce::String& title)
    {
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillAll();
        g.setColour(Theme::panelBg);
        g.fillRoundedRectangle(pb, Theme::panelCornerRadius);
        g.setColour(Theme::panelBorder);
        g.drawRoundedRectangle(pb, Theme::panelCornerRadius, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.setFont(ghFont(24.0f));
        g.drawText(title, (int) pb.getX() + 20, (int) pb.getY() + 12,
                   (int) pb.getWidth() - 40, 26, juce::Justification::left);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText("Rockband Mod v2.0", (int) pb.getX() + 20, (int) pb.getBottom() - 30,
                   (int) pb.getWidth() - 40, 16, juce::Justification::right);
    };

    if (panelOpen)
    {
        drawOverlayFrame(panelBounds(getWidth(), getHeight()).toFloat(), "SETTINGS");
    }
    else if (helpOpen)
    {
        auto hb = helpBounds(getWidth(), getHeight());
        drawOverlayFrame(hb.toFloat(), "HOW TO PLAY");
        auto rowY = hb.getY() + 52;
        auto section = [&](const juce::String& s)
        {
            g.setColour(gold);
            g.setFont(ghFont(17.0f));
            g.drawText(s, hb.getX() + 30, rowY, hb.getWidth() - 60, 20, juce::Justification::left);
            rowY += 24;
        };
        auto line = [&](const juce::String& head, const juce::String& body, int headW = 116)
        {
            g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawText(head, hb.getX() + 34, rowY, headW, 16, juce::Justification::left);
            g.setFont(juce::Font(juce::FontOptions(12.5f)));
            g.setColour(juce::Colours::white.withAlpha(0.62f));
            g.drawText(body, hb.getX() + 36 + headW, rowY, hb.getWidth() - 70 - headW, 16,
                       juce::Justification::left);
            rowY += 20;
        };
        section("CONTROLS");
        line("FRETS + STRUM", "play - up and down strums feel different");
        line("WHAMMY", "bend the note");
        line("MINUS", "switch mode");
        line("PLUS", "tap through strum speeds (0-50ms)");
        line("JOYSTICK", "left/right = key - up/down = octave");
        line("ON SCREEN", "the bottom arrows do all of the above too");
        rowY += 8;
        section("MODES");
        line("CHORDS", "every fret is a chord in your key. can't miss", 70);
        line("SOLO", "5 scale notes, strum any stack. can't miss", 70);
        line("NOTES", "fret combos pick all 32 notes. full control", 70);
        line("CHART", "frets = Clone Hero lanes. record a rough chart", 70);
    }

}
