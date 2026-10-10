# TMS 5220 VOICE SYNTHESIS PROCESSOR DATA MANUAL

This is a transcription of Texas Instruments' data manual for the TMS 5220 Voice Synthesis Processor, marked "PRELIMINARY - JUNE 1981 - DM-02". The text, tables and figures follow TI's original, with its section numbering and capitalisation. It is split here into pages along its chapters; the table of contents below links to them. The project's own measured description of the chip is the [reference datasheet](../../datasheet/index.md), which follows the same section order.

PRELIMINARY - JUNE 1981 - DM-02

## TABLE OF CONTENTS

- 1\. [INTRODUCTION](introduction-and-commands.md#1-introduction)
  - [1.1 Scope](introduction-and-commands.md#11-scope) 1
  - [1.2 Key Features](introduction-and-commands.md#12-key-features) 1
  - [1.3 Device Operation](introduction-and-commands.md#13-device-operation) 1
- 2\. [SYSTEM CLOCK](introduction-and-commands.md#2-system-clock)
- 3\. [CPU INTERFACE](introduction-and-commands.md#3-cpu-interface)
  - [3.1 RS and WS](introduction-and-commands.md#31-rs-and-ws) 2
  - [3.2 READY](introduction-and-commands.md#32-ready) 3
  - [3.3 Interrupts](introduction-and-commands.md#33-interrupts) 3
- 4\. [VOICE SYNTHESIS MEMORY](introduction-and-commands.md#4-voice-synthesis-memory-vsm-tms-6100)
- 5\. [I/O STRUCTURE](introduction-and-commands.md#5-io-structure)
  - [5.1 Command Register](introduction-and-commands.md#51-command-register) 5
  - [5.2 FIFO Buffer](introduction-and-commands.md#52-fifo-buffer) 5
  - [5.3 Data Register](introduction-and-commands.md#53-data-register) 5
  - [5.4 Status Register](introduction-and-commands.md#54-status-register) 5
- 6\. [DESCRIPTION OF COMMANDS](introduction-and-commands.md#6-description-of-commands)
  - [6.1 Read Byte](introduction-and-commands.md#61-read-byte) 6
  - [6.2 Read and Branch](introduction-and-commands.md#62-read-and-branch) 6
  - [6.3 Load Address](introduction-and-commands.md#63-load-address) 6
  - [6.4 Speak](introduction-and-commands.md#64-speak) 7
  - [6.5 Speak External](introduction-and-commands.md#65-speak-external) 7
  - [6.6 Reset](introduction-and-commands.md#66-reset) 7
- 7\. [POWER-UP CLEAR](introduction-and-commands.md#7-power-up-clear)
- 8\. [SPEECH SYNTHESIS](speech-synthesis.md)
  - [8.1 Coded Speech Parameters](speech-synthesis.md#81-coded-speech-parameters) 8
  - [8.2 D/A Conversion](speech-synthesis.md#82-da-conversion) 9
  - [8.3 Audio Output](speech-synthesis.md#83-audio-output) 10
- 9\. [ELECTRICAL CHARACTERISTICS](electrical-and-mechanical.md#9-electrical-characteristics)
  - [9.1 Absolute Maximum Ratings](electrical-and-mechanical.md#91-absolute-maximum-ratings-over-operating-free-air-temperature-range-unless-otherwise-noted) 10
  - [9.2 Recommended Operating Conditions](electrical-and-mechanical.md#92-recommended-operating-conditions) 10
  - [9.3 Electrical Characteristics Over Full Range of Recommended Operating Conditions](electrical-and-mechanical.md#93-electrical-characteristics-over-full-range-of-recommended-operating-conditions) 11
  - [9.4 Static Discharge Protection](electrical-and-mechanical.md#94-static-discharge-protection) 11
- 10\. ENVIRONMENTAL
  - 10.1 Temperature Range 11
  - 10.2 Humidity 11
- 11\. [MECHANICAL DATA](electrical-and-mechanical.md#11-mechanical-data)
  - [11.1 28-Pin 600-Mil Plastic Packages](electrical-and-mechanical.md#111-28-pin-600-mil-plastic-package-100-mil-pin-spacing) 12
  - [11.2 Pin Assignment and Functions](electrical-and-mechanical.md#112-pin-assignments-and-functions) 13
  - [11.3 Terminal Assignments](electrical-and-mechanical.md#113-terminal-assignments) 13
- [APPENDICES](appendices.md)
  - [A System Clock](appendices.md#appendix-a-system-clock) 14
  - [B Internal Command Execution](appendices.md#appendix-b-tms-5220-internal-command-execution) 17
  - [C System Timing Diagrams](appendices.md#appendix-c-system-timing-diagrams) 23

Section 10 is listed in the original's table of contents but is not part of this transcription. The [TMS 5220 CODING TABLE](coding-table.md) follows the appendices.

## LIST OF ILLUSTRATIONS

- Figure 1 [Voice Synthesis Processor Block Diagram](introduction-and-commands.md#3-cpu-interface) 2
- Figure 2 [Read and Write Cycles to the VSP](introduction-and-commands.md#32-ready) 3
- Figure 3 [TMS 6100 Function Timing](introduction-and-commands.md#4-voice-synthesis-memory-vsm-tms-6100) 4
- Figure 4 [Synthesizer Block Diagram](speech-synthesis.md) 8
- Figure 5 [Frame Data String Lengths](speech-synthesis.md#81-coded-speech-parameters) 9
- Figure A-1 [TMS 5220/TMS 5210/20 Oscillator Options](appendices.md#appendix-a-system-clock) 14
- Figure A-2 [System Timing Summary](appendices.md#a1-oscillator-trimming-procedure) 15
- Figure B-1 [Power-On](appendices.md#appendix-b-tms-5220-internal-command-execution) 17
- Figure B-2 [Read Select Active](appendices.md#appendix-b-tms-5220-internal-command-execution) 18
- Figure B-3 [Write Select Active](appendices.md#appendix-b-tms-5220-internal-command-execution) 19
- Figure B-4 [Speak External Command](appendices.md#appendix-b-tms-5220-internal-command-execution) 20
- Figure B-5 [Reset Command](appendices.md#appendix-b-tms-5220-internal-command-execution) 21
- Figure B-6 [Speak](appendices.md#appendix-b-tms-5220-internal-command-execution) 22
- Figure B-7 [Read and Branch](appendices.md#appendix-b-tms-5220-internal-command-execution) 22
- Figure C-1 [Write Cycle for Read-and-Branch, Load-Address, Speak, Speak External, and Reset Commands](appendices.md#write-cycle-for-read-and-branch-load-address-speak-speak-external-and-reset-commands) 23
- Figure C-2 [Write Cycle for External Speech Data](appendices.md#write-cycle-for-external-speech-data) 24
- Figure C-3 [Read Cycle for Status Transfer](appendices.md#read-cycle-for-status-transfers) 25
- Figure C-4 [Read Byte Sequence](appendices.md#read-byte-sequence) 26

## LIST OF TABLES

- Table 1 [RS and WS Function](introduction-and-commands.md#31-rs-and-ws) 2
- Table 2 [VSP Commands and Command Format](introduction-and-commands.md#6-description-of-commands) 6
- Table 3 [Parameter Coding](speech-synthesis.md#81-coded-speech-parameters) 8
- Table 4 [Digital-To-Analog Converter Output](speech-synthesis.md#82-da-conversion) 10
- Table A-1 [A Comparison of System Times](appendices.md#a1-oscillator-trimming-procedure) 16
