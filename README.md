# Buttmeister Tuner / Analyzer

Live VST3/standalone analyzer for hardstyle kicks and other pitched percussion.

This repository contains editable Windows x64 plugin source and build scripts. Compiled plugins and the JUCE framework are not included.

Insert it on a soloed kick or percussion track. It passes audio through unchanged, watches the host bar position, and updates a per-bar snapshot:

- punch pitch and note
- tail pitch and note
- pitch drift in semitones
- peak and RMS level
- likely compatible major/minor keys
- recent bar history

The plugin uses low-frequency autocorrelation, so it is aimed at tonal kick bodies, tails, toms, impacts, and bass-heavy percussion rather than full mixes.

## Build

Requirements: Windows x64, Visual Studio 2022 with the Desktop development with C++ workload and Windows SDK, CMake 3.22+ on PATH, and [JUCE 8.0.12](https://github.com/juce-framework/JUCE/tree/8.0.12).

From this repository in PowerShell:

```powershell
git clone --branch 8.0.12 --depth 1 https://github.com/juce-framework/JUCE.git external/JUCE
.\build_with_vs.ps1 -JuceDir "$PWD\external\JUCE"
```

An existing checkout can be supplied with `-JuceDir`. The build script creates the Release VST3 and standalone application without installing them. Outputs are under `build-vs\ButtmeisterTunerAnalyzer_artefacts\Release`.

## Install and use

Copy the complete `Buttmeister Tuner Analyzer.vst3` bundle from the build output's `VST3` folder into a VST3 folder your DAW scans, such as `%CommonProgramFiles%\VST3` or `%LOCALAPPDATA%\Programs\Common\VST3`. Preserve the bundle's `Contents` folder, rescan, and load it as an audio effect.

Use a soloed, tonal percussion signal for useful estimates. Pitch and compatible-key suggestions are signal-analysis estimates; they do not recover an entire song's key. In standalone mode, DAW bar-position information is unavailable.

## Source and validation

The processor and editor retain older internal `HoneyBadgerKickCartographer` class names. The product, bundle identifier, and plugin identifiers are the original Buttmeister Tuner Analyzer values. No audio behavior or saved-state identifiers were changed for this source snapshot. No automated test suite was present in the recovered project.

## License and dependencies

No project-source license has been selected. Public availability does not itself grant a reuse or redistribution license. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the external JUCE dependency and its original notice.
