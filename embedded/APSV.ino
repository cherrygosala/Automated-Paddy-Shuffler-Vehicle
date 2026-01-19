#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <Wire.h>

#define SDA_PIN 5
#define SCL_PIN 18

// --- WiFi Credentials ---
const char* ssid = "Cherry";
const char* password = "17283946";

WebServer server(80);

// --- Motor Driver Pins ---
#define FRONT_IN1 32
#define FRONT_IN2 33
#define FRONT_ENA 25 // PWM0
#define FRONT_IN3 13
#define FRONT_IN4 12
#define FRONT_ENB 26 // PWM1
#define REAR_IN1 34
#define REAR_IN2 14
#define REAR_ENA 22 // PWM2
#define REAR_IN3 23
#define REAR_IN4 19
#define REAR_ENB 21 // PWM3

// --- Relay Pins ---
#define RELAY1_PIN 2
#define RELAY2_PIN 4
#define RELAY3_PIN 15
#define RELAY4_PIN 16
#define BLADE_MOTOR_PIN 17

// --- Ultrasonic Sensor ---
#define TRIG_PIN 35
#define ECHO_PIN 39

// --- Battery Voltage ---
#define BATTERY_ADC_PIN 36
const float R1 = 10000.0;
const float R2 = 2200.0;
const float LOW_BATTERY_THRESHOLD = 11.0;

// --- LCD ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Servo ---
#define SERVO_PIN 27
Servo servo;

// --- Flags ---
bool obstacleDetectionEnabled = true;
bool bladeMotorRunning = false;
bool vehicleMoving = false;
bool scanningModeEnabled = false;

// --- Constants ---
const int obstacleDistanceThreshold = 20;
const int PWM_FREQ = 1000;
const int PWM_RESOLUTION = 8;
int currentMotorSpeed = 200; // Default speed

// --- For LCD message tracking ---
String lastLCDMessage = "";

void setup() {
  Serial.begin(9600);
  Wire.begin(SDA_PIN, SCL_PIN);

  pinMode(FRONT_IN1, OUTPUT); pinMode(FRONT_IN2, OUTPUT);
  pinMode(FRONT_IN3, OUTPUT); pinMode(FRONT_IN4, OUTPUT);
  pinMode(REAR_IN1, OUTPUT);  pinMode(REAR_IN2, OUTPUT);
  pinMode(REAR_IN3, OUTPUT);  pinMode(REAR_IN4, OUTPUT);

  ledcAttach(FRONT_ENA, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(FRONT_ENB, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(REAR_ENA, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(REAR_ENB, PWM_FREQ, PWM_RESOLUTION);

  setMotorSpeed(currentMotorSpeed);

  pinMode(BLADE_MOTOR_PIN, OUTPUT);
  digitalWrite(BLADE_MOTOR_PIN, LOW);

  pinMode(RELAY1_PIN, OUTPUT); pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT); pinMode(RELAY4_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW); digitalWrite(RELAY2_PIN, LOW);
  digitalWrite(RELAY3_PIN, LOW); digitalWrite(RELAY4_PIN, LOW);

  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);

  lcd.init(); lcd.backlight(); 
  lcdPrint("PaddyBot Ready");

  servo.attach(SERVO_PIN);
  servo.write(90);

  WiFi.softAP(ssid, password);
  Serial.println("WiFi Started");
  Serial.println(WiFi.softAPIP());

  // Web Routes
  server.on("/", handleRoot);
  server.on("/forward", moveForward);
  server.on("/backward", moveBackward);
  server.on("/left", moveLeft);
  server.on("/right", moveRight);
  server.on("/stop", stopVehicle);
  server.on("/bladeon", bladeOn);
  server.on("/bladeoff", bladeOff);
  server.on("/startobstacle", startObstacleDetection);
  server.on("/stopobstacle", stopObstacleDetection);
  server.on("/startscan", startScanningMode);
  server.on("/stopscan", stopScanningMode);
  server.on("/setspeed", []() {
    if (server.hasArg("value")) {
      int speed = server.arg("value").toInt();
      speed = constrain(speed, 0, 255);
      setMotorSpeed(speed);
      server.send(200, "text/plain", "Speed set to " + String(speed));
      Serial.println("Speed changed via slider: " + String(speed));
    } else {
      server.send(400, "text/plain", "No speed value provided");
    }
  });

  for (int i = 1; i <= 4; i++) {
    server.on(("/relay" + String(i) + "on").c_str(), [i]() {
      digitalWrite(RELAY1_PIN + i - 1, HIGH);
      server.send(200, "text/html", "Relay " + String(i) + " ON");
    });
    server.on(("/relay" + String(i) + "off").c_str(), [i]() {
      digitalWrite(RELAY1_PIN + i - 1, LOW);
      server.send(200, "text/html", "Relay " + String(i) + " OFF");
    });
  }

  server.begin();
}

void loop() {
  server.handleClient();
  checkSerialCommands();

  if (obstacleDetectionEnabled && vehicleMoving) checkObstacle();
  if (scanningModeEnabled) scanForObstacles();

  float batteryVoltage = readBatteryVoltage();
  if (batteryVoltage < LOW_BATTERY_THRESHOLD) {
    lcdPrint("Low Battery!");
    if(vehicleMoving) stopVehicle();
  } else if (!vehicleMoving && lastLCDMessage != "PaddyBot Ready") {
    lcdPrint("PaddyBot Ready");
  }
}

void moveForward() {
  Serial.println("Moving Forward");
  digitalWrite(FRONT_IN1, HIGH); digitalWrite(FRONT_IN2, LOW);
  digitalWrite(FRONT_IN3, HIGH); digitalWrite(FRONT_IN4, LOW);
  digitalWrite(REAR_IN1, HIGH); digitalWrite(REAR_IN2, LOW);
  digitalWrite(REAR_IN3, HIGH); digitalWrite(REAR_IN4, LOW);
  setMotorSpeed(currentMotorSpeed); bladeOn(); vehicleMoving = true;
  lcdPrint("Moving Forward"); 
  server.send(200, "text/html", "Moving Forward");
}

void moveBackward() {
  Serial.println("Moving Backward");
  digitalWrite(FRONT_IN1, LOW); digitalWrite(FRONT_IN2, HIGH);
  digitalWrite(FRONT_IN3, LOW); digitalWrite(FRONT_IN4, HIGH);
  digitalWrite(REAR_IN1, LOW); digitalWrite(REAR_IN2, HIGH);
  digitalWrite(REAR_IN3, LOW); digitalWrite(REAR_IN4, HIGH);
  setMotorSpeed(currentMotorSpeed); bladeOn(); vehicleMoving = true;
  lcdPrint("Moving Backward"); 
  server.send(200, "text/html", "Moving Backward");
}

void moveLeft() {
  Serial.println("Turning Left");
  digitalWrite(FRONT_IN1, LOW); digitalWrite(FRONT_IN2, HIGH);
  digitalWrite(FRONT_IN3, HIGH); digitalWrite(FRONT_IN4, LOW);
  digitalWrite(REAR_IN1, LOW); digitalWrite(REAR_IN2, HIGH);
  digitalWrite(REAR_IN3, HIGH); digitalWrite(REAR_IN4, LOW);
  setMotorSpeed(currentMotorSpeed); bladeOn(); vehicleMoving = true;
  lcdPrint("Turning Left"); 
  server.send(200, "text/html", "Turning Left");
}

void moveRight() {
  Serial.println("Turning Right");
  digitalWrite(FRONT_IN1, HIGH); digitalWrite(FRONT_IN2, LOW);
  digitalWrite(FRONT_IN3, LOW); digitalWrite(FRONT_IN4, HIGH);
  digitalWrite(REAR_IN1, HIGH); digitalWrite(REAR_IN2, LOW);
  digitalWrite(REAR_IN3, LOW); digitalWrite(REAR_IN4, HIGH);
  setMotorSpeed(currentMotorSpeed); bladeOn(); vehicleMoving = true;
  lcdPrint("Turning Right");
  server.send(200, "text/html", "Turning Right");
}

void stopVehicle() {
  Serial.println("Stopping Vehicle");
  digitalWrite(FRONT_IN1, LOW); digitalWrite(FRONT_IN2, LOW);
  digitalWrite(FRONT_IN3, LOW); digitalWrite(FRONT_IN4, LOW);
  digitalWrite(REAR_IN1, LOW); digitalWrite(REAR_IN2, LOW);
  digitalWrite(REAR_IN3, LOW); digitalWrite(REAR_IN4, LOW);
  setMotorSpeed(0); bladeOff();
  if (vehicleMoving) {
    vehicleMoving = false;
    lcdPrint("Stopped");
    server.send(200, "text/html", "Stopped");
  }
}

void setMotorSpeed(int speed) {
  currentMotorSpeed = speed;
  ledcWrite(FRONT_ENA, speed); 
  ledcWrite(FRONT_ENB, speed);
  ledcWrite(REAR_ENA, speed); 
  ledcWrite(REAR_ENB, speed);
}

void bladeOn() { digitalWrite(BLADE_MOTOR_PIN, HIGH); bladeMotorRunning = true; }
void bladeOff() { digitalWrite(BLADE_MOTOR_PIN, LOW); bladeMotorRunning = false; }

int getDistance() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  return duration * 0.034 / 2;
}

void checkObstacle() {
  int distance = getDistance();
  if (distance > 0 && distance < obstacleDistanceThreshold) {
    Serial.println("Obstacle Detected!");
    lcdPrint("Obstacle Detected");
    stopVehicle();
  }
}

void scanForObstacles() {
  for (int angle = 0; angle <= 180; angle += 10) {
    servo.write(angle); delay(200);
    int distance = getDistance();
    Serial.printf("Angle: %d, Distance: %d\n", angle, distance);
    if (distance > 0 && distance < obstacleDistanceThreshold) {
      lcdPrint("Obstacle @ " + String(angle) + " deg");
      stopVehicle();
      break;
    }
  }
}

void startObstacleDetection() {
  obstacleDetectionEnabled = true;
  lcdPrint("Obstacle ON");
  server.send(200, "text/html", "Obstacle Detection ON");
}

void stopObstacleDetection() {
  obstacleDetectionEnabled = false;
  lcdPrint("Obstacle OFF");
  server.send(200, "text/html", "Obstacle Detection OFF");
}

void startScanningMode() {
  scanningModeEnabled = true;
  lcdPrint("Scan Mode ON");
  server.send(200, "text/html", "Scan Mode ON");
}

void stopScanningMode() {
  scanningModeEnabled = false;
  lcdPrint("Scan Mode OFF");
  server.send(200, "text/html", "Scan Mode OFF");
}

float readBatteryVoltage() {
  int raw = analogRead(BATTERY_ADC_PIN);
  float vOut = (raw / 4095.0) * 3.3;
  return vOut * ((R1 + R2) / R2);
}

void lcdPrint(String message) {
  if (message != lastLCDMessage) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(message);
    lcd.setCursor(0, 1);
    lcd.print("Batt: ");
    lcd.print(readBatteryVoltage(), 1);
    lcd.print("V");
    lastLCDMessage = message;
  }
}

void checkSerialCommands() {
  if (Serial.available()) {
    char command = Serial.read();
    if (command == '\n' || command == '\r') return;

    Serial.print("Received command: ");
    Serial.println(command);

    switch (command) {
      case 'F': moveForward(); break;
      case 'B': moveBackward(); break;
      case 'L': moveLeft(); break;
      case 'R': moveRight(); break;
      case 'S': stopVehicle(); break;
      case 'O': bladeOn(); break;
      case 'P': bladeOff(); break;
      case 'D': startObstacleDetection(); break;
      case 'E': stopObstacleDetection(); break;
      case '9': startScanningMode(); break;
      case '0': stopScanningMode(); break;
      case '1': digitalWrite(RELAY1_PIN, HIGH); break;
      case '2': digitalWrite(RELAY1_PIN, LOW); break;
      case '3': digitalWrite(RELAY2_PIN, HIGH); break;
      case '4': digitalWrite(RELAY2_PIN, LOW); break;
      case '5': digitalWrite(RELAY3_PIN, HIGH); break;
      case '6': digitalWrite(RELAY3_PIN, LOW); break;
      case '7': digitalWrite(RELAY4_PIN, HIGH); break;
      case '8': digitalWrite(RELAY4_PIN, LOW); break;
      default: Serial.println("Unknown command"); break;
    }
  }
}

void handleRoot() {
  float batteryVoltage = readBatteryVoltage();
  String html = "<html><head><title>PaddyBot</title></head><body><h1>PaddyBot</h1>";
  html += "<p>Battery: " + String(batteryVoltage, 2) + " V</p>";
  html += "<button onclick=\"location.href='/forward'\">Forward</button><br><br>";
  html += "<button onclick=\"location.href='/backward'\">Backward</button><br><br>";
  html += "<button onclick=\"location.href='/left'\">Left</button>";
  html += "<button onclick=\"location.href='/right'\">Right</button><br><br>";
  html += "<button onclick=\"location.href='/stop'\">STOP</button><br><br>";
  html += "<button onclick=\"location.href='/bladeon'\">Blade ON</button>";
  html += "<button onclick=\"location.href='/bladeoff'\">Blade OFF</button><br><br>";
  html += "<button onclick=\"location.href='/startobstacle'\">Obstacle ON</button>";
  html += "<button onclick=\"location.href='/stopobstacle'\">Obstacle OFF</button><br><br>";
  html += "<button onclick=\"location.href='/startscan'\">Scan Mode ON</button>";
  html += "<button onclick=\"location.href='/stopscan'\">Scan Mode OFF</button><br><br>";
  html += "<h3>Relay Control</h3>";
  for (int i = 1; i <= 4; i++) {
    html += "<button onclick=\"location.href='/relay" + String(i) + "on'\">Relay " + String(i) + " ON</button> ";
    html += "<button onclick=\"location.href='/relay" + String(i) + "off'\">Relay " + String(i) + " OFF</button><br><br>";
  }
  html += "<h3>Motor Speed Control</h3>";
  html += "<input type='range' min='0' max='255' value='" + String(currentMotorSpeed) + "' id='speedSlider' oninput='updateSpeed(this.value)'/>";
  html += "<span id='speedValue'>" + String(currentMotorSpeed) + "</span>";
  html += "<script>";
  html += "function updateSpeed(val) {";
  html += "document.getElementById('speedValue').innerText = val;";
  html += "fetch('/setspeed?value=' + val);";
  html += "}";
  html += "</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}
