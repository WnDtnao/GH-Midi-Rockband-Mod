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

## Rock Band guitars

Standard Rock Band guitars (Xplorer/Stratocaster-style, RB1/RB2/RB4) add an
upper-fret row above the usual 5, plus a tilt sensor — LEARN/QuickBind them
like any other control. Holding an upper fret, or tilting the guitar, sends
whatever you strike up an octave, layered on top of what the lower frets
already produce: two octaves of range in CHORDS/NOTES/SOLO without ever
touching the joystick's octave nudge. (RB3's 102-button Pro Guitar neck is
not supported — a different, far more involved protocol.)

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

Open **SETTINGS** and you land on the **DIAGRAM** view: a picture of the
guitar — lower frets, and the Rock Band upper-fret row + tilt pad above
them — with one clickable button per control. Click a button on the
diagram, then press (or sweep, for the whammy/joystick) that control on
your controller — same LEARN engine as before, just click-the-picture
instead of reading down a text list one row at a time. The old row-by-row
**TABLE** view is still there next to it (scrollable — it's grown to 32
rows across the guitar, Rock Band, and pedal controls; handy for unusual
controllers too), and both stay in sync with each other.

**No DAW required.** The release also ships a standalone app: while it runs,
every DAW and synth app on your machine sees a MIDI input called "GH MIDI"
(Logic, GarageBand, Ableton, FL, Reaper, ...). Run either the app or the
plugin, not both — only one process can hold the guitar.

The plugin also publishes a virtual MIDI source ("GH MIDI") so DAWs record
your performance as editable notes. On Windows, which has no OS-level
virtual MIDI port, pick a real MIDI output instead from the new **MIDI out**
dropdown in SETTINGS — see the Windows build notes below.

## Building

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
- **QuickBind**: click-the-picture control mapping (above).
- **Rock Band guitar support**: upper-fret row + tilt sensor (above).
- **Pedal support**: a second HID device and/or a MIDI input fire 7
  performance actions instead of playing notes (above).
- Own product identity ("GH MIDI Rockband Mod", its own VST3 plugin/bundle
  ID) so this can be installed side by side with the original GH MIDI
  without a naming or plugin-ID clash. Settings are also stored separately,
  so the two never overwrite each other's mappings.
- Windows real-MIDI-output fallback (the **MIDI out** picker) since Windows
  has no virtual MIDI ports — the original author's own README already
  flagged this gap; this fork fills it with a loopMIDI-compatible picker
  rather than requiring a bundled driver.

**Not built yet:** a pedal-controlled audio effects chain (distortion/wah-
style tone shaping) — a bigger, more open-ended addition since the plugin
doesn't process audio at all today (it only ever produces MIDI).

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

## Made with Claude Code

The original plugin, README and user guide were made with Claude Code; so
is this fork. Some details may be inaccurate; the code is the reference.

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

## License

AGPL-3.0 (JUCE is used under its AGPLv3 option), same as the original. The
bundled Metal Mania font is by Open Window under the SIL Open Font License —
see `plugin/assets/OFL-MetalMania.txt`. The optional YARG 3D model style
(above) derives from [YARG](https://github.com/YARC-Official/YARG) by
YARC-Official, LGPL-3.0-or-later.
