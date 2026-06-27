#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "spatial_object.hpp"

// 1=name, 4=latitude, 5=longitude, 7=feature code.
// indices desde 0 para tener indices pequenos
inline std::vector<SpatialObject> loadGeoNames(const std::string& path, int limit = -1) {
    std::vector<SpatialObject> out;
    std::ifstream in(path);
    if (!in) return out;

    std::string line;
    int nextId = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;     // si no hay nada pasar a la siguiente

        std::vector<std::string> col;
        std::string field;
        std::stringstream ss(line);
        while (std::getline(ss, field, '\t')) col.push_back(field);     // poner en col los datos separados por \t
        if (col.size() < 8) continue;   // si no esta completo no creamos objeto

        try {
            SpatialObject o;
            o.id = nextId++;            // id desde 0
            o.name = col[1];            
            o.y = std::stod(col[4]);    // latitude
            o.x = std::stod(col[5]);    // longitude
            o.category = col[7];
            out.push_back(std::move(o));
        } catch (...) {
            // Linea malformada: se ignora.
            continue;
        }
        if (limit > 0 && (int)out.size() >= limit) break;           // cantidad especifica de elementos
    }
    return out;
}