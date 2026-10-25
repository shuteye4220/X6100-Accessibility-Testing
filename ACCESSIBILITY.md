# Accessibility fork

Experimental branch adding spoken feedback (screen-reader style) to the X6100 GUI, for use with the radio's own text-to-speech. Built and tested by a blind ham using JAWS conventions as a guide, not a sighted assumption of what "should" be spoken.

## How to try it

Flash the image from [Releases](https://github.com/Jimr9/X6100-Accessibility-Testing/releases) the same way as upstream (see main README). Turn Voice on in Settings.

## One thing worth knowing before you start

A few actions - starting a **recording**, **sending** a saved message, **beaconing**, **playing back** a recording, and **Bluetooth headset PTT** - use a two-step "arm" pattern instead of doing the thing on the first press:

1. First press: announces what it's about to do ("Ready to record, press again to start") and waits.
2. Second press: does it, silently.

This is deliberate, not a bug - speaking at the exact instant those actions start was found to interfere with the radio's own audio hardware. If you press once and nothing seems to happen yet, that's normal - press again.

Menus and settings are otherwise navigated with the same knob used for everything else; turning it while a list has only one entry (or none) still reads that entry.

## Readouts on demand

These can be put on any long-press key (GEN, APP, KEY, MSG, DFN, DFL) or hand-mic F1/F2 key, in Settings > Interface. They speak even with Voice mode off, since they were asked for.

- **Speak status**: VFO, band, frequency, mode, filter width, power, and split or frequency lock when on.
- **Speak meters**: S-meter; in CW with CW peak on, how far the received tone is from your key tone ("50 hertz above your tone"); and the power, SWR and ALC of your last transmission.
- **Speak keys**: the current page's name and what each function key does - handy in windows such as Settings or FT8.
- **Read decoded**: the last few hundred characters received by the RTTY or CW decoder.
- **Speak time**: the radio's clock and date.
- **Speak here**: whatever matters where you are, so one key does for most things. In the FT8 window, the selected message; in the GPS window, the summary; in any other window (Settings, Wi-Fi...), the item you are on and its value; with RTTY or the CW decoder showing, the last text received; anywhere else, the status.

## Also spoken

- **Frequencies** keep their leading zeros, so 14.005 MHz is "14. 0 0 5", not "14. 5".
- **Bands**: the band keys say the band and frequency ("40 meters CW, 7. 0 30"); tuning into another band, or out of band, says so.
- **Key pages** say what F2-F5 do after the page name (Settings > Voice > Read function keys).
- **Pop-up messages** that had no announcement of their own ("Wrong WiFi password", "Nothing to load for memory 3", ...), except while transmitting (Settings > Voice > Speak pop-up messages).
- **Decoded text**: RTTY is read aloud as it comes in, in phrases, with callsigns and abbreviations spelled out ("C Q D E V E 3 X Y Z K"). Settings > Voice > Read decoded text: Off, Text modes (not the CW decoder, which you hear yourself), or All.
- **GPS window**: a summary on opening (fix, grid, satellites, UTC time), the grid in phonetics; Summary, Grid, Position, Time and Sats keys; the multi-function knob reads one row at a time and its press repeats the summary; a gained or lost fix, or a new grid, is announced.

## With a keyboard (USB or Bluetooth)

Screen-reader keys, NVDA/JAWS style, while Voice mode is on. The **voice key** is **Caps Lock** or **Insert**: hold it and press one of the keys below. These combinations never reach the radio. Double-tap Caps Lock for a real caps lock. Tap **Control** on its own to stop speech.

Reading received text (RTTY, and the text modes to come) with a review cursor - it starts on the newest line:

- **Voice key + Left / Right**: previous / next character
- **Voice key + Control + Left / Right**: previous / next word
- **Voice key + Up / Down**: previous / next line
- **Voice key + Home / End**: first / newest line
- **Voice key + C / W / L**: current character / word / line
- **Voice key + Enter**: read from the cursor to the end
- **Voice key + R**: the last text received

Readouts and settings:

- **Voice key + Space**: speak here (as above)
- **Voice key + S / M / T / B / K**: status, meters, time, battery, function keys
- **Voice key + E**: typing echo - off, characters, words, characters and words
- **Voice key + A**: reading received text aloud - off, text modes, all including CW
- **Voice key + H**: these keys

Typing into a text field (callsign, QTH, CW messages, Wi-Fi password, and the text modes' compose boxes to come) is echoed as set in Settings > Voice > Typing echo (characters by default). The arrows, Home, End, Backspace and Delete say the character you land on or delete.

## Making a new app speak

Mainline is adding apps (WSPR, JS8, PSK31, Olivia, WeFax, SSTV). Pieces that cover them with little or no code:

- Pop-ups (`msg_*_text_fmt`) are spoken already, unless the same action speaks.
- Received text: anything shown in the decoder panel (`panel_add_text`) is read aloud already, and can be reviewed from a keyboard. An app with its own text window calls `voice_decoder_add(text, false)` from the UI thread.
- A compose box that is an `lv_textarea` gets typing echo, and the arrows and deletion spoken, with no code.
- Function keys are read out by "Speak keys" and on page load, from their labels.
- `voice_say_requested()` speaks a readout asked for by a key, even with Voice mode off.
- `voice_text.h` makes values speakable: frequencies, bands, labels, S-meter, grid squares, coordinates, decoded text.
- New key actions for voice go in the 200+ range of `press_action_t`, out of the way of mainline's.

## Known limitations

- **SWR Scan and ATU tuning**: starting/stopping an SWR scan, or hearing the result after an auto-tune, can occasionally wedge the speech engine entirely (radio keeps working, RX audio is fine, but no more speech until a soft reset - hold LOCK; SWR Scan's version has needed a full power cycle, ATU's has recovered with just the LOCK-hold). Confirmed as of 2026-08-26 this is a BASE firmware issue, not a bug in this fork or the GUI app: per gdyuldin, BASE mixes several audio sources (handset mic, internal mic, USB soundcard, and the Linux/GUI side) into one output, and after ATU (and possibly SWR Scan) runs, BASE stops feeding the Linux-side input into that mix - so nothing the app plays can be heard, even though PulseAudio/ALSA both report a fully healthy stream the whole time. No GUI-side fix eliminates the wedge itself; gdyuldin has a patched BASE firmware that aims to fix this, at least for the ATU case; the latest public BASE patch release (r13, 2026-09-30) does not mention it, so treat it as still unfixed. Both the ATU key and the SWR Scan Run button use a press-once-to-arm, press-again-to-act pattern (results, when there are any to report, are read out on the next idle press after stopping) - this doesn't fix the underlying wedge, but keeps the hazardous hardware toggle and the announcement further apart, which live testing suggests is somewhat more reliable in practice. Avoid relying on SWR Scan or ATU auto-tune with voice on for now.
- **Bluetooth window** (APP menu, new upstream in this build): opens with a spoken summary; the Status button (page 3) repeats it. Turning the knob reads each device with its position ("2 of 5"). Connections, scan results, pairing codes (digit by digit) and failures are spoken as they happen. Nothing is spoken while the headset PTT is transmitting - with listening off, speech would go out on the air - so "PTT off, receiving" is the first thing you hear after an over. When Bluetooth listening is on, speech goes to the Bluetooth headphones instead of the radio's speaker.
- **FT8 / band-switching deadlock**: RESOLVED as of upstream v0.34.2 (gdyuldin's parameter/Subject redesign, see [issue #261](https://github.com/gdyuldin/x6100_gui/issues/261)). No longer an issue on current builds.

Builds up to the upstream v0.34.2 merge were live-tested on real hardware. The merge onto upstream 1.0.x (new Settings layout, Bluetooth, FT8 country/distance display) is not live-tested yet. Feedback welcome.
