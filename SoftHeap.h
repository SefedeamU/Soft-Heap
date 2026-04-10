/*
 * =============================================================================
 *  SOFT HEAP - Implementación basada en Kaplan & Zwick (2009)
 *  "A simpler implementation and analysis of Chazelle's Soft Heaps"
 * =============================================================================
 *
 *  Un Soft Heap es una cola de prioridad aproximada inventada por Bernard
 *  Chazelle (2000). A diferencia de un heap clásico, permite que algunas
 *  claves se "corrompan" (incrementen artificialmente) a cambio de obtener
 *  operaciones insert y meld en tiempo amortizado O(1).
 *
 *  Parámetro clave: epsilon (ε) ∈ (0, 1]
 *    - Controla la tasa de corrupción: en todo momento, a lo sumo ε·n de
 *      los n elementos actuales pueden tener claves corrompidas.
 *    - Umbral de rango: T = ⌈log₂(1/ε)⌉
 *    - Nodos con rango > T realizan "double sift", causando corrupción.
 *
 *  Complejidades:
 *    insert:     O(1) amortizado (para ε constante)
 *    delete-min: O(log(1/ε)) amortizado
 *    meld:       O(1) amortizado
 *    find-min:   O(1) peor caso (via punteros sufmin)
 *
 *  Aplicación principal: Algoritmo de Minimum Spanning Tree en O(n·α(n))
 *
 *  Observador opcional:
 *    Se puede registrar un callback con setObserver() para recibir
 *    notificaciones en puntos clave de las operaciones internas
 *    (pre/post sift, link, combine). Útil para visualización.
 *    Si no se registra ningún observador, el overhead es cero.
 * =============================================================================
 */

#ifndef SOFT_HEAP_H
#define SOFT_HEAP_H

#include <list>
#include <cmath>
#include <algorithm>
#include <tuple>
#include <utility>
#include <functional>
#include <string>

// ========================= Item =========================
// Representa un elemento almacenado en el heap.
// Cada item conserva su clave original (key) aunque la clave
// reportada por el nodo que lo contiene (ckey) pueda ser mayor.
// Un item está "corrompido" si ckey > key.

struct Item {
    int key;    // clave original (verdadera, nunca cambia)
    int vertex; // opcional: para aplicaciones como MST
    int id;     // identificador único

    Item(int k, int v) : key(k), vertex(v), id(nextId()) {}

    static void resetIds() { idCounter() = 1; }

private:
    static int& idCounter() { static int c = 1; return c; }
    static int nextId() { return idCounter()++; }
};


// ========================= Node =========================
// Nodo del árbol binario dentro del soft heap.
//
// - ckey:  clave corrompida. Es la clave que el heap "reporta"
//          para todos los items almacenados en este nodo.
//          Puede ser MAYOR que la clave real de algunos items
//          (esos items están corrompidos).
//
// - rank:  rango del nodo (similar al rango en binomial heaps).
//          Determina si el nodo realiza "double sift".
//
// - items: lista de items. Un nodo hoja tiene exactamente 1 item.
//          Los nodos internos acumulan items via sift.

struct Node {
    int id;
    int ckey;
    int rank;
    Node* left;
    Node* right;
    std::list<Item> items;

    // Nodo hoja: contiene un solo item con la clave dada
    Node(int key, int vertex)
        : id(nextId()), ckey(key), rank(0),
          left(nullptr), right(nullptr) {
        items.emplace_back(key, vertex);
    }

    // Nodo interno: resultado de link, inicialmente sin items
    Node(int r, Node* l, Node* ri)
        : id(nextId()), ckey(0), rank(r),
          left(l), right(ri) {}

    bool isLeaf() const { return !left && !right; }

    static void resetIds() { idCounter() = 1; }

private:
    static int& idCounter() { static int c = 1; return c; }
    static int nextId() { return idCounter()++; }
};


// ========================= HeadNode =========================
// Nodo cabecera en la lista de raíces del soft heap.
// Cada HeadNode envuelve la raíz de un árbol y mantiene:
// - Enlaces prev/next para la lista doblemente enlazada.
// - sufmin: puntero al HeadNode con menor ckey desde esta
//   posición hasta el final (permite findMin en O(1)).

struct HeadNode {
    Node* tree;
    HeadNode* next;
    HeadNode* prev;
    HeadNode* sufmin;
    int rank;

    HeadNode(Node* t)
        : tree(t), next(nullptr), prev(nullptr),
          sufmin(this), rank(t ? t->rank : 0) {}
};


// ========================= SoftHeap =========================

class SoftHeap {
public:
    // Callback opcional para observar eventos internos.
    // El string describe el evento; el SoftHeap& permite inspeccionar el estado.
    using Observer = std::function<void(const std::string& event)>;

private:
    double epsilon_;
    int threshold_;     // T = ⌈log₂(1/ε)⌉
    HeadNode* head_;
    HeadNode* tail_;
    int size_;
    Observer observer_;

    void notify(const std::string& event) {
        if (observer_) observer_(event);
    }

    // ---------------------------------------------------------
    //  sift(v): Repone la lista de items del nodo v tomando
    //           de sus hijos.
    //
    //  >>> AQUÍ OCURRE LA CORRUPCIÓN <<<
    //
    //  Si rank(v) > T, se realiza un "double sift":
    //    Ronda 1: tomar items del hijo con menor ckey.
    //             v.ckey = hijo.ckey (correcto hasta aquí).
    //    Ronda 2: tomar items del OTRO hijo.
    //             v.ckey = otroHijo.ckey (posiblemente mayor).
    //             → Los items de la ronda 1 ahora tienen
    //               ckey > key original → ¡están corrompidos!
    //
    //  Si rank(v) ≤ T, solo se hace UNA ronda (sin corrupción).
    //  Así, T controla cuántos nodos pueden corromper, y por
    //  tanto la tasa de corrupción se mantiene ≤ ε.
    // ---------------------------------------------------------
    void sift(Node* v) {
        if (!v->left && !v->right) return;

        // Asegurar que el hijo izquierdo existe
        if (!v->left) std::swap(v->left, v->right);
        if (!v->left) return;

        // --- Ronda 1 ---
        // Elegir hijo con menor ckey como izquierdo
        if (v->right && v->right->ckey < v->left->ckey) {
            std::swap(v->left, v->right);
        }

        notify("pre_sift_round1");

        // Tomar TODOS los items del hijo izquierdo
        v->ckey = v->left->ckey;
        v->items.splice(v->items.end(), v->left->items);

        // Manejar hijo izquierdo agotado
        if (v->left->isLeaf()) {
            delete v->left;
            v->left = v->right;
            v->right = nullptr;
        } else {
            sift(v->left);  // reponer recursivamente
        }

        notify("post_sift_round1");

        // --- Ronda 2 (SOLO si rank > threshold) ---
        if (v->rank > threshold_ && v->left) {
            if (v->right && v->right->ckey < v->left->ckey) {
                std::swap(v->left, v->right);
            }

            notify("pre_sift_round2");

            // ckey puede INCREMENTAR → items de ronda 1 se corrompen
            v->ckey = v->left->ckey;
            v->items.splice(v->items.end(), v->left->items);

            if (v->left->isLeaf()) {
                delete v->left;
                v->left = v->right;
                v->right = nullptr;
            } else {
                sift(v->left);
            }

            notify("post_sift_round2");
        }
    }

    // ---------------------------------------------------------
    //  link(x, y): Combina dos árboles del mismo rango r.
    //  Crea un nuevo nodo raíz con rango r+1.
    //  Llama a sift para poblar el nuevo nodo con items
    //  de sus hijos.
    // ---------------------------------------------------------
    Node* link(Node* x, Node* y) {
        Node* z = new Node(x->rank + 1, x, y);
        notify("post_link_pre_sift");
        sift(z);
        return z;
    }

    // ---------------------------------------------------------
    //  updateSufmin(): Recalcula los punteros de sufijo mínimo.
    //  Recorriendo de tail a head, cada HeadNode.sufmin apunta
    //  al HeadNode con menor ckey desde esa posición al final.
    //  Esto permite findMin() en O(1).
    // ---------------------------------------------------------
    void updateSufmin() {
        if (!head_) return;

        HeadNode* t = head_;
        while (t->next) t = t->next;
        tail_ = t;

        t->sufmin = t;
        HeadNode* h = t->prev;
        while (h) {
            if (h->tree->ckey <= h->next->sufmin->tree->ckey)
                h->sufmin = h;
            else
                h->sufmin = h->next->sufmin;
            h = h->prev;
        }
    }

    // ---------------------------------------------------------
    //  repeatedCombine(h): Después de insertar un árbol,
    //  combina árboles adyacentes del mismo rango (análogo
    //  al "carry" en suma binaria / consolidación en
    //  binomial heaps).
    // ---------------------------------------------------------
    void repeatedCombine(HeadNode* h) {
        while (h->next && h->rank == h->next->rank) {
            HeadNode* h2 = h->next;

            notify("pre_combine");

            Node* newTree = link(h->tree, h2->tree);

            HeadNode* newHead = new HeadNode(newTree);
            newHead->rank = newTree->rank;
            newHead->prev = h->prev;
            newHead->next = h2->next;

            if (h->prev) h->prev->next = newHead;
            else head_ = newHead;
            if (h2->next) h2->next->prev = newHead;

            delete h;
            delete h2;
            h = newHead;

            notify("post_combine");
        }
        updateSufmin();
    }

    void removeHead(HeadNode* h) {
        if (h->prev) h->prev->next = h->next;
        else head_ = h->next;
        if (h->next) h->next->prev = h->prev;
        else tail_ = h->prev;
        delete h;
        updateSufmin();
    }

    void destroyTree(Node* n) {
        if (!n) return;
        destroyTree(n->left);
        destroyTree(n->right);
        delete n;
    }

public:
    SoftHeap(double epsilon)
        : epsilon_(epsilon), head_(nullptr), tail_(nullptr),
          size_(0), observer_(nullptr) {
        threshold_ = static_cast<int>(std::ceil(std::log2(1.0 / epsilon)));
    }

    ~SoftHeap() {
        HeadNode* h = head_;
        while (h) {
            HeadNode* next = h->next;
            destroyTree(h->tree);
            delete h;
            h = next;
        }
    }

    // No copiable (gestión manual de memoria)
    SoftHeap(const SoftHeap&) = delete;
    SoftHeap& operator=(const SoftHeap&) = delete;

    // Accesores
    double          getEpsilon()   const { return epsilon_; }
    int             getThreshold() const { return threshold_; }
    int             getSize()      const { return size_; }
    bool            isEmpty()      const { return head_ == nullptr; }
    const HeadNode* getHead()      const { return head_; }

    // Registrar/quitar observador (para visualización)
    void setObserver(Observer obs) { observer_ = std::move(obs); }
    void clearObserver() { observer_ = nullptr; }

    // ==========================================================
    //  insert(key)
    //  Crea un nodo hoja y lo incorpora al heap.
    //  Si hay árboles del mismo rango, los combina (link).
    //  Tiempo amortizado: O(1) para ε constante.
    // ==========================================================
    void insert(int key, int vertex) {
        Node* leaf = new Node(key, vertex);

        HeadNode* newHead = new HeadNode(leaf);
        newHead->rank = 0;

        // Insertar al inicio de la lista de raíces
        newHead->next = head_;
        newHead->prev = nullptr;
        if (head_) head_->prev = newHead;
        head_ = newHead;
        if (!tail_) tail_ = newHead;

        size_++;

        notify("post_insert_pre_combine");

        repeatedCombine(head_);
    }

    // ==========================================================
    //  findMin() → {ckey_reportada, clave_real}
    //  Retorna la clave mínima (posiblemente corrompida).
    //  O(1) gracias a los punteros sufmin.
    //  Retorna {-1, -1} si el heap está vacío.
    // ==========================================================
    std::pair<int,int> findMin() const {
        if (!head_) return {-1, -1};
        HeadNode* minH = head_->sufmin;
        if (minH->tree->items.empty()) return {-1, -1};
        const Item& it = minH->tree->items.front();
        return {minH->tree->ckey, it.key};
    }

    // ==========================================================
    //  deleteMin() → {ckey_reportada, clave_real, item_id}
    //  Extrae el item con menor ckey.
    //  Si el nodo queda vacío, ejecuta sift para reponer
    //  o elimina el árbol si no quedan items.
    //  Tiempo amortizado: O(log(1/ε))
    // ==========================================================
    std::tuple<int,int,int,int> deleteMin() {
        if (!head_) return {-1, -1, -1, -1};

        HeadNode* minH = head_->sufmin;
        Node* root = minH->tree;
        if (root->items.empty()) return {-1, -1, -1, -1};

        // Guardar ckey ANTES de cualquier modificación
        int reportedCkey = root->ckey;

        Item extracted = root->items.front();
        root->items.pop_front();
        size_--;

        if (root->items.empty()) {
            if (root->isLeaf()) {
                notify("pre_dismantle");
                destroyTree(root);
                removeHead(minH);
                notify("post_dismantle");
            } else {
                notify("pre_sift");
                sift(root);
                updateSufmin();
                notify("post_sift");
            }
        } else {
            updateSufmin();
        }

        return {reportedCkey, extracted.key, extracted.vertex, extracted.id};
    }

    // ==========================================================
    //  meld(other)
    //  Fusiona otro SoftHeap en este.
    //  Intercala las listas de raíces por rango y combina
    //  árboles del mismo rango.
    //  Tiempo amortizado: O(1)
    //  Nota: "other" queda vacío tras la operación.
    // ==========================================================
    void meld(SoftHeap& other) {
        if (!other.head_) return;
        if (!head_) {
            head_ = other.head_;
            tail_ = other.tail_;
            size_ = other.size_;
            other.head_ = other.tail_ = nullptr;
            other.size_ = 0;
            updateSufmin();
            return;
        }

        // Intercalar las dos listas por rango
        HeadNode* h1 = head_;
        HeadNode* h2 = other.head_;
        HeadNode* newHead = nullptr;
        HeadNode* newTail = nullptr;

        auto append = [&](HeadNode* h) {
            h->prev = newTail;
            h->next = nullptr;
            if (newTail) newTail->next = h;
            else newHead = h;
            newTail = h;
        };

        while (h1 && h2) {
            if (h1->rank <= h2->rank) {
                HeadNode* nxt = h1->next; append(h1); h1 = nxt;
            } else {
                HeadNode* nxt = h2->next; append(h2); h2 = nxt;
            }
        }
        while (h1) { HeadNode* nxt = h1->next; append(h1); h1 = nxt; }
        while (h2) { HeadNode* nxt = h2->next; append(h2); h2 = nxt; }

        head_ = newHead;
        tail_ = newTail;
        size_ += other.size_;
        other.head_ = other.tail_ = nullptr;
        other.size_ = 0;

        notify("post_meld_pre_combine");

        // Combinar árboles del mismo rango
        HeadNode* h = head_;
        while (h) {
            if (h->next && h->rank == h->next->rank) {
                repeatedCombine(h);
                h = head_;
            } else {
                h = h->next;
            }
        }
        updateSufmin();
    }
};

#endif // SOFT_HEAP_H
