/*
 * =============================================================================
 *  Snapshot.h - Captura y serialización del estado del Soft Heap
 * =============================================================================
 *  Proporciona funciones para:
 *    - Recorrer la estructura interna del SoftHeap (árboles, nodos, items)
 *    - Serializar el estado a JSON (formato NDJSON, una línea por snapshot)
 *    - Generar snapshots con metadatos para visualización
 *
 *  Este archivo es INDEPENDIENTE de la lógica del SoftHeap.
 *  Solo lee el estado público del heap (via getHead()).
 * =============================================================================
 */

#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include "SoftHeap.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <tuple>
#include <iostream>

// ========================= SnapshotData =========================
// Estructura que almacena una "foto" completa del estado del heap.

struct SnapshotData {
    int example;
    std::string title;
    int step;
    int substep;
    std::string description;
    std::string operation;      // insert, deletemin, meld, sift, state, etc.
    bool is_intermediate;
    double epsilon;
    int threshold;
    int total_items;
    int corrupted_items;

    // Highlighting para la visualización
    std::string highlight_type; // none, insert, delete, sift, combine, meld
    std::vector<int> highlight_nodes;
    std::vector<std::pair<int,int>> highlight_edges;
    std::vector<int> highlight_items;

    // Datos del árbol serializados
    struct NodeData {
        int id, ckey, rank;
        int left_id, right_id;
        std::vector<std::tuple<int,int,bool>> items; // key, id, corrupted
    };

    struct TreeData {
        std::vector<NodeData> nodes;
        int root_id;
    };

    std::vector<TreeData> trees;
    std::vector<int> root_ids;
};


// ========================= JSON helpers =========================

namespace snapshot_json {

inline std::string escape(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"')       r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else                r += c;
    }
    return r;
}

inline std::string toJson(const SnapshotData& sd) {
    std::ostringstream o;
    o << "{";
    o << "\"example\":" << sd.example << ",";
    o << "\"title\":\"" << escape(sd.title) << "\",";
    o << "\"step\":" << sd.step << ",";
    o << "\"substep\":" << sd.substep << ",";
    o << "\"description\":\"" << escape(sd.description) << "\",";
    o << "\"operation\":\"" << escape(sd.operation) << "\",";
    o << "\"is_intermediate\":" << (sd.is_intermediate ? "true" : "false") << ",";
    o << "\"epsilon\":" << std::fixed << std::setprecision(4) << sd.epsilon << ",";
    o << "\"threshold\":" << sd.threshold << ",";
    o << "\"total_items\":" << sd.total_items << ",";
    o << "\"corrupted_items\":" << sd.corrupted_items << ",";
    o << "\"highlight_type\":\"" << escape(sd.highlight_type) << "\",";

    o << "\"highlight_nodes\":[";
    for (size_t i = 0; i < sd.highlight_nodes.size(); i++) {
        if (i) o << ","; o << sd.highlight_nodes[i];
    }
    o << "],";

    o << "\"highlight_edges\":[";
    for (size_t i = 0; i < sd.highlight_edges.size(); i++) {
        if (i) o << ",";
        o << "[" << sd.highlight_edges[i].first << "," << sd.highlight_edges[i].second << "]";
    }
    o << "],";

    o << "\"highlight_items\":[";
    for (size_t i = 0; i < sd.highlight_items.size(); i++) {
        if (i) o << ","; o << sd.highlight_items[i];
    }
    o << "],";

    o << "\"trees\":[";
    for (size_t t = 0; t < sd.trees.size(); t++) {
        if (t) o << ",";
        const auto& tree = sd.trees[t];
        o << "{\"root_id\":" << tree.root_id << ",\"nodes\":[";
        for (size_t n = 0; n < tree.nodes.size(); n++) {
            if (n) o << ",";
            const auto& nd = tree.nodes[n];
            o << "{\"id\":" << nd.id
              << ",\"ckey\":" << nd.ckey
              << ",\"rank\":" << nd.rank
              << ",\"left_id\":" << nd.left_id
              << ",\"right_id\":" << nd.right_id
              << ",\"items\":[";
            for (size_t i = 0; i < nd.items.size(); i++) {
                if (i) o << ",";
                o << "{\"key\":" << std::get<0>(nd.items[i])
                  << ",\"id\":" << std::get<1>(nd.items[i])
                  << ",\"corrupted\":" << (std::get<2>(nd.items[i]) ? "true" : "false")
                  << "}";
            }
            o << "]}";
        }
        o << "]}";
    }
    o << "],";

    o << "\"root_ids\":[";
    for (size_t i = 0; i < sd.root_ids.size(); i++) {
        if (i) o << ","; o << sd.root_ids[i];
    }
    o << "]";
    o << "}";
    return o.str();
}

} // namespace snapshot_json


// ========================= SnapshotLogger =========================
// Clase que recorre el SoftHeap y captura snapshots.
// Mantiene un contador de pasos y un log de salida.

class SnapshotLogger {
private:
    int example_;
    std::string title_;
    int step_;
    int substep_;
    std::vector<std::string>& log_;

    // Recorre un nodo recursivamente (pre-order)
    void serializeNode(const Node* n, SnapshotData::TreeData& tree) const {
        if (!n) return;
        SnapshotData::NodeData nd;
        nd.id = n->id;
        nd.ckey = n->ckey;
        nd.rank = n->rank;
        nd.left_id  = n->left  ? n->left->id  : -1;
        nd.right_id = n->right ? n->right->id : -1;
        for (const auto& item : n->items) {
            bool corrupted = (n->ckey > item.key);
            nd.items.push_back({item.key, item.id, corrupted});
        }
        tree.nodes.push_back(nd);
        serializeNode(n->left, tree);
        serializeNode(n->right, tree);
    }

    // Cuenta items totales y corrompidos en un subárbol
    void countItems(const Node* n, int& total, int& corrupted) const {
        if (!n) return;
        for (const auto& item : n->items) {
            total++;
            if (n->ckey > item.key) corrupted++;
        }
        countItems(n->left, total, corrupted);
        countItems(n->right, total, corrupted);
    }

public:
    SnapshotLogger(int example, const std::string& title,
                   std::vector<std::string>& log)
        : example_(example), title_(title), step_(0), substep_(0), log_(log) {}

    void nextStep()    { step_++; substep_ = 0; }
    void nextSubstep() { substep_++; }

    // Captura el estado actual del heap y lo agrega al log
    void capture(const SoftHeap& sh,
                 const std::string& desc,
                 const std::string& op = "state",
                 bool intermediate = false,
                 const std::string& hlType = "none",
                 const std::vector<int>& hlNodes = {},
                 const std::vector<std::pair<int,int>>& hlEdges = {},
                 const std::vector<int>& hlItems = {}) {
        SnapshotData sd;
        sd.example = example_;
        sd.title = title_;
        sd.step = step_;
        sd.substep = substep_;
        sd.description = desc;
        sd.operation = op;
        sd.is_intermediate = intermediate;
        sd.epsilon = sh.getEpsilon();
        sd.threshold = sh.getThreshold();
        sd.highlight_type = hlType;
        sd.highlight_nodes = hlNodes;
        sd.highlight_edges = hlEdges;
        sd.highlight_items = hlItems;

        // Contar items
        int totalItems = 0, corruptedItems = 0;
        for (const HeadNode* h = sh.getHead(); h; h = h->next) {
            countItems(h->tree, totalItems, corruptedItems);
        }
        sd.total_items = totalItems;
        sd.corrupted_items = corruptedItems;

        // Serializar árboles
        for (const HeadNode* h = sh.getHead(); h; h = h->next) {
            SnapshotData::TreeData tree;
            tree.root_id = h->tree->id;
            serializeNode(h->tree, tree);
            sd.trees.push_back(tree);
            sd.root_ids.push_back(h->tree->id);
        }

        log_.push_back(snapshot_json::toJson(sd));
    }
};

#endif // SNAPSHOT_H
