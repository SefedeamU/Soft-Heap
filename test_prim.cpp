/*
 * =============================================================================
 * test_grafos.cpp - Pruebas de rendimiento: Exacto vs Soft Heap
 * Algoritmos probados: Prim (MST) y Dijkstra (Shortest Path)
 * =============================================================================
 */

#include <iostream>
#include <vector>
#include <queue>
#include <chrono>
#include <random>
#include "SoftHeap.h"

using namespace std;
using namespace std::chrono;

struct Edge {
    int to, weight;
};

// ==========================================
// Generador de Grafos
// ==========================================
vector<vector<Edge>> generateRandomGraph(int V, int E) {
    vector<vector<Edge>> adj(V);
    mt19937 rng(42); 
    uniform_int_distribution<int> distWeight(1, 100);

    // Asegurar que sea conexo
    for (int i = 1; i < V; ++i) {
        uniform_int_distribution<int> distNode(0, i - 1);
        int u = distNode(rng);
        int w = distWeight(rng);
        adj[u].push_back({i, w});
        adj[i].push_back({u, w});
    }

    // Aristas aleatorias extra
    uniform_int_distribution<int> distAny(0, V - 1);
    for (int i = V - 1; i < E; ++i) {
        int u = distAny(rng), v = distAny(rng);
        if (u != v) {
            int w = distWeight(rng);
            adj[u].push_back({v, w});
            adj[v].push_back({u, w});
        }
    }
    return adj;
}

// ==========================================
// 1. PRIM - Exacto (std::priority_queue)
// ==========================================
long long primExact(const vector<vector<Edge>>& adj, int start) {
    int V = adj.size();
    vector<bool> visited(V, false);
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
    
    long long mstWeight = 0;
    pq.push({0, start});

    while (!pq.empty()) {
        auto [w, u] = pq.top();
        pq.pop();

        if (visited[u]) continue;
        visited[u] = true;
        mstWeight += w;

        for (const auto& edge : adj[u]) {
            if (!visited[edge.to]) {
                pq.push({edge.weight, edge.to});
            }
        }
    }
    return mstWeight;
}

// ==========================================
// 2. PRIM - Aproximado (Soft Heap)
// ==========================================
long long primSoft(const vector<vector<Edge>>& adj, int start, double epsilon) {
    int V = adj.size();
    vector<bool> visited(V, false);
    SoftHeap sh(epsilon);
    
    long long mstWeight = 0;
    sh.insert(0, start);

    while (!sh.isEmpty()) {
        auto [ck, rk, u, id] = sh.deleteMin(); 
        if (ck == -1) break;

        if (visited[u]) continue;
        visited[u] = true;
        mstWeight += rk;

        for (const auto& edge : adj[u]) {
            if (!visited[edge.to]) {
                sh.insert(edge.weight, edge.to);
            }
        }
    }
    return mstWeight;
}

// ==========================================
// 3. DIJKSTRA - Exacto (std::priority_queue)
// ==========================================
long long dijkstraExact(const vector<vector<Edge>>& adj, int start) {
    int V = adj.size();
    const long long INF = 1e18; 
    vector<long long> dist(V, INF);
    
    priority_queue<pair<long long, int>, vector<pair<long long, int>>, greater<pair<long long, int>>> pq;
    
    dist[start] = 0;
    pq.push({0, start});
    
    long long sumOfShortestPaths = 0;

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        if (d > dist[u]) continue;
        sumOfShortestPaths += dist[u];

        for (const auto& edge : adj[u]) {
            if (dist[u] + edge.weight < dist[edge.to]) {
                dist[edge.to] = dist[u] + edge.weight;
                pq.push({dist[edge.to], edge.to});
            }
        }
    }
    return sumOfShortestPaths;
}

// ==========================================
// 4. DIJKSTRA - Aproximado (Soft Heap)
// ==========================================
long long dijkstraSoft(const vector<vector<Edge>>& adj, int start, double epsilon) {
    int V = adj.size();
    const long long INF = 1e18;
    vector<long long> dist(V, INF);
    
    SoftHeap sh(epsilon);
    
    dist[start] = 0;
    sh.insert(0, start); 
    
    long long sumOfShortestPaths = 0;

    while (!sh.isEmpty()) {
        auto [ck, rk, u, id] = sh.deleteMin();
        if (ck == -1) break;

        if (rk > dist[u]) continue;
        sumOfShortestPaths += dist[u];

        for (const auto& edge : adj[u]) {
            if (dist[u] + edge.weight < dist[edge.to]) {
                dist[edge.to] = dist[u] + edge.weight;
                sh.insert(dist[edge.to], edge.to);
            }
        }
    }
    return sumOfShortestPaths;
}

// ==========================================
// FUNCIÓN PRINCIPAL
// ==========================================
int main() {
    // Parámetros de prueba
    int V = 5000;
    int E = 50000;
    cout << "Generando grafo aleatorio (V=" << V << ", E=" << E << ")...\n";
    auto adj = generateRandomGraph(V, E);
    double epsilons[] = {0.5, 0.25, 0.1, 0.05};

    // ---------------------------------------------------------
    // SECCIÓN PRIM
    // ---------------------------------------------------------
    cout << "\n==================================================" << endl;
    cout << "           PRUEBAS DE RENDIMIENTO PRIM            " << endl;
    cout << "==================================================" << endl;

    auto startExact = high_resolution_clock::now();
    long long exactMST = primExact(adj, 0);
    auto endExact = high_resolution_clock::now();
    auto durationExact = duration_cast<milliseconds>(endExact - startExact).count();

    cout << "\n--- Resultados Standard Priority Queue ---" << endl;
    cout << "Costo MST: " << exactMST << endl;
    cout << "Tiempo:    " << durationExact << " ms" << endl;

    for (double eps : epsilons) {
        auto startSoft = high_resolution_clock::now();
        long long softMST = primSoft(adj, 0, eps);
        auto endSoft = high_resolution_clock::now();
        auto durationSoft = duration_cast<milliseconds>(endSoft - startSoft).count();

        double errorRate = ((double)(softMST - exactMST) / exactMST) * 100.0;

        cout << "\n--- Resultados Soft Heap (Epsilon = " << eps << ") ---" << endl;
        cout << "Costo MST Aproximado: " << softMST << endl;
        cout << "Margen de error:      " << errorRate << "%" << endl;
        cout << "Tiempo:               " << durationSoft << " ms" << endl;
    }

    // ---------------------------------------------------------
    // SECCIÓN DIJKSTRA
    // ---------------------------------------------------------
    cout << "\n==================================================" << endl;
    cout << "           PRUEBAS DE RENDIMIENTO DIJKSTRA        " << endl;
    cout << "==================================================" << endl;

    auto startExactDijkstra = high_resolution_clock::now();
    long long exactDistSum = dijkstraExact(adj, 0);
    auto endExactDijkstra = high_resolution_clock::now();
    auto durationExactD = duration_cast<milliseconds>(endExactDijkstra - startExactDijkstra).count();

    cout << "\n--- Resultados Standard Priority Queue ---" << endl;
    cout << "Suma de distancias: " << exactDistSum << endl;
    cout << "Tiempo:             " << durationExactD << " ms" << endl;

    for (double eps : epsilons) {
        auto startSoftDijkstra = high_resolution_clock::now();
        long long softDistSum = dijkstraSoft(adj, 0, eps);
        auto endSoftDijkstra = high_resolution_clock::now();
        auto durationSoftD = duration_cast<milliseconds>(endSoftDijkstra - startSoftDijkstra).count();

        double errorRateD = ((double)(softDistSum - exactDistSum) / exactDistSum) * 100.0;

        cout << "\n--- Resultados Soft Heap (Epsilon = " << eps << ") ---" << endl;
        cout << "Suma distancias aprox: " << softDistSum << endl;
        cout << "Margen de error:       " << errorRateD << "%" << endl;
        cout << "Tiempo:                " << durationSoftD << " ms" << endl;
    }

    return 0;
}