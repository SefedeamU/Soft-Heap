# Soft Heap — Implementación en C++

**Curso:** Estructura de Datos Avanzados  
**Integrantes:**
- Ariana Mercado
- Sergio Delgado
- Jhon Chilo

---

## 1. Introducción

El presente proyecto implementa un **Soft Heap**, una estructura de datos de cola de prioridad aproximada propuesta originalmente por Bernard Chazelle en el año 2000. Para nuestra implementación nos basamos en el diseño presentado por **Kaplan y Zwick (2009)** en su paper *"A simpler implementation and analysis of Chazelle's Soft Heaps"*, el cual ofrece una formulación más clara y un análisis simplificado respecto al diseño original.

Un Soft Heap se diferencia de un heap clásico en que permite que una fracción controlada de las claves se **corrompan** (se incrementen artificialmente), a cambio de obtener tiempos amortizados significativamente mejores en las operaciones de inserción y fusión.

## 2. Fundamento teórico

### 2.1 Concepto de corrupción

En un heap convencional, cada elemento conserva su clave exacta en todo momento. En un Soft Heap, algunos elementos pueden recibir una **clave corrompida (ckey)** mayor a su clave real. El heap opera con estas claves corrompidas, lo que puede provocar que ciertos elementos se extraigan en un orden ligeramente distinto al ideal.

El parámetro **ε ∈ (0, 1]** controla la tasa máxima de corrupción: en todo momento, a lo sumo **ε·n** de los n elementos presentes pueden estar corrompidos.

### 2.2 Umbral T

El umbral se define como:

```
T = ⌈log₂(1/ε)⌉
```

Este valor determina a partir de qué rango los nodos realizan *double sift* (dos rondas de recolección de items), que es el mecanismo que introduce la corrupción. Los nodos con rango ≤ T realizan una sola ronda de sift y no corrompen. Los nodos con rango > T realizan dos rondas, y en la segunda ronda los items recolectados previamente quedan bajo una ckey potencialmente mayor, corrompiendo sus claves.

### 2.3 Complejidades

| Operación   | Complejidad amortizada       |
|-------------|------------------------------|
| `insert`    | O(1) (para ε constante)      |
| `deleteMin` | O(log(1/ε))                  |
| `meld`      | O(1)                         |
| `findMin`   | O(1) peor caso               |

## 3. Diseño de la implementación

### 3.1 Estructuras de datos

Nuestra implementación se compone de las siguientes estructuras:

- **Item:** Representa un elemento almacenado. Contiene la clave original (`key`) que nunca se modifica y un identificador único (`id`).

- **Node:** Nodo del árbol binario. Cada nodo almacena una clave corrompida (`ckey`), un rango (`rank`), punteros a sus hijos izquierdo y derecho, y una lista de items. Los nodos hoja se crean con un solo item durante la inserción. Los nodos internos se generan al combinar dos árboles mediante la operación `link`.

- **HeadNode:** Nodo cabecera que envuelve la raíz de cada árbol en la lista de raíces. Mantiene enlaces `prev`/`next` para la lista doblemente enlazada y un puntero `sufmin` que apunta al HeadNode con menor `ckey` desde esa posición hasta el final de la lista, lo que permite localizar el mínimo global en O(1).

- **SoftHeap:** Clase principal que gestiona la lista de raíces y expone las operaciones públicas. Almacena el parámetro ε, el umbral T calculado, y punteros a la cabeza y cola de la lista de raíces.

### 3.2 Operaciones internas

#### Sift

La operación `sift` es el mecanismo central de la estructura. Cuando un nodo queda sin items (ya sea tras un `link` o un `deleteMin`), `sift` repone su lista de items tomándolos de sus hijos:

1. **Ronda 1:** Se selecciona el hijo con menor `ckey`, se transfieren todos sus items al nodo padre, y el padre adopta esa `ckey`. Si el hijo era hoja y quedó vacío, se elimina.

2. **Ronda 2 (solo si rank > T):** Se repite el proceso con el otro hijo. Al actualizar la `ckey` del padre con la del segundo hijo (potencialmente mayor), los items de la ronda 1 quedan bajo una clave mayor a su clave real, quedando corrompidos.

De esta manera, la corrupción no es un paso explícito sino una consecuencia natural de agrupar items bajo una clave que no les corresponde.

#### Link

Combina dos árboles del mismo rango r creando un nuevo nodo raíz de rango r+1 con ambos como hijos. Inmediatamente se invoca `sift` sobre el nuevo nodo para poblarlo de items y asignarle una `ckey` válida.

#### Repeated Combine

Después de cada inserción, recorre la lista de raíces combinando árboles adyacentes del mismo rango mediante `link`, de forma análoga a la propagación de acarreo en suma binaria.

#### Update Sufmin

Recorre la lista de raíces de derecha a izquierda, calculando para cada HeadNode el sufijo mínimo. Esto permite que `findMin` opere en O(1) simplemente consultando `head->sufmin`.

### 3.3 Operaciones públicas

- **insert(key):** Crea un nodo hoja con un solo item, lo inserta al inicio de la lista de raíces, y ejecuta `repeatedCombine` para consolidar árboles del mismo rango.

- **findMin():** Retorna la ckey y la clave real del item con menor ckey, accediendo directamente a través del puntero `sufmin` del HeadNode cabecera.

- **deleteMin():** Localiza el árbol con menor `ckey` mediante `sufmin`, extrae el primer item de la raíz, y si la raíz queda vacía ejecuta `sift` para reponerla (o elimina el árbol si es hoja).

- **meld(other):** Fusiona otro Soft Heap intercalando ambas listas de raíces por rango y luego combinando árboles del mismo rango. El heap fuente queda vacío tras la operación.

## 4. Sistema de visualización

Para facilitar la comprensión del comportamiento interno, implementamos un sistema de captura de snapshots y visualización:

- **Snapshot.h:** Módulo de captura que recorre la estructura interna del heap y serializa su estado completo a formato NDJSON (una línea JSON por snapshot). Registra metadatos como el número de items totales, items corrompidos, tipo de operación, y datos de highlighting para la visualización.

- **Observador:** La clase `SoftHeap` permite registrar un callback opcional que se invoca en puntos clave de las operaciones internas (pre/post sift, link, combine). Esto permite capturar estados intermedios sin modificar la lógica del heap.

- **main.cpp:** Contiene cinco ejemplos demostrativos que resaltan diferentes aspectos de la estructura:
  1. Estructura del árbol — cómo insert y link construyen la jerarquía.
  2. Mecanismo de corrupción — el double sift en acción.
  3. Operación meld — fusión de dos heaps.
  4. Tasa de corrupción acotada — verificación de la garantía ε·n.
  5. Ordenamiento aproximado — extracción casi ordenada como efecto práctico.

## 5. Compilación y ejecución

```bash
cd build
cmake ..
make
./softheap
```

La salida se genera en formato NDJSON, una línea JSON por snapshot, que puede ser procesada por herramientas de visualización.

## 6. Estructura del proyecto

```
├── SoftHeap.h       # Implementación del Soft Heap
├── Snapshot.h        # Captura y serialización de estados
├── main.cpp          # Ejemplos demostrativos
├── CMakeLists.txt    # Configuración de compilación
├── build/            # Directorio de compilación
└── visualizations/   # Salida de visualizaciones
```

## 7. Referencias

- Chazelle, B. (2000). *The Soft Heap: An Approximate Priority Queue with Optimal Error Rate*. Journal of the ACM, 47(6), 1012–1027.
- Kaplan, H. & Zwick, U. (2009). *A simpler implementation and analysis of Chazelle's Soft Heaps*. Proceedings of the 19th Annual ACM-SIAM Symposium on Discrete Algorithms (SODA), 477–485.
