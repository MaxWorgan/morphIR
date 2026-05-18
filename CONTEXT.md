# MorphIR — Domain Glossary

## Core Terms

**IR (Impulse Response)**
A buffer of audio samples that encodes the acoustic character of a space or system. Used as the kernel in convolution reverb.

**Convolution Engine**
The real-time audio processing component that convolves incoming audio with the current IR buffer to produce reverb output. Runs on the audio thread. Must never block.

**IR Slot**
A named position in memory holding a loaded IR buffer. The engine works from slots; the user loads IRs into slots off the audio thread.

**Morph Position**
A continuous, automatable parameter that determines how the active IR is derived from the loaded IR slots. Sweeping it changes the reverb character in real time.

**Morphing Algorithm**
The off-audio-thread process that reads from IR slots and a morph position, computes a blended IR, and writes the result into the engine's working IR buffer. Decoupled from the convolution engine.

**Hard Cut**
A swap of the working IR at a block boundary, such that the accumulated reverb tail (convolution state) reflects the old IR while new input immediately uses the new IR.

**Spectral Interpolation**
A morphing strategy that blends two IRs in the frequency domain before writing the result to the working IR buffer. The engine sees a single, continuously-changing IR — not two parallel engines. v1 implementation: magnitude interpolation with phase from the dominant IR. Target implementation: envelope-aware time-domain warping (see C in morphing algorithm options).

**Dual Mono**
v1 stereo configuration. Two independent mono convolution engines (L and R) running in lockstep, sharing the same slot FFTs and morph position but maintaining separate L/R IR buffers. Distinct from true stereo (4-channel LL/LR/RL/RR). Most commercially distributed stereo IR packs are in this format.

**Blended IR Length**
When morphing between two IRs of different lengths, the working IR length is linearly interpolated: `blend_length = lerp(lengthA, lengthB, t)`. Regions beyond blend_length in the working buffer are zeroed. Working buffers are always pre-allocated at maximum IR length (30s).
