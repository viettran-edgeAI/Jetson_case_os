# Jetson Shield OS

## Project Introduction

Jetson Shield OS is a transparent enclosure project for the Jetson Orin Nano Super. An ESP32 acts as a companion controller connected to the Jetson through two serial ports: one channel carries runtime parameters and the LCD_2 terminal bridge, and the second channel captures kernel and debugging messages. The ESP32 then presents system status, alerts, terminal sessions, and boot or shutdown activity across local displays, lighting, and cooling hardware.

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
  <img src="imgs/diagrams/hardware_diagram.jpg" alt="Jetson Shield OS hardware diagram" width="95%" />
</p>

## Software Diagram

```mermaid
flowchart LR
	Jetson[Jetson Orin Nano Super]

	subgraph Ingress[ESP32 Input Tasks]
		Serial1[Serial1 Task<br/>Runtime Metrics]
		Terminal[Serial1 Terminal Frames<br/>tmux Mirror]
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
	Jetson --> Terminal
	Jetson --> Serial2
	Serial1 --> Queue
	Terminal --> Queue
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

[![Watch the Jetson Shield OS short video](https://img.youtube.com/vi/BpIBj3cyqB0/maxresdefault.jpg)](https://youtu.be/BpIBj3cyqB0)

Watch the short demo here: https://youtu.be/BpIBj3cyqB0
