# AmpDualRect — circuit-level 100 W two-channel rectifier tube amp (VST3)

A companion to Amp5150 III with the same look and the same front and back end: input gain, noise
gate, IR loader with the built-in synthetic cab, low/high cut and output level. The amp in the middle
is modelled from the hand-traced 2-channel "Dual Rectifier Solo Head" schematic (sheets mbdr1–mbdr7),
with four mods added: Mr. Scary (gain + bass switch), Tight, De-fizz and Mid Shift. There is no effects loop:
the masters feed the phase inverter directly, as with the loop relay (RY2) in bypass.

## Building on Windows (Cubase or any VST3 host)

1. Install **Visual Studio Community** (2022 or newer) with the **"Desktop development with C++"** workload
   (it includes CMake), and **Git for Windows**.
2. Double-click **`build_windows.bat`**. The first run downloads JUCE 8.0.8, then builds a Release VST3
   and a Standalone app (a few minutes).
3. Copy `build\AmpDualRect_artefacts\Release\VST3\AmpDualRect.vst3` into
   `C:\Program Files\Common Files\VST3` (the script does this itself when run as administrator).
4. Rescan plug-ins in your host. It appears as **AmpDualRect** (vendor *Amp5150Sim*), under Distortion,
   next to the Amp5150 plug-ins.

Alternatively, push this folder to a GitHub repository: `.github/workflows/build-windows.yml` builds
the VST3 in the cloud and offers it as a download under the Actions tab.

## Controls

| Section | Controls |
|---|---|
| Input / noise gate | Input gain, gate on/off, threshold, decay, sidechain HPF (unchanged from Amp5150 III) |
| Channel | **ORANGE** (orange LED), **RED** (red LED) |
| Orange | Gain, Treble, Mid, Bass, Presence, Master; **MODE** lever CLN / NORM / MOD |
| Red | Gain, Treble, Mid, Bass, Presence, Master; **MODE** lever NORM / VINT |
| Mr. Scary | **GAIN** (0 = stage switched out), **BASS** lever NORM / DEEP |
| Mods | **TIGHT** (green LED), **DE-FIZZ** (blue LED), **MID SHIFT** (violet LED) push-buttons |
| Power | **RECTIFIER** DIODE / TUBE, **POWER** BOLD / SPONGY, **TUBES** 6L6 / EL34 |
| Cabinet | IR on/off, load IR (or drag a .wav onto the window), default synthetic cab, quality |
| Output | Low cut, high cut, output level, peak meter |

Every switch (channel, modes, mods (including Mid Shift), Mr. Scary in/out, rectifier, power, tubes) ducks the output for a
moment and changes while silent, so switching is click-free. Knobs are smoothed.

**Quality:** Normal runs the amp at ~96 kHz internally, High at ~192 kHz, Eco at ~48 kHz.

## What is modelled

Both channels share one tube path, as on the real amp; the channel and mode switches only change the
LDRs (light-dependent resistors) listed in the schematic's truth table (sheet mbdr7). The model does the
same: the LDRs are run-time resistors (lit 150 Ω, dark 25 MΩ) inside the circuit.

- **V1A** (220k / 1k8, cathode cap via LDR3b), 20n coupling, the **pad network** (680k || 2n via LDR4,
  82p, 2M2), **input selects** LDR1/2, the two **1M gain pots** with their 1n bright caps, LDR5/6,
  470k + 20p into **V2A**.
- **V2A** (100k / 1k8, cathode cap via LDR3a), 20n, 470k / 1M into the **V2B "cold" clipper** (39k
  unbypassed cathode, 1n across its 100k load), 20n, 330k / 220k into **V3A** (220k / 1k8, cathode cap
  via LDR10), DC-coupled into the **V3B cathode follower** (100k).
- The **Red and Orange tone stacks** (680p / 500p treble caps, 47k slope, 20n bass and mid caps,
  220k treble, 1M bass, 25k mid), selected by LDR8/9, the other stack fed through 1M.
- **Red presence** (22k + 3n + 25k rheostat from the treble wiper), the **Orange treble roll-off**
  (22k + 3n + 100k / LDR7) and **master bypass** (82k || 82p, LDR16), the two **1M masters** (LDR14/15).
- **Power amp:** 20n into the long-tail-pair phase inverter (82k / 90k plate loads with 120p, 75p
  between plates, 470R + 10k + 4k7 tail), **global NFB** from the speaker tap (100n, 47k, more feedback
  47k via LDR20, switched by LDR19) with the **Orange presence** network (100n + 25k rheostat), 47n
  couplings, 220k bias feeds, 1k5 grid stoppers, **4 × 6L6GC or EL34** (two per side) with grid and
  screen current (1k screen resistors), output transformer and reactive speaker load.
- **Supply:** silicon diodes or 2 × 5U4 (Child's-law plate drop), BOLD / SPONGY (SPONGY: lower
  transformer voltage and more source resistance), reservoir and screen-supply sag through the choke.
- Grid current in every stage (blocking distortion and cathode-follower grid conduction), 12AX7
  inter-electrode capacitances and Miller loading.

Measured in the model: ~91 W at 19 % THD and ~115 W into hard clipping (6L6, silicon, BOLD); the 5U4
rectifier sags the supply to ~370 V and SPONGY to ~325 V at full drive, with 55–80 W.

The mode table (from sheet mbdr7):

| | 1/2 input | 3 cathode caps | 4 pad | 7 roll-off | 16 master bypass | 19 NFB | 20 more NFB |
|---|---|---|---|---|---|---|---|
| ORANGE CLN | Orange | off | padded | on | on | on | on |
| ORANGE NORM | Orange | on | — | on | — | on | — |
| ORANGE MOD | Orange | on | — | — | — | — | — |
| RED NORM | Red | on | — | — | — | — | — |
| RED VINT | Red | on | — | on | — | on | — |

Checked against the schematic's DC voltages: V1A 209 V / 1.6 V (schematic 200 / 1.6), V2A 284 / 2.2
(280 / 2), V2B 394 / 4.8 (384 / 5), V3A 219 / 1.7 (213 / 1.6), V3B cathode 220 (216), PI plates 286 /
282 (280), tail 48.7 (48). The bias supply comes out at -49.9 V for a 34 mA idle per 6L6 (schematic
-51 V). Output: see above.

### Things the trace says that you may notice

- **ORANGE CLN** has all four gain stages with unbypassed cathodes, and LDR16 puts 82k || 82p across
  the upper leg of the ORANGE master, so the master has much less range in that mode. Start with GAIN
  around 2 and MASTER around 1–2 (the defaults); the clean breaks up from about GAIN 3 with humbuckers.
- In **RED VINT** the global feedback is on, and the feedback presence network is the one wired to the
  **ORANGE PRESENCE** pot on the trace. So in RED VINT the Orange presence knob also changes the Red
  sound; RED PRESENCE (the preamp network) works in both Red modes.

## The mods

**TIGHT** — V1A cathode bypass cap 1u → 470n and V1A coupling cap 20n → 2n2 (the values on sheet
mbdr1mod). Less low end into the gain stages: about −6 dB at 50 Hz, −4 dB at 100 Hz, −2 dB in the mids.

**DE-FIZZ** — a cap across V3A's 220k plate load and a 22k grid stopper on the V3B cathode follower (the
change on sheet mbdr1mod). The sheet's 220p took only ~1 dB off the 5–20 kHz band in the model, so 470p
is used: about −2.5 dB of fizz on a saturated tone (−4 dB with Mr. Scary on). Small-signal effect:
−1.5 dB at 4 kHz, −4 dB at 8 kHz.

**MID SHIFT** — both tone stacks' mid caps 20n → 47n (27n switched in parallel). On the stock circuit the
MID knob works mainly between 400 Hz and 1 kHz: at 0 it cuts 4.6 dB at 600 Hz but under 1 dB at 250 Hz,
so nothing on the panel reaches the 150–500 Hz low-mid build-up the amp is known for. With MID SHIFT on:
- at the same MID setting, about −3 dB at 150–250 Hz, with 1 kHz and up unchanged;
- MID at 0 cuts 7–10 dB at 250–400 Hz (stock: 0.7–2.4 dB), so the MID knob becomes the low-mid control;
- through the cab on a saturated Red tone: about −3 dB in the 150–500 Hz band at the same MID setting,
  −4 dB at MID 2. It also trims ~3 dB below 150 Hz, so you may want BASS a notch higher.

**MR. SCARY** — the real Mr. Scary is a plug-in module whose circuit is not published (it is described as
adding a tube gain stage with a gain knob from near-unity to heavy compression and a "Deep" bass switch).
This is **our own design in that spirit**, not a copy: an extra 12AX7 stage right after V1A (before the
pad and the gain pots), an inverting stage with plate-to-grid feedback.
- **GAIN** sets the feedback resistor (470k + 10M audio rheostat against a 470k input resistor) with 33p
  across it, so the added gain stays in the lows and mids: just above 0 it is about unity; at 5 about
  +8 dB; fully up about +22 dB at 100 Hz, +18 dB at 1 kHz, +5 dB at 5 kHz, where the stage itself starts
  to clip like an extra cascaded preamp stage. At **0** a switch on the pot takes the stage out of the
  circuit: the stock amp. Through the cab on the Red channel at gain 6.5, Mr. Scary at 10 gives about
  +2.5 dB of lows, +3 dB of low-mids and −2 dB in the 3–8 kHz fizz band (girth and sustain rather than
  fizz).
- **BASS DEEP** changes the stage's input cap from 4.7n to 47n: its low cut moves from ~72 Hz to ~7 Hz in
  front of the extra gain, a modest bass boost (about +2 dB at 100 Hz, more on palm mutes).
- Why after V1A: the first version sat after V3A (where a plug-in module in the V3 socket would be), but
  there the signal is already clipped to a square wave and the knob changed the sound by 1 dB at most.

### Additions that are not on the trace

- Plate flyback diodes on the output tubes and the transformer's core loss. Without them the model's
  transformer flyback drove the cut-off plates to about 1 kV when cranked (they only conduct when a
  plate is driven below ground). With them the speaker peaks stay around 80–90 V, in line with a 100 W
  amp.
- EL34s are given -1.6 dB drive so the tube switch keeps the volume roughly the same.

### Solver fix (also applies to Amp5150 III)

`Tubes.h` had a tiny discontinuity in the 12AX7 model at deep cut-off: the "off" branch dropped the 1 nA/V
leakage the conducting branch carries, a 0.36 µA step at the switch-over (14 mV across the 39k cold-clipper
cathode). Newton could then cycle between the two branches and give up after 16 iterations on a few
samples per second. With the leakage kept in both branches, 27.6 million samples of guitar-level input
over every mode and mod setting solved without a single unconverged sample. Amp5150 III uses the same
`Tubes.h`, so the same one-line change applies there. `DKNet.h` also gained a damped fallback solve
(restart from the last solution with limited steps), which only runs on extreme inputs.

## Files

```
plugin/   PluginProcessor, PluginEditor (Amp5150 III layout, silver faceplate), Parameters,
          DefaultCab.h (same synthetic cab as Amp5150 III)
dsp/      DualRect.h   - the amp: netlists, LDR truth table, mods, power amp, transformer, supplies
          DKNet.h      - nodal circuit solver (DK method, per-sample Newton, damped fallback)
          Tubes.h      - 12AX7, 6L6GC and EL34 models (same as Amp5150 III)
          NoiseGate.h  - unchanged from Amp5150
tests/    dsp_test.cpp (schematic DC points, per mode), pa_test.cpp (power, sag, NFB, presence),
          tone_test.cpp (tone stacks, mods incl. Mid Shift, Mr. Scary response), stress_test.cpp (all switch
          combinations, extreme inputs), level_match.cpp (EL34 vs 6L6, and vs Amp5150 III),
          offline_host.cpp (renders WAVs, switching / silence tests, UI snapshots)
```

The DSP tests build without JUCE, e.g. `g++ -O3 -std=c++17 -DDRECT_TAPS -Idsp tests/dsp_test.cpp`.

## CPU

Like Amp5150 III this is a heavy model: every tube is solved as a circuit at the oversampled rate. It
costs about 15–20 % more than Amp5150 III (measured on the same machine), because both tone stacks and
the Mr. Scary stage are part of the circuit at all times. Eco quality roughly halves the cost; silent passages cost almost nothing.

## Name

"Mesa", "Boogie", "Rectifier" and "Dual Rectifier" are trademarks of their owners and "Mr. Scary" is a
Legendary Tones / George Lynch product name; they are used here only to describe what was modelled. If
you distribute the plug-in, keep those names off the product itself.
