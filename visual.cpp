// ============================================================================
//  Visualizacion del R-Tree con raylib  (Proyecto 2 AED - Opcion B)
//
//  Muestra lo que pide el PDF:
//    1) los puntos espaciales            5) los MBRs del R-Tree
//    2) la region rectangular consultada 6) los nodos/regiones visitados
//    3) el punto de consulta KNN          7) tiempo R-Tree vs busqueda lineal
//    4) los resultados encontrados
//
//  Controles:
//    - Arrastrar con boton IZQUIERDO  -> consulta por RANGO (rectangulo)
//    - Clic con boton DERECHO         -> consulta KNN en ese punto
//    - Tecla M                        -> mostrar/ocultar los MBRs del arbol
//
//  Compilar (raylib instalado por MSYS2):
//    g++ -O2 -std=c++17 visual.cpp -o visual.exe -lraylib -lopengl32 -lgdi32 -lwinmm
//  Ejecutar:
//    ./visual.exe data/cities1000.txt
// ============================================================================
#include "raylib.h"
#include <vector>
#include <chrono>
#include <string>
#include "src/loader.hpp"
#include "src/rtree.hpp"
#include "src/linear_search.hpp"

using Clock = std::chrono::high_resolution_clock;
static double ms(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

const int   SCREEN_W = 1000;
const int   SCREEN_H = 700;
const float MARGIN   = 40.0f;   // borde para que los puntos no toquen el filo
const int   K        = 5;       // vecinos para KNN

// ----------------------------------------------------------------------------
//  View: convierte coordenadas del mundo (lon/lat) <-> pixeles de la ventana.
//  Es la unica parte "matematica": un mapeo lineal de un rango a otro.
//  Ojo: en pantalla la Y crece hacia ABAJO, pero la latitud crece hacia ARRIBA,
//  por eso la Y se invierte.
// ----------------------------------------------------------------------------
struct View {
    double minX, minY, maxX, maxY;   // limites del mundo (lon/lat del dataset)

    Vector2 worldToScreen(double x, double y) const {
        float sx = MARGIN + (float)((x - minX) / (maxX - minX)) * (SCREEN_W - 2 * MARGIN);
        float sy = MARGIN + (float)((maxY - y) / (maxY - minY)) * (SCREEN_H - 2 * MARGIN);
        return { sx, sy };
    }
    // Inverso: de un pixel de pantalla de vuelta a lon/lat (para construir consultas).
    void screenToWorld(float sx, float sy, double& x, double& y) const {
        x = minX + ((sx - MARGIN) / (SCREEN_W - 2 * MARGIN)) * (maxX - minX);
        y = maxY - ((sy - MARGIN) / (SCREEN_H - 2 * MARGIN)) * (maxY - minY);
    }
};

static View makeView(const std::vector<SpatialObject>& objs) {
    View v{ 1e18, 1e18, -1e18, -1e18 };
    for (const auto& o : objs) {
        if (o.x < v.minX) v.minX = o.x;  if (o.x > v.maxX) v.maxX = o.x;
        if (o.y < v.minY) v.minY = o.y;  if (o.y > v.maxY) v.maxY = o.y;
    }
    return v;
}

// Dibuja un MBR (rectangulo del mundo) como un rectangulo en pantalla.
static void drawMBR(const View& v, const MBR& m, Color c, float thick) {
    Vector2 a = v.worldToScreen(m.minX, m.maxY); // esquina superior-izquierda
    Vector2 b = v.worldToScreen(m.maxX, m.minY); // esquina inferior-derecha
    Rectangle r{ a.x, a.y, b.x - a.x, b.y - a.y };
    DrawRectangleLinesEx(r, thick, c);
}

// ----------------------------------------------------------------------------
//  Estado de la ultima consulta (lo que hay que dibujar y mostrar como metricas).
// ----------------------------------------------------------------------------
struct QueryState {
    enum Mode { NONE, RANGE, KNN } mode = NONE;
    MBR region{};                              // rectangulo consultado (modo RANGE)
    Point knnPoint{};                          // punto consultado (modo KNN)
    std::vector<const SpatialObject*> results; // resultados encontrados
    std::vector<MBR> visited;                  // MBRs de nodos visitados por el R-Tree
    long long nodesRT = 0, reviewedLin = 0;    // metricas de comparacion
    double timeRT = 0, timeLin = 0;            // tiempos en ms
};

// Ejecuta una consulta por rango y guarda resultados + metricas.
static void runRange(const RTree& tree, const std::vector<SpatialObject>& objs,
                     const MBR& region, QueryState& q) {
    q.mode = QueryState::RANGE;
    q.region = region;
    q.visited.clear();

    auto t0 = Clock::now();
    q.results = tree.rangeQuery(region, q.nodesRT, &q.visited);
    auto t1 = Clock::now();
    std::vector<const SpatialObject*> lin = linearRange(objs, region, q.reviewedLin);
    auto t2 = Clock::now();

    q.timeRT  = ms(t0, t1);
    q.timeLin = ms(t1, t2);
}

// Ejecuta una consulta KNN y guarda resultados + metricas.
static void runKNN(const RTree& tree, const std::vector<SpatialObject>& objs,
                   Point p, QueryState& q) {
    q.mode = QueryState::KNN;
    q.knnPoint = p;
    q.visited.clear();

    auto t0 = Clock::now();
    q.results = tree.kNN(p, K, q.nodesRT, &q.visited);
    auto t1 = Clock::now();
    std::vector<const SpatialObject*> lin = linearKNN(objs, p, K, q.reviewedLin);
    auto t2 = Clock::now();

    q.timeRT  = ms(t0, t1);
    q.timeLin = ms(t1, t2);
}

// ----------------------------------------------------------------------------
//  Funciones de dibujo (cada una hace UNA cosa, para que se lea facil).
// ----------------------------------------------------------------------------

// (1) Todos los puntos del dataset, en gris.
static void drawPoints(const View& v, const std::vector<SpatialObject>& objs) {
    for (const auto& o : objs) {
        Vector2 s = v.worldToScreen(o.x, o.y);
        DrawCircleV(s, 1.5f, GRAY);
    }
}

// (5) Los MBRs de todos los nodos del arbol, en lineas tenues.
static void drawTreeMBRs(const View& v, const std::vector<std::pair<int, MBR>>& mbrs) {
    for (const auto& pr : mbrs)
        drawMBR(v, pr.second, Fade(BLUE, 0.25f), 1.0f);
}

// (6) Las regiones (MBRs) que el R-Tree visito durante la consulta, en naranja.
static void drawVisited(const View& v, const std::vector<MBR>& visited) {
    for (const auto& m : visited)
        drawMBR(v, m, Fade(ORANGE, 0.8f), 1.5f);
}

// (2,3,4) La consulta en si y sus resultados.
static void drawQuery(const View& v, const QueryState& q) {
    if (q.mode == QueryState::RANGE) {
        drawMBR(v, q.region, YELLOW, 2.0f);                 // (2) region consultada
    } else if (q.mode == QueryState::KNN) {
        Vector2 c = v.worldToScreen(q.knnPoint.x, q.knnPoint.y);
        DrawCircleV(c, 6.0f, YELLOW);                       // (3) punto de consulta
        for (auto* o : q.results) {                         // lineas a los vecinos
            Vector2 s = v.worldToScreen(o->x, o->y);
            DrawLineV(c, s, Fade(YELLOW, 0.5f));
        }
    }
    for (auto* o : q.results) {                             // (4) resultados en rojo
        Vector2 s = v.worldToScreen(o->x, o->y);
        DrawCircleV(s, 3.0f, RED);
    }
}

// (7) Panel de texto con las metricas de comparacion.
static void drawMetrics(const QueryState& q, int total) {
    DrawRectangle(0, 0, 320, 130, Fade(BLACK, 0.6f));
    int y = 8;
    auto line = [&](const char* t) { DrawText(t, 8, y, 16, RAYWHITE); y += 20; };

    if (q.mode == QueryState::NONE) {
        line("Arrastra IZQ = rango | clic DER = KNN");
        line("Tecla M = ver/ocultar MBRs del arbol");
        line(TextFormat("Puntos cargados: %d", total));
        return;
    }
    line(q.mode == QueryState::RANGE ? "Consulta: RANGO" : "Consulta: KNN");
    line(TextFormat("Resultados: %d", (int)q.results.size()));
    line(TextFormat("R-Tree: %lld nodos | %.4f ms", q.nodesRT, q.timeRT));
    line(TextFormat("Lineal: %lld revisados | %.4f ms", q.reviewedLin, q.timeLin));
    if (q.timeRT > 0)
        line(TextFormat("R-Tree es %.1fx mas rapido", q.timeLin / q.timeRT));
}

// ----------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 2) {
        TraceLog(LOG_ERROR, "Uso: visual.exe <ruta_geonames.txt>");
        return 1;
    }
    std::vector<SpatialObject> objs = loadGeoNames(argv[1]);
    if (objs.empty()) {
        TraceLog(LOG_ERROR, "No se pudieron cargar objetos.");
        return 1;
    }

    // Construir el R-Tree una sola vez.
    RTree tree(8);
    for (const auto& o : objs) tree.insert(&o);
    std::vector<std::pair<int, MBR>> treeMBRs = tree.nodeMBRs();

    View view = makeView(objs);
    QueryState query;
    bool showMBRs = false;

    // Para el arrastre del rectangulo de rango.
    bool dragging = false;
    Vector2 dragStart{};

    InitWindow(SCREEN_W, SCREEN_H, "R-Tree - Buscador espacial");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        // ---------------- INPUT ----------------
        if (IsKeyPressed(KEY_M)) showMBRs = !showMBRs;

        // Arrastre con boton izquierdo = consulta por rango.
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            dragging = true;
            dragStart = GetMousePosition();
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && dragging) {
            dragging = false;
            Vector2 end = GetMousePosition();
            // Convertir las dos esquinas de pantalla a mundo y armar el MBR.
            double x1, y1, x2, y2;
            view.screenToWorld(dragStart.x, dragStart.y, x1, y1);
            view.screenToWorld(end.x, end.y, x2, y2);
            MBR region{ std::min(x1, x2), std::min(y1, y2),
                        std::max(x1, x2), std::max(y1, y2) };
            runRange(tree, objs, region, query);
        }

        // Clic derecho = consulta KNN en ese punto.
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            Vector2 m = GetMousePosition();
            double wx, wy;
            view.screenToWorld(m.x, m.y, wx, wy);
            runKNN(tree, objs, Point{ wx, wy }, query);
        }

        // ---------------- DIBUJO ----------------
        BeginDrawing();
        ClearBackground((Color){ 20, 20, 30, 255 });

        drawPoints(view, objs);                       // (1)
        if (showMBRs) drawTreeMBRs(view, treeMBRs);   // (5)
        drawVisited(view, query.visited);             // (6)
        drawQuery(view, query);                       // (2,3,4)

        // Rectangulo "en vivo" mientras se arrastra.
        if (dragging) {
            Vector2 m = GetMousePosition();
            Rectangle r{ std::min(dragStart.x, m.x), std::min(dragStart.y, m.y),
                         (float)fabs(m.x - dragStart.x), (float)fabs(m.y - dragStart.y) };
            DrawRectangleLinesEx(r, 1.5f, Fade(YELLOW, 0.7f));
        }

        drawMetrics(query, (int)objs.size());         // (7)
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
