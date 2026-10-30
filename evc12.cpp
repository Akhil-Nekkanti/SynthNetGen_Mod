// evc1 / evc2: repeatedly pick an edge a-b and redistribute the two nodes'
// private neighbours by degree, which moves the largest eigenvalue.
// build.sh compiles this file twice: evc1 sorts the private neighbours by
// ascending degree (default) and evc2, built with -DEVC_DESC, by descending
// degree. Node a keeps the highest- (evc1) or lowest- (evc2) degree ones.
#include <bits/stdc++.h>
using namespace std;

// Degrees are fixed when the graph is read; cmp deliberately keeps using them.
vector<int> deg;

bool cmp(int a, int b) {
#ifdef EVC_DESC
    return deg[a] > deg[b];
#else
    return deg[a] < deg[b];
#endif
}

// EVC_SEED fixes the RNG for reproducible tests; unset keeps random_device.
unsigned evc_seed() {
    const char* text = getenv("EVC_SEED");
    if (text && *text) return static_cast<unsigned>(strtoul(text, nullptr, 10));
    random_device rd;
    return rd();
}

struct Rebuilt {
    vector<int> newA, newB;         // final neighbourhoods (without a-b)
    vector<int> onlyA, onlyB;       // private neighbours before the move
    vector<int> newOnlyA, newOnlyB; // private neighbours after the move
};

Rebuilt rebuildSets(const vector<int>& A, const vector<int>& B) {
    unordered_set<int> setA(A.begin(), A.end());
    unordered_set<int> setB(B.begin(), B.end());
    Rebuilt r;

    vector<int> common;
    for (int x : A) {
        if (setB.count(x)) common.push_back(x);
        else r.onlyA.push_back(x);
    }
    for (int x : B) {
        if (!setA.count(x)) r.onlyB.push_back(x);
    }

    const size_t cntA = r.onlyA.size();

    vector<int> combined = r.onlyA;
    combined.insert(combined.end(), r.onlyB.begin(), r.onlyB.end());
    sort(combined.begin(), combined.end(), cmp);

    r.newOnlyA.assign(combined.end() - cntA, combined.end());
    r.newOnlyB.assign(combined.begin(), combined.end() - cntA);

    r.newA = r.newOnlyA;
    r.newB = r.newOnlyB;
    r.newA.insert(r.newA.end(), common.begin(), common.end());
    r.newB.insert(r.newB.end(), common.begin(), common.end());
    return r;
}

void erase_value(vector<int>& v, int x) {
    v.erase(remove(v.begin(), v.end(), x), v.end());
}

void sample_edge_prob(vector<vector<int>>& adj, int num_samples) {
    int n = (int)adj.size();
    if (n < 4) {
        cerr << "Graph must have at least 4 nodes.\n";
        return;
    }

    mt19937 gen(evc_seed());
    uniform_int_distribution<int> node_dist(0, n - 1);

    for (int sample = 0; sample < num_samples; sample++) {
        int a = node_dist(gen);
        uniform_int_distribution<int> neighbor_dist(0, (int)adj[a].size() - 1);
        int b = adj[a][neighbor_dist(gen)];

        erase_value(adj[a], b);
        erase_value(adj[b], a);

        if (adj[a].size() < adj[b].size()) {
            swap(a, b);
        }

        Rebuilt r = rebuildSets(adj[a], adj[b]);

        for (int x : r.onlyA) erase_value(adj[x], a);
        for (int x : r.onlyB) erase_value(adj[x], b);
        for (int x : r.newOnlyA) adj[x].push_back(a);
        for (int x : r.newOnlyB) adj[x].push_back(b);

        adj[a] = move(r.newA);
        adj[b] = move(r.newB);
        adj[a].push_back(b);
        adj[b].push_back(a);
    }
}

void readGraph(const string& filename, vector<vector<int>>& adj) {
    ifstream file(filename);

    if (!file.is_open()) {
        cerr << "Error: cannot open file '" << filename << "'\n";
        exit(1);
    }

    unordered_map<string, int> nodeMap;
    vector<pair<int, int>> edges;

    string line, u, v;
    int nodeCount = 0;

    while (getline(file, line)) {
        istringstream iss(line);

        if (!(iss >> u >> v))
            continue;

        auto ia = nodeMap.try_emplace(u, nodeCount);
        if (ia.second) nodeCount++;
        auto ib = nodeMap.try_emplace(v, nodeCount);
        if (ib.second) nodeCount++;

        int a = ia.first->second;
        int b = ib.first->second;

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
        cerr << "Usage: " << argv[0] << " <input_file1> <num_samples>\n";
        return 1;
    }

    vector<vector<int>> adj;
    readGraph(argv[1], adj);

    int num_samples = stoi(argv[2]);
    sample_edge_prob(adj, num_samples);

    for (int i = 0; i < (int)adj.size(); i++) {
        for (int v : adj[i]) {
            if (i < v) cout << i << " " << v << "\n";
        }
    }
    return 0;
}
