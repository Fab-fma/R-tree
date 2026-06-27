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

    // ============================================================
    // 1) DEMO DE CORRECTITUD: R-Tree vs busqueda lineal coinciden.
    // ============================================================
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

    // ============================================================
    // 2) EXPERIMENTOS: 3 tamanos, R-Tree vs lineal.
    // ============================================================
    std::vector<size_t> sizes = { 1000, 5000, 15000 };
    const int NQ = 50; // consultas promediadas por experimento

    std::cout << "=== EXPERIMENTOS (promedio de " << NQ << " consultas) ===\n";
    std::cout << std::left
              << std::setw(8)  << "n"
              << std::setw(12) << "build(ms)"
              << std::setw(13) << "rangeRT(ms)"
              << std::setw(13) << "rangeLin(ms)"
              << std::setw(11) << "knnRT(ms)"
              << std::setw(12) << "knnLin(ms)"
              << std::setw(12) << "nodosRT"
              << std::setw(12) << "revLin" << "\n";
    std::cout << std::string(93, '-') << "\n";

    for (size_t n : sizes) {
        if (n > all.size()) n = all.size();
        std::vector<SpatialObject> sub(all.begin(), all.begin() + n);
        Bounds b = computeBounds(sub);

        auto t0 = Clock::now();
        RTree tree(8);
        for (const auto& o : sub) tree.insert(&o);
        auto t1 = Clock::now();
        double buildMs = ms(t0, t1);

        std::uniform_real_distribution<double> ux(b.minX, b.maxX), uy(b.minY, b.maxY);
        double rangeRT = 0, rangeLin = 0, knnRT = 0, knnLin = 0;
        long long nodosRT = 0, revLin = 0;
        double wHalf = (b.maxX - b.minX) * 0.05, hHalf = (b.maxY - b.minY) * 0.05;

        for (int q = 0; q < NQ; ++q) {
            double cx = ux(rng), cy = uy(rng);
            MBR region{ cx - wHalf, cy - hHalf, cx + wHalf, cy + hHalf };
            long long vis = 0, rev = 0;

            auto a0 = Clock::now(); auto r1 = tree.rangeQuery(region, vis); auto a1 = Clock::now();
            auto r2 = linearRange(sub, region, rev);                       auto a2 = Clock::now();
            rangeRT += ms(a0, a1); rangeLin += ms(a1, a2);
            nodosRT += vis; revLin += rev;

            Point pq{ ux(rng), uy(rng) };
            long long vk = 0, rk = 0;
            auto b0 = Clock::now(); auto k1 = tree.kNN(pq, 10, vk); auto b1 = Clock::now();
            auto k2 = linearKNN(sub, pq, 10, rk);                  auto b2 = Clock::now();
            knnRT += ms(b0, b1); knnLin += ms(b1, b2);
        }

        std::cout << std::left << std::setprecision(4)
                  << std::setw(8)  << n
                  << std::setw(12) << buildMs
                  << std::setw(13) << rangeRT / NQ
                  << std::setw(13) << rangeLin / NQ
                  << std::setw(11) << knnRT / NQ
                  << std::setw(12) << knnLin / NQ
                  << std::setw(12) << nodosRT / NQ
                  << std::setw(12) << revLin / NQ << "\n";
    }
    std::cout << "\nnodosRT = nodos del R-Tree visitados (rango); revLin = elementos revisados por busqueda lineal.\n";
    return 0;
}