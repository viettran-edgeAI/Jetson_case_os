# Jetson ↔ ESP32 Touch Display Upgrade Plan

## 1. Objective

Upgrade the existing Jetson-to-ESP32 touchscreen monitor with:

* A smoother real-time dashboard using **3 samples per second**.
* Graph rendering at **3 FPS**.
* Page switching between the current CPU/GPU/RAM dashboard and a new Network/Disk/Swap dashboard.
* A fullscreen Settings screen.
* Wi-Fi scan and connection setup from the touchscreen.
* A standalone virtual keyboard module for Wi-Fi password input.
* Jetson local IP display.
* LED brightness control moved into Settings.

The ESP32 remains the touchscreen UI device.
The Jetson remains responsible for system monitoring, Wi-Fi control, and UART communication.

---

## 2. Current UI Baseline

The current main dashboard displays:

* CPU time graph
* GPU time graph
* RAM time graph
* CPU temperature
* GPU temperature
* Power
* Fan / PWM / hardware temperature status
* LED brightness slider

The current CPU/GPU/RAM dashboard should remain the default screen after boot.

---

## 3. Main Dashboard Upgrade

## 3.1 Dashboard Pages

The main dashboard will have two pages:

```text
Performance Page
├── CPU graph
├── GPU graph
└── RAM graph

IO Page
├── Network upload/download graph
├── Disk read/write graph
└── Swap usage graph
```

The Performance Page keeps the current layout.

The IO Page replaces the CPU/GPU/RAM graph area with Network/Disk/Swap monitoring.

---

## 3.2 Page Switch Button

Add a circular arrow button near the right side of the GPU graph area.

Behavior:

* Hidden by default.
* Appears when the user touches the screen.
* Disappears automatically after 2 seconds without touch input.
* Uses a semi-transparent background, around 30% opacity.
* Does not permanently block graph visibility.
* Switches between Performance Page and IO Page.

Arrow direction:

```text
Performance Page → IO Page: forward arrow
IO Page → Performance Page: backward arrow
```

---

## 4. Smooth Sampling and Rendering Upgrade

## 4.1 Sampling Rate

Increase Jetson-side system stat sampling to:

```text
3 samples per second
```

Equivalent interval:

```text
100 ms per sample
```

This applies to fast-changing dashboard metrics:

```text
CPU usage
GPU usage
RAM usage
Network upload/download speed
Disk read/write speed
Swap usage
Temperature / power if available at this rate
```

Slower or mostly static information does not need to be sent at 3 Hz.

Examples of slower data:

```text
SSID
IP address
Wi-Fi connection status
Fan mode
Device name
Disk name
```

These can update at a lower rate, such as every 1–2 seconds.

---

## 4.2 Graph Rendering Rate

Set ESP32 graph rendering to:

```text
3 FPS
```

Equivalent frame interval:

```text
100 ms per frame
```

The ESP32 should render the graph at the same rate as incoming fast samples.

Expected result:

```text
The graphs move more smoothly and no longer update only once per second.
```

---

## 4.3 Data Window

Use a fixed rolling time window for graphs.

Recommended graph window:

```text
30 seconds
```

At 3 samples per second:

```text
30 seconds × 3 samples/second = 300 points per metric
```

Recommended behavior:

```text
- Keep recent samples in a rolling buffer.
- Drop the oldest samples when the buffer is full.
- Draw the latest 30-second window.
```

If memory or rendering cost becomes too high, the graph window can be reduced to:

```text
15–20 seconds
```

---

## 4.4 UART Payload Strategy

Because data will now be sent at 3 Hz, avoid sending large JSON messages every frame.

Use two update groups:

```text
Fast metrics: 3 Hz
Slow status: 0.5–1 Hz
```

Fast metrics should include only values needed for live graphs:

```text
CPU
GPU
RAM
Network download
Network upload
Disk read
Disk write
Swap usage
Power
Temperature
```

Slow status can include:

```text
SSID
IP address
active interface
disk name
Wi-Fi status
fan state
static labels
```

This keeps UART traffic stable while still making the graphs smooth.

---

## 5. IO Page Metrics

## 5.1 Network Graph

Display:

```text
Download speed
Upload speed
Active interface
```

Suggested units:

```text
KB/s
MB/s
```

---

## 5.2 Disk Graph

Display:

```text
Read speed
Write speed
Disk usage
```

Suggested device:

```text
nvme0n1
```

---

## 5.3 Swap Graph

Display:

```text
Swap used
Swap total
Swap percentage
```

---

## 6. Settings Button

Add a Settings button in the currently empty top-right corner of the main dashboard.

Suggested icon:

```text
gear icon
```

Behavior:

```text
Tap Settings → open fullscreen Settings screen
```

---

## 7. Settings Screen

The Settings screen should occupy the full display.

Required items:

```text
Settings
├── LED brightness slider
├── Wi-Fi setup button
├── Show Jetson IP button
└── Exit button
```

---

## 7.1 Move LED Brightness Control

Move the current LED brightness slider from the main dashboard into Settings.

After removing the LED slider from the main dashboard, expand the existing right-side status cards to use the freed space:

```text
CPU TEMP
GPU TEMP
POWER
FAN
PWM
HWMON TEMP
```

The main dashboard should focus on monitoring, while Settings should contain controls.

---

## 7.2 Exit Button

The Settings screen must include an Exit button.

Behavior:

```text
Exit → return to the previously active dashboard page
```

---

## 8. Wi-Fi Setup Screen

The Wi-Fi setup screen is opened from Settings.

## 8.1 Wi-Fi Scan

When opened:

```text
ESP32 sends Wi-Fi scan request to Jetson
Jetson scans networks using NetworkManager
Jetson returns Wi-Fi list to ESP32
```

Each Wi-Fi item should include:

```text
SSID
Signal strength
Security type
Current connection marker
```

Example UI:

```text
✓ Home_WiFi      82% WPA2
  Lab_5G         66% WPA2
  Phone_AP       44% WPA2
```

The currently connected Wi-Fi should have a check mark.

---

## 8.2 Selecting Wi-Fi

Behavior:

```text
Tap connected Wi-Fi
→ show current connection status

Tap unconnected Wi-Fi
→ open password input screen
→ show virtual keyboard
```

---

## 9. Virtual Keyboard

Create the virtual keyboard as a separate ESP32-side module/file.

Required behavior:

```text
- Render keyboard UI
- Handle touch input
- Edit password text buffer
- Support delete
- Support shift/caps
- Support number/symbol mode
- Support connect/submit
- Support cancel/exit
```

Password display should be masked:

```text
********
```

Raw password should not be displayed after typing.

---

## 10. Wrong Password Handling

If Wi-Fi connection fails:

```text
Jetson returns failure status
ESP32 shows error message
User can retry password input
User can exit back to Wi-Fi list or Settings
```

The user must always have an exit path.

Required actions:

```text
Retry → return to password input
Exit → return to Wi-Fi list or Settings
```

---

## 11. Jetson Local IP Display

Add a Settings option:

```text
Show Jetson IP
```

Behavior:

```text
ESP32 requests current IP
Jetson returns active interface, SSID, and IP address
ESP32 displays the result
```

Example:

```text
Jetson Network

Interface: wlan0
SSID: Home_WiFi
IP: 192.168.1.25
Status: Connected
```

If disconnected:

```text
Status: Not connected
IP: N/A
```

---

## 12. Jetson-Side Responsibilities

The Jetson handles:

```text
- 3 Hz system metrics sampling
- Network upload/download calculation
- Disk read/write calculation
- Swap usage reading
- Wi-Fi scan
- Wi-Fi connect
- Current IP lookup
- LED brightness command handling if controlled from Jetson
- UART request/response handling
```

Recommended data sources:

```text
CPU/RAM/Swap: system files under /proc
Network: /proc/net/dev
Disk: /proc/diskstats
Wi-Fi: nmcli / NetworkManager
IP: active network interface status
```

Avoid exposing raw shell access over UART.

---

## 13. ESP32-Side Responsibilities

The ESP32 handles:

```text
- Touchscreen UI
- 3 FPS graph rendering
- Dashboard page switching
- Temporary arrow button visibility
- Fullscreen Settings screen
- Wi-Fi list display
- Password keyboard UI
- UART command sending
- UART response parsing
```

The ESP32 should not perform Wi-Fi setup for itself.
It only controls the Jetson through predefined UART commands.

---

## 14. Recommended Implementation Phases

## Phase 1 — Smooth Graph Upgrade

* Increase Jetson fast metrics sampling to 3 Hz.
* Increase graph rendering to 3 FPS.
* Use rolling graph buffers.
* Keep slow status updates separate from fast metrics.

Result:

```text
Existing CPU/GPU/RAM graphs become much smoother.
```

---

## Phase 2 — Dashboard Page Switch

* Add Performance Page / IO Page state.
* Add temporary circular arrow button.
* Hide the button after 2 seconds without touch.
* Add placeholder IO graphs.

Result:

```text
User can switch between the current dashboard and the new IO dashboard.
```

---

## Phase 3 — Network, Disk, and Swap Monitoring

* Add network upload/download metrics.
* Add disk read/write metrics.
* Add swap usage metrics.
* Render them on the IO Page.

Result:

```text
ESP32 can monitor CPU/GPU/RAM and Network/Disk/Swap in separate pages.
```

---

## Phase 4 — Settings Screen

* Add top-right Settings button.
* Create fullscreen Settings screen.
* Move LED brightness slider into Settings.
* Expand the right-side status cards on the main dashboard.

Result:

```text
The main dashboard is cleaner and controls are grouped in Settings.
```

---

## Phase 5 — Jetson IP Display

* Add IP request command.
* Add IP response display.
* Show interface, SSID, IP address, and connection status.

Result:

```text
User can view Jetson local IP directly from the touchscreen.
```

---

## Phase 6 — Wi-Fi Scan

* Add Wi-Fi setup button.
* Scan Wi-Fi networks from Jetson.
* Display SSID, signal strength, and security type.
* Mark the currently connected Wi-Fi with a check mark.

Result:

```text
User can view available Jetson Wi-Fi networks from the touchscreen.
```

---

## Phase 7 — Virtual Keyboard

* Add standalone keyboard module.
* Support password input.
* Support delete, shift, numbers/symbols, submit, and cancel.
* Mask password text.

Result:

```text
User can enter Wi-Fi passwords directly on the touchscreen.
```

---

## Phase 8 — Wi-Fi Connect Flow

* Send selected SSID and password to Jetson.
* Jetson connects using NetworkManager.
* ESP32 shows success or failure.
* Wrong password allows retry.
* Exit remains available at all times.

Result:

```text
User can configure Jetson Wi-Fi directly from the ESP32 touchscreen.
```

---

## 15. Final Expected Behavior

After the upgrade:

```text
Default screen:
- Smooth CPU/GPU/RAM dashboard at 3 FPS

Touch interaction:
- Temporary circular arrow appears
- User can switch to Network/Disk/Swap dashboard
- Arrow disappears after 2 seconds

IO dashboard:
- Network upload/download graph
- Disk read/write graph
- Swap usage graph

Settings:
- LED brightness slider
- Wi-Fi setup
- Jetson IP display
- Exit button

Wi-Fi setup:
- Scan available Wi-Fi networks
- Show connected Wi-Fi with check mark
- Select unconnected Wi-Fi
- Enter password using virtual keyboard
- Retry if password is wrong
- Exit when needed
```

---

## 16. Design Rules

* Preserve the existing dashboard as the default page.
* Use 3 Hz sampling for live graph metrics.
* Use 3 FPS rendering for graphs.
* Separate fast metrics from slow status updates.
* Keep temporary controls hidden unless needed.
* Keep Wi-Fi logic on the Jetson.
* Keep ESP32 focused on UI, touch input, and UART commands.
* Keep the virtual keyboard in a separate module.
* Do not display or log raw Wi-Fi passwords.
* Always provide an Exit path from Settings, Wi-Fi, and keyboard screens.
