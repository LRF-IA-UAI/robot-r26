#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>

// Class and struct declarations
struct Command
{
  const char *name;
  void (*action)(const char *);
};

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
  virtual bool checkConnection() {};
  virtual void connectionLoop() {};
};

class mqttConnectionManager : public cmdConnectionManager
{
public:
  mqttConnectionManager(char *wifiSSID, char *wifiPassword, char *mqttBrokerURL, unsigned mqttBrokerPort, char *mqttTopic, char *mqttUsername, char *mqttPassword)
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
    WiFi.begin(wifiSSID, wifiPassword);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
    }

    mqttClient->connect("ESP32Client", mqttUsername, mqttPassword);
    mqttClient->subscribe(this->mqttTopic);
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
  char *wifiSSID;
  char *wifiPassword;
  char *mqttBrokerURL;
  unsigned mqttBrokerPort;
  char *mqttTopic;
  char *mqttUsername;
  char *mqttPassword;

  PubSubClient *mqttClient;
  WiFiClient wifiClient;

  static void mqttCallback(char *topic, byte *payload, unsigned int length)
  {
    String command = String((char *)payload).substring(0, length);
    for (const Command &cmd : excecutableCommands)
    {
      if (command.equals(cmd.name))
      {
        cmd.action(cmd.name);
        break;
      }
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
const Command excecutableCommands[] = {
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
  connectionManager = new mqttConnectionManager(
      "TU_SSID",
      "TU_PASSWORD",
      "TU_BROKER", // p.ej. "broker.hivemq.com" o IP local
      1883,
      "robot/comandos",
      "TU_USUARIO",
      "TU_PASS");
  connectionManager->setupConnection();

  GoStop("NONE");
  Serial.begin(9600);
}

void loop()
{
  connectionManager->connectionLoop();
  delay(250);
}
