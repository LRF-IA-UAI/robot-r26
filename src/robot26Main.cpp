#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <secrets.h>
#include <ArduinoJson.h>
#include <QMC5883LCompass.h>
#include <esp_now.h>
#include <esp_wifi.h>

// General constants
uint8_t serverAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
unsigned long lastHeartbeat = 0;
const long HEARTBEAT_INTERVAL = 30000; // in MS
unsigned long lastTelemetry = 0;
const long TELEMETRY_INTERVAL = 1000; // in MS: envio periodico de sensores

// Class and struct declarations
struct Command
{
  const char *name;
  void (*action)(const char *);
};

extern const Command excecutableCommands[11];

// Declarada mas abajo; el ESP-NOW manager la usa para ejecutar comandos.
void executeCommand(const char *cmd);

// Estado global del comando en ejecucion (definido mas abajo).
extern const char *cmdInExec;

// Rumbo de la brujula en grados (definida mas abajo; la usa el ESP-NOW manager).
int obtenerGradosRotacion();

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
  virtual void sendData(String jsonData) {};
};

class espNowConnectionManager : public cmdConnectionManager
{
public:
  espNowConnectionManager()
  {
    // CRITICO: el callback es estatico y no tiene "this".
    // Hay que guardar la instancia o el callback nunca hara nada.
    instance = this;
  }

  ~espNowConnectionManager()
  {
    if (instance == this)
      instance = nullptr;
  }

  void setupConnection() override
  {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Modo Estacion (ESP-NOW no se conecta a un AP)
    WiFi.mode(WIFI_STA);
    // Evita que el WiFi se reconecte y cambie de canal por su cuenta
    WiFi.disconnect();

    // 1) Inicializar ESP-NOW PRIMERO
    if (esp_now_init() != ESP_OK)
    {
      Serial.println("Error inicializando ESP-NOW");
      return;
    }

    // 2) Fijar el canal DESPUES de init (debe coincidir con el otro peer)
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    // 3) Registrar el callback de recepcion DESPUES de init
    esp_now_register_recv_cb(dataRecieveCallback);

    // 4) Registrar el peer (Servidor)
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, serverAddress, 6);
    peerInfo.channel = 1; // Canal 1
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
      Serial.println("Fallo al agregar el servidor como Peer");
      return;
    }

    ready = true;
    Serial.println("Conexion ESP-NOW establecida con exito");

    // Registrarse ante el host: le enviamos nuestra MAC (emisor) para que
    // nos agregue como peer y nos asigne un UUID.
    _lastRegisterAttempt = millis();
    sendRegister();
  }

  bool checkConnection() override
  {
    // ESP-NOW no requiere estar conectado a un AP
    return ready;
  }

  void sendData(String jsonData) override
  {
    esp_err_t result = esp_now_send(serverAddress, (uint8_t *)jsonData.c_str(), jsonData.length());
    if (result == ESP_OK)
    {
      Serial.println("Enviado: " + jsonData);
    }
    else
    {
      Serial.println("Error al enviar");
    }
  }

  void connectionLoop() override
  {
    // Ejecutar (fuera del callback de ESP-NOW) el ultimo comando recibido.
    if (commandPending)
    {
      commandPending = false;
      executeCommand(pendingCommand);
    }

    // Parpadeo NO bloqueante cuando llega un comando
    if (blinkRequest)
    {
      blinkRequest = false;
      blinkTicks = 4;
    }
    if (blinkTicks > 0 && millis() - lastBlink >= 100)
    {
      lastBlink = millis();
      blinkState = !blinkState;
      digitalWrite(LED_BUILTIN, blinkState ? HIGH : LOW);
      blinkTicks--;
      if (blinkTicks == 0)
        digitalWrite(LED_BUILTIN, LOW);
    }

    unsigned long currentMillis = millis();

    // Reintentar el registro hasta que el host confirme con "welcome".
    if (!_registered && ready && currentMillis - _lastRegisterAttempt >= 2000)
    {
      _lastRegisterAttempt = currentMillis;
      sendRegister();
    }

    // Enviar heartbeat y estado cada 30 segundos
    if (currentMillis - lastHeartbeat >= HEARTBEAT_INTERVAL)
    {
      lastHeartbeat = currentMillis;

      JsonDocument doc;
      doc["type"] = "heartbeat";
      doc["uuid"] = _uuid;
      doc["cmd"] = cmdInExec;
      doc["heading"] = obtenerGradosRotacion();
      String out;
      serializeJson(doc, out);
      sendData(out);
    }

    // Enviar telemetria de sensores (heading de la brujula) periodicamente.
    if (ready && currentMillis - lastTelemetry >= TELEMETRY_INTERVAL)
    {
      lastTelemetry = currentMillis;

      JsonDocument doc;
      doc["type"] = "telemetry";
      doc["uuid"] = _uuid;
      doc["heading"] = obtenerGradosRotacion();
      doc["cmd"] = cmdInExec;
      String out;
      serializeJson(doc, out);
      sendData(out);
    }
  }

private:
  static espNowConnectionManager *instance;
  bool ready = false;
  volatile bool blinkRequest = false;
  uint8_t blinkTicks = 0;
  bool blinkState = false;
  unsigned long lastBlink = 0;
  unsigned long _lastRegisterAttempt = 0;
  bool _registered = false;
  String _uuid;
  char pendingCommand[32] = {0};
  volatile bool commandPending = false;

  // Envia la peticion de registro al host (la MAC viaja como emisor).
  void sendRegister()
  {
    JsonDocument doc;
    doc["type"] = "register";
    String out;
    serializeJson(doc, out);
    sendData(out);
  }

  // Encola un comando recibido para ejecutarlo en connectionLoop (no en el callback).
  void handleIncomingCommand(const char *cmd)
  {
    if (cmd == nullptr || cmd[0] == '\0')
      return;

    Serial.print("Comando recibido: ");
    Serial.println(cmd);

    strncpy(pendingCommand, cmd, sizeof(pendingCommand) - 1);
    pendingCommand[sizeof(pendingCommand) - 1] = '\0';
    commandPending = true;
    blinkRequest = true; // marcar para parpadear (no bloquear el callback)
  }

  static void dataRecieveCallback(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
  {
    if (instance == nullptr)
      return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, incomingData, len);

    if (error)
    {
      // Fallback: el host puede mandar el comando en texto plano (ej. "FOWARD").
      char buf[sizeof(pendingCommand)];
      int n = len < (int)sizeof(buf) - 1 ? len : (int)sizeof(buf) - 1;
      memcpy(buf, incomingData, n);
      buf[n] = '\0';

      if (strchr(buf, '{') == nullptr)
        instance->handleIncomingCommand(buf);
      else
        Serial.println("Error al leer JSON entrante");
      return;
    }

    // Respuesta de registro: el host nos asigna un UUID.
    if (doc["type"].is<const char *>() &&
        strcmp(doc["type"].as<const char *>(), "welcome") == 0)
    {
      instance->_uuid = doc["uuid"].as<const char *>();
      instance->_registered = true;
      Serial.print("Registrado en el host. UUID: ");
      Serial.println(instance->_uuid);
      return;
    }

    if (doc["cmd"].is<const char *>())
    {
      instance->handleIncomingCommand(doc["cmd"].as<const char *>());
    }
  }
};

espNowConnectionManager *espNowConnectionManager::instance = nullptr;

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

// Busca el comando en la tabla y ejecuta su accion asociada.
void executeCommand(const char *cmd)
{
  for (const Command &c : excecutableCommands)
  {
    if (c.name != nullptr && strcmp(c.name, cmd) == 0)
    {
      c.action(cmd);
      return;
    }
  }

  Serial.print("Comando desconocido: ");
  Serial.println(cmd);
}

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

  // goRotateCounterclockwise("ROTATE_COUNTERCLK");
  // calibrarXZ();
  // goStop("NONE");

  connectionManager = new espNowConnectionManager();
  if (connectionManager == nullptr)
  {
    Serial.println("Error: no se pudo crear connectionManager");
    return;
  }

  connectionManager->setupConnection();
}

void loop()
{
  if (connectionManager != nullptr)
  {
    connectionManager->connectionLoop();
  }
}
