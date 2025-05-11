#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "DHT.h"
#include <LiquidCrystal_I2C.h>
#include <vector>
#include <algorithm>

#define DHTTYPE DHT22
#define DHTPIN 4
#define LED_PIN 2
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

// L298N Motor Driver Pins
#define ENA 25 // Enable/PWM pin
#define IN1 26 // Motor control pins
#define IN2 27

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

const char *ssid = "Z";
const char *password = "zxcvbnmkl";

WebServer server(80);

struct User
{
  String name;
  float targetTemp;
};

std::vector<User> users = {{"Default_User", 22.0}};
String currentUser = "Default_User";
float globalTemperature = 0.0;
bool motorRunning = false;

int getUserIndex(String username)
{
  for (size_t i = 0; i < users.size(); i++)
  {
    if (users[i].name == username)
    {
      return i;
    }
  }
  return 0;
}

void controlMotor()
{
  float targetTemp = users[getUserIndex(currentUser)].targetTemp;

  if (globalTemperature > targetTemp)
  {
    if (!motorRunning)
    {
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      analogWrite(ENA, 250); // Full speed
      motorRunning = true;
      Serial.println("Motor started - Cooling needed");
      lcd.setCursor(0, 1);
      lcd.print("FAN:ON ");
    }
  }
  else
  {
    if (motorRunning)
    {
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      analogWrite(ENA, 0);
      motorRunning = false;
      Serial.println("Motor stopped - Temperature OK");
      lcd.setCursor(0, 1);
      lcd.print("FAN:OFF");
    }
  }
}

void updateLCD()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(globalTemperature);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Target: ");
  lcd.print(users[getUserIndex(currentUser)].targetTemp);
  lcd.print("C ");
}

void updateUserLCD()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("User: ");
  lcd.print(currentUser);

  lcd.setCursor(0, 1);
  lcd.print("Users: ");
  lcd.print(users.size());
  delay(2000);
  updateLCD();
}

void updateTemperature()
{
  float temp = dht.readTemperature();
  if (!isnan(temp))
  {
    globalTemperature = temp;
    Serial.print("Updated Temperature: ");
    Serial.println(globalTemperature);
    updateLCD();
  }
  else
  {
    Serial.println("Failed to read from DHT sensor!");
  }
}

void handleTargetTemp()
{
  if (server.hasArg("temp") && server.hasArg("user"))
  {
    String user = server.arg("user");
    float temp = server.arg("temp").toFloat();
    int index = getUserIndex(user);
    if (index >= 0)
    {
      users[index].targetTemp = temp;
      server.send(200, "text/plain", "Target temp set to " + String(temp));
      updateLCD();
      controlMotor(); // Check if motor state needs to change
    }
    else
    {
      server.send(404, "text/plain", "User not found");
    }
  }
  else
  {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleCurrentTemp()
{
  server.send(200, "text/plain", String(globalTemperature));
}

void handleUserManage()
{
  if (server.hasArg("user") && server.hasArg("action") && server.hasArg("temp"))
  {
    String user = server.arg("user");
    String action = server.arg("action");
    float temp = server.arg("temp").toFloat();

    if (action == "add")
    {
      bool userExists = false;
      for (const auto &u : users)
      {
        if (u.name == user)
        {
          userExists = true;
          break;
        }
      }

      if (!userExists)
      {
        users.push_back({user, temp});
        server.send(200, "text/plain", "User " + action + "ed");
        updateUserLCD();
        return;
      }
    }
    else if (action == "remove")
    {
      users.erase(std::remove_if(users.begin(), users.end(),
                                 [&](const User &u)
                                 { return u.name == user; }),
                  users.end());
      if (currentUser == user)
      {
        currentUser = "Default_User";
      }
      server.send(200, "text/plain", "User " + action + "ed");
      updateUserLCD();
      return;
    }
    server.send(200, "text/plain", "No changes made");
  }
  else
  {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleSwitchUser()
{
  if (server.hasArg("user"))
  {
    String user = server.arg("user");
    bool userFound = false;
    for (const auto &u : users)
    {
      if (u.name == user)
      {
        userFound = true;
        break;
      }
    }

    if (userFound)
    {
      currentUser = user;
      server.send(200, "text/plain", "Switched to " + user);
      updateUserLCD();
      controlMotor(); // Check if motor state needs to change
    }
    else
    {
      server.send(404, "text/plain", "User not found");
    }
  }
  else
  {
    server.send(400, "text/plain", "Missing 'user' parameter");
  }
}

void handleCurrentUser()
{
  server.send(200, "text/plain", currentUser);
}

void handleUserList()
{
  String json = "[";
  for (size_t i = 0; i < users.size(); ++i)
  {
    json += "{\"name\":\"" + users[i].name + "\",\"temp\":" + String(users[i].targetTemp) + "}";
    if (i < users.size() - 1)
      json += ",";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // Initialize motor control pins
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 0);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.print("Initializing...");

  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());

  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS mount failed!");
    return;
  }

  server.on("/", HTTP_GET, []()
            {
        File file = SPIFFS.open("/index.html", "r");
        if (!file) {
            server.send(500, "text/plain", "index.html not found");
            return;
        }
        server.streamFile(file, "text/html");
        file.close(); });

  server.onNotFound([]()
                    {
        String path = server.uri();
        if (path.endsWith("/")) path += "index.html";
    
        String contentType = "text/plain";
        if (path.endsWith(".html")) contentType = "text/html";
        else if (path.endsWith(".css")) contentType = "text/css";
        else if (path.endsWith(".js")) contentType = "application/javascript";
        else if (path.endsWith(".ico")) contentType = "image/x-icon";
    
        File file = SPIFFS.open(path, "r");
        if (!file) {
            server.send(404, "text/plain", "File Not Found");
            return;
        }
    
        server.streamFile(file, contentType);
        file.close(); });

  server.on("/api/target_temp", HTTP_GET, handleTargetTemp);
  server.on("/api/current_temp", HTTP_GET, handleCurrentTemp);
  server.on("/api/user", HTTP_GET, handleUserManage);
  server.on("/api/switch", HTTP_GET, handleSwitchUser);
  server.on("/api/current_user", HTTP_GET, handleCurrentUser);
  server.on("/api/users", HTTP_GET, handleUserList);

  server.begin();
  Serial.println("HTTP server started");

  updateLCD();
}

void loop()
{
  server.handleClient();
  updateTemperature();
  controlMotor();
  delay(1000);
}