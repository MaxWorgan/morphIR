# ADR 0002: Double-buffering with atomic pointer swap for IR hot-swap

## Status
Accepted

## Context
The morphing thread continuously writes new IR data while the audio thread reads from it. The audio thread must never block (real-time constraint). A mutex would risk priority inversion and audio dropouts.

## Decision
Maintain two full IR buffers (front and back). The morphing thread always writes to the back buffer. When a new IR is ready, it sets an atomic "swap pending" flag. The audio thread checks this flag at each block boundary and swaps the front/back pointers in a single atomic operation.

## Consequences
- Audio thread never blocks
- 2× IR memory cost (~11MB for a 30s mono IR at 48kHz — acceptable)
- Morphing thread may be one update cycle behind if the audio thread hasn't swapped yet — perceptually irrelevant at 30–60Hz morph update rate
- Old back buffer is immediately available for the next morph computation after swap
