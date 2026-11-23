# HumpCrusher DSP reference

HumpCrusher is a from-scratch port of the signal path of the classic 2006
freeware multi-effect it pays tribute to. The original shipped only as
PPC/i386/x86_64 VST2, AU and RTAS binaries built against Carbon and the old
VSTGUI. None of these load on current macOS, and none run natively on Apple
Silicon.

This document records what the original does, recovered from its x86_64 slice
(the binary still had its debug symbols), so the port can be checked and
maintained. Every constant below was read straight out of the binary.

## Validation

The original VST2 still runs under Rosetta in a minimal host. All 20 factory
presets were rendered through both implementations with the same programme
material, a 60 Hz to 2 kHz sweep plus noise ramping from -26 dB to 0 dB:

| Metric | Result |
|---|---|
| Worst null (residual / signal, after 100 ms) | **-103 dB** |
| Typical null | -115 to -148 dB |
| Worst per-sample difference | 5.9 × 10⁻⁵ |

The remaining difference is float rounding (the original was built with GCC
4.x and x87/SSE mixed precision). `tests/EngineTests.cpp` pins the RMS of
every preset to the original's output to 1 part in 10⁴.

## Signal chain

```
in ─┬─► Mech ─► Filter ─► Tube ─► Compressor ─► (mix with dry) ─► × volume ─► clip ±0.9999 ─► out
    └──────────────────── dry ─────────────────────┘
```

* Audio is processed in sub-blocks of at most **64 samples**.
* Each section runs only when its switch is on. The distortion stages also
  skip when their own drive is 0.
* **Master off** is a true bypass: the input is passed through untouched.
* Mix and volume glide linearly across each sub-block, starting from 0 when
  the plugin is reset, which gives a short fade-in.

## Parameters

All controls are normalised 0..1. Switches count as "on" above 0.5.

| # | Name | Mapping | Display |
|---|---|---|---|
| 0 | Distortion on | switch | |
| 1 | Mech | drive `d` | % |
| 2 | Tube | drive `t` | % |
| 3 | Filter on | switch | |
| 4 | Cutoff | see filter | 80 Hz to 16 kHz |
| 5 | Resonance | `Q = 1 + 6r` | % |
| 6 | Compressor on | switch | |
| 7 | Amount | `a = 0.99 · v` | `+20·log10(1/(1-a))` dB |
| 8 | Phat | switch: raw-peak detector | |
| 9 | Master on | bypass when off | |
| 10 | Mix | wet/dry | % |
| 11 | Volume | `gain = v^2.737` | dB |

Continuous parameters glide linearly to a new value over **10 ms**. The
glided value is sampled once per 64-sample sub-block.

## Mech (hard knee)

```
g     = 1 + 10·d
slope = 0.25 / (g − 0.75)
y     = g·x
|y| > 0.75 :  y = sign(y) · ((|y| − 0.75)·slope + 0.75)
```

The curve is linear up to the 0.75 knee, then flattened so that `x = ±1`
lands exactly on `±1`.

## Tube (parabolic shoulder)

```
g = 1 + 4·t
y = g·x
|y| > 0.6 :  u = (1 − (|y| − 0.6)/(g − 0.6)) · 0.707107
             y = sign(y) · ((0.5 − u²)·0.8 + 0.6)
```

Full scale maps to full scale. Beyond it, `u` goes negative and the curve
folds back down, which contributes to the grit at high drive.

## Filter

**Cutoff mapping** (control `c`):

```
c ≥ 1 → 16 kHz      c ≤ 0 → 80 Hz
s = −0.25 · log10(1 − c)
s > 1 → 16 kHz      s < 1e-7 → 80 Hz
f = 80 + 15920 · s
```

The frequency goes through a MIDI-pitch round trip and is clamped to
0.01 semitone below Nyquist.

**Low-pass:** an RBJ cookbook 2-pole low-pass (`alpha = sin(w)/(2Q)`), direct
form I, run independently on each channel. If the state goes past ±1000 or
denormal, it is zeroed.

**Cutoff glides:** a cutoff change ramps linearly across the sub-block.
Coefficients are recomputed every **16 samples**.

**"Fat" stage:** a stereo-linked limiter after the filter.
* Detector: an envelope follower (below) with attack 7.5 ms and release 50 ms
  on `max(|L|,|R|)`.
* When the envelope exceeds 1, the gain jumps down to `1/env`.
* It then recovers linearly at `(1 − g) / (7.5 s · fs)` per sample.

## Compressor

This is really a drive-plus-soft-limiter. `k = 1/(1 − a)` is applied as input
gain (up to +40 dB), and a soft knee pulls the result back under full scale.

```
peak = max(|L|, |R|)               (Phat on)
peak = env(max(|L|, |R|))          (Phat off; attack 30 ms, release 80 ms)
level = k · peak
level > 0.31 :
    u      = (1 − (level − 0.31)/(k − 0.31)) · 0.707107
    target = ((0.5 − u²)·1.38 + 0.31) / level
    if target < g:  g = target;  rel = (1 − g)/(0.00025 · fs)
out = k · in · g   (while g < 1, then g += rel)
```

* `k` uses the amount at the start of each sub-block.
* In smooth mode the detector sags between waveform peaks. Peaks can
  therefore overshoot full scale by roughly 20%, and the master clip catches
  them. This matches the original.

## Envelope follower

This is a slew-rate limiter, not an RC smoother:

```
floor = v − 1/(release·fs)
δ = x − floor          if x > floor
δ = −x − floor         if x < −floor
δ = 0                  otherwise
v = max(0, floor + min(δ, 1/(attack·fs)))
```

## Factory presets

The 20 presets of the original bank (`CamelCrusherPresetsA.fxb`) are in
`src/FactoryPresets.h`, in the parameter order above. The `.fxb` stores each
program as a float version, a NUL-terminated name, an int32 count (17), and
17 little-endian floats. Slots 12 to 16 are unused.
