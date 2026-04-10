#!/usr/bin/env python3
"""
=============================================================================
 SOFT HEAP - Visualizador (Linux / macOS / Windows)
=============================================================================
 Este script:
   1. Compila el proyecto C++ con CMake
   2. Ejecuta el binario y captura la salida NDJSON
   3. Parsea los snapshots del estado del heap
   4. Genera imagenes PNG con Graphviz para cada paso y sub-paso
   5. Organiza todo en carpetas: visualizations/ejemplo_N/paso_M.png

 Dependencias:
   - cmake           (sistema de build)
   - Compilador C++  (g++, clang++ o MSVC)
   - graphviz        (paquete de sistema: comando 'dot')
   - graphviz        (libreria Python: pip install graphviz)

 Uso:
   python3 visualize.py       (Linux/macOS)
   python  visualize.py       (Windows)
=============================================================================
"""

import sys
import os
import json
import subprocess
import shutil
import textwrap
import platform

IS_WINDOWS = platform.system() == "Windows"


# ===================== Verificacion de dependencias =====================

def check_dependencies():
    """Verifica que todas las dependencias esten disponibles."""
    missing = []

    # --- cmake ---
    if not shutil.which("cmake"):
        if IS_WINDOWS:
            hint = (
                "cmake (sistema de build)\n"
                "  Instalar: winget install Kitware.CMake\n"
                "            choco install cmake\n"
                "            https://cmake.org/download/"
            )
        else:
            hint = (
                "cmake (sistema de build)\n"
                "  Instalar: sudo apt install cmake  (Debian/Ubuntu)\n"
                "            sudo dnf install cmake  (Fedora)\n"
                "            brew install cmake  (macOS)"
            )
        missing.append(hint)

    # --- Compilador C++ ---
    # CMake encontrara el compilador automaticamente, pero verificamos
    # que al menos uno este disponible para dar un mensaje claro.
    compilers = ["g++", "c++", "clang++"]
    if IS_WINDOWS:
        compilers.append("cl")
    has_compiler = any(shutil.which(c) for c in compilers)
    # En Windows, cl.exe puede no estar en PATH pero si en Visual Studio
    if IS_WINDOWS and not has_compiler:
        # Verificar si existe Visual Studio via vswhere
        vswhere = os.path.join(
            os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
            "Microsoft Visual Studio", "Installer", "vswhere.exe"
        )
        if os.path.isfile(vswhere):
            has_compiler = True  # CMake lo encontrara via generator

    if not has_compiler:
        if IS_WINDOWS:
            hint = (
                "Compilador C++ (MSVC, g++ o clang++)\n"
                "  Instalar: Visual Studio Build Tools\n"
                "            https://visualstudio.microsoft.com/\n"
                "            winget install MSYS2.MSYS2  (para g++)"
            )
        else:
            hint = (
                "Compilador C++ (g++ o clang++)\n"
                "  Instalar: sudo apt install g++  (Debian/Ubuntu)\n"
                "            sudo dnf install gcc-c++  (Fedora)\n"
                "            brew install gcc  (macOS)"
            )
        missing.append(hint)

    # --- graphviz (sistema - comando 'dot') ---
    if not shutil.which("dot"):
        if IS_WINDOWS:
            hint = (
                "graphviz (paquete de sistema - comando 'dot')\n"
                "  Instalar: winget install Graphviz.Graphviz\n"
                "            choco install graphviz\n"
                "            https://graphviz.org/download/"
            )
        else:
            hint = (
                "graphviz (paquete de sistema - comando 'dot')\n"
                "  Instalar: sudo apt install graphviz  (Debian/Ubuntu)\n"
                "            sudo dnf install graphviz  (Fedora)\n"
                "            brew install graphviz  (macOS)"
            )
        missing.append(hint)

    # --- graphviz (libreria Python) ---
    try:
        import graphviz as _gv
    except ImportError:
        missing.append(
            "graphviz (libreria Python)\n"
            "  Instalar: pip install graphviz"
        )

    if missing:
        print("=" * 60)
        print("ERROR: Dependencias faltantes")
        print("=" * 60)
        for i, dep in enumerate(missing, 1):
            print(f"\n  {i}. {dep}")
        print("\n" + "=" * 60)
        print("Instale las dependencias listadas y vuelva a ejecutar.")
        print("=" * 60)
        sys.exit(1)

    print(f"[OK] Dependencias verificadas ({platform.system()}).")


check_dependencies()

# Importar graphviz despues de verificar
import graphviz


# ===================== Colores y estilos =====================

# Estados normales (snapshots finales)
COLORS = {
    "node_fill":        "#D6EAF8",   # azul claro
    "node_border":      "#2471A3",   # azul oscuro
    "node_font":        "#1B2631",   # casi negro
    "item_clean":       "#27AE60",   # verde
    "item_corrupted":   "#E74C3C",   # rojo
    "edge":             "#2C3E50",   # gris oscuro
    "root_link":        "#7F8C8D",   # gris
    "bg":               "#FDFEFE",   # blanco
    "title_color":      "#1A5276",   # azul
    "stats_bg":         "#EBF5FB",   # azul muy claro
    "highlight_fill":   "#F9E79F",   # amarillo claro
    "highlight_border": "#F39C12",   # naranja
}

# Pasos intermedios (transiciones)
INTERMEDIATE_COLORS = {
    "node_fill":        "#F5EEF8",   # lavanda claro
    "node_border":      "#8E44AD",   # purpura
    "node_font":        "#4A235A",   # purpura oscuro
    "item_clean":       "#27AE60",
    "item_corrupted":   "#E74C3C",
    "edge":             "#6C3483",   # purpura medio
    "root_link":        "#A569BD",   # purpura claro
    "bg":               "#FEF9E7",   # crema
    "title_color":      "#6C3483",
    "stats_bg":         "#F5EEF8",
    "highlight_fill":   "#FADBD8",   # rosa claro
    "highlight_border": "#E74C3C",   # rojo
    "arrow_move":       "#E74C3C",   # rojo (flechas de movimiento)
    "arrow_sift":       "#F39C12",   # naranja (flechas de sift)
}


# ===================== Compilacion y ejecucion =====================

def compile_cpp(project_dir, build_dir):
    """Compila el proyecto con CMake. Retorna la ruta al binario o None."""
    print(f"\n[COMPILANDO] Proyecto en {project_dir}")
    os.makedirs(build_dir, exist_ok=True)

    # Paso 1: cmake configure
    configure_cmd = ["cmake", "-S", project_dir, "-B", build_dir]
    if not IS_WINDOWS:
        configure_cmd.append("-DCMAKE_BUILD_TYPE=Release")
    # En Windows, el tipo de build se elige en --build con --config

    try:
        result = subprocess.run(
            configure_cmd,
            capture_output=True, text=True, timeout=60
        )
        if result.returncode != 0:
            print(f"[ERROR] cmake configure fallo:\n{result.stderr}")
            return None
    except FileNotFoundError:
        print("[ERROR] cmake no encontrado.")
        return None
    except subprocess.TimeoutExpired:
        print("[ERROR] cmake configure excedio el tiempo limite.")
        return None

    # Paso 2: cmake build
    build_cmd = ["cmake", "--build", build_dir, "--config", "Release"]
    try:
        result = subprocess.run(
            build_cmd,
            capture_output=True, text=True, timeout=120
        )
        if result.returncode != 0:
            print(f"[ERROR] Compilacion fallida:\n{result.stderr}")
            return None
    except subprocess.TimeoutExpired:
        print("[ERROR] Compilacion excedio el tiempo limite.")
        return None

    # Buscar el binario generado
    bin_name = "softheap.exe" if IS_WINDOWS else "softheap"
    candidates = [
        os.path.join(build_dir, bin_name),
        os.path.join(build_dir, "Release", bin_name),
        os.path.join(build_dir, "Debug", bin_name),
        os.path.join(build_dir, "RelWithDebInfo", bin_name),
        os.path.join(build_dir, "MinSizeRel", bin_name),
    ]

    bin_path = None
    for candidate in candidates:
        if os.path.isfile(candidate):
            bin_path = candidate
            break

    if bin_path is None:
        print(f"[ERROR] Binario '{bin_name}' no encontrado en {build_dir}")
        # Listar contenido del directorio de build para debug
        print("  Contenido del directorio de build:")
        for root, dirs, files in os.walk(build_dir):
            depth = root.replace(build_dir, "").count(os.sep)
            if depth > 2:
                continue
            for f in files:
                if f.endswith((".exe", "")) and not f.endswith((".cmake", ".txt", ".log")):
                    print(f"    {os.path.join(root, f)}")
        return None

    print(f"[OK] Compilacion exitosa: {bin_path}")
    return bin_path


def run_binary(bin_path):
    """Ejecuta el binario y retorna la salida estandar."""
    print(f"\n[EJECUTANDO] {bin_path}")
    try:
        result = subprocess.run(
            [bin_path],
            capture_output=True, text=True, timeout=60
        )
        if result.returncode != 0:
            print(f"[ERROR] Ejecucion fallida (codigo {result.returncode}):\n{result.stderr}")
            return None
        print(f"[OK] Ejecucion exitosa. {len(result.stdout.splitlines())} lineas de salida.")
        return result.stdout
    except subprocess.TimeoutExpired:
        print("[ERROR] Ejecucion excedio el tiempo limite.")
        return None


def parse_snapshots(output):
    """Parsea la salida NDJSON en una lista de diccionarios."""
    snapshots = []
    for line_num, line in enumerate(output.strip().splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        try:
            snap = json.loads(line)
            snapshots.append(snap)
        except json.JSONDecodeError as e:
            print(f"[WARN] Linea {line_num}: JSON invalido: {e}")
    print(f"[OK] {len(snapshots)} snapshots parseados.")
    return snapshots


# ===================== Generacion de grafos =====================

def build_node_index(trees):
    """Construye un indice de nodos por ID a partir de los arboles."""
    index = {}
    for tree in trees:
        for node in tree.get("nodes", []):
            index[node["id"]] = node
    return index


def add_node_to_graph(g, node, colors, highlight_nodes, is_root=False):
    """Agrega un nodo al grafo Graphviz con su formato visual."""
    nid = str(node["id"])
    is_highlighted = node["id"] in highlight_nodes

    fill = colors["highlight_fill"] if is_highlighted else colors["node_fill"]
    border = colors["highlight_border"] if is_highlighted else colors["node_border"]
    penwidth = "3.0" if is_highlighted else "1.5"

    # Construir el label con formato HTML
    items_html = ""
    for item in node.get("items", []):
        corrupted = item.get("corrupted", False)
        item_color = colors["item_corrupted"] if corrupted else colors["item_clean"]
        corruption_mark = " [C]" if corrupted else ""
        items_html += (
            f'<TR><TD ALIGN="LEFT">'
            f'<FONT COLOR="{item_color}">&#8226; key={item["key"]} '
            f'(id:{item["id"]}){corruption_mark}</FONT>'
            f'</TD></TR>'
        )

    if not node.get("items"):
        items_html = (
            '<TR><TD ALIGN="LEFT">'
            '<FONT COLOR="#95A5A6"><I>(sin items)</I></FONT>'
            '</TD></TR>'
        )

    root_marker = ' <FONT COLOR="#E74C3C"><B>[RAIZ]</B></FONT>' if is_root else ""

    label = f'''<
    <TABLE BORDER="0" CELLBORDER="0" CELLSPACING="2" CELLPADDING="4">
        <TR><TD ALIGN="CENTER"><B>
            <FONT COLOR="{colors["node_font"]}">Nodo {node["id"]}</FONT>
            {root_marker}
        </B></TD></TR>
        <TR><TD ALIGN="CENTER">
            <FONT COLOR="{border}">ckey={node["ckey"]}  |  rank={node["rank"]}</FONT>
        </TD></TR>
        <HR/>
        {items_html}
    </TABLE>>'''

    g.node(nid, label=label, shape="box", style="filled,rounded",
           fillcolor=fill, color=border, penwidth=penwidth,
           fontname="Helvetica")


def add_tree_edges(g, node, node_index, colors, highlight_edges, is_intermediate):
    """Agrega las aristas del arbol recursivamente."""
    nid = str(node["id"])

    for child_key in ["left_id", "right_id"]:
        child_id = node.get(child_key, -1)
        if child_id == -1:
            continue
        cid = str(child_id)
        side = "L" if child_key == "left_id" else "R"

        is_highlight_edge = (node["id"], child_id) in highlight_edges
        edge_color = (INTERMEDIATE_COLORS["arrow_move"] if is_highlight_edge
                      else colors["edge"])
        edge_style = "bold,dashed" if is_highlight_edge else "solid"
        edge_width = "3.0" if is_highlight_edge else "1.5"

        g.edge(nid, cid,
               label=f"  {side}  ", fontsize="10",
               color=edge_color, style=edge_style,
               penwidth=edge_width,
               fontcolor=edge_color,
               fontname="Helvetica")

        if child_id in node_index:
            add_tree_edges(g, node_index[child_id], node_index,
                           colors, highlight_edges, is_intermediate)


def render_snapshot(snap, output_path):
    """Genera una imagen PNG para un snapshot dado."""
    is_intermediate = snap.get("is_intermediate", False)
    colors = INTERMEDIATE_COLORS if is_intermediate else COLORS
    highlight_nodes = set(snap.get("highlight_nodes", []))
    highlight_edges_raw = snap.get("highlight_edges", [])
    highlight_edges = set(tuple(e) for e in highlight_edges_raw)

    # Crear grafo
    g = graphviz.Digraph(
        format="png",
        engine="dot",
        graph_attr={
            "bgcolor": colors["bg"],
            "rankdir": "TB",
            "pad": "0.5",
            "nodesep": "0.6",
            "ranksep": "0.8",
            "dpi": "150",
            "label": "",
            "fontname": "Helvetica",
        },
        node_attr={
            "fontname": "Helvetica",
            "fontsize": "11",
        },
        edge_attr={
            "fontname": "Helvetica",
            "fontsize": "10",
        }
    )

    # Titulo y descripcion
    step_label = f"Paso {snap['step']}"
    if snap.get("substep", 0) > 0:
        step_label += f".{snap['substep']}"

    tag = "[INTERMEDIO] " if is_intermediate else ""
    title = f"{tag}{step_label}: {snap['description']}"
    title_wrapped = "\\n".join(textwrap.wrap(title, width=70))

    # Stats
    total = snap.get("total_items", 0)
    corrupted = snap.get("corrupted_items", 0)
    rate = corrupted / total if total > 0 else 0.0
    epsilon = snap.get("epsilon", 0)
    threshold = snap.get("threshold", 0)

    stats_text = (
        f"epsilon={epsilon:.2f}  |  T={threshold}  |  "
        f"Items: {total}  |  Corrompidos: {corrupted}  |  "
        f"Tasa: {rate:.1%} (limite: {epsilon:.0%})"
    )

    g.attr(label=f"{title_wrapped}\\n{stats_text}",
           labelloc="t", fontsize="14",
           fontcolor=colors["title_color"],
           fontname="Helvetica Bold")

    trees = snap.get("trees", [])
    node_index = build_node_index(trees)
    root_ids = snap.get("root_ids", [])

    if not trees:
        g.node("empty", label="<Heap vacio>",
               shape="plaintext", fontsize="16",
               fontcolor="#95A5A6", fontname="Helvetica Italic")
    else:
        for ti, tree in enumerate(trees):
            with g.subgraph(name=f"cluster_tree_{ti}") as sg:
                sg.attr(style="dashed", color=colors["root_link"],
                        label=f"Arbol {ti + 1} (rank={tree['nodes'][0]['rank'] if tree['nodes'] else '?'})",
                        fontsize="10", fontcolor=colors["root_link"],
                        fontname="Helvetica")

                for node in tree.get("nodes", []):
                    is_root = node["id"] == tree.get("root_id", -1)
                    add_node_to_graph(sg, node, colors, highlight_nodes,
                                      is_root=is_root)

                if tree.get("nodes"):
                    root_node = tree["nodes"][0]
                    add_tree_edges(sg, root_node, node_index, colors,
                                   highlight_edges, is_intermediate)

        if len(root_ids) > 1:
            for i in range(len(root_ids) - 1):
                g.edge(str(root_ids[i]), str(root_ids[i + 1]),
                       style="invis", weight="0")

    # Leyenda
    with g.subgraph(name="cluster_legend") as lg:
        lg.attr(style="filled", color="#EAECEE", fillcolor="#FDFEFE",
                label="Leyenda", fontsize="10", fontname="Helvetica Bold",
                fontcolor="#566573")

        lg.node("leg_clean", label="Item limpio (ckey == key)",
                shape="box", style="filled", fillcolor="#D5F5E3",
                fontcolor=colors["item_clean"], fontsize="9",
                fontname="Helvetica", width="0", height="0")

        lg.node("leg_corrupt", label="Item corrompido (ckey > key) [C]",
                shape="box", style="filled", fillcolor="#FADBD8",
                fontcolor=colors["item_corrupted"], fontsize="9",
                fontname="Helvetica", width="0", height="0")

        lg.node("leg_hl", label="Nodo destacado (operacion actual)",
                shape="box", style="filled",
                fillcolor=colors["highlight_fill"],
                color=colors["highlight_border"],
                fontsize="9", fontname="Helvetica", width="0", height="0")

        if is_intermediate:
            lg.node("leg_inter",
                    label="Paso INTERMEDIO (fondo crema = transicion)",
                    shape="box", style="filled",
                    fillcolor=INTERMEDIATE_COLORS["bg"],
                    color=INTERMEDIATE_COLORS["node_border"],
                    fontsize="9", fontname="Helvetica", width="0", height="0")

        lg.edge("leg_clean", "leg_corrupt", style="invis")
        lg.edge("leg_corrupt", "leg_hl", style="invis")
        if is_intermediate:
            lg.edge("leg_hl", "leg_inter", style="invis")

    # Renderizar
    try:
        g.render(output_path, cleanup=True)
    except graphviz.backend.execute.ExecutableNotFound:
        print("[ERROR] El comando 'dot' de Graphviz no se encontro.")
        if IS_WINDOWS:
            print("  Instalar: winget install Graphviz.Graphviz")
        else:
            print("  Instalar: sudo apt install graphviz")
        sys.exit(1)


# ===================== Organizacion de carpetas =====================

def organize_and_render(snapshots, base_dir):
    """Organiza los snapshots en carpetas y genera las imagenes."""
    examples = {}
    for snap in snapshots:
        ex = snap["example"]
        if ex not in examples:
            examples[ex] = []
        examples[ex].append(snap)

    if os.path.exists(base_dir):
        shutil.rmtree(base_dir)
    os.makedirs(base_dir)

    total_images = 0

    for ex_num in sorted(examples.keys()):
        snaps = examples[ex_num]
        title = snaps[0].get("title", f"Ejemplo {ex_num}")

        ex_dir = os.path.join(base_dir, f"ejemplo_{ex_num}")
        os.makedirs(ex_dir, exist_ok=True)

        readme_path = os.path.join(ex_dir, "info.txt")
        with open(readme_path, "w", encoding="utf-8") as f:
            f.write(f"Ejemplo {ex_num}: {title}\n")
            f.write(f"{'=' * 60}\n")
            f.write(f"Epsilon: {snaps[0].get('epsilon', '?')}\n")
            f.write(f"Threshold T: {snaps[0].get('threshold', '?')}\n")
            f.write(f"Total de pasos: {len(snaps)}\n\n")
            f.write("Pasos:\n")
            for i, snap in enumerate(snaps):
                tag = " [INTERMEDIO]" if snap.get("is_intermediate") else ""
                f.write(f"  {i + 1:3d}. [{snap['operation']:10s}]{tag} "
                        f"{snap['description']}\n")

        print(f"\n[EJEMPLO {ex_num}] {title}")
        print(f"  Generando {len(snaps)} imagenes...")

        for i, snap in enumerate(snaps):
            suffix = "_intermedio" if snap.get("is_intermediate") else ""
            filename = f"paso_{i + 1:03d}{suffix}"
            filepath = os.path.join(ex_dir, filename)

            render_snapshot(snap, filepath)
            total_images += 1

            if (i + 1) % 10 == 0 or i + 1 == len(snaps):
                print(f"  ... {i + 1}/{len(snaps)} imagenes generadas")

    # Indice general
    index_path = os.path.join(base_dir, "indice.txt")
    with open(index_path, "w", encoding="utf-8") as f:
        f.write("=" * 60 + "\n")
        f.write("  SOFT HEAP - Visualizaciones\n")
        f.write("=" * 60 + "\n\n")
        f.write("Estructura de carpetas:\n\n")
        for ex_num in sorted(examples.keys()):
            snaps = examples[ex_num]
            title = snaps[0].get("title", f"Ejemplo {ex_num}")
            eps = snaps[0].get("epsilon", "?")
            n_steps = len(snaps)
            n_inter = sum(1 for s in snaps if s.get("is_intermediate"))
            f.write(f"  ejemplo_{ex_num}/\n")
            f.write(f"    Titulo:  {title}\n")
            f.write(f"    Epsilon: {eps}\n")
            f.write(f"    Pasos:   {n_steps} ({n_inter} intermedios)\n\n")

        f.write(f"\nTotal de imagenes generadas: {total_images}\n")
        f.write("\nConvenciones de color:\n")
        f.write("  - Fondo BLANCO: paso final (estado estable)\n")
        f.write("  - Fondo CREMA:  paso intermedio (transicion)\n")
        f.write("  - Nodo AMARILLO: nodo involucrado en la operacion actual\n")
        f.write("  - Item VERDE:   item limpio (ckey == clave real)\n")
        f.write("  - Item ROJO:    item corrompido (ckey > clave real) [C]\n")
        f.write("  - Bordes PURPURA: transicion/movimiento\n")

    return total_images


# ===================== Main =====================

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.join(script_dir, "build")
    vis_dir = os.path.join(script_dir, "visualizations")

    print("=" * 60)
    print("  SOFT HEAP - Compilacion, Ejecucion y Visualizacion")
    print(f"  Plataforma: {platform.system()} ({platform.machine()})")
    print("=" * 60)

    cmake_file = os.path.join(script_dir, "CMakeLists.txt")
    if not os.path.isfile(cmake_file):
        print(f"\n[ERROR] No se encontro CMakeLists.txt en: {script_dir}")
        sys.exit(1)

    # Paso 1: Compilar con CMake
    bin_path = compile_cpp(script_dir, build_dir)
    if bin_path is None:
        sys.exit(1)

    # Paso 2: Ejecutar
    output = run_binary(bin_path)
    if output is None:
        sys.exit(1)

    # Paso 3: Parsear
    print("\n[PARSEANDO] Salida del programa...")
    snapshots = parse_snapshots(output)
    if not snapshots:
        print("[ERROR] No se obtuvieron snapshots.")
        sys.exit(1)

    # Paso 4: Generar visualizaciones
    print(f"\n[GENERANDO] Visualizaciones en: {vis_dir}")
    total = organize_and_render(snapshots, vis_dir)

    print("\n" + "=" * 60)
    print(f"  COMPLETADO: {total} imagenes generadas")
    print(f"  Directorio: {vis_dir}")
    print("=" * 60)


if __name__ == "__main__":
    main()
