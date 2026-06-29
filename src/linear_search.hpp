#pragma once
#include <vector>
#include <algorithm>
#include "spatial_object.hpp"

// solucion ingenua: revisa los n objetos uno por uno (O(n)). es la base de comparacion:
// el R-Tree gana porque no los revisa todos. 'reviewed' = cuantos miro (metrica).

inline std::vector<const SpatialObject*> linearRange(const std::vector<SpatialObject>& objs, const MBR& region, long long& reviewed) {
    std::vector<const SpatialObject*> res;
    reviewed = 0;                   // contar numero de comprobaciones
    for (const auto& o : objs) {
        ++reviewed;
        if (region.contains(o.point())) res.push_back(&o);      // push if o in region
    }
    return res;
}

inline std::vector<const SpatialObject*> linearKNN(const std::vector<SpatialObject>& objs, const Point& q, int k, long long& reviewed) {
    reviewed = 0;
    std::vector<std::pair<double, const SpatialObject*>> d;     // distance, point
    d.reserve(objs.size());
    for (const auto& o : objs) {
        ++reviewed;
        double dx = o.x - q.x, dy = o.y - q.y;                  // distance current point to q
        d.push_back({ dx * dx + dy * dy, &o });
    }
    // review knn
    if ((int)d.size() > k)
        std::partial_sort(d.begin(), d.begin() + k, d.end(),
                          [](auto& a, auto& b) { return a.first < b.first; });
    else
        std::sort(d.begin(), d.end(), [](auto& a, auto& b) { return a.first < b.first; });

    std::vector<const SpatialObject*> res;
    for (int i = 0; i < k && i < (int)d.size(); ++i) res.push_back(d[i].second);
    return res;
}

// --- Overloads que operan sobre un conjunto vivo de punteros (para insertar/eliminar) ---

inline std::vector<const SpatialObject*> linearRange(const std::vector<const SpatialObject*>& objs, const MBR& region, long long& reviewed) {
    std::vector<const SpatialObject*> res;
    reviewed = 0;
    for (auto* o : objs) {
        ++reviewed;
        if (region.contains(o->point())) res.push_back(o);
    }
    return res;
}

inline std::vector<const SpatialObject*> linearKNN(const std::vector<const SpatialObject*>& objs, const Point& q, int k, long long& reviewed) {
    reviewed = 0;
    std::vector<std::pair<double, const SpatialObject*>> d;
    d.reserve(objs.size());
    for (auto* o : objs) {
        ++reviewed;
        double dx = o->x - q.x, dy = o->y - q.y;
        d.push_back({ dx * dx + dy * dy, o });
    }
    if ((int)d.size() > k)
        std::partial_sort(d.begin(), d.begin() + k, d.end(),
                          [](auto& a, auto& b) { return a.first < b.first; });
    else
        std::sort(d.begin(), d.end(), [](auto& a, auto& b) { return a.first < b.first; });

    std::vector<const SpatialObject*> res;
    for (int i = 0; i < k && i < (int)d.size(); ++i) res.push_back(d[i].second);
    return res;
}