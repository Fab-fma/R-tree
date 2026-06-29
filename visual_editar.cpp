// ============================================================================
//  visual_editar.cpp - Insertar y eliminar puntos (R-Tree vs forma naive)
//
//  Compara el TIEMPO de las dos formas:
//    - Insertar:  R-Tree (chooseLeaf + posible split)  vs  vector.push_back
//    - Eliminar:  R-Tree remove(id)                     vs  busqueda lineal + erase
//
//  Controles:
//    - Clic IZQUIERDO        -> seleccionar el punto mas cercano al cursor
//    - Tecla D               -> eliminar el punto seleccionado
//    - Tecla I               -> insertar un punto en la posicion del cursor
//    - Rueda                 -> zoom    |   clic DERECHO arrastrar -> mover
//
//  Compilar: usar editar.bat
//
//  Nota de seguridad: el R-Tree guarda PUNTEROS a los objetos. Por eso el
//  almacen real es un std::deque (sus elementos no cambian de direccion al
//  hacer push_back). El "vector naive" guarda solo punteros: insertarlos y
//  borrarlos no mueve los objetos, asi que los punteros del arbol siguen validos.
// ============================================================================
#include "raylib.h"
#include <vector>
#include <deque>
#include <chrono>
#include "src/loader.hpp"
#include "src/rtree.hpp"
#include "src/view.hpp"

using Clock = std::chrono::high_resolution_clock;
static double us(Clock::time_point a, Clock::time_point b) {  // microsegundos
    return std::chrono::duration<double, std::micro>(b - a).count();
}

// Estado de la ultima operacion (para el panel).
struct Op {
    bool hecho = false;
    const char* nombre = "";
    double tRT = 0, tNaive = 0;  // en microsegundos
};

static void drawPanel(const Op& op, int total) {
    DrawRectangle(0, 0, 380, 130, Fade(BLACK, 0.65f));
    int y = 8;
    auto line = [&](const char* t) { DrawText(t, 10, y, 16, RAYWHITE); y += 22; };

    line("Clic = seleccionar | I = insertar | D = eliminar");
    line(TextFormat("Puntos actuales: %d", total));
    if (op.hecho) {
        line(TextFormat("Ultima operacion: %s", op.nombre));
        DrawText(TextFormat("R-Tree: %.3f us", op.tRT), 10, y, 18, GREEN);  y += 24;
        DrawText(TextFormat("Naive : %.3f us", op.tNaive), 10, y, 18, ORANGE);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) { TraceLog(LOG_ERROR, "Uso: visual_editar.exe <dataset.txt>"); return 1; }
    std::vector<SpatialObject> cargados = loadGeoNames(argv[1]);
    if (cargados.empty()) { TraceLog(LOG_ERROR, "No se cargaron objetos."); return 1; }

    // Almacen estable (direcciones fijas) y estructura naive (solo punteros).
    std::deque<SpatialObject> almacen(cargados.begin(), cargados.end());
    std::vector<const SpatialObject*> naive;
    for (const auto& o : almacen) naive.push_back(&o);

    RTree tree(8);
    for (auto* p : naive) tree.insert(p);

    View view = makeView(cargados);
    int nextId = (int)almacen.size();        // ids nuevos para los insertados
    const SpatialObject* seleccionado = nullptr;
    Op op;

    InitWindow(SCREEN_W, SCREEN_H, "R-Tree - Insertar / Eliminar");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        aplicarZoom(view);
        aplicarPan(view);

        // ---- Seleccionar: clic izquierdo elige el punto mas cercano ----
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !naive.empty()) {
            Vector2 m = GetMousePosition();
            double wx, wy;
            view.screenToWorld(m.x, m.y, wx, wy);
            long long tmp = 0;
            auto vecinos = tree.kNN(Point{ wx, wy }, 1, tmp);
            if (!vecinos.empty()) seleccionado = vecinos[0];
        }

        // ---- Insertar: tecla I, en la posicion del cursor ----
        if (IsKeyPressed(KEY_I)) {
            Vector2 m = GetMousePosition();
            double wx, wy;
            view.screenToWorld(m.x, m.y, wx, wy);

            almacen.push_back(SpatialObject{ nextId++, "nuevo", wx, wy, "" });
            const SpatialObject* p = &almacen.back();

            auto t0 = Clock::now(); tree.insert(p);       auto t1 = Clock::now();
            auto t2 = Clock::now(); naive.push_back(p);    auto t3 = Clock::now();

            op = { true, "INSERTAR", us(t0, t1), us(t2, t3) };
        }

        // ---- Eliminar: tecla D, el punto seleccionado ----
        if (IsKeyPressed(KEY_D) && seleccionado) {
            int id = seleccionado->id;

            // R-Tree: eliminar por id.
            auto t0 = Clock::now(); tree.remove(id); auto t1 = Clock::now();

            // Naive: buscar linealmente el id y borrarlo del vector.
            auto t2 = Clock::now();
            for (size_t i = 0; i < naive.size(); ++i)
                if (naive[i]->id == id) { naive.erase(naive.begin() + i); break; }
            auto t3 = Clock::now();

            op = { true, "ELIMINAR", us(t0, t1), us(t2, t3) };
            seleccionado = nullptr;   // ya no existe
        }

        // ---- Dibujo ----
        BeginDrawing();
        ClearBackground((Color){ 20, 20, 30, 255 });
        for (auto* o : naive) {                          // puntos vivos (desde naive)
            Vector2 s = view.worldToScreen(o->x, o->y);
            DrawPixelV(s, GRAY);
        }
        if (seleccionado) {                              // resaltar el seleccionado
            Vector2 s = view.worldToScreen(seleccionado->x, seleccionado->y);
            DrawCircleV(s, 6.0f, YELLOW);
        }
        drawPanel(op, (int)naive.size());
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
