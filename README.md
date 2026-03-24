# 📘 Práctica 4 – Parte 3 (IA)

## Proyecto con FreeRTOS y generación asistida por IA

---

## 📌 Descripción

En esta parte de la práctica se desarrolla un **proyecto completo basado en FreeRTOS**, utilizando **inteligencia artificial como apoyo en el diseño e implementación**.

El proyecto consiste en una **estación meteorológica IoT**, que recoge datos de sensores reales y los muestra a través de una **página web embebida en el ESP32**.

---

## 🎯 Objetivos

* Aplicar los conceptos de FreeRTOS en un sistema real
* Diseñar un sistema multitarea completo
* Utilizar **IA como herramienta de desarrollo**
* Implementar comunicación entre tareas
* Integrar sensores y visualización web

---

## ⚙️ Funcionamiento del sistema

El sistema se compone de varias tareas concurrentes:

* 🔹 **Task_Sensores**

  * Lee datos de temperatura, humedad y presión (DHT22 + BMP280)

* 🔹 **Task_WiFi**

  * Gestiona la conexión a la red WiFi

* 🔹 **Task_Web**

  * Atiende peticiones HTTP y sirve la página web

* 🔹 **Task_Alertas**

  * Activa LED/buzzer si se superan umbrales

* 🔹 **Task_Monitor**

  * Supervisa el estado de las tareas y el uso de memoria

---

## 🔄 Comunicación entre tareas

* **Cola (Queue)**
  → envío de datos de sensores a otras tareas

* **Mutex**
  → protección de los datos compartidos

* **Semáforo binario**
  → activación de alertas

---

## 🔌 Hardware utilizado

* ESP32
* DHT22 → temperatura y humedad
* BMP280 → presión atmosférica
* LED (alerta)
* Buzzer (opcional)

---

## 🌐 Interfaz web

El ESP32 actúa como servidor web.

Desde un navegador se puede acceder a la IP del dispositivo y visualizar:

* temperatura
* humedad
* presión
* estado del sistema (normal/alerta)

La página se actualiza automáticamente cada pocos segundos.

---

## ▶️ Ejecución

1. Configurar red WiFi en el código
2. Subir el programa al ESP32
3. Abrir el monitor serie
4. Obtener la dirección IP
5. Acceder desde el navegador

Ejemplo:

```id="2r7m0q"
http://192.168.1.45/
```

---

## 🔍 Resultado esperado

* Lecturas reales de sensores
* Página web funcional
* Actualización automática de datos
* Activación de alertas al superar umbrales
* Ejecución concurrente en ambos núcleos

---

## 🤖 Uso de Inteligencia Artificial

La IA ha sido utilizada para:

* Generación del código base
* Diseño de la arquitectura de tareas
* Implementación de comunicación (colas, semáforos, mutex)
* Desarrollo de la interfaz web
* Optimización del sistema

---

## ✅ Conclusión

Esta parte demuestra la aplicación práctica de FreeRTOS en un sistema real, integrando múltiples tareas, sensores físicos y una interfaz web, con el apoyo de herramientas de inteligencia artificial para acelerar el desarrollo.

---
