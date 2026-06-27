#pragma once
#include <string>
#include "geometry.hpp"

// geonames interest point
struct SpatialObject {
    int id;
    std::string name;
    double x, y;          // x = longitud, y = latitud
    std::string category; // code de GeoNames

    Point point() const { return { x, y }; }
};
