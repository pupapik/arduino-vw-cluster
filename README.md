# VW T5/T6 Instrument Cluster Sim Dashboard

Drive a real Volkswagen T5/T6 instrument cluster (and many other VW-based clusters) from your PC as a sim racing dashboard. An Arduino + MCP2515 CAN module emulates the car's CAN bus messages, so the cluster behaves as if it were bolted into a real vehicle — speedometer, tachometer, fuel/temp gauges, warning lights, and turn signals all respond to commands sent over serial.

No car required. No original ECU required. Just the cluster, a CAN module, and an Arduino.

Works with any software that can send plain text over a serial port, including [SimHub](https://www.simhubdash.com/) (see below). Confirmed working with **Forza Horizon 6** and **BeamNG.drive** through SimHub; it should work with any title SimHub supports, since the protocol only needs a handful of common telemetry properties (speed, RPM, etc.).

## How it works

The instrument cluster expects to see specific CAN frames on specific IDs at specific intervals — that's how it knows the car is "alive" and what to display. This sketch builds and sends those frames continuously:

- **Fast loop (~20 ms):** speed, RPM, fuel counter, ABS frame
- **Slow loop (~100 ms):** airbag/seatbelt, indicators/lights, oil temp, coolant temp, status message

Values are controlled live over a serial connection using short text commands, so you can drive the cluster from a sim racing game, a custom dashboard app, a script, or just by typing commands directly into the Serial Monitor for testing.

## Hardware

- Arduino Uno or Nano (tested) — other SPI-capable boards likely work too
- MCP2515 CAN module (8 MHz crystal, configured for 500 kbps)
- The instrument cluster itself
- 12V DC power supply

> **Note:** This was built around the VW T5/T6 cluster's CAN protocol, but the same message IDs and frame layout are shared across a number of other VW-based clusters. Compatibility isn't guaranteed for every variant — test carefully before wiring anything permanently.

## Wiring

Tested on **Arduino Uno** and **Arduino Nano**.

**MCP2515 module → Arduino**

| MCP2515 pin | Arduino pin |
|---|---|
| VCC | 5V |
| GND | GND (common ground) |
| CS | 10 |
| SO (MISO) | 12 |
| SI (MOSI) | 11 |
| SCK | 13 |
| INT | not used |

> Pin numbers above are the standard Uno/Nano hardware SPI pins, so they apply regardless of how your specific MCP2515 board labels SO/SI/SCK.

**MCP2515 → Cluster CAN bus**

| MCP2515 pin | Cluster pin |
|---|---|
| H (CAN-H) | Pin 5 (counting from bottom-left of the cluster connector) |
| L (CAN-L) | Pin 4 (counting from bottom-left of the cluster connector) |

There's also a jumper on the MCP2515 board that needs to be connected (this is typically the 120Ω termination resistor jumper).

**Alternative: wiring directly to the 32-pin cluster connector**

If you're wiring straight into one of the cluster's 32-pin connectors (rather than through an intermediate connector), these pin numbers were used:

| Function | Pin |
|---|---|
| GND | 16 |
| +12V | 31, 32 |
| CAN | 28, 29 |

Pin 31 is believed to be constant +12V and pin 32 switched +12V (after ignition) — also not fully confirmed, so if your cluster behaves oddly (e.g. doesn't power down with ignition off), try swapping which pin gets which feed.

The 28/29 CAN pair wasn't confirmed against the cluster's own documentation during this build — which one is CAN-H and which is CAN-L wasn't verified. For reference, a pinout posted on the [T6 Forum](https://www.t6forum.com/threads/cluster-32-pin-connector-data.46433/) for the same 32-pin connector lists pin 28 as CAN-H and pin 29 as CAN-L, with +12V on pin 31 and GND on pin 16 — consistent with the pins above. If your cluster doesn't come up, try swapping H/L on this pair.

**Power**

| Connection | Goes to |
|---|---|
| 12V DC supply (+) | Cluster pin 2 (bottom-left) — main power |
| 12V DC supply (+) | Cluster pin 1 (bottom-left) — ignition |
| 12V DC supply (+) | Arduino Vin |
| 12V DC supply (–) | Common ground — cluster pins 1 & 2 (right side), Arduino GND, MCP2515 GND |

Arduino GND and the common ground above all need to be tied together.

> Pin numbering ("bottom-left", "pin 1/2/4/5") refers to one specific T5/T6 cluster connector and is provided as a real, tested reference point — but connector pinouts vary between cluster variants and revisions. Always verify against your own cluster's connector before wiring.

Double-check your specific cluster's connector pinout before connecting — getting this wrong can damage the cluster.

## Getting started

1. Install the [autowp/arduino-mcp2515](https://github.com/autowp/arduino-mcp2515) library (provides `mcp2515.h`).
2. Wire up the MCP2515 module as above and connect the cluster's CAN lines.
3. Flash the sketch to your Arduino.
4. Open a serial connection at **115200 baud**.
5. Send commands terminated with `;` (see below).

## Serial commands

Every command is plain text, ends with `;`, and is case-sensitive where noted.

| Command | Effect |
|---|---|
| `sXXX;` | Set speed (km/h) |
| `rXXX;` | Set RPM |
| `kX.XX;` | Set display correction factor (speedo calibration) |
| `tX;` | Turn signal, 0–3 (0=off, 1=left, 2=right, 3=both) |
| `lX;` | Left turn signal on/off (0/1) |
| `hX;` | Right turn signal on/off (0/1) |
| `HX;` | High beam on/off (0/1) |
| `fX;` | Fog light on/off (0/1) |
| `ccX;` | Cruise control indicator on/off (0/1) |
| `bwX;` | Battery warning light on/off (0/1) |
| `tpX;` | Tire pressure warning on/off (0/1) |
| `epcX;` | EPC warning light on/off (0/1) |
| `dpfX;` | DPF warning light on/off (0/1) |
| `bX;` | Seatbelt warning on/off (0/1) |
| `dXX;` | Door open status (bitmask) |
| `mXX;` | Dashboard status message ID (1–15) |
| `blXX;` | Backlight/illumination level |
| `ToXX;` | Oil temperature (°C, roughly -49 to 194 range supported) |
| `TcXX;` | Coolant temperature (°C, roughly -45 to 130 range supported) |
| `help;` | Print the command list over serial |

Example: setting 120 km/h and 3000 RPM:
```
s120;r3000;
```

## Using it with SimHub

The easiest way to drive this from a real sim racing game is [SimHub](https://www.simhubdash.com/), using its **Custom serial devices** feature (Devices → Custom serial devices → Add new serial device).

1. Set **Serial port** to the Arduino's COM port and **Baudrate** to `115200`.
2. Under **Protocol definition → Update messages**, add a message with a custom update formula that builds a command string in the format this sketch expects, and set the update rate (Hz) to whatever your hardware can comfortably handle — 10 Hz is a safe starting point. SimHub's formula field uses [NCalc](https://github.com/ncalc/ncalc) syntax, which is what the `format(...)` calls below are.
3. Enable the device and confirm it shows **Connected**.

Example formula tested working with **Forza Horizon 6** and **BeamNG.drive**, sending speed, RPM, and both turn signals:

```
'S'+format([SpeedKmh],'0')+';R'+format([Rpms],'0')+';l'+format([TurnIndicatorLeft],'0')+';h'+format([TurnIndicatorRight],'0')+';'
```

This produces a string like `S120;R3000;l0;h0;` on every update tick — exactly the command syntax the sketch parses over serial.

You can extend this the same way to map any other SimHub property to the remaining commands (high beam, fog light, cruise control, warning lights, door states, temperatures, and so on) — just chain more `';X'+format([Property],'0')` segments onto the formula, using the matching command letter(s) from the table below.

> SimHub exposes hundreds of telemetry properties depending on the game; check **Available properties** in SimHub to see what your specific title supports (not every game reports things like door state or coolant temperature).

## Notes on the simulation

- Speed entered via `sXXX` is treated as the *displayed* value; the actual value sent to the cluster is divided by the calibration factor (`k`) before transmission, so the gauge needle matches real-world speed if your cluster has its own internal correction.
- Mileage/odometer distance accumulates automatically based on the current speed and elapsed time — there's no command to set it directly.
- Fuel level counter currently increments automatically over time as a placeholder; replace this with real sim telemetry if you want accurate fuel display.
- This sketch only emulates a fixed set of CAN IDs/signals. If your sim or game telemetry needs to drive other gauges or warning lights, you'll need to extend the packet-building functions.

## Disclaimer

This project is intended for sim racing / hobby use on a bench-mounted cluster, disconnected from any real vehicle. It is not intended to be connected to or used in an actual running vehicle's CAN bus. Wiring this incorrectly to a real car's network could trigger faults or damage other modules. Use at your own risk.

## License

MIT — see [LICENSE](LICENSE).
