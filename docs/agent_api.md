# SappChoir Agent API

The `sappchoir` CLI is the stable machine interface for external software —
MIDI-generation agents in particular. Every command prints exactly one JSON
document to stdout; diagnostics go to stderr; exit codes are `0` ok,
`1` ok-with-warnings, `2` failure.

Binary: `build/sappchoir` (CMake target `sappchoir-cli`).

## Typical agent workflow

```text
1. sappchoir inspect  → learn range, articulations, vowel map, controllers
2. compose MIDI       → chords + CC20 vowel journey + CC1 swells + CC11
3. sappchoir render   → deterministic WAV (fixed --seed)
4. judge / iterate
```

## The vowel morph (the signature)

Every loaded instrument becomes vowel-morphable. If the SFZ already ships
live CC crossfades on **CC 20** they are used as-is; otherwise sappchoir
**generates** four formant-filtered layers (oo/oh/ah/eh) from the source
samples at load and registers them as live crossfades on CC 20
(SappSounds `xfin_locc20`/`xfout_hicc20` semantics — the morph tracks the
controller *while notes sound*).

| CC 20 | vowel |
|---|---|
| 0 | oo (full) |
| 42 | oh |
| 85 | ah |
| 127 | eh |

Sweep gradually (≥ ~24 steps per transition) for a seamless morph.
`sappchoir vowels` dumps this map with the formant frequencies. `--raw`
(inspect/render) skips generation and plays the source SFZ untouched.

## inspect

```bash
sappchoir inspect (--sfz <file.sfz> | --diagnostic) [--regions] [--raw]
```

Returns `name`, `regions`, `estimatedRamBytes`, `vowelMorph` (whether live
vowel crossfades are active), `playableRange`, `articulations` (index +
keyswitch protocol), `capabilities`, `controllers` (CC 20 vowel, CC 1
dynamics, CC 11 expression, CC 64 sustain), and the `vowels` map.

Composition rules an agent should follow:

- Keep notes inside `playableRange`.
- Ride CC 20 through held chords — that is the instrument's voice.
- Ride CC 1 through phrases (choir swells breathe); CC 11 for phrase-level
  balance; CC 64 is a real sustain pedal.
- Switch articulations by keyswitch note, or `--articulation <index>`.

## validate

```bash
sappchoir validate --sfz <file.sfz>
```

`{"ok":bool, "errors":N, "warnings":N, "missingSamples":N, "regions":N,
"vowelMorph":bool, "unsupportedOpcodes":[...], "diagnostics":[...]}`
(validates the source SFZ as-is; no vowel generation).

## params

```bash
sappchoir params
```

Full parameter schema: `{"params":[{name,id,min,max,default,cc,doc}],
"enums":{"quality":[...]}}`. Use these names with `render --param`.

| name | id | range | default | MIDI CC | meaning |
|---|---|---|---|---|---|
| vowel | vowel | 0–1 | 0.35 | 20 | oo→oh→ah→eh morph position |
| dynamics | dynamics | 0–1 | 0.65 | 1 (native) | level + timbre |
| expression | expression | 0–1 | 1.0 | 11 (native) | phrase volume |
| breath | breath | 0–1 | 0.25 | 21 | air lift + breath-noise bed |
| ensemble | ensemble | 0–1 | 0.5 | 22 | section size: detune spread + slow breathing |
| width | width | 0–2 | 1.2 | 18 | stereo width |
| early_level | earlyLevel | 0–1 | 0.28 | 14 | early stone reflections |
| tail_level | tailLevel | 0–1 | 0.45 | 91 | cathedral tail level |
| space_size | spaceSize | 0.2–1.5 | 1.15 | 92 | cathedral size |
| space_decay | spaceDecay | 1–20 | 6.5 | 15 (log) | T60 seconds |
| space_damping | spaceDamping | 0–1 | 0.55 | 19 | stone → drapes absorption |
| legato | legato | 0–1 | 1.0 | 68 | slurred lines (chord-safe) |
| master_gain_db | masterGain | −24–12 | 0 | 7 | output gain |
| clean | clean | 0–1 | 0.0 | 3 | 0 = every modeled imperfection as designed, 1 = none |

**SappLink CC-in:** the MIDI CC column is a live contract — CCs embedded in
a rendered `.mid` (or played into the plugin) move these parameters, with
slew smoothing, on any channel. See [sapplink.md](sapplink.md) and the
manifest at `~/apps/sapptune/sapplink/manifests/sappchoir.json`.

## vowels

```bash
sappchoir vowels
```

`{"vowels":[{index,name,cc,ccFullAt,formantsHz}], "doc": ...}`.

## render

```bash
sappchoir render (--sfz <file.sfz> | --diagnostic) \
    --midi <file.mid> --out <file.wav> \
    [--sr 48000] [--seed N] [--tail seconds] [--raw] \
    [--articulation INDEX] [--param NAME=VALUE ...]
```

- Input: SMF format 0/1. Notes, CCs (20/1/11/64/...), pitch bend honored;
  keyswitch notes switch articulations mid-piece.
- Output: stereo float32 WAV through the full chain (sampler + vowel
  crossfades → dynamics → breath/ensemble → early reflections → cathedral
  tail → limiter). Default tail: 8 s — a cathedral rings long.
- **Deterministic:** identical inputs + `--seed` ⇒ bit-identical WAV. Vary
  the seed for a new ensemble take.

Result: `{"ok":true, "out":..., "frames":N, "durationSeconds":s, "peak":p,
"rms":r, "midiEvents":N, "seed":N, "vowelMorph":bool}`.

## scan

```bash
sappchoir scan <library-dir> [--all]
```

Walks a library folder for `.sfz` instruments (skipping `includes/`
partials unless `--all`) and returns
`{"instruments":[{path,name,category,regions,articulations,keyswitches,
vowelMorph,lowKey,highKey}], "count":N}` — parse-only, fast. This is how an
agent discovers what it can write for.

## Demo

[scripts/make_choir_demo.py](../scripts/make_choir_demo.py) renders an
8-bar sacred progression with Sonatina's Mixed Chorus: SATB voicings,
CC1 swells, and a full oo→ah→oo vowel journey on CC20.

## Stability

Command names, field names, exit codes, and parameter names are contracts.
New fields may be added; existing ones are not renamed or repurposed.
