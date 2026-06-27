#pragma once
#include <algorithm>
#include <limits>
#include <cmath>

struct Point {
    double x, y;
};

// minimum bounding rectangle
struct MBR {
    double minX, minY, maxX, maxY;

    // heavy
    static MBR empty() {
        double inf = std::numeric_limits<double>::infinity();   // something easier?
        return { inf, inf, -inf, -inf };
    }

    // literally from a point
    static MBR fromPoint(const Point& p) {
        return { p.x, p.y, p.x, p.y };
    }

    // area
    double area() const {
        double w = maxX - minX, h = maxY - minY;
        if (w < 0 || h < 0) return 0.0;     // just in case of negatives
        return w * h;
    }

    // grows mbr
    void expand(const MBR& o) {
        minX = std::min(minX, o.minX);
        minY = std::min(minY, o.minY);
        maxX = std::max(maxX, o.maxX);
        maxY = std::max(maxY, o.maxY);
    }

    // return a bigger box inluding the two (inside a) 
    static MBR combine(const MBR& a, const MBR& b) {
        MBR r = a;
        r.expand(b);
        return r;
    }

    // gets bigger if it needs to include a point
    double enlargement(const MBR& o) const {
        return combine(*this, o).area() - area();
    }

    // true if point in mbr
    bool contains(const Point& p) const {
        return p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY;
    }

    // will like to know why
    bool intersectsPoint(const Point& p) const { return contains(p); }

    // true is this mbr is in some are of other mbr
    bool intersects(const MBR& o) const {
        return !(o.minX > maxX || o.maxX < minX || o.minY > maxY || o.maxY < minY);
    }

    // min distance of a point to this mbr
    double mindist2(const Point& p) const {
        double dx = 0.0, dy = 0.0;
        if (p.x < minX) dx = minX - p.x; else if (p.x > maxX) dx = p.x - maxX;
        if (p.y < minY) dy = minY - p.y; else if (p.y > maxY) dy = p.y - maxY;
        return dx * dx + dy * dy;
    }
};