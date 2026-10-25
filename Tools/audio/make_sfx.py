#!/usr/bin/env python3
"""Builds Atlas's sound-effect pack (Assets/Sounds/*.wav).

Every effect is synthesized here from layered noise, oscillators,
filters, saturation and a small synthetic room, then written as 16-bit
mono 44.1 kHz WAVs. Effects that fire often get several takes
(name_1.wav, name_2.wav, ...) and the engine picks among them so rapid
repeats don't sound like one sample looping.

    python3 Tools/audio/make_sfx.py

Requires numpy. The output is deterministic (fixed seeds).
"""

import os
import wave

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT_DIR = os.path.join(ROOT, "Assets", "Sounds")
SR = 44100


# ---------------------------------------------------------------------------
# Building blocks
# ---------------------------------------------------------------------------

def ns(seconds):
    """Sample count for a duration (rounded, so lengths always agree)."""
    return int(round(seconds * SR))


class Kit:
    def __init__(self, seed):
        self.rng = np.random.default_rng(seed)

    def noise(self, seconds):
        return self.rng.uniform(-1.0, 1.0, ns(seconds))

    def jitter(self, amount):
        """A random factor around 1.0, for take-to-take variation."""
        return 1.0 + self.rng.uniform(-amount, amount)


def t_axis(n):
    return np.arange(n) / SR


def silence(seconds):
    return np.zeros(ns(seconds))


def place(buf, sound, at):
    """Mixes sound into buf starting at `at` seconds (buf grows if needed)."""
    start = ns(at)
    end = start + len(sound)
    if end > len(buf):
        buf = np.concatenate([buf, np.zeros(end - len(buf))])
    buf[start:end] += sound
    return buf


def env_exp(n, decay, attack=0.001):
    """Exponential decay (time constant `decay` s) with a short linear attack."""
    t = t_axis(n)
    e = np.exp(-t / max(decay, 1e-4))
    if attack > 0:
        e *= np.clip(t / attack, 0.0, 1.0)
    return e


def env_ar(n, attack, release_start, release):
    t = t_axis(n)
    e = np.clip(t / max(attack, 1e-4), 0.0, 1.0)
    e *= np.clip(1.0 - (t - release_start) / max(release, 1e-4), 0.0, 1.0)
    return e


def spectral(x, response):
    """Filters x by a magnitude response given as a function of Hz (FFT)."""
    n = len(x)
    size = 1 << (n - 1).bit_length()
    spec = np.fft.rfft(x, size)
    freqs = np.fft.rfftfreq(size, 1.0 / SR)
    spec *= response(freqs)
    return np.fft.irfft(spec, size)[:n]


def lowpass(x, cutoff, order=2):
    return spectral(x, lambda f: 1.0 / np.sqrt(1.0 + (f / cutoff) ** (2 * order)))


def highpass(x, cutoff, order=2):
    return spectral(x, lambda f: 1.0 / np.sqrt(1.0 + (cutoff / np.maximum(f, 1e-3)) ** (2 * order)))


def bandpass(x, low, high, order=2):
    return highpass(lowpass(x, high, order), low, order)


def resonator(x, hz, q):
    """Narrow peak at hz (a ringing body: metal, wood, a tube)."""
    return spectral(x, lambda f: 1.0 / np.sqrt(1.0 + (q * (f / hz - hz / np.maximum(f, 1e-3))) ** 2))


def sweep(n, f0, f1, curve=1.0, wave="sine"):
    """Oscillator gliding from f0 to f1 over n samples."""
    k = np.linspace(0.0, 1.0, n) ** curve
    freq = f0 + (f1 - f0) * k
    phase = np.cumsum(freq) / SR
    if wave == "sine":
        return np.sin(2 * np.pi * phase)
    if wave == "tri":
        return 4.0 * np.abs((phase % 1.0) - 0.5) - 1.0
    if wave == "square":
        return np.tanh(np.sin(2 * np.pi * phase) * 4.0)
    if wave == "saw":
        return 2.0 * (phase % 1.0) - 1.0
    raise ValueError(wave)


def tone(seconds, hz, wave="sine"):
    n = ns(seconds)
    return sweep(n, hz, hz, wave=wave)


def saturate(x, drive):
    return np.tanh(x * drive) / np.tanh(drive)


def room(x, kit, size=0.4, mix=0.2, damp=3000.0, predelay=0.012):
    """Adds a synthetic reverb tail: decaying, darkening noise impulse."""
    n = ns(size)
    ir = kit.noise(size) * env_exp(n, size / 5.0, attack=0.0)
    ir = lowpass(ir, damp, 1)
    ir[: ns(predelay)] = 0.0
    ir /= np.sqrt(np.sum(ir ** 2)) + 1e-9
    size_fft = 1 << (len(x) + n - 1).bit_length()
    wet = np.fft.irfft(np.fft.rfft(x, size_fft) * np.fft.rfft(ir, size_fft), size_fft)
    wet = wet[: len(x) + n]
    dry = np.concatenate([x, np.zeros(n)])
    return dry + wet * mix


def echo(x, delays, gains, cutoff=2500.0):
    """Discrete outdoor slap-backs, each darker than the last."""
    out = np.concatenate([x, np.zeros(ns(max(delays)) + 1)])
    for d, g in zip(delays, gains):
        tail = lowpass(x, cutoff * (1.0 - d * 0.6), 1) * g
        out = place(out, tail, d)[: len(out)]
    return out


def fade_out(x, seconds=0.01):
    n = min(len(x), ns(seconds))
    x = x.copy()
    x[-n:] *= np.linspace(1.0, 0.0, n)
    return x


def trim(x, threshold=1e-3):
    idx = np.nonzero(np.abs(x) > threshold * np.max(np.abs(x)))[0]
    return x[: idx[-1] + 1] if len(idx) else x


def finish(x, peak):
    x = x - np.mean(x)
    x = fade_out(trim(x), 0.015)
    return x * (peak / (np.max(np.abs(x)) + 1e-9))


def click(kit, seconds=0.003, hz=4000.0, q=4.0):
    n = ns(seconds) + 1
    return resonator(kit.noise(n / SR), hz, q) * env_exp(n, seconds / 3.0, 0.0)


def metal_tick(kit, hz, decay=0.03, amp=1.0):
    """A short, ringing metallic contact: a few inharmonic partials."""
    n = ns(decay * 6)
    out = np.zeros(n)
    for ratio, gain in ((1.0, 1.0), (2.76, 0.5), (5.4, 0.3), (8.93, 0.15)):
        out += np.sin(2 * np.pi * hz * ratio * kit.jitter(0.02) * t_axis(n)) * gain * env_exp(
            n, decay / ratio ** 0.5, 0.0005)
    out += highpass(kit.noise(n / SR), 2000) * env_exp(n, 0.002, 0.0) * 0.8
    return out * amp


# ---------------------------------------------------------------------------
# Effects. Each returns a mono float array.
# ---------------------------------------------------------------------------

def gunshot(kit, body_lo, body_hi, body_decay, thump_hz, thump_decay, crack, tail,
            echo_gain, drive):
    length = 0.06 + body_decay * 8
    n = ns(length)
    # supersonic crack / muzzle blast transient
    out = highpass(kit.noise(length), 3000, 2) * env_exp(n, 0.0025, 0.0003) * crack
    # gas body
    out += bandpass(kit.noise(length), body_lo, body_hi) * env_exp(n, body_decay, 0.0008)
    # chest thump
    out += sweep(n, thump_hz * 2.2, thump_hz * 0.6, 0.4) * env_exp(n, thump_decay, 0.001) * 0.9
    out = saturate(out, drive)
    out = room(out, kit, size=tail, mix=0.18, damp=2600)
    if echo_gain > 0:
        out = echo(out, [0.11 * kit.jitter(0.1), 0.24 * kit.jitter(0.1)],
                   [echo_gain, echo_gain * 0.5], 1800)
    return out


def sfx_shot(kit):
    out = gunshot(kit, 500, 4200, 0.022 * kit.jitter(0.15), 140 * kit.jitter(0.08), 0.02,
                  0.9, 0.14, 0.0, 2.2)
    # bolt cycling
    out = place(out, metal_tick(kit, 2400 * kit.jitter(0.1), 0.012, 0.25), 0.028)
    return out


def sfx_shotgun(kit):
    out = gunshot(kit, 180, 2600, 0.05 * kit.jitter(0.1), 85, 0.06, 0.7, 0.5, 0.22, 2.8)
    # pump: back, forward
    out = place(out, metal_tick(kit, 900, 0.025, 0.35) +
                np.pad(bandpass(kit.noise(0.05), 600, 3000) * env_exp(ns(0.05), 0.012) * 0.3,
                       (0, max(0, ns(0.15) - ns(0.05)))), 0.42)
    out = place(out, metal_tick(kit, 1250, 0.02, 0.45), 0.53)
    return out


def sfx_rifle(kit):
    out = gunshot(kit, 350, 5000, 0.035 * kit.jitter(0.1), 110, 0.035, 1.1, 0.4, 0.3, 2.6)
    return out


def sfx_sniper(kit):
    out = gunshot(kit, 250, 6000, 0.05, 80, 0.06, 1.4, 0.9, 0.45, 3.2)
    # rolling thunder down the valley
    roll = lowpass(kit.noise(1.4), 400, 2) * env_ar(ns(1.4), 0.15, 0.2, 1.2) * 0.35
    out = place(out, roll, 0.12)
    # the bolt
    out = place(out, metal_tick(kit, 1500, 0.02, 0.2), 0.7)
    out = place(out, metal_tick(kit, 1900, 0.02, 0.25), 0.82)
    return out


def sfx_rocket(kit):
    length = 0.9
    n = ns(length)
    pop = sweep(ns(0.12), 160, 50, 0.5) * env_exp(ns(0.12), 0.04)
    out = np.zeros(n)
    out = place(out, pop, 0.0)
    # motor: noise through a rising band, with a growl
    roar = kit.noise(length)
    low_band = lowpass(roar, 900, 2)
    high_band = bandpass(roar, 1500, 6000)
    rise = np.linspace(0.0, 1.0, n) ** 0.6
    motor = (low_band * (1.0 - rise * 0.5) + high_band * rise * 0.6)
    motor *= env_ar(n, 0.04, 0.35, 0.55)
    motor *= 1.0 + 0.3 * np.sin(2 * np.pi * 37 * t_axis(n))
    out += motor * 0.8
    out = saturate(out, 1.8)
    return room(out, kit, 0.4, 0.15)


def sfx_explosion(kit):
    length = 2.0
    n = ns(length)
    out = np.zeros(n)
    # blast front
    out += highpass(kit.noise(length), 1200) * env_exp(n, 0.008, 0.0005) * 0.7
    # body
    out += lowpass(kit.noise(length), 1400, 2) * env_exp(n, 0.18 * kit.jitter(0.15), 0.002)
    # sub drop
    out += sweep(n, 90 * kit.jitter(0.1), 28, 0.35) * env_exp(n, 0.35, 0.003) * 1.4
    # debris crackle: sparse impulses
    crackle = np.zeros(n)
    for _ in range(140):
        at = ns(kit.rng.uniform(0.02, 1.1) ** 1.6)
        if at < n:
            crackle[at] += kit.rng.uniform(-1, 1) * (1.0 - at / n)
    out += bandpass(crackle, 800, 7000) * 1.2
    # long rumble
    out += lowpass(kit.noise(length), 220, 2) * env_ar(n, 0.05, 0.25, 1.6) * 0.8
    out = saturate(out, 2.5)
    return room(out, kit, 1.2, 0.3, 1800)


def sfx_flame(kit):
    length = 0.32
    n = ns(length)
    roar = lowpass(kit.noise(length), 1200, 1) + bandpass(kit.noise(length), 2000, 5000) * 0.25
    flutter = 1.0 + 0.5 * lowpass(kit.noise(length), 25, 1) / 0.05
    out = roar * np.clip(flutter, 0.2, 2.0) * env_ar(n, 0.05, 0.16, 0.16)
    return saturate(out, 1.5)


def sfx_dig(kit):
    length = 0.18
    n = ns(length)
    out = np.zeros(n)
    # gravel grains
    for _ in range(int(kit.rng.integers(9, 15))):
        g = ns(kit.rng.uniform(0.004, 0.012))
        grain = bandpass(kit.noise(g / SR), 300, 2400 * kit.jitter(0.3)) * env_exp(g, g / SR / 3, 0.0005)
        at = ns(kit.rng.uniform(0.0, 0.13))
        out[at:at + g] += grain[: n - at] * kit.rng.uniform(0.3, 1.0)
    out = lowpass(out, 2600, 1)
    out += sweep(n, 120, 70) * env_exp(n, 0.03, 0.006) * 0.35
    return out * env_ar(n, 0.006, 0.12, 0.06)


def sfx_jump(kit):
    length = 0.22
    n = ns(length)
    whoosh = bandpass(kit.noise(length), 400, 2500) * env_ar(n, 0.03, 0.05, 0.15)
    kick = lowpass(kit.noise(0.04), 600) * env_exp(ns(0.04), 0.01)
    gear = metal_tick(kit, 1700 * kit.jitter(0.1), 0.01, 0.15)
    out = whoosh * 0.6
    out = place(out, kick, 0.0)
    out = place(out, gear, 0.015)
    return out


def sfx_land(kit):
    length = 0.35
    n = ns(length)
    thud = sweep(n, 110, 45, 0.5) * env_exp(n, 0.05, 0.001)
    crunch = bandpass(kit.noise(length), 300, 3000) * env_exp(n, 0.04, 0.001) * 0.5
    gear = metal_tick(kit, 1300 * kit.jitter(0.1), 0.015, 0.2)
    out = thud + crunch
    out = place(out, gear, 0.02)
    return saturate(out, 1.6)


def formant_voice(kit, seconds, pitch0, pitch1, formants, breath=0.3):
    n = ns(seconds)
    glottal = sweep(n, pitch0, pitch1, 0.7, "saw")
    glottal += kit.noise(seconds) * breath
    out = np.zeros(n)
    for hz, q, gain in formants:
        out += resonator(glottal, hz, q) * gain
    return out


def sfx_hurt(kit):
    length = 0.32
    n = ns(length)
    impact = lowpass(kit.noise(0.06), 900) * env_exp(ns(0.06), 0.012)
    smack = bandpass(kit.noise(0.04), 900, 4000) * env_exp(ns(0.04), 0.006) * 0.5
    p = 150 * kit.jitter(0.15)
    grunt = formant_voice(kit, 0.24, p * 1.25, p * 0.8,
                          [(650 * kit.jitter(0.1), 6, 1.0), (1100, 8, 0.6), (2500, 10, 0.25)], 0.2)
    grunt *= env_ar(len(grunt), 0.015, 0.1, 0.14)
    out = np.zeros(n)
    out = place(out, impact + np.pad(smack, (0, len(impact) - len(smack))), 0.0)
    out = place(out, grunt * 0.55, 0.02)
    return saturate(out, 1.8)


def sfx_gib(kit):
    length = 0.7
    n = ns(length)
    out = np.zeros(n)
    for _ in range(int(kit.rng.integers(5, 9))):
        g = ns(kit.rng.uniform(0.03, 0.09))
        squelch = resonator(kit.noise(g / SR), kit.rng.uniform(300, 900), 2.5)
        squelch *= env_exp(g, g / SR / 4, 0.002)
        at = ns(kit.rng.uniform(0.0, 0.35) ** 1.5)
        out[at:at + g] += squelch[: n - at] * kit.rng.uniform(0.4, 1.0)
    crack = bandpass(kit.noise(0.02), 1500, 6000) * env_exp(ns(0.02), 0.003)
    out = place(out, crack * 0.8, 0.005)
    out += sweep(n, 90, 40) * env_exp(n, 0.06) * 0.7
    return saturate(out, 2.0)


def sfx_throw(kit):
    length = 0.3
    n = ns(length)
    swish = bandpass(kit.noise(length), 600, 4000)
    swish *= np.sin(np.linspace(0, np.pi, n)) ** 2
    pin = metal_tick(kit, 3200, 0.04, 0.35)
    out = swish * 0.7
    return place(out, pin, 0.0)


def sfx_reload(kit):
    out = silence(0.45)
    out = place(out, metal_tick(kit, 1100, 0.02, 0.6), 0.0)            # mag release
    slide = bandpass(kit.noise(0.06), 800, 3500) * env_ar(ns(0.06), 0.01, 0.03, 0.03) * 0.25
    out = place(out, slide, 0.08)
    out = place(out, metal_tick(kit, 780, 0.03, 1.0), 0.16)            # mag seated
    out = place(out, metal_tick(kit, 1650, 0.018, 0.7), 0.3)           # charging handle
    out = place(out, metal_tick(kit, 2100, 0.014, 0.6), 0.36)
    return room(out, kit, 0.15, 0.1)


def sfx_jetburst(kit):
    length = 0.24
    n = ns(length)
    hiss = bandpass(kit.noise(length), 700, 5000) * 0.6 + lowpass(kit.noise(length), 300) * 1.2
    hiss *= 1.0 + 0.25 * np.sin(2 * np.pi * 55 * t_axis(n))
    return saturate(hiss * env_ar(n, 0.03, 0.12, 0.12), 1.4)


def sfx_dronedown(kit):
    length = 1.0
    n = ns(length)
    whine = sweep(n, 1400, 180, 0.8, "saw") * env_ar(n, 0.005, 0.45, 0.2) * 0.3
    whine = lowpass(whine, 3000)
    fizz = highpass(kit.noise(length), 2500) * env_exp(n, 0.12, 0.002) * 0.4
    zap = sweep(ns(0.08), 3000, 600) * env_exp(ns(0.08), 0.02) * 0.5
    out = whine + fizz
    out = place(out, zap, 0.0)
    crash = sfx_land(kit) * 1.2
    crash = place(crash, metal_tick(kit, 700, 0.06, 0.8), 0.0)
    out = place(out, crash, 0.55)
    return room(out, kit, 0.4, 0.15)


def sfx_ui_click(kit):
    n = ns(0.06)
    out = sweep(n, 1900, 1500) * env_exp(n, 0.012, 0.0008) * 0.6
    return place(out, click(kit, 0.004, 5000, 3) * 0.4, 0.0)


def sfx_squad_order(kit):
    # Radio: squelch, two beeps, squelch.
    out = silence(0.35)
    out = place(out, bandpass(kit.noise(0.05), 1500, 5000) * env_exp(ns(0.05), 0.015) * 0.5, 0.0)
    out = place(out, tone(0.06, 1250, "square") * env_ar(ns(0.06), 0.004, 0.05, 0.01) * 0.35, 0.04)
    out = place(out, tone(0.08, 1660, "square") * env_ar(ns(0.08), 0.004, 0.07, 0.01) * 0.35, 0.12)
    out = place(out, bandpass(kit.noise(0.08), 1500, 5000) * env_exp(ns(0.08), 0.025) * 0.4, 0.22)
    return bandpass(out, 300, 3400)


def sfx_weapon_switch(kit):
    out = silence(0.22)
    cloth = bandpass(kit.noise(0.08), 500, 3000) * env_ar(ns(0.08), 0.02, 0.03, 0.05) * 0.3
    out = place(out, cloth, 0.0)
    out = place(out, metal_tick(kit, 1350 * kit.jitter(0.05), 0.02, 0.9), 0.06)
    out = place(out, metal_tick(kit, 2300, 0.012, 0.5), 0.11)
    return out


def sfx_body_switch(kit):
    length = 0.5
    n = ns(length)
    rise = sweep(n, 300, 2400, 1.8, "tri") * env_ar(n, 0.01, 0.3, 0.2) * 0.4
    shimmer = np.zeros(n)
    for hz in (1320, 1760, 2640):
        shimmer += np.sin(2 * np.pi * hz * t_axis(n)) * env_ar(n, 0.2, 0.32, 0.18) * 0.15
    blip = tone(0.04, 3000) * env_exp(ns(0.04), 0.01) * 0.3
    out = rise + shimmer
    out = place(out, blip, 0.42)
    return room(out, kit, 0.4, 0.25, 5000)


def bell(hz, seconds, amp=1.0):
    n = ns(seconds)
    t = t_axis(n)
    out = np.sin(2 * np.pi * hz * t) + 0.35 * np.sin(2 * np.pi * hz * 2.0 * t) * np.exp(-t * 8)
    return out * env_exp(n, seconds / 4, 0.002) * amp


def sfx_heal(kit):
    out = silence(0.7)
    for i, hz in enumerate((784, 988, 1175, 1568)):
        out = place(out, bell(hz, 0.45, 0.4), i * 0.06)
    out = place(out, sfx_weapon_switch(kit) * 0.4, 0.0)
    return room(out, kit, 0.5, 0.25, 6000)


def sfx_pickup(kit):
    out = silence(0.4)
    out = place(out, metal_tick(kit, 900, 0.03, 1.0), 0.0)
    out = place(out, metal_tick(kit, 1500, 0.02, 0.6), 0.07)
    out = place(out, bell(1047, 0.25, 0.25), 0.12)
    out = place(out, bell(1568, 0.3, 0.25), 0.18)
    return room(out, kit, 0.3, 0.15)


def sfx_steam(kit):
    length = 0.7
    n = ns(length)
    hiss = highpass(kit.noise(length), 3000, 2) * env_ar(n, 0.02, 0.15, 0.5)
    sizzle = np.zeros(n)
    for _ in range(80):
        at = ns(kit.rng.uniform(0, 0.5))
        sizzle[at] += kit.rng.uniform(-1, 1)
    return hiss * 0.6 + highpass(sizzle, 2000)


def sfx_alarm(kit):
    out = silence(1.1)
    for i in range(2):
        hi = sweep(ns(0.22), 700, 760, wave="square") * env_ar(ns(0.22), 0.01, 0.2, 0.02)
        lo = sweep(ns(0.22), 520, 500, wave="square") * env_ar(ns(0.22), 0.01, 0.2, 0.02)
        out = place(out, hi, i * 0.5)
        out = place(out, lo, i * 0.5 + 0.24)
    out = lowpass(out, 2500)
    return room(out, kit, 0.5, 0.2)


def sfx_order_sent(kit):
    out = silence(0.6)
    out = place(out, sfx_squad_order(kit) * 0.6, 0.0)
    out = place(out, bell(1319, 0.3, 0.3), 0.3)
    out = place(out, bell(1760, 0.35, 0.3), 0.38)
    return out


def sfx_ship(kit):
    # A drop ship passing overhead: Doppler-dropping engine whine over a
    # roaring jet, loudest mid-pass.
    length = 2.4
    n = ns(length)
    t = np.linspace(-1.0, 1.0, n)
    loud = 1.0 / (1.0 + (t * 2.6) ** 2)
    doppler = 1.0 + 0.18 * np.tanh(-t * 3.0)
    phase = np.cumsum(420.0 * doppler) / SR
    whine = (np.sin(2 * np.pi * phase) + 0.4 * np.sin(2 * np.pi * phase * 2.01)) * 0.25
    roar = lowpass(kit.noise(length), 700, 2) + bandpass(kit.noise(length), 1500, 5000) * 0.3
    out = (whine + roar) * loud
    out = saturate(out, 1.3)
    return out * env_ar(n, 0.2, length - 0.3, 0.3)


def chord_stab(notes, seconds, amp):
    n = ns(seconds)
    out = np.zeros(n)
    for note in notes:
        hz = 440.0 * 2 ** ((note - 69) / 12)
        for detune in (-0.004, 0.004):
            out += sweep(n, hz * (1 + detune), hz * (1 + detune), wave="saw")
    out = lowpass(out, 2200, 2)
    return out * env_ar(n, 0.01, seconds * 0.6, seconds * 0.4) * amp


def sfx_victory(kit):
    out = silence(2.6)
    seq = [([60, 64, 67], 0.0, 0.25), ([62, 65, 69], 0.25, 0.25), ([64, 67, 71], 0.5, 0.25),
           ([65, 69, 72, 77], 0.8, 1.6)]
    for notes, at, length in seq:
        out = place(out, chord_stab(notes, length, 0.3), at)
    for at in (0.0, 0.25, 0.5, 0.8):
        out = place(out, sweep(ns(0.25), 120, 45) * env_exp(ns(0.25), 0.06) * 0.8, at)
    return room(out, kit, 1.0, 0.3, 4000)


def sfx_defeat(kit):
    out = silence(2.8)
    seq = [([57, 60, 64], 0.0, 0.5), ([55, 58, 62], 0.5, 0.5), ([53, 56, 60], 1.0, 1.6)]
    for notes, at, length in seq:
        out = place(out, chord_stab(notes, length, 0.3), at)
    out = place(out, sweep(ns(1.5), 70, 30) * env_exp(ns(1.5), 0.5) * 0.6, 1.0)
    return room(out, kit, 1.2, 0.35, 3000)


# name: (generator, takes, peak)
EFFECTS = {
    "shot": (sfx_shot, 4, 0.55),
    "shotgun": (sfx_shotgun, 2, 0.65),
    "rifle": (sfx_rifle, 3, 0.6),
    "sniper": (sfx_sniper, 2, 0.7),
    "rocket": (sfx_rocket, 2, 0.55),
    "explosion": (sfx_explosion, 3, 0.8),
    "flame": (sfx_flame, 3, 0.25),
    "dig": (sfx_dig, 5, 0.24),
    "jump": (sfx_jump, 2, 0.28),
    "land": (sfx_land, 3, 0.38),
    "hurt": (sfx_hurt, 4, 0.36),
    "gib": (sfx_gib, 3, 0.55),
    "throw": (sfx_throw, 2, 0.26),
    "reload": (sfx_reload, 2, 0.34),
    "jetburst": (sfx_jetburst, 3, 0.22),
    "dronedown": (sfx_dronedown, 2, 0.5),
    "uiclick": (sfx_ui_click, 1, 0.3),
    "squadorder": (sfx_squad_order, 1, 0.26),
    "weaponswitch": (sfx_weapon_switch, 2, 0.32),
    "bodyswitch": (sfx_body_switch, 1, 0.35),
    "heal": (sfx_heal, 1, 0.35),
    "pickup": (sfx_pickup, 1, 0.38),
    "steam": (sfx_steam, 2, 0.22),
    "alarm": (sfx_alarm, 1, 0.32),
    "ordersent": (sfx_order_sent, 1, 0.38),
    "ship": (sfx_ship, 1, 0.45),
    "victory": (sfx_victory, 1, 0.55),
    "defeat": (sfx_defeat, 1, 0.55),
}


def write_wav(path, samples):
    data = np.clip(samples, -1.0, 1.0)
    pcm = (data * 32767.0).astype("<i2").tobytes()
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(SR)
        f.writeframes(pcm)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for old in os.listdir(OUT_DIR):
        if old.endswith(".wav"):
            os.remove(os.path.join(OUT_DIR, old))

    total = 0
    for index, (name, (generator, takes, peak)) in enumerate(EFFECTS.items()):
        for take in range(takes):
            kit = Kit(index * 100 + take + 1)
            samples = finish(generator(kit), peak * kit.jitter(0.04))
            suffix = "_%d" % (take + 1) if takes > 1 else ""
            write_wav(os.path.join(OUT_DIR, name + suffix + ".wav"), samples)
            total += len(samples)
    print("wrote %d effects (%.1f s of audio) to %s" % (len(EFFECTS), total / SR, OUT_DIR))


if __name__ == "__main__":
    main()
