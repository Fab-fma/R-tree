#pragma once
// ============================================================================
//  view.hpp - utilidades compartidas por las dos ventanas (rango y KNN).
//  Maneja: el mapeo mundo<->pantalla, dibujar MBRs, la textura de puntos
//  (optimizacion de rendimiento) y la medicion de tiempos promediada.
// ============================================================================
#include "raylib.h"
#include <vector>
#include <chrono>
#include "geometry.hpp"
#include "spatial_object.hpp"

const int   SCREEN_W = 1000;
const int   SCREEN_H = 700;
const float MARGIN   = 40.0f;   // borde para que los puntos no toquen el filo

// ----------------------------------------------------------------------------
//  View: convierte coordenadas del mundo (lon/lat) <-> pixeles de la ventana.
//  Es un mapeo lineal de un rango a otro. En pantalla la Y crece hacia ABAJO
//  pero la latitud crece hacia ARRIBA, por eso la Y se invierte.
//
//  Encima del mapeo base se aplican 'zoom' (escala) y 'pan' (desplazamiento en
//  pixeles), para poder acercar y moverse por el mapa.
// ----------------------------------------------------------------------------
struct View {
    double minX, minY, maxX, maxY;   // limites del mundo (lon/lat del dataset)
    float   zoom = 1.0f;             // factor de acercamiento
    Vector2 pan  = { 0, 0 };         // desplazamiento en pixeles

    Vector2 worldToScreen(double x, double y) const {
        // 1) mapeo base lon/lat -> pixeles ; 2) zoom ; 3) pan
        float bx = MARGIN + (float)((x - minX) / (maxX - minX)) * (SCREEN_W - 2 * MARGIN);
        float by = MARGIN + (float)((maxY - y) / (maxY - minY)) * (SCREEN_H - 2 * MARGIN);
        return { bx * zoom + pan.x, by * zoom + pan.y };
    }
    void screenToWorld(float sx, float sy, double& x, double& y) const {
        // inverso: quitar pan y zoom, luego deshacer el mapeo base
        float bx = (sx - pan.x) / zoom;
        float by = (sy - pan.y) / zoom;
        x = minX + ((bx - MARGIN) / (SCREEN_W - 2 * MARGIN)) * (maxX - minX);
        y = maxY - ((by - MARGIN) / (SCREEN_H - 2 * MARGIN)) * (maxY - minY);
    }
};

// Rueda del mouse = acercar/alejar hacia el cursor (mantiene fijo el punto bajo el).
inline void aplicarZoom(View& v) {
    float wheel = GetMouseWheelMove();
    if (wheel == 0) return;
    Vector2 m = GetMousePosition();
    double wx, wy;
    v.screenToWorld(m.x, m.y, wx, wy);     // punto del mundo bajo el cursor (antes)
    v.zoom *= (wheel > 0 ? 1.1f : 0.9f);
    if (v.zoom < 0.1f) v.zoom = 0.1f;
    Vector2 ahora = v.worldToScreen(wx, wy); // donde quedo ese punto (despues)
    v.pan.x += m.x - ahora.x;                // corregir el pan para que no se mueva
    v.pan.y += m.y - ahora.y;
}

// Arrastrar con clic DERECHO = desplazar el mapa.
inline void aplicarPan(View& v) {
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 d = GetMouseDelta();
        v.pan.x += d.x;
        v.pan.y += d.y;
    }
}

// Calcula los limites (lon/lat) que abarca el dataset.
inline View makeView(const std::vector<SpatialObject>& objs) {
    View v{ 1e18, 1e18, -1e18, -1e18 };
    for (const auto& o : objs) {
        if (o.x < v.minX) v.minX = o.x;
        if (o.x > v.maxX) v.maxX = o.x;
        if (o.y < v.minY) v.minY = o.y;
        if (o.y > v.maxY) v.maxY = o.y;
    }
    return v;
}

// Dibuja un MBR (rectangulo del mundo) como rectangulo en pantalla.
inline void drawMBR(const View& v, const MBR& m, Color c, float thick) {
    Vector2 a = v.worldToScreen(m.minX, m.maxY); // esquina superior-izquierda
    Vector2 b = v.worldToScreen(m.maxX, m.minY); // esquina inferior-derecha
    Rectangle r{ a.x, a.y, b.x - a.x, b.y - a.y };
    DrawRectangleLinesEx(r, thick, c);
}

// ----------------------------------------------------------------------------
//  Dibuja todos los puntos del dataset (un pixel por punto). Con ~25k puntos
//  esto es barato de hacer cada frame, y al pasar por worldToScreen respeta el
//  zoom y el pan automaticamente.
// ----------------------------------------------------------------------------
inline void drawPoints(const View& v, const std::vector<SpatialObject>& objs) {
    for (const auto& o : objs) {
        Vector2 s = v.worldToScreen(o.x, o.y);
        DrawPixelV(s, GRAY);   // un pixel por punto: barato y fluido aunque haya muchos
    }
}

// ----------------------------------------------------------------------------
//  Medicion de tiempo estable: corre 'fn' N veces y devuelve el promedio en ms.
//  Una sola consulta dura microsegundos y el numero salta mucho; promediar da
//  un valor fiable.
// ----------------------------------------------------------------------------
template <typename Fn>
double medirMs(Fn fn, int N) {
    using Clock = std::chrono::high_resolution_clock;
    auto t0 = Clock::now();
    for (int i = 0; i < N; ++i) fn();
    auto t1 = Clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count() / N;
}
