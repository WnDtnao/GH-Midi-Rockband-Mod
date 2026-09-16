// Rockband Mod (2026) -- modified from the original GH MIDI by Michael Loya
// (michaelloya.studio), AGPL-3.0. See README.md for the full credit / changes
// note (AGPL-3.0 §5a). This file adds: a real-MIDI-output fallback for
// platforms with no virtual MIDI port (Windows), Rock Band upper-fret/tilt
// support, and pedal-as-controller support (a second HID device polled
// alongside the guitar, and/or an incoming MIDI note/CC) that fires
// performance actions (mode/key/octave/sustain) instead of playing notes.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <hidapi.h>
#include <cstring>

// One process-wide guitar reader shared by every GH MIDI instance.
//
// hidapi/macOS rule (learned the hard way): ALL hidapi calls live on this
// service's single thread, and that thread does its own teardown before dying.
// The pedal's HID device is polled on this SAME thread for the same reason
// (see pollPedal(), called from run()); it is not given its own thread.
//
// v1.0: controls are read through a ControllerMap (learned via the in-plugin
// calibration wizard and persisted to Application Support), so any HID
// controller can drive the instrument — not just the raphnet WUSBMote.
class GuitarService : private juce::Thread, private juce::MidiInputCallback
{
public:
    enum Mode { Easy = 0, Real = 1, Penta = 2, Chart = 3 };
    static constexpr int kOctaveMin = -36, kOctaveMax = 36;   // semitones, ±3 octaves

    struct ButtonMap
    {
        int byteIdx = -1;
        uint8_t mask = 0;
        bool valid() const { return byteIdx >= 0 && mask != 0; }
    };
    struct AxisMap        // whammy: rest position -> fully pressed
    {
        int byteIdx = -1;
        int rest = 0, extreme = 255;
        bool valid() const { return byteIdx >= 0 && rest != extreme; }
    };
    struct StickMap       // joystick axis with centre + travel
    {
        int byteIdx = -1;
        int center = 128, lo = 0, hi = 255;
        bool valid() const { return byteIdx >= 0 && hi > lo; }
    };
    struct ControllerMap
    {
        ButtonMap frets[5], strumDown, strumUp, plusBtn, minusBtn;
        AxisMap whammy;
        StickMap stickX, stickY;
        // Rock Band standard guitars: an upper-fret row (green..orange, same
        // order as frets[]) above the usual 5, plus a tilt sensor. Both stay
        // unmapped (invalid()) for GH guitars.
        ButtonMap upperFrets[5];
        ButtonMap tilt;
    };

    // Pedal-as-controller: a HID footswitch board fires these 7 actions by
    // button, in place of playing notes. A separate MIDI input can fire the
    // same 7 actions by note-on or CC (see MidiPedalMap) -- independent of
    // whether a HID pedal is also mapped.
    struct PedalMap
    {
        ButtonMap modeFwd, modeBack, keyUp, keyDown, octUp, octDown, sustain;
    };
    struct MidiPedalMap
    {
        // -1 = unmapped. 0..127 = that note number (any channel, any note-on);
        // 1000+cc = that CC number (any channel, value >= 64), same order as
        // PedalMap's 7 actions.
        int trig[7] = { -1, -1, -1, -1, -1, -1, -1 };
    };

    struct DeviceInfo
    {
        juce::String label;
        int vid = 0, pid = 0;
    };

    GuitarService();
    ~GuitarService() override;

    void addClient(juce::MidiMessageCollector* c);
    void removeClient(juce::MidiMessageCollector* c);

    // ---- MIDI output routing ----
    // macOS/Linux create a virtual "GH MIDI" port automatically (CoreMIDI and
    // the ALSA sequencer both support it). Windows has no OS-level virtual
    // MIDI, so hasVirtualPort reads false there and the UI should offer a
    // real MIDI output device to send to instead -- typically a loopMIDI
    // port the user created themselves. Runtime check, not #ifdef, so it
    // also covers CoreMIDI/ALSA failing to create a port for any reason.
    std::atomic<bool> hasVirtualPort { false };
    juce::Array<juce::MidiDeviceInfo> getMidiOutputs() const { return juce::MidiOutput::getAvailableDevices(); }
    juce::String currentMidiOutId() const { const juce::ScopedLock sl(midiOutLock); return midiOutId; }
    void selectMidiOutput(const juce::String& identifier)
    {
        { const juce::ScopedLock sl(midiOutLock); midiOutId = identifier; }
        midiOutRequest = true;
        saveRequest = true;
        notify();
    }

    // ---- live state for editors ----
    std::atomic<int> uiFretBits { 0 };
    std::atomic<int> uiMode { Easy };
    std::atomic<int> uiKey { 0 };
    std::atomic<int> uiOctave { 0 };     // NOTES / SOLO octave offset (semitones)
    std::atomic<int> uiEasyOct { 0 };    // CHORDS octave offset (semitones)
    std::atomic<float> uiWhammy { 0.0f };
    std::atomic<bool> guitarFound { false };
    std::atomic<double> lastPlayedAt { -1.0e9 };  // when the HUD label last changed
    std::atomic<int> lastPlayedFrets { 0 };       // CHART pops: fret mask, drawn as coloured letters (0 = plain text)
    void announceFrets(int mask);                 // CHART: pop the held frets as letters
    std::atomic<bool> adapterEmpty { false };
    std::atomic<bool> hammerOn { false };   // legacy blob compat; no longer user-facing
    std::atomic<int> uiButtonBits { 0 };    // live control test: bit per LearnTarget

    // ---- debug panel (Rockband Mod addition) ----
    std::atomic<bool> debugWireframe { false };   // HighwayRenderer reads this directly
    std::atomic<float> uiBpm { 0.0f };            // from strum downstroke timing; 0 = not enough data yet
    struct DebugSnapshot
    {
        uint8_t guitar[64] {}; int guitarLen = 0;
        uint8_t pedal[64] {};  int pedalLen = 0;
    };
    DebugSnapshot getDebugSnapshot() const
    {
        const juce::ScopedLock sl(debugLock);
        DebugSnapshot s;
        std::memcpy(s.guitar, debugGuitarBuf, sizeof(s.guitar)); s.guitarLen = debugGuitarLen;
        std::memcpy(s.pedal, debugPedalBuf, sizeof(s.pedal));   s.pedalLen = debugPedalLen;
        return s;
    }

    // ---- settings (all persisted) ----
    std::atomic<int> whammyMode { 0 };      // 0 = bend + CC20, 1 = bend, 2 = CC20
    std::atomic<bool> virtualMidiOn { true };
    std::atomic<bool> strumSustain { false }; // on: a note lasts while the strum BAR is held, not the fret
    std::atomic<int> strumRollMs { 10 };   // ms between rolled chord notes (0 = off)  // "GH MIDI" virtual source for DAW note recording
    // 3D highway gem/fret look: the original hand-built meshes, or the
    // YARG-derived textured ones (HighwayRenderer falls back to Classic if
    // the YARG assets fail to load, so this is never a hard requirement)
    enum ModelStyle { ModelClassic = 0, ModelYarg = 1 };
    std::atomic<int> modelStyle { ModelClassic };

    // ---- state-restore requests (picked up by the guitar thread) ----
    std::atomic<int> modeRequest { -1 };
    std::atomic<int> keyRequest { -1 };

    // ---- on-screen arrows: the UI adds a delta, the guitar thread applies it ----
    void nudgeMode(int d)   { modeNudge += d;   notify(); }
    void nudgeKey(int d)    { keyNudge += d;    notify(); }
    void nudgeOctave(int d) { octaveNudge += d; notify(); }
    void nudgeStrum(int d)  { strumNudge += d;  notify(); }   // ±5 ms per step
    static constexpr int kStrumStepMs = 5, kStrumMaxMs = 50;

    // ---- devices & calibration (UI thread calls; work runs on the guitar thread) ----
    void requestDeviceScan() { scanRequest = true; notify(); }
    juce::Array<DeviceInfo> getDevices() const
    {
        const juce::ScopedLock sl(deviceLock);
        return devices;
    }
    int getDeviceListVersion() const { return deviceListVersion.load(); }
    void selectDevice(int vid, int pid)
    {
        targetVid = vid;
        targetPid = pid;
        reconnectRequest = true;
        saveRequest = true;
        notify();
    }
    int currentVid() const { return targetVid.load(); }
    int currentPid() const { return targetPid.load(); }

    // ---- pedal: a second, optional HID device, sharing the same device
    // list as the guitar (see getDevices()/requestDeviceScan() above) ----
    void selectPedalDevice(int vid, int pid)
    {
        targetPedalVid = vid;
        targetPedalPid = pid;
        pedalReconnectRequest = true;
        saveRequest = true;
        notify();
    }
    int currentPedalVid() const { return targetPedalVid.load(); }
    int currentPedalPid() const { return targetPedalPid.load(); }
    std::atomic<bool> pedalFound { false };

    // ---- pedal: an optional MIDI input, independent of the HID pedal ----
    juce::Array<juce::MidiDeviceInfo> getMidiInputs() const { return juce::MidiInput::getAvailableDevices(); }
    juce::String currentMidiInId() const { const juce::ScopedLock sl(midiInLock); return midiInId; }
    void selectMidiInput(const juce::String& identifier)
    {
        { const juce::ScopedLock sl(midiInLock); midiInId = identifier; }
        midiInRequest = true;
        saveRequest = true;
        notify();
    }
    // called from processBlock() (VST3: MIDI arrives via the host's routing,
    // not a system MIDI input device) -- safe from the audio thread, same as
    // handleIncomingMidiMessage() below is safe from JUCE's MIDI thread: both
    // only ever touch mapLock (briefly) and the existing cross-thread-safe
    // nudge*/request* methods, never guitar-thread-only state directly.
    void handleMidiPedalMessage(const juce::MidiMessage& message);
    void requestSustainToggle() { sustainToggleRequest = true; notify(); }

    // per-control mapping: each row learned or cleared independently
    enum LearnTarget { LFretG = 0, LFretR, LFretY, LFretB, LFretO,
                       LStrumDown, LStrumUp, LPlus, LMinus, LWhammy, LStickX, LStickY,
                       // Rock Band standard guitars only -- see ControllerMap
                       LFretUpG, LFretUpR, LFretUpY, LFretUpB, LFretUpO, LTilt,
                       // pedal-as-controller: HID footswitch (see PedalMap)...
                       LPedalModeFwd, LPedalModeBack, LPedalKeyUp, LPedalKeyDown,
                       LPedalOctUp, LPedalOctDown, LPedalSustain,
                       // ...and/or MIDI note/CC (see MidiPedalMap), same 7 actions
                       LMidiPedalModeFwd, LMidiPedalModeBack, LMidiPedalKeyUp, LMidiPedalKeyDown,
                       LMidiPedalOctUp, LMidiPedalOctDown, LMidiPedalSustain,
                       LTargetCount };
    void startLearn(int target) { learnTarget = juce::jlimit(0, LTargetCount - 1, target); learnPhase = 0; learnT0req = true; notify(); }
    void cancelLearn() { learnTarget = -1; }
    int getLearnTarget() const { return learnTarget.load(); }
    int getMapVersion() const { return mapVersion.load(); }
    void clearMapping(int target);
    juce::String describeMapping(int target) const;
    static juce::String targetName(int target);
    // true for the 3 targets learned by sweeping an axis (whammy, joystick
    // X/Y) rather than pressing a button -- everything else is a button,
    // including every target added after these three (Rock Band, pedals).
    // A range check on LWhammy alone (as this used to be, before Rock Band
    // and pedal targets were added after LStickY) silently mis-scoped once
    // those were added; centralised here instead of inlined per-UI-file so
    // it can't drift out of sync like that again.
    static bool isAxisTarget(int target) { return target == LWhammy || target == LStickX || target == LStickY; }

    void requestSave() { saveRequest = true; notify(); }

    juce::String getLastPlayed() const
    {
        const juce::ScopedLock sl(labelLock);
        return lastPlayed;
    }

    struct Gem
    {
        int mask = 0;
        double t0 = 0.0;
        double t1 = -1.0;
        bool legato = false;
    };
    juce::Array<Gem> getRecentGems() const
    {
        const juce::ScopedLock sl(gemLock);
        return gems;
    }

private:
    void run() override;
    void demoRun();
    void step(const uint8_t* d, int len);
    void learnTick(const uint8_t* d, int len, double now);
    void scanDevices();
    void loadSettings();
    void saveSettings();
    void applyMapForDevice();
    void applyMidiOutput();
    juce::String deviceKey() const;
    static juce::File settingsFile();

    // pedal (guitar thread only, except handleIncomingMidiMessage/
    // handleMidiPedalMessage -- see the comment on those above)
    void pollPedal();               // called every run() tick, independent of guitar state
    void pedalStep(const uint8_t* d, int len);
    void pedalLearnTick(const uint8_t* d, int len, double now);
    void applyMidiInput();
    void fireAction(int actionIdx);   // 0..6, same order as PedalMap/MidiPedalMap
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;

    bool demoMode = false;
    void sendMsg(const juce::MidiMessage& m);
    void noteOn(int note, int vel);
    void allOff();
    void announce(const juce::String& s);

    juce::CriticalSection clientLock;
    juce::Array<juce::MidiMessageCollector*> clients;
    std::unique_ptr<juce::MidiOutput> virtualOut;  // created/destroyed on the guitar thread
    std::unique_ptr<juce::MidiOutput> realMidiOut;  // Windows fallback: a real, user-picked output (e.g. loopMIDI); guitar thread only
    mutable juce::CriticalSection midiOutLock;
    juce::String midiOutId;
    std::atomic<bool> midiOutRequest { false };

    hid_device* dev = nullptr;
    int ioMode = 0;        // 1 = get_input_report id 1 · 2 = id 0 · 3 = hid_read stream
    uint8_t lastBuf[64] {};
    int lastLen = 0;

    // ---- controller mapping (guarded by mapLock; step copies it per tick) ----
    mutable juce::CriticalSection mapLock;
    ControllerMap map;
    juce::var controllersVar;   // per-device saved mappings (guitar thread only)
    std::atomic<int> mapVersion { 0 };
    std::atomic<int> targetVid { 0x289B }, targetPid { 0x0080 };

    // ---- pedal: HID (guarded by mapLock like map above) ----
    PedalMap pedalMap;
    std::atomic<int> targetPedalVid { 0 }, targetPedalPid { 0 };   // 0 = no pedal selected
    std::atomic<bool> pedalReconnectRequest { false };
    hid_device* pedalDev = nullptr;
    bool pedalPrevHeld[7] { false, false, false, false, false, false, false };

    // ---- pedal: MIDI (guarded by mapLock like map above) ----
    MidiPedalMap midiPedalMap;
    std::unique_ptr<juce::MidiInput> midiIn;   // created/destroyed on the guitar thread
    mutable juce::CriticalSection midiInLock;
    juce::String midiInId;
    std::atomic<bool> midiInRequest { false };
    std::atomic<bool> sustainToggleRequest { false };

    // device list (guitar thread writes, UI reads)
    mutable juce::CriticalSection deviceLock;
    juce::Array<DeviceInfo> devices;
    std::atomic<int> deviceListVersion { 0 };
    std::atomic<bool> scanRequest { false }, reconnectRequest { false }, saveRequest { false };

    // per-row learn engine
    std::atomic<int> learnTarget { -1 };
    std::atomic<bool> learnT0req { false };
    int learnPhase = 0, learnByte = -1;
    uint8_t learnMask = 0;
    uint8_t learnBase[64] {};
    int learnBaseLen = 0;
    double learnT0 = 0, learnChangeAt = 0, axStart = 0;
    int axMin[64] {}, axMax[64] {};

    // engine state (guitar thread only)
    std::atomic<int> modeNudge { 0 }, keyNudge { 0 }, octaveNudge { 0 }, strumNudge { 0 };
    void cycleMode(int dir);
    void shiftKey(int d);
    void shiftOctave(int d);
    void setStrum(int ms);
    int mode = Easy;
    int key = 0, octaveReal = 0, easyOct = 0;
    bool prevDown = false, prevUp = false, prevPlus = false, prevMinus = false;
    double strumLatchAt = -1.0;
    int latchVel = 100;
    bool latchDown = true;
    juce::SortedSet<int> ringing;
    int ringFret = -1, ringCombo = -1;
    int soloNotes[5] { -1, -1, -1, -1, -1 };  // SOLO: ringing note per fret
    int chartHeld = 0;                         // CHART: frets whose lane notes are sounding
    int candCombo = -1;
    double candSince = 0.0;
    double sustainOnAt = -1.0;       // strum-sustain: when the ringing notes started
    bool sustainOffPending = false;  // strum-sustain: bar released before the minimum, off owed
    double lastLegatoAt = -1.0;
    int joyPos = 0, joyPosY = 0;
    float prevWham = 0.0f;
    double whamBusyUntil = -1.0;
    int bend = 0;

    void beginGem(int mask, bool legato);
    void endGem();

    // ---- debug panel (guitar thread writes, UI thread reads via getDebugSnapshot()) ----
    mutable juce::CriticalSection debugLock;
    uint8_t debugGuitarBuf[64] {}; int debugGuitarLen = 0;
    uint8_t debugPedalBuf[64] {};  int debugPedalLen = 0;
    double strumBeatTimes[8] {};   // ring buffer of recent downstroke timestamps, newest first
    int strumBeatCount = 0;
    void trackBeat(double now);    // called on each downstroke edge; updates uiBpm

    mutable juce::CriticalSection labelLock;
    juce::String lastPlayed;

    mutable juce::CriticalSection gemLock;
    juce::Array<Gem> gems;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarService)
};

class GHMidiProcessor : public juce::AudioProcessor
{
public:
    GHMidiProcessor();
    ~GHMidiProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override { service->removeClient(&collector); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GH MIDI"; }
    // true: reserves a MIDI input bus for the pedal-as-MIDI-controller path
    // (Rockband Mod) -- processBlock() feeds it to
    // GuitarService::handleMidiPedalMessage(). On Standalone, a real MIDI
    // input device works too (GuitarService::selectMidiInput()); a VST3
    // hosted in a DAW gets pedal MIDI through this bus instead.
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    GuitarService& guitar() { return *service; }

private:
    juce::SharedResourcePointer<GuitarService> service;
    juce::MidiMessageCollector collector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GHMidiProcessor)
};
