# Laboratorio_3_FreeRTOS - ESP32 template

Proyecto mínimo para ESP32 usando ESP-IDF.

Instrucciones rápidas:
1. Instalar ESP-IDF (https://docs.espressif.com)
2. En el directorio del repo:
   idf.py set-target esp32
   idf.py build
   idf.py -p <PORT> flash

Archivos añadidos:
- main/main.c  (blink demo usando FreeRTOS driver/gpio)
- CMakeLists.txt
- main/CMakeLists.txt

Solicita adaptar a una placa concreta si quieres (por ejemplo elegir otro pin, configuraciones adicionales o usar PlatformIO).
