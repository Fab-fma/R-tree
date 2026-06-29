// ============================================================================
//  visual_knn.cpp - Consulta KNN: k vecinos mas cercanos (R-Tree vs lineal)
//
//  Uso: hacer clic en cualquier punto de la ventana. Se marcan los K vecinos
//  mas cercanos y se muestran los tiempos comparados.
//
//  Compilar: usar knn.bat   (o el comando g++ con raylib).
// ============================================================================
#include "raylib.h"
#include <vector>
#include "src/loader.hpp"
#include "src/rtree.hpp"
#include "src/linear_search.hpp"
#include "src/view.hpp"

const int N_REPS = 100;   // repeticiones para promediar el tiempo
const int K      = 8;     // numero de vecinos

struct Resultado {
    bool hecho = false;
    Point punto{};
    std::vector<const SpatialObject*> vecinos;
    std::vector<MBR> visitadas;
    long long nodosRT = 0, revisadosLin = 0;
    double msRT = 0, msLin = 0;
};

static void drawPanel(const Resultado& r, int total) {
    DrawRectangle(0, 0, 360, 150, Fade(BLACK, 0.65f));
    int y = 8;
    auto line = [&](const char* t) { DrawText(t, 10, y, 16, RAYWHITE); y += 22; };

    if (!r.hecho) {
        line(TextFormat("Haz clic para buscar los %d vecinos", K));
        line(TextFormat("Puntos cargados: %d", total));
        return;
    }
    line(TextFormat("Vecinos (K = %d)", K));
    DrawText(TextFormat("R-Tree: %.5f ms", r.msRT), 10, y, 18, GREEN);  y += 24;
    DrawText(TextFormat("Lineal: %.5f ms", r.msLin), 10, y, 18, ORANGE); y += 24;
    if (r.msRT > 0)
        line(TextFormat("R-Tree es %.1fx mas rapido", r.msLin / r.msRT));
    line(TextFormat("Nodos R-Tree: %lld   vs   revisados: %lld",
                    r.nodosRT, r.revisadosLin));
}

int main(int argc, char** argv) {
    if (argc < 2) { TraceLog(LOG_ERROR, "Uso: visual_knn.exe <dataset.txt>"); return 1; }
    std::vector<SpatialObject> objs = loadGeoNames(argv[1]);
    if (objs.empty()) { TraceLog(LOG_ERROR, "No se cargaron objetos."); return 1; }

    RTree tree(8);
    for (const auto& o : objs) tree.insert(&o);
    View view = makeView(objs);

    InitWindow(SCREEN_W, SCREEN_H, "R-Tree - Consulta KNN");
    SetTargetFPS(60);

    Resultado res;
    int mostrados = 0;           // nodos visitados revelados (paso a paso)

    while (!WindowShouldClose()) {
        // ---- Input: zoom (rueda) y pan (clic derecho) ----
        aplicarZoom(view);
        aplicarPan(view);

        // Paso a paso del recorrido: R reinicia, ESPACIO avanza un nodo.
        if (IsKeyPressed(KEY_R)) mostrados = 0;
        if (IsKeyPressed(KEY_SPACE) && mostrados < (int)res.visitadas.size()) ++mostrados;

        // ---- Input: clic izquierdo = punto de consulta ----
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 m = GetMousePosition();
            double wx, wy;
            view.screenToWorld(m.x, m.y, wx, wy);
            res.punto = { wx, wy };

            res.visitadas.clear();
            res.vecinos = tree.kNN(res.punto, K, res.nodosRT, &res.visitadas);
            linearKNN(objs, res.punto, K, res.revisadosLin);

            long long tmp = 0;
            res.msRT  = medirMs([&]{ tree.kNN(res.punto, K, tmp); }, N_REPS);
            res.msLin = medirMs([&]{ linearKNN(objs, res.punto, K, tmp); }, N_REPS);
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
            Vector2 c = view.worldToScreen(res.punto.x, res.punto.y);
            for (auto* o : res.vecinos) {                       // lineas a los vecinos
                Vector2 s = view.worldToScreen(o->x, o->y);
                DrawLineV(c, s, Fade(YELLOW, 0.5f));
                DrawCircleV(s, 4.0f, RED);                      // (4) vecinos
            }
            DrawCircleV(c, 6.0f, YELLOW);                       // (3) punto de consulta
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
