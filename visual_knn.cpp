// consulta KNN. clic = punto de consulta. ESPACIO/R = paso a paso. I/D = insertar/eliminar.
// rueda = zoom, clic derecho = mover. compilar: knn.bat
#include "raylib.h"
#include <vector>
#include <deque>
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
const int K      = 8;     // numero de vecinos

struct Resultado {
    bool hecho = false;
    Point punto{};
    std::vector<const SpatialObject*> vecinos;
    std::vector<MBR> visitadas;
    std::vector<int> pasoResultado;            // paso en que se confirmo cada vecino
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
        line(TextFormat("Clic = %d vecinos | I inserta | D elimina", K));
        line(TextFormat("Puntos actuales: %d", total));
        if (r.editado)
            line(TextFormat("Ult. edicion: %s  RT %.2fus | naive %.2fus",
                            r.edicion, r.edRT, r.edNaive));
        return;
    }
    line(TextFormat("Vecinos (K = %d)", K));
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
    if (argc < 2) { TraceLog(LOG_ERROR, "Uso: visual_knn.exe <dataset.txt>"); return 1; }
    std::vector<SpatialObject> cargados = loadGeoNames(argv[1]);
    if (cargados.empty()) { TraceLog(LOG_ERROR, "No se cargaron objetos."); return 1; }

    // deque = almacen estable (insertar no mueve direcciones, los punteros del arbol
    // siguen validos). 'vivos' = punteros vivos, lo que se dibuja y corre la lineal.
    std::deque<SpatialObject> almacen(cargados.begin(), cargados.end());
    std::vector<const SpatialObject*> vivos;
    for (const auto& o : almacen) vivos.push_back(&o);

    RTree tree(8);
    for (auto* p : vivos) tree.insert(p);

    View view = makeView(cargados);
    int nextId = (int)almacen.size();
    Resultado res;
    int mostrados = 0;

    // Ejecuta la consulta KNN sobre el conjunto vivo actual y llena 'res'.
    auto correr = [&](Point p) {
        res.punto = p;
        res.visitadas.clear();
        res.pasoResultado.clear();
        res.vecinos = tree.kNN(p, K, res.nodosRT, &res.visitadas, &res.pasoResultado);
        linearKNN(vivos, p, K, res.revisadosLin);
        long long tmp = 0;
        res.msRT  = medirMs([&]{ tree.kNN(p, K, tmp); }, N_REPS);
        res.msLin = medirMs([&]{ linearKNN(vivos, p, K, tmp); }, N_REPS);
        res.hecho = true;
        mostrados = (int)res.visitadas.size();
    };

    InitWindow(SCREEN_W, SCREEN_H, "R-Tree - Consulta KNN");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        aplicarZoom(view);
        aplicarPan(view);

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
            if (res.hecho) correr(res.punto);
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
                auto t2 = Clock::now();
                for (size_t i = 0; i < vivos.size(); ++i)
                    if (vivos[i]->id == id) { vivos.erase(vivos.begin() + i); break; }
                auto t3 = Clock::now();
                res.editado = true; res.edicion = "ELIMINAR";
                res.edRT = us(t0, t1); res.edNaive = us(t2, t3);
                if (res.hecho) correr(res.punto);
            }
        }

        // Fijar el punto de consulta con clic izquierdo.
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 m = GetMousePosition();
            double wx, wy; view.screenToWorld(m.x, m.y, wx, wy);
            correr(Point{ wx, wy });
        }

        // ---- Dibujo ----
        BeginDrawing();
        ClearBackground((Color){ 20, 20, 30, 255 });
        drawPoints(view, vivos);

        // cajas recorridas (naranja); el ultimo paso resaltado
        for (int i = 0; i < mostrados; ++i) {
            bool ultimo = (i == mostrados - 1);
            drawMBR(view, res.visitadas[i],
                    ultimo ? ORANGE : Fade(ORANGE, 0.5f), ultimo ? 2.5f : 1.5f);
        }

        if (res.hecho) {
            Vector2 c = view.worldToScreen(res.punto.x, res.punto.y);
            // vecinos, solo los ya revelados por el paso a paso
            for (size_t i = 0; i < res.vecinos.size(); ++i)
                if (res.pasoResultado[i] <= mostrados) {
                    Vector2 s = view.worldToScreen(res.vecinos[i]->x, res.vecinos[i]->y);
                    DrawLineV(c, s, Fade(YELLOW, 0.5f));
                    DrawCircleV(s, 4.0f, RED);
                }
            DrawCircleV(c, 6.0f, YELLOW);                       // punto de consulta
        }

        drawPanel(res, (int)vivos.size());
        if (res.hecho)
            DrawText(TextFormat("Paso %d / %d  (ESPACIO avanza, R reinicia)",
                                mostrados, (int)res.visitadas.size()),
                     10, SCREEN_H - 24, 16, RAYWHITE);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
