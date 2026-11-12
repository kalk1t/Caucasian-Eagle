"""Builds the placeholder sounds of Caucasian Eagle: effects, loops and music.

    python art/sounds/build_sounds.py            (from the repository root)

Everything is synthesised here from noise, sine waves, plucked strings and a breathy flute,
so the game has a complete set of sounds without any recording. They are placeholders: any
file in game/assets/sounds can be replaced by a real recording with the same name (WAV,
PCM 16-bit, mono or stereo). The spoken lines are made by art/sounds/build_voice.py.

Made to be easy on the ear: every sound is rounded off above 9 kHz, has a soft start and a
tail of room reverb, and nothing is a raw buzz. Levels differ on purpose (a footstep is
quieter than a guardian's roar), so the game can play most of them near full volume.

The same seed is used every run, so the files come out identical.
"""
import os
import wave
import numpy as np
from scipy.signal import butter, lfilter, fftconvolve

SR = 44100
OUT = os.path.join(os.path.dirname(__file__), '..', '..', 'game', 'assets', 'sounds')
rng = np.random.default_rng(1187)


# ------------------------------------------------------------------ helpers
def t(seconds):
    return np.arange(int(SR * seconds)) / SR

def noise(seconds):
    return rng.uniform(-1, 1, int(SR * seconds))

def band(x, lo, hi, order=2):
    b, a = butter(order, [lo / (SR / 2), min(hi, SR / 2 - 100) / (SR / 2)], btype='band')
    return lfilter(b, a, x, axis=0)

def low(x, f, order=2):
    b, a = butter(order, f / (SR / 2))
    return lfilter(b, a, x, axis=0)

def high(x, f, order=2):
    b, a = butter(order, f / (SR / 2), btype='high')
    return lfilter(b, a, x, axis=0)

def env(n, attack, decay, power=1.0):
    """Rises smoothly over 'attack' seconds, then falls to nothing at the end."""
    x = np.arange(n) / SR
    a = np.sin(np.clip(x / max(attack, 1e-4), 0, 1) * np.pi / 2) ** 2
    d = np.clip(1 - (x - attack) / max(decay, 1e-4), 0, 1) ** power
    return a * d

def sweep(seconds, f0, f1):
    x = t(seconds)
    f = f0 * (f1 / f0) ** (x / seconds)
    return 2 * np.pi * np.cumsum(f) / SR

def norm(x, peak=.9):
    m = np.max(np.abs(x))
    return x * (peak / m) if m > 0 else x

def impulse(seconds, bright=5000):
    """What a stone hall answers to a clap: noise dying away, the high part first."""
    n = int(SR * seconds)
    x = rng.normal(0, 1, n) * np.exp(-np.arange(n) / SR * 6.9 / seconds)
    x[:int(.012 * SR)] *= np.linspace(0, 1, int(.012 * SR))      # the first reflections come a moment later
    return low(x, bright) / np.sqrt(np.sum(x * x))

def room(x, seconds=.7, mix=.18, tail=True):
    """Adds reverb. With 'tail' the sound grows longer by the reverb; without, it keeps its length."""
    wet = fftconvolve(x, impulse(seconds))
    dry = np.concatenate([x, np.zeros(len(wet) - len(x))])
    out = dry + wet * mix * 3
    return out if tail else out[:len(x)]

def room_loop(x, seconds=1.2, mix=.2):
    """Reverb for a loop: the tail of the end is heard at the beginning, so the loop has no seam."""
    if x.ndim == 2:
        return np.stack([room_loop(x[:, c], seconds, mix) for c in range(2)], axis=1)
    wet = fftconvolve(np.concatenate([x, x]), impulse(seconds))[len(x):2 * len(x)]
    return x + wet * mix * 3

def fade_ends(x, start=.004, end=.02):
    a, b = int(SR * start), min(int(SR * end), len(x) // 2)
    x = x.copy()
    x[:a] *= np.linspace(0, 1, a)
    x[-b:] *= np.linspace(1, 0, b) ** 2
    return x

def loopable(x, seconds=.5):
    """Folds the tail over the head so the end runs into the beginning without a click."""
    n = int(SR * seconds)
    head, body, tail = x[:n], x[n:-n], x[-n:]
    ramp = np.sin(np.linspace(0, np.pi / 2, n)) ** 2
    return np.concatenate([head * ramp + tail * (1 - ramp), body])

def save(name, x, peak=.8, loop=False):
    x = np.asarray(x, dtype=np.float64)
    if loop:                                         # filtered as if it had been playing for ever, so the seam stays shut
        n = len(x)
        x = high(low(np.concatenate([x, x, x]), 9000), 35)[n:2 * n]
    else:
        x = high(low(x, 9000), 35)                   # no hiss, no rumble
    x = np.tanh(norm(x, 1.4)) / np.tanh(1.4)         # the loudest moments are rounded, not clipped
    x = norm(x, peak)
    if not loop:
        x = fade_ends(x)
    path = os.path.join(OUT, name + '.wav')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, 'wb') as f:
        f.setnchannels(1 if x.ndim == 1 else 2)
        f.setsampwidth(2)
        f.setframerate(SR)
        f.writeframes((np.clip(x, -1, 1) * 32767).astype('<i2').tobytes())

def bell(seconds, base, ratios, decay=3.0):
    """Struck metal or glass: a few pure tones, the high ones dying first."""
    x = t(seconds)
    out = np.zeros_like(x)
    for i, r in enumerate(ratios):
        out += np.sin(2 * np.pi * base * r * x + rng.uniform(0, 6)) * np.exp(-x * decay * (1 + i * .7)) / (1 + i) ** 1.2
    return out * np.clip(x / .003, 0, 1)

def throat(seconds, f0, f1, formants, breath=.3, growl=0.0):
    """A voice without words: a soft tone and breath through three resonances. 'growl' adds a slow flutter."""
    n = int(SR * seconds)
    x = t(seconds)
    phase = sweep(seconds, f0, f1) + .04 * np.sin(2 * np.pi * 5.5 * x) * 2 * np.pi
    tone = sum(np.sin(k * phase) / k ** 1.6 for k in range(1, 9))          # rounder than a sawtooth
    if growl > 0:
        tone *= 1 - growl * (.5 + .5 * np.sin(2 * np.pi * rng.uniform(17, 24) * x + rng.uniform(0, 6))) ** 2
    source = tone + breath * 2 * low(noise(seconds)[:n], 4000)
    return sum(band(source, f * .8, f * 1.25) * g for f, g in formants)

def pluck(freq, seconds, bright=.5):
    """Karplus-Strong plucked string: the sound of a panduri, near enough."""
    n = int(SR * seconds)
    period = int(SR / freq)
    line = low(rng.uniform(-1, 1, period * 3), 900 + 3500 * bright)[period:2 * period]
    out = np.empty(n)
    for i in range(n):
        j = i % period
        out[i] = line[j]
        line[j] = .997 * .5 * (line[j] + line[(j + 1) % period])
    return out

def flute(freq, seconds):
    """A shepherd's flute (salamuri): a pure tone with a little breath and a slow waver that comes in late."""
    x = t(seconds)
    waver = 1 + .006 * np.sin(2 * np.pi * 5 * x) * np.clip((x - .25) / .4, 0, 1)
    phase = 2 * np.pi * np.cumsum(freq * waver) / SR
    tone = np.sin(phase) + .22 * np.sin(2 * phase) + .07 * np.sin(3 * phase)
    air = band(noise(seconds), freq * 1.5, freq * 4) * .12
    return (tone + air) * env(len(x), .09, seconds - .09, .6)

def strings(freqs, seconds, attack=1.5):
    """A soft held chord, like low strings or a men's choir far away."""
    x = t(seconds)
    out = np.zeros_like(x)
    for f in freqs:
        for detune in (-.003, 0, .0035):
            phase = 2 * np.pi * f * (1 + detune) * x + rng.uniform(0, 6)
            out += sum(np.sin(k * phase) / k ** 2 for k in range(1, 6))
    swell = .85 + .15 * np.sin(2 * np.pi * .2 * x + rng.uniform(0, 6))
    return low(out, 1100) * swell * env(len(x), attack, seconds - attack, 1.3)

def place(buffer, sound, at, gain=1.0, pan=0.0):
    """Adds 'sound' into a stereo loop buffer at 'at' seconds, wrapping round the end."""
    n = buffer.shape[0]
    index = (int(at * SR) % n + np.arange(len(sound))) % n
    np.add.at(buffer[:, 0], index, sound * gain * (1 - max(pan, 0) * .7))
    np.add.at(buffer[:, 1], index, sound * gain * (1 + min(pan, 0) * .7))

def hz(semitones, root=146.83):   # D3
    return root * 2 ** (semitones / 12)

def whoosh(seconds, lo, hi, peak_at=.45, sharp=1.6):
    """Air moved by a blade or a wing: soft noise that swells and dies."""
    n = noise(seconds)
    x = np.arange(len(n)) / len(n)
    shape = np.exp(-((x - peak_at) / .22) ** 2 * sharp)
    return band(n, lo, hi) * shape


# ------------------------------------------------------------------ Nyxara
def build_nyxara():
    for i, f in enumerate((700, 900, 1100)):
        save(f'nyxara/swing_{i + 1}', room(whoosh(.26, f * .6, f * 2.2), .35, .1), .55)
    for i, f in enumerate((420, 520)):
        x = whoosh(.46, f * .5, f * 2.4, .5) + .35 * np.sin(sweep(.46, 95, 55)) * env(int(SR * .46), .12, .3, 2)
        save(f'nyxara/swing_heavy_{i + 1}', room(x, .5, .14), .65)
    save('nyxara/dash', room(whoosh(.5, 500, 3600, .4, 1.2), .6, .16), .65)
    for i, root in enumerate((12, 15)):
        x = t(.9)
        chord = sum(np.sin(2 * np.pi * hz(root + s, 293.66) * x * (1 + .0015 * k)) / (1 + k * .4) for k, s in enumerate((0, 7, 12, 19)))
        shimmer = band(noise(.9), 2500, 6000) * env(len(x), .08, .5, 2) * .25
        save(f'nyxara/skill_{i + 1}', room(chord * env(len(x), .03, .85, 2.4) + shimmer, 1.1, .25), .65)
    save('nyxara/guard', room(bell(.6, 440, (1, 2.01, 2.99, 4.2), 7), .7, .2), .6)
    x = bell(.35, 1175, (1, 2.0, 3.0), 22)
    x[int(.09 * SR):] += bell(.35, 1568, (1, 2.0, 3.0), 22)[:len(x) - int(.09 * SR)]
    save('nyxara/swap', room(x, .4, .15), .45)
    woman = ((800, 1), (1300, .55), (2700, .2))
    for i, (a, b) in enumerate(((310, 262), (330, 277), (294, 247))):       # a short breath out: "ha"
        v = throat(.24, a, b, woman, breath=.9)
        save(f'nyxara/effort_{i + 1}', room(v * env(len(v), .03, .21, 1.6), .4, .12), .4)
    for i, (a, b) in enumerate(((392, 294), (370, 277))):
        v = throat(.34, a, b, woman, breath=.7)
        save(f'nyxara/hurt_{i + 1}', room(v * env(len(v), .025, .31, 1.4), .45, .14), .5)
    v = throat(1.2, 392, 196, woman, breath=.8)
    save('nyxara/death', room(v * env(len(v), .05, 1.15, 1.2), 1.4, .3), .6)
    for i in range(4):                                                        # a boot on stone: a soft thud, a little grit
        n = noise(.11)
        x = low(n, 380 + 60 * i) * env(len(n), .006, .1, 2.5) + .8 * np.sin(sweep(.11, 105 - 6 * i, 62)) * env(len(n), .004, .08, 2)
        save(f'nyxara/step_{i + 1}', room(x, .25, .08), .45)
    for i, f in enumerate((260, 320)):
        n = noise(.5)
        beat = np.sin(np.pi * np.clip(np.arange(len(n)) / len(n) * 2 % 1, 0, 1)) ** 2
        save(f'nyxara/wing_{i + 1}', room(low(n, f * 2) * beat * env(len(n), .04, .46), .5, .15), .6)
    n = noise(.22)
    save('nyxara/land', room(low(n, 260) * env(len(n), .006, .2, 3) + np.sin(sweep(.22, 90, 48)) * env(len(n), .004, .16, 2), .35, .1), .5)
    n = noise(6)
    gust = .7 + .3 * np.sin(2 * np.pi * .35 * t(6)) * np.sin(2 * np.pi * .11 * t(6) + 1)
    save('nyxara/flight', loopable(band(n, 250, 1100) * gust, .8), .5, loop=True)
    out = np.zeros(int(SR * 2.6))                                             # a rising harp and a breath of light
    for k, s in enumerate((0, 7, 12, 16, 19, 24)):
        note = pluck(hz(s, 293.66), 2.6 - k * .17, .6) * (.9 - k * .05)
        start = int(k * .17 * SR)
        out[start:start + len(note)] += note[:len(out) - start]
    out += .25 * strings([hz(0, 293.66), hz(7, 293.66), hz(16, 293.66)], 2.6, 1.0) / 6
    save('nyxara/evolve', room(out, 1.8, .3), .75)
    x = t(.7)
    gulp = sum(np.sin(sweep(.7, 240 + 70 * k, 520 + 110 * k)) * np.clip(np.sin(2 * np.pi * 5 * x + k * 2), 0, 1) ** 2 for k in range(3))
    save('nyxara/potion', room(gulp * env(len(x), .03, .66, 1.5) * .6 + .5 * bell(.7, 1318.5, (1, 2, 3), 5), .6, .2), .5)


# ------------------------------------------------------------------ creatures
def build_creatures():
    beast = ((380, 1), (760, .5), (1500, .12))
    for i, f in enumerate((82, 70, 92)):
        v = throat(.85, f, f * .82, beast, breath=.35, growl=.75)
        save(f'creatures/growl_{i + 1}', room(low(v, 1200) * env(len(v), .15, .7, 1.3), .7, .2), .6)
    for i, f in enumerate((110, 128)):
        v = throat(.4, f, f * 1.35, beast, breath=.4, growl=.6)
        save(f'creatures/attack_{i + 1}', room(low(v, 1500) * env(len(v), .06, .34, 1.4), .5, .16), .55)
    for i, f in enumerate((185, 165)):
        v = throat(.28, f, f * .7, beast, breath=.4, growl=.35)
        save(f'creatures/hurt_{i + 1}', room(low(v, 1500) * env(len(v), .02, .26, 1.5), .4, .14), .5)
    for i, f in enumerate((120, 104)):
        v = throat(1.0, f, f * .45, beast, breath=.5, growl=.5)
        save(f'creatures/death_{i + 1}', room(low(v, 1200) * env(len(v), .04, .96, 1.3), .9, .25), .6)
    giant = ((260, 1), (560, .6), (1100, .15))
    v = throat(2.0, 60, 47, giant, breath=.4, growl=.85) + .5 * low(noise(2.0), 150)
    save('creatures/boss_roar', room(low(v, 1000) * env(len(v), .3, 1.7, 1.2), 2.0, .35), .85)
    v = throat(2.8, 66, 27, giant, breath=.5, growl=.7) + .5 * low(noise(2.8), 120)
    save('creatures/boss_death', room(low(v, 900) * env(len(v), .12, 2.68, 1.1), 2.2, .35), .85)


# ------------------------------------------------------------------ combat
def build_combat():
    for i, f in enumerate((1320, 1480, 1175)):                               # steel on something solid: a ring over a soft thud
        n = noise(.2)
        x = .35 * band(n, 1200, 4500) * env(len(n), .003, .06, 2.5) + .6 * bell(.2, f, (1, 2.0, 2.99), 20) + np.sin(sweep(.2, 140, 70)) * env(len(n), .003, .13, 2)
        save(f'combat/hit_{i + 1}', room(x, .35, .12), .6)
    for i, f in enumerate((92, 78)):
        n = noise(.26)
        save(f'combat/blow_{i + 1}', room(np.sin(sweep(.26, f, f * .5)) * env(len(n), .005, .24, 2) + .4 * low(n, 500) * env(len(n), .004, .1, 2), .4, .12), .7)
    n = noise(2.4)
    rumble = low(n, 110) * env(len(n), 1.1, 1.3) * 3 + .3 * band(noise(2.4), 300, 1800) * env(len(n), 1.5, .9, 2)
    save('combat/walls', room(rumble, 1.2, .2), .8)
    notes = np.zeros(int(SR * 2.2))                                          # the way is open: a rising major chord on bells and harp
    for k, s in enumerate((0, 4, 7, 12)):
        start = int(k * .09 * SR)
        note = bell(2.2, hz(s, 293.66), (1, 2, 3, 4.2), 1.8) + .6 * pluck(hz(s, 293.66), 2.2, .5)
        notes[start:] += note[:len(notes) - start]
    save('combat/victory', room(notes, 1.8, .3), .7)
    save('combat/chain', room(bell(1.4, 233, (1, 1.5, 2.76, 4.07), 3.2) + .3 * band(noise(1.4), 1500, 4000) * env(int(SR * 1.4), .003, .1, 2), 1.6, .3), .8)


# ------------------------------------------------------------------ world
def build_world():
    n = noise(.9)
    x = low(n, 700) * env(len(n), .004, .13, 2)
    for k in range(8):                                                       # coins
        start = int((.12 + rng.uniform(0, .5)) * SR)
        ping = bell(.25, rng.uniform(1900, 3100), (1, 2.3), 18) * .35
        x[start:start + len(ping)] += ping[:len(x) - start]
    save('world/chest', room(x, .6, .18), .6)
    x = bell(.4, 1568, (1, 2.0, 3.0), 10)
    x[int(.08 * SR):] += bell(.4, 2349, (1, 2.0), 10)[:len(x) - int(.08 * SR)]
    save('world/crystal', room(x, .6, .2), .4)
    x = np.zeros(int(SR * .9))                                               # a charm: three small wooden-bright notes going up
    for k, f in enumerate((784, 988, 1175)):
        note = pluck(f, .6) * .5 + bell(.6, f * 2, (1, 2.0), 9) * .25
        start = int(k * .09 * SR)
        x[start:start + len(note)] += note[:len(x) - start]
    save('world/charm', room(x, .6, .2), .45)
    save('world/bell', room(bell(3.4, 196, (.5, 1, 1.19, 1.5, 2, 2.51, 3.01), .8), 2.5, .3), .65)
    x = np.zeros(int(SR * 1.6))
    for k, s in enumerate((0, 4, 7, 12, 16, 19)):
        note = pluck(hz(s, 293.66), 1.1, .6)
        start = int(k * .08 * SR)
        x[start:start + len(note)] += note[:len(x) - start]
    save('world/gift', room(x, 1.4, .28), .65)

    n = noise(10)
    gust = .6 + .4 * np.sin(2 * np.pi * .1 * t(10)) * np.sin(2 * np.pi * .3 * t(10) + 2)
    save('world/wind', loopable(low(n, 330) * gust + .15 * band(noise(10), 500, 1100) * gust ** 2, 1.2), .5, loop=True)

    # a city: many people talking far away, a hammer on an anvil now and then, all soft
    seconds = 10
    x = np.zeros(int(SR * seconds))
    for k in range(16):
        rate = rng.uniform(2.5, 5)
        syllables = np.clip(np.sin(2 * np.pi * rate * t(seconds) + rng.uniform(0, 6)) + rng.uniform(-.2, .3), 0, 1)
        phrases = np.clip(np.sin(2 * np.pi * rng.uniform(.1, .3) * t(seconds) + rng.uniform(0, 6)) + .3, 0, 1)
        centre = rng.uniform(220, 650)
        x += band(noise(seconds), centre * .8, centre * 1.3) * syllables * phrases
    x = low(x, 1400)
    for at in (1.3, 1.9, 5.2, 5.8, 6.4):
        ping = bell(.6, 880, (1, 2.6, 4.2), 8) * .08 * np.max(np.abs(x))
        start = int(at * SR)
        x[start:start + len(ping)] += ping
    save('world/city', room_loop(loopable(x, 1.0), 1.2, .2), .45, loop=True)

    seconds = 7
    x = low(noise(seconds), 380) * (.65 + .35 * np.sin(2 * np.pi * .6 * t(seconds)))
    crackle = np.zeros(int(SR * seconds))
    for at in rng.uniform(0, seconds - .05, 70):
        click = band(noise(.03), 900, 3500) * env(int(SR * .03), .002, .027, 3) * rng.uniform(.2, .8)
        start = int(at * SR)
        crackle[start:start + len(click)] += click[:len(crackle) - start]
    save('world/fire', loopable(x + .8 * crackle, .7), .5, loop=True)


# ------------------------------------------------------------------ interface
def build_ui():
    def tone(f0, f1, seconds):
        return np.sin(sweep(seconds, f0, f1)) * env(int(SR * seconds), .012, seconds - .012, 2.5)
    save('ui/open', room(tone(392, 587, .18), .4, .15), .35)
    save('ui/close', room(tone(587, 392, .18), .4, .15), .35)
    save('ui/move', room(bell(.12, 880, (1, 2), 30), .25, .1), .25)
    save('ui/page', room(whoosh(.22, 700, 2600, .4), .3, .1), .3)
    x = bell(.9, 783.99, (1, 2, 3.01), 4.5)
    x[int(.11 * SR):] += bell(.9, 1174.7, (1, 2), 4.5)[:len(x) - int(.11 * SR)]
    save('ui/buy', room(x, .9, .22), .5)
    save('ui/deny', room(tone(196, 165, .26) + .4 * tone(392, 330, .26), .3, .1), .35)


# ------------------------------------------------------------------ music
def drum(deep=True):
    """A doli: a hand drum, its deep middle or its dry edge."""
    if deep:
        x = np.sin(sweep(.34, 120, 58)) * env(int(SR * .34), .004, .33, 2.5)
        return x + .2 * low(noise(.34), 600) * env(int(SR * .34), .002, .05, 2)
    return band(noise(.12), 500, 1800) * env(int(SR * .12), .002, .11, 3)

# Georgian music, as far as a script can make it:
#   - three voices, as in Georgian song: a held bass (bani) on the tonic that steps down to the seventh and back,
#     a tune that moves by step with small turns, and a second voice under it;
#   - the voices meet on the chord of first, fourth and fifth (no third in it) and close on the bare fifth;
#   - the cadence is the seventh below rising to the tonic (C to D), not a dominant;
#   - a panduri strummed in a steady pattern, a salamuri (flute) and a doli (drum);
#   - dances in 6/8 and the war dance (Khorumi) in 5/8.
# The tunes are written in steps of the scale (0 = the tonic D), not in semitones, so they cannot fall into
# the gapped five-note scale that made the first version sound East Asian.
SCALE = (0, 2, 3, 5, 7, 9, 10)                      # D E F G A B C: the minor mode with the raised sixth

def step(degree):
    """Semitones above the tonic for a step of the scale (negative and above 6 allowed)."""
    return 12 * (degree // 7) + SCALE[degree % 7]

def choir(freq, seconds, attack=.25):
    """Men singing a vowel: a rich tone with a slow waver through the resonances of 'o' and 'a'."""
    x = t(seconds)
    out = np.zeros_like(x)
    for detune in (-.004, 0, .005):                                         # three men to a voice
        waver = 1 + .007 * np.sin(2 * np.pi * rng.uniform(4.6, 5.6) * x + rng.uniform(0, 6)) * np.clip((x - .2) / .5, 0, 1)
        phase = 2 * np.pi * np.cumsum(freq * (1 + detune) * waver) / SR + rng.uniform(0, 6)
        out += sum(np.sin(k * phase) / k ** 1.25 for k in range(1, 14))
    voice = band(out, 420, 760) * 1.0 + band(out, 850, 1250) * .7 + band(out, 2300, 2900) * .12 + low(out, 300) * .25
    return voice * env(len(x), attack, seconds - attack, .5)

def strum(buffer, at, degrees, gain, down=True, pan=0.0):
    """One stroke across the panduri's three strings."""
    notes = degrees if down else degrees[::-1]
    for i, d in enumerate(notes):
        place(buffer, pluck(hz(step(d)), .9, .55 if down else .4), at + i * .014, gain * (1 - .12 * i), pan)

def music(name, beat, per_bar, bass, tune, strums, drums, peak, lead='choir', echo=1.8, low_voices=True):
    """One bar per entry of 'bass' (the step the held voice sings in that bar).
    'tune': bars of (beat, step of the scale, beats long). 'strums': (beat, down?, gain) in every bar.
    'drums': (beat, deep?, gain) in every bar. 'lead': choir, flute, or both (they take turns every four bars)."""
    bars, bar = len(bass), beat * per_bar
    buffer = np.zeros((int(SR * bars * bar), 2))
    for b in range(bars):
        root = bass[b]
        if low_voices:                                                       # the bani: held through the bar, an octave and more below
            place(buffer, choir(hz(step(root), 73.42), bar * 1.08, min(.5, bar * .2)), b * bar, .11, pan=0)
            place(buffer, choir(hz(step(root + 4), 73.42), bar * 1.08, min(.6, bar * .25)), b * bar, .05, pan=.25)   # its fifth, softly
        chord = (root, root + 3, root + 4)                                   # first, fourth, fifth
        if bass[(b + 1) % bars] == root and b % 4 == 3: chord = (root, root + 4, root + 7)   # the close: bare fifth and octave
        for at, down, gain in strums:
            strum(buffer, b * bar + at * beat + rng.uniform(0, .008), chord, gain * rng.uniform(.85, 1), down, pan=-.25)
        for at, degree, length in tune[b % len(tune)]:
            late = rng.uniform(0, .012)
            loud = rng.uniform(.85, 1.0) * (1.1 if at == 0 else 1)
            by_flute = lead == 'flute' or (lead == 'both' and (b // 4) % 2 == 1)
            start, held = b * bar + at * beat + late, max(.3, length * beat * 1.04)
            if by_flute:
                place(buffer, flute(hz(step(degree), 587.33), held), start, .2 * loud, pan=.15)
                if length >= 1 and length * beat > .3:                       # a short turn before a long note, as shepherds play
                    place(buffer, flute(hz(step(degree + 1), 587.33), .09), start - .08, .09 * loud, pan=.15)
            else:
                place(buffer, choir(hz(step(degree), 293.66), held, min(.12, held * .3)), start, .15 * loud, pan=.2)     # the first voice
                place(buffer, choir(hz(step(degree - 2), 293.66), held, min(.14, held * .3)), start + .015, .10 * loud, pan=-.2)   # the second, a third under it
        for at, deep, gain in drums:
            place(buffer, drum(deep), b * bar + at * beat + rng.uniform(0, .006), gain * rng.uniform(.85, 1))
    save(name, room_loop(buffer, echo, .24), peak, loop=True)

def build_music():
    # The start screen: a slow table song. Voices only, over the held bass; it sinks to the seventh and comes home.
    music('music/menu', 1.05, 4, [0, 0, 0, -1, 0, 0, -1, 0, 0, 0, -1, -1, 0, -1, -1, 0],
          [[(0, 4, 2), (2, 5, 1), (3, 4, 1)], [(0, 3, 1), (1, 4, 1), (2, 2, 2)],
           [(0, 3, 1.5), (1.5, 2, .5), (2, 1, 1), (3, 2, 1)], [(0, 1, 2), (2, 0, 2)],
           [(0, 2, 1), (1, 3, 1), (2, 4, 2)], [(0, 6, 1.5), (1.5, 5, .5), (2, 4, 2)],
           [(0, 3, 1), (1, 2, 1), (2, 1, 1), (3, -1, 1)], [(0, 0, 4)],
           [(0, 4, 1), (1, 5, 1), (2, 6, 1), (3, 7, 1)], [(0, 7, 1.5), (1.5, 6, .5), (2, 5, 1), (3, 4, 1)],
           [(0, 5, 1), (1, 4, 1), (2, 3, 2)], [(0, 3, 1), (1, 2, 1), (2, 1, 2)],
           [(0, 2, 1), (1, 3, 1), (2, 4, 1), (3, 3, 1)], [(0, 2, 1.5), (1.5, 1, .5), (2, 1, 2)],
           [(0, 1, 1), (1, 0, 1), (2, -1, 2)], [(0, 0, 4)]],
          [], [], .45, lead='choir', echo=2.6)

    # The road: a walking song. The panduri keeps the step, the voices and the flute take the tune in turn.
    walk = [(0, True, .2), (1, True, .12), (1.5, False, .1), (2, True, .16), (3, True, .12), (3.5, False, .1)]
    music('music/road', .6, 4, [0, 0, -1, 0, 0, -1, -1, 0, 0, 0, -1, 0, 3, 3, -1, 0],
          [[(0, 0, 1), (1, 1, 1), (2, 2, 1), (3, 3, 1)], [(0, 4, 1.5), (1.5, 3, .5), (2, 2, 2)],
           [(0, 1, 1), (1, 2, 1), (2, 3, 1), (3, 1, 1)], [(0, 2, 1), (1, 1, 1), (2, 0, 2)],
           [(0, 2, 1), (1, 3, 1), (2, 4, 1), (3, 5, 1)], [(0, 6, 1), (1, 5, 1), (2, 4, 2)],
           [(0, 3, .5), (.5, 4, .5), (1, 3, 1), (2, 2, 1), (3, 1, 1)], [(0, 0, 4)],
           [(0, 4, 1), (1, 4, .5), (1.5, 5, .5), (2, 6, 1), (3, 7, 1)], [(0, 7, 1), (1, 6, 1), (2, 5, 1), (3, 4, 1)],
           [(0, 5, 1), (1, 6, .5), (1.5, 5, .5), (2, 4, 1), (3, 3, 1)], [(0, 4, 2), (2, 4, 2)],
           [(0, 7, 1), (1, 6, 1), (2, 5, 1), (3, 6, 1)], [(0, 5, 1), (1, 4, 1), (2, 3, 2)],
           [(0, 2, 1), (1, 3, .5), (1.5, 2, .5), (2, 1, 1), (3, -1, 1)], [(0, 0, 4)]],
          walk, [(0, True, .18), (2, False, .06), (3, False, .05)], .45, lead='both')

    # A city: a round dance in 6/8. Six quick beats to the bar, the drum on one and four.
    six = [(0, True, .2), (1, False, .08), (2, True, .1), (3, True, .17), (4, False, .08), (5, True, .1)]
    music('music/city', .27, 6, [0, 0, -1, 0, 3, 3, -1, 0, 0, 3, -1, 0, 3, -1, -1, 0],
          [[(0, 4, 1), (1, 5, 1), (2, 4, 1), (3, 3, 1), (4, 4, 1), (5, 5, 1)], [(0, 6, 2), (2, 5, 1), (3, 4, 3)],
           [(0, 5, 1), (1, 6, 1), (2, 5, 1), (3, 4, 1), (4, 3, 1), (5, 4, 1)], [(0, 4, 2), (2, 3, 1), (3, 2, 3)],
           [(0, 7, 1), (1, 8, 1), (2, 7, 1), (3, 6, 1), (4, 5, 1), (5, 6, 1)], [(0, 7, 2), (2, 6, 1), (3, 5, 3)],
           [(0, 6, 1), (1, 5, 1), (2, 4, 1), (3, 3, 1), (4, 2, 1), (5, 1, 1)], [(0, 0, 3), (3, 4, 3)],
           [(0, 2, 1), (1, 3, 1), (2, 4, 1), (3, 2, 1), (4, 3, 1), (5, 4, 1)], [(0, 5, 2), (2, 6, 1), (3, 7, 3)],
           [(0, 6, 1), (1, 7, 1), (2, 6, 1), (3, 5, 1), (4, 4, 1), (5, 3, 1)], [(0, 4, 3), (3, 2, 3)],
           [(0, 7, 1), (1, 6, 1), (2, 5, 1), (3, 7, 1), (4, 6, 1), (5, 5, 1)], [(0, 6, 1), (1, 5, 1), (2, 4, 1), (3, 3, 3)],
           [(0, 3, 1), (1, 2, 1), (2, 1, 1), (3, 2, 1), (4, 1, 1), (5, -1, 1)], [(0, 0, 6)]],
          six, [(0, True, .26), (2, False, .08), (3, True, .2), (4, False, .08), (5, False, .09)], .45, lead='both', echo=1.4)

    # A fight: the war dance, in 5/8 (three and two). Low voices call, the drum drives.
    five = [(0, True, .22), (1, False, .08), (2, True, .1), (3, True, .18), (4, False, .1)]
    music('music/battle', .2, 5, [0, 0, 0, -1, 0, 0, -1, 0, 3, 3, 0, 0, -1, -1, 0, 0],
          [[(0, 0, 1), (1, 0, 1), (2, 1, 1), (3, 2, 2)], [(0, 2, 1), (1, 1, 1), (2, 0, 1), (3, 1, 2)],
           [(0, 0, 1), (1, 0, 1), (2, 2, 1), (3, 4, 2)], [(0, 3, 1), (1, 2, 1), (2, 1, 1), (3, -1, 2)],
           [(0, 4, 1), (1, 4, 1), (2, 5, 1), (3, 4, 2)], [(0, 3, 1), (1, 4, 1), (2, 3, 1), (3, 2, 2)],
           [(0, 1, 1), (1, 2, 1), (2, 1, 1), (3, -1, 2)], [(0, 0, 3), (3, 0, 2)],
           [(0, 7, 1), (1, 6, 1), (2, 5, 1), (3, 6, 2)], [(0, 5, 1), (1, 4, 1), (2, 3, 1), (3, 4, 2)],
           [(0, 4, 1), (1, 3, 1), (2, 2, 1), (3, 4, 2)], [(0, 2, 1), (1, 1, 1), (2, 0, 1), (3, 2, 2)],
           [(0, 1, 1), (1, 2, 1), (2, 3, 1), (3, 1, 2)], [(0, 1, 1), (1, 0, 1), (2, -1, 1), (3, -1, 2)],
           [(0, 0, 1), (1, 1, 1), (2, 2, 1), (3, 0, 2)], [(0, 0, 5)]],
          five, [(0, True, .36), (1, False, .1), (2, False, .12), (3, True, .3), (4, False, .12)], .5, lead='choir', echo=1.2)

    # A guardian: slow and heavy. Men's voices an octave down, the deep drum alone.
    music('music/boss', .56, 4, [0, 0, -1, 0, 0, -2, -1, 0],
          [[(0, -7, 2), (2, -6, 1), (3, -5, 1)], [(0, -4, 2), (2, -5, 2)],
           [(0, -6, 1), (1, -5, 1), (2, -6, 1), (3, -8, 1)], [(0, -7, 4)],
           [(0, -3, 2), (2, -4, 1), (3, -5, 1)], [(0, -5, 1.5), (1.5, -6, .5), (2, -5, 2)],
           [(0, -6, 1), (1, -7, 1), (2, -6, 1), (3, -8, 1)], [(0, -7, 4)]],
          [(0, True, .14), (2, True, .1)],
          [(0, True, .4), (.75, True, .18), (2, True, .4), (3, True, .26), (3.5, True, .18)], .5, lead='choir', echo=2.4)


if __name__ == '__main__':
    build_nyxara(); build_creatures(); build_combat(); build_world(); build_ui(); build_music()
    count = sum(len([f for f in files if f.endswith('.wav')]) for _, _, files in os.walk(OUT))
    print(f'{count} sound files in {os.path.normpath(OUT)}')
