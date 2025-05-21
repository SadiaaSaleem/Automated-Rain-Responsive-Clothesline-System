#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ESP32Servo.h>  

// Pin Definitions
#define RAIN_SENSOR_PIN 34  // Digital rain sensor input pin
#define RAIN_ANALOG_PIN 35  // Analog rain sensor input pin for intensity measurement

// Servo Control Pins and Positions
constexpr int SERVO_PIN = 25;  // Servo control pin
constexpr int SERVO_OPEN_ANGLE = 0;   // Shelter open position
constexpr int SERVO_CLOSED_ANGLE = 90;  // Shelter closed position

// Rain Intensity Constants
const int dryThreshold = 4095;  // Threshold value for considering it as dry (adjust based on your sensor)
const unsigned long sampleInterval = 1000;  // Sample interval in milliseconds (1 second)
const unsigned long aiDataInterval = 5000;  // Interval to send AI data during rain (every 5 seconds)

// Rain Intensity Variables
unsigned long rainStartTime = 0;  // When rain started
unsigned long rainEndTime = 0;    // When rain ended
unsigned long totalRainDuration = 0;  // Total duration of rain in milliseconds
unsigned long lastSampleTime = 0;  // Last time we took a sample
unsigned long lastAiDataTime = 0;  // Last time we sent AI data during rain
int rainIntensitySum = 0;         // Sum of all intensity readings
int rainSampleCount = 0;          // Count of samples taken during rain
float avgRainIntensity = 0.0;     // Average rain intensity
bool isRaining = false;           // Flag to track if it's currently raining

// Wi-Fi Credentials
const char *ssid = "codeplex";
const char *password = "iotproject";

// MQTT Broker Details
String device_id = "Device0001";
const char *mqtt_server = "a1844d6db9f847b88e8653fc92321028.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;  // Use port 8883 for secure connection (TLS)
const char *mqtt_user = "Device0001";
const char *mqtt_password = "Device0001";
const char *mqtt_clientId = "Device_Device0001";
const char *topic_publish = "RainSensorData";
const char *topic_aidata_publish = "RainSensorData/aiData";  

WiFiClientSecure esp_client;  // Use WiFiClient for secure connection
PubSubClient *mqtt_client;

Servo shelterServo;  // Create Servo object for controlling the shelter

void callback(char *topic, byte *payload, unsigned int length);

// Data Sending Time
unsigned long CurrentMillis, PreviousMillis, DataSendingTime = (unsigned long)1000 * 10;
bool shelterClosed = false;  // Variable to track the shelter state

void setup() {
  Serial.begin(115200);
  pinMode(RAIN_SENSOR_PIN, INPUT);    // Configure the digital rain sensor pin as input
  pinMode(RAIN_ANALOG_PIN, INPUT);    // Configure the analog rain sensor pin as input
  
  // Initialize Servo
  shelterServo.setPeriodHertz(50);
  shelterServo.attach(SERVO_PIN, 500, 2400);
  openShelter();  // Start with the shelter open

  setup_wifi();
  esp_client.setInsecure();
  mqtt_client = new PubSubClient(esp_client);
  mqtt_client->setServer(mqtt_server, mqtt_port);
  mqtt_client->setCallback(callback);

  mqtt_connect();
  
  Serial.println("Rain Intensity and Duration Measurement System");
  Serial.println("=============================================");
  Serial.println("Waiting for rain...");
  
  // Initialize timing variables
  lastSampleTime = millis();
  lastAiDataTime = millis();
}

void loop() {
  // Get current time
  unsigned long currentTime = millis();
  
  // Read digital rain sensor for shelter control
  int digitalRainState = digitalRead(RAIN_SENSOR_PIN);  // LOW = raining, HIGH = not raining
  
  // Control shelter based on digital rain sensor
  if (digitalRainState == LOW) {
    if (!shelterClosed) {
      closeShelter();  // Close shelter if it's raining
    }
    
    // If the digital sensor indicates rain and we're not already tracking it,
    // start a new rain event
    if (!isRaining) {
      isRaining = true;
      rainStartTime = currentTime;
      lastAiDataTime = currentTime;  // Reset AI data timing when rain starts
      Serial.println("Rain detected!");
      
      // Reset our intensity calculations
      rainIntensitySum = 0;
      rainSampleCount = 0;
      avgRainIntensity = 0.0;
    }
  } else {
    // If digital sensor indicates it's dry
    if (shelterClosed) {
      delay(7000);
      openShelter();  // Open shelter if it stops raining
    }
    
    // If we were previously tracking rain, end the rain event
    if (isRaining) {
      isRaining = false;
      rainEndTime = currentTime;
      unsigned long rainDuration = rainEndTime - rainStartTime;
      totalRainDuration += rainDuration;
      
      // Display final data for this rain event
      Serial.println("\n----- Rain Event Summary -----");
      Serial.print("Average rain intensity: ");
      Serial.print(avgRainIntensity);
      Serial.println("%");
      
      Serial.print("Rain duration: ");
      printDuration(rainDuration);
      Serial.println("\n");
      
      // Also publish final rain data to AI data topic
      Serial.println("Publishing final rain data...");
      publishRainData(avgRainIntensity, rainDuration);
      
      Serial.println("Waiting for next rain event...");
    }
  }
  
  // Process analog rain intensity measurement only if we're in a rain event
  if (isRaining && (currentTime - lastSampleTime >= sampleInterval)) {
    lastSampleTime = currentTime;  // Update sample time
    
    // Read the rain sensor value from analog pin
    int rainValue = analogRead(RAIN_ANALOG_PIN);
    Serial.print("Rain sensor value: ");
    Serial.println(rainValue);
    
    // Map the rain value to a percentage (0-100%)
    // Lower values typically indicate more rain with most sensors
    int rainIntensity = map(rainValue, dryThreshold, 0, 0, 100);
    
    // Constrain the intensity between 0 and 100
    rainIntensity = constrain(rainIntensity, 0, 100);
    Serial.print("Mapped intensity: ");
    Serial.print(rainIntensity);
    Serial.println("%");
    
    // Add to our intensity calculations
    rainIntensitySum += rainIntensity;
    rainSampleCount++;
    
    // Calculate average intensity so far
    avgRainIntensity = (float)rainIntensitySum / rainSampleCount;
    
    // Check if it's time to send intensity and duration data (every 5 seconds)
    if (currentTime - lastAiDataTime >= aiDataInterval) {
      lastAiDataTime = currentTime;  // Update last AI data time
      unsigned long currentDuration = currentTime - rainStartTime;
      
      // Publish current rain intensity and duration to AI data topic every 5 seconds
      Serial.println("Time to send AI data...");
      publishRainData(avgRainIntensity, currentDuration);
    }
  }

  // Publish regular status data to MQTT every 10 seconds
  CurrentMillis = millis();
  if (CurrentMillis - PreviousMillis > DataSendingTime) {
    PreviousMillis = CurrentMillis;

    // Publish simple message "Raining" or "Dry"
    String message = (digitalRainState == LOW) ? "Raining" : "Dry";  // "Raining" if LOW, "Dry" if HIGH
    mqtt_publish(topic_publish, message.c_str());
  }

  if (!mqtt_client->loop()) {
    mqtt_connect();
  }
  delay(100);  // Reduced delay for better responsiveness
}

// Setup Wi-Fi connection
void setup_wifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println("\"" + String(ssid) + "\"");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// MQTT Connection
void mqtt_connect() {
  while (!mqtt_client->connected()) {
    Serial.println("\nAttempting MQTT connection...");
    if (mqtt_client->connect(mqtt_clientId, mqtt_user, mqtt_password)) {
      Serial.println("MQTT Client Connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client->state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// MQTT Publish function with topic parameter
void mqtt_publish(const char *topic, const char *data) {
  mqtt_connect();
  Serial.println("Publish Topic: \"" + String(topic) + "\"");
  if (mqtt_client->publish(topic, data)) {
    Serial.println("Publish \"" + String(data) + "\" ok");
  } else {
    Serial.println("Publish \"" + String(data) + "\" failed");
  }
}

// Function to publish rain data (both during rain and when it stops)
void publishRainData(float intensity, unsigned long duration) {
  // Format: "Intensity: X%, Duration: Xh Xm Xs"
  String durationStr = formatDuration(duration);
  String message = "Intensity: " + String(intensity, 1) + "%, Duration: " + durationStr;
  
  mqtt_publish(topic_aidata_publish, message.c_str());
  
  // Also show on serial for debugging
  Serial.println("Publishing data: " + message);
}

// Function to format duration as string
String formatDuration(unsigned long duration) {
  unsigned long seconds = duration / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds %= 60;
  minutes %= 60;
  
  String result = "";
  if (hours > 0) {
    result += String(hours) + "h ";
  }
  if (minutes > 0 || hours > 0) {
    result += String(minutes) + "m ";
  }
  result += String(seconds) + "s";
  
  return result;
}

// Function to print duration in a readable format
void printDuration(unsigned long duration) {
  unsigned long seconds = duration / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds %= 60;
  minutes %= 60;
  
  if (hours > 0) {
    Serial.print(hours);
    Serial.print("h ");
  }
  if (minutes > 0 || hours > 0) {
    Serial.print(minutes);
    Serial.print("m ");
  }
  Serial.print(seconds);
  Serial.print("s");
}

// Shelter Control Functions
void closeShelter() {
  shelterServo.write(SERVO_CLOSED_ANGLE);
  shelterClosed = true;
  Serial.println("Action: Closing shelter");
}

void openShelter() {
  shelterServo.write(SERVO_OPEN_ANGLE);
  shelterClosed = false;
  Serial.println("Action: Opening shelter");
}

// MQTT Callback Function (Not needed for publishing)
void callback(char *topic, byte *payload, unsigned int length) {
  // No need for callback in this scenario, we are only publishing data
}