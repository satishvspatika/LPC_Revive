# Spatika TWS 5.1.0.5 System Architecture, Build & Changelog Guide

> **Important**: This document is the single source of truth for the recovered **TWS 5.1.0.5** firmware repository. When opening this folder as a standalone project, all required scripts, configuration files, linker definitions, and toolchain configurations are completely self-contained. Always update this document whenever modifications are made so past issues and failed fixes are never repeated.

---

## 1. System Architecture Overview

The Spatika Automatic Weather Station (TWS) consists of two tightly coupled NXP microcontrollers communicating via full-duplex UART:

```
+-------------------------------------------------------------------+
|                            KSNDMC / CLOUD                         |
|                         rtdas.ksndmc.net                          |
|                       (117.216.42.181:80)                         |
+-------------------------------------------------------------------+
                                  ^
                                  | 4G LTE / 2G GPRS (HTTP POST)
                                  v
+-------------------------------------------------------------------+
|                 NXP LPC1768 (Main Controller)                     |
|  - ARM Cortex-M3 @ 100 MHz, 512 KB Flash, 64 KB RAM               |
|  - FreeRTOS Kernel, Multi-tasking Telemetry Scheduler             |
|  - Modem: SIMCom SIM7600 on UART2 (115200 baud)                   |
|  - UI: 16x2 HD44780 LCD & 3x3 Keypad                              |
|  - Storage: SD Card (SPI/SSP1) & Internal IAP Flash               |
+-------------------------------------------------------------------+
             ^                                           ^
   UART3 (9600 8-N-1)                          UART0 (115200)
   Commands: $S, $G, $n, $f                    Debug / Console Output
             v                                           
+-------------------------------------------------------------------+
|                 NXP LPC845 (Sensor Coprocessor)                   |
|  - ARM Cortex-M0+ @ 30 MHz FRO, 64 KB Flash, 16 KB RAM            |
|  - Power rail sequencer (LCD power, sensor power)                 |
|  - Real-Time Clock & Periodic 1-second $D sensor packet generator |
|  - Sensors: Rain gauge tips, Wind Speed, Wind Dir, Temp, Humid    |
+-------------------------------------------------------------------+
```

---

## 2. Directory Structure & File Map

```
TWS5_1_0_5_recovered_Jan2026/
├── PROJECT_SETUP_AND_CHANGELOG.md  <-- This master documentation file
├── README.md                       <-- Symlink to this master file
├── build_all.sh                    <-- 1-click build for BOTH LPC1768 and LPC845
├── flash_lpc1768.sh                <-- 1-click erase, flash & verify LPC1768
├── flash_lpc845.sh                 <-- 1-click erase, flash & verify LPC845
├── scripts/
│   ├── lpc_checksum.py             <-- Calculates & patches LPC Vector 7 Checksum (offset 0x1C)
│   ├── boot_stub.s                 <-- 56-byte Cortex-M3 reset boot stub for Sector 0
│   ├── boot_stub.ld                <-- Linker script for boot stub (Origin: 0x00000000)
│   ├── lpc1768-mculink.json        <-- MCU-LINK probe config (100 kHz, sysresetreq)
│   └── lpc845-mculink.json         <-- MCU-LINK probe config for LPC845
├── Example01/                      <-- LPC1768 Main Application Project
│   ├── build.sh                    <-- Standalone build script for LPC1768
│   ├── gprs.c                      <-- Upgraded cellular engine (2G/4G/APN/HTTP fixes)
│   ├── gprs.c.before_2g_4g_fix     <-- Backup of original recovered gprs.c
│   ├── main.c                      <-- System entry point, FreeRTOS task spawns
│   ├── sensor.c                    <-- UART3 coprocessor link & 9600-baud parser
│   ├── memory.c                    <-- IAP Flash day/param sectors (0x00001000 - 0x00008FFF)
│   ├── disk.c                      <-- SD card FatFs storage manager
│   ├── lcd.c / kbd.c / ui.c        <-- Display and keypad drivers
│   ├── Debug/
│   │   ├── example01.ld            <-- Linker script (Origin: 0x00018000, Length: 160 KB)
│   │   ├── LPC1768_TWS_FULL.hex    <-- Production flash image (Boot stub + App)
│   │   └── LPC1768_TWS_FULL.bin    <-- Binary production image
│   ├── NXP/Drivers/                <-- LPC17xx peripheral drivers (UART, SSP, GPIO, etc.)
│   ├── fat_sd/                     <-- ChaN FatFs SD card driver
│   └── USBHostLite/                <-- LPC17xx USB Host stack
├── LPC845/                         <-- LPC845 Coprocessor Project
│   ├── build.sh                    <-- Standalone build script for LPC845
│   ├── main.c                      <-- Sensor loops, RTC timekeeping, 9600-baud $D packet
│   ├── globals.h                   <-- Sensor data definitions and thresholds
│   ├── LPC845_TWS.hex              <-- Production flash image (Patched checksum)
│   ├── LPC845_TWS.bin              <-- Binary production image
│   ├── CMSIS/                      <-- Cortex-M0+ CMSIS headers
│   ├── drivers/                    <-- NXP MCUXpresso LPC845 SDK drivers
│   └── Debug/
│       └── lpcxpresso845max_gpio_led_output_Debug.ld <-- Linker script
├── CMSISv1p30_LPC17xx/             <-- LPC1768 CMSIS core & system files
└── FreeRTOS_Library/               <-- FreeRTOS kernel source code
```

---

## 3. How to Compile

### Option A: Build Everything in One Step (Recommended)
From the root of this project, simply run:
```bash
./build_all.sh
```
This executes both builds sequentially and outputs:
- LPC1768 Image: `Example01/Debug/LPC1768_TWS_FULL.hex`
- LPC845 Image: `LPC845/LPC845_TWS.hex`

### Option B: Build Individually
- **LPC1768 only**:
  ```bash
  cd Example01 && ./build.sh
  ```
- **LPC845 only**:
  ```bash
  cd LPC845 && ./build.sh
  ```

---

## 4. How to Flash

Flashing is performed via NXP MCU-LINK debug probe using LinkServer.

### Step 1: Flashing LPC845 Coprocessor
1. Connect the MCU-LINK SWD ribbon cable to the **LPC845** programming header.
2. Power **ON** the LPC845 board.
3. Run:
   ```bash
   ./flash_lpc845.sh
   ```
   *Action*: Automatically mass-erases 64 KB, flashes `LPC845_TWS.hex`, performs read-back verification against ELF, and issues a timed hardware reset.

### Step 2: Flashing LPC1768 Main Controller
1. Move the MCU-LINK SWD ribbon cable to the **LPC1768** header (`JTAG/SWD`).
2. Power **ON** the LPC1768 board.
3. Run:
   ```bash
   ./flash_lpc1768.sh
   ```
   *Action*: Automatically mass-erases 512 KB, programs Sector 0 Boot Stub + Application at `0x00018000`, performs read-back verification against ELF, and issues a timed hardware reset.

---

## 5. LPC1768 Memory Architecture & Standalone Boot Stub

### Flash Sector Map
| Sector | Address Range | Size | Allocation / Content |
| :---: | :---: | :---: | :--- |
| **0** | `0x00000000 - 0x00000FFF` | 4 KB | **Primary Boot Stub** (Vector Table & Reset handler) |
| **1** | `0x00001000 - 0x00001FFF` | 4 KB | **`PARAM_SECTOR`** (Station ID, APN, calibration values) |
| **2–8** | `0x00002000 - 0x00008FFF` | 28 KB | **`days_sector`** (Historical daily rainfall & sensor logs) |
| **9–16** | `0x00009000 - 0x00017FFF` | 60 KB | Reserved / Unused |
| **17–21** | `0x00018000 - 0x0003A000` | 138 KB | **Application Firmware (`Example01`)** |
| **22–29** | `0x0003A000 - 0x0007FFFF` | 256 KB | Free flash space |

### Why the Boot Stub is Required
1. In the original production architecture, a DOTA bootloader lived at `0x00000000`, while the main application was linked at `ORIGIN = 0x00018000` so that it would never collide with the parameter sector (`0x1000`) or day records (`0x2000-0x8FFF`).
2. If the chip is flashed without a reset vector at `0x00000000`, the Cortex-M3 hardware boot ROM finds nothing at `0x0` and fails to boot into `0x18000`.
3. If the entire application is re-linked to `0x00000000`, its code would span `0x00000000 - 0x00022000`, directly overlapping sectors 1–8. When `memory.c` erases a sector to write daily logs, it would erase its own application code!
4. **The Solution**: A tiny 56-byte standalone boot stub at `0x00000000`:
   - Configures Vector 7 Checksum = `0xEFFF7FDF` so the NXP Boot ROM accepts the image.
   - On reset, the stub sets `SCB->VTOR = 0x00018000`, sets `MSP = *(uint32_t*)0x18000`, and jumps directly to `0x180ED` (`Reset_Handler`).
   - Sectors 1–8 remain completely safe from code overwrites.

---

## 6. Complete 2G / 4G / APN / HTTP Changelog & Fixes Applied

### 1. Multi-RAT Network Registration Check (`check_sim_registration`)
- **Problem**: 2G SIM cards (and roaming cards) failed registration because the legacy code only issued `AT+CREG?` and did not accept roaming status `5`, or timed out before 2G attach completed.
- **Fix**: Implemented unified `check_sim_registration()` polling all three 3GPP registration commands:
  - `AT+CREG?` (Circuit Switched 2G)
  - `AT+CGREG?` (Packet Data 2.5G GPRS)
  - `AT+CEREG?` (EPS 4G LTE)
- Accepts both Status `1` (Home) and Status `5` (Roaming).
- Sets `gi_sim_reg = 1` immediately on match so all downstream tasks proceed.

### 2. Dynamic APN Auto-Detection (`get_apn`)
- **Problem**: Karnataka State Natural Disaster Monitoring Centre (KSNDMC) uses Airtel IoT SIMs requiring `airteliot.com`, while retail SIMs require `airtelgprs.com`, BSNL requires `bsnlnet`, Jio requires `jionet`, and Vodafone requires `portalnmms`. Hardcoded APNs caused bearer activation failure.
- **Fix**:
  1. Queries SIM ICCID via `AT+ICCID` or `AT+CCID`. Detects Airtel IoT prefix `899116`, `899110`, or `899145` and sets APN to `airteliot.com`.
  2. Queries network operator via `AT+COPS?`:
     - BSNL / CellOne / MCC-MNC `40438` / `40453` -> `bsnlnet` (fallback `bsnlgprs.in`).
     - Reliance Jio / `4058` -> `jionet`.
     - Vodafone / Idea / Vi -> `portalnmms` / `www`.
     - Airtel Retail -> `airtelgprs.com`.
  3. Added `set_fallback_apn()` on bearer activation failure.

### 3. Bearer Activation Resilience (`connect_http1`)
- **Problem**: Giving `AT+NETOPEN` when the bearer was already open returned `+IP ERROR: Network is already opened`, causing `gprs.c` to report a failure and abort.
- **Fix**:
  - Issues `AT+NETOPEN?` first. If already open (`+NETOPEN: 1`), checks `AT+IPADDR` to verify active IP and reuses the existing bearer without error.
  - Added `AT&D0` so floating DTR lines do not tear down the PPP link.

### 4. HTTP Headers & Socket Cleanup (`http_send_data`)
- **Problem**: Hanging sockets on slot 0 caused subsequent posts to fail. Weak 2G signals failed DNS resolution for `rtdas.ksndmc.net`. HTTP server connections were kept alive by default, exhausting socket buffers.
- **Fix**:
  - Closes dangling socket 0 with `AT+CIPCLOSE=0` prior to establishing a connection.
  - Added `Connection: close\r\n` to all HTTP POST headers.
  - Added direct IP fallback (`117.216.42.181:80`) if DNS resolution fails.
  - Explicit socket close `AT+CIPCLOSE=0` following response receipt or timeout.

### 5. Periodic 2G Re-Check & NACK Assignment Fix (`vGprs_Task`)
- **Problem**: If 2G registration was slow on startup, `gi_sim_reg` remained 0 permanently. In addition, an assignment bug existed: `response_no == eNack;` (equality test instead of assignment).
- **Fix**:
  - Re-runs `check_sim_registration()` at the start of every 15-minute cycle if `gi_sim_reg == 0`.
  - Fixed typo `response_no == eNack;` to `response_no = eNack;`.
  - Clears `http_server_connected = 0` on transmission failure so bearer is cleanly renegotiated next cycle.

### 6. LPC845 Sleep Watchdog Accumulation Fix (`LPC845/main.c`)
- **Problem**: In `LPC845/main.c`, `time_since_reset_interval` was only reset to 0 upon receiving `$f` from LPC1768. During the 13–14 minutes of power-off sleep, `time_since_reset_interval` continued incrementing. When the 15-minute wake interval arrived (`minute % 15 == 0`), LPC845 turned on power to LPC1768 and SIM7600 without resetting the counter. After only 3–4 minutes, `time_since_reset_interval > 18*60*1000` tripped, triggering `NVIC_SystemReset()` and cutting power abruptly during network registration or transmission.
- **Fix**: Added `time_since_reset_interval = 0;` at line 1978 in `LPC845/main.c` inside `if((Present_Date_Time.minute % 15) == 0)` immediately upon turning on the power rails, guaranteeing the full 18-minute watchdog budget for LPC1768.

### 7. Dual RAT Mode & Airtel 4G Lockout Elimination (`check_sim_registration`)
- **Problem**: If network registration was not established by iteration 20 (~11 seconds), the code sent `AT+CNMP=13` to force GSM 2G mode. Airtel 4G LTE SIM cards often take 12–20 seconds to scan and attach; sending `AT+CNMP=13` aborted LTE attachment and permanently locked the SIM7600 modem into 2G in NVRAM.
- **Fix**: Removed forced `AT+CNMP=13` GSM lock. Uses `AT+CNMP=2` (Automatic RAT selection) allowing SIM7600 to seamlessly attach to either 4G LTE or 2G/3G without manual override. Added `AT+CGREG?` check alongside `CREG` and `CEREG` for BSNL packet data SIMs.

### 8. Airtel IoT APN Protection (`get_apn`)
- **Problem**: `get_apn()` correctly identified Airtel IoT ICCID prefixes (`899116`, etc.) and set `gc_apn_str = "airteliot.com"`. However, the subsequent `AT+COPS?` check matched "Airtel" and unconditionally overwrote `gc_apn_str` with retail `"airtelgprs.com"`, causing PDP authentication failure.
- **Fix**: Preserves `airteliot.com` if already identified by ICCID. Only defaults to `airtelgprs.com` if no IoT ICCID prefix was detected.

### 9. Socket PDP Context Configuration (`connect_http1`)
- **Problem**: SIM7600 TCP sockets require the socket stack PDP context (`AT+CGSOCKCONT`) to be set. Because `AT+CGDCONT` returned OK, `AT+CGSOCKCONT` was previously skipped, leaving the socket stack APN unconfigured. Furthermore, `Send_And_Check_Gprs` was called with `nack = "OK"`, so the immediate `\r\nOK\r\n` response to `AT+NETOPEN` was interpreted as failure.
- **Fix**: Explicitly configures both `AT+CGDCONT=1,"IP","%s"` and `AT+CGSOCKCONT=1,"IP","%s"`, binds profile 1 with `AT+CSOCKSETPN=1`, and correctly checks for `+NETOPEN: 0` with `nack = "ERROR"` (also accepting "already opened").

### 10. `AT+CIPSEND` Syntax & Prompt Scanner (`http_send_data_single`)
- **Problem**: The command was sent as `AT+CIPSEND=0,\r\n` with an illegal trailing comma. Furthermore, the `getline` prompt wait terminated on the first `\r\n`, causing HTTP payload bytes to be pushed before the modem issued the `>` prompt.
- **Fix**: Changed command to `AT+CIPSEND=0\r\n`. Implemented explicit circular buffer scanner checking byte-by-byte for `>` prompt before streaming data.

### 11. HTTP Response Parsing & Server Time Sync (`http_send_data_single`)
- **Problem**: Terminating on `200 OK` on the first line caused the loop to exit before parsing the `Date:` header or checking the response body for `Success` / `SUCCESS`.
- **Fix**: Tracks `got_200_ok = 1` while continuing to parse headers (updating RTC date/time from `Date:`) and body (`Success` / `SUCCESS`). If the server closes the connection with `got_200_ok == 1`, marks the transmission as successful.

### 12. HTTP Payload Dual-Param Formatting & Buffer Expansion
- **Problem**: KSNDMC endpoints expect `stn_no=...` while Spatika and others expect `stn_id=...`. `gc_http_data` was only 150 bytes in `Example01/memory.h`, risking overflow.
- **Fix**: Expanded `gc_http_data` to 256 bytes in `Example01/memory.h`. Formats both `stn_no=%s&stn_id=%s` in both `prepare_http_record` and `prepare_present_rec_for_http_sms`.

---

## 7. Anti-Patterns & Failed Approaches (DO NOT REPEAT)

| Mistake / Failed Approach | Why It Failed | What Must Be Done Instead |
| :--- | :--- | :--- |
| **Relinking application to `ORIGIN = 0x00000000`** | Application is ~138 KB. Starting at `0x0` collides with Sector 1 (`PARAM_SECTOR`) and Sectors 2–8 (`days_sector`). Writing IAP logs erases application code! | Keep application at `0x00018000` (Sector 17). Use the Sector 0 Boot Stub at `0x00000000`. |
| **Flashing application at `0x00018000` without Sector 0 Boot Stub** | LPC1768 hardware ROM only looks at `0x00000000` on reset. Without a bootloader or stub, CPU never jumps to `0x18000`. | Always flash `LPC1768_TWS_FULL.hex`, which includes the boot stub. |
| **Missing Vector 7 Checksum at offset `0x1C`** | NXP LPC microcontrollers require `Vector 7 = 0 - sum(Vector[0..6])`. If invalid, the ROM halts or enters ISP boot mode. | Always run `scripts/lpc_checksum.py` on the binary. |
| **SWD wire speed at 1 MHz (`1000000`)** | Long ribbon cables / PCB trace capacitance cause `Wire ACK Fault` errors on LPC1768 during mass-erase and verification. | Always use `wirespeed: 100000` (100 kHz) in `lpc1768-mculink.json`. |
| **Using `vectreset` on LPC1768 during mass-erase** | `vectreset` only resets the core. Clocks/peripherals remain locked, leading to `Debug port inaccessible at 0x10000000`. | Always configure `connect-reset: system` and `flash-driver-reset: system` (`SYSRESETREQ`). |
| **Mismatched sensor protocol between LPC845 and LPC1768** | Newer `source_v2` LPC845 code used CSV `$D,%u,%u...` while LPC1768 `sensor.c` strictly parses classic `$D%02d%02d%02d...`. | Always build LPC845 using recovered `LPC845/main.c`. |
| **Hardcoding APN to `airtelgprs.com`** | Government/IoT SIM cards on KSNDMC stations use `airteliot.com`. A retail APN causes bearer rejection. | Always use the dynamic ICCID inspection in `get_apn()`. |
| **Sending `AT+CNMP=13` (GSM only) during registration** | Kills Airtel 4G LTE registration; SIM7600 stays locked in 2G mode permanently across reboots. | Always use `AT+CNMP=2` (Auto RAT selection) and check `CREG`, `CGREG`, and `CEREG`. |
| **Omitting `time_since_reset_interval = 0` on 15-min wake** | LPC845 watchdog timer accumulates 14 minutes during sleep, resetting the system after only 3–4 minutes. | Reset `time_since_reset_interval = 0;` at `minute % 15 == 0` when power rails are switched on. |
| **Passing `"OK"` as `nack` parameter in `AT+NETOPEN`** | `AT+NETOPEN` responds `OK` immediately before sending `+NETOPEN: 0`. Passing `"OK"` as `nack` falsely triggers error handling. | Pass `nack = "ERROR"` and check for `+NETOPEN: 0` or `"already opened"`. |
| **Formatting `AT+CIPSEND=0,\r\n` with trailing comma** | SIM7600 returns `ERROR` on trailing comma syntax when no length parameter is provided. | Use `AT+CIPSEND=0\r\n` without trailing comma. |
| **Hardcoding non-existent fallback IPs (`144.91.104.105:80`) for KSNDMC** | Hangs HTTP transmission for 25 seconds on Airtel IoT private APNs. | Use KSNDMC primary domain name first, then fallback to `117.216.42.181:80`. |

