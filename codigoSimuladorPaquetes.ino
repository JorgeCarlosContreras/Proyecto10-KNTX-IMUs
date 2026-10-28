#include <esp_now.h>
#include <WiFi.h>

// LA MAC EXACTA DE TU ESP32-S3 GATEWAY
uint8_t gatewayAddress[] = {0xE0, 0x72, 0xA1, 0xF4, 0xEB, 0xD8}; 

typedef struct struct_message {
    uint8_t node_id;
    uint32_t timestamp;
    float quat[4];
    float accel[3];
} struct_message;

struct_message fakeData;
esp_now_peer_info_t peerInfo;

// Variables de inyección de fallos controladas por el Monitor Serie
bool simulate_node_loss = false;
bool simulate_impact = false;
uint32_t loss_start_time = 0;

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error inicializando ESP-NOW");
        return;
    }

    memcpy(peerInfo.peer_addr, gatewayAddress, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Error vinculando con el Gateway ESP32-S3");
        return;
    }
    Serial.println("\n--- SPOOFER KNTX LISTO ---");
    Serial.println("Generando 1,200 paquetes por segundo...");
    Serial.println("Comandos de estrés:");
    Serial.println("Escribe 'A' -> Simular desconexión del Nodo 5");
    Serial.println("Escribe 'B' -> Simular impacto brusco en Nodo 2");
}

void loop() {
    // 1. Leer comandos de inyección de errores desde tu teclado
    if (Serial.available() > 0) {
        char cmd = Serial.read();
        if (cmd == 'A' || cmd == 'a') {
            simulate_node_loss = true;
            loss_start_time = millis();
            Serial.println("--> [FALLO INYECTADO]: Nodo 5 apagado por 2 segundos.");
        }
        if (cmd == 'B' || cmd == 'b') {
            simulate_impact = true;
            Serial.println("--> [IMPACTO INYECTADO]: Nodo 2 reportando 3.5 g.");
        }
    }

    // 2. Lógica para reconectar el Nodo 5 después de la falla
    if (simulate_node_loss && (millis() - loss_start_time > 2000)) {
        simulate_node_loss = false;
        Serial.println("--> [RECUPERACIÓN]: Nodo 5 reconectado.");
    }

    // 3. Generar el tráfico masivo de red (12 nodos * 100 Hz = 1,200 pkt/seg)
    uint32_t loop_start = millis();

    for (uint8_t i = 1; i <= 12; i++) {
        // Omitir el envío si el nodo 5 está en estado de falla
        if (simulate_node_loss && i == 5) continue;

        fakeData.node_id = i;
        fakeData.timestamp = millis();
        fakeData.quat[0] = 1.0; fakeData.quat[1] = 0.0; fakeData.quat[2] = 0.0; fakeData.quat[3] = 0.0;
        
        // Simular impacto en Nodo 2
        if (simulate_impact && i == 2) {
            fakeData.accel[0] = 3.5; // Pico superior a 2g
            simulate_impact = false; // Se resetea después de dispararlo
        } else {
            fakeData.accel[0] = 1.0; // Gravedad normal de reposo
        }
        fakeData.accel[1] = 0.0; fakeData.accel[2] = 0.0;

        // Enviar paquete vía ESP-NOW
        esp_now_send(gatewayAddress, (uint8_t *) &fakeData, sizeof(fakeData));
    }

    // 4. Mantener la frecuencia estricta del hardware a 100 Hz (10 milisegundos)
    uint32_t process_time = millis() - loop_start;
    if (process_time < 10) {
        delay(10 - process_time);
    }
}