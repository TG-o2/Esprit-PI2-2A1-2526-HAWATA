/*
 * Arduino Temperature Control Sketch for Hawata Project
 * 
 * Hardware:
 * - DHT11 temperature sensor on pin 2
 * - Relay module on pin 3 (controls fan)
 * - Serial communication at 9600 baud (USB/COM port)
 * 
 * Features:
 * - Reads temperature from DHT11 sensor
 * - Sends temperature data to Qt application via serial
 * - Receives FAN_ON/FAN_OFF commands from Qt application
 * - Controls relay based on received commands
 * - Temperature threshold for auto fan control: 30°C (set by Qt app)
 */

#include <DHT.h>

// ───────────────────────────────────────────────────────────────────
// Pin Configuration
// ───────────────────────────────────────────────────────────────────
#define DHTPIN 2          // DHT11 data pin
#define DHTTYPE DHT11     // DHT11 sensor type
#define RELAYPIN 3        // Relay control pin (HIGH = fan ON, LOW = fan OFF)
#define BAUD_RATE 9600    // Serial communication speed

// Many relay modules are active-low: LOW energizes the relay, HIGH turns it off.
// Keep this configurable so the sketch matches the actual hardware wiring.
#define RELAY_ACTIVE_LOW true

// ───────────────────────────────────────────────────────────────────
// Constants
// ───────────────────────────────────────────────────────────────────
#define TEMP_READ_INTERVAL 1000    // Read temperature every 1000ms (1 second)
#define MAX_COMMAND_LENGTH 20

// ───────────────────────────────────────────────────────────────────
// Global Variables
// ───────────────────────────────────────────────────────────────────
DHT dht(DHTPIN, DHTTYPE);
unsigned long lastTempReadTime = 0;
bool fanState = false;  // false = OFF, true = ON
String incomingCommand = "";

void setFanState(bool state);

// ───────────────────────────────────────────────────────────────────
// Setup
// ───────────────────────────────────────────────────────────────────
void setup() {
  // Initialize serial communication
  Serial.begin(BAUD_RATE);
  delay(1000);
  Serial.println("SYSTEM: Arduino Temperature Controller Starting...");
  
  // Initialize DHT sensor
  dht.begin();
  delay(500);
  Serial.println("SYSTEM: DHT11 sensor initialized");
  
  // Initialize relay pin
  pinMode(RELAYPIN, OUTPUT);
  setFanState(false);  // Start with fan OFF
  delay(100);
  Serial.println("SYSTEM: Relay initialized (Fan OFF)");
  Serial.println("SYSTEM: Ready to receive commands");
  Serial.println("SYSTEM: Commands: FAN_ON, FAN_OFF, STATUS");
}

// ───────────────────────────────────────────────────────────────────
// Main Loop
// ───────────────────────────────────────────────────────────────────
void loop() {
  // Check for incoming commands from Qt application
  if (Serial.available() > 0) {
    char ch = Serial.read();
    
    if (ch == '\n' || ch == '\r') {
      if (incomingCommand.length() > 0) {
        processCommand(incomingCommand);
        incomingCommand = "";  // Clear buffer
      }
    } else {
      incomingCommand += ch;
    }
  }
  
  // Read and send temperature at regular intervals
  unsigned long currentTime = millis();
  if (currentTime - lastTempReadTime >= TEMP_READ_INTERVAL) {
    lastTempReadTime = currentTime;
    readAndSendTemperature();
  }
}

// ───────────────────────────────────────────────────────────────────
// Read and Send Temperature
// ───────────────────────────────────────────────────────────────────
void readAndSendTemperature() {
  // Read temperature from DHT11
  float temperature = dht.readTemperature();
  
  // Check if reading is valid
  if (isnan(temperature)) {
    Serial.println("ERROR: DHT11 sensor error - unable to read temperature!");
    return;
  }
  
  // Send temperature to Qt application in format it expects
  // Format: "TEMPERATURE: XX.XX C"
  Serial.print("TEMPERATURE: ");
  Serial.print(temperature, 2);  // 2 decimal places
  Serial.println(" C");
  
  // Also send fan status
  if (fanState) {
    Serial.println("FAN: ON");
  } else {
    Serial.println("FAN: OFF");
  }
}

// ───────────────────────────────────────────────────────────────────
// Process Incoming Commands
// ───────────────────────────────────────────────────────────────────
void processCommand(String command) {
  // Convert to uppercase for case-insensitive comparison
  command.toUpperCase();
  command.trim();
  
  if (command == "FAN_ON") {
    setFanState(true);
    Serial.println("COMMAND: Fan turned ON by Qt application");
    
  } else if (command == "FAN_OFF") {
    setFanState(false);
    Serial.println("COMMAND: Fan turned OFF by Qt application");
    
  } else if (command == "STATUS" || command == "GET_STATUS") {
    // Send current status
    float temperature = dht.readTemperature();
    if (!isnan(temperature)) {
      Serial.print("STATUS: Temperature = ");
      Serial.print(temperature, 2);
      Serial.print(" C, Fan = ");
      Serial.println(fanState ? "ON" : "OFF");
    } else {
      Serial.println("STATUS: Sensor error");
    }
    
  } else {
    // Unknown command - log it
    Serial.print("COMMAND: Unknown - '");
    Serial.print(command);
    Serial.println("'");
  }
}

// ───────────────────────────────────────────────────────────────────
// Set Fan State
// ───────────────────────────────────────────────────────────────────
void setFanState(bool state) {
  const bool relayOutputHigh = RELAY_ACTIVE_LOW ? !state : state;
  digitalWrite(RELAYPIN, relayOutputHigh ? HIGH : LOW);
  fanState = state;
}

/*
 * PROTOCOL DOCUMENTATION:
 * 
 * Qt Application → Arduino:
 *   "FAN_ON\n"   - Turn fan ON (set relay HIGH)
 *   "FAN_OFF\n"  - Turn fan OFF (set relay LOW)
 *   "STATUS\n"   - Request current status
 * 
 * Arduino → Qt Application:
 *   "TEMPERATURE: XX.XX C"  - Temperature reading
 *   "FAN: ON" or "FAN: OFF"  - Fan state
 *   "SYSTEM: ..."            - System messages
 *   "COMMAND: ..."           - Command acknowledgments
 *   "STATUS: ..."            - Status responses
 * 
 * Temperature Threshold Control:
 *   - Qt app is responsible for determining when to send FAN_ON/FAN_OFF
 *   - Threshold set in Qt app: 30°C (see appwindow.cpp)
 *   - Arduino simply executes the commands received
 */
