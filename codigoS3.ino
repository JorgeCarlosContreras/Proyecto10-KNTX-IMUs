#include <esp_now.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>

typedef struct struct_message {
    uint8_t node_id;
    uint32_t timestamp;
    float quat[4];
    float accel[3];
} struct_message;

struct_message incomingData;

// Aumentamos el buffer a 1000 para soportar la ráfaga de 12 nodos
QueueHandle_t dataQueue;
const int QUEUE_SIZE = 1000; 

uint32_t last_time_node[13] = {0}; 
int jitter_count = 0;
int max_jitter = 0;

void OnDataRecv(const esp_now_recv_info_t * esp_now_info, const uint8_t *incomingDataPtr, int len) {
    struct_message tempMsg;
    memcpy(&tempMsg, incomingDataPtr, sizeof(tempMsg));
    
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(dataQueue, &tempMsg, &xHigherPriorityTaskWoken) != pdPASS) {
        // En un entorno de producción, aquí se encendería un LED rojo de fallo crítico.
        Serial.println("¡CRÍTICO! OVERFLOW MASIVO.");
    }
}

void ProcessDataTask(void * pvParameters) {
    struct_message rxMsg;
    while(1) {
        if(xQueueReceive(dataQueue, &rxMsg, portMAX_DELAY) == pdPASS) {
            
            // Si hay impacto (>2g), se imprime al instante porque es prioridad
            if(rxMsg.accel[0] > 2.0) {
                Serial.printf("\n>>> [IMPACTO] Nodo %d reporta %.2f g <<<\n\n", rxMsg.node_id, rxMsg.accel[0]);
            }

            // Cálculo silencioso de Jitter
            uint32_t current_time = millis();
            if (last_time_node[rxMsg.node_id] != 0) {
                uint32_t delta = current_time - last_time_node[rxMsg.node_id];
                int jitter = abs((int)delta - 10);
                if (jitter >= 5) {
                    jitter_count++;
                    if(jitter > max_jitter) max_jitter = jitter;
                }
            }
            last_time_node[rxMsg.node_id] = current_time;

            // Reporte de salud: Solo se imprime 1 vez por segundo (no bloquea al CPU)
            if(rxMsg.node_id == 12 && (rxMsg.timestamp % 100 == 0)) {
                int spaces = uxQueueSpacesAvailable(dataQueue);
                Serial.printf("[SALUD KNTX] Cola: %d/%d libres | Jitters>5ms: %d (Max desfase: %d ms) | BLE ON\n", spaces, QUEUE_SIZE, jitter_count, max_jitter);
                jitter_count = 0; 
                max_jitter = 0;
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); 
    delay(100); 
    
    Serial.println("\n--- GATEWAY KNTX EN LÍNEA ---");
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());

    BLEDevice::init("KNTX_Gateway_Test");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService("12345678-1234-5678-1234-56789abcdef0");
    pService->start();
    pServer->getAdvertising()->start();

    dataQueue = xQueueCreate(QUEUE_SIZE, sizeof(struct_message));

    // Aumentamos la prioridad de esta tarea a 2 para que no sea interrumpida fácilmente
    xTaskCreatePinnedToCore(ProcessDataTask, "Procesamiento", 4096, NULL, 2, NULL, 1);

    if (esp_now_init() != ESP_OK) return;
    esp_now_register_recv_cb(OnDataRecv);
}

void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }