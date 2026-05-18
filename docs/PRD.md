# PRD: MorphIR — Real-Time Morphing Convolution Reverb Plugin

## Problem Statement

Musicians and sound designers working with convolution reverb are locked into static spaces. Once an IR is loaded, the acoustic character of the reverb is fixed for the duration of a performance or mix. Crossfading between two reverb engines is the industry workaround, but it doubles CPU cost and produces a blend of two simultaneous spaces rather than a genuine transition between them. There is no plugin that lets a performer continuously morph the acoustic character of a convolution reverb — the shape of the space itself — as an expressive real-time parameter.

## Solution

MorphIR is a convolution reverb audio plugin (VST3, AU, CLAP) that supports real-time spectral morphing between loaded IRs. The user loads IRs into named slots, then sweeps a single Morph Position parameter to continuously transition between the acoustic characters of those slots. A dedicated morphing thread continuously computes a blended IR from the loaded slots and feeds it into a single, continuously-running convolution engine — there are no parallel engines, no doubled CPU cost, and no "two rooms at once" artefact. The reverb tail reflects the accumulated history of past morph positions; new input immediately responds to the current one.

## User Stories

1. As a musician, I want to load two IRs into a plugin and morph between their acoustic characters in real time, so that I can transition between spaces expressively during a performance.
2. As a sound designer, I want to morph between two unrelated audio files used as IRs, so that I can create timbral transformations that have no direct spatial analogy.
3. As a mixing engineer, I want to automate the Morph Position parameter in my DAW, so that the reverb character evolves over the course of a track.
4. As a user, I want the morph transition to be smooth and continuous, so that I don't hear discrete jumps or crossfade artefacts when sweeping the Morph Position.
5. As a user, I want the reverb tail to reflect where I've been while new input responds to where I am, so that the plugin behaves like a physical space being reshaped in real time.
6. As a user, I want to load IRs by dragging and dropping audio files onto IR slots, so that the workflow integrates naturally with my existing sample library browsing.
7. As a user, I want to load IRs via a file browser, so that I can navigate my library when drag-and-drop isn't convenient.
8. As a user, I want the plugin to support WAV and AIFF IR files, so that I can use my existing IR library without conversion.
9. As a user, I want the plugin to support stereo IR files, so that the reverb produces a natural stereo width.
10. As a user, I want to control Dry/Wet mix, so that I can blend the reverb with the dry signal.
11. As a user, I want to control Output Gain, so that I can compensate for level changes introduced by the reverb.
12. As a user, I want to control Pre-delay, so that I can push the reverb onset back to create separation between the dry signal and the reverb tail.
13. As a user, I want the Morph Position to be automatable at full DAW resolution, so that fast automated sweeps sound smooth.
14. As a producer, I want the plugin to run on macOS in Logic, Ableton, Reaper, Bitwig, and Cubase, so that I'm not locked into a single DAW.
15. As a performer, I want the plugin to have near-zero latency for early reflections, so that it doesn't affect the feel of played notes.
16. As a user, I want to load IRs up to 30 seconds in length, so that I can use large-hall and cathedral captures.
17. As a user, I want the plugin to handle IRs of different lengths in the two slots, so that I can morph between a short room and a long hall without manual trimming.
18. As a developer, I want the plugin to be open source under GPL, so that the community can extend and contribute to it.
19. As a user, I want the plugin to perform efficiently on Apple Silicon, so that it uses minimal CPU even with long IRs.
20. As a user, I want swapping an IR in a slot to not cause audio glitches, so that I can change IRs mid-session safely.
21. As a user, I want the Morph Position to respond smoothly even when automated at high rates, so that LFO-driven morphing sounds fluid.
22. As a future user, I want to load more than two IRs into slots and navigate between them, so that I can create more complex morphing journeys.

## Implementation Decisions

### Module Breakdown

**FFTProvider**
A thin abstraction over the platform FFT library. Exposes `forward(input, output, size)` and `inverse(input, output, size)` operations. macOS implementation uses Apple vDSP (Accelerate framework). Linux implementation uses FFTW. All other modules depend on FFTProvider, never on a specific FFT library directly.

**IRSlot**
Holds a single loaded IR for one channel (mono buffer). Stores both the time-domain samples (zero-padded to max IR length) and the pre-computed frequency-domain representation at max FFT size. FFTs are computed once at load time by the IRLoader, off the audio thread. The audio thread never writes to an IRSlot.

**IRLoader**
Off-audio-thread component. Accepts a file path, decodes WAV/AIFF via JUCE's AudioFormatManager, resamples to the session sample rate if needed, splits into L and R channels, zero-pads to max IR length, computes FFTs via FFTProvider, and populates a pair of IRSlots. Reports load completion and errors asynchronously. The audio thread never calls IRLoader.

**SpectralMorpher**
A pure, stateless function. Takes two frequency-domain IRSlots (L or R channel), a morph position (0.0–1.0), and a target length determined by linearly interpolating the two slot lengths. Produces a time-domain blended IR buffer: interpolates FFT magnitudes element-wise, retains phase from the dominant slot (the slot with higher weight), applies IFFT, zeros the region beyond the blended length. Has no threading concerns — entirely deterministic and testable in isolation.

**MorphThread**
A dedicated background thread running at approximately 30–60Hz. Reads the current Morph Position atomically. Calls SpectralMorpher for each channel (L and R). Writes the resulting blended IR into the back buffer of the NUPCEngine. Sets an atomic swap-pending flag when the back buffer is ready. Does not touch the audio thread.

**NUPCEngine**
Non-Uniform Partitioned Convolution engine for a single channel. Implements the NUPC algorithm with small early partitions (low latency) and large tail partitions (CPU efficiency). Maintains two complete working IR buffers (front and back). The audio thread always reads from the front buffer. At each block boundary, the audio thread checks an atomic swap-pending flag; if set, it swaps front and back pointers in a single atomic operation and clears the flag. The audio thread never blocks.

**DualMonoConvolver**
Owns two NUPCEngine instances (L and R) and one MorphThread. Coordinates lockstep processing: both engines receive the same morph updates, share slot FFT data, and process audio blocks together. Exposes a simple `process(stereoBlock)` interface to the plugin processor.

**MorphIRProcessor** (JUCE AudioProcessor)
The JUCE plugin processor. Owns the DualMonoConvolver, IRLoader, and IR slot state. Manages the parameter tree (Morph Position, Dry/Wet, Output Gain, Pre-delay). Bridges DAW parameter automation to the atomic morph position read by MorphThread. Handles plugin state save/restore (stores file paths, not IR data).

**MorphIREditor** (JUCE AudioProcessorEditor)
The plugin UI. Two IR slot panels, each with a drag-drop target and a file browser button. A morph knob/slider. Controls for Dry/Wet, Output Gain, Pre-delay. Slot panels display filename, IR length, and a waveform thumbnail. No audio processing logic.

### Key Architecture Decisions

- **NUPC** is used for all convolution (see ADR 0001). Partition schedule is fixed at initialisation based on max IR length; shorter blended IRs zero out tail partitions rather than restructuring the engine.
- **Double buffering with atomic pointer swap** for IR hot-swap (see ADR 0002). The audio thread never blocks, never waits for morphing to complete.
- **vDSP on macOS / FFTW on Linux** behind FFTProvider abstraction (see ADR 0003).
- **Blended IR length** is linearly interpolated: `lerp(slotA.length, slotB.length, morphPosition)`. All working buffers are pre-allocated at maximum IR size (30 seconds at session sample rate).
- **FFTs are pre-computed at load time** and stored per slot at max FFT size. SpectralMorpher never performs a forward FFT — only element-wise interpolation and one IFFT per update.
- **Dual Mono** stereo configuration for v1: two NUPCEngine instances sharing slot FFTs but maintaining independent L/R IR buffers and working buffers.
- **Plugin formats**: VST3, AU, CLAP via JUCE. CLAP is included because its per-sample parameter automation model is superior to VST3 for high-rate Morph Position sweeps.
- **IR slot count**: 2 for v1. SpectralMorpher, IRSlot, and MorphThread are designed to accept N slots with a single linear-chain morph position, to allow expansion without architectural change.

### Threading Model

```
[Audio Thread]                    [Morph Thread ~30-60Hz]         [Main/UI Thread]
NUPCEngine.process(block)         SpectralMorpher.compute()        IRLoader.load(path)
  check swap_pending (atomic)       read morphPosition (atomic)      write IRSlot L/R
  if set: swap front/back           write back buffer                compute slot FFTs
  convolve with front buffer        set swap_pending (atomic)
```

No mutex is held by the audio thread at any point.

## Testing Decisions

Good tests for this plugin test observable behaviour, not internal state. They do not assert on partition schedules, buffer indices, or thread timing — those are implementation details that will change. They assert on what comes out given what went in.

**SpectralMorpher** — highest priority for unit testing. It is a pure function: given two slot FFTs and a morph position, it produces a deterministic output IR. Tests can verify that at position 0.0 the output matches slot A, at 1.0 it matches slot B, and at 0.5 the output spectral magnitude is midway between the two. No threading, no audio thread required.

**IRLoader** — unit tests verify that a known WAV file produces an IRSlot with the correct sample count, correct channel split, correct zero-padding, and correct FFT (round-trip: IFFT the stored FFT and compare to the original samples within tolerance).

**NUPCEngine** — integration tests feed a known input signal (e.g. a Dirac delta) and a known IR, then verify the output matches the expected convolution result within a small tolerance. A second test verifies that swapping the IR mid-stream (triggering the atomic swap) produces no sample-accurate glitch in the output (RMS of the difference from a reference output stays below threshold).

**DualMonoConvolver** — integration test: verify that L and R outputs are processed independently (feed different signals to L and R, verify outputs don't bleed), and that a morph position change is reflected in both channels within one morph thread period.

**MorphIRProcessor** — plugin-level integration tests using JUCE's test harness: load two IRs, automate Morph Position from 0 to 1 over 1 second, verify no output discontinuities (no sample exceeds a clipping threshold, RMS doesn't drop to zero unexpectedly).

No tests for MorphIREditor — UI components are exercised manually.

## Out of Scope

- **True stereo (4-channel LL/LR/RL/RR) IR support** — dual mono only in v1.
- **Envelope-aware time-domain IR warping** — spectral magnitude interpolation only in v1. Warping is the target morphing algorithm for a future version.
- **More than 2 IR slots** — architecture supports N, but UI and morph algorithm expose only 2 slots in v1.
- **XY pad morph control** — single knob/slider only in v1.
- **IR time-stretch / reverse** — not in v1.
- **Stereo width control** — not in v1.
- **Preset system** — plugin state save/restore (DAW project recall) is in scope; a dedicated preset browser is not.
- **Windows support** — macOS only in v1. Linux is a future target with FFTW as the FFT backend.
- **AAX (Pro Tools)** — excluded due to iLok and Avid certification requirements.
- **MIDI control beyond DAW automation** — not in v1.

## Further Notes

- The morph update rate (30–60Hz) means the blended IR may lag the Morph Position by up to ~33ms. This is perceptually smooth for continuous sweeps. A snap/jump of the Morph Position (hard cut) will be reflected within one morph thread period — the reverb tail from the old IR rings out naturally while new input immediately responds to the next computed IR.
- Pre-computed slot FFTs at max size (~23MB per slot channel at 30s / 48kHz float32) mean a 2-slot plugin holds approximately 4 FFT buffers × 23MB = ~92MB in the worst case. This is acceptable for a desktop plugin but should be documented prominently.
- The SpectralMorpher uses phase from the dominant slot (higher weight). This is a known approximation — it avoids phase cancellation artefacts from naive complex interpolation. The envelope-aware warping approach (future v2) is the correct solution for fully natural morphing.
- JUCE's built-in `juce::dsp::Convolution` is explicitly not used. MorphIR implements its own NUPC engine to retain full control over the IR buffer and swap mechanism.
