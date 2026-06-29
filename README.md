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
│   ├── editar.bat             ← Compila y ejecuta la visualización de inserción/eliminación
│   ├── knn.bat                ← Compila y ejecuta la visualización KNN
│   ├── main.bat               ← Compila y ejecuta el benchmark en consola
│   └── rango.bat              ← Compila y ejecuta la visualización por rango
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
├── visual_editar.cpp
├── visual_knn.cpp
└── visual_rango.cpp
```

---

## Cómo ejecutar

Cada módulo del proyecto tiene su propio archivo `.bat` dentro de la carpeta `bat/`. El proceso es el mismo para todos:

### Pasos

1. Abre el **Explorador de archivos** y navega hasta la carpeta `bat/` del proyecto.
2. Haz **doble clic** sobre el `.bat` que quieres ejecutar.
3. Se abrirá una ventana de comandos. Cuando el programa termine de compilar (o al cerrar la ventana), **presiona `Enter`** para cerrarla.

> Si ves un error de compilación, verifica que Raylib esté instalado en `C:\raylib\` y que los datasets estén en `data\`.

---

## Descripción de cada `.bat`

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

### `editar.bat` — Visualización de inserción y eliminación

Compila y ejecuta `visual_editar.cpp`. Abre una ventana gráfica con el mapa de ciudades y permite **insertar y eliminar puntos** en tiempo real, comparando el costo de cada operación entre el R-Tree y la solución naive (vector de punteros).

**Controles:**
| Acción | Resultado |
|--------|-----------|
| **Clic izquierdo** | Selecciona el punto más cercano al cursor |
| **`I`** | Inserta un nuevo punto en la posición del cursor |
| **`D`** | Elimina el punto actualmente seleccionado |
| **Rueda del mouse** | Zoom in / Zoom out |
| **Clic derecho + arrastrar** | Desplazar el mapa |

**Qué se visualiza:**
- Punto amarillo → punto actualmente seleccionado
- Panel superior izquierdo → última operación realizada (INSERTAR o ELIMINAR), tiempo del R-Tree vs. naive en microsegundos, y cantidad de puntos actuales

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

**Qué se visualiza:**
- Rectángulo amarillo → región de consulta dibujada
- Puntos rojos → ciudades encontradas dentro del rango
- Rectángulos naranjas → MBRs de los nodos del R-Tree visitados durante la búsqueda
- Panel superior izquierdo → tiempo R-Tree vs. búsqueda lineal, speedup, cantidad de resultados y nodos/elementos revisados

---

## Dataset

El proyecto usa los archivos `cities1000.txt`, `cities5000.txt` y `cities15000.txt` del dataset **GeoNames**.

## Fuentes

GeoNames. (2026). GeoNames Cities [Conjunto de datos]. https://download.geonames.org/export/dump/
