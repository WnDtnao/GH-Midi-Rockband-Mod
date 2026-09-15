#pragma once
#include "PluginProcessor.h"
#include "HighwayRenderer.h"
#include "QuickBindPanel.h"

// 3D highway underneath (OpenGL); HUD, settings panel (device + QuickBind
// diagram / mapping table) and help overlay composited on top.
class GHMidiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GHMidiEditor(GHMidiProcessor&);
    ~GHMidiEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setPanelVisible(bool visible);
    void setHelpVisible(bool visible);
    void updateHudButtons();
    void refreshDeviceBox();
    void refreshMappingRows();
    void updateBindModeVisibility();  // QuickBind diagram vs. row table
    void refreshMidiOutBox();

    GHMidiProcessor& proc;
    HighwayRenderer highway;
    juce::OpenGLContext context;
    QuickBindPanel quickBind;

    bool panelOpen = false, helpOpen = false;

    juce::TextButton gearBtn { "SETTINGS" }, helpBtn { "?" };

    // which control decides how long a note lasts: the fret (default) or the
    // strum bar. Lives on the main view under ? / SETTINGS, not in the panel.
    juce::Label sustainLabel;
    juce::TextButton sustainFretBtn { "FRET" }, sustainStrumBtn { "STRUM" };
    void syncSustainButtons();

    // on-screen arrows for the three settings the controller also changes
    juce::TextButton modePrev, modeNext, strumPrev, strumNext, keyPrev, keyNext, octPrev, octNext;

    // settings panel
    juce::Label deviceLabel, learnHint;
    juce::ComboBox deviceBox;
    juce::TextButton rescanBtn { "RESCAN" }, closeBtn { "X" };
    juce::ToggleButton vmidiToggle { "Virtual MIDI output (record notes in your DAW)" };

    // QuickBind diagram (default) vs. the generic per-row table. The table is
    // scrollable (juce::Viewport) since LTargetCount only grows as more
    // controller types are added (Rock Band added 6; pedals will add more).
    bool quickBindMode = true;
    juce::TextButton diagramBtn { "DIAGRAM" }, tableBtn { "TABLE" };
    juce::Viewport rowTableViewport;
    juce::Component rowTableContent;
    juce::OwnedArray<juce::Label> rowNames, rowDescs;
    juce::OwnedArray<juce::TextButton> rowLearn, rowClear;

    // Windows (or anywhere else with no OS-level virtual MIDI port): pick a
    // real output instead, e.g. a loopMIDI port. Hidden when
    // GuitarService::hasVirtualPort is true (macOS/Linux, normally).
    juce::Label midiOutLabel;
    juce::ComboBox midiOutBox;
    juce::Array<juce::MidiDeviceInfo> shownMidiOuts;

    // help overlay
    juce::TextButton helpCloseBtn { "X" }, moreBtn { "MORE HELP" };

    // GHMIDI_HUDSNAP=<dir> (standalone only): paint the HUD states to PNGs and quit
    juce::String hudSnapDir;
    int hudSnapTick = 0;
    void saveHudSnapshot(const juce::String& name);

    juce::Array<GuitarService::DeviceInfo> shownDevices;
    int lastDevVersion = -1, lastMapVersion = -1, lastLearnTarget = -999;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GHMidiEditor)
};
