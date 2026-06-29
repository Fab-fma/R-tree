#pragma once
#include <vector>
#include <queue>
#include <algorithm>
#include "geometry.hpp"
#include "spatial_object.hpp"

// Nodos basados en punteros. Una hoja guarda objetos; un nodo interno guarda hijos.
class RTree {
public:
    struct Node;

    // Entrada de un nodo: su mbr y a que apunta hijo si interno, objeto si hoja
    struct Entry {
        MBR mbr;
        Node* child = nullptr;              // para nodos internos
        const SpatialObject* obj = nullptr; // para hojas
    };

    struct Node {
        bool leaf;
        std::vector<Entry> entries;
        Node* parent = nullptr;
        explicit Node(bool isLeaf) : leaf(isLeaf) {}
    };

    explicit RTree(int maxEntries = 8) : 
        maxEntries_(maxEntries),
        minEntries_(std::max(2, maxEntries / 2)) {
        root_ = new Node(true);
    }

    ~RTree() { destroy(root_); }

    RTree(const RTree&) = delete;
    RTree& operator=(const RTree&) = delete;

    // ---- Insercion ----
    void insert(const SpatialObject* obj) {
        Entry e;
        e.mbr = MBR::fromPoint(obj->point());
        e.obj = obj;

        Node* leaf = chooseLeaf(root_, e.mbr);
        leaf->entries.push_back(e);

        Node* split = nullptr;
        if ((int)leaf->entries.size() > maxEntries_)
            split = quadraticSplit(leaf);

        adjustTree(leaf, split);
    }

    // ---- Eliminacion (por id de objeto) ----
    // Version simple: localiza la hoja, borra la entrada y reinserta huerfanos si
    // el nodo queda por debajo del minimo. Cumple la "funcionalidad minima" del PDF.
    bool remove(int id) {
        Node* leaf = nullptr;
        int idx = -1;
        if (!findLeaf(root_, id, leaf, idx)) return false;

        leaf->entries.erase(leaf->entries.begin() + idx);

        std::vector<const SpatialObject*> orphans;
        condenseTree(leaf, orphans);

        for (auto* o : orphans) insert(o);

        // Si la raiz interna quedo con un solo hijo, baja un nivel.
        if (!root_->leaf && root_->entries.size() == 1) {
            Node* child = root_->entries[0].child;
            child->parent = nullptr;
            delete root_;
            root_ = child;
        }
        return true;
    }

    // ---- Consulta por rango rectangular ----
    // 'visitedNodes' acumula cuantos nodos se examinaron (metrica vs busqueda lineal).
    // 'visitadas' (opcional): si se pasa, guarda el MBR de cada nodo examinado (para dibujar).
    // 'pasoResultado' (opcional): por cada resultado, el nro de paso (nodos visitados) en que
    //   se encontro, para revelarlos en sincronia con las cajas en la visualizacion.
    std::vector<const SpatialObject*> rangeQuery(const MBR& region, long long& visitedNodes,
                                                 std::vector<MBR>* visitadas = nullptr,
                                                 std::vector<int>* pasoResultado = nullptr) const {
        std::vector<const SpatialObject*> res;
        visitedNodes = 0;
        rangeRec(root_, region, res, visitedNodes, visitadas, pasoResultado);
        return res;
    }

    // ---- KNN: k vecinos mas cercanos a un punto ----
    // Recorrido best-first con poda por mindist sobre los MBRs.
    std::vector<const SpatialObject*> kNN(const Point& q, int k, long long& visitedNodes,
                                          std::vector<MBR>* visitadas = nullptr,
                                          std::vector<int>* pasoResultado = nullptr) const {
        visitedNodes = 0;
        // Cola de prioridad por menor distancia: entradas (nodo o objeto).
        struct QItem {
            double dist;
            bool isObj;
            Node* node;
            const SpatialObject* obj;
            bool operator>(const QItem& o) const { return dist > o.dist; }
        };
        std::priority_queue<QItem, std::vector<QItem>, std::greater<QItem>> pq;
        pq.push({ 0.0, false, root_, nullptr });

        std::vector<const SpatialObject*> res;
        while (!pq.empty() && (int)res.size() < k) {
            QItem it = pq.top(); pq.pop();
            if (it.isObj) {
                res.push_back(it.obj);
                if (pasoResultado) pasoResultado->push_back((int)visitedNodes);
                continue;
            }
            ++visitedNodes;
            if (visitadas) visitadas->push_back(nodeMBR(it.node));
            for (const auto& e : it.node->entries) {
                if (it.node->leaf) {
                    double dx = e.obj->x - q.x, dy = e.obj->y - q.y;
                    pq.push({ dx * dx + dy * dy, true, nullptr, e.obj });
                } else {
                    pq.push({ e.mbr.mindist2(q), false, e.child, nullptr });
                }
            }
        }
        return res;
    }

    int height() const { return heightRec(root_); }

    // ---- Para visualizacion: MBR de cada nodo con su nivel (0 = raiz) ----
    std::vector<std::pair<int, MBR>> nodeMBRs() const {
        std::vector<std::pair<int, MBR>> out;
        collectNodeMBRs(root_, 0, out);
        return out;
    }

private:
    Node* root_;
    int maxEntries_;
    int minEntries_;

    // --- MBR de un nodo: union de las MBRs de sus entradas ---
    static MBR nodeMBR(const Node* n) {
        MBR m = MBR::empty();
        for (const auto& e : n->entries) m.expand(e.mbr);
        return m;
    }

    // --- chooseLeaf: descender por menor enlargement (desempate por menor area) ---
    Node* chooseLeaf(Node* n, const MBR& m) {
        while (!n->leaf) {
            int best = 0;
            double bestEnl = n->entries[0].mbr.enlargement(m);
            double bestArea = n->entries[0].mbr.area();
            for (int i = 1; i < (int)n->entries.size(); ++i) {
                double enl = n->entries[i].mbr.enlargement(m);
                double ar = n->entries[i].mbr.area();
                if (enl < bestEnl || (enl == bestEnl && ar < bestArea)) {
                    best = i; bestEnl = enl; bestArea = ar;
                }
            }
            n = n->entries[best].child;
        }
        return n;
    }

    // --- adjustTree: propaga MBRs y splits hacia la raiz ---
    void adjustTree(Node* n, Node* split) {
        while (n != root_) {
            Node* parent = n->parent;
            // Actualizar la MBR de la entrada que apunta a n.
            for (auto& e : parent->entries)
                if (e.child == n) { e.mbr = nodeMBR(n); break; }

            if (split) {
                Entry e;
                e.child = split;
                e.mbr = nodeMBR(split);
                split->parent = parent;
                parent->entries.push_back(e);
                split = ((int)parent->entries.size() > maxEntries_)
                            ? quadraticSplit(parent) : nullptr;
            }
            n = parent;
        }
        // Llegamos a la raiz: si se partio, crear nueva raiz.
        if (split) {
            Node* newRoot = new Node(false);
            Entry a; a.child = root_;  a.mbr = nodeMBR(root_);  root_->parent = newRoot;
            Entry b; b.child = split;  b.mbr = nodeMBR(split);  split->parent = newRoot;
            newRoot->entries.push_back(a);
            newRoot->entries.push_back(b);
            root_ = newRoot;
        }
    }

    // --- Split cuadratico (requisito obligatorio del proyecto) ---
    // Devuelve un nodo nuevo con la mitad de las entradas; 'n' conserva la otra mitad.
    Node* quadraticSplit(Node* n) {
        std::vector<Entry> all = std::move(n->entries);
        n->entries.clear();

        // 1) pickSeeds: par que maximiza el "desperdicio" de area si van juntas.
        int s1 = 0, s2 = 1;
        double worst = -1.0;
        for (int i = 0; i < (int)all.size(); ++i)
            for (int j = i + 1; j < (int)all.size(); ++j) {
                double d = MBR::combine(all[i].mbr, all[j].mbr).area()
                           - all[i].mbr.area() - all[j].mbr.area();
                if (d > worst) { worst = d; s1 = i; s2 = j; }
            }

        Node* nn = new Node(n->leaf);
        MBR mbr1 = all[s1].mbr, mbr2 = all[s2].mbr;
        n->entries.push_back(all[s1]);
        nn->entries.push_back(all[s2]);
        if (!n->leaf)  all[s1].child->parent = n;
        if (!nn->leaf) all[s2].child->parent = nn;

        std::vector<bool> used(all.size(), false);
        used[s1] = used[s2] = true;
        int remaining = (int)all.size() - 2;

        // 2-3) pickNext: asignar al grupo de menor incremento de area.
        while (remaining > 0) {
            // Si un grupo necesita todos los restantes para alcanzar el minimo, dárselos.
            if ((int)n->entries.size() + remaining <= minEntries_) {
                assignRest(all, used, n, mbr1);
                break;
            }
            if ((int)nn->entries.size() + remaining <= minEntries_) {
                assignRest(all, used, nn, mbr2);
                break;
            }

            // Elegir la entrada con mayor diferencia de costo entre ambos grupos.
            int pick = -1; double bestDiff = -1.0; double enl1 = 0, enl2 = 0;
            for (int i = 0; i < (int)all.size(); ++i) {
                if (used[i]) continue;
                double e1 = mbr1.enlargement(all[i].mbr);
                double e2 = mbr2.enlargement(all[i].mbr);
                double diff = std::abs(e1 - e2);
                if (diff > bestDiff) { bestDiff = diff; pick = i; enl1 = e1; enl2 = e2; }
            }

            // Colocar en el grupo de menor incremento (desempates: area, luego cantidad).
            Node* target;
            MBR* tmbr;
            if (enl1 < enl2)       { target = n;  tmbr = &mbr1; }
            else if (enl2 < enl1)  { target = nn; tmbr = &mbr2; }
            else if (mbr1.area() < mbr2.area()) { target = n;  tmbr = &mbr1; }
            else if (mbr2.area() < mbr1.area()) { target = nn; tmbr = &mbr2; }
            else if (n->entries.size() <= nn->entries.size()) { target = n; tmbr = &mbr1; }
            else { target = nn; tmbr = &mbr2; }

            if (!target->leaf) all[pick].child->parent = target;
            target->entries.push_back(all[pick]);
            tmbr->expand(all[pick].mbr);
            used[pick] = true;
            --remaining;
        }
        return nn;
    }

    void assignRest(std::vector<Entry>& all, std::vector<bool>& used, Node* target, MBR& tmbr) {
        for (int i = 0; i < (int)all.size(); ++i) {
            if (used[i]) continue;
            if (!target->leaf) all[i].child->parent = target;
            target->entries.push_back(all[i]);
            tmbr.expand(all[i].mbr);
            used[i] = true;
        }
    }

    // --- Busqueda por rango ---
    void rangeRec(const Node* n, const MBR& region,
                  std::vector<const SpatialObject*>& res, long long& visited,
                  std::vector<MBR>* visitadas, std::vector<int>* pasoResultado) const {
        ++visited;
        if (visitadas) visitadas->push_back(nodeMBR(n));
        for (const auto& e : n->entries) {
            if (n->leaf) {
                if (region.contains(e.obj->point())) {
                    res.push_back(e.obj);
                    if (pasoResultado) pasoResultado->push_back((int)visited);
                }
            } else if (region.intersects(e.mbr)) {
                rangeRec(e.child, region, res, visited, visitadas, pasoResultado);
            }
        }
    }

    // --- Soporte para remove ---
    bool findLeaf(Node* n, int id, Node*& outLeaf, int& outIdx) {
        if (n->leaf) {
            for (int i = 0; i < (int)n->entries.size(); ++i)
                if (n->entries[i].obj->id == id) { outLeaf = n; outIdx = i; return true; }
            return false;
        }
        for (auto& e : n->entries)
            if (findLeaf(e.child, id, outLeaf, outIdx)) return true;
        return false;
    }

    void findLeafNode(Node*, int, Node*&, int&) {} // (reservado)

    // Tras un borrado, propaga hacia arriba y recoge huerfanos de nodos infrapoblados.
    void condenseTree(Node* n, std::vector<const SpatialObject*>& orphans) {
        while (n != root_) {
            Node* parent = n->parent;
            int idx = -1;
            for (int i = 0; i < (int)parent->entries.size(); ++i)
                if (parent->entries[i].child == n) { idx = i; break; }

            if ((int)n->entries.size() < minEntries_) {
                // Recolectar todos los objetos de hoja del subarbol y eliminar el nodo.
                collectObjects(n, orphans);
                parent->entries.erase(parent->entries.begin() + idx);
                destroy(n);
            } else {
                parent->entries[idx].mbr = nodeMBR(n);
            }
            n = parent;
        }
    }

    // Recoge todos los objetos de las hojas de un subarbol.
    void collectObjects(Node* n, std::vector<const SpatialObject*>& out) {
        if (n->leaf) {
            for (auto& e : n->entries) out.push_back(e.obj);
        } else {
            for (auto& e : n->entries) collectObjects(e.child, out);
        }
    }

    void collectNodeMBRs(const Node* n, int level,
                         std::vector<std::pair<int, MBR>>& out) const {
        out.push_back({ level, nodeMBR(n) });
        if (!n->leaf)
            for (const auto& e : n->entries) collectNodeMBRs(e.child, level + 1, out);
    }

    int heightRec(const Node* n) const {
        if (n->leaf) return 1;
        return 1 + heightRec(n->entries[0].child);
    }

    void destroy(Node* n) {
        if (!n) return;
        if (!n->leaf)
            for (auto& e : n->entries) destroy(e.child);
        delete n;
    }
};