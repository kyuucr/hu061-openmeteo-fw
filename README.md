# HU-061 weather clock: Open-Meteo firmware

Replacement firmware for the HU-061 clock kit (ESP-01S + 0.96" OLED), often sold as
a kit on AliExpress. It gets the weather from [Open-Meteo](https://open-meteo.com/),
which needs **no account, API key or phone number** and free for non-commercial use.
The clock makes about 48 requests a day.

## What it does

* Same screens and button as the stock firmware: date/time, current weather and a 3-day
  forecast, in English; press the button to update the weather.

New compared with the stock firmware:

* **Settings page on your home network** (`http://weatherclock.local/`), instead of a setup
  hotspot at power-on. It can have an optional password.
* **City by name** (`Paris, FR`) or coordinates, instead of a QWeather location ID.
* **Time zone with daylight saving**, set automatically from the location, instead of a
  whole-hour UTC offset.
* **°F/mph, 12-hour clock and the update interval** can be changed (stock: °C, 24-hour, 30 min).
* **Firmware updates over Wi-Fi.**
* **Night icons**, and wind speed instead of the wind force number.
* **Correct time at power-on**: no "1970-01-01" while it waits for internet time.

## Flashing (first time)

Takes about 15 minutes. You don't need to compile anything: the ready-made firmware is
`release/hu061-openmeteo-v1.0.0.bin`.

### What you need

* **A USB-to-serial adapter.** Either:
  * an **"ESP-01 USB programmer" adapter with a PROG/UART switch** (about $2). Easiest: the
    module plugs straight in, no wires; or
  * a **CH341A programmer** (the black one sold for flash chips), set to serial mode. It needs
    six jumper wires and two resistors, because it sends 5 V signals and the ESP-01S is a
    3.3 V chip.
* **esptool**, the official Espressif flashing program. It needs Python 3:

  ```bash
  pip install esptool          # or: pipx install esptool
  ```

  Windows: `py -m pip install esptool`. If the `esptool` command isn't found afterwards,
  use `python -m esptool` in its place in the commands below.

### 1. Take the ESP-01S out of the clock

Unplug the clock. **Take a photo first** so you can put the module back the same way round,
then pull the small blue/black ESP-01S board off its 8-pin socket. Its pins are labelled on
the back.

### 2. Connect it to the adapter

**ESP-01 USB adapter:** set the switch to **PROG** and plug the module in, the way round shown
on the adapter's printing or the seller's photo. Go to step 3.

**CH341A:**

1. Move the mode jumper to **pins 2-3** (marked TTL刷机 / serial). In the default position
   (1-2) it is a flash-chip programmer and the computer won't see a serial port.
2. Wire it like this, with short jumper wires:

   | CH341A pin | ESP-01S pin | |
   |---|---|---|
   | TXD | RX | **through the voltage divider below** |
   | RXD | TX | direct |
   | GND | GND | |
   | 3.3V (on the other header) | 3V3 (VCC) **and** EN (CH_PD) | |
   | GND | IO0 (GPIO0) | puts the chip in flashing mode |
   | 5V | nothing | **don't connect** |

   The voltage divider turns the CH341A's 5 V into about 3.3 V:

   ```
   CH341A TXD ──[ 1 kΩ ]──┬──────► ESP-01S RX
                          │
                       [ 2 kΩ ]
                          │
   CH341A GND ────────────┴──────► ESP-01S GND
   ```

   Any pair with a 1:2 ratio works (1 k + 2.2 k, 10 k + 20 k). To check it, plug the CH341A in
   *without* the ESP-01S and measure between the junction and GND: you should see about
   3.1–3.3 V.

Some people skip the divider and get away with it, but it can damage the ESP8266 over time.

### 3. Plug in and find the port

Plug the adapter into the computer **with GPIO0 already connected to GND** (or the switch on
PROG). The chip only enters flashing mode at the moment it gets power.

| System | Port name | |
|---|---|---|
| Linux | `/dev/ttyUSB0` | if you get "permission denied": `sudo usermod -aG dialout $USER`, then log out and in |
| Windows | `COM3`, `COM4`, … | see Device Manager → Ports. No port? Install the CH340/CH341 driver ("CH341SER") |
| macOS | `/dev/cu.usbserial-…` or `/dev/cu.wchusbserial…` | `ls /dev/cu.*` |

### 4. Back up the original firmware (recommended)

This is the **only** way to get the original firmware back later; nobody publishes it.
Replace `/dev/ttyUSB0` with your port:

```bash
esptool --port /dev/ttyUSB0 read-flash 0 0x100000 hu061-stock.bin
```

It takes about 2 minutes and should end with a 1,048,576-byte file. Keep it somewhere safe.

### 5. Flash the new firmware

**Unplug the adapter and plug it back in** (GPIO0 still on GND), then:

```bash
esptool --port /dev/ttyUSB0 write-flash 0x0 release/hu061-openmeteo-v1.0.0.bin
```

It takes about a minute. It worked if the output ends with **`Hash of data verified.`**
(Older esptool versions use `esptool.py` and `write_flash` / `read_flash` with underscores.)

### 6. Put it back

Unplug the adapter, **remove the GPIO0–GND wire** (or set the switch back to UART), and put
the ESP-01S back into the clock the same way round as in your photo. Then go on to
[Setting up](#setting-up).

### If something goes wrong

| Message or symptom | Fix |
|---|---|
| `No serial data received` / `Failed to connect` | The chip isn't in flashing mode. Check the GPIO0–GND wire, then unplug and replug the adapter. Check that TX and RX aren't swapped. |
| `Invalid head of packet` or garbled data | Usually the RX line or power. Check the divider, use shorter wires, make sure both GND wires are firm. Replug and try again; the first try after a failure often fails. |
| The port doesn't appear | CH341A: jumper on 2-3? Windows: install the CH341SER driver. Try another USB cable or port. |
| Flashing worked, but the screen stays dark | GPIO0 is still connected to GND, or the module is in the wrong way round. |

Nothing you do here can permanently break the module: if a write fails, just flash again.

### Going back to the original firmware

Same steps 1–3 and 5–6, but flash your backup:

```bash
esptool --port /dev/ttyUSB0 write-flash 0x0 hu061-stock.bin
```

### Why esptool and not the Arduino IDE or PlatformIO?

They all end up running esptool. The Arduino IDE's **Upload** button and PlatformIO's
`pio run -t upload` compile the source first, then call esptool to write the result. Using
esptool directly skips the compiling, so you only need it plus the ready-made `.bin`. You only
need the other tools if you want to change the code (see [Building from source](#building-from-source)).

## Setting up

1. Plug the clock into USB. If it remembers a Wi-Fi network from the original firmware, it
   connects to it. Otherwise it opens a hotspot called **HU061-Setup**: join it from your
   phone. The setup page usually opens by itself (if not, go to `192.168.4.1`). Pick your
   **2.4 GHz** network and enter the password.
2. The clock shows its address, e.g. `http://192.168.1.50`. Open it in a browser on the same
   network (or try `http://weatherclock.local/`), type your city and press **Save**.

Forgot the address? Hold the button for 2 seconds.

## Button

| Action | Result |
|---|---|
| Short press | Update the weather now |
| Hold 2 s | Show the settings page address (press again to close) |
| Hold while plugging in | Wi-Fi setup hotspot **HU061-Setup** → open `192.168.4.1` |

## Later updates over Wi-Fi

Settings page → **Firmware update** → choose `release/hu061-openmeteo-vX.bin.gz` (the gzip
file). Space is tight on 1 MB flash: the compressed image must be smaller than the free space
(≈430 KB; v1.0.0 is 407 KB). If an update ever fails, the clock keeps running the old version;
the cable method always works.

## Building from source

Most people don't need this: the ready-made image is in `release/`. The source folder
`hu061_weather_clock/` is a normal Arduino sketch, so it builds in either tool below. Both
produce the same firmware (within a few hundred bytes).

### Arduino IDE (1.8 or 2.x)

1. **File → Preferences → Additional boards manager URLs:**
   `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
2. **Boards Manager:** install **esp8266** by ESP8266 Community, version **3.1.2**.
3. **Library Manager:** install these exact versions (newer ones may work, but aren't tested):

   | Library | Author | Version |
   |---|---|---|
   | WiFiManager | tzapu | 2.0.17 |
   | ArduinoJson | Benoit Blanchon | 7.4.1 |
   | ESP8266 and ESP32 OLED driver for SSD1306 displays | ThingPulse | 4.6.1 |

4. Open `hu061_weather_clock/hu061_weather_clock.ino`. The other files open as tabs.
5. **Tools** menu:

   | Setting | Value |
   |---|---|
   | Board | Generic ESP8266 Module |
   | Flash Size | **1MB (FS:none OTA:~502KB)**: anything else and Wi-Fi updates won't fit |
   | Flash Mode | QIO (fast) |
   | Flash Frequency | 40MHz |
   | Reset Method | no dtr (aka ck): the CH341A can't reset the chip, you ground GPIO0 by hand |
   | Upload Speed | 115200 |
   | everything else | default |

6. **Upload** (with GPIO0 grounded and the module power-cycled, as above), or
   **Sketch → Export Compiled Binary** to get a `.bin` for esptool or the Wi-Fi update page.

### arduino-cli

The board settings and library versions are pinned in `hu061_weather_clock/sketch.yaml`:

```bash
arduino-cli compile --profile hu061 hu061_weather_clock
arduino-cli upload  --profile hu061 -p /dev/ttyUSB0 hu061_weather_clock
```

### PlatformIO

Everything is pinned in `platformio.ini`:

```bash
pio run                      # -> .pio/build/hu061/firmware.bin
pio run -t upload            # flash over /dev/ttyUSB0
```

### Host unit tests (time-zone parsing)

```bash
g++ -std=c++17 -Itest/host -Ihu061_weather_clock -include climits -o test/host/test_tz \
    test/host/test_tz.cpp hu061_weather_clock/config.cpp && test/host/test_tz
```

### Making a release

1. Bump `FW_VERSION` in `hu061_weather_clock/version.h`.
2. Build, then copy the image to `release/hu061-openmeteo-vX.Y.Z.bin` and make the Wi-Fi
   update file with `gzip -9k`.

## Files

| File | Purpose |
|---|---|
| `hu061_weather_clock/hu061_weather_clock.ino` | setup/loop, Wi-Fi, button, update scheduling, time zone |
| `hu061_weather_clock/weather.cpp` | Open-Meteo geocoding + forecast, WMO code → icon/text |
| `hu061_weather_clock/display.cpp` | screens (layout from the stock firmware / ThingPulse demo) |
| `hu061_weather_clock/web.cpp` | settings page, firmware upload |
| `hu061_weather_clock/config.cpp` | settings in flash (with checksum), time-zone strings |
| `hu061_weather_clock/fonts.h` | Meteocons fonts and indicator dots (same bytes as the stock firmware) |
| `hu061_weather_clock/version.h` | firmware version |
| `hu061_weather_clock/sketch.yaml` | pinned board/library setup for arduino-cli |
| `platformio.ini` | the same for PlatformIO (`src_dir` points at the sketch folder) |
| `release/` | ready-to-flash images |

## Known limits

* HTTPS certificates aren't checked (as in the stock firmware); the weather isn't secret.
* With time zone **auto**, a daylight-saving change shows up at the next weather update
  (≤ the update interval). Use a POSIX rule if you need it on the exact minute.
* A new settings password protects firmware uploads only after a restart.

## License

MIT, see [LICENSE.md](LICENSE.md). Parts by others:

* **Meteocons fonts** in `fonts.h`: MIT, © 2015 Daniel Eichhorn (ThingPulse); the notice is in
  the file. The screen layout follows ThingPulse's WeatherStationDemo (also MIT).
* **Libraries** (downloaded at build time, not included here): WiFiManager, ArduinoJson and
  the ThingPulse SSD1306 driver (all MIT), and the ESP8266 Arduino core (LGPL 2.1). The
  ready-made `.bin` files contain all of these. Their full source is public, and this
  project's source is here, so anyone can rebuild or modify the firmware.
* **Weather data** by [Open-Meteo](https://open-meteo.com/) (CC BY 4.0).
