# ADR 0001: Non-Uniform Partitioned Convolution for IR processing

## Status
Accepted

## Context
The plugin must support IRs up to 30 seconds (1,440,000 samples at 48kHz). Naive single-block FFT convolution introduces unacceptable latency. Uniform partitioned convolution requires large partitions to be CPU-efficient, also producing unacceptable latency. The plugin is intended for expressive real-time performance use, so latency matters.

## Decision
Use Non-Uniform Partitioned Convolution (NUPC). Small partitions (e.g. 64–512 samples) handle the early IR with near-zero latency, processed every audio block. Large partitions (e.g. 8192–65536 samples) handle the tail efficiently, scheduled every N blocks.

## Consequences
- Near-zero latency for early reflections
- CPU-efficient processing of long tails
- Natural scheduling seams in the tail partitions where the morphing thread can safely write updated IR data
- More complex implementation than UPC
