# PsSwitch3_IDF

## Overview
ESP-IDF based smart switch device that supports both plug and fan configurations with Hue bridge protocol integration, web interface control, and OTA updates.

## Project Structure
```
PsSwitch3_IDF/
├── main/                 # Main application source code
│   ├── main.c            # Entry point and initialization
│   └── CMakeLists.txt    # Build configuration for main component
├── components/           # Reusable components
│   ├── wifi_manager/     # WiFi connection management
│   ├── webserver/        # Web-based user interface
│   ├── hue/              # Hue bridge protocol implementation
│   ├── buttons/          # Button input handling
│   ├── ota/              # Over-The-Air firmware updates
│   ├── persistent_data/  # Non-volatile storage
│   ├── ssdp/             # SSDP (Simple Service Discovery Protocol)
│   ├── templates/        # HTML templates for web interface
│   └── config/           # Configuration headers
├── CMakeLists.txt        # Top-level build configuration
├── partitions.csv        # Partition table
└── sdkconfig             # ESP-IDF configuration
```

## Key Components

### main/main.c
- Entry point that initializes the device
- Sets up WiFi connection, web server, and OTA updates
- Initializes button handling
- Starts Hue protocol implementation

### components/config/Global.h
- Device configuration including:
  - Device type selection (plug or fan)
  - GPIO pin configurations for inputs and outputs
  - Debug settings
  - Hardware-specific definitions
- Supports both plug (single output) and fan (three-speed outputs) configurations
- Defines GPIO mask calculations for low/high GPIO banks

### components/hue/hue.c/hue.h
- Implements Philips Hue bridge protocol for smart home integration
- Provides functions to:
  - Get/set device state, brightness, timer settings
  - Toggle state and apply outputs
  - Manage timers and start/stop functionality

### components/webserver/webserver.c/webserver.h
- Provides web-based user interface for device configuration and control
- Handles HTTP requests for device management
- Includes provisioning and main interface pages

### components/wifi_manager/wifi_manager.c/wifi_manager.h
- Handles WiFi connection management with callbacks for connection events
- Supports saving and connecting to WiFi networks

### components/buttons/buttons_handler.c/buttons_handler.h
- Manages button input handling for local device control

### components/ota/ota_manager.c/ota_manager.h
- Enables Over-The-Air firmware updates
- Handles OTA HTTP request processing

## Device Capabilities
- Support for both plug (single output) and fan (three-speed outputs) configurations
- Web-based control interface
- Hue bridge protocol support for smart home integration
- OTA firmware updates
- WiFi connectivity management
- SSDP discovery for network service discovery
- Button input handling for local control

## Configuration
Device type can be configured in `components/config/Global.h`:
- `DEVICE_TYPE PLUG` - Single output configuration
- `DEVICE_TYPE VENTILADOR` - Three-speed output configuration

GPIO pins are configurable and automatically calculate low/high bank masks for proper GPIO control.