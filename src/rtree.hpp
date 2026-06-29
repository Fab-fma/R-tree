#pragma once
#include <vector>
#include <queue>
#include <algorithm>
#include "geometry.hpp"
#include "spatial_object.hpp"

// cada nodo tiene un MBR que encierra todo lo de abajo (cajas dentro de cajas).
// hoja = objetos reales; interno = hijos. la poda en las consultas sale de esos MBR.
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

    // maxEntries = tope por nodo (al pasarse, se parte). minEntries = mitad.
    explicit RTree(int maxEntries = 8) :
        maxEntries_(maxEntries),
        minEntries_(std::max(2, maxEntries / 2)) {
        root_ = new Node(true);
    }

    ~RTree() { destroy(root_); }

    RTree(const RTree&) = delete;
    RTree& operator=(const RTree&) = delete;

    // baja a la mejor hoja (chooseLeaf), mete el objeto; si la hoja se pasa de
    // maxEntries se parte (split); adjustTree sube arreglando MBRs y propagando el split.
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

    // a diferencia de B+ (que hace merge/redistribute entre hermanos), aca el underflow
    // se resuelve por reinsercion: borra la entrada, condenseTree saca los objetos de los
    // nodos que quedaron por debajo del minimo, y se reinsertan desde la raiz.
    bool remove(int id) {
        Node* leaf = nullptr;
        int idx = -1;
        if (!findLeaf(root_, id, leaf, idx)) return false;

        leaf->entries.erase(leaf->entries.begin() + idx);

        std::vector<const SpatialObject*> orphans;
        condenseTree(leaf, orphans);                        // recoge huerfanos por underflow

        for (auto* o : orphans) insert(o);                  // reinsertar (mantiene balance)

        // si la raiz interna quedo con un solo hijo, baja un nivel (arbol encoge)
        if (!root_->leaf && root_->entries.size() == 1) {
            Node* child = root_->entries[0].child;
            child->parent = nullptr;
            delete root_;
            root_ = child;
        }
        return true;
    }

    // DFS podando: solo baja a los nodos cuyo MBR toca la region (el resto se descarta).
    // visitedNodes = cuantos nodos abrio (metrica vs lineal). visitadas/pasoResultado = opcionales,
    // para dibujar las cajas recorridas y revelar cada resultado en su paso.
    std::vector<const SpatialObject*> rangeQuery(const MBR& region, long long& visitedNodes,
                                                 std::vector<MBR>* visitadas = nullptr,
                                                 std::vector<int>* pasoResultado = nullptr) const {
        std::vector<const SpatialObject*> res;
        visitedNodes = 0;
        rangeRec(root_, region, res, visitedNodes, visitadas, pasoResultado);
        return res;
    }

    // best-first: saca siempre lo mas cercano de la cola. al sacar k objetos termina;
    // lo que queda en la cola esta garantizado mas lejos -> no se mira.
    std::vector<const SpatialObject*> kNN(const Point& q, int k, long long& visitedNodes,
                                          std::vector<MBR>* visitadas = nullptr,
                                          std::vector<int>* pasoResultado = nullptr) const {
        visitedNodes = 0;
        // cola por menor distancia. mete nodos (con mindist de su caja) y objetos (dist real).
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
            // expandir el nodo: hoja -> mete sus objetos; interno -> mete sus hijos.
            // mindist2 = cota inferior de distancia a cualquier punto de esa caja.
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

    // MBR de cada nodo con su nivel (para dibujar el arbol por niveles)
    std::vector<std::pair<int, MBR>> nodeMBRs() const {
        std::vector<std::pair<int, MBR>> out;
        collectNodeMBRs(root_, 0, out);
        return out;
    }

private:
    Node* root_;
    int maxEntries_;
    int minEntries_;

    // MBR de un nodo = union de las cajas de sus entradas
    static MBR nodeMBR(const Node* n) {
        MBR m = MBR::empty();
        for (const auto& e : n->entries) m.expand(e.mbr);
        return m;
    }

    // baja eligiendo en cada nivel el hijo cuya caja crece menos al meter m
    // (enlargement minimo; desempate: la de menor area). asi desordena lo menos posible.
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

    // sube desde n hasta la raiz: agranda los MBR de los padres y, si hubo split,
    // mete el nodo nuevo en el padre (que a su vez puede partirse y subir el split).
    void adjustTree(Node* n, Node* split) {
        while (n != root_) {
            Node* parent = n->parent;
            // reajustar la caja de la entrada que apunta a n
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
        // si el split llego hasta la raiz, se crea una raiz nueva (arbol crece hacia arriba)
        if (split) {
            Node* newRoot = new Node(false);
            Entry a; a.child = root_;  a.mbr = nodeMBR(root_);  root_->parent = newRoot;
            Entry b; b.child = split;  b.mbr = nodeMBR(split);  split->parent = newRoot;
            newRoot->entries.push_back(a);
            newRoot->entries.push_back(b);
            root_ = newRoot;
        }
    }

    // split cuadratico: reparte las entradas de un nodo lleno en dos.
    // 'n' se queda con un grupo y se devuelve un nodo nuevo con el otro.
    Node* quadraticSplit(Node* n) {
        std::vector<Entry> all = std::move(n->entries);
        n->entries.clear();

        // pickSeeds: las 2 semillas = el par que mas area desperdiciaria junto (las mas separadas).
        // es O(n^2) sobre las entradas del nodo (pocas), de ahi lo de "cuadratico".
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

        // pickNext: reparte el resto, cada uno al grupo cuya caja crece menos.
        while (remaining > 0) {
            // si a un grupo le faltan justo todos los restantes para el minimo, se los lleva
            if ((int)n->entries.size() + remaining <= minEntries_) {
                assignRest(all, used, n, mbr1);
                break;
            }
            if ((int)nn->entries.size() + remaining <= minEntries_) {
                assignRest(all, used, nn, mbr2);
                break;
            }

            // toca la entrada con mayor diferencia de costo entre grupos (la mas "decidida")
            int pick = -1; double bestDiff = -1.0; double enl1 = 0, enl2 = 0;
            for (int i = 0; i < (int)all.size(); ++i) {
                if (used[i]) continue;
                double e1 = mbr1.enlargement(all[i].mbr);
                double e2 = mbr2.enlargement(all[i].mbr);
                double diff = std::abs(e1 - e2);
                if (diff > bestDiff) { bestDiff = diff; pick = i; enl1 = e1; enl2 = e2; }
            }

            // al grupo de menor crecimiento (desempates: menor area, luego menos lleno)
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

    // vuelca todas las entradas que quedan en un grupo (para respetar el minimo)
    void assignRest(std::vector<Entry>& all, std::vector<bool>& used, Node* target, MBR& tmbr) {
        for (int i = 0; i < (int)all.size(); ++i) {
            if (used[i]) continue;
            if (!target->leaf) all[i].child->parent = target;
            target->entries.push_back(all[i]);
            tmbr.expand(all[i].mbr);
            used[i] = true;
        }
    }

    // DFS: en hoja agrega los puntos dentro de la region; en interno solo recurre a los
    // hijos cuyo MBR toca la region (los que no tocan se podan, no se abren).
    void rangeRec(const Node* n, const MBR& region, std::vector<const SpatialObject*>& res, long long& visited,
                  std::vector<MBR>* visitadas, std::vector<int>* pasoResultado) const {
        ++visited;
        if (visitadas) visitadas->push_back(nodeMBR(n));
        for (const auto& e : n->entries) {
            if (n->leaf) {
                if (region.contains(e.obj->point())) {
                    res.push_back(e.obj);
                    if (pasoResultado) pasoResultado->push_back((int)visited);
                }
            }
            else if (region.intersects(e.mbr)) {
                rangeRec(e.child, region, res, visited, visitadas, pasoResultado);
            }
        }
    }

    // busca el id recorriendo todo el arbol (no es espacial: el remove es por id, no por zona)
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

    // sube desde la hoja: si un nodo quedo con menos del minimo (underflow), lo elimina
    // y guarda sus objetos como huerfanos (luego remove() los reinserta). si no, solo
    // reajusta el MBR del padre. (en B+ aca se haria merge/redistribution con hermanos).
    void condenseTree(Node* n, std::vector<const SpatialObject*>& orphans) {
        while (n != root_) {
            Node* parent = n->parent;
            int idx = -1;
            for (int i = 0; i < (int)parent->entries.size(); ++i)
                if (parent->entries[i].child == n) { idx = i; break; }

            if ((int)n->entries.size() < minEntries_) {
                // underflow: sacar todos los objetos del subarbol y borrar el nodo
                collectObjects(n, orphans);
                parent->entries.erase(parent->entries.begin() + idx);
                destroy(n);
            } 
            else {
                parent->entries[idx].mbr = nodeMBR(n);
            }
            n = parent;
        }
    }

    // baja hasta las hojas y junta todos sus objetos
    void collectObjects(Node* n, std::vector<const SpatialObject*>& out) {
        if (n->leaf) {
            for (auto& e : n->entries) out.push_back(e.obj);
        } 
        else {
            for (auto& e : n->entries) collectObjects(e.child, out);
        }
    }

    // DFS guardando (nivel, MBR) de cada nodo
    void collectNodeMBRs(const Node* n, int level, std::vector<std::pair<int, MBR>>& out) const {
        out.push_back({ level, nodeMBR(n) });
        if (!n->leaf)
            for (const auto& e : n->entries) collectNodeMBRs(e.child, level + 1, out);
    }

    // altura: baja por un solo hijo (el arbol esta balanceado, todas las hojas al mismo nivel)
    int heightRec(const Node* n) const {
        if (n->leaf) return 1;
        return 1 + heightRec(n->entries[0].child);
    }

    // libera el subarbol (recursivo)
    void destroy(Node* n) {
        if (!n) return;
        if (!n->leaf)
            for (auto& e : n->entries) destroy(e.child);
        delete n;
    }
};