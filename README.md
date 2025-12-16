# Plant Monitoring IoT Device

> **CS4447 Embedded Systems Project**  
> Real-time plant monitoring system with ESP32, FreeRTOS, and cloud connectivity

## Overview

This project implements an IoT-based plant monitoring system that provides real-time temperature and soil moisture monitoring with configurable alert thresholds. The device features:

- **Dual-sensor monitoring**: Temperature (LM35/TMP36) and capacitive soil moisture sensing
- **Real-time alerts**: Visual (RGB LCD), audible (vibration module), and cloud notifications
- **Cloud connectivity**: MQTT-based telemetry and bi-directional command/control
- **Runtime configuration**: Update monitoring thresholds remotely via Node-RED dashboard
- **Offline resilience**: Buffers up to 512 samples during network outages
- **Persistent storage**: Threshold settings saved to NVS (non-volatile storage)

## Hardware Requirements

### Components

| Component | Model/Type | Connection |
|-----------|------------|------------|
| **Microcontroller** | ESP32 Firebeetle | Main board |
| **Temperature Sensor** | LM35 or TMP36 | Analog (GPIO 36) |
| **Soil Moisture Sensor** | Capacitive analog | ADC (GPIO 34) |
| **Alarm Output** | Vibration module or buzzer | Digital (GPIO 19) |
| **Display** | DFRobot 16x2 RGB LCD | I2C (SDA=21, SCL=22) |

### Pin Assignments

| Component | GPIO Pin | Function | Notes |
|-----------|----------|----------|-------|
| **Temperature Sensor** | GPIO 36 | ADC1_CH0 | Input-only, analog sensor |
| **Soil Moisture Sensor** | GPIO 34 | ADC1_CH6 | Capacitive sensor |
| **Vibration Module** | GPIO 19 | Digital output | Alarm/buzzer |
| **LCD SDA** | GPIO 21 | I2C data | 16x2 RGB display |
| **LCD SCL** | GPIO 22 | I2C clock | 100 kHz |

### Wiring Diagram

```
ESP32 Firebeetle          LM35 Temperature Sensor
┌──────────────┐         ┌─────────────┐
│              │         │             │
│   GPIO 36 ◄──┼─────────┤ Vout        │
│   3.3V    ───┼─────────┤ VCC         │
│   GND     ───┼─────────┤ GND         │
│              │         └─────────────┘
│              │         
│              │         Soil Moisture Sensor
│              │         ┌─────────────┐
│   GPIO 34 ◄──┼─────────┤ AOUT        │
│   3.3V    ───┼─────────┤ VCC         │
│   GND     ───┼─────────┤ GND         │
│              │         └─────────────┘
│              │         
│              │         Vibration Module
│              │         ┌─────────────┐
│   GPIO 19 ───┼────────►│ Signal      │
│   3.3V    ───┼─────────┤ VCC         │
│   GND     ───┼─────────┤ GND         │
│              │         └─────────────┘
│              │         
│              │         RGB LCD (I2C)
│              │         ┌─────────────┐
│   GPIO 21 ◄─►┼─────────┤ SDA         │
│   GPIO 22 ───┼────────►┤ SCL         │
│   5V      ───┼─────────┤ VCC         │
│   GND     ───┼─────────┤ GND         │
└──────────────┘         └─────────────┘
```

## Software Setup

### Prerequisites

- **ESP-IDF**: v5.0 or later
- **Python**: 3.8+ (for ESP-IDF tools)
- **Git**: For cloning repository

### Build Instructions

1. **Clone the repository:**
   ```bash
   git clone <repository-url>
   cd thermometer-embedded
   ```

2. **Configure WiFi and MQTT credentials:**
   ```bash
   cp main/secrets.hpp.defaults main/secrets.hpp
   ```
   
   Edit `main/secrets.hpp` with your credentials:
   ```cpp
   namespace Secrets {
       static constexpr const char* WIFI_SSID = "YourWiFiSSID";
       static constexpr const char* WIFI_PASSWORD = "YourWiFiPassword";
       static constexpr const char* MQTT_HOST = "your-mqtt-broker.com";
       static constexpr int MQTT_PORT = 1883;
       static constexpr const char* DEVICE_ID = "thermo-001";
   }
   ```

3. **Build the project:**
   ```bash
   idf.py build
   ```

4. **Flash to ESP32:**
   ```bash
   idf.py -p /dev/ttyUSB0 flash
   ```

5. **Monitor serial output:**
   ```bash
   idf.py -p /dev/ttyUSB0 monitor
   ```

## Default Thresholds

The device ships with the following default thresholds (defined in `main/config/config.hpp`):

### Temperature Thresholds (°C)

| Level | Low | High |
|-------|-----|------|
| **Critical** | 5.0°C | 32.0°C |
| **Warning** | 10.0°C | 28.0°C |

### Soil Moisture Thresholds (%)

| Level | Low | High |
|-------|-----|------|
| **Critical** | 20.0% | 90.0% |
| **Warning** | 35.0% | 80.0% |

**Note:** All thresholds can be updated at runtime via the Node-RED dashboard and are persisted to NVS.

## Device Operation

### Alert States

The device operates in three alert states, indicated by LCD backlight color:

#### 🟢 OK (Green LCD)
- **Condition**: All parameters within normal range
- **LCD Display**: `T:23.5C M:45.0%` / `OK`
- **Backlight**: Solid green (RGB: 0, 255, 0)
- **Alarm**: Silent

#### 🟠 WARNING (Orange LCD)
- **Condition**: One or more parameters in warning range
- **LCD Display**: `T:30.0C M:85.0%` / `Warn: T+M`
- **Backlight**: Solid orange (RGB: 255, 128, 0)
- **Alarm**: Single short beep (120ms)
- **Debounce**: Must persist for 5 seconds before triggering

#### 🔴 CRITICAL (Flashing Red LCD)
- **Condition**: One or more parameters in critical range
- **LCD Display**: `T:35.0C M:15.0%` / `Crit: T+M`
- **Backlight**: Flashing red (500ms cycle)
- **Alarm**: Repeating triple beep pattern (200ms on, 150ms off, ×3, every 2s)
- **Debounce**: Must persist for 3 seconds before triggering

### Alert Reasons

The LCD and cloud alerts show which parameter(s) triggered the alert:
- `T` - Temperature out of range
- `M` - Moisture out of range
- `T+M` - Both out of range

## Node-RED Dashboard

### Accessing the Dashboard

1. Ensure Node-RED is running on your server
2. Open browser to: `http://localhost:1880/ui` (or your server's address)
3. Select the "Greenhouse monitoring dashboard" tab

### Dashboard Layout

```
┌──────────────────────────────────────────────────┐
│  thermo-001                                      │
├──────────────────────────────────────────────────┤
│  Temperature: 14.8 °C      Moisture: 92.9 %     │
│                                                  │
│  WARNING : moisture_high                        │
│                                                  │
│  ┌─ Temperature Thresholds ──────────────────┐  │
│  │  Critical High: [25.0] ▲▼                 │  │
│  │  Warning High:  [20.0] ▲▼                 │  │
│  │  Warning Low:   [ 5.0] ▲▼                 │  │
│  │  Critical Low:  [ 0.0] ▲▼                 │  │
│  └────────────────────────────────────────────┘  │
│                                                  │
│  ┌─ Moisture Thresholds ─────────────────────┐  │
│  │  Critical High: [100.0] ▲▼                │  │
│  │  Warning High:  [ 90.0] ▲▼                │  │
│  │  Warning Low:   [ 35.0] ▲▼                │  │
│  │  Critical Low:  [ 20.0] ▲▼                │  │
│  └────────────────────────────────────────────┘  │
│                                                  │
│        [ Update Device Thresholds ]             │
│                                                  │
│  Thresholds updated: temp_low_warn = 5, ...     │
└──────────────────────────────────────────────────┘
```

### Using the Dashboard

#### Real-time Monitoring

The dashboard displays:
- **Current temperature** in °C (updates every 5 seconds)
- **Current soil moisture** as percentage (updates every 5 seconds)
- **Device state banner**:
  - Green "OK" when all parameters normal
  - Orange "WARNING : reason" when in warning range
  - Red "CRITICAL : reason" when in critical range

#### Updating Thresholds

1. **Adjust sliders** to desired threshold values
2. **Click "Update Device Thresholds"** button
3. **Confirmation appears** at bottom showing all updated values
4. **Device receives updates** via MQTT and stores to NVS
5. **New thresholds take effect** immediately (visible within 5 seconds)

**Example threshold adjustment:**
```
Original: Warning High Temperature = 28.0°C
Adjust to: Warning High Temperature = 25.0°C
→ Click "Update Device Thresholds"
→ Device receives command and updates threshold
→ Status shows: "Thresholds updated: temp_high_warn = 25, ..."
```

### Dashboard States (Visual Reference)

The dashboard appearance changes based on device state:

1. **CRITICAL State** (Red Banner)
   - Large red "CRITICAL : reason" banner
   - Indicates critical threshold violation
   - Device vibration module activates with repeating pattern

2. **OK State** (Green Banner)
   - Large green "OK" banner
   - All parameters within normal range
   - No alarms active

3. **WARNING State** (Orange Banner)
   - Large orange "WARNING : reason" banner
   - Indicates warning threshold violation
   - Device beeps once

## MQTT Topics and Protocol

### Topic Schema

All topics follow the pattern: `thermometer/{device_id}/{topic}`

Replace `{device_id}` with your configured device ID (e.g., `thermo-001`).

### Telemetry Topics (Device → Cloud)

#### Temperature Readings
**Topic:** `thermometer/{device_id}/temperature`

**Payload:**
```json
{
  "value": 23.5,
  "ts": "20251216211745",
  "buffered": 0
}
```
- `value`: Temperature in °C
- `ts`: ISO-8601-like timestamp (YYYYMMDDHHmmss)
- `buffered`: 1 if this is buffered data from offline period, 0 for live

**Publish Rate:** Every 5 seconds (when connected)

#### Soil Moisture Readings
**Topic:** `thermometer/{device_id}/moisture`

**Payload:**
```json
{
  "percent": 45.2,
  "ts": "20251216211745",
  "buffered": 0
}
```
- `percent`: Soil moisture percentage (0-100%)
- `ts`: Timestamp
- `buffered`: 1 if buffered, 0 if live

**Publish Rate:** Every 5 seconds (when connected)

#### Alert Messages
**Topic:** `thermometer/{device_id}/alert`

**Payload:**
```json
{
  "state": "CRITICAL",
  "reason": "temp_high",
  "temp": 35.2,
  "moisture": 45.0,
  "ts": "20251216211745"
}
```
- `state`: "OK", "WARNING", or "CRITICAL"
- `reason`: "clear", "temp_high", "temp_low", "moisture_low", "moisture_high"
- `temp`: Current temperature
- `moisture`: Current moisture percentage
- `ts`: Timestamp

**Publish Trigger:** On state transitions

#### Status Messages
**Topic:** `thermometer/{device_id}/status`

**Payload:**
```json
{
  "status": "online",
  "uptime_ms": 123456,
  "buffered": 0,
  "buffered_temp": 0,
  "buffered_moist": 0,
  "state": "OK",
  "reasons": []
}
```
- `status`: "online" or "offline" (via Last Will & Testament)
- `uptime_ms`: Device uptime in milliseconds
- `buffered`: Total buffered samples waiting to send
- `state`: Current alert state
- `reasons`: Array of active alert reasons (if any)

**Publish Rate:** Every 5 seconds  
**Retained:** Yes (QoS 1 retained for LWT)

#### Threshold Change Acknowledgment
**Topic:** `thermometer/{device_id}/thresholds-changed`

**Payload:**
```json
{
  "changes": {
    "temp_high_warn": 30.0,
    "moisture_low_crit": 15.0
  },
  "ts": "20251216211745",
  "status": "ok"
}
```
- `changes`: Object containing all thresholds that were updated
- Only includes thresholds that changed
- Sent after successful threshold update

### Command Topic (Cloud → Device)

#### Command Subscription
**Topic:** `thermometer/{device_id}/cmd`

The device subscribes to this topic to receive commands.

#### Single Threshold Update

**Payload:**
```json
{
  "command": "update_threshold",
  "threshold": "temp_high_warn",
  "value": 30.0
}
```

**Valid threshold names:**
- Temperature: `temp_low_warn`, `temp_low_crit`, `temp_high_warn`, `temp_high_crit`
- Moisture: `moisture_low_warn`, `moisture_low_crit`, `moisture_high_warn`, `moisture_high_crit`

#### Batch Threshold Update

**Payload:**
```json
{
  "command": "update_thresholds",
  "temp_high_crit": 32.0,
  "temp_high_warn": 28.0,
  "temp_low_warn": 10.0,
  "temp_low_crit": 5.0,
  "moisture_high_crit": 90.0,
  "moisture_high_warn": 80.0,
  "moisture_low_warn": 35.0,
  "moisture_low_crit": 20.0
}
```

Include any subset of the 8 thresholds - only specified thresholds will be updated.

## Node-RED Dashboard Setup

### Importing the Flow

1. Open Node-RED: `http://localhost:1880`
2. Click the **hamburger menu** (top right) → **Import**
3. Select the **node-red-flow.json** file from this repository
4. Click **Import**
5. Deploy the flow

### Configuring MQTT Connection

After importing:
1. Open any MQTT node (double-click)
2. Edit the MQTT broker configuration
3. Set your broker address (e.g., `localhost:1883`)
4. Update the device ID in topic strings if needed (default: `thermo-001`)
5. Deploy changes

### Dashboard Usage

Access the dashboard at: `http://localhost:1880/ui`

#### Viewing Real-time Data

The dashboard automatically displays:
- Live temperature and moisture readings (refresh every 5 seconds)
- Current device state (OK/WARNING/CRITICAL)
- Alert reasons when applicable

#### Adjusting Thresholds

1. Use the **slider controls** to set desired thresholds
2. Click **"Update Device Thresholds"** button
3. Wait for confirmation message at bottom
4. Device receives updates via MQTT and saves to NVS
5. New thresholds take effect immediately

**Example Use Cases:**

- **Warmer climate:** Increase high temperature thresholds
- **Dry season:** Decrease low moisture thresholds
- **Sensitive plants:** Narrow the acceptable range

## Architecture and Technical Details

### System Architecture

```
┌─────────────────────────────────────────────────────┐
│                    ESP32 Device                     │
│                                                     │
│  ┌──────────────┐  ┌──────────────┐               │
│  │ Temperature  │  │ Soil Moisture│               │
│  │    Task      │  │    Task      │               │
│  │  (HIGH pri)  │  │  (HIGH pri)  │               │
│  └──────┬───────┘  └──────┬───────┘               │
│         │                  │                        │
│         └────────┬─────────┘                        │
│                  ▼                                  │
│         ┌────────────────┐                          │
│         │ Plant Monitor  │                          │
│         │     Task       │                          │
│         │  (HIGH pri)    │                          │
│         └────┬──────┬────┘                          │
│              │      │                               │
│      ┌───────┘      └───────┐                       │
│      ▼                      ▼                       │
│  ┌────────┐            ┌────────┐                   │
│  │ Alarm  │            │  LCD   │                   │
│  │  Task  │            │  Task  │                   │
│  └────────┘            └────────┘                   │
│                                                     │
│  ┌──────────────────────────────┐                   │
│  │  Cloud Communication Task    │                   │
│  │       (NORMAL pri)            │                   │
│  └──────────────────────────────┘                   │
│         │                  ▲                        │
└─────────┼──────────────────┼────────────────────────┘
          │ MQTT Publish     │ MQTT Subscribe
          ▼                  │
┌─────────────────────────────────────────────────────┐
│              MQTT Broker (QoS 1)                    │
└─────────────────────────────────────────────────────┘
          │                  ▲
          ▼                  │
┌─────────────────────────────────────────────────────┐
│              Node-RED Dashboard                     │
│   • Visualize telemetry                            │
│   • Send threshold commands                        │
└─────────────────────────────────────────────────────┘
```

### Task Priorities

| Task | Priority | Period | Purpose |
|------|----------|--------|---------|
| **Temperature Sensor** | HIGH | 1s | Real-time data acquisition |
| **Soil Moisture Sensor** | HIGH | 1s | Real-time data acquisition |
| **Plant Monitoring** | HIGH | 100ms | Control logic and state machine |
| **Alarm Control** | CRITICAL | Event-driven | Safety-critical alarm response |
| **LCD Display** | NORMAL | Event-driven | User interface updates |
| **Cloud Communication** | NORMAL | 100ms | Network I/O and MQTT |
| **Command Handler** | NORMAL | Blocking | Process threshold updates |

### Memory Management

**Static Allocation Strategy:**
- All FreeRTOS queues created with `xQueueCreateStatic()`
- All tasks created with `xTaskCreateStatic()`
- Zero heap allocation in real-time task loops
- JSON parsing uses mjson (zero-allocation in-place parsing)
- JSON creation uses snprintf with static buffers

**Queue Sizes:**
- Temperature data: 32 samples
- Moisture data: 16 samples
- Alarm events: 16 events
- Commands: 16 commands
- Offline buffers: 512 samples each (temperature & moisture)

### Resilience Features

1. **WiFi Reconnection**: Automatic retry with 30-second backoff
2. **MQTT Reconnection**: Automatic on network restore
3. **Offline Buffering**: Up to 512 samples buffered during disconnection
4. **Data Flush**: Buffered data automatically published on reconnect
5. **Last Will & Testament**: Broker publishes "offline" status on disconnect
6. **Watchdog Timer**: 8-second timeout for safety-critical tasks
7. **Sensor Recovery**: Automatic retry on sensor initialization failure

## Calibration

### Soil Moisture Sensor

The soil moisture sensor may need calibration for your specific sensor:

1. **Measure dry reading:**
   - Keep sensor in air (completely dry)
   - Note the raw ADC value from serial logs
   - Update `Config::Hardware::Moisture::raw_dry` in `config.hpp`

2. **Measure wet reading:**
   - Submerge sensor in water (or saturated soil)
   - Note the raw ADC value from serial logs
   - Update `Config::Hardware::Moisture::raw_wet` in `config.hpp`

3. **Rebuild and reflash**

Default calibration values: `raw_dry = 0`, `raw_wet = 2700`

### Temperature Sensor

The temperature sensor (LM35/TMP36) is factory-calibrated with 0.1°C/mV gain. No calibration needed for typical use.

## Troubleshooting

### WiFi Connection Issues

**Symptom:** Device logs "WiFi init failed" or repeatedly disconnects

**Solutions:**
1. Verify SSID and password in `secrets.hpp`
2. Check router compatibility (2.4 GHz required, not 5 GHz)
3. Ensure ESP32 is within WiFi range
4. Check `Config::Wifi::max_retry_count` (default: 5)

### MQTT Connection Issues

**Symptom:** Device logs "MQTT connection failed"

**Solutions:**
1. Verify MQTT broker address and port in `secrets.hpp`
2. Ensure broker is accessible from ESP32 network
3. Check broker allows anonymous connections (or add auth to secrets)
4. Verify firewall settings on broker

### Sensor Not Reading

**Symptom:** Log shows "Sensor init failed" or "Temperature read failed"

**Solutions:**
1. Check wiring connections (see pin assignments above)
2. Verify power supply (3.3V or 5V as appropriate)
3. For temperature sensor: Check LM35 vs TMP36 (different pinouts!)
4. For moisture sensor: Verify ADC channel in `config.hpp`

### Thresholds Not Updating

**Symptom:** Commands sent but thresholds don't change

**Solutions:**
1. Check serial monitor for "MQTT RX parsed" messages
2. Verify JSON command format matches examples above
3. Check threshold name spelling (case-sensitive)
4. Ensure value is within valid range (-50 to 100°C, 0 to 100%)

### Buffer Overflow

**Symptom:** Log shows "queue full" warnings

**Solutions:**
1. This is normal during network outages (buffers fill up)
2. Device will flush buffers on reconnect
3. If persistent, check that cloud communication task is running
4. Verify MQTT broker is accepting connections

## Advanced Configuration

All system parameters can be customized in `main/config/config.hpp`:

### Network Settings
```cpp
Config::Wifi::max_retry_count = 5;
Config::Wifi::reconnect_backoff_ms = 1000;
Config::Mqtt::keepalive_seconds = 60;
Config::Mqtt::default_qos = 1;
```

### Task Timing
```cpp
Config::Tasks::Temperature::period_ms = 1000;  // Sensor sampling
Config::Tasks::Moisture::period_ms = 1000;
Config::Tasks::Cloud::telemetry_period_ms = 5000;  // Publish rate
```

### Alert Behavior
```cpp
Config::Monitoring::confirm_warn_ms = 5000;  // Warning debounce
Config::Monitoring::confirm_crit_ms = 3000;  // Critical debounce
Config::Monitoring::clear_hysteresis_c = 1.0f;  // Temperature hysteresis
```

### Feature Toggles
Enable/disable subsystems for testing:
```cpp
Config::Features::enable_cloud_comm = true;
Config::Features::enable_temperature_task = true;
Config::Features::enable_moisture_task = true;
Config::Features::enable_alarm_task = true;
Config::Features::enable_lcd_task = true;
```

## License

[Your license information here]

## Authors

[Your team members here]

## Acknowledgments

- Built with ESP-IDF framework
- Uses mjson library for zero-allocation JSON parsing
- DFRobot RGB LCD library adapted for ESP32
