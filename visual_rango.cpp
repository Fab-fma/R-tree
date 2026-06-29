// ============================================================================
//  visual_rango.cpp - Consulta por RANGO rectangular (R-Tree vs busqueda lineal)
//
//  Controles:
//    - Arrastrar clic IZQUIERDO -> definir el rectangulo de consulta
//    - ESPACIO / R              -> revelar paso a paso (cajas + resultados) / reiniciar
//    - I                        -> insertar un punto en el cursor
//    - D                        -> eliminar el punto mas cercano al cursor
//    - Rueda = zoom  |  clic DERECHO arrastrar = mover
//
//  Al insertar/eliminar se vuelve a ejecutar la consulta activa, asi el resultado
//  refleja el cambio. Almacen en deque para que los punteros del arbol no se
//  invaliden (ver nota en visual_editar.cpp).
//
//  Compilar: usar rango.bat
// ============================================================================
#include "raylib.h"
#include <vector>
#include <deque>
#include <cmath>
#include <chrono>
#include "src/loader.hpp"
#include "src/rtree.hpp"
#include "src/linear_search.hpp"
#include "src/view.hpp"

using Clock = std::chrono::high_resolution_clock;
static double us(Clock::time_point a, Clock::time_point b) {  // microsegundos
    return std::chrono::duration<double, std::micro>(b - a).count();
}

const int N_REPS = 100;   // repeticiones para promediar el tiempo de consulta

struct Resultado {
    bool hecho = false;
    MBR region{};
    std::vector<const SpatialObject*> objetos;
    std::vector<MBR> visitadas;
    std::vector<int> pasoResultado;            // paso en que se encontro cada resultado
    long long nodosRT = 0, revisadosLin = 0;
    double msRT = 0, msLin = 0;
    // ultima edicion (insertar/eliminar)
    bool editado = false;
    const char* edicion = "";
    double edRT = 0, edNaive = 0;
};

static void drawPanel(const Resultado& r, int total) {
    DrawRectangle(0, 0, 380, 178, Fade(BLACK, 0.65f));
    int y = 8;
    auto line = [&](const char* t) { DrawText(t, 10, y, 16, RAYWHITE); y += 22; };

    if (!r.hecho) {
        line("Arrastra para consultar | I inserta | D elimina");
        line(TextFormat("Puntos actuales: %d", total));
        if (r.editado)
            line(TextFormat("Ult. edicion: %s  RT %.2fus | naive %.2fus",
                            r.edicion, r.edRT, r.edNaive));
        return;
    }
    line(TextFormat("Resultados encontrados: %d", (int)r.objetos.size()));
    DrawText(TextFormat("R-Tree: %.5f ms", r.msRT), 10, y, 18, GREEN);  y += 24;
    DrawText(TextFormat("Lineal: %.5f ms", r.msLin), 10, y, 18, ORANGE); y += 24;
    if (r.msRT > 0)
        line(TextFormat("R-Tree es %.1fx mas rapido", r.msLin / r.msRT));
    line(TextFormat("Nodos R-Tree: %lld  vs  revisados: %lld", r.nodosRT, r.revisadosLin));
    if (r.editado)
        line(TextFormat("Ult. edicion: %s  RT %.2fus | naive %.2fus",
                        r.edicion, r.edRT, r.edNaive));
}

int main(int argc, char** argv) {
    if (argc < 2) { TraceLog(LOG_ERROR, "Uso: visual_rango.exe <dataset.txt>"); return 1; }
    std::vector<SpatialObject> cargados = loadGeoNames(argv[1]);
    if (cargados.empty()) { TraceLog(LOG_ERROR, "No se cargaron objetos."); return 1; }

    // Almacen estable (deque) + conjunto vivo (punteros) sobre el que opera la lineal.
    std::deque<SpatialObject> almacen(cargados.begin(), cargados.end());
    std::vector<const SpatialObject*> vivos;
    for (const auto& o : almacen) vivos.push_back(&o);

    RTree tree(8);
    for (auto* p : vivos) tree.insert(p);

    View view = makeView(cargados);
    int nextId = (int)almacen.size();
    Resultado res;
    int mostrados = 0;

    // Ejecuta la consulta de rango sobre el conjunto vivo actual y llena 'res'.
    auto correr = [&](const MBR& region) {
        res.region = region;
        res.visitadas.clear();
        res.pasoResultado.clear();
        res.objetos = tree.rangeQuery(region, res.nodosRT, &res.visitadas, &res.pasoResultado);
        linearRange(vivos, region, res.revisadosLin);
        long long tmp = 0;
        res.msRT  = medirMs([&]{ tree.rangeQuery(region, tmp); }, N_REPS);
        res.msLin = medirMs([&]{ linearRange(vivos, region, tmp); }, N_REPS);
        res.hecho = true;
        mostrados = (int)res.visitadas.size();
    };

    InitWindow(SCREEN_W, SCREEN_H, "R-Tree - Consulta por RANGO");
    SetTargetFPS(60);

    bool arrastrando = false;
    Vector2 inicio{};

    while (!WindowShouldClose()) {
        aplicarZoom(view);
        aplicarPan(view);

        // Paso a paso: R reinicia, ESPACIO avanza (revela caja + sus resultados).
        if (IsKeyPressed(KEY_R)) mostrados = 0;
        if (IsKeyPressed(KEY_SPACE) && mostrados < (int)res.visitadas.size()) ++mostrados;

        // Insertar en el cursor.
        if (IsKeyPressed(KEY_I)) {
            Vector2 m = GetMousePosition();
            double wx, wy; view.screenToWorld(m.x, m.y, wx, wy);
            almacen.push_back(SpatialObject{ nextId++, "nuevo", wx, wy, "" });
            const SpatialObject* p = &almacen.back();
            auto t0 = Clock::now(); tree.insert(p);     auto t1 = Clock::now();
            auto t2 = Clock::now(); vivos.push_back(p);  auto t3 = Clock::now();
            res.editado = true; res.edicion = "INSERTAR";
            res.edRT = us(t0, t1); res.edNaive = us(t2, t3);
            if (res.hecho) correr(res.region);   // re-ejecutar para reflejar el cambio
        }

        // Eliminar el mas cercano al cursor.
        if (IsKeyPressed(KEY_D) && !vivos.empty()) {
            Vector2 m = GetMousePosition();
            double wx, wy; view.screenToWorld(m.x, m.y, wx, wy);
            long long tmp = 0;
            auto cerca = tree.kNN(Point{ wx, wy }, 1, tmp);
            if (!cerca.empty()) {
                int id = cerca[0]->id;
                auto t0 = Clock::now(); tree.remove(id); auto t1 = Clock::now();
                auto t2 = Clock::now();                              // naive: buscar + borrar
                for (size_t i = 0; i < vivos.size(); ++i)
                    if (vivos[i]->id == id) { vivos.erase(vivos.begin() + i); break; }
                auto t3 = Clock::now();
                res.editado = true; res.edicion = "ELIMINAR";
                res.edRT = us(t0, t1); res.edNaive = us(t2, t3);
                if (res.hecho) correr(res.region);
            }
        }

        // Definir el rectangulo de consulta arrastrando.
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { arrastrando = true; inicio = GetMousePosition(); }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && arrastrando) {
            arrastrando = false;
            Vector2 fin = GetMousePosition();
            double x1, y1, x2, y2;
            view.screenToWorld(inicio.x, inicio.y, x1, y1);
            view.screenToWorld(fin.x, fin.y, x2, y2);
            correr({ std::min(x1, x2), std::min(y1, y2), std::max(x1, x2), std::max(y1, y2) });
        }

        // ---- Dibujo ----
        BeginDrawing();
        ClearBackground((Color){ 20, 20, 30, 255 });
        drawPoints(view, vivos);                                // (1) puntos

        for (int i = 0; i < mostrados; ++i) {                   // (6) regiones visitadas
            bool ultimo = (i == mostrados - 1);
            drawMBR(view, res.visitadas[i],
                    ultimo ? ORANGE : Fade(ORANGE, 0.5f), ultimo ? 2.5f : 1.5f);
        }

        if (res.hecho) {
            drawMBR(view, res.region, YELLOW, 2.0f);            // (2) region consultada
            for (size_t i = 0; i < res.objetos.size(); ++i)     // (4) resultados por paso
                if (res.pasoResultado[i] <= mostrados) {
                    Vector2 s = view.worldToScreen(res.objetos[i]->x, res.objetos[i]->y);
                    DrawCircleV(s, 3.0f, RED);
                }
        }

        if (arrastrando) {                                      // rectangulo "en vivo"
            Vector2 m = GetMousePosition();
            Rectangle r{ std::min(inicio.x, m.x), std::min(inicio.y, m.y),
                         (float)fabs(m.x - inicio.x), (float)fabs(m.y - inicio.y) };
            DrawRectangleLinesEx(r, 1.5f, Fade(YELLOW, 0.7f));
        }

        drawPanel(res, (int)vivos.size());                      // (7) tiempos
        if (res.hecho)
            DrawText(TextFormat("Paso %d / %d  (ESPACIO avanza, R reinicia)",
                                mostrados, (int)res.visitadas.size()),
                     10, SCREEN_H - 24, 16, RAYWHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
