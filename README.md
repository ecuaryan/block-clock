# Block Clock

A block clock is a small desk display that shows live Bitcoin network data — block height, blocks remaining to the next halving, BTC price, and network hashrate — on a low-power e-ink screen.

This is a DIY block clock that runs on the Heltec Vision Master E213 or E290 boards. It costs about $30 in parts, has no soldering for the basic build, and reads data either from your own Bitcoin node over MQTT or from mempool.space directly. The device wakes on a configurable interval, refreshes the display, and goes back to deep sleep — roughly one week of runtime on a small LiPo battery.

| Front | Side |
| --- | --- |
| ![Front view of the DIY block clock showing block height, halving countdown, BTC price, and hashrate on an e-ink display](photos/blockclock-front.jpg) | ![Side view of the DIY block clock case showing the slim e-ink module on the Heltec Vision Master board](photos/blockclock-side.jpg) |

Site page: [freedomclock.io/diy-block-clock/](https://freedomclock.io/diy-block-clock/).

The current firmware supports both Heltec Vision Master E-series boards:

- Heltec Vision Master E290 (recommended)
- Heltec Vision Master E213

Both boards use the same source code. The firmware selects the right display driver and layout at compile time based on the board selected in Arduino IDE.

## What It Shows

- Block height
- Blocks remaining to the next halving
- BTC price in USD, EUR, CHF, GBP, CAD, or AUD
- Network hashrate in EH/s
- Recommended low, medium, and high transaction fees in sat/vB
- Minutes since the latest Bitcoin block
- Wi-Fi offline icon when the last fetch failed
- Last update time
- Light or dark display theme

The display refreshes on the configured interval, then the ESP32 goes to deep sleep.

## Data Sources

Block Clock has two data modes.

**Online mode** fetches public data directly from mempool.space on every wake:

- block height
- BTC price in the selected currency (CoinGecko as fallback)
- network hashrate in EH/s
- recommended transaction fees and latest block timestamp from mempool.space
- halving countdown calculated locally from block height

**MQTT mode** is for people who already publish Bitcoin data on their local network, for example from a Bitcoin node or Home Assistant. Use retained MQTT messages so the clock can wake, read the latest values, refresh the display, and sleep again.

MQTT is the better privacy option. Online mode is simpler.

## Supported Hardware

- Heltec Vision Master E290, 2.90" display, `296 x 128` (recommended) (https://heltec.org/project/vision-master-e290/)
- OR Heltec Vision Master E213, 2.13" display, `250 x 122` (https://heltec.org/project/vision-master-e213/)
- 3.7 V LiPo battery, 803040 with PH 1.25 connector

No soldering is required for the basic build.

The E213 build has been tested around one week of runtime on a 500 mAh battery with 10-minute refreshes. Runtime on E290 depends on the battery, refresh interval, and Wi-Fi conditions.

## 3D Printable Case

The repository includes an E290 case design in [hardware/case](hardware/case):

- `block_clock_body.stl`
- `block_clock_cover.stl`
- `block_clock_e290_case.scad`

The `.stl` files are ready for slicing and printing. The `.scad` file is the source model and can be modified with [OpenSCAD](https://openscad.org/).

## First Setup

On first boot, or after factory reset, the device shows a welcome screen:

```text
BLOCK CLOCK
Press any button to begin
```

Press either button. The device starts a setup Wi-Fi network and shows a QR code on the e-ink display. Scan the QR code to join the setup network, then open:

```text
http://192.168.4.1
```

The setup page lets you configure:

- Wi-Fi network and password
- Data source: online or MQTT
- Currency: USD, EUR, CHF, GBP, CAD, or AUD
- Refresh interval
- Display theme: light or dark
- Battery percentage display
- MQTT server, port, credentials, and topics

When settings are saved, the e-ink display shows a saved screen and the device restarts.

There is no setup PIN. Block Clock does not store personal financial data.

## Buttons

- `FUNCTION` (`GPIO21`): a quick tap while the clock is asleep flips to the next screen (Price, Network, Supply) from the last fetched data, with no network wait. Hold for about 1.5 seconds to fetch fresh data instead.
- `SETUP` (`BOOT / GPIO0`): a quick tap flips screens too, so either button works for browsing. Hold for about 3 seconds to enter setup mode. Hold for about 10 seconds to factory reset — the device shows a reset screen, clears all saved settings, and returns to the welcome screen.

The clock stays on the screen you picked when it refreshes on its timer.

On the printed case the buttons carry icons: the square frame is `SETUP`, the X is `FUNCTION`, and the ring is `HOME` (RST). `HOME` reboots the clock: it clears the saved state, fetches fresh data, and returns to the Price screen.

Avoid holding `SETUP` while pressing `HOME` (RST); on ESP32 boards that combination can enter the bootloader used for flashing firmware.

## Default MQTT Topics

```text
home/bitcoin/height
home/bitcoin/halving/blocks_remaining
home/bitcoin/hashrate_ehs
home/bitcoin/price/usd
```

The price topic should publish the BTC price in the currency selected on the setup page.

## Defaults

Main defaults are in [src/config.h](src/config.h).

- currency: `USD`
- refresh interval: `10 minutes`
- theme: `Light`
- battery percent: shown
- MQTT port: `1883`

## Build And Flash

1. Install Arduino IDE (https://support.arduino.cc/hc/en-us/articles/360019833020-Download-and-install-Arduino-IDE).
2. Install these libraries via the Library Manager in Arduino IDE:
   - heltec-eink-modules by Todd Herbert
   - Heltec ESP32 Dev-Boards
   - PubSubClient by Nick O'Leary
   - ArduinoJson by Benoit Blanchon
3. Open [Block_Clock.ino](Block_Clock.ino).
4. Connect the board via USB-C. Select the right port in Tools > Port.
5. Select the matching board in Tools > Board:
   - `Heltec Vision Master E213`
   - `Heltec Vision Master E290`

   The three-screen dashboard is laid out for the E290 (296 × 128). The E213 profile still builds, but its layout has not been adapted to the smaller 250 × 122 panel.
6. Upload the sketch.
7. Press the RST button on the device once the upload starts if the board does not enter flash mode automatically.
8. Join the device setup Wi-Fi and configure it in the browser.

## Optional Secrets Bootstrap

Normal setup happens through the browser page. If you want local defaults for development, copy `secrets.example.h` to `secrets.h` and edit it.

`secrets.h` is ignored by git.

## E-ink Emulator

The native emulator renders the firmware screen code to SVG, so you can review layouts before uploading to a board.

Generate the default screen set for E290:

```sh
./dev-local/generate_eink_screens.sh
```

Render one screen:

```sh
./dev-local/eink_emulator.sh --profile e290 --screen main --out /tmp/block-clock-main.svg
```

Supported screens: `main` (price), `network`, `supply`, `loading`, `welcome`, `setup`, `saved`, `reset`. The generated gallery is written under `dev-local/eink-screens/`, which is ignored by git.

## Code Structure

The Arduino entrypoint is intentionally small. Most firmware behavior lives in focused headers:

- [Block_Clock.ino](Block_Clock.ino): Arduino entrypoint, boot sequence, sleep, and wake handling
- [src/config.h](src/config.h): board profile, defaults, constants, and config structs
- [src/config_runtime.h](src/config_runtime.h): saved config, battery reading, device ID, and helpers
- [src/setup_portal.h](src/setup_portal.h): captive setup page, Wi-Fi scanning, and save handler
- [src/block_data.h](src/block_data.h): MQTT and online data fetching
- [src/display_screens.h](src/display_screens.h): e-ink screens, icons, and main dashboard drawing
- [dev-local/eink_emulator.cpp](dev-local/eink_emulator.cpp): native SVG emulator for e-ink screens

## License

MIT
