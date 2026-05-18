# ADR 0003: vDSP on macOS, FFTW on Linux, behind FFTProvider abstraction

## Status
Accepted

## Context
FFT performance is critical for both NUPC partition processing (per audio block, small transforms) and IR morphing (per morph update, large transforms up to 2^21 samples). The plugin is open source (GPL compatible). Primary target is macOS; Linux is a future target.

## Decision
Use Apple vDSP (Accelerate framework) on macOS and FFTW on Linux, wrapped behind a thin `FFTProvider` interface. No Windows target planned.

## Consequences
- Best possible FFT performance on Apple Silicon (AMX hardware acceleration via vDSP)
- Zero external dependency on macOS
- GPL-compatible FFTW on Linux when that port happens
- ~50 lines of abstraction to maintain
- FFTW planning overhead at IR load time on Linux (acceptable — plan is computed off audio thread and cached)
