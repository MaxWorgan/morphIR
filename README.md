# MorphIR

A real-time morphing convolution reverb plugin (VST3, AU, CLAP) for macOS.

Load two impulse responses into slots. Sweep the **Morph Position** knob — or automate it from your DAW — to continuously transition between the acoustic characters of the two IRs. The transition happens via frequency-domain magnitude interpolation, computed on a dedicated background thread and lock-free-swapped into a single non-uniform partitioned convolution (NUPC) engine. There are no parallel reverb engines and no audible crossfade artefact: the reverb tail reflects where you've been; new input responds to where you are.

## Features

- VST3, AU, and CLAP plugin formats
- Stereo (dual mono) processing
- IRs up to 30 seconds at the session sample rate
- WAV / AIFF loading via drag-and-drop or file browser
- Near-zero latency (NUPC small early partitions)
- Lock-free morph thread — audio thread never blocks
- Sample-accurate parameter automation under CLAP

## Parameters

| Parameter        | Range         | Default |
|------------------|---------------|---------|
| Morph Position   | 0.0 – 1.0     | 0.0     |
| Dry/Wet          | 0.0 – 1.0     | 0.5     |
| Output Gain      | -24 dB – +12 dB | 0 dB  |
| Pre-delay        | 0 ms – 500 ms | 0 ms    |

## Building from source

Requires CMake 3.21+ and a C++20 compiler. macOS only for now.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target MorphIR_All
cmake --build build --target MorphIR_CLAP
```

Build artefacts land in `build/MorphIR_artefacts/Release/{AU,VST3,CLAP,Standalone}/`.

## Running tests

```bash
cmake --build build --target MorphIR_Tests
./build/MorphIR_Tests
```

## Installing locally

Copy or symlink the built bundles to the standard plugin folders:

```bash
ln -sf "$PWD/build/MorphIR_artefacts/Debug/AU/MorphIR.component"   ~/Library/Audio/Plug-Ins/Components/
ln -sf "$PWD/build/MorphIR_artefacts/Debug/VST3/MorphIR.vst3"      ~/Library/Audio/Plug-Ins/VST3/
ln -sf "$PWD/build/MorphIR_artefacts/Debug/CLAP/MorphIR.clap"      ~/Library/Audio/Plug-Ins/CLAP/
```

After symlinking, rescan plugins in your DAW.

## Architecture

See [`docs/PRD.md`](docs/PRD.md) for the full design and [`docs/adr/`](docs/adr/) for individual architecture decisions:

- **ADR 0001**: Non-uniform partitioned convolution for IR processing
- **ADR 0002**: Double-buffering with atomic pointer swap for IR hot-swap
- **ADR 0003**: vDSP on macOS, FFTW on Linux, behind FFTProvider abstraction

Domain vocabulary is defined in [`CONTEXT.md`](CONTEXT.md).

## License

This project is open source under the GPL — to be confirmed when the LICENSE file is added.
