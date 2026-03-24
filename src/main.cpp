#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_BMP280.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// =========================
// CONFIGURACION WIFI
// =========================
const char* WIFI_SSID = "TU_WIFI";
const char* WIFI_PASS = "TU_PASSWORD";

// =========================
// PINES
// =========================
#define DHTPIN 4
#define DHTTYPE DHT22

#define I2C_SDA 8
#define I2C_SCL 9

#define LED_ALERT_PIN 5
#define BUZZER_PIN 6

// =========================
// UMBRALES DE ALERTA
// =========================
const float TEMP_ALERT = 30.0;
const float HUM_ALERT  = 70.0;

// =========================
// OBJETOS HARDWARE
// =========================
DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP280 bmp;
WebServer server(80);

// =========================
// ESTRUCTURA DE DATOS
// =========================
struct SensorData {
  float temperature;
  float humidity;
  float pressure;
  bool dhtOk;
  bool bmpOk;
  bool alert;
  uint32_t sampleId;
  uint32_t timestampMs;
};

// =========================
// RECURSOS FREERTOS
// =========================
QueueHandle_t sensorQueue = NULL;
SemaphoreHandle_t dataMutex = NULL;
SemaphoreHandle_t alertSemaphore = NULL;

// =========================
// TASK HANDLES
// =========================
TaskHandle_t taskSensoresHandle = NULL;
TaskHandle_t taskWebHandle = NULL;
TaskHandle_t taskWiFiHandle = NULL;
TaskHandle_t taskAlertasHandle = NULL;
TaskHandle_t taskMonitorHandle = NULL;

// =========================
// DATOS COMPARTIDOS
// =========================
SensorData latestData = {0};
volatile uint32_t hbSensores = 0;
volatile uint32_t hbWeb = 0;
volatile uint32_t hbWiFi = 0;
volatile uint32_t hbAlertas = 0;

const uint32_t WATCHDOG_TIMEOUT_MS = 10000;

// =========================
// HTML PAGINA WEB
// =========================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Estación Meteorológica ESP32</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 0; background: #f4f6f8; color: #222; }
    header { background: #1f6feb; color: white; padding: 18px; text-align: center; }
    .wrap { max-width: 900px; margin: 20px auto; padding: 0 16px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 16px; }
    .card {
      background: white; border-radius: 14px; padding: 20px;
      box-shadow: 0 4px 14px rgba(0,0,0,0.08);
    }
    .label { font-size: 14px; color: #666; }
    .value { font-size: 32px; font-weight: bold; margin-top: 8px; }
    .ok { color: #138a36; font-weight: bold; }
    .alert { color: #c62828; font-weight: bold; }
    .footer { margin-top: 20px; font-size: 14px; color: #555; }
  </style>
</head>
<body>
  <header>
    <h1>Estación Meteorológica ESP32</h1>
    <p>FreeRTOS + DHT22 + BMP280 + Web</p>
  </header>

  <div class="wrap">
    <div class="grid">
      <div class="card">
        <div class="label">Temperatura</div>
        <div class="value" id="temp">--</div>
      </div>
      <div class="card">
        <div class="label">Humedad</div>
        <div class="value" id="hum">--</div>
      </div>
      <div class="card">
        <div class="label">Presión</div>
        <div class="value" id="pres">--</div>
      </div>
      <div class="card">
        <div class="label">Estado</div>
        <div class="value" id="estado">--</div>
      </div>
    </div>

    <div class="card" style="margin-top:16px;">
      <div><strong>DHT22:</strong> <span id="dht">--</span></div>
      <div><strong>BMP280:</strong> <span id="bmp">--</span></div>
      <div><strong>Muestra:</strong> <span id="sample">--</span></div>
      <div><strong>Tiempo:</strong> <span id="time">--</span></div>
    </div>

    <div class="footer">
      La página se actualiza cada 2 segundos.
    </div>
  </div>

  <script>
    async function updateData() {
      try {
        const res = await fetch('/data');
        const data = await res.json();

        document.getElementById('temp').textContent = data.temperature.toFixed(1) + ' °C';
        document.getElementById('hum').textContent = data.humidity.toFixed(1) + ' %';
        document.getElementById('pres').textContent = data.pressure.toFixed(1) + ' hPa';
        document.getElementById('sample').textContent = data.sampleId;
        document.getElementById('time').textContent = data.timestampMs + ' ms';
        document.getElementById('dht').textContent = data.dhtOk ? 'OK' : 'ERROR';
        document.getElementById('bmp').textContent = data.bmpOk ? 'OK' : 'ERROR';

        const estado = document.getElementById('estado');
        if (data.alert) {
          estado.textContent = 'ALERTA';
          estado.className = 'value alert';
        } else {
          estado.textContent = 'NORMAL';
          estado.className = 'value ok';
        }
      } catch (e) {
        console.log('Error leyendo datos');
      }
    }

    updateData();
    setInterval(updateData, 2000);
  </script>
</body>
</html>
)rawliteral";

// =========================
// FUNCIONES WEB
// =========================
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleData() {
  SensorData copy;

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    copy = latestData;
    xSemaphoreGive(dataMutex);
  } else {
    server.send(500, "application/json", "{\"error\":\"mutex timeout\"}");
    return;
  }

  String json = "{";
  json += "\"temperature\":" + String(copy.temperature, 1) + ",";
  json += "\"humidity\":" + String(copy.humidity, 1) + ",";
  json += "\"pressure\":" + String(copy.pressure, 1) + ",";
  json += "\"dhtOk\":" + String(copy.dhtOk ? "true" : "false") + ",";
  json += "\"bmpOk\":" + String(copy.bmpOk ? "true" : "false") + ",";
  json += "\"alert\":" + String(copy.alert ? "true" : "false") + ",";
  json += "\"sampleId\":" + String(copy.sampleId) + ",";
  json += "\"timestampMs\":" + String(copy.timestampMs);
  json += "}";

  server.send(200, "application/json", json);
}

// =========================
// TASK SENSORES
// =========================
void Task_Sensores(void *pvParameters) {
  uint32_t sample = 0;

  for (;;) {
    SensorData data = {0};

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    float p = bmp.readPressure() / 100.0f;

    data.dhtOk = !(isnan(h) || isnan(t));
    data.bmpOk = !isnan(p) && p > 300.0f && p < 1200.0f;

    data.temperature = data.dhtOk ? t : -999.0f;
    data.humidity    = data.dhtOk ? h : -999.0f;
    data.pressure    = data.bmpOk ? p : -999.0f;
    data.sampleId    = sample++;
    data.timestampMs = millis();

    data.alert = false;
    if (data.dhtOk) {
      if (data.temperature > TEMP_ALERT || data.humidity > HUM_ALERT) {
        data.alert = true;
      }
    }

    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      latestData = data;
      xSemaphoreGive(dataMutex);
    }

    xQueueOverwrite(sensorQueue, &data);

    if (data.alert) {
      xSemaphoreGive(alertSemaphore);
    }

    hbSensores = millis();

    Serial.printf("[Task_Sensores] Core=%d | T=%.1f C | H=%.1f %% | P=%.1f hPa | DHT=%s | BMP=%s | ALERTA=%s\n",
                  xPortGetCoreID(),
                  data.temperature,
                  data.humidity,
                  data.pressure,
                  data.dhtOk ? "OK" : "ERR",
                  data.bmpOk ? "OK" : "ERR",
                  data.alert ? "SI" : "NO");

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// =========================
// TASK WIFI
// =========================
void Task_WiFi(void *pvParameters) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[Task_WiFi] Reconectando WiFi...");
      WiFi.disconnect(true, true);
      delay(200);
      WiFi.begin(WIFI_SSID, WIFI_PASS);

      uint32_t start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
      }
      Serial.println();

      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[Task_WiFi] Conectado. IP: ");
        Serial.println(WiFi.localIP());
      } else {
        Serial.println("[Task_WiFi] No se pudo conectar");
      }
    }

    hbWiFi = millis();
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

// =========================
// TASK WEB
// =========================
void Task_Web(void *pvParameters) {
  for (;;) {
    server.handleClient();
    hbWeb = millis();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// =========================
// TASK ALERTAS
// =========================
void Task_Alertas(void *pvParameters) {
  pinMode(LED_ALERT_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_ALERT_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  for (;;) {
    if (xSemaphoreTake(alertSemaphore, portMAX_DELAY) == pdTRUE) {
      SensorData copy;

      if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        copy = latestData;
        xSemaphoreGive(dataMutex);
      }

      Serial.printf("[Task_Alertas] Core=%d | ALERTA -> Temp=%.1f C | Hum=%.1f %%\n",
                    xPortGetCoreID(),
                    copy.temperature,
                    copy.humidity);

      for (int i = 0; i < 3; i++) {
        digitalWrite(LED_ALERT_PIN, HIGH);
        digitalWrite(BUZZER_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(150));
        digitalWrite(LED_ALERT_PIN, LOW);
        digitalWrite(BUZZER_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(150));
      }

      hbAlertas = millis();
    }
  }
}

// =========================
// TASK MONITOR
// =========================
void Task_Monitor(void *pvParameters) {
  for (;;) {
    uint32_t now = millis();

    Serial.println("--------------------------------------------------");
    Serial.printf("[Task_Monitor] Core=%d\n", xPortGetCoreID());
    Serial.printf("  Stack Sensores : %u words\n", uxTaskGetStackHighWaterMark(taskSensoresHandle));
    Serial.printf("  Stack Web      : %u words\n", uxTaskGetStackHighWaterMark(taskWebHandle));
    Serial.printf("  Stack WiFi     : %u words\n", uxTaskGetStackHighWaterMark(taskWiFiHandle));
    Serial.printf("  Stack Alertas  : %u words\n", uxTaskGetStackHighWaterMark(taskAlertasHandle));

    bool timeout =
      (now - hbSensores > WATCHDOG_TIMEOUT_MS) ||
      (now - hbWeb > WATCHDOG_TIMEOUT_MS) ||
      (now - hbWiFi > WATCHDOG_TIMEOUT_MS);

    if (timeout) {
      Serial.println("[Task_Monitor] WARNING: alguna tarea no responde");
    }

    vTaskDelay(pdMS_TO_TICKS(4000));
  }
}

// =========================
// SETUP
// =========================
void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("==================================================");
  Serial.println("PRACTICA 4 - PARTE 3");
  Serial.println("Estacion meteorologica REAL con pagina web");
  Serial.println("DHT22 + BMP280 + FreeRTOS + WebServer");
  Serial.println("==================================================");

  Wire.begin(I2C_SDA, I2C_SCL);
  dht.begin();

  bool bmpOk = bmp.begin(0x76);
  if (!bmpOk) {
    bmpOk = bmp.begin(0x77);
  }

  if (!bmpOk) {
    Serial.println("[Setup] ERROR: BMP280 no encontrado en 0x76 ni 0x77");
  } else {
    Serial.println("[Setup] BMP280 detectado");
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  sensorQueue = xQueueCreate(1, sizeof(SensorData));
  dataMutex = xSemaphoreCreateMutex();
  alertSemaphore = xSemaphoreCreateBinary();

  if (sensorQueue == NULL || dataMutex == NULL || alertSemaphore == NULL) {
    Serial.println("[Setup] Error creando recursos FreeRTOS");
    while (true) {
      delay(1000);
    }
  }

  uint32_t now = millis();
  hbSensores = now;
  hbWeb = now;
  hbWiFi = now;
  hbAlertas = now;

  xTaskCreatePinnedToCore(Task_Sensores, "Task_Sensores", 4096, NULL, 3, &taskSensoresHandle, 1);
  xTaskCreatePinnedToCore(Task_WiFi,     "Task_WiFi",     4096, NULL, 1, &taskWiFiHandle,     0);
  xTaskCreatePinnedToCore(Task_Web,      "Task_Web",      6144, NULL, 2, &taskWebHandle,      0);
  xTaskCreatePinnedToCore(Task_Alertas,  "Task_Alertas",  3072, NULL, 3, &taskAlertasHandle,  1);
  xTaskCreatePinnedToCore(Task_Monitor,  "Task_Monitor",  3072, NULL, 1, &taskMonitorHandle,  0);

  Serial.println("[Setup] Tareas creadas");
  Serial.println("[Setup] Cuando conecte al WiFi, abre la IP que salga en el monitor serie");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}