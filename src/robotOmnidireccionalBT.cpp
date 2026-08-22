#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <secrets.h>
#include <ArduinoJson.h>

// Class and struct declarations
struct Command
{
  const char *name;
  void (*action)(const char *);
};

extern const Command excecutableCommands[5];

class Motor
{
public:
  Motor(unsigned pinA, unsigned pinB)
  {
    this->pinA = pinA;
    this->pinB = pinB;
  }

  void move(float speed)
  {
    int pwm = abs(speed) * 255;
    if (speed > 0)
    {
      analogWrite(pinA, pwm);
      analogWrite(pinB, 0);
    }
    else if (speed < 0)
    {
      analogWrite(pinA, 0);
      analogWrite(pinB, pwm);
    }
    else
    {
      analogWrite(pinA, 0);
      analogWrite(pinB, 0);
    }
  }

private:
  unsigned pinA;
  unsigned pinB;
};

class cmdConnectionManager
{
public:
  virtual void setupConnection() {};
  virtual bool checkConnection() { return false; };
  virtual void connectionLoop() {};
};

class mqttConnectionManager : public cmdConnectionManager
{
public:
  mqttConnectionManager(const char *wifiSSID, const char *wifiPassword, const char *mqttBrokerURL, const unsigned mqttBrokerPort, const char *mqttTopic, const char *mqttUsername, const char *mqttPassword)
  {
    this->wifiSSID = wifiSSID;
    this->wifiPassword = wifiPassword;
    this->mqttBrokerURL = mqttBrokerURL;
    this->mqttBrokerPort = mqttBrokerPort;
    this->mqttTopic = mqttTopic;
    this->mqttUsername = mqttUsername;
    this->mqttPassword = mqttPassword;

    this->mqttClient = new PubSubClient(this->wifiClient);
    mqttClient->setServer(mqttBrokerURL, mqttBrokerPort);
    mqttClient->setCallback(mqttCallback);
  };

  void setupConnection() override
  {
    Serial.begin(9600);
    WiFi.begin(wifiSSID, wifiPassword);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    while (WiFi.status() != WL_CONNECTED)
    {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      Serial.print(".");
      delay(100);
    }

    Serial.println("Conexión exitosa");
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);

    mqttClient->connect("ESP32Client", mqttUsername, mqttPassword);
    mqttClient->subscribe(this->mqttTopic);
    Serial.println("Conexión y subscripción a broker exitosa");
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
  };

  bool checkConnection() override
  {
    return WiFi.status() == WL_CONNECTED && mqttClient->connected();
  };

  void connectionLoop() override
  {
    if (!checkConnection())
    {
      WiFi.reconnect();
      mqttClient->connect("ESP32Client", mqttUsername, mqttPassword);
      mqttClient->subscribe(this->mqttTopic);
    }
    mqttClient->loop();
  };

private:
  const char *wifiSSID;
  const char *wifiPassword;
  const char *mqttBrokerURL;
  unsigned mqttBrokerPort;
  const char *mqttTopic;
  const char *mqttUsername;
  const char *mqttPassword;

  PubSubClient *mqttClient;
  WiFiClient wifiClient;

  static void mqttCallback(char *topic, byte *payload, unsigned int length)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    String command = String((char *)payload).substring(0, length);
    Serial.println(command);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    // 3. Validar que el JSON sea válido
    if (error)
    {
      Serial.print("Error al parsear JSON: ");
      Serial.println(error.c_str());
      return;
    }

    const char *cmd = doc["cmd"] | "";
    const int value = doc["value"] | 0;

    const Command *commandToExecute = nullptr;
    for (const Command &command : excecutableCommands)
    {
      if (strcmp(command.name, cmd) == 0)
      {
        commandToExecute = const_cast<Command *>(&command);
        break;
      }
    }

    if (commandToExecute != nullptr)
    {
      commandToExecute->action(cmd);
    }
    else
    {
      Serial.println("Comando no reconocido");
    }
  }
};

// Global variables and objects
const float porcentajeDeVelocidad = 1.0;

const uint8_t motorFrontRightPinA = 5;
const uint8_t motorFrontRightPinB = 23;
const uint8_t motorFrontLeftPinA = 19;
const uint8_t motorFrontLeftPinB = 18;
const uint8_t motorBackRightPinA = 13;
const uint8_t motorBackRightPinB = 12;
const uint8_t motorBackLeftPinA = 27;
const uint8_t motorBackLeftPinB = 14;

Motor motorFrontRight(motorFrontRightPinA, motorFrontRightPinB);
Motor motorFrontLeft(motorFrontLeftPinA, motorFrontLeftPinB);
Motor motorBackRight(motorBackRightPinA, motorBackRightPinB);
Motor motorBackLeft(motorBackLeftPinA, motorBackLeftPinB);

// Declaraciones adelantadas (prototipos)
void GoFoward(const char *cmd);
void GoBackward(const char *cmd);
void goRotateClockwise(const char *cmd);
void goRotateCounterclockwise(const char *cmd);
void GoStop(const char *cmd);

const char *cmdInExec = "NONE";
const Command excecutableCommands[5] = {
    {"FOWARD", GoFoward},
    {"STOP", GoStop},
    {"BACKWARD", GoBackward},
    {"ROTATE_CLK", goRotateClockwise},
    {"ROTATE_COUNTERCLK", goRotateCounterclockwise}};

cmdConnectionManager *connectionManager = nullptr;

// general functions
void moverMotores(float frSpeed, float flSpeed, float brSpeed, float blSpeed)
{
  motorFrontRight.move(frSpeed);
  motorFrontLeft.move(flSpeed);
  motorBackLeft.move(brSpeed);
  motorBackRight.move(blSpeed);
}

void GoFoward(const char *cmd)
{
  moverMotores(1.0 * porcentajeDeVelocidad, 1.0 * porcentajeDeVelocidad,
               1.0 * porcentajeDeVelocidad, 1.0 * porcentajeDeVelocidad);
  cmdInExec = cmd;
}

void GoBackward(const char *cmd)
{
  moverMotores(-1.0 * porcentajeDeVelocidad, -1.0 * porcentajeDeVelocidad,
               -1.0 * porcentajeDeVelocidad, -1.0 * porcentajeDeVelocidad);
  cmdInExec = cmd;
}

void goRotateClockwise(const char *cmd)
{
  moverMotores(-1.0 * porcentajeDeVelocidad, 1.0 * porcentajeDeVelocidad,
               -1.0 * porcentajeDeVelocidad, 1.0 * porcentajeDeVelocidad);
  cmdInExec = cmd;
}

void goRotateCounterclockwise(const char *cmd)
{
  moverMotores(1.0 * porcentajeDeVelocidad, -1.0 * porcentajeDeVelocidad,
               1.0 * porcentajeDeVelocidad, -1.0 * porcentajeDeVelocidad);
  cmdInExec = cmd;
}

void GoStop(const char *cmd)
{
  moverMotores(0, 0, 0, 0);
  cmdInExec = "NONE";
}

// main arduino/esp32 functions
void setup()
{
  GoStop("NONE");
  connectionManager = new mqttConnectionManager(
      SECRET_WIFI_SSID,
      SECRET_WIFI_PASS,
      SECRET_MQTT_SERVER,
      SECRET_MQTT_PORT,
      "robot-omni/movement",
      SECRET_MQTT_USER,
      SECRET_MQTT_PASS);
  connectionManager->setupConnection();
}

void loop()
{
  connectionManager->connectionLoop();
  delay(250);
}
