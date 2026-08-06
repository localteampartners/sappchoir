#!/usr/bin/env python3
"""Render a short sacred choral progression with Sonatina's Mixed Chorus
through the sappchoir CLI: SATB voicings, CC1 swells, and the signature
vowel morph ridden on CC20 (oo -> ah -> oo) while the chords sound.

Usage: make_choir_demo.py [cli-path] [chorus.sfz] [out.wav]
"""
import os
import struct
import subprocess
import sys

CLI = sys.argv[1] if len(sys.argv) > 1 else "./build/sappchoir"
SFZ = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser(
    "~/Samples/sonatina/Sonatina Symphonic Orchestra/Chorus - Performance/Mixed Chorus.sfz")
OUT = sys.argv[3] if len(sys.argv) > 3 else "/tmp/sappchoir-demo.wav"
TPQ = 480
BPM = 64


def write_midi(path, events):
    events = sorted(events, key=lambda e: e[0])
    track = b""
    us = int(60_000_000 / BPM)
    track += bytes([0x00, 0xFF, 0x51, 0x03]) + us.to_bytes(3, "big")
    last = 0
    for tick, data in events:
        delta = tick - last
        last = tick
        vlq = [delta & 0x7F]
        d = delta >> 7
        while d:
            vlq.append(0x80 | (d & 0x7F))
            d >>= 7
        track += bytes(reversed(vlq)) + bytes(data)
    track += bytes([0x00, 0xFF, 0x2F, 0x00])
    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, TPQ))
        f.write(b"MTrk" + struct.pack(">I", len(track)) + track)


def b(beats):
    return int(beats * TPQ)


def note(ev, start, dur, key, vel):
    ev.append((b(start), [0x90, key, vel]))
    ev.append((b(start + dur), [0x80, key, 0]))


def cc(ev, at, num, val):
    ev.append((b(at), [0xB0, num, max(0, min(127, int(val)))]))


def ramp(ev, num, t0, t1, v0, v1, steps=24):
    for i in range(steps + 1):
        cc(ev, t0 + (t1 - t0) * i / steps, num, v0 + (v1 - v0) * i / steps)


# --- The piece: 8 slow bars in D minor, ~30 s --------------------------------
# Dm - Bb - F - C | Dm - Gm - Asus4->A - Dm, SATB inside the chorus range.
CHORDS = [
    [50, 57, 62, 65, 69],           # Dm:   D3 A3 D4 F4 A4
    [46, 53, 58, 62, 70],           # Bb:   Bb2 F3 Bb3 D4 Bb4
    [45, 53, 57, 60, 69],           # F:    A2 F3 A3 C4 A4
    [48, 55, 60, 64, 67],           # C:    C3 G3 C4 E4 G4
    [50, 57, 62, 65, 74],           # Dm:   D3 A3 D4 F4 D5
    [43, 55, 58, 62, 70],           # Gm:   G2 G3 Bb3 D4 Bb4
    [45, 57, 62, 64, 69],           # Asus4->: A2 A3 D4 E4 A4
    [50, 57, 62, 66, 69],           # D:    D3 A3 D4 F#4 A4 (picardy)
]

ev = []

# Each chord = one whole bar (4 beats), slightly overlapped for legato slurs.
for i, chord in enumerate(CHORDS):
    start = i * 4.0
    dur = 4.15 if i < len(CHORDS) - 1 else 8.0   # final chord rings out
    for key in chord:
        note(ev, start, dur, key, 84)

# The vowel journey (CC20): closed oo, opening through oh into a radiant ah
# at the climax (bar 5), then folding back to oo for the final cadence.
cc(ev, 0, 20, 0)                       # oo
ramp(ev, 20, 4.0, 12.0, 0, 42)         # -> oh across bars 2-3
ramp(ev, 20, 12.0, 18.0, 42, 85)       # -> ah into the climax
ramp(ev, 20, 20.0, 30.0, 85, 10)       # fold back toward oo
cc(ev, 0, 21, 45)                      # a breath of air throughout

# CC1 swells: each two-bar phrase breathes; the climax opens fully.
ramp(ev, 1, 0.0, 4.0, 48, 76)
ramp(ev, 1, 4.0, 8.0, 76, 58)
ramp(ev, 1, 8.0, 14.0, 58, 92)
ramp(ev, 1, 14.0, 16.0, 92, 70)
ramp(ev, 1, 16.0, 20.0, 70, 108)       # climax
ramp(ev, 1, 20.0, 26.0, 108, 62)
ramp(ev, 1, 26.0, 32.0, 62, 40)        # dying fall
# CC11 phrase balance.
cc(ev, 0, 11, 116)
ramp(ev, 11, 26.0, 34.0, 116, 84)

midi_path = "/tmp/sappchoir-demo.mid"
write_midi(midi_path, ev)

result = subprocess.run(
    [CLI, "render", "--sfz", SFZ, "--midi", midi_path, "--out", OUT,
     "--seed", "20260806", "--tail", "10",
     "--param", "ensemble=0.8",
     "--param", "width=1.35",
     "--param", "space_decay=8.5",
     "--param", "space_size=1.3",
     "--param", "tail_level=0.5",
     "--param", "early_level=0.3",
     "--param", "master_gain_db=8"],
    capture_output=True, text=True)
print(result.stdout.strip())
if result.returncode != 0:
    sys.stderr.write(result.stderr)
    sys.exit(result.returncode)
print(f"demo: {OUT}")
