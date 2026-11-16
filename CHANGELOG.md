# Changelog

## 2.0.0

### Sound
- **Presets:** 47 factory presets (was 8) across five eras: 80s chip, 90s handheld/16-bit chip, 80s synth, medieval, and game SFX + drum kit. The original eight keep their slots, so projects saved with 1.x recall the same sounds.
- **Waveforms:** 20 (was 6): FM keys, bass and bells, PWM, LFSR noise, a GB-style wavetable, plucked lute and harp, pipe organ, flute, reed, bowed viol, choir and a 12-piece drum kit.
- **Harmony:** organum, major/minor/octave arpeggios and a "coin jump" mode.
- **Effects:** phaser, drive, tape wow/flutter, tremolo, handheld speaker and stereo width are new. Reverb has room, hall, church and cave types; delay has ping-pong, tape, 16-bit, slapback and canyon types.
- **Voice controls:** a full ADSR, filter envelope, pitch sweep, arp rate, glide, noise layer and detune.
- **Demo songs:** four built-in original or public-domain songs. PLAY DEMO picks the one that matches the preset's era.

### Interface
- **Layout:** era tabs, a filterable preset browser, a 12-way FX macro dial, list steppers and 16 sliders. Double-clicking a slider resets it.
- **Keyboard:** the 61-key keyboard shows note letters (C D E F G A B), lights up the keys being played, and names the chord. The A–K keys on a computer keyboard play notes, and Z/X change octave.
- **Skins:** six skins (NES, DMG, NEON, CODEX, ARCADE, PAPER) plus AUTO, which follows the preset's era.
- **Zoom:** 100 / 125 / 150 / 200 %.

### Host integration
- **Automation:** 35 automatable parameters. IDs 0–15 are unchanged, so existing automation still works.
- **Tempo:** follows the host's tempo.
- **MIDI CC:** more CC mappings (CC 5, 72, 73, 75, 92, 95).
- **Project state:** saves a new state format (PXL2) and migrates 1.x state automatically. A sustain pedal is never restored as held.

### Distribution
- **Installer:** `PixelLABS-Setup.exe` installs per user or for all users, registers an uninstall entry, and supports silent mode.
- **Portable zip:** a portable zip of the plugin is also provided.
