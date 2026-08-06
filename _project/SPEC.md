# SPEC — sappchoir

<!-- UPDATE WHEN: goals change, scope changes, non-goals change, or the target user changes -->

## What this is

An ethereal choir & vocal-pad instrument (JUCE Standalone/VST3/AU + agent
CLI) built on the SappSounds sample engine, following the sapporchestra
architecture. It turns free SFZ choir libraries (Sonatina Chorus,
FreePats/legato-vocal) into a playable sacred-space choir with **live vowel
morphing** as the signature feature. Users: Michael (music production) and
MIDI-generation agents (sapptune) via the JSON CLI + SappLink manifest.

## Why it exists

Free choir libraries are static "aah"/single-vowel sustains. Real choirs
shape vowels through phrases. SappSounds' live CC crossfade engine
(`xfin_loccN`/`xfout_hiccN`) makes per-voice vowel morphing possible; no
free instrument packages it playable. sappchoir does, and exposes it to
agents through a deterministic render API.

## Goals

1. **Vowel morphing on CC 20** — oo/oh/ah/eh layers crossfade while notes
   sound. Libraries without native vowel layers get formant-filtered layers
   generated offline at load (biquad formant banks in the core).
2. Choir performance controls: CC1 dynamics (level+timbre), CC11 expression,
   breath/air, ensemble size (random-tune spread + width + slow breathing),
   legato level 2.
3. Cathedral room: distant early reflections + long FDN tail (T60 to 20 s,
   wide damping range).
4. Candlelit cathedral UI with a big vowel morph wheel.
5. Agent CLI (`inspect | validate | params | vowels | scan | render`),
   JSON out, seeded/deterministic; SappLink v1 manifest + drift guard.

## Non-goals (for now)

- Shimmer (octave-up send) — roadmap, not v0.1.
- Word/syllable building (consonants, lyric engines).
- Redistributing samples — libraries are fetched by the user.
- SappLink code inside SappSounds (engine stays product-neutral).
