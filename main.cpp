/*
 * =============================================================================
 *  main.cpp - Ejemplos demostrativos del Soft Heap
 * =============================================================================
 *  Cada ejemplo resalta un aspecto único del soft heap:
 *
 *    1. Estructura del arbol       → cómo insert + link construyen árboles binarios
 *    2. Mecanismo de corrupcion    → double sift: ckey sube y corrompe items previos
 *    3. Operacion Meld             → fusión de dos heaps con intercalado por rango
 *    4. Tasa de corrupcion acotada → garantía ε·n: a menor ε, menos corrupción
 *    5. Ordenamiento aproximado    → efecto práctico: extracción "casi" ordenada
 *
 *  Salida: NDJSON (una línea JSON por snapshot) para visualize.py
 * =============================================================================
 */

#include "SoftHeap.h"
#include "Snapshot.h"
#include <iostream>
#include <string>
#include <vector>

using namespace std;

int main() {
    vector<string> log;

    // ================================================================
    //  EJEMPLO 1: Estructura del árbol (ε = 0.03125, T = 5)
    // ================================================================
    //  Con T = 5 los nodos de rango ≤ 5 hacen solo UNA ronda de sift,
    //  por lo que los hijos se preservan en el árbol y podemos ver
    //  la estructura jerárquica completa con todos los nodos.
    //
    //  Se insertan 8 elementos para crear árboles de rango 0 → 3.
    //  El observador captura la estructura ANTES y DESPUÉS de cada
    //  link y sift para que ningún nodo intermedio se pierda.
    {
        Node::resetIds(); Item::resetIds();
        SoftHeap sh(0.03125); // T = ceil(log2(32)) = 5
        SnapshotLogger snap(1, "Estructura del arbol (epsilon=0.03, threshold=5)", log);

        snap.nextStep();
        snap.capture(sh, "Estado inicial: heap vacio");

        int keys[] = {10, 4, 7, 2, 8, 13, 1, 6};

        for (int k : keys) {
            // Observador: captura el estado en cada fase interna
            sh.setObserver([&](const string& event) {
                if (event == "post_insert_pre_combine") {
                    snap.nextStep();
                    snap.capture(sh,
                        "Hoja insertada (clave " + to_string(k) +
                        "). Antes de consolidar.",
                        "insert", true, "insert");
                }
                else if (event == "post_link_pre_sift") {
                    snap.nextStep();
                    snap.capture(sh,
                        "Link: nuevo nodo interno creado (pre-sift). "
                        "Hijos visibles como sub-arboles.",
                        "link", true, "combine");
                }
                else if (event == "post_combine") {
                    snap.nextStep();
                    snap.capture(sh,
                        "Combine completado. Sift movio items de hijo a padre.",
                        "combine", false, "combine");
                }
            });

            sh.insert(k);
            sh.clearObserver();

            snap.nextStep();
            snap.capture(sh,
                "Estado estable tras insertar " + to_string(k),
                "insert");
        }

        // findMin
        auto [ck, rk] = sh.findMin();
        snap.nextStep();
        snap.capture(sh,
            "findMin: ckey=" + to_string(ck) +
            ", real=" + to_string(rk) +
            (ck > rk ? " [CORROMPIDO]" : " [limpio]"),
            "findmin");

        // deleteMin x3 con intermedios
        for (int i = 0; i < 3; i++) {
            sh.setObserver([&](const string& event) {
                if (event == "pre_sift") {
                    snap.nextStep();
                    snap.capture(sh,
                        "Nodo raiz quedo vacio. Pre-sift: estructura actual.",
                        "sift", true, "sift");
                }
                else if (event == "post_sift") {
                    snap.nextStep();
                    snap.capture(sh,
                        "Post-sift: items recolectados de hijos.",
                        "sift", false, "sift");
                }
            });

            auto [dck, drk, did] = sh.deleteMin();
            sh.clearObserver();

            snap.nextStep();
            snap.capture(sh,
                "deleteMin #" + to_string(i+1) +
                ": ckey=" + to_string(dck) +
                ", real=" + to_string(drk) +
                (dck > drk ? " [CORROMPIDO]" : " [limpio]"),
                "deletemin", false, "delete");
        }
    }

    // ================================================================
    //  EJEMPLO 2: Mecanismo de corrupción (ε = 0.25, T = 2)
    // ================================================================
    //  Con T = 2, los nodos de rango > 2 hacen double sift.
    //  Insertamos 8 claves para llegar a rango 3 y observar
    //  EXACTAMENTE cómo el double sift incrementa ckey y
    //  corrompe los items de la primera ronda.
    {
        Node::resetIds(); Item::resetIds();
        SoftHeap sh(0.25); // T = 2
        SnapshotLogger snap(2, "Mecanismo de corrupcion (epsilon=0.25, threshold=2)", log);

        snap.nextStep();
        snap.capture(sh, "Estado inicial (epsilon=0.25, T=2). "
                         "Nodos de rango > 2 haran double sift.");

        int keys[] = {15, 3, 22, 8, 1, 19, 11, 5};

        for (int k : keys) {
            sh.setObserver([&](const string& event) {
                if (event == "post_insert_pre_combine") {
                    snap.nextStep();
                    snap.capture(sh,
                        "Hoja " + to_string(k) + " insertada, pre-combine.",
                        "insert", true, "insert");
                }
                else if (event == "post_link_pre_sift") {
                    snap.nextStep();
                    snap.capture(sh,
                        "Link creado. Pre-sift: arbol con hijos intactos.",
                        "link", true, "combine");
                }
                else if (event == "pre_sift_round2") {
                    snap.nextStep();
                    snap.capture(sh,
                        ">>> DOUBLE SIFT: ronda 2 inicia. "
                        "ckey puede subir y corromper items previos!",
                        "sift", true, "sift");
                }
                else if (event == "post_sift_round2") {
                    snap.nextStep();
                    snap.capture(sh,
                        ">>> DOUBLE SIFT completado. "
                        "Items de ronda 1 ahora tienen ckey mayor: CORROMPIDOS.",
                        "sift", false, "sift");
                }
                else if (event == "post_combine") {
                    snap.nextStep();
                    snap.capture(sh,
                        "Combine completado.",
                        "combine", false, "none");
                }
            });

            sh.insert(k);
            sh.clearObserver();

            snap.nextStep();
            snap.capture(sh,
                "Tras insertar " + to_string(k) + ". Verificar items [C].",
                "insert");
        }

        snap.nextStep();
        snap.capture(sh, "8 elementos insertados. Items con [C] = corrompidos.");

        // Extraer para ver el efecto
        for (int i = 0; i < 4; i++) {
            auto [ck, rk, id] = sh.deleteMin();
            snap.nextStep();
            string desc = "deleteMin #" + to_string(i+1) +
                          ": ckey=" + to_string(ck) +
                          ", real=" + to_string(rk);
            if (ck > rk) desc += " [CORROMPIDO: extraido fuera de orden!]";
            else         desc += " [limpio]";
            snap.capture(sh, desc, "deletemin", false, "delete");
        }

        snap.nextStep();
        snap.capture(sh, "Estado final tras 4 extracciones.");
    }

    // ================================================================
    //  EJEMPLO 3: Operación Meld (ε = 0.125, T = 3)
    // ================================================================
    //  Dos heaps independientes se fusionan. Meld intercala las
    //  listas de raíces por rango y luego combina árboles del
    //  mismo rango con link.
    {
        Node::resetIds(); Item::resetIds();
        SoftHeap sh1(0.125); // T = 3
        SoftHeap sh2(0.125);
        SnapshotLogger snap(3, "Operacion Meld (epsilon=0.125, threshold=3)", log);

        // Construir Heap 1
        snap.nextStep();
        snap.capture(sh1, "Heap 1 vacio");

        for (int k : {20, 5, 12, 17}) {
            sh1.insert(k);
            snap.nextStep();
            snap.capture(sh1, "Heap1: insertar " + to_string(k), "insert");
        }

        // Construir Heap 2 (sin logging, solo resultado)
        for (int k : {15, 3, 8, 25}) {
            sh2.insert(k);
        }

        snap.nextStep();
        snap.capture(sh1, "Heap 1 tiene {20,5,12,17}. Heap 2 tiene {15,3,8,25}.");

        // Capturar estado pre-meld
        sh1.setObserver([&](const string& event) {
            if (event == "post_meld_pre_combine") {
                snap.nextStep();
                snap.capture(sh1,
                    "Post-intercalado: listas de raices fusionadas por rango. "
                    "Pre-combine.",
                    "meld", true, "meld");
            }
            else if (event == "post_link_pre_sift") {
                snap.nextStep();
                snap.capture(sh1,
                    "Meld: link de arboles del mismo rango (pre-sift).",
                    "link", true, "combine");
            }
            else if (event == "post_combine") {
                snap.nextStep();
                snap.capture(sh1,
                    "Meld: combine completado.",
                    "combine", false, "none");
            }
        });

        sh1.meld(sh2);
        sh1.clearObserver();

        snap.nextStep();
        snap.capture(sh1, "Meld completado: 8 elementos en un solo heap.", "meld");

        // Extraer todos
        while (!sh1.isEmpty()) {
            auto [ck, rk, id] = sh1.deleteMin();
            snap.nextStep();
            snap.capture(sh1,
                "deleteMin: ckey=" + to_string(ck) +
                " real=" + to_string(rk) +
                (ck > rk ? " [C]" : ""),
                "deletemin", false, "delete");
        }

        snap.nextStep();
        snap.capture(sh1, "Heap vacio tras extraer todos los elementos.");
    }

    // ================================================================
    //  EJEMPLO 4: Tasa de corrupción acotada (ε = 0.1, T = 4)
    // ================================================================
    //  Con T = 4, solo nodos de rango > 4 hacen double sift.
    //  Se insertan 16 elementos (rango máx ~4) y se verifica
    //  que la tasa de corrupción se mantiene ≤ ε = 10%.
    {
        Node::resetIds(); Item::resetIds();
        SoftHeap sh(0.1); // T = ceil(log2(10)) = 4
        SnapshotLogger snap(4, "Tasa de corrupcion acotada (epsilon=0.1, threshold=4)", log);

        snap.nextStep();
        snap.capture(sh, "Estado inicial (epsilon=0.1, T=4). "
                         "Solo rango > 4 corrompe.");

        int keys[] = {50, 23, 7, 42, 15, 31, 3, 28, 19, 36, 11, 44, 6, 25, 38, 9};
        for (int k : keys) {
            sh.insert(k);
            snap.nextStep();
            snap.capture(sh, "Insertar " + to_string(k), "insert", false, "insert");
        }

        snap.nextStep();
        snap.capture(sh, "16 elementos. Tasa de corrupcion debe ser <= 10%.");

        for (int i = 0; i < 8; i++) {
            sh.setObserver([&](const string& event) {
                if (event == "pre_sift") {
                    snap.nextStep();
                    snap.capture(sh, "Pre-sift: estructura antes de recolectar.",
                                 "sift", true, "sift");
                }
                else if (event == "post_sift") {
                    snap.nextStep();
                    snap.capture(sh, "Post-sift: items recolectados.",
                                 "sift", false, "sift");
                }
            });

            auto [ck, rk, id] = sh.deleteMin();
            sh.clearObserver();

            snap.nextStep();
            snap.capture(sh,
                "deleteMin #" + to_string(i+1) + ": ckey=" + to_string(ck) +
                " real=" + to_string(rk) + (ck > rk ? " [C]" : ""),
                "deletemin", false, "delete");
        }

        snap.nextStep();
        snap.capture(sh, "Tras 8 extracciones. Estructura restante.");
    }

    // ================================================================
    //  EJEMPLO 5: Ordenamiento aproximado (ε = 0.2, T = 3)
    // ================================================================
    //  Se insertan 10 elementos y se extraen todos con deleteMin.
    //  El orden es "casi" correcto: los corrompidos (*) salen
    //  fuera de orden. Un heap clásico daría orden perfecto.
    {
        Node::resetIds(); Item::resetIds();
        SoftHeap sh(0.2); // T = ceil(log2(5)) = 3
        SnapshotLogger snap(5, "Ordenamiento aproximado (epsilon=0.2, threshold=3)", log);

        snap.nextStep();
        snap.capture(sh, "Estado inicial");

        int keys[] = {30, 10, 50, 20, 40, 5, 35, 15, 45, 25};
        for (int k : keys) {
            sh.insert(k);
            snap.nextStep();
            snap.capture(sh, "Insertar " + to_string(k), "insert", false, "insert");
        }

        snap.nextStep();
        snap.capture(sh, "10 elementos listos. Extraccion aproximadamente ordenada:");

        string order = "Orden de extraccion (ckey->real): ";
        bool first = true;
        while (!sh.isEmpty()) {
            auto [ck, rk, id] = sh.deleteMin();
            if (ck == -1) break;

            if (!first) order += ", ";
            order += to_string(ck) + "->" + to_string(rk);
            if (ck != rk) order += "*";
            first = false;

            snap.nextStep();
            snap.capture(sh,
                "deleteMin: ckey=" + to_string(ck) + " real=" + to_string(rk) +
                (ck > rk ? " [CORROMPIDO]" : " [limpio]"),
                "deletemin", false, "delete");
        }

        snap.nextStep();
        snap.capture(sh, order);
    }

    // ===================== Salida NDJSON =====================
    for (const auto& line : log) {
        cout << line << "\n";
    }

    return 0;
}
