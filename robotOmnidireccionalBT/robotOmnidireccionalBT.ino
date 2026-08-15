#include "BluetoothSerial.h"
#include <map>
#include <functional>

BluetoothSerial SerialBT;

float porcentajeDeVelocidad = 1.0;

const int motor_A_IB = 5;
const int motor_A_IA = 23;
const int motor_B_IB = 19;
const int motor_B_IA = 18;
const int motor_C_IB = 13;
const int motor_C_IA = 12;
const int motor_D_IB = 27;
const int motor_D_IA = 14;

String cmdInExec = "NONE";

std::map<String, std::function<void(String)>> simpleCommands;
std::map<String, std::function<void(uint)>> uintArgumentCommands;

void Detener(String cmd) {  // argument just so the function can be stored inside the simpleCommands map
  moverMotores(0, 0, 0, 0);
  cmdInExec = "NONE";
  SerialBT.println("Deteniendo...");
}

void moverMotor(int pinIA, int pinIB, float velocidad) {
  int pwm = abs(velocidad) * 255;
  if (velocidad > 0) {
    analogWrite(pinIA, pwm);
    analogWrite(pinIB, 0);
  } else if (velocidad < 0) {
    analogWrite(pinIA, 0);
    analogWrite(pinIB, pwm);
  } else {
    analogWrite(pinIA, 0);
    analogWrite(pinIB, 0);
  }
}

void moverMotores(float vA, float vB, float vC, float vD) {
  moverMotor(motor_A_IA, motor_A_IB, vA);
  moverMotor(motor_B_IA, motor_B_IB, vB);
  moverMotor(motor_C_IA, motor_C_IB, vC);
  moverMotor(motor_D_IA, motor_D_IB, vD);
}

void Adelante(String cmd) {
  moverMotores(1.0 * porcentajeDeVelocidad, 1.0 * porcentajeDeVelocidad,
               1.0 * porcentajeDeVelocidad, 1.0 * porcentajeDeVelocidad);
  cmdInExec = cmd;
  SerialBT.println("Avanzando...");
}

void Retroceder(String cmd) {
  moverMotores(-1.0 * porcentajeDeVelocidad, -1.0 * porcentajeDeVelocidad,
               -1.0 * porcentajeDeVelocidad, -1.0 * porcentajeDeVelocidad);
  cmdInExec = cmd;
  SerialBT.println("Retrocediendo...");
}

void Rotar_Horario(String cmd) {
  moverMotores(-1.0 * porcentajeDeVelocidad, 1.0 * porcentajeDeVelocidad,
               -1.0 * porcentajeDeVelocidad, 1.0 * porcentajeDeVelocidad);
  cmdInExec = cmd;
  SerialBT.println("Rotando en sentido horario...");
}

void Rotar_Antihorario(String cmd) {
  moverMotores(1.0 * porcentajeDeVelocidad, -1.0 * porcentajeDeVelocidad,
               1.0 * porcentajeDeVelocidad, -1.0 * porcentajeDeVelocidad);
  cmdInExec = cmd;
  SerialBT.println("Rotando en sentido anti-horario...");
}

void setup() {
  Detener("NONE");

  simpleCommands["FOWARD"] = Adelante;
  simpleCommands["STOP"] = Detener;
  simpleCommands["BACKWARD"] = Retroceder;
  simpleCommands["ROTATE_CLK"] = Rotar_Horario;
  simpleCommands["ROTATE_ACLK"] = Rotar_Antihorario;

  Serial.begin(9600);
  SerialBT.begin("ESP32-nemo");
  while (!SerialBT.hasClient()) delay(100);
  delay(100);
  SerialBT.println("Conectado!");
}

void loop() {
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();

    if (cmd == cmdInExec) {
      Detener("NONE");
      return;
    }

    auto foundResult = simpleCommands.find(cmd);

    if (foundResult != simpleCommands.end()) {
      std::function<void(String)> foundFunction = foundResult->second;
      foundFunction(cmd);
    } else {
      SerialBT.println("Comando no reconocido");
      Detener("NONE"); // in doubt, cancel any command in excecution
    }
  }

  delay(250);
}
