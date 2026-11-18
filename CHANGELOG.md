# Changelog

## 1.2.0

### Interface
- **New look:** every window, button, tab, slider and dialog is drawn from the *Dark Ages UI* pixel-art pack: gold knotwork frames, a gilded plaque for the sound name, scroll-end buttons, jewel slider handles and a gem-LED ring around the macro dial.
- **New type:** headings use *Abaddon Bold*, secondary text *Abaddon Light*, lists and values *BoldPixels*, and the dial value *Fibberish*. Preset names now show in their own upper and lower case.
- **GILDED skin:** shows the art in its own colours and is the default for new instances. With AUTO, medieval sounds open in GILDED.
- **Other skins:** NES, DMG, NEON, CODEX, ARCADE and PAPER recolour the same art. Text colours adapt to each surface so every skin stays readable.
- **Projects:** a project keeps the skin it was saved with.
- **Layout:** 12 list rows, tabs sized to their labels, and slider clicks now land where the handle goes.

### Building from source
- **UI art:** the art packs are not in git, because the *Dark Ages UI* licence forbids redistributing them. Download them into `assets/` (see README, "UI art"); the first build bakes them into `src/gui/UiAssets.inc` with `scripts/make_ui_assets.py`.

## 1.1.0

### Sound
- **Presets:** 25 new factory presets, 72 in all. They are added after the old ones, so existing projects keep their sounds.
  - **80s chip:** Octave Bounce Bass, Echo Canyon Lead, Minor Key Arp, Square Brass Stab, Chip Harmonica.
  - **90s chip:** Wave Channel Lead, Tracker Rock Organ, Battle Brass Hit, Handheld Glow Pad, Pocket Kalimba.
  - **Synth 80s:** Ensemble Strings 79, Supersaw Anthem, Neon Pluck, Midnight Sub Bass, Machine Choir.
  - **Medieval:** Shepherd Pan Pipes, Bright Psaltery, Capped Crumhorn, Abbey Choir AH, Rebec Fiddle.
  - **SFX + kit:** Stadium Drum Kit, Handheld Crunch Kit, Warp Portal, Red Alert Klaxon, Bubble Pop.
- **Touch:** new `TOUCH` (Off / Light / Normal / Heavy) and `VEL SENS` settings. Changing sounds keeps them, and Normal at 65 % matches 1.0.0. Clicking lower on an on-screen key plays harder.

### Sound library
- **Save / Save As:** any edit, effects included, marks the sound `EDITED`. `SAVE AS` stores it under a new name, and `SAVE` overwrites a sound you saved before.
- **Unsaved changes:** switching away from an edited sound asks first: Save, Save As, Don't Save or Cancel.
- **New tabs:** `MY SOUNDS`, `FAVORITES` and `BANKS`. Click a star to add a favorite. Make as many named banks as you like and add sounds with `+ BANK`.
- **Shared file:** the library is stored in `%APPDATA%\PixelLABS\PixelLABS_Library.txt` and shared by every instance.
- **Projects:** a project remembers which sound is loaded and whether it was edited. It still opens correctly on a computer without your library.

### Host integration
- **Automation:** 37 automatable parameters; Velocity Sens and Touch Curve are new.
- **Older projects:** projects saved with 1.0.0 and pre-release builds load unchanged and get the default touch.

### Fixes
- **Sustain pedal:** a freshly added instance no longer starts with the sustain pedal held.
- **Hover help:** the help line now also explains the keyboard and the bottom strip.

## 1.0.0

First public release.

### Sound
- **Presets:** 47 factory presets across five eras: 80s chip, 90s handheld/16-bit chip, 80s synth, medieval, and game SFX + drum kit.
- **Waveforms:** 20 oscillator models: pulse, NES-style triangle, GB-style wavetable, LFSR noise, PWM, chip saw, 2-operator FM (leads, keys, slap bass, bells), soft strings, pipe organ, flute, reed, plucked lute/harp, bowed viol, choir and a 12-piece drum kit.
- **Harmony:** single, fifth, octave, organum, major/minor/octave arpeggios, and a "coin jump" mode.
- **Effects:** 12 effects on the macro dial: reverb (room, hall, church, cave), delay (ping-pong, tape, 16-bit, slapback, canyon), chorus, phaser, tremolo, tape wow/flutter, drive, pixel crush, handheld speaker and stereo width.
- **Voice controls:** full ADSR, filter envelope, pitch sweep, arp rate, glide, noise layer, detune and vibrato.
- **Demo songs:** four built-in songs, chosen by era. PLAY DEMO picks the one that matches the preset's era.

### Interface
- **Layout:** pixel-art editor with era tabs, a filterable preset browser, a 12-way FX macro dial, list steppers and 16 sliders. Double-clicking a slider resets it.
- **Keyboard:** the 61-key keyboard shows note letters (C D E F G A B), lights up the keys being played, and names the chord. The A–K keys on a computer keyboard play notes, and Z/X change octave.
- **Skins:** six skins (NES, DMG, NEON, CODEX, ARCADE, PAPER) plus AUTO, which follows the preset's era.
- **Zoom:** 100 / 125 / 150 / 200 %.

### Host integration
- **Automation:** 35 automatable parameters, host tempo sync and a program-change preset list.
- **MIDI CC:** built-in mapping, including the M-Audio Oxygen 61 faders and knobs.
- **Project state:** the sustain pedal is never restored as held when a project loads.

### Distribution
- **Installer:** `PixelLABS-Setup-1.0.0.exe` installs per user or for all users, registers an uninstall entry, and supports silent mode.
- **Portable zip:** a portable zip of the plugin is also provided.
