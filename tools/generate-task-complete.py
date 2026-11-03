"""Generate task-complete.wav: a soft two-note glass bell.

Built-in modules only. Mono 44.1 kHz, 16-bit PCM, about 1.15 s.
"""

import math
import struct
import wave

RATE = 44100
DURATION = 1.15
FRAMES = int(RATE * DURATION)
OUT = "tools/sounds/task-complete.wav"


def strike(buffer, start_seconds, frequency, amplitude, decay):
    start = int(start_seconds * RATE)
    for index in range(start, FRAMES):
        t = (index - start) / RATE
        # Soft 4 ms attack so the transient is a glass edge, not a click.
        attack = 1.0 - math.exp(-t / 0.004)
        envelope = attack * math.exp(-t / decay)
        # A few inharmonic partials give a small bell rather than a pure tone.
        shimmer = 1.0 + 0.0007 * math.sin(2.0 * math.pi * 5.1 * t)
        voice = (
            0.62 * math.sin(2.0 * math.pi * frequency * shimmer * t)
            + 0.23 * math.sin(2.0 * math.pi * frequency * 2.01 * t + 0.35)
            + 0.10 * math.sin(2.0 * math.pi * frequency * 3.03 * t + 1.10)
            + 0.04 * math.sin(2.0 * math.pi * frequency * 4.97 * t + 2.10)
        )
        # Upper partials fade first; the fundamental rings longest.
        voice *= math.exp(-t * 0.55)
        buffer[index] += amplitude * envelope * voice


samples = [0.0] * FRAMES
# E6 then B6: a rising fifth, bright but gentle.
strike(samples, 0.00, 1318.51, 0.50, 0.46)
strike(samples, 0.17, 1975.53, 0.40, 0.52)
# Quiet body note for warmth, not audibly a third strike.
strike(samples, 0.00, 659.25, 0.10, 0.60)

# One-pole low-pass removes the brittle top end and softens the attack.
filtered = []
state = 0.0
for value in samples:
    state += 0.58 * (value - state)
    filtered.append(state)

peak = max(abs(value) for value in filtered) or 1.0
gain = 0.42 / peak
fade_frames = int(0.03 * RATE)

frames = bytearray()
for index, value in enumerate(filtered):
    envelope = 1.0
    remaining = FRAMES - index
    if remaining < fade_frames:
        envelope = remaining / fade_frames
    sample = value * gain * envelope
    frames += struct.pack("<h", int(max(-1.0, min(1.0, sample)) * 32767))

with wave.open(OUT, "wb") as handle:
    handle.setnchannels(1)
    handle.setsampwidth(2)
    handle.setframerate(RATE)
    handle.writeframes(bytes(frames))

print(OUT)
