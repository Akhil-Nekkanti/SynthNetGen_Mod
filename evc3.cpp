#include <bits/stdc++.h>
using namespace std;

vector<int> deg;

// Remove exactly one occurrence of x from v.
bool erase_one(vector<int>& v, int x) {
    auto it = find(v.begin(), v.end(), x);
    if (it == v.end()) return false;
    v.erase(it);
    return true;
}

bool has_edge(const vector<vector<int>>& adj, int u, int v) {
    return find(adj[u].begin(), adj[u].end(), v) != adj[u].end();
}

void remove_edge(vector<vector<int>>& adj, int u, int v) {
    bool a = erase_one(adj[u], v);
    bool b = erase_one(adj[v], u);

    if (a != b) {
        cerr << "Warning: asymmetric edge " << u << " " << v << "\n";
    }

    // Always refresh degrees immediately.
    deg[u] = (int)adj[u].size();
    deg[v] = (int)adj[v].size();
}

void add_edge(vector<vector<int>>& adj, int u, int v) {
    if (u == v) return;

    if (!has_edge(adj, u, v)) {
        adj[u].push_back(v);
        adj[v].push_back(u);
    }

    // Always refresh degrees immediately.
    deg[u] = (int)adj[u].size();
    deg[v] = (int)adj[v].size();
}

/*
    Assume A-B is currently an edge.

    Ignoring the A-B edge itself:

        new N(A) = old N(A) AND old N(B)
        new N(B) = old N(A) OR  old N(B)

    In other words:

        common:   stays connected to both
        A-only:   moves from A to B
        B-only:   stays connected to B
*/
void and_or_transform(vector<vector<int>>& adj, int A, int B) {
    // Remove A-B so A and B themselves do not pollute
    // the neighborhood intersection/union.
    remove_edge(adj, A, B);

    // Snapshot before changing anything else.
    vector<int> oldA = adj[A];

    unordered_set<int> setB;
    setB.reserve(adj[B].size() * 2 + 1);

    for (int x : adj[B]) {
        setB.insert(x);
    }

    // Anything connected to A but not B is A-only.
    // For:
    //
    // A := A AND B
    // B := A OR B
    //
    // move every such edge A-x to B-x.
    vector<int> moved;
    for (int x : oldA) {
        if (!setB.count(x)) moved.push_back(x);
    }
    if (!moved.empty()) {
        // One order-preserving pass over A instead of one linear erase per
        // moved neighbour. Adjacency order must match the old per-edge
        // updates exactly because later samples index into it. No moved x is
        // already adjacent to B (x is not in setB), so each B-x is new.
        unordered_set<int> movedSet(moved.begin(), moved.end());
        adj[A].erase(remove_if(adj[A].begin(), adj[A].end(),
                               [&](int x) { return movedSet.count(x) != 0; }),
                     adj[A].end());
        for (int x : moved) {
            erase_one(adj[x], A);
            adj[B].push_back(x);
            adj[x].push_back(B);
            deg[x] = (int)adj[x].size();
        }
        deg[A] = (int)adj[A].size();
        deg[B] = (int)adj[B].size();
    }

    // Restore the sampled A-B edge.
    add_edge(adj, A, B);
}

// EVC_SEED fixes the RNG for reproducible tests; unset keeps random_device.
unsigned evc_seed() {
    const char* text = getenv("EVC_SEED");
    if (text && *text) return static_cast<unsigned>(strtoul(text, nullptr, 10));
    random_device rd;
    return rd();
}

void sample_edge_prob(vector<vector<int>>& adj, int num_samples) {
    int n = (int)adj.size();

    if (n < 2) {
        cerr << "Graph must have at least 2 nodes.\n";
        return;
    }

    mt19937 gen(evc_seed());

    uniform_int_distribution<int> node_dist(0, n - 1);

    for (int sample = 0; sample < num_samples; sample++) {

        // Find a node having at least one neighbor.
        // This avoids crashing if the graph contains isolated nodes.
        int A = -1;

        for (int attempt = 0; attempt < 100; attempt++) {
            int x = node_dist(gen);

            if (!adj[x].empty()) {
                A = x;
                break;
            }
        }

        if (A == -1) {
            // Extremely sparse graph: do a deterministic fallback.
            for (int x = 0; x < n; x++) {
                if (!adj[x].empty()) {
                    A = x;
                    break;
                }
            }
        }

        if (A == -1) {
            // Graph has no edges.
            return;
        }

        uniform_int_distribution<int> neighbor_dist(
            0, (int)adj[A].size() - 1
        );

        int B = adj[A][neighbor_dist(gen)];

        and_or_transform(adj, A, B);
    }
}

void readGraph(const string& filename, vector<vector<int>>& adj) {
    ifstream file(filename);

    if (!file.is_open()) {
        cerr << "Error: cannot open file '" << filename << "'\n";
        exit(1);
    }

    map<string, int> nodeMap;
    vector<pair<int, int>> edges;

    string line, u, v;
    int nodeCount = 0;

    while (getline(file, line)) {
        istringstream iss(line);

        if (!(iss >> u >> v))
            continue;

        if (!nodeMap.count(u))
            nodeMap[u] = nodeCount++;

        if (!nodeMap.count(v))
            nodeMap[v] = nodeCount++;

        int a = nodeMap[u];
        int b = nodeMap[v];

        if (a != b)
            edges.push_back({a, b});
    }

    adj.assign(nodeCount, {});

    // Avoid duplicate edges.
    vector<unordered_set<int>> seen(nodeCount);

    for (auto [a, b] : edges) {
        if (seen[a].insert(b).second) {
            seen[b].insert(a);

            adj[a].push_back(b);
            adj[b].push_back(a);
        }
    }

    deg.resize(nodeCount);

    for (int i = 0; i < nodeCount; i++) {
        deg[i] = (int)adj[i].size();
    }
}

int main(int argc, char* argv[]) {
    ios::sync_with_stdio(false);
    if (argc < 3) {
        cerr << "Usage: "
             << argv[0]
             << " <input_file> <num_samples>\n";

        return 1;
    }

    vector<vector<int>> adj;
    readGraph(argv[1], adj);

    int num_samples = stoi(argv[2]);

    sample_edge_prob(adj, num_samples);

    for (int i = 0; i < (int)adj.size(); i++) {
        for (int v : adj[i]) {
            if (i < v) {
                cout << i << " " << v << "\n";
            }
        }
    }

    return 0;
}
