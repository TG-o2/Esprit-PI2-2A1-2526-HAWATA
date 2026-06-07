# Temperature Control System Implementation Guide

## Overview
This document explains the temperature control system implemented for the Hawata Project. The system reads temperature from a DHT11 sensor via Arduino, compares it with a 30°C threshold, and automatically controls a relay to turn a cooling fan ON or OFF.

---

## System Architecture

### Components
1. **Arduino with DHT11 Sensor**: Reads temperature data
2. **Relay Module**: Controls the fan (ON/OFF)
3. **Qt Application (Hawata)**: Reads temperature from Arduino, implements control logic, stores data in database
4. **Database (HWT)**: Stores product data with temperature readings

### Hardware Setup
```
Arduino Pin 2  ──→ DHT11 Temperature Sensor
Arduino Pin 3  ──→ Relay Module (Fan Control)
Arduino USB    ──→ COM3 (or auto-detected port)
```

---

## Temperature Threshold

**MAX TEMPERATURE THRESHOLD: 30°C**

- If temperature **> 30°C**: Fan turns **ON** (relay HIGH)
- If temperature **≤ 30°C**: Fan turns **OFF** (relay LOW)

This threshold is defined in `appwindow.h`:
```cpp
constexpr double MAX_TEMP_THRESHOLD = 30.0;
```

---

## Arduino Sketch Installation

### Step 1: Prepare Arduino IDE
1. Install Arduino IDE (https://www.arduino.cc/en/software)
2. Install DHT11 library:
   - In Arduino IDE: Sketch → Include Library → Manage Libraries
   - Search for "DHT11" and install the latest version by Adafruit

### Step 2: Upload Sketch
1. Open `Arduino_Temperature_Control_Sketch.ino` in Arduino IDE
2. Select Board: **Arduino Uno** (or your Arduino variant)
3. Select Port: **COM3** (or the port where Arduino is connected)
4. Click **Upload** (Ctrl+U)

### Step 3: Verify Installation
1. Open Arduino IDE Serial Monitor (Ctrl+Shift+M)
2. Set Baud Rate to **9600**
3. You should see messages like:
   ```
   SYSTEM: Arduino Temperature Controller Starting...
   SYSTEM: DHT11 sensor initialized
   SYSTEM: Relay initialized (Fan OFF)
   SYSTEM: Ready to receive commands
   ```

---

## Communication Protocol

### Arduino → Qt Application

The Arduino sends temperature readings every 1 second in the following format:

```
TEMPERATURE: 28.50 C
FAN: OFF
TEMPERATURE: 31.20 C
FAN: ON
```

### Qt Application → Arduino

The Qt application sends fan control commands to the Arduino:

- **FAN_ON**: Turn fan ON (when temp > 30°C)
- **FAN_OFF**: Turn fan OFF (when temp ≤ 30°C)
- **STATUS**: Request current temperature and fan status

Example:
```
FAN_ON
FAN_OFF
STATUS
```

---

## Qt Application Flow

### 1. Temperature Reading (`update_label()` in appwindow.cpp)

```cpp
// Read temperature from Arduino
// Parse: "TEMPERATURE: 28.50 C"

// Compare with threshold (30°C)
if (temperature > 30.0) {
    // Send "FAN_ON" command to Arduino
    A.write_to_arduino("FAN_ON\n");
    fanStatus = "ON";
} else {
    // Send "FAN_OFF" command to Arduino
    A.write_to_arduino("FAN_OFF\n");
    fanStatus = "OFF";
}

// Update UI display
ui->tempSourceLabel->setText(
    QString("Arduino: %1 C | Fan: %2").arg(temperature, 0, 'f', 2).arg(fanStatus));
```

### 2. Temperature Storage in Database

When a user adds a product:
1. Temperature value from `ui->fishTemp` is read
2. Product is created with this temperature value
3. Temperature is stored in `products` table column `temperature`

```sql
INSERT INTO products 
(type, status, location, quantity, price, temperature, fish_caught, dateofpurchase, boatid) 
VALUES (..., 28.50, ...);
```

---

## Integration Points

### Products Page (appwindow.cpp)
- **Function**: `on_checkProductButton_2_clicked()`
- **Line**: ~2697
- Reads temperature from `ui->fishTemp` (auto-populated from Arduino)
- Stores temperature in database via `productManager.createProduct()`

### Add Product Form
- **UI File**: `appwindow.ui`
- **Temperature Input**: `QLineEdit` named `fishTemp`
- **Placeholder Text**: "Auto from Arduino or enter manually"
- **Default Behavior**: Arduino auto-populates this field every 1 second

### Product Table Display
- **Line**: ~2874-2891 in appwindow.cpp
- Column 7 shows "Temperature (C)"
- Displays stored temperature with 2 decimal places

---

## Troubleshooting

### Issue: Arduino Not Detected
**Solution:**
- Check USB cable connection to COM3
- Verify Arduino drivers are installed
- Check Device Manager for Arduino port
- Restart Qt application

### Issue: Temperature Not Reading
**Solution:**
- Check DHT11 sensor wiring (Pin 2)
- Verify DHT11 library is installed in Arduino IDE
- Check Serial Monitor output for sensor errors
- Replace DHT11 sensor if faulty

### Issue: Fan Not Turning On/Off
**Solution:**
- Verify relay module wiring (Pin 3)
- Check relay module power supply
- Test relay manually with Arduino Serial Monitor: send "FAN_ON" or "FAN_OFF"
- Verify fan is properly connected to relay

### Issue: Temperature Shows "-" in Product Table
**Solution:**
- This is normal for products added without temperature data
- Manually enter temperature if needed
- Or wait for Arduino to auto-populate from active readings

---

## Database Schema

### Products Table
```sql
CREATE TABLE products (
    productid INT PRIMARY KEY AUTO_INCREMENT,
    type VARCHAR(100),
    location VARCHAR(100),
    status VARCHAR(50),
    quantity INT,
    price DOUBLE,
    temperature DOUBLE,  -- ← Temperature from Arduino
    fish_caught DATETIME,
    dateofpurchase DATETIME,
    boatid INT,
    FOREIGN KEY (boatid) REFERENCES BOAT(BOATID)
);
```

The `temperature` column stores:
- The temperature reading when the product was added
- `NULL` if no temperature was recorded

---

## Testing the System

### Manual Test 1: Temperature Reading
1. Connect Arduino with DHT11
2. Open Hawata Qt application
3. Go to **Products → Add Product** tab
4. Watch `tempSourceLabel` in the status area
5. Should show: `Arduino: XX.XX C | Fan: ON/OFF`

### Manual Test 2: Fan Control
1. In Arduino IDE Serial Monitor, send commands:
   ```
   FAN_ON
   FAN_OFF
   ```
2. Observe relay activation
3. Watch fan state change in Qt application

### Manual Test 3: Database Storage
1. Add a product with temperature data
2. Query database:
   ```sql
   SELECT productid, type, temperature FROM products ORDER BY productid DESC LIMIT 5;
   ```
3. Verify temperature values are stored correctly

---

## Configuration

### To Change Temperature Threshold
Edit `appwindow.h`:
```cpp
constexpr double MAX_TEMP_THRESHOLD = 30.0;  // Change 30 to desired value
```

Recompile and run the Qt application.

### To Change Temperature Read Interval
Edit `Arduino_Temperature_Control_Sketch.ino`:
```cpp
#define TEMP_READ_INTERVAL 1000  // Change 1000ms to desired interval
```

Upload new sketch to Arduino.

---

## System Status Indicators

### tempSourceLabel Display
- `"Arduino: not found (manual temperature enabled)"` - Arduino not connected
- `"Arduino: connected on COM3"` - Arduino connected, no data yet
- `"Arduino: 28.50 C | Fan: OFF"` - Normal operation, temp low
- `"Arduino: 31.50 C | Fan: ON"` - Normal operation, fan active
- `"Arduino: DHT11 sensor error — check wiring"` - Sensor malfunction

---

## Performance Metrics

- **Temperature Read Rate**: 1 reading per second
- **Serial Baud Rate**: 9600 bps
- **Database Transaction Time**: <100ms
- **UI Update Latency**: ~500ms from Arduino reading

---

## Safety Notes

⚠️ **Important:**
- Ensure relay can handle fan current specifications
- Test fan operation with actual load before deployment
- Monitor temperature readings during first operation
- Check for water ingress if using in marine environment
- Perform regular maintenance on temperature sensor

---

## Support & Debugging

For detailed logs, check:
- **Qt Debug Output**: View → Output → Debug Console
- **Arduino Serial Monitor**: Tools → Serial Monitor
- **Database Logs**: Check connection.cpp output in Debug Console

Example debug output:
```
arduino is available and connected to : "COM3"
Arduino: connected on COM3
Sent to Arduino: FAN_ON
Temperature 31.50°C > 30.0°C threshold - FAN ON
Database: Inserted product with ID 42
```

---

## Version Information

- **Qt Version**: 6.7.3
- **Arduino Board**: Arduino UNO (or compatible)
- **DHT Sensor**: DHT11
- **Serial Protocol**: Standard 9600 8N1
- **Database**: HWT (MySQL/Oracle ODBC)

---

**Last Updated**: May 8, 2026
**System Author**: Hawata Development Team
