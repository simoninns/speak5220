# Electrical and Mechanical Data

## 9. ELECTRICAL CHARACTERISTICS

### 9.1 ABSOLUTE MAXIMUM RATINGS OVER OPERATING FREE-AIR TEMPERATURE RANGE (UNLESS OTHERWISE NOTED)\*

Any pin with respect to VSS -20 V to +0.3 V

Power Dissipation 600 mW

Operating temperature range 0°C to 70°C

Storage temperature range -30°C to 125°C

*Stresses beyond those listed under "Absolute Maximum Ratings" may cause permanent damage to the device. This is a stress rating only and functional operation of the device at these or any other conditions beyond those indicated in the "Recommended Operating Conditions" section of this specification is not implied. Exposure to absolute-maximum-rated conditions for extended periods may affect device reliability.

### 9.2 RECOMMENDED OPERATING CONDITIONS

|  PARAMETER | MIN | NOM | MAX | UNIT  |
| --- | --- | --- | --- | --- |
|  Supply voltage, V<sub>SS</sub> | 4.5 | 5 | 5.5 | V  |
|  Supply voltage, V<sub>REF</sub> |  | 0 |  | V  |
|  Supply voltage, V<sub>DD</sub> | -4.5 | -5 | -5.5 | V  |
|  High-level input voltage, V<sub>IH</sub> | V<sub>SS</sub>-0.6 |  | V<sub>SS</sub> | V  |
|  Low-level input voltage, V<sub>IL</sub> (see Note 1) |  | 0 | V<sub>SS</sub>-4 | V  |
|  Operating free-air temperature, T<sub>A</sub> | 0 |  | 70 | °C  |
|  Operational frequency (External RC) | 576 | 640 | 704 | kHz  |

NOTE 1: The algebraic convention, where the more-positive (less-negative) limit is designated as maximum, is used in this data sheet for logic voltages levels only.

### 9.3 ELECTRICAL CHARACTERISTICS OVER FULL RANGE OF RECOMMENDED OPERATING CONDITIONS

|  PARAMETER |   |   | MIN | TYP | MAX | UNIT  |
| --- | --- | --- | --- | --- | --- | --- |
|  V<sub>OH</sub> | D0-D7, WS, RS, INT | I<sub>OH</sub> = 0.4 mA | 2.4 |  | V<sub>SS</sub> | V  |
|   |  ROMCLK, ADD 1-8, M0, M1 | I<sub>OH</sub> = 100 μA | V<sub>SS</sub>-0.5 |  | V<sub>SS</sub> | V  |
|  V<sub>OL</sub> | D0-D7, WS, RS, INT | I<sub>OL</sub> = 1.6 mA | V<sub>REF</sub>-0.5 | 0 | V<sub>REF</sub>+0.5 | V  |
|   |  ROMCLK, ADD 1-8, M0, M1 | I<sub>OL</sub> = 100 μA |  |  | V<sub>SS</sub>-4.5 | V  |
|  I<sub>REF</sub> | Supply current from V<sub>REF</sub> |   |  | 3 | 5 | mA  |
|  I<sub>DD</sub> | Supply current from V<sub>DD</sub> |   |  | 10 | 35 | mA  |
|  C<sub>i</sub> | Input capacitance, (except data bus) |   |  | 15 |  | pF  |
|  C<sub>o</sub> | Output capacitance, (except data bus) |   |  | 15 |  | pF  |
|  C<sub>db</sub> | Data bus load capacitance |   | 25 |  | 300 | pF  |

### 9.4 STATIC DISCHARGE PROTECTION

All inputs and outputs are guarded against electrostatic damage by state-of-the-art protection devices incorporated on the chip.

## 11. MECHANICAL DATA

### 11.1 28-PIN 600-MIL PLASTIC PACKAGE (100-MIL PIN SPACING)

![28-pin 600-mil plastic package outline](img/img-5.jpeg)

NOTES: a. Each pin centerline is located within 0.010 inch (0.26 millimeters) of its true longitudinal position.

b. All linear dimensions are shown in inches (and parenthetically in millimeters for reference only). Inch dimensions govern.

### 11.2 PIN ASSIGNMENTS AND FUNCTIONS

|  PIN | NAME | IN/OUT | FUNCTION  |
| --- | --- | --- | --- |
|  1 | DBUS 7 | I/O | Memory data bus (LSB)  |
|  2 | ADD1 | O | Address bus to VSM (LSB)  |
|  3 | ROMCLK | O | Clock to VSM  |
|  4 | VDD | I | Drain supply voltage (-5 V NOM)  |
|  5 | VSS | I | Substrate supply voltage (+5 V NOM)  |
|  6 | OSC | I | Oscillator input  |
|  7 | T11 | O | Sync  |
|  8 | SPEAKER | O | Audio output  |
|  9 | I/O | O | Serial data out  |
|  10 | PROM OUT | O | Testing use only  |
|  11 | VREF | I | Ground reference voltage (0 V NOM)  |
|  12 | DBUS 2 | I/O | Memory data bus  |
|  13 | DBUS 1 | I/O | Memory data bus  |
|  14 | DBUS 0 | I/O | Memory data bus (MSB)  |
|  15 | M0 | O | Command bit 0 to VSM  |
|  16 | M1 | O | Command bit 1 to VSM  |
|  17 | INT | O | Interrupt (active low)  |
|  18 | READY | O | Transfer cycle W/CPU complete  |
|  19 | DBUS 3 | I/O | Memory data bus  |
|  20 | TEST | I | Testing use only  |
|  21 | ADD8/DATA | I/O | Address to VSM and serial data in (MSB)  |
|  22 | DBUS 4 | I/O | Memory data bus  |
|  23 | ADD 4 | O | Address bus to VSM  |
|  24 | DBUS 5 | I/O | Memory data bus  |
|  25 | ADD 2 | O | Address bus to VSM  |
|  26 | DBUS 6 | I/O | Memory data bus  |
|  27 | WS | I | Write select (active low)  |
|  28 | RS | I | Read select (active low)  |

### 11.3 TERMINAL ASSIGNMENTS

![TMS 5220 terminal assignments](img/img-6.jpeg)
