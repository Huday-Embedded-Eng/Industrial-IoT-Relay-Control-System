Industrial IoT Relay Control System using STM32 & ESP8266
📌 Overview

This project demonstrates an Industrial IoT Relay Control System built using an STM32F411 microcontroller and an ESP8266 WiFi module.
The system allows users to control a relay remotely through HTTP commands from a web browser.

A 16x2 I2C LCD is used to display the WiFi status and relay state in real time.

This project showcases key embedded concepts including UART communication, interrupt-driven data handling, AT command processing, and HTTP request parsing.

⚙️ System Architecture
Browser / Mobile
       │
       │ HTTP Request
       ▼
ESP8266 WiFi Module
       │ UART Communication
       ▼
STM32F411 Microcontroller
       │
 ┌─────┴─────┐
 │           │
Relay      16x2 LCD
Control     Status Display


🌐 HTTP Commands

The relay can be controlled using a web browser.

http://<ESP_IP>/ledon
http://<ESP_IP>/ledoff
Command	Function
/ledon	Turns relay ON
/ledoff	Turns relay OFF

🛠 Hardware Components

STM32F411 Nucleo Board

ESP8266 WiFi Module (ESP-01)

5V Relay Module (Active Low)

16x2 LCD with I2C Interface

3.3V Regulated Supply for ESP8266

💻 Software Concepts
Embedded C Programming

Register-Level STM32 Peripheral Configuration

UART Interrupt Communication

Circular Buffer Implementation

ESP8266 AT Command Interface

HTTP Request Parsing

I2C LCD Driver

📡 Key Features
Remote relay control via HTTP

Interrupt-driven UART reception

Non-blocking circular buffer

Automatic WiFi reconnection

Real-time status display on LCD

UART debug output support

🎯 Learning Outcomes
STM32 peripheral initialization

UART interrupt handling

ESP8266 WiFi communication using AT commands

Embedded string parsing

Basic IoT system development
