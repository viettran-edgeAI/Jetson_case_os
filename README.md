# Jetson Shield OS

## Project Introduction

Jetson Shield OS is a transparent enclosure project for the Jetson Orin Nano Super. An ESP32 acts as a companion controller connected to the Jetson through two serial ports: one channel carries runtime parameters, and the second channel captures kernel and debugging messages. The ESP32 then presents system status, alerts, and boot or shutdown activity across local displays, lighting, and cooling hardware.

## Hardware

- Jetson Orin Nano Super
- ESP32
- Transparent case / shield enclosure
- PWM fan
- LEDs
- OLED LCD
- TFT touch screen
- Sensors
- Buttons
- Switches

## Hardware Diagram

<p align="center">
  <img src="imgs/diagrams/hardware_diagram.png" alt="Jetson Shield OS hardware diagram" width="95%" />
</p>

## Software Diagram

```mermaid
flowchart LR
	Jetson[Jetson Orin Nano Super]

	subgraph Ingress[ESP32 Input Tasks]
		Serial1[Serial1 Task<br/>Runtime Metrics]
		Serial2[Serial2 Task<br/>Kernel / Debug Logs]
		Sensor[Sensor Task<br/>Temperature / Humidity]
	end

	Queue[(Message Queue / Bus)]

	subgraph Control[Jetson Shield OS]
		Controller[SystemController]
		State[State Machine<br/>POWER_OFF / BOOTING_ON<br/>RUNNING / SHUTTING_DOWN]
		Alerts[Alert Logic<br/>Temperature / Humidity]
	end

	subgraph Outputs[Peripheral Modules]
		OLED[OLED LCD]
		TFT[TFT Touch Screen]
		LED[LED Module]
		Fan[Fan Module]
	end

	Jetson --> Serial1
	Jetson --> Serial2
	Serial1 --> Queue
	Serial2 --> Queue
	Sensor --> Queue
	Queue --> Controller
	Controller --> State
	Controller --> Alerts
	Controller --> OLED
	Controller --> TFT
	Controller --> LED
	Controller --> Fan
	IO --> Controller
```

Additional assembly and design image galleries are collected in [docs/README.md](docs/README.md).

## Demo Images

<p align="center">
	<img src="imgs/demo/A1.jpg" alt="Jetson Shield OS demo 1" width="49%" />
	<img src="imgs/demo/A2.jpg" alt="Jetson Shield OS demo 2" width="49%" />
</p>

<p align="center">
	<img src="imgs/demo/A3.jpg" alt="Jetson Shield OS demo 3" width="49%" />
	<img src="imgs/demo/A4.jpg" alt="Jetson Shield OS demo 4" width="49%" />
</p>

<p align="center">
	<img src="imgs/demo/A5.jpg" alt="Jetson Shield OS demo 5" width="49%" />
	<img src="imgs/demo/A6.jpg" alt="Jetson Shield OS demo 6" width="49%" />
</p>

## Video

[![Watch the Jetson Shield OS short video](https://img.youtube.com/vi/KmZ7pbCNbDI/maxresdefault.jpg)](https://youtu.be/KmZ7pbCNbDI)

Watch the short demo here: https://youtu.be/KmZ7pbCNbDI

## Setup Guide

### 1. Create the Jetson service

The `system_monitor.c` program reads Jetson stats from `tegrastats` and sends a compact status line to the ESP32 over UART.

Build it on Jetson:

```bash
gcc -O2 -s -o system_monitor system_monitor.c
```

Copy the binary and service file into place:

```bash
sudo cp system_monitor /usr/local/bin/system_monitor
sudo cp system_monitor.service /etc/systemd/system/system_monitor.service
sudo systemctl daemon-reload
sudo systemctl enable --now system_monitor.service
```

Check that it is running:

```bash
sudo systemctl status system_monitor.service
sudo journalctl -u system_monitor.service -f
```

If your Jetson UART device is different, update the `ExecStart` line in `system_monitor.service` before enabling the service.

### 2. Upload the ESP32 code

The ESP32 firmware is inside the `Jetson_case_os/` folder.

1. Copy the whole `Jetson_case_os/` folder into your Arduino sketches directory.
2. Open `Jetson_case_os.ino` in Arduino IDE.
3. Select the correct ESP32 board and port.
4. Click Upload.

After uploading, connect the ESP32 to the Jetson and start the `system_monitor` service so the two sides can exchange data.