# GH MIDI — Rockband Mod

Turn a Guitar Hero controller into a real MIDI instrument — on **macOS,
Windows, and Linux**.

> This is an unofficial mod/fork of **[GH MIDI](https://michaelloya.studio/gh-midi)**
> by **Michael Loya** (michaelloya.studio). All credit for the original
> plugin — the HID-to-MIDI engine, the four playing modes, the OpenGL note
> highway — goes to him; full guide at the link above. This mod adds
> cross-platform support and new features on top; see
> [Changes from the original](#changes-from-the-original) below. Same
> license, AGPL-3.0.

A VST3 plugin and standalone app that reads a Guitar Hero controller over USB
HID and plays it like an instrument — inside your DAW, or as a MIDI
controller for anything — with a real-time 3D note highway rendered in
OpenGL.

## Modes

- **CHORDS** — every fret is a chord in the current key (green I, red V,
  yellow vi, blue IV, orange ii). Fret combos add sevenths and the borrowed
  chords the five frets can't make. You cannot play a wrong note.
- **NOTES** — the frets are a binary number (G=1 R=2 Y=4 B=8 O=16) selecting
  fully chromatic notes: 32 per octave position. A real instrument you have
  to learn.
- **SOLO** — each fret is one note of the key's pentatonic scale. Every note
  fits over every chord.
- **CHART** — each fret sends its Clone Hero lane note, all four difficulties
  at once. Record a rough chart in your DAW, export the .mid, finish it in
  Moonscraper.

Whammy = pitch bend (+ CC20 for knob-linking). Minus cycles modes, plus taps
through strum speeds, the joystick changes key (left/right) and octave
(up/down, ±3 octaves in every mode) — and on-screen arrows do all four for
controllers without a joystick.

## Intro

The highway stays dark until a controller is connected and you press
**Plus** (or **Pause**, same button, most Rock Band guitars label it that)
once — a GH3-style camera swoop into the play position, with a rise sound.
Nothing plays before that first press either; everything else (SETTINGS,
LEARN, the row table's live-bit highlighting) works normally while you wait,
so you can map a fresh controller before ever pressing start.
`GHMIDI_DEMO=1` skips this — there's no real button to press when nothing's
plugged in.

## Rock Band guitars

Standard Rock Band guitars (Xplorer/Stratocaster-style, RB1/RB2/RB4) add an
upper-fret row above the usual 5, plus a tilt sensor — LEARN/QuickBind them
like any other control. Holding an upper fret, or tilting the guitar, sends
whatever you strike up an octave, layered on top of what the lower frets
already produce: two octaves of range in CHORDS/NOTES/SOLO without ever
touching the joystick's octave nudge. (RB3's 102-button Pro Guitar neck is
not supported — a different, far more involved protocol.)

Some Rock Band guitars don't give the upper/solo frets their own bits at
all — the same colour bit fires whether you fret low or high on the neck,
and a single shared bit elsewhere in the report is the only thing marking
"this press is on the solo row" (confirmed on a real controller via the
DEBUG byte grid). If LEARNing the 5 upper frets individually doesn't work on
your guitar, LEARN **SOLO MODIFIER** instead (one control, press any
solo-row fret once) — the engine then reinterprets whatever lower frets are
held as upper ones while that bit is down, instead of needing 5 independent
bits that hardware doesn't have.

**HOPO** (hammer-on/pull-off): SETTINGS has a toggle to let a fret change
play through without a fresh strum, whenever a note is already ringing —
same as a real guitar. Off by default.

## Pedals

A footswitch pedal (or a spare gamepad — the mapping is generic byte/bitmask
HID, same engine as the guitar) can fire 7 actions instead of playing notes:
next/previous mode, key up/down, octave up/down, and a sustain toggle. Map
it from **SETTINGS → Pedal (HID)**, then LEARN each action like any other
control (row table only for now — QuickBind's diagram is guitar-shaped and
doesn't have a pedal picture yet).

A MIDI pedal works the same way, independently: pick it from
**SETTINGS → Pedal (MIDI in)**, then LEARN each action by sending the note
or CC you want from the pedal — a note-on, or a CC message with value ≥ 64,
binds it. A VST3 hosted in a DAW gets pedal MIDI through the host's own
routing into the plugin instead of opening a system MIDI device itself (a
plugin shouldn't grab a system device out from under its DAW); the
Standalone app opens the picked device directly.

A pedal-controlled audio effects chain (distortion/wah-style tone shaping)
is a possible future addition, not built yet.

## QuickBind

Open **SETTINGS** and you land on the **DIAGRAM** view: a photo of an RB4
Stratocaster guitar, lit up one control at a time as you map it — click a
control on the diagram, then press (or sweep, for the whammy/joystick) that
control on yours — same LEARN engine as before, just click-the-picture
instead of reading down a text list one row at a time. Every control with a
natural spot on the photo uses it (frets, solo frets, strum, tilt, whammy,
plus/minus); the joystick axes and the solo modifier (above) don't have one,
so those three stay small text pills under the diagram. The old row-by-row
**TABLE** view is still there next to it (scrollable — it's grown to 33 rows
across the guitar, Rock Band, and pedal controls; handy for unusual
controllers too), and both stay in sync with each other.

**No DAW required.** The release also ships a standalone app: while it runs,
every DAW and synth app on your machine sees a MIDI input called "GH MIDI"
(Logic, GarageBand, Ableton, FL, Reaper, ...). Run either the app or the
plugin, not both — only one process can hold the guitar.

The plugin also publishes a virtual MIDI source ("GH MIDI") so DAWs record
your performance as editable notes. On Windows, which has no OS-level
virtual MIDI port, pick a real MIDI output instead from the new **MIDI out**
dropdown in SETTINGS — see the Windows build notes below.

## Sounds

The **Standalone app only** (never the VST3 — a plugin shouldn't add
uninvited audio to its host's mix) plays a handful of Guitar Hero III sound
effects: the intro highway-rise (above), and a click for checkbox/menu
interactions in SETTINGS. A further batch (Star Power cues, a wrong-note
buzz, a song-failed stinger, "You Rock") ships alongside them, embedded but
not wired to anything yet — reserved for if/when the practice-along mode
below grows scoring.

These clips are not under a license that permits redistribution, unlike the
YARG assets above — see [License](#license).

## Building

> **Just want to try it, not use it in a DAW?** Build for your platform below,
> then run the **Standalone** app directly — it's a full, independent
> program (not just a DAW plugin), useful for testing controller mappings
> or just playing. `GHMIDI_DEMO=1` (as an environment variable) runs a
> self-playing demo with no controller needed, to check the build works at
> all first.

### macOS

Requirements: macOS, CMake ≥ 3.24, Xcode command line tools. JUCE 8 and
hidapi are fetched automatically.

```
cd plugin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The VST3 installs to `~/Library/Audio/Plug-Ins/VST3` after a successful
build. A Standalone app is also built; run it with `GHMIDI_DEMO=1` for a
self-playing demo (no controller needed).

### Windows

Requirements: CMake ≥ 3.24 and Visual Studio 2022 (or the standalone
[Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022))
with the "Desktop development with C++" workload. JUCE 8 and hidapi are
fetched automatically.

**MinGW-w64 does not work here** — JUCE 8 has dropped MinGW support outright
and fails to compile (`juce_gui_basics`'s Windows accessibility code needs
UI-Automation declarations MinGW's headers don't have). MSVC is the only
Windows compiler this actually builds with.

```
cd plugin
cmake -B build
cmake --build build --config Release
```

(add `-G Ninja` before `cmake --build` if you'd rather use Ninja than MSBuild
— either works with MSVC, just not with MinGW)

The VST3 and the Standalone `.exe` land under
`build/GHMidi_artefacts/Release/`. Copy the VST3 to your DAW's VST3 folder
(typically `C:\Program Files\Common Files\VST3`) if it isn't copied there
automatically.

Windows has no OS-level virtual MIDI port (the original author's own porting
note, still true), so the Standalone app can't broadcast notes system-wide
the way it does on macOS/Linux out of the box. If you want that, install
[loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html), create a
port, and pick it from the **MIDI out** dropdown in SETTINGS — this doesn't
matter at all for the VST3 inside a DAW, which routes its MIDI output
normally regardless of platform.

### Linux

Requirements: CMake ≥ 3.24, a C++17 compiler, Ninja (or Make), and the
packages JUCE and hidapi need to build against (Debian/Ubuntu names; adjust
for your distro):

```
sudo apt install libasound2-dev libfreetype-dev libfontconfig1-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev \
    mesa-common-dev libudev-dev
```

```
cd plugin
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Then install the udev rule so hidapi can open the controller as a normal
user (without it, `hid_open()` fails silently unless you run as root):

```
sudo cp ../linux/99-ghmidi.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Log out/in (or just replug the controller) afterwards for group membership
to take effect. The VST3 typically goes in `~/.vst3/`.

## Changes from the original

- **Cross-platform**: builds and runs on Windows and Linux, in addition to
  the original's macOS.
- **QuickBind**: click-the-picture control mapping, now a real photo diagram
  (above).
- **Rock Band guitar support**: upper-fret row + tilt sensor, plus a solo
  modifier for guitars that share bits between the two fret rows (above).
- **Pedal support**: a second HID device and/or a MIDI input fire 7
  performance actions instead of playing notes (above).
- **HOPO**: hammer-on/pull-off without a fresh strum, optional (above).
- **GH3-style intro**: the highway stays hidden until you press start
  (above).
- **Sounds**: Standalone-only UI/intro sound effects (above).
- **16:9 window**, and a flatter, more Windows-native look and default
  system font in place of the original's rounded dark-glass panels and
  Metal Mania display face (still bundled, still used for the on-stage "GH"
  wordmark).
- A practice-along mode that scrolls a `.chart`/`.mid` file's notes down the
  existing highway while you play along on your own controller — no
  scoring/Star Power/pass-fail yet, see [Practice mode](#practice-mode).
- Own product identity ("GH MIDI Rockband Mod", its own VST3 plugin/bundle
  ID) so this can be installed side by side with the original GH MIDI
  without a naming or plugin-ID clash. Settings are also stored separately,
  so the two never overwrite each other's mappings.
- Windows real-MIDI-output fallback (the **MIDI out** picker) since Windows
  has no virtual MIDI ports — the original author's own README already
  flagged this gap; this fork fills it with a loopMIDI-compatible picker
  rather than requiring a bundled driver.
- **Fixed**: the upper/solo frets not registering on controllers that share
  bits between the two fret rows (see the solo modifier, above); open notes
  missing a hold texture on the highway; CHART mode not recording open
  strums.

**Not built yet:** a pedal-controlled audio effects chain (distortion/wah-
style tone shaping) — a bigger, more open-ended addition since the plugin
doesn't process the audio bus at all today (it only ever produces MIDI, plus
now some Standalone-only one-shot SFX playback, see [Sounds](#sounds)); the
practice-along mode's scoring/Star Power/audio-sync (see
[Practice mode](#practice-mode)).

Modified under AGPL-3.0 §5(a) ("you must cause the modified files to carry
prominent notices stating that you changed the files"); files substantially
changed for this fork carry a short header notice pointing back here.

## Porting

Windows and Linux support (this fork's main addition) builds on the
portability the original engine (`plugin/src/PluginProcessor.cpp`) already
had going for it — plain JUCE + hidapi, no macOS-only APIs in the actual
HID-reading/MIDI logic. The parts that needed real platform-specific work:
the build (`plugin/CMakeLists.txt`, now guarding macOS-only settings behind
`if(APPLE)`), and the virtual-MIDI-port gap on Windows (see above).
Xbox-360-era XInput-only controllers still aren't supported (would need an
input backend beyond hidapi) — PRs welcome, same as upstream.

`tools/` scripts from the prototyping era (Python HID dump / mapper / MIDI
bridge) are kept for adapter debugging.

## 3D model style

SETTINGS → **3D model style** switches the highway's gem/fret look between
the original hand-built meshes (CLASSIC) and a textured set derived from
[YARG](https://github.com/YARC-Official/YARG) (YARG). The YARG meshes/
textures were converted once, offline, by `tools/model_convert/` (never
built as part of the main plugin, never linked into it) from YARG's own
`Assets/Art/Meshes/Gameplay/{Frets,Notes}` FBX + `Assets/Art/Textures/
Gameplay/{Frets,Notes}` PNG assets; the converted `.obj`/`.png` files are
checked into `plugin/assets/models/` and embedded into the plugin exactly
like the Metal Mania font already is.

## Practice mode

SETTINGS → **LOAD SONG...** opens a `.chart` (Moonscraper/Clone Hero) or
`.mid` file; **PLAY** scrolls its notes down the existing highway from the
top, same lane colours as everything else, so you can play along on your
own controller for practice or just for fun. This is a **first pass, not a
full rhythm-game mode**: no scoring, no Star Power, no pass/fail, no audio
track played alongside it (the notes are the whole show) — Play always
restarts from the top, there's no pause/resume/seek yet. `.chart` is the
primary, better-tested path (parsed directly, tick timing verified against
[TheNathannator's GuitarGame_ChartFormats](https://github.com/TheNathannator/GuitarGame_ChartFormats)
spec); `.mid` support reads whichever 5-fret-guitar difficulty track it
finds, via the same note convention as this plugin's own CHART mode output
(see [Changes from the original](#changes-from-the-original)), so a chart
you record in CHART mode and clean up in Moonscraper can be loaded straight
back in here afterward.

## License

AGPL-3.0 (JUCE is used under its AGPLv3 option), same as the original. The
bundled Metal Mania font is by Open Window under the SIL Open Font License —
see `plugin/assets/OFL-MetalMania.txt`. The optional YARG 3D model style
(above) derives from [YARG](https://github.com/YARC-Official/YARG) by
YARC-Official, LGPL-3.0-or-later.

The [Sounds](#sounds) (`plugin/assets/sfx/`) and the QuickBind diagram
(`plugin/assets/sprites/`) are a different situation: the sound effects are
sourced from the original Guitar Hero III, owned by Activision, under no
license that permits redistribution. They're included in this repository at
the project owner's explicit direction, made with full knowledge that this
is a copyright grey area for a publicly-distributed mod — unlike everything
else listed above, which is either original work or used under a license
that actually allows it. Anyone redistributing this project further should
weigh that themselves rather than assume it's cleared the way the rest of
this file's credits are.
