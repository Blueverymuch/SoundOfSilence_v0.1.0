# SoundOfSilence

> **Initial Release (v0.1.0)** — A playful weekend prototype built just for fun.

## What is this?
**SoundOfSilence** listens to the sounds people make while working—typing, sighing, muttering, or breathing—and tracks periods of silence in real time.

Instead of just being a productivity timer, it was created as a lighthearted experiment to **awaken the inner instinct to take a break** whenever things get suspiciously quiet.

---

## Key Features
- **Real-Time Audio & Silence Detection:** Monitors live microphone input and visualizes activity across a 10-minute timeline.
- **Cruelty Level Switch:** Choose how unforgiving the app gets when you stop making work noises:
    - **Gentle:** Gives you 60 seconds to breathe and space out.
    - **Normal:** Checks in after 25 seconds of silence.
    - **Brutal:** Calls you out in just 10 seconds.
- **Audio / UI Thread Separation:** Built with JUCE C++ using atomic state sharing and custom `Graphics` rendering.

## Tech Stack
- **Framework:** JUCE (C++)
- **Platform:** Android (built via Android Studio)

## Roadmap
- Custom SVG-based UI skinning & `LookAndFeel`
- Multi-language support

---

*Built just for fun — sometimes silence is the loudest sign that you need a rest.*