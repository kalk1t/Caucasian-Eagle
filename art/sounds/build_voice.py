"""Builds the spoken lines of Caucasian Eagle with the voices that come with Windows.

    python art/sounds/build_voice.py            (from the repository root)

The words are read out of game/src/Story.cpp, so what is heard is what the journal shows.
The storyteller is the Windows voice "Microsoft David", Nyxara is "Microsoft Zira"
(System.Speech, on this computer, no network). They are placeholders for a recorded
storyteller and actress: record a line, save it under the same name in
game/assets/sounds/voice (WAV, PCM 16-bit), and the game plays the recording instead.

    voice/legend.wav            the three passages of the legend (R in the journal)
    voice/chapter_01..12.wav    told when she enters a region
    voice/city_1..7.wav         told the first time she enters a city
    voice/memory_01..12.wav     told when a chest gives a memory of the cave
    voice/end_1..4.wav          the four parts of the end (the fourth: all three links)
    nyxara/say_fight_1..4.wav   Nyxara, when the walls rise
    nyxara/say_win_1..3.wav     Nyxara, when a fight is won
    nyxara/say_evolve.wav       Nyxara, when she evolves
"""
import os
import re
import subprocess
import tempfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUT = os.path.join(ROOT, 'game', 'assets', 'sounds')

NYXARA = {
    'nyxara/say_fight_1': 'Come, then.',
    'nyxara/say_fight_2': 'You stand between me and the mountain.',
    'nyxara/say_fight_3': 'I have no time for you.',
    'nyxara/say_fight_4': 'Seven years I trained for this.',
    'nyxara/say_win_1': 'The road is open.',
    'nyxara/say_win_2': 'Wait for me, Amirani.',
    'nyxara/say_win_3': 'Onward.',
    'nyxara/say_evolve': 'I feel the sky in my wings.',
}


def strings(block):
    """The string literals of a C++ initialiser, adjacent literals joined."""
    out, last_end = [], None
    for m in re.finditer(r'L"((?:[^"\\]|\\.)*)"', block):
        text = m.group(1).replace('\\"', '"').replace('\\\\', '\\')
        if last_end is not None and block[last_end:m.start()].strip() == '':
            out[-1] += text
        else:
            out.append(text)
        last_end = m.end()
    return out


def block(source, name):
    start = source.index(name)
    start = source.index('= {', start)
    return source[start:source.index('};', start)]


def lines():
    source = open(os.path.join(ROOT, 'game', 'src', 'Story.cpp'), encoding='utf-8').read()
    chapters, cities = strings(block(source, 'Chapters[12]')), strings(block(source, 'Cities[CityCount]'))
    memories, legend, end = strings(block(source, 'Memories[12]')), strings(block(source, 'LegendText[]')), strings(block(source, 'EndText[4]'))
    assert (len(chapters), len(cities), len(memories), len(legend), len(end)) == (24, 42, 12, 6, 8), 'Story.cpp changed shape'
    said = {'voice/legend': ' '.join(legend[1::2])}
    for i in range(12):
        said[f'voice/chapter_{i + 1:02d}'] = f'{chapters[2 * i]}. {chapters[2 * i + 1]}'
        said[f'voice/memory_{i + 1:02d}'] = memories[i]
    for i in range(7):
        name, land, known, text, gift, gift_text = cities[6 * i:6 * i + 6]
        said[f'voice/city_{i + 1}'] = f'{name}, in {land}. {text}'
    for i in range(4):
        said[f'voice/end_{i + 1}'] = end[2 * i + 1]
    return said


SCRIPT = r'''
Add-Type -AssemblyName System.Speech
$format = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(22050, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)
foreach ($line in [System.IO.File]::ReadAllLines($args[0], [System.Text.Encoding]::UTF8)) {
    $voice, $rate, $path, $text = $line.Split("`t")
    $speaker = New-Object System.Speech.Synthesis.SpeechSynthesizer
    try { $speaker.SelectVoice($voice) } catch { }
    $speaker.Rate = [int]$rate
    $speaker.SetOutputToWaveFile($path, $format)
    $speaker.Speak($text)
    $speaker.Dispose()
}
'''

if __name__ == '__main__':
    jobs = [('Microsoft David Desktop', -2, name, text) for name, text in lines().items()]
    jobs += [('Microsoft Zira Desktop', 0, name, text) for name, text in NYXARA.items()]
    with tempfile.TemporaryDirectory() as folder:
        listing, script = os.path.join(folder, 'lines.txt'), os.path.join(folder, 'speak.ps1')
        with open(listing, 'w', encoding='utf-8') as f:
            for voice, rate, name, text in jobs:
                path = os.path.join(OUT, name.replace('/', os.sep) + '.wav')
                os.makedirs(os.path.dirname(path), exist_ok=True)
                f.write(f'{voice}\t{rate}\t{path}\t{text}\n')
        open(script, 'w', encoding='utf-8').write(SCRIPT)
        subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', script, listing], check=True)
    print(f'{len(jobs)} spoken lines in {OUT}')
