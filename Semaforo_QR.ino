//BIBLIOTECAS
#include <WebServer.h>  // Biblioteca para el servidor web
#include <WiFi.h>       // Biblioteca para la conexión WiFi
#include <esp32cam.h>   // Biblioteca para la cámara ESP32

const char* WIFI_SSID = "TU RED";    // Nombre de la red WiFi
const char* WIFI_PASS = "TU CLAVE";  // Contraseña de la red WiFi

WebServer server(80);  // Crea un servidor web en el puerto 80

// LEDs
const int L1_Red = 12;         // Variable para el Pin del LED rojo
const int L1_Yellow = 13;      // Variable para el Pin del LED amarillo
const int L1_Green = 2;       // Variable para el Pin del LED verde

bool Emergency = false; // Bandera para indicar si hay una emergencia
bool GreenON = false;  // Bandera para el estado del LED verde

// Enumeración para representar los estados del semáforo
enum TF_Estate {ROJO, AMARILLO, VERDE, EMERGENCIA};  // Define los estados del semáforo
TF_Estate Estate = ROJO;  // Estado inicial del semáforo
bool TLightActive = true;  // Bandera para controlar el estado del semáforo

// Variables para control de tiempo
unsigned long Time = 0, EmergencyStart = 0; //Tiempo de Inicio
const unsigned long Time_Green = 5000, Time_Yellow = 1000, Time_Red = 5000, Time_Emergency = 5000; // Variable para medir el tiempo en cada estado

//MÉTODO DE PREPARACIÓN
void setup() {
  Serial.begin(115200);
  
  pinMode(L1_Red, OUTPUT);
  pinMode(L1_Yellow, OUTPUT);
  pinMode(L1_Green, OUTPUT);
  
  // Configura los LEDs apagados inicialmente
  digitalWrite(L1_Red, LOW);
  digitalWrite(L1_Yellow, LOW);
  digitalWrite(L1_Green, LOW);

  // Configuración WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000); // Espera medio segundo entre intentos
    Serial.print(".");
  }
  Serial.println("\nConectado. Dirección IP: " + WiFi.localIP().toString()); // Imprime la IP local del ESP32

  // Configuración de la cámara
  esp32cam::Config cfg;
  cfg.setPins(esp32cam::pins::AiThinker); // Configura los pines para la cámara AiThinker
  cfg.setResolution(esp32cam::Resolution::find(320, 240)); // Establece la resolución de la cámara
  cfg.setBufferCount(2);  // Establece el número de buffers de imagen
  cfg.setJpeg(90); // Establece la calidad JPEG de la imagen
  
  if (!esp32cam::Camera.begin(cfg)) {
    Serial.println("Error inicializando la cámara");
    return;
  }

  // Rutas del servidor
  server.on("/", []() {
    server.send(200, "text/html",
      "<!DOCTYPE html>"
      "<html lang='es'>"
      "<head>"
      "<meta charset='UTF-8'>"
      "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
      "<title>Botón de Emergencia</title>"
      "<style>"
      "body { display: flex; flex-direction: column; align-items: center; justify-content: flex-start; height: 100vh; margin: 0; font-family: Arial, sans-serif; }"
      "#rectangulo { width: 100%; background-color: red; color: white; text-align: center; padding: 20px; font-size: 50px; font-weight: bold; }"
      "#semaforo { width: 120px; height: 280px; background-color: transparent; border-radius: 80px; padding: 10px; display: flex; flex-direction: column; align-items: center; justify-content: space-around; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.5); margin-top: 100px; border: 5px solid gray; position: relative; }"
      "#semaforo .detalle { width: 100px; height: 250px; background-color: transparent; border-radius: 70px; border: 5px solid gray; padding: 10px; display: flex; flex-direction: column; align-items: center; justify-content: space-around; }"
      ".luz { width: 30px; height: 30px; border-radius: 50%; background-color: transparent; transition: background-color 0.3s; }"
      ".rojo { border: 5px solid red; }"
      ".amarillo { border: 5px solid yellow; }"
      ".verde { border: 5px solid green; }"
      "#movimiento { color: white; position: absolute; width: 70px; height: 70px; font-size: 18px; font-weight: bold; cursor: pointer; border: none; border-radius: 50%; top: 240px; background-color: red; transition: top 0.3s ease, background-color 0.3s ease;}"
      "</style>"
      "</head>"
      "<body>"
      "<div id='rectangulo'>EMERGENCIA</div>"
      "<div id='semaforo'>"
      "<div class='detalle'>"
      "<div class='luz rojo' id='rojo'></div>"
      "<div class='luz amarillo' id='amarillo'></div>"
      "<div class='luz verde' id='verde'></div>"
      "</div>"
      "</div>"
      "<button id='movimiento' class='off-position' onclick='toggleLight()'>OFF</button>"
      "<script>"
      "async function toggleLight() {"
      "  const button = document.getElementById('movimiento');"
      "  if (button.innerText === 'OFF') {"
      "    button.innerText = 'ON';"
      "    button.style.top = '400px';"
      "    button.style.backgroundColor = 'green';"
      "    await fetch('/on');"
      "  } else {"
      "    button.innerText = 'OFF';"
      "    button.style.top = '240px';"
      "    button.style.backgroundColor = 'red';"
      "    await fetch('/off');"
      "  }"
      "}"
      "</script>"
      "</body>"
      "</html>");
  });

  server.on("/cam-hi.jpg", handleJpgHi);  // Ruta para capturar imágenes de alta resolución

  server.on("/on", []() {
    Estate = EMERGENCIA;
    TLightActive = false; // Detiene el flujo normal
    actualizarLuces();
    server.send(200, "text/plain", "Aviso: Emergencia");
  });

  server.on("/off", []() {
    Estate = ROJO;
    delay(2000);
    TLightActive = true; // Restaura el flujo normal
    Time = millis();
    server.send(200, "text/plain", "Semáforo restaurado");
  });

  // Ruta para detectar QR
  server.on("/qr", []() {
    if (server.hasArg("data")) {
        String qrTexto = server.arg("data");
        Serial.println("QR recibido: " + qrTexto);

        if (qrTexto.equalsIgnoreCase("emergencia")) {
            Serial.println("Estado de emergencia activado.");
            Estate = EMERGENCIA; 
            TLightActive = false; // Detiene el flujo normal del semáforo
            EmergencyStart = 0;
            actualizarLuces();
        } 
        delay(6000);
        Estate = ROJO;
        delay(2000);
        TLightActive = true; // Restaura el flujo normal
        Time = millis();
        server.send(200, "application/json", "{\"qr\":\"" + qrTexto + "\"}");
    } else {
        Serial.println("Error: Falta parámetro 'data' en la solicitud");
        server.send(400, "application/json", "{\"error\":\"Falta parámetro 'data'.\"}");
    }
  });


  server.begin(); // Inicia el servidor web
  Time = millis();
}

//MÉTODO DE PROCESAMIENTO DE IMÁGENES 
void handleJpgHi() {
  auto frame = esp32cam::capture();  // Captura un frame de la cámara
  if (frame == nullptr) {  // Verifica si la captura fue exitosa
    server.send(503, "", "");  // Envía un error 503 si no hay frame
    return;  // Sale de la función
  }
  server.setContentLength(frame->size());  // Establece la longitud del contenido
  server.send(200, "image/jpeg");  // Envía la imagen con un código 200
  WiFiClient client = server.client();  // Obtiene el cliente WiFi
  frame->writeTo(client);  // Escribe el frame al cliente
}

// BUCLE PRINCIPAL
void loop() {
  server.handleClient();

  if (TLightActive) {
    gestionarSemaforo();
  }
}

// Lógica del semáforo
void gestionarSemaforo() {
  unsigned long tiempoActual = millis();
  
  switch (Estate) {
    case ROJO:
      if (tiempoActual - Time >= Time_Red) {
        Estate = AMARILLO; // Cambia a amarillo
        Time = tiempoActual; // Reiniciar temporizador
        actualizarLuces();
      }
      break;
    case AMARILLO:
      if (tiempoActual - Time >= Time_Yellow) {
        Estate = VERDE; // Cambia a verde
        Time = tiempoActual; // Reiniciar temporizador
        actualizarLuces();
      }
      break;
    case VERDE:
      if (tiempoActual - Time >= Time_Green) {
        Estate = ROJO; // Cambia a rojo
        Time = tiempoActual; // Reiniciar temporizador
        actualizarLuces();
      }
      break;
    
    case EMERGENCIA:
      if (EmergencyStart == 0) 
          EmergencyStart = tiempoActual;
      if (tiempoActual - EmergencyStart >= Time_Emergency) {
          Estate = ROJO;
          EmergencyStart = 0;
          TLightActive = true;
          Time = tiempoActual;
      }
      actualizarLuces();
      break;
    
    default:
      // En caso de error, reiniciar al estado ROJO
      Estate = ROJO;
      Time = tiempoActual; // Reiniciar temporizador
      actualizarLuces();
      break;
  }
  actualizarLuces();
}

//MÉTODO PARA CONTROLAR LOS LEDS Y ACTUALIZAR SU ESTADO
void actualizarLuces() {
  digitalWrite(L1_Red, Estate == ROJO ? HIGH : LOW);
  digitalWrite(L1_Yellow, Estate == AMARILLO ? HIGH : LOW);
  digitalWrite(L1_Green, Estate == VERDE || Estate == EMERGENCIA ?HIGH:LOW);
}
