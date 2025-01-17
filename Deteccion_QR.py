#----------------------------------------
#LIBRERIAS
#----------------------------------------
import cv2  # Importa la biblioteca OpenCV para procesamiento de imágenes
import numpy as np  # Importa NumPy para operaciones numéricas
import pyzbar.pyzbar as pyzbar  # Importa pyzbar para decodificar códigos QR
import urllib.request  # Importa urllib para realizar solicitudes HTTP
import os  # Importa el módulo os para reiniciar el script
import sys  # Importa sys para obtener el nombre del script

#----------------------------------------
#CONFIGURACION INICIAL DE LA IP, VENTANA Y VARIABLES
#----------------------------------------
# URL base del ESP32-CAM
url_base = 'http://192.168.X.X/'  # Cambia esto según tu configuración de red
font = cv2.FONT_HERSHEY_PLAIN  # Define la fuente para el texto en la imagen
cv2.namedWindow("Transmision del ESP32 CAM", cv2.WINDOW_AUTOSIZE)  # Crea una ventana para mostrar la transmisión en vivo

prev = ""  # Variable para almacenar el último dato QR procesado
pres = ""  # Variable para almacenar el dato QR actual

#----------------------------------------
#BUCLE PRINCIPAL DE CAPTURA Y PROCESAMIENTO DE IMAGENES DEL ESP32-CAM
#----------------------------------------
while True:  # Bucle principal para la captura de imágenes
    # Solicita la imagen de la cámara ESP32
    img_resp = urllib.request.urlopen(url_base + 'cam-hi.jpg')
    # Convierte la respuesta de bytes a un array de NumPy
    imgnp = np.array(bytearray(img_resp.read()), dtype=np.uint8)
    # Decodifica la imagen en formato adecuado para OpenCV
    frame = cv2.imdecode(imgnp, -1)

    # Redimensiona el fotograma para adaptarlo a la ventana
    width = 240  # Ancho deseado
    height = 240  # Alto deseado
    frame = cv2.resize(frame, (width, height))

    # Decodifica los objetos QR presentes en la imagen
    decodedObjects = pyzbar.decode(frame)
    for obj in decodedObjects:  # Itera sobre cada objeto QR detectado
        pres = obj.data.decode('utf-8')  # Decodifica el dato QR a una cadena de texto
        if prev != pres:  # Verifica si el dato QR ha cambiado
            print("Tipo:", obj.type)  # Imprime el tipo de código QR
            print("Dato: ", pres)  # Imprime el dato QR
            prev = pres  # Actualiza el último dato procesado
            
            # Envía el dato QR al ESP32 a través de una solicitud HTTP
            url_send = url_base + 'qr?data=' + urllib.parse.quote(pres)  # Codifica el dato para la URL
            urllib.request.urlopen(url_send)  # Realiza la solicitud para enviar el dato

            # Verifica si se detecta "EMERGENCIA"
            if pres == "EMERGENCIA":
                print("EMERGENCIA detectada")
                os.execv(sys.executable, ['python'] + [sys.argv[0]])  # Reinicia el script actual

        # Muestra el dato QR en la imagen
        cv2.putText(frame, str(obj.data), (50, 50), font, 2, (255, 0, 0), 3)

    # Muestra la imagen redimensionada en la ventana creada
    cv2.imshow("Transmision del ESP32 CAM", frame)

    #----------------------------------------
    #TECLA ESPECIAL PARA CERRAR LA TRANSMISION
    #----------------------------------------
    key = cv2.waitKey(1)  # Espera 1 milisegundo para detectar una tecla
    if key == ord('q'):  # Si se presiona 'q', sale del bucle
        break
cv2.destroyAllWindows()  # Cierra todas las ventanas al finalizar
