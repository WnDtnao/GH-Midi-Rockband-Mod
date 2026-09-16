// Rockband Mod (2026) addition -- see README.md for the credit / changes
// note (AGPL-3.0 §5a).
#include "ChartFile.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <vector>

namespace {
// preference order: richest chart actually present wins
const char* kDifficultySections[] = { "[ExpertSingle]", "[HardSingle]", "[MediumSingle]", "[EasySingle]" };
constexpr int kNumDiff = 4;
struct TempoPoint { juce::int64 tick; double bpm; };

// .chart ticks -> seconds: verified against thenathannator.github.io's
// GuitarGame_ChartFormats spec (Format-Overview.md) and cross-checked
// against a from-scratch rhythm-game-engine writeup (specterdev.ca) that
// states the same formula -- msPerTick = 60000 / (bpm * resolution), no
// extra factor. tempo must be sorted by tick and start at tick 0.
double ticksToSeconds(juce::int64 tick, int resolution, const std::vector<TempoPoint>& tempo)
{
    double seconds = 0.0;
    juce::int64 lastTick = 0;
    double bpm = tempo.empty() ? 120.0 : tempo.front().bpm;
    for (auto& tp : tempo)
    {
        if (tp.tick > tick)
            break;
        seconds += (double) (tp.tick - lastTick) * 60.0 / (bpm * (double) resolution);
        lastTick = tp.tick;
        bpm = tp.bpm;
    }
    seconds += (double) (tick - lastTick) * 60.0 / (bpm * (double) resolution);
    return seconds;
}
}

bool ChartFile::loadAuto(const juce::File& file)
{
    notes.clear();
    title = file.getFileNameWithoutExtension();
    lengthSecs = 0.0;
    const auto ext = file.getFileExtension().toLowerCase();
    if (ext == ".mid" || ext == ".midi")
        return loadMidi(file);
    return loadChart(file);   // .chart, or anything else -- try as the text format
}

bool ChartFile::loadChart(const juce::File& file)
{
    const juce::String text = file.loadFileAsString();
    if (text.isEmpty())
        return false;

    juce::StringArray lines;
    lines.addLines(text);

    int resolution = 192;
    std::vector<TempoPoint> tempo;
    tempo.push_back({ 0, 120.0 });   // default, overwritten if the file sets tick 0 itself

    struct RawNote { juce::int64 tick; int lane; juce::int64 lengthTicks; };
    std::vector<RawNote> perDiff[kNumDiff];
    int diffIndex = -1;   // -1 = not inside one of kDifficultySections

    juce::String section;
    for (auto rawLine : lines)
    {
        const auto line = rawLine.trim();
        if (line.isEmpty() || line == "{" || line == "}")
            continue;
        if (line.startsWithChar('['))
        {
            section = line;
            diffIndex = -1;
            for (int i = 0; i < kNumDiff; ++i)
                if (section == kDifficultySections[i]) { diffIndex = i; break; }
            continue;
        }
        if (section == "[Song]")
        {
            if (line.startsWith("Resolution"))
            {
                const int eq = line.indexOfChar('=');
                if (eq >= 0)
                    resolution = juce::jmax(1, line.substring(eq + 1).trim().getIntValue());
            }
            continue;
        }
        const int eq = line.indexOfChar('=');
        if (eq < 0)
            continue;
        const juce::int64 tick = line.substring(0, eq).trim().getLargeIntValue();
        juce::StringArray parts;
        parts.addTokens(line.substring(eq + 1).trim(), " ", "");
        parts.removeEmptyStrings();
        if (parts.isEmpty())
            continue;

        if (section == "[SyncTrack]" && parts[0] == "B" && parts.size() >= 2)
            tempo.push_back({ tick, (double) parts[1].getLargeIntValue() / 1000.0 });
        else if (diffIndex >= 0 && parts[0] == "N" && parts.size() >= 3)
            perDiff[diffIndex].push_back({ tick, parts[1].getIntValue(), parts[2].getLargeIntValue() });
    }

    int chosen = -1;
    for (int i = 0; i < kNumDiff; ++i)
        if (! perDiff[i].empty()) { chosen = i; break; }
    if (chosen < 0)
        return true;   // parsed fine, this file just has no 5-fret guitar track

    // stable: the default tick-0 entry was pushed before parsing, so a real
    // tick-0 B line from the file (pushed later, during parsing) sorts
    // after it and correctly wins ticksToSeconds' "last one at this tick"
    // resolution instead of an unstable sort leaving that unspecified
    std::stable_sort(tempo.begin(), tempo.end(), [](const TempoPoint& a, const TempoPoint& b) { return a.tick < b.tick; });
    std::sort(perDiff[chosen].begin(), perDiff[chosen].end(),
              [](const RawNote& a, const RawNote& b) { return a.tick < b.tick; });

    for (auto& n : perDiff[chosen])
    {
        // 0-4 = green..orange, 7 = open; 5/6 are the forced-HOPO/tap flags
        // on the PRECEDING note, not notes of their own -- MVP scope has no
        // HOPO/tap distinction to apply them to, so they're just skipped
        const int lane = n.lane == 7 ? 5 : n.lane;
        if (lane < 0 || lane > 5)
            continue;
        ChartNote cn;
        cn.timeSecs = ticksToSeconds(n.tick, resolution, tempo);
        cn.lengthSecs = n.lengthTicks > 0
                       ? ticksToSeconds(n.tick + n.lengthTicks, resolution, tempo) - cn.timeSecs : 0.0;
        cn.lane = lane;
        notes.add(cn);
        lengthSecs = juce::jmax(lengthSecs, cn.timeSecs + cn.lengthSecs);
    }
    return true;
}

bool ChartFile::loadMidi(const juce::File& file)
{
    juce::FileInputStream stream(file);
    if (! stream.openedOk())
        return false;
    juce::MidiFile midi;
    if (! midi.readFrom(stream))
        return false;
    midi.convertTimestampTicksToSeconds();

    // verified against thenathannator.github.io's GuitarGame_ChartFormats
    // spec: green..orange = base+0..base+4 per difficulty, open = base-1
    // (the note-based convention -- needs Enhanced Opens in Moonscraper,
    // same as this plugin's own CHART mode, see README)
    constexpr int kBases[4] = { 96, 84, 72, 60};   // Expert, Hard, Medium, Easy
    int chosenBase = -1;
    for (int t = 0; t < midi.getNumTracks(); ++t)
    {
        const auto* track = midi.getTrack(t);
        for (int base : kBases)
        {
            bool any = false;
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                const auto& mm = track->getEventPointer(i)->message;
                if (mm.isNoteOn() && mm.getNoteNumber() >= base - 1 && mm.getNoteNumber() <= base + 4)
                { any = true; break; }
            }
            if (any) { chosenBase = base; break; }
        }
        if (chosenBase < 0)
            continue;

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& mm = track->getEventPointer(i)->message;
            if (! mm.isNoteOn())
                continue;
            const int nn = mm.getNoteNumber();
            const int rel = nn - chosenBase;
            if (rel < -1 || rel > 4)
                continue;
            ChartNote cn;
            cn.timeSecs = mm.getTimeStamp();
            cn.lane = rel < 0 ? 5 : rel;
            // find the matching note-off for length; a fixed short default
            // if none is found rather than scanning unboundedly
            cn.lengthSecs = 0.0;
            for (int j = i + 1; j < track->getNumEvents(); ++j)
            {
                const auto& off = track->getEventPointer(j)->message;
                if (off.isNoteOff() && off.getNoteNumber() == nn)
                {
                    cn.lengthSecs = juce::jmax(0.0, off.getTimeStamp() - cn.timeSecs);
                    break;
                }
            }
            notes.add(cn);
            lengthSecs = juce::jmax(lengthSecs, cn.timeSecs + cn.lengthSecs);
        }
        break;   // one track's worth is enough for the MVP
    }
    return true;
}
