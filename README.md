# Simulación y Gestión QoS de Red en Coworking (NS-3)

## Descripción del Proyecto
Este proyecto modela y simula la infraestructura de red de un coworking utilizando el simulador de eventos discretos NS-3 (implementado en C++). El objetivo principal es resolver la congestión generada por la coexistencia de dos perfiles de usuarios con necesidades de red completamente opuestas:
*   **Usuarios de Trading:** Requieren latencia ultrabaja (<50ms) y jitter mínimo para enviar órdenes financieras críticas (tráfico UDP).
*   **Usuarios de Diseño:** Generan transferencias masivas de archivos pesados hacia la nube que consumen todo el ancho de banda disponible (tráfico TCP).

Actualmente, ambos tráficos compiten en una cola FIFO en el router de salida hacia Internet, lo que degrada la operativa financiera. Este estudio evalúa el impacto de segmentar la red y aplicar políticas de Calidad de Servicio (QoS) frente a la alternativa de simplemente aumentar el ancho de banda del enlace WAN.

## Arquitectura y Tecnologías
*   **Simulador:** NS-3 (versión 3.45) basado en C++.
*   **Gestión de Colas (QoS):** Comparativa experimental entre `FifoQueueDisc` (modelo tradicional sin prioridad) y `PfifoFastQueueDisc` (Prioridad Estricta etiquetando el tráfico crítico con TOS 0x70).
*   **Modelado de Tráfico Estocástico:** 
    *   Uso de `OnOffApplication` sobre UDP para generar ráfagas bursátiles realistas (Trading).
    *   Uso de `BulkSendApplication` sobre TCP para simular saturación agresiva del enlace (Diseño).
*   **Análisis y Monitorización:** Implementación del patrón *Observador* en C++ mediante "Vigilantes" conectados a las trazas del simulador para extraer métricas en tiempo real, complementado con análisis de archivos `.pcap` en Wireshark.

## Compilación y Ejecución
El proyecto está diseñado para ejecutarse en el simulador NS-3 parametrizado por consola, lo que permite probar diferentes escenarios de estrés sin modificar el código.

**1. Preparación del Entorno:**
Asegúrate de copiar todos los archivos del código fuente (el archivo `.cc` principal y las cabeceras/clases vigilantes auxiliares) dentro del subdirectorio `scratch/` de tu instalación de NS-3.

**2. Compilación del Proyecto:**
Desde la raíz del directorio de NS-3, configura y compila el entorno para que reconozca los nuevos ficheros:
```bash
./ns3 configure
./ns3 build
```

**3. Ejecución Básica (Valores por defecto):**
Una vez compilado con éxito, lanza la simulación con los parámetros estándar (FIFO, 150 Mbps, 50 Trading / 25 Diseño):
```bash
NS_LOG="coworking" ./ns3 run "coworking"
```

**4. Ejecución de Escenarios Personalizados:**
Puedes modificar variables clave en caliente. Ejemplo para activar Prioridad Estricta (QoS) en un enlace de 130 Mbps con 20 usuarios de diseño:
```bash
./ns3 run "coworking --useQoS=true --wanBW=130000000bps --nDesign=20"
```

**5. Generación de Trazas para Wireshark:**
Si deseas extraer los archivos `.pcap` para inspeccionar el flujo de paquetes y comprobar visualmente las etiquetas TOS de prioridad:
```bash
./ns3 run "coworking --tracing=true"
```

**6. Modo Experimentación (Extracción de Gráficas):**
Para lanzar ejecuciones y extraer los datos necesarios para generar las gráficas comparativas de rendimiento (Latencia, Jitter, Throughput y Pérdida):
```bash
./ns3 run "coworking --generatePlots=true"
```

## 📊 Conclusiones del Estudio
La experimentación demostró que la implementación de **Prioridad Estricta (QoS)** es capaz de reducir la latencia del Trading al mínimo físico (~21 ms) sin importar la congestión. Sin embargo, para cumplir con el requisito de 0% de pérdida de paquetes y mantener a los diseñadores operativos, se concluyó como solución definitiva aplicar QoS junto a un dimensionamiento de la línea WAN a **130 Mbps**.
