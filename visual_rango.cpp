// ============================================================================
//  visual_rango.cpp - Consulta por RANGO rectangular (R-Tree vs busqueda lineal)
//
//  Uso: arrastrar con el mouse para dibujar un rectangulo. Al soltar, se ejecuta
//  la consulta y se muestran los resultados y los tiempos comparados.
//
//  Compilar: usar rango.bat   (o el comando g++ con raylib).
// ============================================================================
#include "raylib.h"
#include <vector>
#include <cmath>
#include "src/loader.hpp"
#include "src/rtree.hpp"
#include "src/linear_search.hpp"
#include "src/view.hpp"

const int N_REPS = 100;   // repeticiones para promediar el tiempo

// Lo que hay que dibujar tras una consulta.
struct Resultado {
    bool hecho = false;
    MBR region{};
    std::vector<const SpatialObject*> objetos;
    std::vector<MBR> visitadas;
    long long nodosRT = 0, revisadosLin = 0;
    double msRT = 0, msLin = 0;
};

// Panel de texto con enfasis en los TIEMPOS.
static void drawPanel(const Resultado& r, int total) {
    DrawRectangle(0, 0, 360, 150, Fade(BLACK, 0.65f));
    int y = 8;
    auto line = [&](const char* t) { DrawText(t, 10, y, 16, RAYWHITE); y += 22; };

    if (!r.hecho) {
        line("Arrastra con el mouse para consultar un RANGO");
        line(TextFormat("Puntos cargados: %d", total));
        return;
    }
    line(TextFormat("Resultados encontrados: %d", (int)r.objetos.size()));
    DrawText(TextFormat("R-Tree: %.5f ms", r.msRT), 10, y, 18, GREEN);  y += 24;
    DrawText(TextFormat("Lineal: %.5f ms", r.msLin), 10, y, 18, ORANGE); y += 24;
    if (r.msRT > 0)
        line(TextFormat("R-Tree es %.1fx mas rapido", r.msLin / r.msRT));
    line(TextFormat("Nodos R-Tree: %lld   vs   revisados: %lld",
                    r.nodosRT, r.revisadosLin));
}

int main(int argc, char** argv) {
    if (argc < 2) { TraceLog(LOG_ERROR, "Uso: visual_rango.exe <dataset.txt>"); return 1; }
    std::vector<SpatialObject> objs = loadGeoNames(argv[1]);
    if (objs.empty()) { TraceLog(LOG_ERROR, "No se cargaron objetos."); return 1; }

    RTree tree(8);
    for (const auto& o : objs) tree.insert(&o);
    View view = makeView(objs);

    InitWindow(SCREEN_W, SCREEN_H, "R-Tree - Consulta por RANGO");
    SetTargetFPS(60);

    Resultado res;
    int  mostrados = 0;          // nodos visitados revelados (paso a paso)
    bool arrastrando = false;
    Vector2 inicio{};

    while (!WindowShouldClose()) {
        // ---- Input: zoom (rueda) y pan (clic derecho) ----
        aplicarZoom(view);
        aplicarPan(view);

        // Paso a paso del recorrido: R reinicia, ESPACIO avanza un nodo.
        if (IsKeyPressed(KEY_R)) mostrados = 0;
        if (IsKeyPressed(KEY_SPACE) && mostrados < (int)res.visitadas.size()) ++mostrados;

        // ---- Input: arrastrar (clic izq) para definir el rectangulo ----
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            arrastrando = true;
            inicio = GetMousePosition();
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && arrastrando) {
            arrastrando = false;
            Vector2 fin = GetMousePosition();
            double x1, y1, x2, y2;
            view.screenToWorld(inicio.x, inicio.y, x1, y1);
            view.screenToWorld(fin.x, fin.y, x2, y2);
            res.region = { std::min(x1, x2), std::min(y1, y2),
                           std::max(x1, x2), std::max(y1, y2) };

            // Una corrida para obtener resultados y metricas (nodos/revisados).
            res.visitadas.clear();
            res.objetos = tree.rangeQuery(res.region, res.nodosRT, &res.visitadas);
            linearRange(objs, res.region, res.revisadosLin);

            // Tiempos: promedio de N_REPS corridas.
            long long tmp = 0;
            res.msRT  = medirMs([&]{ tree.rangeQuery(res.region, tmp); }, N_REPS);
            res.msLin = medirMs([&]{ linearRange(objs, res.region, tmp); }, N_REPS);
            res.hecho = true;
            mostrados = (int)res.visitadas.size();   // mostrar todo el recorrido
        }

        // ---- Dibujo ----
        BeginDrawing();
        ClearBackground((Color){ 20, 20, 30, 255 });
        drawPoints(view, objs);                                 // (1) puntos

        for (int i = 0; i < mostrados; ++i) {                   // (6) regiones visitadas
            bool ultimo = (i == mostrados - 1);
            drawMBR(view, res.visitadas[i],
                    ultimo ? ORANGE : Fade(ORANGE, 0.5f), ultimo ? 2.5f : 1.5f);
        }

        if (res.hecho) {
            drawMBR(view, res.region, YELLOW, 2.0f);            // (2) region consultada
            for (auto* o : res.objetos) {                       // (4) resultados
                Vector2 s = view.worldToScreen(o->x, o->y);
                DrawCircleV(s, 3.0f, RED);
            }
        }

        // Rectangulo "en vivo" mientras se arrastra.
        if (arrastrando) {
            Vector2 m = GetMousePosition();
            Rectangle r{ std::min(inicio.x, m.x), std::min(inicio.y, m.y),
                         (float)fabs(m.x - inicio.x), (float)fabs(m.y - inicio.y) };
            DrawRectangleLinesEx(r, 1.5f, Fade(YELLOW, 0.7f));
        }

        drawPanel(res, (int)objs.size());                       // (7) tiempos
        if (res.hecho)                                          // paso a paso
            DrawText(TextFormat("Paso %d / %d  (ESPACIO avanza, R reinicia)",
                                mostrados, (int)res.visitadas.size()),
                     10, SCREEN_H - 24, 16, RAYWHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
