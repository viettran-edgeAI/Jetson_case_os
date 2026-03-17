# Jetson Shield OS - System Design Specification

**Document ID:** JSOS-SDD-001  
**Version:** 1.3  
**Date:** 2026-03-16  
**Target Platform:** ESP32 Dev Module (Dual Core 240 MHz, 256 KB RAM, 2 MB Flash)

## 1. Purpose and Scope

This document defines the architecture and engineering standards for the Jetson Shield OS firmware running on ESP32.

### 1.1 Objectives
- Receive and process Jetson Orin Nano runtime data over UART links.
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
- Jetson-side service implementation details beyond the UART payload contract.
- Final UI artwork/theme refinement for displays.
- OTA/update infrastructure.

## 2. System Context and External Interfaces

### 2.1 Hardware Components
- ESP32 controller board.
- LCD_1: OLED 128x32 (I2C).
- LCD_2: TFT 240x320 touchscreen (used in landscape mode via rotation setting).
- Sensor: DHT11.
- Fan: 3-pin PWM fan with optional tach input.
- LED: WS2811/NeoPixel-compatible LED ring.
- Jetson Orin Nano via UART1 and UART2.

### 2.2 Logical Interface Roles
- **Serial2 (kernel/debug channel):** state discovery and boot/shutdown text stream.
- **Serial1 (metrics channel):** periodic runtime stats during RUNNING state.
- **Sensor task:** periodic local environment telemetry.

## 3. Architecture Overview

### 3.1 Core Pattern
The firmware follows a **module + controller + message bus** pattern:

1. Peripheral modules own hardware-specific logic.
2. Producer tasks collect input events/data (Serial1, Serial2, Sensor).
3. A central `SystemController` consumes queue messages, updates state/alerts, and dispatches commands to modules.

### 3.2 Main Runtime Blocks
- `SystemController` (single authority for orchestration).
- `TaskSerial1` (Jetson stats parser ingress).
- `TaskSerial2` (kernel/debug ingress and boot/power transition detector).
- `TaskSensor` (humidity and ambient telemetry).
- Optional module service tasks (LED, LCD refresh) if needed for smooth rendering.

### 3.3 Design Principles
- All state transitions and alert transitions must happen only in `SystemController`.
- LCD_2 rendering must be state-scoped: transition log console in `BOOTING_ON`/`SHUTTING_DOWN`, dashboard in `RUNNING`, display-off in `POWER_OFF`.
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
- `RUNNING` is authoritative only when `Serial1` receives valid Jetson stats frames that match the `system_monitor.c` contract.
- `BOOTING_ON` and `SHUTTING_DOWN` are transitional states inferred when only `Serial2` is active:
  - `BOOTING_ON`: valid `Serial2` traffic without active shutdown/off indicators.
  - `SHUTTING_DOWN`: valid `Serial2` traffic containing shutdown, suspend, or Jetson-off indicators such as `ivc channel driver missing`.
- `POWER_OFF` is inferred only when neither `Serial1` nor `Serial2` has recent valid Jetson evidence inside the configured evidence window.
- State inference shall be re-evaluated continuously so the controller can recover after ESP32 restart, interrupted boot, abrupt Jetson power loss, or rapid user power toggling.

## 5. Message Bus Contract

### 5.1 Transport
- FreeRTOS queue (`message_queue`) with fixed-length message envelope.
- Producers: Serial1, Serial2, Sensor tasks.
- Consumer: SystemController task.

### 5.2 Envelope Standard
Each message shall include:
- Source (`SERIAL1`, `SERIAL2`, `SENSOR`, `CONTROLLER`)
- Type (`JETSON_STATS`, `KERNEL_LOG`, `SENSOR_READING`, `STATE_EVENT`, `ALERT_EVENT`)
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
  - Active in `RUNNING`; parse/forward Jetson metric lines.
  - Idle or blocked in non-running states.
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
- Serial1 metrics parsing shall remain enabled in every state so the controller can rediscover `RUNNING` after ESP32 restart while Jetson is already active.
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
  - `RUNNING`: show dashboard and touch controls.
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
- Graph traces at 0% shall remain visible above the x-axis.
- Lower-right telemetry shall show:
  - current fan duty
  - enclosure humidity from the sensor module
  - enclosure temperature from the sensor module
- LCD_2 shall provide one user control only: a permanent vertical LED brightness slider on the far right edge of the dashboard.
- Fan control shall remain automatic and shall not be user-adjustable from LCD_2.
- LCD_2 touch handling shall avoid mode toggles for basic operation; LED brightness control is always visible during `RUNNING`.
- LED brightness control shall keep the user-selected percentage stable after touch release; controller synchronization must not quantize the requested value via repeated percent-to-PWM-to-percent round-trips.
- Rotation default is landscape (`setRotation(3)` as baseline config, matching `Jetson_graph/`).

### 7.2.1 LCD_2 Touch Calibration Standard
- Touch input shall be calibrated against the active LCD rotation before normal use.
- Calibration data shall persist in flash so normal firmware reloads do not require recalibration.
- Stored calibration shall include rotation metadata; mismatched rotation invalidates the saved calibration and forces recalibration.
- If calibration data is missing, corrupt, or invalid for the active rotation, the firmware shall run guided touchscreen calibration on LCD_2 during initialization.
- Touch input shall remain disabled until valid calibration data has been loaded or newly created.
- A compile-time force-recalibration switch shall be available for service/debug use.

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
- Serial1 (stats): 115200 8N1
- Serial2 (kernel/debug): 115200 8N1

### 8.2 Serial1 Data Contract (from Jetson monitor service)
Expected compact tokens:
- `RAM:<int>`
- `CPU:<int>`
- `GPU:<int>`
- `CT:<float>` (CPU temperature)
- `GT:<float>` (GPU temperature)
- `P:<int>mW` (power)

Parser requirements:
- Ignore unknown tokens safely.
- Preserve previous valid value when current token parse fails.
- Never crash on malformed lines.

### 8.3 Serial2 Event Contract
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
2. State transitions pass scripted UART replay tests.
3. Alert rules trigger expected LED/fan behavior.
4. LCD_2 behavior is state-correct: display-off in `POWER_OFF`, Serial2 log console in `BOOTING_ON`/`SHUTTING_DOWN`, and valid stats dashboard in `RUNNING`.
5. No queue overflow in nominal telemetry load.
6. 30-minute stability run completes without crash/reset.

## 12. Next Implementation Step (Planned)

- Add regression checks for LCD_2 touch calibration persistence across reset and firmware reload.
- Add validation for LED brightness slider stability under press/drag/release interaction.
- Add lightweight replay and display integration tests for UART state transitions and dashboard rendering.

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