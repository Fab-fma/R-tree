# Proyecto 2: R-Tree
**Curso:** Algoritmo y Estructura de Datos

**Profesor:** Brenner Humberto Ojeda Rios

**Sección:** 3

**Integrantes:** 
- Fabian Nicolás Arana Espinoza
- Lucia Jimena Cartagena Miranda

## Requisito previo

Antes de ejecutar cualquier archivo, asegúrate de tener instalado:

- **Raylib** → descárgalo desde [https://www.raylib.com](https://www.raylib.com)

---

## Estructura del proyecto

```
proyecto/
├── bat/
│   ├── knn.bat
│   ├── main.bat
│   └── rango.bat
├── data/
│   ├── cities1000.txt         ← Dataset de GeoNames
│   ├── cities5000.txt         ← Dataset de GeoNames
│   └── cities15000.txt        ← Dataset de GeoNames
├── src/
│   ├── geometry.hpp
│   ├── linear_search.hpp
│   ├── loader.hpp
│   ├── rtree.hpp
│   ├── spatial_object.hpp
│   └── view.hpp
├── main.cpp
├── visual_knn.cpp
└── visual_rango.cpp
```

## Instrucciones de compilación y ejecución

**No necesitas abrir una terminal ni escribir comandos.** Todo se hace desde el Explorador de archivos.

### Pasos

1. Abre el **Explorador de archivos** (Windows + E).
2. Navega hasta la carpeta `bat/` del proyecto.
3. Haz **doble clic** en el archivo `.bat` que quieras ejecutar.
4. Se abrirá una ventana negra de comandos que compilará el programa automáticamente.
5. Cuando termine de ejecutarse (o al cerrar la ventana gráfica), la ventana de comandos mostrará `Presione una tecla para continuar . . .`
6. **Presiona `Enter`** para cerrar la ventana.

> Eso es todo. El `.bat` se encarga de compilar y ejecutar en un solo paso.

### Resumen rápido por módulo

| Archivo `.bat` | Qué hace | ¿Abre ventana gráfica? |
|-----------------|----------|------------------------|
| `main.bat` | Benchmark de rendimiento (R-Tree vs lineal) | No — solo consola |
| `knn.bat` | Consulta KNN interactiva | Sí |
| `rango.bat` | Consulta por rango interactiva | Sí |

---

## Solución de problemas

| Problema | Causa probable | Solución |
|----------|----------------|----------|
| `"g++ no se reconoce como comando interno"` | Raylib no está en `C:\raylib\` o falta w64devkit | Reinstala Raylib descomprimiendo el zip en `C:\` |
| `"cannot find -lraylib"` | La librería no está donde el `.bat` la busca | Verifica que `C:\raylib\raylib\src\` exista |
| `"No se pudo cargar objetos"` | Los archivos `.txt` no están en la carpeta `data/` | Asegúrate de que `data/cities1000.txt` (y los demás) existan |
| La ventana gráfica se abre y cierra al instante | Error en tiempo de ejecución | Revisa la consola antes de que se cierre; probablemente falta el dataset |
| Error de compilación raro en `main.bat` | El `.bat` tiene la ruta de Raylib mal escrita | Abre `main.bat` con un editor de texto y verifica que diga `C:\raylib\w64devkit\bin` |

---

## Descripción de cada módulo

### `main.bat` — Experimentos de rendimiento (consola)

Compila y ejecuta `main.cpp`. No abre ninguna ventana gráfica: todos los resultados se imprimen directamente en la consola. Cubre los **experimentos requeridos** del proyecto comparando R-Tree vs. búsqueda lineal.

Primero ejecuta una **demo de correctitud** que verifica que R-Tree y búsqueda lineal devuelvan los mismos resultados en rango y KNN, y que la eliminación funcione correctamente. Luego se repite automáticamente para **tres tamaños de dataset**: 1 000, 5 000 y 15 000 ciudades. Para cada tamaño imprime:

**1. Tiempo de construcción del R-Tree** y altura del árbol resultante.

**2. Experimento de consulta por RANGO** — efecto del tamaño del rectángulo.
Prueba tres tamaños de ventana (1%, 5% y 10% de la extensión geográfica del dataset) y reporta, para cada uno:

| Columna | Significado |
|---------|-------------|
| `rect%` | Tamaño del rectángulo como % de la extensión total |
| `RT(ms)` | Tiempo promedio de consulta con R-Tree |
| `Lin(ms)` | Tiempo promedio de búsqueda lineal |
| `speedup` | Cuántas veces es más rápido el R-Tree |
| `nodosRT` | Promedio de nodos del R-Tree visitados |
| `revLin` | Promedio de elementos revisados por la búsqueda lineal |
| `result` | Promedio de resultados encontrados |

**3. Experimento de consulta KNN** — efecto del valor de k.
Prueba k = 1, 10 y 50 vecinos y reporta:

| Columna | Significado |
|---------|-------------|
| `k` | Número de vecinos buscados |
| `RT(ms)` | Tiempo promedio de consulta con R-Tree |
| `Lin(ms)` | Tiempo promedio de búsqueda lineal |
| `speedup` | Cuántas veces es más rápido el R-Tree |
| `nodosRT` | Promedio de nodos del R-Tree visitados |
| `revLin` | Promedio de elementos revisados por la búsqueda lineal |

Cada fila es el promedio de **200 consultas aleatorias** independientes.

---

### `knn.bat` — Visualización KNN interactiva

Compila y ejecuta `visual_knn.cpp`. Abre una ventana gráfica con el mapa de ciudades. Permite ejecutar consultas de **K vecinos más cercanos** (K = 8) haciendo clic en cualquier punto del mapa.

**Controles:**
| Acción | Resultado |
|--------|-----------|
| **Clic izquierdo en cualquier punto** | Ejecuta la consulta KNN desde ese punto |
| **Rueda del mouse** | Zoom in / Zoom out |
| **Clic derecho + arrastrar** | Desplazar el mapa |
| **`ESPACIO`** | Avanza un paso en el recorrido del árbol |
| **`R`** | Reinicia el recorrido paso a paso desde el inicio |
| **`I`** | Inserta un nuevo punto en la posición del cursor |
| **`D`** | Elimina el punto más cercano al cursor |

**Qué se visualiza:**
- Punto amarillo → punto de consulta (donde hiciste clic)
- Puntos rojos → los K vecinos más cercanos encontrados
- Líneas amarillas → conexión del punto de consulta a cada vecino
- Rectángulos naranjas → MBRs de los nodos del R-Tree visitados durante la búsqueda
- Panel superior izquierdo → tiempo R-Tree vs. búsqueda lineal, speedup, y cantidad de nodos/elementos revisados

---

### `rango.bat` — Visualización por rango interactiva

Compila y ejecuta `visual_rango.cpp`. Abre una ventana gráfica con el mapa de ciudades. Permite ejecutar **consultas por rango rectangular** dibujando un rectángulo con el mouse.

**Controles:**
| Acción | Resultado |
|--------|-----------|
| **Clic izquierdo + arrastrar** | Dibuja el rectángulo de consulta |
| **Soltar el clic** | Ejecuta la consulta de rango sobre el rectángulo dibujado |
| **Rueda del mouse** | Zoom in / Zoom out |
| **Clic derecho + arrastrar** | Desplazar el mapa |
| **`ESPACIO`** | Avanza un paso en el recorrido del árbol |
| **`R`** | Reinicia el recorrido paso a paso desde el inicio |
| **`I`** | Inserta un nuevo punto en la posición del cursor |
| **`D`** | Elimina el punto más cercano al cursor |

**Qué se visualiza:**
- Rectángulo amarillo → región de consulta dibujada
- Puntos rojos → ciudades encontradas dentro del rango
- Rectángulos naranjas → MBRs de los nodos del R-Tree visitados durante la búsqueda
- Panel superior izquierdo → tiempo R-Tree vs. búsqueda lineal, speedup, cantidad de resultados y nodos/elementos revisados

---

## Dataset

El proyecto usa los archivos `cities1000.txt`, `cities5000.txt` y `cities15000.txt` del dataset **GeoNames**. Estos archivos deben estar dentro de la carpeta `data/`.

---

## Fuentes

GeoNames. (2026). GeoNames Cities [Conjunto de datos]. https://download.geonames.org/export/dump/