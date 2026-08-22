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

extern const Command excecutableCommands[11];

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
    speed = constrain(speed, -1.0f, 1.0f);
    int pwm = abs(speed) * 255;

    if (speed > 0.0f)
    {
      // Adelante
      analogWrite(pinA, pwm);
      analogWrite(pinB, 0);
    }
    else if (speed < 0.0f)
    {
      // Atrás (corregido)
      analogWrite(pinA, 0);
      analogWrite(pinB, pwm);
    }
    else
    {
      // Detenido (ambos en 0)
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
Motor motorFrontRight(MOTOR_FRONT_RIGHT_PIN_A, MOTOR_FRONT_RIGHT_PIN_B);
Motor motorFrontLeft(MOTOR_FRONT_LEFT_PIN_A, MOTOR_FRONT_LEFT_PIN_B);
Motor motorBackRight(MOTOR_BACK_RIGHT_PIN_A, MOTOR_BACK_RIGHT_PIN_B);
Motor motorBackLeft(MOTOR_BACK_LEFT_PIN_A, MOTOR_BACK_LEFT_PIN_B);

// Declaraciones adelantadas (prototipos)
void GoFoward(const char *cmd);
void GoBackward(const char *cmd);
void goRotateClockwise(const char *cmd);
void goRotateCounterclockwise(const char *cmd);
void goStrafeRight(const char *cmd);
void goStrafeLeft(const char *cmd);
void goDiagonalForwardRight(const char *cmd);
void goDiagonalBackwardRight(const char *cmd);
void goDiagonalForwardLeft(const char *cmd);
void goDiagonalBackwardLeft(const char *cmd);
void GoStop(const char *cmd);

const char *cmdInExec = "NONE";
const Command excecutableCommands[11] = {
    {"FOWARD", GoFoward},
    {"STOP", GoStop},
    {"BACKWARD", GoBackward},
    {"ROTATE_CLK", goRotateClockwise},
    {"ROTATE_COUNTERCLK", goRotateCounterclockwise},
    {"STRAFE_RIGHT", goStrafeRight},
    {"STRAFE_LEFT", goStrafeLeft},
    {"DIAGONAL_FORWARD_RIGHT", goDiagonalForwardRight},
    {"DIAGONAL_BACKWARD_RIGHT", goDiagonalBackwardRight},
    {"DIAGONAL_FORWARD_LEFT", goDiagonalForwardLeft},
    {"DIAGONAL_BACKWARD_LEFT", goDiagonalBackwardLeft}};

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
  moverMotores(1.0, -1.0, -1.0, 1.0);
  cmdInExec = cmd;
}

void GoBackward(const char *cmd)
{
  moverMotores(-1.0, 1.0, 1.0, -1.0);
  cmdInExec = cmd;
}

void goRotateClockwise(const char *cmd)
{
  moverMotores(-1.0, -1.0, -1.0, -1.0);
  cmdInExec = cmd;
}

void goRotateCounterclockwise(const char *cmd)
{
  moverMotores(1.0, 1.0, 1.0, 1.0);
  cmdInExec = cmd;
}

void goStrafeRight(const char *cmd)
{
  moverMotores(-1.0, -1.0, 1.0, 1.0);
  cmdInExec = cmd;
}

void goStrafeLeft(const char *cmd)
{
  moverMotores(1.0, 1.0, -1.0, -1.0);
  cmdInExec = cmd;
}

void goDiagonalForwardRight(const char *cmd)
{
  moverMotores(0.0, -1.0, 0.0, 1.0);
  cmdInExec = cmd;
}

void goDiagonalBackwardRight(const char *cmd)
{
  moverMotores(-1.0, 0.0, 1.0, 0.0);
  cmdInExec = cmd;
}

void goDiagonalForwardLeft(const char *cmd)
{
  moverMotores(1.0, 0.0, -1.0, 0.0);
  cmdInExec = cmd;
}

void goDiagonalBackwardLeft(const char *cmd)
{
  moverMotores(0.0, 1.0, 0.0, -1.0);
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
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(MOTOR_FRONT_RIGHT_PIN_A, OUTPUT);
  pinMode(MOTOR_FRONT_RIGHT_PIN_B, OUTPUT);
  pinMode(MOTOR_FRONT_LEFT_PIN_A, OUTPUT);
  pinMode(MOTOR_FRONT_LEFT_PIN_B, OUTPUT);
  pinMode(MOTOR_BACK_RIGHT_PIN_A, OUTPUT);
  pinMode(MOTOR_BACK_RIGHT_PIN_B, OUTPUT);
  pinMode(MOTOR_BACK_LEFT_PIN_A, OUTPUT);
  pinMode(MOTOR_BACK_LEFT_PIN_B, OUTPUT);

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
