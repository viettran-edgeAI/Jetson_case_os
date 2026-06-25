# Jetson Shield OS - System Design Specification

**Document ID:** JSOS-SDD-001
**Version:** 1.5
**Date:** 2026-06-22
**Target Platform:** ESP32 Dev Module (Dual Core 240 MHz, 256 KB RAM, 2 MB Flash)

## 1. Purpose and Scope

This document defines the architecture and engineering standards for the Jetson Shield OS firmware running on ESP32.

### 1.1 Objectives
- Receive and process Jetson Orin Nano runtime data over UART links.
- Send predefined Jetson settings requests from the LCD_2 Settings UI without exposing shell access.
- Control peripheral modules (LCD_1, LCD_2, LED ring, fan, DHT sensor) through a unified controller.
- Enforce deterministic behavior through state-based orchestration.
- Provide a maintainable modular codebase for deployment and future scaling.

### 1.2 In-Scope
- Task-based runtime architecture on FreeRTOS.
- Message queue contract between producer tasks and system controller.
- State machine and alert management.
- Module responsibilities and APIs.
- Configuration standards and deployment baseline.

### 1.3 Out-of-Scope
- Jetson-side service internals beyond the UART payload and command contract.
- Final UI artwork/theme refinement for displays.
- OTA/update infrastructure.

## 2. System Context and External Interfaces

### 2.1 Hardware Components
- ESP32 controller board.
- LCD_1: OLED 128x32 (I2C).
- LCD_2: TFT touchscreen configured by `kLcd2Width`, `kLcd2Height`, and `kLcd2Rotation` in `system_configuration.h` (current baseline: 320x240, rotation 2).
- Sensor: DHT11.
- Fan: 3-pin PWM fan with optional tach input.
- LED: WS2811/NeoPixel-compatible LED ring.
- Jetson Orin Nano via two ESP32 hardware UARTs.

### 2.2 Logical Interface Roles
- **Serial1 (runtime/settings channel):** bidirectional channel for Jetson runtime metrics and predefined settings commands/responses.
- **Serial2 (kernel/debug channel):** receive-only state discovery and boot/shutdown text stream.
- **Sensor task:** periodic local environment telemetry.

### 2.3 ESP32 UART Pin Baseline
- Debug USB Serial: 115200 8N1.
- Serial1 RX: GPIO25, connected to Jetson runtime UART TX.
- Serial1 TX: GPIO17, connected to Jetson runtime UART RX.
- Serial2 RX: GPIO16, connected to Jetson kernel/debug UART TX.
- Serial2 TX is not required by the current architecture.

## 3. Architecture Overview

### 3.1 Core Pattern
The firmware follows a **module + controller + message bus** pattern:

1. Peripheral modules own hardware-specific logic.
2. Producer tasks collect input events/data (Serial1, Serial2, Sensor), while controller-owned UI actions may enqueue predefined Serial1 commands.
3. A central `SystemController` consumes queue messages, updates state/alerts, and dispatches commands to modules.

### 3.2 Main Runtime Blocks
- `SystemController` (single authority for orchestration).
- `TaskSerial1` (Jetson stats and settings-response ingress).
- `TaskSerial2` (kernel/debug ingress and boot/power transition detector).
- `TaskSensor` (humidity and ambient telemetry).
- Dedicated LCD_2 render task plus optional module service tasks for smooth, non-blocking rendering.

### 3.3 Design Principles
- All state transitions and alert transitions must happen only in `SystemController`.
- LCD_2 rendering must be state-scoped: transition log console in `BOOTING_ON`/`SHUTTING_DOWN`, dashboard and settings in `RUNNING`, and idle/game screens in `POWER_OFF`. Frequently updated LCD_2 values must use sprite-backed or tightly bounded dirty-region rendering to avoid flicker.
- User control values exposed by LCD_2 (for example LED brightness) must have a single controller-owned source of truth and must not drift due to quantized feedback loops.

Producer tasks are data sources, not policy owners.

## 4. State Machine Specification

### 4.1 System States
- `POWER_OFF` (initial default)
- `BOOTING_ON`
- `RUNNING`
- `SHUTTING_DOWN`

### 4.2 Alert Classes
- `ALERT_NONE`
- `ALERT_HIGH_TEMPERATURE`
- `ALERT_HIGH_HUMIDITY`

### 4.3 Transition Rules (Normative)
- `POWER_OFF -> BOOTING_ON` when Serial2 indicates boot-start token.
- `BOOTING_ON -> RUNNING` when Serial2 indicates login/boot-complete token.
- `RUNNING -> SHUTTING_DOWN` when Serial2 indicates shutdown/suspend token.
- `SHUTTING_DOWN -> POWER_OFF` when Jetson-off token is confirmed.
- Direct transitions not listed above are invalid and shall be ignored unless a safety override is defined.

### 4.4 Transition Debounce Standard
- Transition events from Serial2 must be validated with minimal debounce (default 50 ms).
- Repeated identical transition tokens within debounce window are ignored.

### 4.5 Dual-UART State Inference Principle
- Runtime state inference shall use only Jetson-facing `Serial1` and `Serial2` evidence. The USB debug `Serial` port is out-of-band and must never drive state transitions.
- `RUNNING` is authoritative only when `Serial1` receives valid Jetson stats frames that match the `jetson_shield.c` contract.
- `BOOTING_ON` and `SHUTTING_DOWN` are transitional states inferred when only `Serial2` is active:
  - `BOOTING_ON`: valid `Serial2` traffic without active shutdown/off indicators.
  - `SHUTTING_DOWN`: valid `Serial2` traffic containing shutdown, suspend, or Jetson-off indicators such as `ivc channel driver missing`.
- `POWER_OFF` is inferred only when neither `Serial1` nor `Serial2` has recent valid Jetson evidence inside the configured evidence window.
- State inference shall be re-evaluated continuously so the controller can recover after ESP32 restart, interrupted boot, abrupt Jetson power loss, or rapid user power toggling.

## 5. Message Bus Contract

### 5.1 Transport
- FreeRTOS queue (`message_queue`) with fixed-length message envelope.
- Producers: Serial1, Serial2, Sensor tasks. Controller-owned UI actions may enqueue outbound Serial1 commands through the Jetson serial module.
- Consumer: SystemController task.

### 5.2 Envelope Standard
Each message shall include:
- Source (`SERIAL1`, `SERIAL2`, `SENSOR`, `CONTROLLER`)
- Type (`JETSON_STATS`, `JETSON_RESPONSE`, `KERNEL_LOG`, `SENSOR_READING`, `STATE_EVENT`, `ALERT_EVENT`)
- Timestamp (ms)
- Payload structure

### 5.3 Queue Policy
- Queue full behavior: drop oldest non-critical telemetry first, preserve critical state events.
- Max payload line length is bounded to prevent heap fragmentation.
- No dynamic allocation in hot path for message transport.

## 6. Tasking and Scheduling Standard

### 6.1 Task Responsibilities
- **TaskSerial2:**
  - In `BOOTING_ON`/`SHUTTING_DOWN`: stream log events with low latency.
  - In `RUNNING`/`POWER_OFF`: poll every 200 ms for transition triggers.
- **TaskSerial1:**
  - Remains enabled in all states so the controller can rediscover an already-running Jetson.
  - Parses Jetson metric lines and forwards valid runtime stats.
  - Forwards non-stats Serial1 lines, such as IP and Wi-Fi responses, as `JETSON_RESPONSE` messages.
- **TaskSensor:**
  - Always active; sample humidity/temperature periodically.
- **Controller Task:**
  - Deterministic event loop; owns state machine and actuation commands.

### 6.1.1 Power-State Serial Monitoring Procedure
- In `POWER_OFF`, Jetson is expected to be silent on both Serial1 and Serial2.
- While in `POWER_OFF`, Serial2 shall wake periodically using the idle probe period and validate any received line before acting on it. Short or malformed lines shall be treated as pin noise and ignored.
- A validated boot-start line on Serial2 causes `POWER_OFF -> BOOTING_ON`.
- Valid `Serial2` kernel/debug lines observed after an ESP32 reset shall also be treated as `BOOTING_ON` evidence when `Serial1` has not yet produced valid stats.
- During `BOOTING_ON`, Serial2 remains in low-latency receive mode and continues parsing kernel/debug lines until the boot-complete token is observed (same intent as the legacy boot-completion check in `Jetson_Triple_Serial.ino`).
- When the boot-complete token is validated, the controller arms the startup-success path and waits for valid Serial1 stats before confirming `BOOTING_ON -> RUNNING`.
- Serial1 metrics parsing shall remain enabled in every state so the controller can rediscover `RUNNING` after ESP32 restart while Jetson is already active. Non-stats Serial1 responses shall remain available to the settings UI.
- During `RUNNING`, Serial2 shall continue periodic probing for shutdown, suspend, or Jetson-off indicators. The `ivc channel driver missing` pattern is a valid Jetson-off indicator and shall be treated as `SHUTTING_DOWN` evidence.
- When a valid shutdown/suspend/Jetson-off indicator is observed while in `RUNNING`, the controller performs `RUNNING -> SHUTTING_DOWN` unless newer Serial1 stats re-establish `RUNNING`.
- `SHUTTING_DOWN` is expected to be brief. If both Serial1 and Serial2 fall quiet inside the configured evidence windows, the controller shall finalize `SHUTTING_DOWN -> POWER_OFF`.
- During `RUNNING`, loss of both Serial1 and Serial2 evidence shall be treated as Jetson-off after the configured staleness windows, allowing recovery from sudden power loss without requiring a dedicated heartbeat line.

### 6.2 Core Affinity Guidance
- Core 0: I/O ingress tasks (`Serial1`, `Serial2`, Sensor).
- Core 1: Controller and display-heavy work (optional split LCD2 update task).

### 6.3 Timing Baseline
- Controller tick: 50 ms.
- Serial2 idle probe period: 200 ms.
- Sensor sample period (DHT11): >= 2000 ms.
- LED animation update period: 20-40 ms.
- LCD_2 touch scan period: ~20 ms.
- LCD_2 graph animation cadence: about 6 FPS for smooth scrolling between 1 Hz Jetson metric samples.

## 7. Module Interface Standards

Each module must provide at minimum:
- `init(...)` (required)
- `onStateChange(newState)`
- `onAlertChange(alertMask)`
- `update(nowMs)` if non-blocking periodic logic is needed

### 7.1 LCD_1 Standard
- Active display for boot/shutdown text flow.
- In non-active states, may run lightweight idle effects.
- Must not block controller loop.
- `POWER_OFF -> BOOTING_ON` shall show the Nvidia logo for about 800 ms before kernel text is rendered.
- After boot-complete detection, LCD_1 shall run the welcome sequence derived from `Jetson_Triple_Serial.ino`:
  - `Boot Complete !`
  - scrolling `Viet Tran`
  - scrolling `_ Jetson Orin Nano _`
  - final pause before switching to the running idle logo.

### 7.2 LCD_2 Standard
- Operating mode by state:
  - `BOOTING_ON`: show Serial2 kernel/debug log console.
  - `SHUTTING_DOWN`: show Serial2 kernel/debug log console.
  - `RUNNING`: show dashboard, touch controls, and settings interface.
  - `POWER_OFF`: LCD_2 shall remain active on a black background and show a compact idle screen that clearly indicates Jetson is off.
- `POWER_OFF` idle screen shall include:
  - a black background with a concise `Jetson is off` style status message
  - a touch button below the message that opens a compact games menu
  - optional enclosure telemetry if available, without cluttering the idle screen
- `POWER_OFF` games menu shall provide at least lightweight touch-controlled mini-games such as:
  - an endless runner in the style of the offline Chrome dinosaur game
  - a simple bouncing ball or paddle-ball game
- `POWER_OFF` game rendering shall use a full-screen 1-bit sprite with TFT DMA-backed pushes to minimize RAM use and keep animation smooth on ESP32.
- `POWER_OFF` game loop timing shall be bounded and non-blocking so controller responsiveness and state transition detection are preserved.
- Transition log console (`BOOTING_ON` and `SHUTTING_DOWN`) shall use a full-screen 1-bit sprite (`320x240`) and push full frames for smooth scrolling with bounded RAM usage.
- Must render:
  - CPU usage graph
  - GPU usage graph
  - RAM usage graph
  - CPU temperature numeric
  - GPU temperature numeric
  - Power consumption numeric (mW)
- Graph traces at 0% shall remain visible above the x-axis. Graph motion shall scroll smoothly between 1 Hz metric samples using a delayed/offscreen history window so new samples enter without a visible empty leading gap.
- Lower-right telemetry shall show:
  - current fan duty
  - enclosure humidity from the sensor module
  - enclosure temperature from the sensor module
- LCD_2 shall provide a settings entry point from the running dashboard.
- LED brightness control shall live in Settings as a subtle horizontal slider and remain controller-owned.
- Fan control shall remain automatic and shall not be user-adjustable from LCD_2.
- LCD_2 touch handling shall avoid full-screen flicker by redrawing only changed dynamic regions during normal updates.
- LED brightness control shall keep the user-selected percentage stable after touch release; controller synchronization must not quantize the requested value via repeated percent-to-PWM-to-percent round-trips.
- Rotation default is the configured `jetson_cfg::kLcd2Rotation`; the current firmware baseline is rotation `2`, with runtime width/height read back from TFT_eSPI after rotation is applied.

### 7.2.1 LCD_2 Settings Interface Standard
- The settings entry point shall be available from the running dashboard as an icon button.
- The Settings main page shall show a horizontal LED brightness slider, enclosure temperature/humidity, ESP32 filesystem usage, heap RAM, PSRAM status, and uptime.
- Wi-Fi shall provide scan, network list, password keyboard, connection result, and status handling.
- Network shall provide IP status, SSH status/control, and ngrok SSH status/control.
- System shall provide Headless Mode, Restart Monitor, Reboot, and Shutdown, with confirmation before disruptive actions.
- Headless Mode shall show status and provide Enable after reboot, Disable after reboot, Apply now, and Reboot actions.
- About shall show Jetson IP, hostname, and service version.
- Settings requests shall use bounded timeouts and must not block controller state inference, stats parsing, graph animation, or touch handling.
- Frequently changing Settings values, such as uptime, shall update only their own bounded dynamic region and must not dirty the whole page.

### 7.2.2 LCD_2 Touch Calibration Standard
- Touch input shall be calibrated against the active LCD rotation before normal use.
- Calibration data shall persist in flash so normal firmware reloads do not require recalibration.
- Stored calibration shall include rotation metadata; mismatched rotation invalidates the saved calibration and forces recalibration.
- If calibration data is missing, corrupt, or invalid for the active rotation, the firmware shall run guided touchscreen calibration on LCD_2 during initialization.
- Touch input shall remain disabled until valid calibration data has been loaded or newly created.
- A compile-time force-recalibration switch shall be available for service/debug use.


### 7.2.3 LCD_2 Flicker-Free Rendering Standard
- Static scaffolds, frames, labels, axes, and buttons shall be drawn only on page/layout/state changes.
- Dynamic values shall use small sprites or tightly bounded dirty rectangles; updating one value must not redraw unrelated controls.
- Full-screen redraws are allowed only on page transitions, state transitions, calibration, degraded-mode notices, and intentionally full-frame 1-bit modes.
- Graph plot regions shall remain sprite-backed. Graph headers and numeric labels shall be separated from plot redraw paths.
- Color depth shall be selected by region: 1-bit for monochrome full-screen logs/games, 8-bit for graph plots and colored panels, and 4-bit/8-bit for small smooth-font value cells.
- Direct dynamic TFT drawing is allowed only as a documented exception and must clear no more than its own stable rectangle.
- The project-level refactor plan is `docs/LCD2_FLICKER_REFACTOR_PLAN.md`.

### 7.3 LED Standard
- Off only in `POWER_OFF`.
- Normal operation: animated palette cycling.
- On power transition entry: blink fast twice.
- Any active alert: force red override until alert clears.
- Manual brightness trimming from LCD_2 is allowed and shall scale the active LED output without replacing controller-owned alert/state behavior.

### 7.4 Fan Standard
- Base rule: in `RUNNING`, enabled when Jetson thermal metric > `TEMP_LOW`.
- In non-running states: fan off unless humidity alert is active.
- High-temperature alert may force high duty profile.

### 7.5 Sensor Standard
- Always active in all states.
- Primary policy output: humidity alert evaluation.

## 8. UART and Data Protocol Standard

### 8.1 UART Defaults
- Debug Serial: 115200 8N1
- Serial1 (runtime/settings): 115200 8N1, bidirectional.
- Serial2 (kernel/debug): 115200 8N1, Jetson-to-ESP32 receive-only.

### 8.2 Serial1 Data Contract (from Jetson monitor service)
Expected compact metric tokens:
- `RAM:<int>`
- `CPU:<int>`
- `GPU:<int>`
- `CT:<float>` (CPU temperature)
- `GT:<float>` (GPU temperature)
- `P:<int>mW` (power)
- `ND:<int>` / `NU:<int>` (network download/upload KB/s)
- `DR:<int>` / `DW:<int>` (disk read/write KB/s)
- `SW:<int>` (swap used MB)
- `DU:<int>` (disk used percent)

Parser requirements:
- Ignore unknown tokens safely.
- Preserve previous valid value when current token parse fails.
- Never crash on malformed lines.
- Treat non-metric Serial1 lines as possible settings responses and route them separately from stats parsing.

### 8.3 Serial1 Command/Response Contract
Outbound ESP32 requests to the Jetson monitor service:
- `REQ:IP`
- `REQ:WIFI_SCAN`
- `WIFI_CONNECT SSID:<percent-escaped-ssid> PSK:<percent-escaped-password>`
- `REQ:ABOUT`
- `REQ:SSH_STATUS`, `SSH_START`, `SSH_STOP`
- `REQ:NGROK_STATUS`, `NGROK_START`, `NGROK_STOP`
- `REQ:HEADLESS_STATUS`, `HEADLESS_ENABLE_BOOT`, `HEADLESS_DISABLE_BOOT`, `HEADLESS_APPLY_NOW`
- `MONITOR_RESTART`, `JETSON_REBOOT`, `JETSON_SHUTDOWN`

Expected Jetson responses include:
- `IP ...` status lines containing interface, SSID, address, and connection state fields.
- `WIFI_BEGIN` followed by zero or more `WIFI ...` network rows and a final `WIFI_END COUNT:<int>`.
- `WIFI_CONNECT ...` result lines.
- `ABOUT ...` hostname, IP, and service version lines.
- `SSH ...` and `SSH_RESULT ...` status/action result lines.
- `NGROK ...` and `NGROK_RESULT ...` status/action result lines, including TCP endpoint availability when the local ngrok API is available.
- `HEADLESS ...` and `HEADLESS_RESULT ...` status/action result lines.
- `SYSTEM_RESULT ...` acknowledgement lines for monitor restart, reboot, and shutdown.
- `ERR ...` lines for rejected or failed requests.

Command handling requirements:
- Only predefined commands are allowed; no raw shell execution shall be exposed over UART. Service names and ngrok API URL may be configured through `/etc/default/jetson_shield`.
- Requests shall be line-oriented and newline terminated.
- UI timeouts shall be bounded and recoverable.
- Responses may arrive interleaved with metric frames and must not break RUNNING-state inference.

### 8.4 Serial2 Event Contract
Kernel line matching shall use configurable token strings for:
- Boot start
- Boot complete
- Shutdown/suspend detection
- Power-off confirmation

## 9. Reliability and Safety Standards

- No blocking calls longer than controller tick inside control path.
- All hardware writes must be idempotent where possible.
- Failed sensor reads must not trigger undefined behavior.
- Queue overflow and parse errors shall be counted in diagnostics counters.
- Watchdog integration is recommended for production builds.
- Touch calibration persistence shall be robust against firmware reloads; loss of calibration after every upload is considered a defect.
- Touch jitter or release spikes should be filtered so slider controls do not jump noticeably after finger release.

## 10. Coding and Integration Standards

- Use clear module boundaries under `components/<module>/`.
- Keep public interfaces in `.h`, logic in `.cpp`.
- Avoid global mutable state outside controller-owned context.
- Use compile-time constants from `system_configuration.h`; no duplicated literals.
- Keep ISR logic minimal and lock-free.
- Persistent LCD_2 touch calibration settings shall be defined in `system_configuration.h`, including the force-calibration flag and calibration file path.
- Touch calibration storage shall use a deterministic file format with version and rotation checks.

## 11. Deployment Readiness Gates

A firmware baseline is deployment-ready when:
1. All modules initialize successfully on boot.
2. State transitions and Serial1 command/response handling pass scripted UART replay tests.
3. Alert rules trigger expected LED/fan behavior.
4. LCD_2 behavior is state-correct: idle/game menu in `POWER_OFF`, Serial2 log console in `BOOTING_ON`/`SHUTTING_DOWN`, and valid stats dashboard plus settings interface in `RUNNING`.
5. No queue overflow in nominal telemetry load.
6. 30-minute stability run completes without crash/reset.

## 12. Next Implementation Step (Planned)

- Execute the LCD_2 flicker refactor in `docs/LCD2_FLICKER_REFACTOR_PLAN.md`, starting with graph header/value sprites and Settings value-cell sprites.
- Add regression checks for LCD_2 touch calibration persistence across reset and firmware reload.
- Add validation for LED brightness slider stability under press/drag/release interaction.
- Add lightweight replay and display integration tests for UART state transitions, Serial1 command/response handling, dashboard rendering, and Settings command/response handling.

## 13. Reliability Hardening Update (2026-03-16)

The following reliability fixes were implemented in the firmware codebase:

1. LCD2 mutex timing is now bounded (no `portMAX_DELAY` in controller paths).
  - Controller-side LCD2 accesses use bounded mutex waits and only unlock after successful lock acquisition.
  - This prevents display-lock contention from stalling controller policy/state processing indefinitely.

2. LCD2 synchronization is now consistent across tasks.
  - Controller writes/reads to LCD2 shared state are lock-protected.
  - Render task keeps lock ownership only for the update window.

3. Fan anti-chatter dwell-time guards were added.
  - Minimum ON dwell and OFF dwell windows reduce rapid toggling around threshold boundaries.

4. LCD2 calibration init path is less blocking.
  - Removed filesystem auto-format from the hot init path.
  - Removed the fixed calibration intro delay before touch calibration begins.

5. Sensor freshness protection was added.
  - Controller now tracks last-valid sensor timestamp and consecutive read failures.
  - Stale sensor values are aged out using timeout/failure thresholds.

6. Serial overlength frame handling now has explicit overflow behavior.
  - Overlong serial lines are dropped safely by entering discard-until-newline mode.
  - Overflow counters are tracked for Serial1/Serial2 diagnostics.

7. Dino game heavy compute bursts were bounded.
  - Survival simulation frames, cluster search attempts, and star placement attempts are capped lower.
  - Grid-search fallback uses a bounded budget to avoid long frame spikes.

8. Serial2 plausibility filtering is applied before transition classification in all states.
  - Malformed/noise lines are ignored globally, not only in `POWER_OFF`.

9. LCD2 sprite init failures are handled explicitly.
  - Dashboard enters a degraded mode with visible on-screen indicator when sprite allocation fails.

10. POWER_OFF idle refresh cadence was tightened.
  - Idle-screen refresh interval reduced to lower long static-image risk.

11. Dino dead-state input buffering was fixed.
  - Jump request is cleared during dead-state ticks to prevent buffered jump after respawn.
