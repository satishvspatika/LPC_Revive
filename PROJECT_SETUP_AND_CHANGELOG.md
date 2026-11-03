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
├── get_git.sh                      <-- 1-click pull/sync or checkout specific -tag
├── push_git.sh                     <-- 1-click commit, tag and push (-tag "<tag>")
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

## 5. Multi-Machine Git Sync Tools (`push_git.sh` & `get_git.sh`)

To effortlessly synchronize changes between your development laptop and office computer:

### Pushing Changes with a Tag (`push_git.sh`)
```bash
# Push with a tag
./push_git.sh -tag "v1.0.1"

# Or push with a tag and a custom commit message
./push_git.sh -tag "v1.0.1" -m "Fixed HTTP socket prompt and 4G APN handling"
```
*Action*: Automatically stages modified files, creates a commit, creates an annotated Git tag, and pushes both the branch and the tag to GitHub (`satishvspatika/LPC_Revive`).

### Retrieving on Another Computer (`get_git.sh`)
```bash
# 1. Retrieve the latest updates on the current branch:
./get_git.sh

# 2. Or retrieve and checkout a specific tagged release:
./get_git.sh -tag "v1.0.1"

# 3. List all tags available in the repository:
./get_git.sh -list
```
*Action*: Fetches all remote updates and tags. If a `-tag` is given, cleanly checks out that exact tagged snapshot.

---

## 6. LPC1768 Memory Architecture & Standalone Boot Stub

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

### 13. HTTP Flow Hardening & Buffer Corruption Elimination (`v1.0.3`)
- **Problem**:
  1. In `http_send_data_single()`, `dest_in_count` was initialized to 0 outside the response receiving loop, but was never reset inside `if (lf_recd)`. As HTTP headers (Status, Date, Server, Content-Type, etc.) were received line-by-line, each line was appended to `http_response[300]`. Once the total character count exceeded 300 bytes, `getline()` wrapped around (`(loc_in_count_dest + 1) % 300`) and corrupted earlier characters in `http_response`, causing subsequent string searches (`Date:`, `Success`, `200 OK`) to fail or match garbage.
  2. `vWDT_Task` checks task heartbeats every 10 seconds. In weak cellular conditions, TCP opening (`http_open_tcp`) and HTTP response waits (`http_send_data_single`) could wait up to 25–30 seconds without feeding `gi_wdt_task_ok_stat |= WDT_GPRS_OK;`, risking watchdog reset.
  3. `http_data` was a 512-byte local array on the FreeRTOS task stack. `vGprs_Task` stack is 1300 words; allocating 512 bytes on the stack left little headroom and risked data corruption while the asynchronous UART THRE ISR streamed bytes.
  4. Transmission of `http_data` via UART2 THRE interrupt takes ~35 ms at 115200 baud for a 400-byte POST request. The original code waited only 20 ms before reading responses, risking reading modem echo as response data.
  5. If `AT+IPADDR` failed during `connect_http1()`, the bearer was left dangling, locking subsequent `AT+NETOPEN` commands into `+IP ERROR: Network is already opened`.
- **Fix**:
  1. Added `dest_in_count = 0; http_response[0] = 0;` inside `if (lf_recd)` immediately after processing each line. Each received line is now parsed and logged independently without buffer accumulation or wrapping.
  2. Added periodic `gi_wdt_task_ok_stat |= WDT_GPRS_OK;` feeds every 1 second (100 ticks) in both `http_open_tcp()` and `http_send_data_single()` wait loops.
  3. Made `http_data` `static char http_data[1024];` in BSS memory, saving stack space and guaranteeing memory validity throughout ISR streaming.
  4. Added `while (*gprs_xmit_ptr && tx_wait > 0)` wait loop after `UART_SendData(...)` to guarantee all bytes and `0x1A` (Ctrl-Z) are loaded and flushed by the UART hardware before awaiting the modem's response.
  5. Added defensive path check ensuring `formatted_path` begins with `/` for strict HTTP/1.1 RFC compliance.
  6. Added case-insensitive `"success"` check alongside `"Success"` and `"SUCCESS"`.
  7. Added `Send_And_Check_Gprs("AT+NETCLOSE\r\n", "OK", "ERROR", 2000);` if `AT+IPADDR` fails in `connect_http1()`.

### 14. ESP32 Reference Alignment: KSNDMC_TWS (SYSTEM 1) Integration (`v1.0.4`)
- **Problem**:
  1. In `Example01/memory.c`, the default TWS profile was hardcoded to `rtdas.spatika.net`, `/tws_gprs/update_tws_data_v2`, and key `climate4p2013`. However, the authoritative ESP32 reference for `SYSTEM == 1` with `UNIT_CFG == KSNDMC_TWS` targets the Karnataka Government AWS server `rtdas.ksndmc.net` (direct IP `117.216.42.181:80`), URL path `/tws_gprs/update_tws_data_v3`, and key `climate4pTWS`.
  2. Legacy server-side parsers at KSNDMC are strict fixed-column/fixed-key parsers. Including both `stn_no` and `stn_id` caused key-count mismatches. Furthermore, temperature and humidity lacked width formatting (`%05.1f`), causing column shifts on values < 10.0 or > 99.9.
  3. Aggregate servers strictly reject packets received less than 62 seconds after the 15-minute slot boundary with a `"Rejected"` response. Fast-booting stations that finish network connect in 30 seconds were consistently rejected.
  4. Asynchronous `+CGEV` (GPRS event) URCs from the cellular modem during data transfers polluted the circular buffer and corrupted `AT+CIPOPEN` and `AT+CIPSEND` response parsing.
  5. Consecutive backlog transfers with only 200 ms spacing triggered carrier NAT throttling (`TIME_WAIT` RSTs).
- **Fix**:
  1. Aligned `Global_Data_Params` defaults in `Example01/memory.c`:
     - Server: `rtdas.ksndmc.net` (Direct IP: `117.216.42.181:80`)
     - Path: `/tws_gprs/update_tws_data_v3`
     - Key: `climate4pTWS`
  2. Updated `gc_version` and `new_version` to `TWS6-1.0.4` so LPC1768 automatically refreshes Flash Sector 1 on boot while preserving the station ID.
  3. Implemented exact ESP32 `SYSTEM == 1` payload formatting:
     `stn_no=%s&rec_time=%04d-%02d-%02d,%02d:%02d&temp=%s&humid=%s&w_speed=%s&w_dir=%s&signal=%d&bat_volt=%s&key=%s`
     with `%05.1f` for temp/humid, `%04.1f` for wind speed, `%03d` for wind direction, and `%05.2f` for battery.
  4. Implemented the **62-Second Slot Boundary Guard** in `vGprs_Task`: checks `(minute % 15) * 60 + second` and holds until `T+1m02s` (feeding watchdog) before initiating HTTP connection.
  5. Added `AT+CGEREP=0` to `check_basic_commands()` to silence carrier URC noise.
  6. Enforced a **2500 ms carrier breather** between historical backlog transfers in `process_http_backlogs()`.

### 15. Cross-System Script Isolation via `.gitignore` & Snapshot Restoration (`v1.0.5`)
- **Problem**:
  1. Transferring code between development environments (e.g. office workstation vs laptop) via Git caused `get_git.sh` and `push_git.sh` to overwrite local host configurations or cause merge conflicts during `git pull` and `git checkout`.
  2. Because older tags (`v1.0.1`–`v1.0.4`) tracked `get_git.sh` and `push_git.sh` in Git history, switching to an older tag aborted with: `error: The following untracked working tree files would be overwritten by checkout: get_git.sh push_git.sh`.
  3. Returning to `main` from a tag deleted the scripts from the working directory because they were removed from tracking on `main`.
- **Fix**:
  1. Added `get_git.sh` and `push_git.sh` to `.gitignore` so they remain untracked and never get staged, committed, or pulled over between systems.
  2. Implemented pre-operation snapshotting into `.git/local_scripts_backup/` with an automated `trap restore_local_scripts EXIT` in both scripts to guarantee local preservation under all conditions.
  3. Handled historical tag transitions: automatically avoids untracked collision before `git checkout tags/<tag>`, restores local scripts, and marks them `--skip-worktree`. Switching back to `main` restores scripts cleanly and keeps working directory clean.
  4. Wrapped both scripts in bash `main() { ... }; main "$@"` so the interpreter loads the entire script in memory, eliminating EOF/offset corruption if disk files are modified mid-run.

### 16. Standalone Toolchain Auto-Detection & `yr` Declaration Fix (`v1.0.6`)
- **Problem**:
  1. Running `./build_all.sh` on machines without MCUXpresso IDE installed exited with error 127 (`arm-none-eabi-gcc not found`), preventing local compilation verification before pushing commits.
  2. In `Example01/gprs.c:prepare_present_rec_for_http_sms()`, `yr` was used without declaration, breaking compilation.
- **Fix**:
  1. Declared and sanitized `int yr = (xPresent_Date_Time.year >= 2000) ? xPresent_Date_Time.year : (xPresent_Date_Time.year > 0 ? (2000 + xPresent_Date_Time.year) : 2026);` in `Example01/gprs.c`.
  2. Added workspace `tools/arm-toolchain/bin/arm-none-eabi-gcc` auto-detection to `Example01/build.sh` and `LPC845/build.sh` so full C compilation verification runs natively on any machine.
  3. Added `tools/` to `.gitignore` so local compilers remain excluded from Git tracking.

### 17. RamLoc32 `.bss` Overflow Fix & AHB SRAM Bank 1 Relocation (`v1.0.7`)
- **Problem**:
  1. LPC1768 local RAM (`RamLoc32`) is strictly 32 KB (`0x10000000`–`0x10007FFF`, 32,768 bytes). FreeRTOS heap (`configTOTAL_HEAP_SIZE` = 16 KB) plus global structs (`Fatfs`, `File_Sd`, `Global_Data_Params`, etc.) left only 4 bytes of free space in the recovered codebase.
  2. In `v1.0.3`, `static char http_data[1024]` was introduced into `Example01/gprs.c`, and `gc_http_data` was sized to 256 bytes. When linking with MCUXpresso IDE, the linker failed with:
     ```
     Example01.axf section '.bss' will not fit in region 'RamLoc32'
     region 'RamLoc32' overflowed by 1124 bytes
     ```
  3. Furthermore, missing standard header inclusions (`<stdio.h>`, `"lpc17xx_gpio.h"`, `<string.h>`, `"ff.h"`) generated dozens of implicit declaration warnings for `sprintf`, `sscanf`, `GPIO_ClearValue`, `GPIO_SetValue`, `GPIO_SetDir`, `f_unlink`, etc.
- **Fix**:
  1. Relocated `http_data` in `http_send_data_single()` to dedicated LPC1768 AHB SRAM Bank 1 (`0x20080000`), a 16 KB unreserved on-chip hardware SRAM bank (`LPC_AHBRAM1_BASE`). This completely removed 1024 bytes from `RamLoc32` `.bss` while keeping the FreeRTOS task stack unaffected.
  2. Relocated `ftp_buff` to AHB SRAM Bank 1 (`0x20080800`), removing another 1064 bytes from `RamLoc32` `.bss`.
  3. Sized `gc_http_data` back to 150 bytes in `Example01/memory.h` (matching its original footprint and comfortably accommodating the ~127-byte HTTP payload).
  4. Added explicit zero-termination for both AHB Bank 1 buffers in `vGprs_Task` (`*((char *)0x20080000) = 0; *((char *)0x20080800) = 0;`).
  5. Total RAM freed from `RamLoc32`: 2,194 bytes, eliminating the 1124-byte overflow and providing **1,070 bytes of clean free headroom** in `RamLoc32`.
  6. Added all missing header includes and declarations (`<stdio.h>`, `"lpc17xx_gpio.h"`, `<string.h>`, `"ff.h"`, `mark_http_sample_sent_in_flash()`), fixed `sim_holder[8]` buffer overflow in `get_gps_info()`, and fixed `mv2endof_str()` pointer return (`s = 0`). All source files now compile with zero errors and zero warnings.


### 18. SIM7600 Direct Socket `AT+CIPSEND` Byte Length & Payload Framing (`v1.0.8`)
- **Problem**:
  1. In field tests with `v1.0.7`, the modem successfully opened the network bearer (`NETOPEN OK`), obtained IP `172.19.147.122`, and connected TCP to KSNDMC direct IP (`117.216.42.181:80` `+CIPOPEN: 0,0 OK`).
  2. However, transmission immediately aborted with `HTTP: No > prompt received from modem!`.
  3. Analysis of SIM7600 AT Command Specification revealed that SIM7600 direct socket data mode (`AT+CIPMODE=0`) strictly requires an explicit length argument: `AT+CIPSEND=<link_num>,<length>\r\n`. Issuing `AT+CIPSEND=0` without length causes SIM7600 to reject the command with `ERROR` instead of returning the `>` prompt.
  4. Additionally, legacy 2G firmware appended `\032` (Ctrl+Z, 0x1A) to terminate transmission. In fixed-length direct socket mode, appending `\032` adds an extraneous invalid character to the HTTP payload and corrupts socket framing.
- **Fix**:
  1. Refactored `http_send_data_single()` in `Example01/gprs.c`: formatted the entire HTTP request headers and payload into AHB SRAM Bank 1 (`0x20080000`) upfront using safe `snprintf` bounds checking.
  2. Calculated total transmission byte length `rawLen = strlen(http_data)`.
  3. Eliminated the trailing `\032` (0x1A) Ctrl+Z termination.
  4. Issued `AT+CIPSEND=0,<rawLen>\r\n`. With the length parameter specified, SIM7600 immediately issues the `>` prompt (< 5 ms).
  5. Streamed exactly `rawLen` bytes of HTTP request over UART upon detecting `>` prompt.
  6. Updated firmware version strings in `Example01/memory.c` (`gc_version` and `new_version`) to `"TWS6-1.0.8"`.

### 19. Comprehensive Telemetry Diagnostic Logging & Firmware Version Display (`v1.0.9`)
- **Problem**:
  1. Field logs in `httplog.txt` lacked granular insight: they did not identify the running firmware version, did not explain why `CIPOPEN` failed, did not capture what the modem returned if `>` prompt timed out, and truncated response logs without detailed HTTP status code breakdown.
  2. `log_tmp` in `http_send_data_single()` was 160 bytes, risking stack buffer overflow when logging server response lines up to 300 bytes.
- **Fix**:
  1. **Full Telemetry Banner**: Added Station ID, Firmware Version (`TWS6-1.0.9`), Hardware Model (`TWS-DMC`), Operator, Signal level (dBm), Registration status, APN, and Primary/Secondary endpoint URLs at the start of every 15-minute transmission slot.
  2. **CIPOPEN Error Translation**: Added human-readable error descriptions for all SIM7600 `+CIPOPEN: 0,<err>` rejection codes (e.g., Code 1: Network error / bearer not ready, Code 2: TCP connect rejected / server unreachable, Code 3: DNS resolution failed, Code 5: Socket in use).
  3. **Prompt Diagnostics**: Expanded `prompt_rx[100]` capture buffer to log the raw modem reply and exact wait duration in ms if `>` prompt fails.
  4. **UART TX & Timing**: Logged exact payload byte size, prompt reception time, and UART shift completion time in milliseconds.
  5. **Response Sanitization & HTTP Code Breakdown**: Increased `log_tmp` to 320 bytes to prevent stack overflow. Trimmed trailing whitespace/newlines, logged all received lines (`HTTP: Rcvd <- ...`), and added explicit status detection for `200 OK`, `400 Bad Request`, `403 Forbidden`, `404 Not Found`, and `500 Server Error`.
  6. **Body Validation & Flash Tracking**: Logged validation of `SUCCESS`, `REJECTED`, and `INVALID`, remote socket closure (`+CIPCLOSE: 0`), and sample marking confirmation (`HTTP: Present sample marked SENT in Flash`).
  7. Bumped firmware version strings to `TWS6-1.0.9`.

### 20. SWD Wire Speed Signal Integrity & `flash_lpc1768.sh` Verification Fix (`v1.0.9`)
- **Problem**:
  1. During flashing on developer laptop, LinkServer mass-erased the chip, programmed Sector 0 (56 bytes), but crashed at 38% while writing the Application at `0x18000`:
     ```text
     Nc: 00025000 done  38% (53248 out of 137732)
     Nc: request to clear DAP error failed - status 131
     Nc: After error Ee(83). Hardware Transfer Error -
     Pb: (100) Writing Flash ended with an error.
     Ed:05: File 'tmpc3vqf1di.elf' load failure: Em(17). Debug port inaccessible
     ```
  2. Because `flash_lpc1768.sh` checked `grep -q "Sectors written:"` (which matched `Sectors written: 5`), the script falsely reported `Flash write confirmed OK` even though the write aborted at 38%, leaving corrupted/partial firmware on the target.
  3. The root cause was `"wirespeed": 500000` (500 kHz) in `scripts/lpc1768-mculink.json`. High SWD clock frequency over ribbon cables and probe capacitance causes CoreSight DAP transfer errors under sustained writes.
- **Fix**:
  1. Reduced `wirespeed` in `scripts/lpc1768-mculink.json` from `500000` to `100000` (100 kHz), providing noise immunity and stable SWD communications across ribbon cables.
  2. Updated `flash_lpc1768.sh` to strictly verify `Finished writing Flash successfully` and abort with an explicit error code on `Writing Flash ended with an error`, `Hardware Transfer Error`, or `Could not connect to core`.

### 21. Non-Volatile Station ID & Parameter Protection (`v1.0.10`)
- **Problem**:
  1. Whenever `./flash_lpc1768.sh` was executed, Step 1 ran a full chip mass-erase: `LinkServer flash ... erase`.
  2. In LPC1768 memory layout:
     - Sector 0 (`0x00000000` - `0x00000FFF`): Sector 0 Boot Stub
     - Sector 1 (`0x00001000` - `0x00001FFF`): `PARAM_SECTOR` (EEPROM-emulated storage for Station ID, station name, GSM numbers, server endpoints, and calibration values)
     - Sectors 16..29 (`0x00018000`+): Application Firmware
  3. Unconditional mass-erase wiped Sector 1, erasing the on-device Station ID (e.g. `001922`).
  4. On initial boot after an erase, `memory.c` fell back to `params_default()`, which contained legacy dummy ID `"001060"`, forcing the user to manually re-enter their station ID every time they flashed.
- **Fix**:
  1. **Safe Flashing Without Mass Erase**: Updated `flash_lpc1768.sh` to eliminate the automatic `erase` step. LinkServer `load LPC1768_TWS_FULL.hex` erases only the sectors required by the payload (Sector 0 and Sectors 16+). Sector 1 is preserved intact. Added optional `--mass-erase` / `--erase-all` flags for when full wipe is explicitly desired.
  2. **Updated Fallback Default Station ID**: Updated `params_default()` in `memory.c` from `"001060"` to `"001922"` (for both `CFG_KSNDMC_TWS` and default `CFG_KSNDMC_TRG`).
  3. **Guarded Parameter Restoration**: Added validation in `memory.c` so parameter sector reinitialization will never overwrite a valid station ID with uninitialized memory if `gi_log_interval > 31`.
  4. Bumped firmware version strings to `TWS6-1.0.10`.

### 22. Dynamic MCUXpresso Redlib Auto-Detection & HEX Re-linking (`v1.0.11`)
- **Problem**:
  1. In `Example01/build.sh` and `LPC845/build.sh`, the MCUXpresso IDE tools path was hardcoded to a specific timestamped directory (`MCUXpressoIDE_24.9.25/ide/plugins/com.nxp.mcuxpresso.tools.macosx_24.9.0.202407110857/tools/arm-none-eabi/lib/thumb/v7-m/nofp`).
  2. When the user executed `./build_all.sh` on their Mac, the hardcoded directory did not match their installation, causing `build.sh` to compile all `.c` files to `.o` but **skip Step 2 (Linking)** and reuse the old `LPC1768_TWS_FULL.hex` from initial commit `d51b5f9` (which had `TWS6-1.0.3`).
  3. When the user flashed, the board ran the old `TWS6-1.0.3` firmware:
     - `1.0.3` had `AT+CNMP=13` (force 2G GSM), failing on Airtel 4G SIMs.
     - With registration failing, the modem NETLIGHT / GPRS LED kept blinking fast continuously.
     - `httplog.txt` was never generated because the updated telemetry logging code was never actually linked or flashed.
- **Fix**:
  1. Replaced hardcoded paths in `Example01/build.sh` and `LPC845/build.sh` with dynamic search using wildcards (`/Applications/MCUXpresso*`) and relative path resolution from `arm-none-eabi-gcc`.
  2. Auto-detects Redlib (`libcr_c.a`, `libcr_eabihelpers.a`) for both Cortex-M3 (`thumb/v7-m/nofp`) and Cortex-M0+ (`thumb/v6-m/nofp`).
  3. Prompts the developer clearly if MCUXpresso Redlib libraries are missing on the build host.

### 23. Socket Handshake Race Condition & Virtual Host Header Resolution (`v1.0.12`)
- **Problem**:
  1. In `http_open_tcp()`, before issuing `AT+CIPOPEN=0`, `gprs_recv_out_cnt` was not synchronized to `gprs_recv_in_cnt`. Residual `ERROR` strings emitted by preliminary `AT+CIPCLOSE=0` remained in the circular buffer, causing `http_open_tcp()` to read stale `ERROR` in 0 ms and falsely report `HTTP: CIPOPEN returned ERROR`.
  2. When Attempt 2 (Direct IP `117.216.42.181:80`) executed, `AT+CIPSEND=0,287` was dispatched before SIM7600 completed the asynchronous 3-way TCP handshake over LTE. The modem returned `+CIPERROR: 4` (*Socket not ready*), followed immediately by asynchronous `+CIPOPEN: 0,0`, causing Attempt 2 to fail and fall through to Attempt 3.
  3. In `http_send_data_single()`, the `Host:` header check mapped both `117.216.42.181` and `144.91.104.105` to `gc_http_server_name1` (`rtdas.ksndmc.net`), which would have sent an incorrect virtual host header to the Spatika server.
- **Fix**:
  1. **Circular Buffer Flush**: Flushed `gprs_recv_out_cnt = gprs_recv_in_cnt;` immediately prior to issuing `AT+CIPOPEN=0` in `http_open_tcp()`, and cleared residual bytes after socket close errors.
  2. **Socket Settling Delay**: Added a 150 ms settling delay upon receiving `+CIPOPEN: 0,0` before allowing `http_send_data_single()` to send `AT+CIPSEND=0,<len>`, eliminating the `+CIPERROR: 4` socket busy race condition.
  3. **Accurate Virtual Host Mapping**: Mapped `117.216.42.181` to `gc_http_server_name1` (`rtdas.ksndmc.net`) and `144.91.104.105` to `gc_http_server_name2` (`rtdas.spatika.net`).
  4. Bumped firmware version strings to `TWS6-1.0.12`.

### 24. Power-Down, NETLIGHT LED Shutoff & Sleep Rails Fix (`v1.0.13`)
- **Problem**:
  1. **Continuous GPRS / NETLIGHT LED Blinking After HTTP Transmission**: The SIM7600 NETLIGHT LED kept blinking continuously even after HTTP telemetry transmission completed, draining battery power.
  2. **Legacy FTP Loops After HTTP**: In `Example01/gprs.c`, after `http_status == 1`, the code unconditionally entered a legacy FTP retry loop for older samples in `days_sector`, attempting `connect_ftp1()` to `ftp1.ksndmc.net`. Each failed FTP connection hung for 70s on `AT+CFTPSSTART` and 45s on `AT+CFTPSLOGIN`.
  3. **PDP Bearer Left Active**: `AT+NETCLOSE` was never issued after HTTP telemetry completed, leaving the cellular PDP context active.
  4. **Modem Baseband Power-Down (`AT+CPOF`) Never Executed**: The SIM7600 baseband was never commanded to shut down, so the modem remained fully powered and RF transmission/LED blinking continued indefinitely.
  5. **Deadlock on `gUi_Active` Blocking Sleep & Power Rail Shutdown**:
     - In `Example01/memory.c`, `if (sleep_mode == 1)` checked `if (!gUi_Active) system_enter_sleep(); else shutdown_pwr = 1;`. If the LCD had booted or a key was touched, `gUi_Active` was 1, so `system_enter_sleep()` was skipped.
     - In `Example01/sensor.c:514`, `if (shutdown_pwr == 1 && !gUi_Active)` also blocked sending `$f7;\012` to LPC845 when `gUi_Active` was set. As a result, the LPC845 power rails (LPC1768 MCU, SIM7600, LCD) were never cut!
     - In `Example01/ui.c:1102`, the 60s inactivity timeout only toggled the LCD display without triggering sleep.
- **Fix**:
  1. **Bypass Legacy FTP on HTTP Success**: In `Example01/gprs.c`, when `http_status == 1`, set `update_present_day_flag = 1;` and bypass all legacy FTP loops.
  2. **Graceful Bearer Teardown & Modem Baseband Shutoff**: Immediately upon telemetry completion, execute `AT+CIPCLOSE=0`, `AT+NETCLOSE` (`+NETCLOSE: 0`), and `AT+CPOF` to shut down the SIM7600 baseband. This instantly turns off the NETLIGHT LED and saves power.
  3. **Unconditional Sleep & Power Rail Cut**:
     - In `Example01/memory.c`, when `sleep_mode == 1`, unconditionally invoke `system_enter_sleep()`.
     - In `Example01/sensor.c`, removed the `!gUi_Active` gate so `shutdown_pwr == 1` immediately dispatches `$f7;\012` to LPC845 to cut power rails.
     - In `Example01/ui.c`, updated the 60-second inactivity timeout to call `system_enter_sleep()` directly.
  4. Bumped firmware version strings to `TWS6-1.0.13`.

### 25. Complete Elimination of Legacy 2013 FTP Fallback on Failed HTTP & Standby Slot Clarification (`v1.0.14`)
- **Problem**:
  1. In `v1.0.13`, if an HTTP telemetry transmission failed (`http_status == 0`), the code fell into an `else` branch containing an obsolete 2013 FTP fallback loop that attempted `connect_ftp1()` to `ftp1.ksndmc.net`.
  2. Because `ftp1.ksndmc.net` is unreachable, `AT+CFTPSSTART` waited 70 seconds and `AT+CFTPSLOGIN` waited 45 seconds (over 2 minutes wasted).
  3. During this delay, the modem remained powered with the `NETLIGHT` LED pulsing, and if a timeout occurred or the task hung, modem power-down (`AT+CPOF`) and sleep entry were postponed or missed entirely.
  4. In KSNDMC TWS stations, telemetry is 100% HTTP-based. Any unsent record remains stored in Flash and is automatically picked up and sent by `process_http_backlogs()` on subsequent 15-minute slots. An FTP fallback for 15-min data is completely redundant.
- **Fix**:
  1. **Purged Legacy FTP Loop**: Completely removed the 158-line FTP fallback block from `vGprs_Task` in `Example01/gprs.c`.
  2. **Deterministic Modem Power-Down & Sleep**: Regardless of whether HTTP transmission succeeds or fails, the firmware now deterministically executes `AT+CIPCLOSE=0` -> `AT+NETCLOSE` -> `AT+CPOF` to power off the SIM7600 RF engine (shutting off the `NETLIGHT` LED) and immediately triggers `sleep_mode = 1` -> `system_enter_sleep()` to cut power rails via the LPC845.
  3. Bumped firmware version strings to `TWS6-1.0.14`.

### 26. Standalone GNU Toolchain Linking, LCD Unfreeze, Keypad Debounce & Power Flow (`v1.0.16`)
- **Problem**:
  1. **Stale Firmware Flashing (The Root Cause)**: In `Example01/build.sh`, the link stage was strictly conditional on finding NXP MCUXpresso Redlib (`libcr_c.a`). If Redlib was not installed at `/Applications/MCUXpressoIDE*`, `build.sh` silently skipped linking and exited with code 0! Consequently, all builds since Nov 2 reused stale `LPC1768_TWS_FULL.hex` from initial commit `d51b5f9` (`TWS6-1.0.3`), and none of the code changes (HTTP fixes, power-off fixes, LCD unfreezes) were ever actually flashed to the board.
  2. **LCD Stuck at `"VARSHA-TWS INITIALIZING.."`**: `wait_lcd()` in `lcd.c` polled `LCD_BUSY_FLAG` (P2.8) in an infinite loop without performing the dual EN pulse required on 4-bit bus mode. Because the pin floated high due to internal pull-ups, the LCD task hung permanently on boot.
  3. **Unresponsive Keypad**:
     - `iDebounce_Count` was set to 4 cycles (~52 ms debounce window), making rapid or short key taps register as unacknowledged noise.
     - Keypad row pins (`Row0`, `Row1`, `Row2`) were not explicitly initialized HIGH on boot.
     - In `vUi_Task` (`ui.c`), the 60-second inactivity timeout called `system_enter_sleep()`. This set `shutdown_pwr = 1` and spun in `while (shutdown_pwr == 1)`, locking `vKbd_Task` and `vUi_Task` in an infinite while loop and tristating keypad GPIO pins.
  4. **Modem NETLIGHT Running Continuously**:
     - If the user was in the UI when telemetry finished, sleep was skipped, leaving SIM7600 powered with NETLIGHT blinking.
- **Fix**:
  1. **Standalone GNU Linking Enabled**: Created `Example01/Debug/example01_gnu.ld` and `Example01/Debug/example01_gnu_lib.ld`. Configured `build.sh` to link seamlessly using GNU Arm Embedded toolchain (`--specs=nano.specs --specs=nosys.specs -lm`) whenever Redlib is absent. Fresh `LPC1768_TWS_FULL.hex` and `LPC1768_TWS_FULL.bin` are now compiled, linked, checksummed, and merged every time `./build_all.sh` is executed.
  2. **LCD Busy Flag Deadlock Eliminated**: Replaced busy-flag polling in `wait_lcd()` with a calibrated, non-blocking delay (~50 µs). Hardwired `LCD_RW_MASK` low (`0` = write mode) to eliminate bus contention. Splash screen transitions cleanly to `SCR_STATION` after 1.5 seconds.
  3. **Keypad Responsiveness Restored**:
     - Reduced `iDebounce_Count` from 4 to 2 cycles (~26 ms) for instantaneous key response.
     - Initialized row pins HIGH in `init_kbd_pins()`.
     - Replaced `system_enter_sleep()` on 60s UI inactivity with `sleep_display()`, turning off only the LCD backlight to conserve power without freezing tasks or locking the keypad.
  4. **Independent Modem Power-Down**: Guarded modem shutdown so SIM7600 powers off (`AT+CPOF` turns off NETLIGHT) even if the operator is actively interacting with the UI.
  5. Bumped firmware version strings to `TWS6-1.0.16`.

### 27. Instant Boot, Non-Locking Keypad, and KSNDMC IP-First Telemetry (v1.0.17)
- **Problem**:
  1. **LCD Boot Delay (~8s)**:
     - `main.c` had a blocking 4000x10000 crude loop that burned ~2 seconds before FreeRTOS started.
     - `vUi_Task` waited up to 2.5s (`boot_wait`) polling for an LPC845 `$D` packet, and then waited 1.5s on the splash screen.
  2. **Unresponsive Keypad**:
     - `Kbd_Scan()` executed `vTaskDelay(1)` for each of the 3 matrix rows, triggering context switches and taking ~3–30ms per scan.
     - When background telemetry finished (~15s after boot), `memory.c` checked `if (!gUi_Active)` and called `system_enter_sleep()`.
     - `system_enter_sleep()` set `shutdown_pwr = 1` and called `kbd_release_pins_for_sleep()`, tri-stating all keypad pins into High-Z.
     - `vKbd_Task` entered `while (shutdown_pwr == 1)`. On bench/USB power where the LPC845 never physically cut power, the keypad was permanently frozen!
  3. **KSNDMC Server Endpoint Mismatch**:
     - The telemetry loop included an Attempt 3 fallback to `rtdas.spatika.net:80/tws_gprs/update_tws_data_v2`.
     - Per the authoritative ESP32 reference (`httpSet[6]`), KSNDMC TWS stations transmit exclusively to KSNDMC servers (`117.216.42.181` / `rtdas.ksndmc.net`), with Direct IP first and zero Spatika backup.
     - `Host:` header rewriting sent `Host: rtdas.ksndmc.net` when connecting to `117.216.42.181`, causing the server to close the socket with `+IPCLOSE: 0,1`.
     - The response parser did not match `+IPCLOSE: 0`, causing the modem loop to hang for 30s before reporting timeout.
  4. **SD Card File Handle Leak in `check_sdc_status()`**:
     - If "Welcome\r\n" was matched, the loop executed `break;` without calling `f_close(&File_Sd)`.
- **Fix**:
  1. **Instant LCD Boot (<400ms)**:
     - Replaced crude loop in `main.c` with a fast ~20ms settling delay.
     - Removed the 2.5-second `boot_wait` in `vUi_Task`; display initializes immediately and splash screen displays for 300ms before showing `SCR_STATION`.
  2. **Continuous Keypad Scanning with Zero Deadlock**:
     - Replaced `vTaskDelay(1)` in `Kbd_Scan()` with a fast ~10 µs pin settling loop (`d < 300`).
     - Removed `shutdown_pwr` gating and `kbd_release_pins_for_sleep()` from `vKbd_Task`. Keypad matrix is continuously scanned every 20ms and never tri-stated.
     - Instant tap detection: key transitions immediately post `key_value` and `sig_key = 1` to UI.
     - Completely eliminated automatic `system_enter_sleep()` from `memory.c`. Telemetry completion turns off the modem (`AT+CPOF`) and NETLIGHT LED, but keeps LPC1768 UI and keypad running.
  3. **KSNDMC Direct IP First & Standard RFC 7230 Host Header**:
     - Attempt 1: Direct IP (`117.216.42.181:80`) with `Host: 117.216.42.181`.
     - Attempt 2: Domain fallback (`rtdas.ksndmc.net:80`) with `Host: rtdas.ksndmc.net`.
     - Both point to `/tws_gprs/update_tws_data_v3` with key `climate4pTWS`. Spatika backup removed for KSNDMC profiles.
     - Added immediate `+IPCLOSE: 0` detection to prevent 30s wasted timeout on remote connection termination.
  4. **Fixed SD Card Handle Leak**: Ensured `f_close(&File_Sd)` is called unconditionally upon completing file read in `check_sdc_status()`.
  5. Bumped firmware version strings to `TWS6-1.0.17`.

---

### Section 28: Bulletproof HD44780 LCD Timing, Non-Cutting Display Sleep, and Zero-Surprise System Flow (v1.0.18)

- **Release Date**: November 3, 2026
- **Firmware Version**: `TWS6-1.0.18`
- **Key Enhancements & Bug Fixes**:
  1. **Eliminated Destructive 5V Rail Shutdown (`$f4`) in `sleep_display()`**:
     - *Issue*: Inactivity timeout called `sleep_display()`, which sent `$f4;\012` to LPC845 to physically cut the 5V power rail to the LCD. When woken, the 5V rail took hundreds of milliseconds to ramp up while LPC1768 immediately sent `eLcd_Init`, causing HD44780 to completely miss the 4-bit initialization sequence and remain dead/blank. Furthermore, HD44780 controller without backlight draws `< 0.25 mA` (< 1.3 mW).
     - *Fix*: `sleep_display()` now simply turns off the backlight (`lcd_backlight_off()`) and sends `eLcd_Off` (command 0x08). The 5V rail remains powered, 4-bit nibble synchronization is never lost, and waking is instantaneous (< 1 ms) via `eLcd_On` and `lcd_backlight_on()`.
  2. **Fixed DSP Key Sleep/Wake Recursion**:
     - *Issue*: When screen was asleep, pressing DSP key woke the display (`lcd_active = 1`), but then immediately fell into `case eDisp_Key:` which called `toggle_display()`. Because `lcd_active` was now 1, it immediately put the screen back to sleep!
     - *Fix*: If `!lcd_active`, waking the display on `eDisp_Key` consumes the event via `continue;`, cleanly staying awake.
  3. **Strict HD44780 Nibble Timing & Setup Times**:
     - *Issue*: `Output_Lcd()` was driving EN high *before* clearing and setting data lines, and EN pulse width was only ~80 ns (`d < 20`). Missed nibbles permanently swapped high and low nibbles, turning commands like `0x80` into `0x08` (Display OFF).
     - *Fix*: Created `lcd_write_nibble()`:
       1. Drives RS and RW while EN is LOW.
       2. Drives D4..D7 data while EN is LOW.
       3. Delays 500 ns address/data setup time.
       4. Pulses EN HIGH for 2 µs.
       5. Drops EN LOW (falling edge latches data).
       6. Delays 2 µs hold/cycle recovery time.
       7. Clear Display (`0x01`) and Return Home (`0x02`) enforce 3 ms execution delay.
  4. **Eliminated Boot Race Condition with `lcd_task_started`**:
     - *Issue*: `vUi_Task` never waited for `vLcd_Task` to finish `Initialize_Lcd()`, causing startup splash screen writes to collide with power-on reset nibbles and freeze at `VARSHA-TWS INITIALIZING..`.
     - *Fix*: `vLcd_Task` signals `lcd_task_started = 1;` at completion of `Initialize_Lcd()`. `vUi_Task` startup synchronization loops on `while (!lcd_task_started) vTaskDelay(20);`.
  5. **Auto-Recovery for SD Card Storage**:
     - *Issue*: `check_sdc_status()` was only called once at boot. If SD card was inserted late or needed settling, `gi_disk_ok` stayed 0 forever, skipping all daily telemetry logging (`store_data_in_disk()`). In addition, 2-digit years were not normalized, resulting in files named `03110026.txt`.
     - *Fix*: `vDisk_Task` retries `check_sdc_status()` every 10 seconds if `!gi_disk_ok`. `store_data_in_disk()` auto-retries `check_sdc_status()` before writing. Years are normalized to 4-digit (e.g. 26 -> 2026), guaranteeing daily log files named `DDMM2026.txt`.
  6. **Bumped Firmware Version**: Updated to `TWS6-1.0.18`.

---

## 7. Anti-Patterns & Failed Approaches (DO NOT REPEAT)

| Mistake / Failed Approach | Why It Failed | What Must Be Done Instead |
| :--- | :--- | :--- |
| **Cutting 5V LCD power rail via `$f4` during display sleep** | Destroys HD44780 4-bit nibble synchronization, causes rail power bounce, and risks ESD latchup via backfeeding I/O pins. The LCD controller alone draws `< 0.25 mA`. | Keep 5V rail continuously ON while LPC1768 is running. Sleep display by turning off backlight and sending `0x08` (Display OFF); wake display instantly with `0x0C` (Display ON) and backlight ON. |
| **Pulsing LCD EN before setting data lines or with < 230ns pulse width** | Violates HD44780 setup timing ($t_{AS} \ge 40\text{ ns}, t_{DS} \ge 80\text{ ns}, t_W \ge 230\text{ ns}$). Causes missed nibbles which permanently swap high/low nibble alignment, turning address `0x80` into Display OFF `0x08`. | Always place RS, RW, and D4..D7 data on the bus while EN is LOW, delay $\ge 500\text{ ns}$, pulse EN for $\ge 2\text{ µs}$, and drop EN LOW. |
| **Starting UI rendering before `lcd_task_started == 1`** | `Initialize_Lcd()` takes ~75 ms. Sending splash screen or display commands during this window corrupts 4-bit mode setup and freezes the screen at `VARSHA-TWS INITIALIZING..`. | Always synchronize `vUi_Task` on `while (!lcd_task_started) vTaskDelay(20);`. |
| **Calling `check_sdc_status()` only once at boot** | If SD card powers up slower than MCU or is inserted after boot, `gi_disk_ok` remains 0 forever, silently disabling all daily logging. | Periodically retry `check_sdc_status()` in `vDisk_Task` and before `store_data_in_disk()` if `gi_disk_ok == 0`. |
| **Formatting SD log file year as `%04d` without 2-digit normalization** | 2-digit year (e.g. 26) formats as `0026`, creating files like `03110026.txt` instead of `03112026.txt`. | Normalize year: `if (yr < 100 && yr > 0) yr += 2000; if (yr < 2020) yr = 2026;`. |
| **Silently skipping linker stage when Redlib is absent** | Causes `build.sh` to exit with success while leaving stale binaries from previous commits on disk. Flashing appears successful, but the device runs old code. | Always provide a standalone GNU Arm linker fallback (`example01_gnu.ld`) with `nano.specs` so builds never produce stale images. |
| **Polling LCD busy flag in an infinite loop without dual EN strobes** | On 4-bit bus mode, reading the busy flag requires two EN strobes (high nibble, low nibble). Polling single pin floats high with pull-up, trapping the MCU in an infinite loop. | Use calibrated microsecond execution delays (~50 µs) instead of busy-flag polling on HD44780 LCDs. |
| **Calling `system_enter_sleep()` on UI inactivity timeout** | Traps `vKbd_Task` and `vUi_Task` in `while (shutdown_pwr == 1)` and tristates pins without LPC845 cutting power, making the keypad unresponsive. | Call `sleep_display()` to turn off the LCD backlight while keeping tasks and keypad scanning active. |

| Mistake / Failed Approach | Why It Failed | What Must Be Done Instead |
| :--- | :--- | :--- |
| **Attempting FTP fallback when HTTP telemetry fails** | `ftp1.ksndmc.net` is unreachable; waiting for FTP timeouts wastes 2+ minutes, delays sleep, and leaves the modem NETLIGHT pulsing. In TWS, all transmission is HTTP; failed samples are resent via HTTP backlog on the next slot. | Completely eliminate 15-min FTP fallbacks. On HTTP completion (success or failure), immediately close sockets, shut down the modem with `AT+CPOF`, and enter deep sleep. |
| **Running FTP loops unconditionally after successful HTTP** | Hangs transmission for minutes trying to connect to FTP1 even when HTTP transmission already succeeded. | Skip legacy FTP loops entirely when `http_status == 1`. |
| **Not closing PDP context (`AT+NETCLOSE`) and modem baseband (`AT+CPOF`) before sleep** | Keeps SIM7600 powered with NETLIGHT LED blinking fast continuously, draining power. | Always execute `AT+CIPCLOSE=0`, `AT+NETCLOSE`, and `AT+CPOF` upon telemetry completion before sleeping. |
| **Gating `system_enter_sleep()` or `$f7;\012` power rail shutdown behind `!gUi_Active`** | If the operator touched a key or LCD activated, `gUi_Active` remains set, preventing LPC845 power rail shutdown and causing LPC1768 and modem to stay powered on continuously. | Unconditionally call `system_enter_sleep()` on `sleep_mode == 1`, dispatch `$f7;\012` on `shutdown_pwr == 1`, and trigger `system_enter_sleep()` on 60s UI inactivity. |
| **Not flushing UART circular buffer before `AT+CIPOPEN`** | Preliminary `AT+CIPCLOSE=0` on an already-closed socket returns `ERROR`. Without flushing, `http_open_tcp()` reads this stale `ERROR` in 0 ms and aborts genuine connection attempts. | Always flush `gprs_recv_out_cnt = gprs_recv_in_cnt;` immediately before dispatching `AT+CIPOPEN=0`. |
| **Sending `AT+CIPSEND` immediately after `+CIPOPEN: 0,0` without settling delay** | SIM7600 internal TCP stack needs ~50–150 ms to transition socket state to open. Immediate `AT+CIPSEND` returns `+CIPERROR: 4` (*Operation not allowed*). | Add a 150 ms delay (`vTaskDelay(150 / portTICK_RATE_MS)`) after receiving `+CIPOPEN: 0,0` before sending `AT+CIPSEND`. |
| **Hardcoding MCUXpresso IDE plugin paths in `build.sh`** | MCUXpresso plugins contain installation-specific date stamps. Hardcoded paths fail to match, causing `build.sh` to silently skip final HEX linking and leave stale firmware in place. | Dynamically resolve toolchain and Redlib library paths relative to `arm-none-eabi-gcc` or via `/Applications/MCUXpresso*` wildcards. |
| **Running unconditional mass-erase in `flash_lpc1768.sh`** | Mass-erase wipes Sector 1 (`0x1000`–`0x1FFF`), destroying configured Station ID (`001922`), GSM numbers, and calibration parameters. | Let LinkServer `load` erase only Sector 0 and Application sectors (`0x18000+`). Only mass-erase if `--mass-erase` flag is explicitly passed. |
| **Running unconditional mass-erase in `flash_lpc1768.sh`** | Mass-erase wipes Sector 1 (`0x1000`–`0x1FFF`), destroying configured Station ID (`001922`), GSM numbers, and calibration parameters. | Let LinkServer `load` erase only Sector 0 and Application sectors (`0x18000+`). Only mass-erase if `--mass-erase` flag is explicitly passed. |
| **Hardcoding fallback Station ID to `001060`** | `001060` was an old 2013 test ID. Blank/new sectors defaulted to 1060, requiring manual reconfiguration. | Set fallback default in `params_default()` to active test station ID (`001922`). |
| **Adding large static buffers to `RamLoc32`** | LPC1768 `RamLoc32` has only 32 KB and is nearly full with FreeRTOS heap and FAT filesystem structures. Adding static buffers overflows `RamLoc32` `.bss`. | Relocate large telemetry buffers (`http_data`, `ftp_buff`) to AHB SRAM Bank 1 (`0x20080000`–`0x20083FFF`, 16 KB unreserved). |
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
| **Sending `AT+CIPSEND=0` without length or with trailing comma** | SIM7600 returns `ERROR` on `AT+CIPSEND=0` or `AT+CIPSEND=0,`. SIM7600 direct socket mode strictly requires the payload byte count. | Always format HTTP headers and body first, compute `rawLen = strlen(http_data)`, and issue `AT+CIPSEND=0,<rawLen>\r\n`. |
| **Appending `\032` (Ctrl+Z / 0x1A) in direct socket mode** | In fixed-length direct socket mode (`AT+CIPSEND=0,<len>`), Ctrl+Z is NOT a packet terminator; it gets sent as a literal byte corrupting the stream. | Send exactly `rawLen` bytes without `\032`. |
| **Hardcoding non-existent fallback IPs (`144.91.104.105:80`) for KSNDMC** | Hangs HTTP transmission for 25 seconds on Airtel IoT private APNs. | Use KSNDMC primary domain name first, then fallback to `117.216.42.181:80`. |
| **Not resetting `dest_in_count` between response lines** | Multi-line HTTP headers exceed 300 bytes, wrapping around in `http_response` and corrupting `Date:`, `200 OK`, or `Success` matches. | Reset `dest_in_count = 0; http_response[0] = 0;` inside `if (lf_recd)` after each line. |
| **Omitting FreeRTOS WDT feed during 25–30s modem waits** | `vWDT_Task` checks task health every 10 seconds. Un-fed waits in `http_open_tcp` or response polling trip watchdog resets. | Feed `gi_wdt_task_ok_stat |= WDT_GPRS_OK;` periodically inside timeout loops. |
| **Sending HTTP before 62 seconds past the slot boundary** | KSNDMC server aggregator database slot isn't active until T+1m02s, responding with `"Rejected"`. | Enforce the 62-second hold guard before establishing TCP socket. |
| **Omitting `AT+CGEREP=0` modem URC silencing** | `+CGEV` network event URCs arrive during `AT+CIPOPEN` or `AT+CIPSEND`, corrupting UART responses. | Send `AT+CGEREP=0` during modem basic command init. |

