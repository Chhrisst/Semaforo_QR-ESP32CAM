#include <WebServer.h>  // Incluye la biblioteca para manejar el servidor web
#include <WiFi.h>      // Incluye la biblioteca para manejar la conexión WiFi
#include <esp32cam.h>  // Incluye la biblioteca para manejar la cámara ESP32
// Credenciales de la red WiFi
const char* WIFI_SSID = "TU RED";  // Nombre de la red WiFi
const char* WIFI_PASS = "TU CLAVE";  // Contraseña de la red WiFi
WebServer server(80);  // Crea un servidor web en el puerto 80

// Configuración de la resolución de la cámara
static auto hiRes = esp32cam::Resolution::find(800, 600);  // Busca una resolución de 800x600
// Definición de pines para los LEDs
int pinVerde = 15;    // Pin del LED verde (15)
int pinAmarillo = 13; // Pin del LED amarillo (13)
int pinRojo = 12;     // Pin del LED rojo (12)
const int internalLedPin = 4; // Pin del LED integrado del ESP32-CAM
String qrData = "";  // Variable para almacenar los datos del código QR
bool emergencia = false;  // Bandera para indicar si hay una emergencia
bool ledVerdeEncendido = false; // Bandera para el estado del LED verde
bool semaforoDetenido = false; // Bandera para controlar el estado del semáforo
// Enumeración para representar los estados del semáforo
enum EstadoSemáforo { ROJO, AMARILLO, VERDE, EMERGENCIA };  // Define los estados del semáforo
EstadoSemáforo estadoActual = ROJO;  // Estado inicial del semáforo
unsigned long tiempoInicioEstado = 0; // Variable para medir el tiempo en cada estado

void setup() {
  Serial.begin(115200);  // Inicializa la comunicación serie a 115200 bps
  // Configura los pines de los LEDs como salidas
  pinMode(pinVerde, OUTPUT);    // Configura el pin del LED verde como salida
  pinMode(pinAmarillo, OUTPUT); // Configura el pin del LED amarillo como salida
  pinMode(pinRojo, OUTPUT);     // Configura el pin del LED rojo como salida
  pinMode(internalLedPin, OUTPUT); // Configura el pin del LED integrado como salida
  // Inicializa los LEDs en el estado inicial
  digitalWrite(pinVerde, LOW);    // Asegúrate de que el LED verde esté apagado al inicio
  digitalWrite(pinAmarillo, HIGH); // Enciende el LED amarillo al inicio
  digitalWrite(pinRojo, LOW);      // Asegúrate de que el LED rojo esté apagado al inicio
  digitalWrite(internalLedPin, LOW); // Apaga el LED integrado del ESP32-CAM
  using namespace esp32cam;  // Utiliza el espacio de nombres esp32cam
  Config cfg;  // Crea una configuración para la cámara
  cfg.setPins(pins::AiThinker);  // Configura los pines para la cámara AiThinker
  cfg.setResolution(hiRes);  // Establece la resolución de la cámara
  cfg.setBufferCount(2);  // Establece el número de buffers de imagen
  cfg.setJpeg(80);  // Establece la calidad JPEG de la imagen

  bool ok = Camera.begin(cfg);  // Inicializa la cámara con la configuración
  Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");  // Imprime el estado de la cámara
  WiFi.begin(WIFI_SSID, WIFI_PASS);  // Inicia la conexión WiFi
  while (WiFi.status() != WL_CONNECTED) {  // Espera hasta que esté conectado
    delay(500);  // Espera medio segundo entre intentos
  }
  Serial.print("http://");  // Imprime la dirección IP del servidor Serial.println(WiFi.localIP());  // Imprime la IP local del ESP32

  // Define las rutas para manejar las solicitudes HTTP
  server.on("/", sendHtml);  // Ruta para servir la página HTML
  server.on("/cam-hi.jpg", handleJpgHi);  // Ruta para capturar imágenes de alta resolución
  server.on("/qr-data", handleQrData);    // Ruta para recibir datos del código QR
  server.on("/on", handleOn);   // Ruta para encender el LED verde
  server.on("/off", handleOff);  // Ruta para apagar el LED verde
  server.begin();  // Inicia el servidor web
}
void loop() {
  server.handleClient();  // Maneja las solicitudes del cliente
  // Lógica del semáforo
  if (!semaforoDetenido) {  // Solo ejecuta la lógica del semáforo si no está detenido
    switch (estadoActual) {
      case ROJO:  // Estado ROJO
        digitalWrite(pinRojo, HIGH);   // Enciende el LED rojo
        digitalWrite(pinAmarillo, LOW);   // Apaga el LED amarillo
        digitalWrite(pinVerde, LOW);    // Apaga el LED verde
        if (millis() - tiempoInicioEstado >= 5000) { // Si han pasado 5 segundos
          estadoActual = AMARILLO;  // Cambia al estado AMARILLO
          tiempoInicioEstado = millis();  // Reinicia el temporizador
        }
        break;
      case AMARILLO:  // Estado AMARILLO
        digitalWrite(pinRojo, LOW);   // Apaga el LED rojo
        digitalWrite(pinAmarillo, HIGH);  // Enciende el LED amarillo
        digitalWrite(pinVerde, LOW);    // Apaga el LED verde
        if (millis() - tiempoInicioEstado >= 2000) { // Si han pasado 2 segundos
          estadoActual = VERDE;  // Cambia al estado VERDE
          tiempoInicioEstado = millis();  // Reinicia el temporizador
        }
        break;
      case VERDE:  // Estado VERDE
        digitalWrite(pinRojo, LOW);   // Apaga el LED rojo
        digitalWrite(pinAmarillo, LOW);   // Apaga el LED amarillo
        digitalWrite(pinVerde, HIGH);  // Enciende el LED verde
        if (millis() - tiempoInicioEstado >= 5000) { // Si han pasado 5 segundos
          estadoActual = ROJO;  // Cambia al estado ROJO
          tiempoInicioEstado = millis();  // Reinicia el temporizador
        }
        break;
      case EMERGENCIA:  // Estado de EMERGENCIA
        digitalWrite(pinVerde, HIGH);  // Enciende el LED verde
        digitalWrite(pinRojo, LOW);   // Apaga el LED rojo
        digitalWrite(pinAmarillo, LOW);   // Apaga el LED amarillo
        if (millis() - tiempoInicioEstado >= 10000) { // Si han pasado 10 segundos
          estadoActual = ROJO; // Volver al estado ROJO
          tiempoInicioEstado = millis();  // Reinicia el temporizador
        }
        break;
    }
  }

  // Verificar si hay una emergencia
  if (emergencia) {
    estadoActual = EMERGENCIA; // Cambiar al estado de EMERGENCIA
    emergencia = false; // Reiniciar la bandera de emergencia
    tiempoInicioEstado = millis();  // Reinicia el temporizador
  }

  // Mantener el LED verde encendido si la bandera está activa
  if (ledVerdeEncendido) {
    digitalWrite(pinVerde, HIGH);  // Asegúrate de que el LED verde esté encendido
  } else {
    digitalWrite(pinVerde, LOW);  // Apaga el LED verde si la bandera está desactivada
  }
}

// Función que envía el HTML
void sendHtml() {
  String response = R"html(
  <!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Boton de Emergencia</title>
  <style>
    body { display: flex; flex-direction: column; align-items: center; justify-content: flex-start; height: 100vh; margin: 0; font-family: Arial, sans-serif; }
    #rectangulo { width: 100%; background-color: red; color: white; text-align: center; padding: 20px; font-size: 50px; font-weight: bold; }
    #semaforo { width: 120px; height: 280px; background-color: transparent; border-radius: 80px; padding: 10px; display: flex; flex-direction: column;
      align-items: center; justify-content: space-around; box-shadow: 0px 0px 10px rgba(0, 0, 0, 0.5); margin-top: 100px; border: 5px solid gray; position: relative; }
    #semaforo .detalle { width: 100px; height: 250px; background-color: transparent; border-radius: 70px; border: 5px solid gray; padding: 10px;
      display: flex; flex-direction: column; align-items: center; justify-content: space-around; }
    .luz { width: 30px; height: 30px; border-radius: 50%; background-color: transparent; transition: background-color 0.3s; }
    .rojo { border: 5px solid red; }
    .amarillo { border: 5px solid yellow; }
    .verde { border: 5px solid green; }
    #movimiento { color: white; position: absolute; width: 70px; height: 70px; font-size: 18px; font-weight: bold;
      cursor: pointer; border: none; border-radius: 50%; top: 240px; background-color: red; transition: top 0.3s ease, background-color 0.3s ease;}
  </style>
</head>
<body>
  <div id="rectangulo">EMERGENCIA</div>
  <div id="semaforo">
    <div class="detalle">
      <div class="luz rojo" id="rojo"></div>
      <div class="luz amarillo" id="amarillo"></div>
      <div class="luz verde" id="verde"></div>
    </div>
  </div>
  <button id="movimiento" class="off-position" onclick="toggleLight()">OFF</button>

  <script>
    async function toggleLight() {
      const button = document.getElementById('movimiento');
      const verde = document.getElementById('verde');
     
      if (button.innerText === 'OFF') {
        button.innerText = 'ON';
        button.style.top = "400px";  
        button.style.backgroundColor = 'green';
        await fetch('/on');
      } else {
        button.innerText = 'OFF';
        button.style.top = "240px";
        button.style.backgroundColor = 'red';
        await fetch('/off');
      }
    }
  </script>
</body>
</html>
  )html";
  server.send(200, "text/html", response);
}
// Función que maneja la solicitud para encender el LED verde
void handleOn() {
  ledVerdeEncendido = true;  // Establece la bandera para encender el LED verde
  semaforoDetenido = true;   // Detiene el semáforo
  digitalWrite(pinRojo, LOW); // Asegúrate de que el LED rojo esté apagado
  digitalWrite(pinAmarillo, LOW); // Asegúrate de que el LED amarillo esté apagado
  digitalWrite(pinVerde, HIGH);  // Enciende el LED verde
  server.send(200, "text/plain", "LED verde encendido y semáforo detenido");
}

// Función que maneja la solicitud para apagar el LED verde
void handleOff() {
  ledVerdeEncendido = false;  // Establece la bandera para apagar el LED verde
  semaforoDetenido = false;   // Reanuda el semáforo
  server.send(200, "text/plain", "Semáforo en funcionamiento");
}

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

void handleQrData() {
  if (server.hasArg("data")) {  // Verifica si hay un argumento "data"
    qrData = server.arg("data"); // Obtener el dato del QR
    Serial.print("QR Data Received: ");  // Imprime el mensaje de recepción
    Serial.println(qrData); // Imprimir el dato en el monitor serial

    if (qrData == "EMERGENCIA") {  // Si el dato recibido es "EMERGENCIA"
      emergencia = true;  // Establece la bandera de emergencia
      Serial.println("Emergencia detectada. Semáforo en verde.");  // Mensaje de emergencia
    } else {
      emergencia = false;  // No hay emergencia
      Serial.println("Fin de emergencia.");  // Mensaje de fin de emergencia
    }

    // Actualizar los LEDs según el estado de emergencia
    if (emergencia) {
      digitalWrite(pinVerde, HIGH);  // Enciende el LED verde
      digitalWrite(pinRojo, HIGH);   // Enciende el LED rojo 
      digitalWrite(pinAmarillo, HIGH);   // Enciende el LED amarillo
    } else {
      digitalWrite(pinVerde, LOW); // Apaga el LED verde
      digitalWrite(pinRojo, LOW); // Apaga el LED rojo
      digitalWrite(pinAmarillo, LOW);  // Apaga el LED amarillo
    }
  }
  server.send(200, "text/plain", "QR Data Received");  // Envía una respuesta de éxito al cliente
}
