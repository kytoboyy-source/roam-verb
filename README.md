# ROAM VERB (Windows VST3)

Place vocals, melodies, drums and samples inside real acoustic environments.
Convolution reverb + an interactive room pad: drag a dot to move the sound
left/right (pan) and near/far (direct-to-reverb ratio, pre-delay gap, air absorption).

- 8 built-in spaces (small room to cathedral, cave, arena)
- **Load IR...** to use any recorded impulse response (WAV/AIFF/FLAC)
- Parameters: Room, Position X, Position Depth, Reverb, Pre-Delay, Width (all automatable)

## Get the VST3 from GitHub
1. Push this folder to a GitHub repo's `main` branch.
2. Open **Actions** > *Build ROAM VERB (Windows)* and wait for the green check.
3. Download the `ROAM-VERB-Windows-VST3` artifact and unzip it.
4. Copy the `ROAM VERB.vst3` folder to `C:\Program Files\Common Files\VST3\`
5. Rescan plugins in your DAW.

Tag a commit `v1.0.0` to also publish a zip on the GitHub Releases page.

## Local build (Visual Studio 2022 + CMake)
```
cmake -B build
cmake --build build --config Release
```
