// ============================================================================
//  benchmark.cpp - Metricas para el informe (R-Tree vs busqueda lineal)
//
//  Solo consola, sin raylib. Cubre los "experimentos requeridos" del PDF:
//  para 3 tamanos (1k / 5k / 15k) reporta tiempo de construccion, tiempo de
//  consulta (rango y KNN), nodos visitados y elementos revisados, y el efecto
//  del tamano del rectangulo y del valor de k.
//
//  Compilar: g++ -O2 -std=c++17 -static benchmark.cpp -o benchmark.exe
//  Ejecutar: ./benchmark.exe data/cities15000.txt
// ============================================================================
#include <iostream>
#include <iomanip>
#include <vector>
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

struct Bounds { double minX, minY, maxX, maxY; };
static Bounds computeBounds(const std::vector<SpatialObject>& objs) {
    Bounds b{ 1e18, 1e18, -1e18, -1e18 };
    for (const auto& o : objs) {
        if (o.x < b.minX) b.minX = o.x;
        if (o.x > b.maxX) b.maxX = o.x;
        if (o.y < b.minY) b.minY = o.y;
        if (o.y > b.maxY) b.maxY = o.y;
    }
    return b;
}

const int NQ = 200;   // consultas promediadas por configuracion

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <dataset.txt>\n";
        return 1;
    }
    std::vector<SpatialObject> all = loadGeoNames(argv[1]);
    if (all.empty()) {
        std::cerr << "No se pudieron cargar objetos desde: " << argv[1] << "\n";
        return 1;
    }
    std::cout << "Dataset: " << argv[1] << " (" << all.size() << " objetos)\n";
    std::cout << std::fixed;

    std::vector<size_t>      sizes    = { 1000, 5000, 15000 };
    std::vector<double>      rectFrac = { 0.01, 0.05, 0.10 }; // % de la extension
    std::vector<int>         kValues  = { 1, 10, 50 };
    std::mt19937 rng(123);

    for (size_t n : sizes) {
        if (n > all.size()) n = all.size();
        std::vector<SpatialObject> sub(all.begin(), all.begin() + n);
        Bounds b = computeBounds(sub);
        double extX = b.maxX - b.minX, extY = b.maxY - b.minY;

        // ---- Tiempo de construccion ----
        auto t0 = Clock::now();
        RTree tree(8);
        for (const auto& o : sub) tree.insert(&o);
        auto t1 = Clock::now();

        std::cout << "\n========================================================\n";
        std::cout << " n = " << n << "   |   construccion R-Tree: "
                  << std::setprecision(3) << ms(t0, t1) << " ms   |   altura: "
                  << tree.height() << "\n";
        std::cout << "========================================================\n";

        std::uniform_real_distribution<double> ux(b.minX, b.maxX), uy(b.minY, b.maxY);

        // ---- Consulta por RANGO: efecto del tamano del rectangulo ----
        std::cout << "\n  [RANGO]  efecto del tamano del rectangulo\n";
        std::cout << "  " << std::left
                  << std::setw(8)  << "rect%"
                  << std::setw(12) << "RT(ms)"
                  << std::setw(12) << "Lineal(ms)"
                  << std::setw(11) << "speedup"
                  << std::setw(12) << "nodosRT"
                  << std::setw(12) << "revLin"
                  << std::setw(10) << "result" << "\n";

        for (double f : rectFrac) {
            double wHalf = extX * f / 2, hHalf = extY * f / 2;
            double tRT = 0, tLin = 0;
            long long nodos = 0, rev = 0, results = 0;

            for (int q = 0; q < NQ; ++q) {
                double cx = ux(rng), cy = uy(rng);
                MBR region{ cx - wHalf, cy - hHalf, cx + wHalf, cy + hHalf };
                long long vis = 0, rv = 0;

                auto a0 = Clock::now();
                auto r1 = tree.rangeQuery(region, vis);
                auto a1 = Clock::now();
                auto r2 = linearRange(sub, region, rv);
                auto a2 = Clock::now();

                tRT += ms(a0, a1); tLin += ms(a1, a2);
                nodos += vis; rev += rv; results += (long long)r1.size();
            }
            double rt = tRT / NQ, lin = tLin / NQ;
            std::cout << "  " << std::left << std::setprecision(4)
                      << std::setw(8)  << (f * 100)
                      << std::setw(12) << rt
                      << std::setw(12) << lin
                      << std::setw(11) << (rt > 0 ? lin / rt : 0)
                      << std::setw(12) << (nodos / NQ)
                      << std::setw(12) << (rev / NQ)
                      << std::setw(10) << (results / NQ) << "\n";
        }

        // ---- Consulta KNN: efecto del valor de k ----
        std::cout << "\n  [KNN]  efecto del valor de k\n";
        std::cout << "  " << std::left
                  << std::setw(8)  << "k"
                  << std::setw(12) << "RT(ms)"
                  << std::setw(12) << "Lineal(ms)"
                  << std::setw(11) << "speedup"
                  << std::setw(12) << "nodosRT"
                  << std::setw(12) << "revLin" << "\n";

        for (int k : kValues) {
            double tRT = 0, tLin = 0;
            long long nodos = 0, rev = 0;

            for (int q = 0; q < NQ; ++q) {
                Point p{ ux(rng), uy(rng) };
                long long vis = 0, rv = 0;

                auto a0 = Clock::now();
                auto r1 = tree.kNN(p, k, vis);
                auto a1 = Clock::now();
                auto r2 = linearKNN(sub, p, k, rv);
                auto a2 = Clock::now();

                tRT += ms(a0, a1); tLin += ms(a1, a2);
                nodos += vis; rev += rv;
            }
            double rt = tRT / NQ, lin = tLin / NQ;
            std::cout << "  " << std::left << std::setprecision(4)
                      << std::setw(8)  << k
                      << std::setw(12) << rt
                      << std::setw(12) << lin
                      << std::setw(11) << (rt > 0 ? lin / rt : 0)
                      << std::setw(12) << (nodos / NQ)
                      << std::setw(12) << (rev / NQ) << "\n";
        }
    }

    std::cout << "\n(promedio de " << NQ << " consultas aleatorias por fila)\n";
    std::cout << "nodosRT = nodos del R-Tree visitados; revLin = elementos revisados por la lineal.\n";
    return 0;
}
