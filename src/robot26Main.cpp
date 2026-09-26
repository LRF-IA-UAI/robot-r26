#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <secrets.h>
#include <ArduinoJson.h>
#include <QMC5883LCompass.h>

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
Motor motorFront(MOTOR_FRONT_PIN_A, MOTOR_FRONT_PIN_B);
Motor motorBack(MOTOR_BACK_PIN_A, MOTOR_BACK_PIN_B);
QMC5883LCompass compass;

const float DECLINACION = -10.5;
// Variables de calibración para los ejes en el plano horizontal (X y Z)
int xMin = 32767, xMax = -32768;
int zMin = 32767, zMax = -32768;

// Declaraciones adelantadas (prototipos)
void goFoward(const char *cmd);
void goBackward(const char *cmd);
void goRotateClockwise(const char *cmd);
void goRotateCounterclockwise(const char *cmd);
void goStop(const char *cmd);

const char *cmdInExec = "NONE";
const Command excecutableCommands[11] = {
    {"FOWARD", goFoward},
    {"STOP", goStop},
    {"BACKWARD", goBackward},
    {"ROTATE_CLK", goRotateClockwise},
    {"ROTATE_COUNTERCLK", goRotateCounterclockwise}};
cmdConnectionManager *connectionManager = nullptr;

// general functions
void moverMotores(float frontSpeed, float backSpeed)
{
  motorFront.move(frontSpeed);
  motorBack.move(backSpeed);
}

void goFoward(const char *cmd)
{
  moverMotores(0.5, 0);
  cmdInExec = cmd;
}

void goBackward(const char *cmd)
{
  moverMotores(-0.5, 0);
  cmdInExec = cmd;
}

void goRotateClockwise(const char *cmd)
{
  moverMotores(0, -0.5);
  cmdInExec = cmd;
}

void goRotateCounterclockwise(const char *cmd)
{
  moverMotores(0, 0.5);
  cmdInExec = cmd;
}

void goStop(const char *cmd)
{
  moverMotores(0, 0);
  cmdInExec = "NONE";
}

void calibrarXZ()
{
  Serial.println(">> Calibrando: Gira el robot 360° en el plano horizontal (6 seg)...");
  unsigned long tInicio = millis();

  while (millis() - tInicio < 6000)
  {
    compass.read();
    int x = compass.getX();
    int z = compass.getZ();

    if (x < xMin)
      xMin = x;
    if (x > xMax)
      xMax = x;
    if (z < zMin)
      zMin = z;
    if (z > zMax)
      zMax = z;
    delay(20);
  }
  Serial.println(">> Calibración lista.");
}

int obtenerGradosRotacion()
{
  compass.read();

  // 1. Lecturas crudas de los ejes horizontales
  int rawX = compass.getX();
  int rawZ = compass.getZ();

  // 2. Centrado en el origen (0, 0)
  float xOffset = (xMax + xMin) / 2.0;
  float zOffset = (zMax + zMin) / 2.0;

  float x = rawX - xOffset;
  float z = rawZ - zOffset;

  // 3. Cálculo de rumbo (Frente: X, Lateral: Z)
  float heading = atan2(z, x);

  // 4. Radianes a grados
  float grados = heading * 180.0 / PI;

  if (grados < 0)
  {
    grados += 360.0;
  }

  // 5. Corrección por declinación magnética
  grados += DECLINACION;

  if (grados >= 360.0)
    grados -= 360.0;
  if (grados < 0)
    grados += 360.0;

  return (int)(round(grados)) % 360;
}

void setup()
{
  Serial.begin(9600);

  compass.init();
  compass.setMode(0x01, 0x08, 0x00, 0x00);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(MOTOR_FRONT_PIN_A, OUTPUT);
  pinMode(MOTOR_FRONT_PIN_B, OUTPUT);
  pinMode(MOTOR_BACK_PIN_A, OUTPUT);
  pinMode(MOTOR_BACK_PIN_B, OUTPUT);

  // primer stop de limpieza de pines de motores
  goStop("NONE");

  goRotateCounterclockwise("ROTATE_COUNTERCLK");
  calibrarXZ();
  goStop("NONE");

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
