#include <iostream>
#include <iomanip>
#include <vector>
#include <set>
#include <random>
#include <chrono>
#include <string>
#include "src/loader.hpp"
#include "src/rtree.hpp"
#include "src/linear_search.hpp"

using Clock = std::chrono::high_resolution_clock;

static double ms(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// Conjunto de ids para comparar resultados sin importar el orden.
static std::set<int> idset(const std::vector<const SpatialObject*>& v) {
    std::set<int> s;
    for (auto* o : v) s.insert(o->id);
    return s;
}

struct Bounds { double minX, minY, maxX, maxY; };
static Bounds computeBounds(const std::vector<SpatialObject>& objs) {
    Bounds b{ 1e18, 1e18, -1e18, -1e18 };
    for (const auto& o : objs) {
        b.minX = std::min(b.minX, o.x); b.maxX = std::max(b.maxX, o.x);
        b.minY = std::min(b.minY, o.y); b.maxY = std::max(b.maxY, o.y);
    }
    return b;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <ruta_geonames.txt>\n";
        return 1;
    }
    std::string path = argv[1];

    std::vector<SpatialObject> all = loadGeoNames(path);
    if (all.empty()) {
        std::cerr << "No se pudieron cargar objetos desde: " << path << "\n";
        return 1;
    }
    std::cout << "Cargados " << all.size() << " objetos espaciales desde " << path << "\n";
    Bounds gb = computeBounds(all);
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Bounding box global: x[" << gb.minX << ", " << gb.maxX
              << "]  y[" << gb.minY << ", " << gb.maxY << "]\n\n";

    std::mt19937 rng(42);

    // 1) correctitud: el R-Tree debe devolver el mismo conjunto que la lineal
    {
        RTree tree(8);
        for (const auto& o : all) tree.insert(&o);

        // Consulta de rango: una ventana central del 10% del ancho/alto.
        double cx = (gb.minX + gb.maxX) / 2, cy = (gb.minY + gb.maxY) / 2;
        double w = (gb.maxX - gb.minX) * 0.05, h = (gb.maxY - gb.minY) * 0.05;
        MBR region{ cx - w, cy - h, cx + w, cy + h };

        long long visited = 0, reviewed = 0;
        auto rt = tree.rangeQuery(region, visited);
        auto lr = linearRange(all, region, reviewed);

        std::cout << "=== DEMO correctitud: consulta por RANGO ===\n";
        std::cout << "Region: x[" << region.minX << ", " << region.maxX
                  << "] y[" << region.minY << ", " << region.maxY << "]\n";
        std::cout << "R-Tree -> " << rt.size() << " resultados, nodos visitados = " << visited << "\n";
        std::cout << "Lineal -> " << lr.size() << " resultados, elementos revisados = " << reviewed << "\n";
        std::cout << "Conjuntos identicos: " << (idset(rt) == idset(lr) ? "SI" : "NO") << "\n";
        std::cout << "Ejemplos: ";
        for (size_t i = 0; i < rt.size() && i < 5; ++i) std::cout << "[" << rt[i]->name << "] ";
        std::cout << "\n\n";

        // Consulta KNN sobre un punto aleatorio del dataset.
        Point q = all[rng() % all.size()].point();
        int k = 5;
        long long vKnn = 0, rKnn = 0;
        auto rk = tree.kNN(q, k, vKnn);
        auto lk = linearKNN(all, q, k, rKnn);

        std::cout << "=== DEMO correctitud: consulta KNN (k=" << k << ") ===\n";
        std::cout << "Punto consulta: (" << q.x << ", " << q.y << ")\n";
        std::cout << "R-Tree -> nodos visitados = " << vKnn << "\n";
        std::cout << "Lineal -> elementos revisados = " << rKnn << "\n";
        std::cout << "Conjuntos identicos: " << (idset(rk) == idset(lk) ? "SI" : "NO") << "\n";
        std::cout << "Vecinos (R-Tree): ";
        for (auto* o : rk) std::cout << "[" << o->name << "] ";
        std::cout << "\n\n";

        // Verificacion de remove().
        int victim = rk.empty() ? all[0].id : rk[0]->id;
        bool removed = tree.remove(victim);
        long long v2 = 0;
        auto rk2 = tree.kNN(q, k, v2);
        bool stillThere = false;
        for (auto* o : rk2) if (o->id == victim) stillThere = true;
        std::cout << "=== DEMO remove() ===\n";
        std::cout << "Eliminado id=" << victim << ": " << (removed ? "OK" : "fallo")
                  << "; sigue apareciendo en KNN: " << (stillThere ? "SI (bug)" : "NO") << "\n\n";
    }

    // 2) benchmark: para 3 tamanos, efecto del tamano del rectangulo (rango) y de k (KNN)
    std::vector<size_t> sizes    = { 1000, 5000, 15000 };
    std::vector<double> rectFrac = { 0.01, 0.05, 0.10 }; // % de la extension
    std::vector<int>    kValues  = { 1, 10, 50 };
    const int NQ = 200; // consultas promediadas por configuracion

    for (size_t n : sizes) {
        if (n > all.size()) n = all.size();
        std::vector<SpatialObject> sub(all.begin(), all.begin() + n);
        Bounds b = computeBounds(sub);
        double extX = b.maxX - b.minX, extY = b.maxY - b.minY;

        // Tiempo de construccion del R-Tree.
        auto t0 = Clock::now();
        RTree tree(8);
        for (const auto& o : sub) tree.insert(&o);
        auto t1 = Clock::now();

        std::cout << "\n========================================================\n";
        std::cout << std::setprecision(3)
                  << " n = " << n << "   |   construccion R-Tree: " << ms(t0, t1)
                  << " ms   |   altura: " << tree.height() << "\n";
        std::cout << "========================================================\n";

        std::uniform_real_distribution<double> ux(b.minX, b.maxX), uy(b.minY, b.maxY);

        // ---- RANGO: efecto del tamano del rectangulo ----
        std::cout << "\n  [RANGO]  efecto del tamano del rectangulo\n";
        std::cout << "  " << std::left
                  << std::setw(8)  << "rect%"   << std::setw(12) << "RT(ms)"
                  << std::setw(12) << "Lin(ms)"  << std::setw(11) << "speedup"
                  << std::setw(12) << "nodosRT"  << std::setw(12) << "revLin"
                  << std::setw(10) << "result"   << "\n";
        for (double f : rectFrac) {
            double wHalf = extX * f / 2, hHalf = extY * f / 2;
            double tRT = 0, tLin = 0; long long nodos = 0, rev = 0, results = 0;
            for (int q = 0; q < NQ; ++q) {
                double cx = ux(rng), cy = uy(rng);
                MBR region{ cx - wHalf, cy - hHalf, cx + wHalf, cy + hHalf };
                long long vis = 0, rv = 0;
                auto a0 = Clock::now(); auto r1 = tree.rangeQuery(region, vis); auto a1 = Clock::now();
                auto r2 = linearRange(sub, region, rv);                         auto a2 = Clock::now();
                tRT += ms(a0, a1); tLin += ms(a1, a2);
                nodos += vis; rev += rv; results += (long long)r1.size();
            }
            double rt = tRT / NQ, lin = tLin / NQ;
            std::cout << "  " << std::left << std::setprecision(4)
                      << std::setw(8)  << (f * 100) << std::setw(12) << rt
                      << std::setw(12) << lin << std::setw(11) << (rt > 0 ? lin / rt : 0)
                      << std::setw(12) << (nodos / NQ) << std::setw(12) << (rev / NQ)
                      << std::setw(10) << (results / NQ) << "\n";
        }

        // ---- KNN: efecto del valor de k ----
        std::cout << "\n  [KNN]  efecto del valor de k\n";
        std::cout << "  " << std::left
                  << std::setw(8)  << "k"       << std::setw(12) << "RT(ms)"
                  << std::setw(12) << "Lin(ms)"  << std::setw(11) << "speedup"
                  << std::setw(12) << "nodosRT"  << std::setw(12) << "revLin" << "\n";
        for (int k : kValues) {
            double tRT = 0, tLin = 0; long long nodos = 0, rev = 0;
            for (int q = 0; q < NQ; ++q) {
                Point p{ ux(rng), uy(rng) };
                long long vis = 0, rv = 0;
                auto a0 = Clock::now(); auto r1 = tree.kNN(p, k, vis); auto a1 = Clock::now();
                auto r2 = linearKNN(sub, p, k, rv);                   auto a2 = Clock::now();
                tRT += ms(a0, a1); tLin += ms(a1, a2);
                nodos += vis; rev += rv;
            }
            double rt = tRT / NQ, lin = tLin / NQ;
            std::cout << "  " << std::left << std::setprecision(4)
                      << std::setw(8)  << k << std::setw(12) << rt
                      << std::setw(12) << lin << std::setw(11) << (rt > 0 ? lin / rt : 0)
                      << std::setw(12) << (nodos / NQ) << std::setw(12) << (rev / NQ) << "\n";
        }
    }

    std::cout << "\n(promedio de " << NQ << " consultas aleatorias por fila)\n";
    std::cout << "nodosRT = nodos del R-Tree visitados; revLin = elementos revisados por la lineal.\n";
    return 0;
}