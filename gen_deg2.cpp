#include <bits/stdc++.h>
using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " input.el output.el\n";
        return 1;
    }

    string inputFile = argv[1];
    string outputFile = argv[2];

    ifstream fin(inputFile);
    if (!fin) {
        cerr << "Could not open input file.\n";
        return 1;
    }

    vector<pair<int,int>> edges;
    unordered_set<int> nodeSet;

    int u, v;
    while (fin >> u >> v) {
        if (u == v) continue;
        edges.push_back({u, v});
        nodeSet.insert(u);
        nodeSet.insert(v);
    }
    fin.close();

    vector<int> nodes(nodeSet.begin(), nodeSet.end());
    sort(nodes.begin(), nodes.end());

    unordered_map<int,int> id;
    for (int i = 0; i < (int)nodes.size(); i++) {
        id[nodes[i]] = i;
    }

    int n = nodes.size();
    if (n == 0) {
        cerr << "Empty graph.\n";
        return 1;
    }

    vector<int> deg(n, 0);

    for (auto &e : edges) {
        int a = id[e.first];
        int b = id[e.second];
        deg[a]++;
        deg[b]++;
    }

    for (int i = 0; i < n; i++) {
        if (deg[i] == 0 && n > 1) {
            cerr << "Node " << nodes[i]
                 << " is disconnected.\n";
            return 1;
        }
    }

    // GEN_DEG_SEED fixes the RNG for reproducible tests; unset keeps the clock.
    const char* seed_text = getenv("GEN_DEG_SEED");
    mt19937 rng(seed_text && *seed_text
                    ? static_cast<mt19937::result_type>(strtoull(seed_text, nullptr, 10))
                    : static_cast<mt19937::result_type>(
                          chrono::steady_clock::now().time_since_epoch().count()));

    vector<vector<int>> adj(n);
    vector<int> currDeg(n, 0);

    // Build a spanning tree. A random ordering/choice can trap the process by
    // saturating every currently connected node too early, so restart from a
    // completely empty generated graph when that happens.
    constexpr int MAX_TREE_ATTEMPTS = 50;
    bool treeBuilt = false;

    for (int attempt = 1; attempt <= MAX_TREE_ATTEMPTS; attempt++) {
        adj.assign(n, {});
        currDeg.assign(n, 0);

        vector<int> perm(n);
        iota(perm.begin(), perm.end(), 0);
        shuffle(perm.begin(), perm.end(), rng);

        // The candidates are the connected nodes with spare degree, in the
        // order they joined. A Fenwick tree over join positions picks the
        // same k-th candidate the old rebuilt-per-step list did, in O(log n).
        vector<int> connected;
        vector<int> fenwick(n + 1, 0);
        size_t open_count = 0;
        auto fenwick_add = [&](int pos, int delta) {
            for (int i = pos + 1; i <= n; i += i & -i) fenwick[i] += delta;
        };
        int top_step = 1;
        while (top_step * 2 <= n) top_step *= 2;
        auto fenwick_kth = [&](size_t k) { // 0-based k-th open position
            int pos = 0;
            for (int step = top_step; step; step >>= 1) {
                if (pos + step <= n && (size_t)fenwick[pos + step] <= k) {
                    pos += step;
                    k -= fenwick[pos];
                }
            }
            return pos;
        };
        vector<int> join_pos(n, -1);
        auto join = [&](int u) {
            join_pos[u] = (int)connected.size();
            connected.push_back(u);
            if (currDeg[u] < deg[u]) {
                fenwick_add(join_pos[u], 1);
                ++open_count;
            }
        };
        join(perm[0]);

        bool attemptFailed = false;

        for (int idx = 1; idx < n; idx++) {
            int u = perm[idx];

            if (open_count == 0) {
                attemptFailed = true;
                break;
            }

            int v = connected[fenwick_kth(rng() % open_count)];

            adj[u].push_back(v);
            adj[v].push_back(u);

            currDeg[u]++;
            currDeg[v]++;
            if (currDeg[v] == deg[v]) {
                fenwick_add(join_pos[v], -1);
                --open_count;
            }

            join(u);
        }

        if (!attemptFailed) {
            treeBuilt = true;
            break;
        }
    }

    if (!treeBuilt) {
        cerr << "Too hard to build spanning tree.\n";
        return 1;
    }

    // Remaining degree
    vector<int> rem(n);

    long long totalRem = 0;

    for (int i = 0; i < n; i++) {
        rem[i] = deg[i] - currDeg[i];

        if (rem[i] < 0) {
            cerr << "Negative remaining degree at node "
                 << nodes[i] << "\n";
            return 1;
        }

        totalRem += rem[i];
    }


    vector<pair<int,int>> existingEdges;
    // treeUpper[i]: tree neighbours j > i, ascending, for a merge-walk below.
    vector<vector<int>> treeUpper(n);

    for (int i = 0; i < n; i++) {
        for (int j : adj[i]) {
            if (i < j) {
                existingEdges.push_back({i, j});
                treeUpper[i].push_back(j);
            }
        }
        sort(treeUpper[i].begin(), treeUpper[i].end());
    }

    uniform_real_distribution<double> dist(0.0, 1.0);

    // Only pairs where both nodes still have spare degree draw a random
    // number, and rem is fixed during the loop, so visiting just those pairs
    // in the same (i, j) order keeps the RNG sequence identical. Each pair is
    // visited once, so only tree edges need the membership test.
    if (totalRem > 0) {
        vector<int> open;
        for (int i = 0; i < n; i++) {
            if (rem[i] != 0) open.push_back(i);
        }
        for (size_t a = 0; a < open.size(); a++) {
            const int i = open[a];
            auto tree_it = treeUpper[i].begin();
            const auto tree_end = treeUpper[i].end();
            for (size_t b = a + 1; b < open.size(); b++) {
                const int j = open[b];

                while (tree_it != tree_end && *tree_it < j) ++tree_it;
                if (tree_it != tree_end && *tree_it == j)
                    continue;

                double p = (1.0 * rem[i] * rem[j]) / totalRem;
                p = min(1.0, p);

                if (dist(rng) < p) {
                    existingEdges.push_back({i, j});
                }
            }
        }
    }

    // Output graph, ordered by (i, j) like the old std::set.
    sort(existingEdges.begin(), existingEdges.end());
    ofstream fout(outputFile);

    if (!fout) {
        cerr << "Could not open output file.\n";
        return 1;
    }

    for (auto &e : existingEdges) {
        fout << nodes[e.first] << " " << nodes[e.second] << "\n";
    }

    fout.close();

    return 0;
}
