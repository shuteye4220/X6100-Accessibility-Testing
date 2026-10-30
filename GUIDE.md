# Using the X6100 by ear

A guide for blind operators to the Xiegu X6100 with the OpenX6100 firmware and this fork's speech. It tells you what you press and what the radio says, not what is on the screen. Quoted text is what you hear.

Start with "Finding your way around the radio" to learn where the keys are. The hand microphone's key layout isn't covered yet, so get a sighted or tactile tour of it.

## Finding your way around the radio

This layout is based on Ian Spencer's audio walkaround of the X6100 (spencerweb.net). Hold the radio with the screen facing you. Left and right are as you hold it.

### Front panel

The display fills the middle, from near the top to well down the panel.

- **Below the display:** five keys in a row, **F1** to **F5** from left to right. Their job depends on the page the radio has loaded.
- **Right of the display:** the loudspeaker at the top, and the large **main tuning knob** below it. Just to the left of the knob, above it, is the small **LOCK** key.
- **Left of the display:** from the top-left corner, move your finger down and to the right at 45 degrees to reach the **VOL** knob. Going straight down from there you find:
  1. A small light on the left, which is green when receiving and red when transmitting. To its right is the elongated **POWER** key: hold it about a second to turn the radio on, and again to turn it off.
  2. A pair of keys: **GEN** on the left, **APP** on the right.
  3. A pair of keys: **KEY** on the left, **MSG** on the right.
  4. A pair of keys: **DFN** on the left, **DFL** on the right.
  5. Below and between DFN and DFL, the **MFK** knob.

### Top edge

From the left end, near the front, there is a square **PTT** key for handheld use, then a small screw head. Further right are two long keys side by side: **band down**, with an arrow to the left, and **band up**. In front of them, toward you, are three small keys: **AM**, **CW**, **SSB**.

Past a second screw head, near the right end, is a block of six keys in two rows of three:

- **Back row, left to right:** **A/B**, **PRE**, **ATU**.
- **Front row, left to right:** **V/M**, **AGC**, **FST**.

### Left end

- **Top:** the BNC antenna socket.
- **Below it:** I/Q out, for SDR software on a computer.
- **Below that, slightly left:** the DC power input, 9 to 15 volts and never more than 15. It powers the radio and charges its battery.
- **Next to it:** a fold-out stand. There is another at the right end, so the radio can sit tilted on a desk.

### Right end

- **Nearest the front, from the top:**
  1. The **microSD card slot**, just back from the front panel. Your firmware card goes here.
  2. A USB-C device port, for a computer.
  3. A USB-C host port, for a keyboard or mouse.

  Neither USB-C port charges the radio or powers it. Charging is only through the DC input on the left end.
- **Further back, from the top:**
  1. A square RJ45 socket for the hand microphone, level with the card slot.
  2. 3.5 mm ACC (accessory interface).
  3. 3.5 mm key jack, for a straight key or paddle.
  4. 3.5 mm stereo speaker or headphone jack.
- **Furthest back:** the second fold-out stand.

Some keys work differently in this firmware than in the walkaround, which describes the original firmware:
- A press of VOL switches what the knob adjusts, rather than stepping through AF, RF and squelch.
- PRE turns the preamplifier on and off with a press, and the attenuator with a hold.

See Front panel for what each key does here.

## Getting started

1. **Get the image.** Go to the Releases page of shuteye4220/x6100-accessibility-testing on GitHub. Builds named "Accessibility additions" use the RHVoice voice. Builds named "eSpeak" use eSpeak, which is faster and more robotic. Download `sdcard.img.zip` from the newest build and unzip it.
2. **Write it to a microSD card.** On Windows, use Rufus or balenaEtcher. On Linux, run `sudo dd if=sdcard.img of=/dev/sdX bs=4M conv=fsync status=progress`, with your card's device in place of sdX. Everything already on the card is erased.
3. **Start the radio from the card.** With the radio off, put the card in the microSD slot, at the right end nearest the front. Then hold POWER for about a second. The radio is silent while it starts, and the first start takes longer. After a minute or two you hear "Radio ready", then the frequency and mode. If it stays silent for more than three minutes, write the card again.
4. **Turn the speech off, or back on.** Hold **DFN**. Each hold moves to the next voice mode and says it:
   - "Voice mode: always"
   - "Voice mode: only with the screen off"
   - "Voice mode: off"

   With "only with the screen off", press **POWER** briefly to turn the screen off ("Display off"), and the voice speaks. Hold **POWER** to turn the radio off ("Power off").
5. **Set up your readout keys.** The readouts (see Readouts) speak only when you ask, so put them on long presses:
   1. Press **APP** until you hear "Application page 2".
   2. Press **F5**. You hear "Settings window".
   3. Press **F2**. You hear "Interface".
   4. Turn the **MFK** to "KEY long press".
   5. Press the MFK to open the list, turn it to "Speak here", and press it again to choose.

   Do the same for "GEN long press" with "Speak status". You can also use "HMic F2 press" with "Speak meters". Press the **VOL** knob to close the window.
6. **Make the voice faster or slower.**
   1. Press **APP** until you hear "Application page 2".
   2. Press **F5** ("Settings window"), then **F3** ("Voice").
   3. Turn the **MFK** until you hear the rate. It is the second item, after "Voice mode":
      - With eSpeak, "Voice rate, words per minute": 80 to 450, default 175.
      - With RHVoice, "Voice rate": 50 to 150, default 100.
   4. Press the MFK. You hear the rate, "editing, steps of 1".
   5. Turning now changes the rate, and it takes effect at once. For bigger changes, press again for "Steps of 10", and again for "Steps of 100". A fourth press goes back to steps of 1.
   6. Hold the MFK until you hear the rate without "editing". That ends the change.

   Pitch and volume are the next two items, and work the same way. Press **VOL** to close Settings.
7. **Learn the keys anywhere.** On any page, the readout "Speak keys" tells you what F1 to F5 do.

### Getting back to the main screen

The main screen is the radio with no window open. The VOL and MFK knobs then change their settings, and F1 to F5 hold a page of settings.

- **Press VOL** to close the window you are in. You hear the name of the page that was there before, such as "Application page 2".
- **Press GEN** to close whatever is open, from anywhere. You hear a Volume or MFK page, which means you are on the main screen.
- **"Selection mode" when you press VOL** means you were already on the main screen. Press VOL again to hear "Edit mode", where turning VOL changes the volume.
- **Speak status** (see Readouts) tells you the frequency and mode once you are there.

Settings are saved on the card. If speech stops entirely and the radio still receives, hold **LOCK** for a soft reset (see Known limits).

## How it talks

- **Interruptions.** New speech cuts off old speech, so turning a knob quickly gives you only the last value.
- **Frequencies** are said in megahertz with the zeros after the point kept: 14.074 is "14. 0 74", 14.005 is "14. 0 0 5".
- **Callsigns and abbreviations** are spelled out: "C Q D E V E 3 X Y Z".
- **Pop-up messages** are read aloud ("Wrong WiFi password"). Turn this off in Settings, Voice, "Speak pop-up messages".
- **Silence on transmit.** Nothing is spoken while a digital mode (FT8, JS8, SSTV, Winlink and others), a voice message or a Bluetooth headset is transmitting, because the speech would go out on the air. What was missed is said afterwards, or on request.
- **Two presses for some actions.** Recording, playing back, sending a message, beaconing, SWR scan and auto-tune take two presses. The first press tells you what will happen ("Ready to record, press again to start"). The second press does it silently.
- **Unused keys.** An empty function key says "Not used".

## Front panel

### Main tuning knob

Turning it changes the frequency, and the frequency is said when you stop. Going into another band, or out of band, is said too.

### Mode keys

- **SSB** switches between "LSB modulation" and "USB modulation". Holding it gives the digital variants, "USB digital modulation" or "LSB digital modulation".
- **CW** switches between "CW modulation" and "CWR modulation".
- **AM** switches between "A M modulation" and "N F M modulation".

### Band keys

Band up and band down say the band and the frequency: "40 meters CW, 7. 0 30".

### Other keys

- **PRE.** A press gives "Preamplifier on" or "Preamplifier off". Hold it for "Attenuator on" or "Attenuator off".
- **AGC.** Each press gives the next setting: "Auto gain slow mode", "fast mode", "auto mode", then "Auto gain off". Hold it for "Split on" or "Split off".
- **FST.** A press gives "Frequency step 100 herz" for the next step up. Hold it for the next step down.
- **ATU.** A press gives "Auto tuner on" or "Auto tuner off". Hold it to tune, and press once more afterwards to hear "Auto tuner tuning complete, S W R 1.2 to 1". See Known limits.
- **A/B.** A press gives "V F O A" or "V F O B". Hold it for "V F O cloned from A to B".
- **LOCK.** A press gives "Frequency locked" or "Frequency unlocked". Holding it is the soft reset.
- **POWER.** A press gives "Display off" or "Display on". Hold it for "Power off".
- **V/M.** A press gives "VM parameters" and loads the memory keys: F2 to F5 are memories 1 to 4 on page 1, and 5 to 8 on page 2. A press of a memory key gives "Memory 3 loaded". Holding one gives "Memory 3 stored".

### Group keys and F1 to F5

Each group key loads a set of pages onto F1 to F5. In most sets, F1 is the page key: a press goes to the next page and a hold to the previous one, and the page says its name. With Settings, Voice, "Read function keys" on, the page name is followed by what F2 to F5 do. A press of F2 to F5 says the setting and its value.

- **GEN:**
  - "Volume page 1": volume, squelch, RF gain, power.
  - "Volume page 2": mic selector, hand-mic gain, internal-mic gain, monitor level.
  - "MFK page 1": RIT, XIT, spectrum zoom, antenna.
  - "MFK page 2": AGC hang, knee, slope, compressor.
  - "MFK page 3": IF shift.
  - "MFK page 4": VOX on, gain, anti-gain, delay.

  Holding a setting key moves it between the knobs: "Added to volume encoder", "Moved to MFK encoder", "Removed from MFK encoder".
- **APP:**
  - "Application page 1": RTTY, FT8, SWR Scan, GPS.
  - "Application page 2": Recorder, QTH, Callsign, Settings.
  - "Application page 3": Wi-Fi, Bluetooth, JS8, WeFax.
  - "Application page 4": SSTV, Channels, Winlink.
- **KEY:**
  - "Key page 1": key speed, volume, train, tone.
  - "Key page 2": key mode, iambic mode, QSK time, ratio.
  - "CW page 1" and "CW page 2": the CW decoder, its tuner and SNR, CW peak, and CW zap.
- **MSG** opens the message window for the mode you are in: CW messages in CW, voice messages in SSB, AM or FM. "Not used in this mode" in the others.
- **DFN:**
  - "D N F page": the notch filter.
  - "N B page": the noise blanker.
  - "N R page": noise reduction.
- **DFL** ("DFL parameters"): the filter's low edge, high edge and bandwidth.

These are the default long presses. You can change them in Settings, Interface.

- **GEN:** screenshot.
- **APP:** the Recorder window.
- **KEY:** nothing.
- **MSG:** "Audio recorder on" or "Audio recorder off".
- **DFN:** voice mode.
- **DFL:** battery info.

### VOL knob

Turning it changes the volume, or whichever setting is on the knob, and says the value ("Audio level 20"). A press gives "Selection mode", and turning then picks which setting the knob changes. Press again for "Edit mode". In most windows, a press of VOL closes the window.

### MFK knob

On the main screen it works like VOL, for the settings on the MFK pages (RIT, zoom and others): a press switches between "Selection mode" and "Edit mode". In windows (Settings, Wi-Fi, lists) it moves:

1. Turn to move between items. Each one says its name and value.
2. Press to change an item. How it works depends on the kind of item:
   - **On or off:** the press switches it, and the new state is said.
   - **A list:** the press opens it ("editing"). Turn to hear the choices, and press to choose one.
   - **A number:** the press starts editing ("editing, steps of 1"), and turning changes the value. Each further press makes the step ten times bigger ("Steps of 10", "Steps of 100", then back to 1). Hold the MFK to finish. You hear the value again without "editing".
   - **A slider:** the press starts editing. Turn to change it, and press again to finish.

   Changes take effect as you make them; there is nothing to save.

### On-screen keyboard

For callsign, QTH, names and passwords. Turn the MFK to move across the keys. Each key is said ("A", "space", "backspace", "enter", "numbers", "close keyboard"). Press to type it. "Cancelled" when you leave without saving. A USB or Bluetooth keyboard is easier (see Keyboard).

## Hand microphone

The key names below are the firmware's. The names printed on your microphone may differ slightly.

- **1 to 9.** A press gives "Memory 4 loaded". Hold one for "Memory 4 stored".
- **UP and DOWN.** A press steps the frequency. With channels saved, a press goes to the next or previous channel instead ("Channel 40m net"). Hold for band up or down.
- **V/M** opens or closes the Channels window, even over an app.
- **GENE.** Broadcast info for the frequency you are on, for shortwave listening: "Radio Example, 2 of 3. 9580 kilohertz, 14 00 to 15 00 U T C. Language English, target Asia, daily". Press again for the next match, or "No station scheduled".
- **NW** saves the frequency as a channel: "Channel saved, Radio Example".
- **SPCH.** "Frequency locked" or "Frequency unlocked".
- **TUNER.** A press switches the tuner on or off. Hold it to tune.
- **XFC** switches VFO.
- **F-INP.** "Enter frequency". Type the digits on the microphone keypad (each one is said), then press F-INP again. You hear "Frequency has been set" or "Incorrect frequency".
- **F1.** A press is frequency step up, and a hold is step down.
- **F2** does nothing by default. Both F1 and F2 can be set in Settings, Interface.

## Readouts

Readouts speak even with the voice mode off, because you asked for them. Put them on long presses or hand-mic keys in Settings, Interface, or use a keyboard.

- **Speak status:** VFO, band, frequency, mode, filter, power, and split or lock if on. Then what the screen's indicator row shows: the AGC setting, and whichever of preamp, attenuator, noise reduction, noise blanker, notch, tuner, RIT or XIT (with its offset) and Wi-Fi are on. Last, the other VFO's frequency.
- **Speak meters:** the S-meter. In CW with CW peak on, how far the signal is from your tone ("50 hertz above your tone"). Also power, SWR and ALC of your last transmission.
- **Speak keys:** the page name and what F1 to F5 do.
- **Read decoded:** the last text received (RTTY, CW decoder, JS8).
- **Speak time:** clock and date.
- **Speak memory:** free memory, what the firmware uses, and swap.
- **Speak here:** whatever matters where you are:
  - In FT8, the selected message.
  - In JS8, the status and the selected station.
  - In Channels, the selected channel.
  - In GPS, Winlink, SSTV and WeFax, their status.
  - In other windows, the item you are on.
  - With RTTY or CW text showing, the last text.
  - Anywhere else, the status.
- **Battery info:** the battery, and whether external power is connected and charging: "Battery 7.9 volts, 64 percent. No external power." or "Battery 7.4 volts, 62 percent. External power 13.6 volts, charging." While charging, the percentage is the thing to watch: the voltage stays close to 7.4 for most of a charge.

## Settings

APP, page 2, F5. You hear "Settings window". F1 to F4 choose the tab: "General", "Interface", "Voice", "Info". Use the MFK as described under MFK knob, and the VOL press to close.

- **General:** clock, backlight, audio gains, line in and out, CESSB, compressor, transverter, TX calibration.
  - **"Charger"**, near the end: On (the default) charges from external power, Off never charges, and Shadow charges only while the radio is switched off.
- **Interface:** long presses and hand-mic keys, timeouts, frequency acceleration, spectrum and display options.
- **Voice:**
  - "Voice mode"
  - "Voice type" or "Person": the voice
  - "Voice rate", "Voice pitch", "Voice volume"
  - "Interrupt speech" and the interrupt delays: how soon new speech cuts in
  - "Full announce every": how often a repeated prompt is said in full
  - "Read function keys"
  - "Speak pop-up messages"
  - "Read decoded text": off, text modes only, or all including CW
  - "Typing echo": off, characters, words, or both

## Keyboard (USB or Bluetooth)

The keyboard works like a screen reader's. The voice key is **Caps Lock** or **Insert**: hold it and press:

- **Space:** speak here.
- **S, M, T, B, U, K:** status, meters, time, battery, memory, keys.
- **R:** the last text received.
- **Left and Right:** previous or next character of received text.
- **Control+Left and Control+Right:** previous or next word.
- **Up and Down:** previous or next line.
- **Home and End:** first or newest line.
- **C, W, L:** current character, word or line.
- **Enter:** read from here to the end.
- **E:** typing echo.
- **A:** reading text aloud.
- **H:** help on these keys.

Tap **Control** to stop speech. Double-tap Caps Lock for a real caps lock. Page Up and Page Down change band, F switches to frequency entry, and Scroll Lock locks the frequency. "Keyboard connected" or "Keyboard disconnected" tells you it was found.

## Apps

Every app window says its name when it opens. F1 is usually its page key, and "Speak keys" lists the rest. A press of VOL, or of the app's own Close key, closes it.

### RTTY

APP page 1, F2. You hear "Teletype window". The keys are Rate, Shift, Center and Reverse. Received text is read as it comes in, in phrases.

### FT8 and FT4

APP page 1, F3. It opens with the mode and frequency ("F T 8, 14. 0 74"). Turning the MFK reads each message with its signal report, and your own as "You sent". Speak here repeats the selected one. Press the MFK on a message to answer it: "Next transmit, VE3XYZ K1ABC FN42". You also hear "Q S O saved with VE3XYZ, sent -10, received -12".

- **Page 1:** "Show all stations" or "Show C Q only", "Mode F T 4" or "F T 8", TX CQ and TX call ("TX call enabled", "TX C Q disabled").
- **Page 2:** "Hold frequency", "Auto", force QSO save.
- **Page 3:** CQ modifier, time sync ("Time synchronized").

### SWR Scan

APP page 1, F4. You hear "SWR scan window". The Run key arms: "S W R Scan, press again to start". The result is "Scan complete, lowest S W R 1.3 at 7100 kilohertz, highest...". There are also Span and Scale keys. See Known limits.

### GPS

APP page 1, F5. It opens with "GPS window" and a summary: fix, grid in phonetics, satellites, UTC. The keys are Summary, Grid, Position, Time and Sats. Turning the MFK reads one row at a time. A gained or lost fix ("GPS fix lost") or a new grid is said.

### Recorder

APP page 2, F2. You hear "Audio recorder window". Rec and Play take two presses ("Ready to play name, press again to start"). Turn the MFK through the recordings. There are Rename and Delete keys ("Deleted name").

### Voice and CW messages

The MSG key opens them.

- **Page 1:** Send, Beacon and Beacon Period. Send and Beacon take two presses ("Ready to send name, press again to start").
- **Page 2:** for voice messages, Rec, Rename, Delete and Play. For CW messages, New, Edit and Delete.

"Send stopped", "Beacon stopped" and "Beacon period 30 seconds" are said.

### QTH and Callsign

APP page 2, F3 and F4. You hear "QTH window" and "Callsign window". Type on the keyboard. You hear "QTH grid saved" or "Incorrect QTH Grid", and "Callsign VE3XYZ".

### Wi-Fi

APP page 3, F2. You hear "Wi-Fi window". The keys are On/Off, Scan, Connect, Change password and Delete. Turning the MFK reads each network: "Home, known, connected, signal 80 percent, I P address 192.168.1.40". You also hear "Scanning for networks", "Scan complete, 5 networks found", "Enter password for Home", "Connecting to Home", "Wifi Connected, I P address ...", and "Incorrect password".

### Bluetooth

APP page 3, F3. It opens with a summary. It has three pages, and the Status key repeats the summary. Turning the MFK reads each device ("2 of 5"). Connections, pairing codes (digit by digit) and failures are said. With Bluetooth listening on, the speech goes to the headphones.

### JS8

APP page 3, F4. You hear "J S 8 window" and the frequency. F1 goes through six pages ("J S 8 page 1" to "page 6").

- **Received messages** are read as they arrive. One for you is said at once: "For you: ...". Heartbeats and SNR reports are kept for Read decoded but not read aloud.
- **The message list.** Turning the MFK reads each row, with the SNR and audio offset, and your own rows as "You sent: ...".
- **Menus** say their title, then the item and "2 of 5".
- **Compose** says what to type and what is already there.
- **After sending:** "Sent" or "Transmit stopped".

### WeFax

APP page 3, F5. You hear "WeFax window". It has four pages ("WeFax page 1" to "page 4"). The decoder's messages are read: "Awaiting WEFAX header", "Fax started", "Fax saved". During a fax you hear the lines received every 300 lines. Speak here gives what it is doing and the lines so far. The lists (stations, the fax browser) read the row and "3 of 12". With WeFax open, the hand-mic UP and DOWN go through its stations, and V/M opens its stations list. The Info key (page 4) reads WeFax's help a part at a time (see SSTV).

### SSTV

APP page 4, F2. You hear "SSTV window". It has three pages ("S S T V page 1" to "page 3"). You hear:
- The status line as it changes: "SSTV Autodecoder is running. Awaiting signal...".
- The mode detected: "SSTV mode detected: Martin M1".
- Progress at 25, 50 and 75 percent.
- "End of SSTV picture detected" or "Decoding failed".

The picture lists read the row and "3 of 12".

The Info key (page 3) opens SSTV's help. It starts reading the first part: "SSTV info. Turn the knob for each part. Part 1 of 9...". Turn the MFK for the next or previous part. Press the MFK or VOL to close it.

### Channels

APP page 4, F3, or the hand-mic V/M. You hear "Channels window" and the selected channel: "40m net, 7. 0 75, L S B, 3 of 12". With none saved, you hear "No channels. Add saves the frequency you are on".

- **Moving:** the MFK moves one channel, and the VOL knob ten.
- **Choosing:** press the MFK to tune to the channel and close the window ("Channel 40m net").
- **Keys:** Close, Add ("Channel added"), Delete ("Channel deleted"), Edit (rename with the keyboard, "Channel saved"), and Drag.
- **Reordering:** Drag gives "Drag channel". Turn to move the channel ("Moving 40m net, 4 of 12"). Press Drop to finish ("Channel moved").

### Winlink

APP page 4, F4, in test builds that carry the xara modem and Pat. It opens with "Winlink window. Stopped. Press Start." You read and write mail on Pat's web page, from a phone or computer on the same Wi-Fi.

- **F1 Start or Stop.** Start sets USB digital and at most 5 watts. Your callsign must be set first.
- **F2** sets the bandwidth: 500, 2300 or 2750.
- **F3 Network** is off each time the window opens. When on, Pat's page is at the radio's address, port 8080, and the window says the address. **Anyone on that network can then use your Winlink account and key your radio**, so use it only on your own network.
- **F4 Status:** also the MFK press, and Speak here.
- **Page 2:** TX level, and speeds "Registered" or "Free".

You hear "Calling VE3XYZ", "Connected to VE3XYZ" and "Disconnected". Turning the MFK reads recent events, newest first.

## The radio's web page

From a browser on the same network, open the radio's address (the Wi-Fi window says it). You can edit bands and digital-mode frequencies, and get files from the card. The test builds' pages are made to work with screen readers: real headings, labelled fields, and a skip link.

## Known limits

- **Speech can stop after ATU tuning or SWR Scan.** This is a fault in the radio's base firmware, not in this firmware. The radio keeps working, but speech is gone until a soft reset (hold LOCK), and sometimes a power cycle.
- **Speech can be cut short.** In digital modes, the next transmission can cut a message short. Use Speak here, or the app's Status key, to hear it again.
- **Some app windows from mainline** may still have parts that aren't spoken.

Feedback welcome: open an issue on the GitHub repository.
