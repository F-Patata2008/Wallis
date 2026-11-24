/*
 * ============================================================================
 * PROYECTO: Control Integral de Wall-I's (ESP32)
 * Tracción de Tanque + Servidor Web Wi-Fi + Soporte Control Xbox + Serial IA
 * CD1201 EcoMaker — Equipo 13: Wall-I's (FCFM Beauchef)
 * ============================================================================
 * 
 * FORMAS DE CONTROL CONCURRENTES:
 * 1. Celular / PC por Wi-Fi: Genera la red "Wallis-Robot" (IP: 192.168.4.1).
 *    Interfaz web táctil en pantalla con D-Pad y botones.
 * 2. Mando de Xbox Series S: Si vinculas el mando por Bluetooth al celular o PC,
 *    la página web lo detecta con la Gamepad API y lo manejas con los sticks!
 * 3. Teclado en PC: Teclas W, A, S, D, Espacio y tecla R (Rodillo).
 * 4. Puerto Serial USB: Acepta comandos 'F', 'B', 'L', 'R', 'S', 'T' para IA.
 * 5. Botones Físicos en Mini-Proto:
 *    - GPIO 16: Toggle Rodillo N20
 *    - GPIO 17: Toggle Ruedas Adelante/Parada
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// --- PINES MOTORES (L298N / HW-95) ---
const int PIN_RUEDAS_IN1 = 25; // Motor Izquierdo Adelante
const int PIN_RUEDAS_IN2 = 26; // Motor Izquierdo Reversa
const int PIN_RUEDAS_IN3 = 27; // Motor Derecho Adelante
const int PIN_RUEDAS_IN4 = 19; // Motor Derecho Reversa

const int PIN_RODILLO_IN1 = 32; // Rodillo N20 Adelante
const int PIN_RODILLO_IN2 = 33; // Rodillo N20 Reversa

// --- BOTONES FÍSICOS (INPUT_PULLUP a GND) ---
const int PIN_BOTON_RODILLO = 16;
const int PIN_BOTON_RUEDAS  = 17;

const int PIN_LED_ESTADO    = 2;

// --- CONFIGURACIÓN WI-FI ACCESS POINT (Sin necesidad de internet) ---
const char* AP_SSID = "Wallis-Robot";
const char* AP_PASS = "wallis1234"; // Mínimo 8 caracteres WPA2

WebServer server(80);

// --- ESTADOS DEL SISTEMA ---
bool activoRodillo = false;
bool activoRuedas  = false;
char estadoMovimiento = 'S'; // 'S'=Stop, 'F'=Forward, 'B'=Back, 'L'=Left, 'R'=Right

// Debounce para botones físicos
bool prevBtnRodillo = HIGH;
bool prevBtnRuedas  = HIGH;
unsigned long lastDebounceRodillo = 0;
unsigned long lastDebounceRuedas  = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// Declaración de funciones
void moverAdelante();
void moverAtras();
void girarIzquierda();
void girarDerecha();
void frenarRuedas();
void toggleRodillo();
void ejecutarComando(char c);

// --- PÁGINA WEB HTML5 / CSS / JS EMBEBIDA (Responsive + Gamepad API) ---
const char PAGE_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>Wall-I's Control</title>
<style>
  * { box-sizing: border-box; touch-action: manipulation; margin: 0; padding: 0; }
  body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #f8fafc; text-align: center; padding: 15px; }
  h1 { font-size: 1.4rem; color: #38bdf8; margin-bottom: 4px; }
  .badge { display: inline-block; background: #0369a1; color: #e0f2fe; padding: 3px 10px; border-radius: 999px; font-size: 0.75rem; margin-bottom: 12px; }
  .grid-tank { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; max-width: 320px; margin: 10px auto; }
  .btn { background: #1e293b; color: #fff; border: 2px solid #334155; border-radius: 12px; padding: 22px 0; font-size: 1.5rem; font-weight: bold; cursor: pointer; user-select: none; }
  .btn:active, .btn.active { background: #38bdf8; color: #0f172a; border-color: #0284c7; }
  .btn-stop { background: #ef4444; border-color: #dc2626; grid-column: 2; }
  .btn-stop:active { background: #b91c1c; }
  .panel-rodillo { max-width: 320px; margin: 15px auto; padding: 12px; background: #1e293b; border-radius: 12px; border: 1px solid #334155; }
  .btn-toggle { width: 100%; padding: 14px; border-radius: 8px; border: none; font-size: 1rem; font-weight: bold; cursor: pointer; }
  .btn-on { background: #10b981; color: white; }
  .btn-off { background: #475569; color: white; }
  .status-bar { font-size: 0.8rem; color: #94a3b8; margin-top: 10px; }
  .gamepad-tag { color: #fbbf24; font-size: 0.8rem; margin-top: 5px; }
</style>
</head>
<body>
  <h1>WALL-I'S TANK</h1>
  <div class="badge">FCFM BEAUCHEF · CD1201</div>
  
  <div class="grid-tank">
    <div></div>
    <button class="btn" onpointerdown="send('F')" onpointerup="send('S')">▲</button>
    <div></div>
    <button class="btn" onpointerdown="send('L')" onpointerup="send('S')">◀</button>
    <button class="btn btn-stop" onclick="send('S')">■</button>
    <button class="btn" onpointerdown="send('R')" onpointerup="send('S')">▶</button>
    <div></div>
    <button class="btn" onpointerdown="send('B')" onpointerup="send('S')">▼</button>
    <div></div>
  </div>

  <div class="panel-rodillo">
    <button id="btnRod" class="btn-toggle btn-off" onclick="send('T')">RODILLO N20: APAGADO</button>
  </div>

  <div id="gpInfo" class="gamepad-tag">Conecta tu mando de Xbox o usa teclado (WASD)</div>
  <div class="status-bar" id="status">Listo para operar en terreno</div>

<script>
let rodilloActivo = false;
function send(c) {
  fetch('/cmd?dir=' + c);
  document.getElementById('status').innerText = 'Comando: ' + c;
  if(c === 'T') {
    rodilloActivo = !rodilloActivo;
    const b = document.getElementById('btnRod');
    b.className = 'btn-toggle ' + (rodilloActivo ? 'btn-on' : 'btn-off');
    b.innerText = 'RODILLO N20: ' + (rodilloActivo ? 'ENCENDIDO [ON]' : 'APAGADO [OFF]');
  }
}

// Soporte Teclado PC (WASD)
let keyState = {};
window.addEventListener('keydown', (e) => {
  if (keyState[e.key]) return;
  keyState[e.key] = true;
  if (e.key === 'w' || e.key === 'ArrowUp') send('F');
  else if (e.key === 's' || e.key === 'ArrowDown') send('B');
  else if (e.key === 'a' || e.key === 'ArrowLeft') send('L');
  else if (e.key === 'd' || e.key === 'ArrowRight') send('R');
  else if (e.key === ' ' || e.key === 'x') send('S');
  else if (e.key === 'r') send('T');
});
window.addEventListener('keyup', (e) => {
  keyState[e.key] = false;
  if (['w','s','a','d','ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(e.key)) send('S');
});

// Soporte Gamepad API (Mando de Xbox por Bluetooth en Celular o PC)
let lastGpCmd = 'S';
function checkGamepad() {
  const gps = navigator.getGamepads ? navigator.getGamepads() : [];
  for (let gp of gps) {
    if (!gp) continue;
    document.getElementById('gpInfo').innerText = '🎮 Mando conectado: ' + gp.id.substring(0, 24);
    
    // Ejes stick izquierdo / cruceta
    let x = gp.axes[0] || 0;
    let y = gp.axes[1] || 0;
    if (gp.buttons[12] && gp.buttons[12].pressed) y = -1; // D-Pad Arriba
    if (gp.buttons[13] && gp.buttons[13].pressed) y = 1;  // D-Pad Abajo
    if (gp.buttons[14] && gp.buttons[14].pressed) x = -1; // D-Pad Izq
    if (gp.buttons[15] && gp.buttons[15].pressed) x = 1;  // D-Pad Der

    let cmd = 'S';
    if (y < -0.4) cmd = 'F';
    else if (y > 0.4) cmd = 'B';
    else if (x < -0.4) cmd = 'L';
    else if (x > 0.4) cmd = 'R';

    if (cmd !== lastGpCmd) {
      lastGpCmd = cmd;
      send(cmd);
    }

    // Botón A o RT (Gatillo) para Toggle Rodillo
    if (gp.buttons[0] && gp.buttons[0].pressed) {
      if (!gp._btnA) { send('T'); gp._btnA = true; }
    } else { gp._btnA = false; }
  }
  requestAnimationFrame(checkGamepad);
}
window.addEventListener('gamepadconnected', () => requestAnimationFrame(checkGamepad));
</script>
</body>
</html>
)rawliteral";

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Desactivar detector de caídas de voltaje

  Serial.begin(115200);
  delay(100);

  // Configurar actuadores
  pinMode(PIN_RUEDAS_IN1, OUTPUT);
  pinMode(PIN_RUEDAS_IN2, OUTPUT);
  pinMode(PIN_RUEDAS_IN3, OUTPUT);
  pinMode(PIN_RUEDAS_IN4, OUTPUT);

  pinMode(PIN_RODILLO_IN1, OUTPUT);
  pinMode(PIN_RODILLO_IN2, OUTPUT);
  pinMode(PIN_LED_ESTADO, OUTPUT);

  frenarRuedas();
  digitalWrite(PIN_RODILLO_IN1, LOW);
  digitalWrite(PIN_RODILLO_IN2, LOW);

  // Configurar botones físicos de la protoboard
  pinMode(PIN_BOTON_RODILLO, INPUT_PULLUP);
  pinMode(PIN_BOTON_RUEDAS, INPUT_PULLUP);

  // Iniciar Access Point Wi-Fi
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  IPAddress myIP = WiFi.softAPIP();
  Serial.println(F("\n========================================================"));
  Serial.println(F("     WALL-I'S: SERVIDOR WI-FI TANK DRIVE LISTO          "));
  Serial.println(F("========================================================"));
  Serial.print(F("Red Wi-Fi:    ")); Serial.println(AP_SSID);
  Serial.print(F("Contrasena:   ")); Serial.println(AP_PASS);
  Serial.print(F("IP de Control: http://")); Serial.println(myIP);
  Serial.println(F("========================================================"));

  // Rutas del servidor web
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", PAGE_INDEX);
  });

  server.on("/cmd", HTTP_GET, []() {
    if (server.hasArg("dir")) {
      String d = server.arg("dir");
      if (d.length() > 0) {
        ejecutarComando(d.charAt(0));
      }
    }
    server.send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  // 1. Atender peticiones Wi-Fi (Celular / Web / Gamepad)
  server.handleClient();

  // 2. Atender comandos Seriales por USB (Notebook / Script Python IA)
  if (Serial.available() > 0) {
    char c = Serial.read();
    ejecutarComando(c);
  }

  // 3. Atender los 2 Botones Físicos en la mini-protoboard (T Flip-Flop)
  int lecturaRodillo = digitalRead(PIN_BOTON_RODILLO);
  int lecturaRuedas  = digitalRead(PIN_BOTON_RUEDAS);

  // Botón Rodillo N20 (GPIO 16)
  if (lecturaRodillo == LOW && prevBtnRodillo == HIGH && (millis() - lastDebounceRodillo > DEBOUNCE_DELAY)) {
    toggleRodillo();
    lastDebounceRodillo = millis();
    Serial.printf("[BOTON] Rodillo -> %s\n", activoRodillo ? "ON" : "OFF");
  }
  prevBtnRodillo = lecturaRodillo;

  // Botón Ruedas Tracción (GPIO 17)
  if (lecturaRuedas == LOW && prevBtnRuedas == HIGH && (millis() - lastDebounceRuedas > DEBOUNCE_DELAY)) {
    if (estadoMovimiento == 'F') {
      frenarRuedas();
    } else {
      moverAdelante();
    }
    lastDebounceRuedas = millis();
    Serial.printf("[BOTON] Ruedas -> %s\n", (estadoMovimiento == 'F') ? "AVANZANDO" : "DETENIDO");
  }
  prevBtnRuedas = lecturaRuedas;

  // Actualizar LED de estado
  digitalWrite(PIN_LED_ESTADO, (activoRodillo || (estadoMovimiento != 'S')) ? HIGH : LOW);
}

// ============================================================================
// FUNCIONES DE CONTROL CINEMÁTICO TANK DRIVE
// ============================================================================

void ejecutarComando(char c) {
  switch (c) {
    case 'F': moverAdelante(); break;
    case 'B': moverAtras(); break;
    case 'L': girarIzquierda(); break;
    case 'R': girarDerecha(); break;
    case 'S': frenarRuedas(); break;
    case 'T': toggleRodillo(); break;
  }
}

void moverAdelante() {
  estadoMovimiento = 'F';
  digitalWrite(PIN_RUEDAS_IN1, HIGH);
  digitalWrite(PIN_RUEDAS_IN2, LOW);
  digitalWrite(PIN_RUEDAS_IN3, HIGH);
  digitalWrite(PIN_RUEDAS_IN4, LOW);
}

void moverAtras() {
  estadoMovimiento = 'B';
  digitalWrite(PIN_RUEDAS_IN1, LOW);
  digitalWrite(PIN_RUEDAS_IN2, HIGH);
  digitalWrite(PIN_RUEDAS_IN3, LOW);
  digitalWrite(PIN_RUEDAS_IN4, HIGH);
}

// Giro de Tanque sobre su eje: Rueda Izq Atrás, Rueda Der Adelante
void girarIzquierda() {
  estadoMovimiento = 'L';
  digitalWrite(PIN_RUEDAS_IN1, LOW);
  digitalWrite(PIN_RUEDAS_IN2, HIGH);
  digitalWrite(PIN_RUEDAS_IN3, HIGH);
  digitalWrite(PIN_RUEDAS_IN4, LOW);
}

// Giro de Tanque sobre su eje: Rueda Izq Adelante, Rueda Der Atrás
void girarDerecha() {
  estadoMovimiento = 'R';
  digitalWrite(PIN_RUEDAS_IN1, HIGH);
  digitalWrite(PIN_RUEDAS_IN2, LOW);
  digitalWrite(PIN_RUEDAS_IN3, LOW);
  digitalWrite(PIN_RUEDAS_IN4, HIGH);
}

void frenarRuedas() {
  estadoMovimiento = 'S';
  digitalWrite(PIN_RUEDAS_IN1, LOW);
  digitalWrite(PIN_RUEDAS_IN2, LOW);
  digitalWrite(PIN_RUEDAS_IN3, LOW);
  digitalWrite(PIN_RUEDAS_IN4, LOW);
}

void toggleRodillo() {
  activoRodillo = !activoRodillo;
  if (activoRodillo) {
    digitalWrite(PIN_RODILLO_IN1, HIGH);
    digitalWrite(PIN_RODILLO_IN2, LOW);
  } else {
    digitalWrite(PIN_RODILLO_IN1, LOW);
    digitalWrite(PIN_RODILLO_IN2, LOW);
  }
}
