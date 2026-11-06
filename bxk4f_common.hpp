// Shared core of the batch bxk4f programs (bxk4f_rseas, bxk4f_rmse): read one
// round's transformation table, grid-search every active pair, and print one
// result line per pair. Each program supplies only its score function and CLI.
#ifndef BXK4F_COMMON_HPP
#define BXK4F_COMMON_HPP

#include <bits/stdc++.h>
#include "bxk4f_disabled.hpp"

namespace bxk4f {

using namespace std;
using Vec = vector<double>;

// Support transformation IDs through 95, including the new modes 84..95.
constexpr int MAX_TRANSFORMATION = 95;

constexpr double LOG_FLOOR = -15.0;

inline double safe_log(double value) {
    return value > 0.0 ? log(value) : LOG_FLOOR;
}

constexpr double EPS = 1e-12;

struct Curve {
    double ka = 0.0;
    double kb = 0.0;
    Vec A;
    Vec B;

    explicit Curve(size_t dimension) : A(dimension, 0.0), B(dimension, 0.0) {}
};

inline Vec lerp(const Vec& p, const Vec& q, double u) {
    Vec result(p.size(), 0.0);
    for (size_t i = 0; i < p.size(); ++i) {
        result[i] = (1.0 - u) * p[i] + u * q[i];
    }
    return result;
}

inline Vec average(const Vec& a, const Vec& b) {
    Vec result(a.size(), 0.0);
    for (size_t i = 0; i < a.size(); ++i) {
        result[i] = 0.5 * (a[i] + b[i]);
    }
    return result;
}

inline Curve normalize_curve(Curve curve) {
    curve.ka = max(0.0, curve.ka);
    curve.kb = max(0.0, curve.kb);

    // Keep each measured displacement attached to the scale that produced it.
    if (curve.kb < curve.ka) {
        swap(curve.ka, curve.kb);
        swap(curve.A, curve.B);
    }

    // Avoid division by zero if both probes completed the same effective scale.
    if (curve.ka > 0.0 && abs(curve.kb - curve.ka) <= EPS) {
        curve.A = average(curve.A, curve.B);
        curve.B = curve.A;
    }

    return curve;
}

inline Vec displacement(double x, const Curve& curve) {
    const Vec zero(curve.A.size(), 0.0);
    if (x <= 0.0 || curve.kb <= 0.0) {
        return zero;
    }

    if (curve.ka <= 0.0) {
        return lerp(zero, curve.B, x / curve.kb);
    }

    if (x <= curve.ka) {
        return lerp(zero, curve.A, x / curve.ka);
    }

    if (curve.kb - curve.ka <= EPS) {
        return lerp(zero, curve.A, x / curve.ka);
    }

    // u > 1 deliberately extrapolates beyond the larger tested scale.
    const double u = (x - curve.ka) / (curve.kb - curve.ka);
    return lerp(curve.A, curve.B, u);
}

inline void add_range(vector<double>& grid, double first, double last, double step) {
    if (step <= 0.0 || last < first) return;
    for (double x = first; x <= last + EPS; x += step) {
        grid.push_back(x);
        if (x + step == x) throw runtime_error("grid scale is too large for its step");
    }
}

inline vector<double> build_grid(const Curve& first, const Curve& second) {
    vector<double> grid;

    const double upper =
        2.0 * min(max(first.ka, first.kb),
                  max(second.ka, second.kb));

    add_range(grid, 0.0, 600.0, 24.0);
    add_range(grid, 600.0, 6000.0, 240.0);
    add_range(grid, 6000.0, 60000.0, 2400.0);

    if (upper > 60000.0) {
        add_range(grid, 60000.0, upper, 12000.0);
    }

    grid.push_back(first.ka);
    grid.push_back(first.kb);
    grid.push_back(second.ka);
    grid.push_back(second.kb);

    grid.erase(remove_if(grid.begin(), grid.end(),
                         [&](double x) {
                             return x < -EPS || x > upper + EPS;
                         }),
               grid.end());

    sort(grid.begin(), grid.end());
    grid.erase(unique(grid.begin(), grid.end(), [](double a, double b) {
        return abs(a - b) <= EPS;
    }), grid.end());

    return grid;
}

// All pair metadata and probe curves are read once for a round. Edge changes
// come from rpll's ec table, so that table remains the single source of truth.
struct Transformation {
    int id = 0;
    int edge_change = 0;
    bool enabled = false;
    Curve curve;
    map<double, Vec> cached_displacements;

    explicit Transformation(size_t dimension) : curve(dimension) {}
};

inline int read_integer(istream& input, const string& label) {
    string token;
    if (!(input >> token)) throw runtime_error("missing " + label);
    try {
        size_t used = 0;
        int value = stoi(token, &used);
        if (used == token.size()) return value;
    } catch (const exception&) {}
    throw runtime_error("invalid integer for " + label + ": '" + token + "'");
}

inline double read_number(istream& input, const string& label) {
    double value = 0.0;
    if (!(input >> value) || !isfinite(value)) {
        throw runtime_error("expected a finite number for " + label);
    }
    return value;
}

inline void read_vector(istream& input, Vec& values, const string& label,
                 bool frequencies = false) {
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] = read_number(input, label + "[" + to_string(i) + "]");
        if (frequencies && values[i] < 0.0) {
            throw runtime_error(label + " contains a negative frequency");
        }
    }
}

inline vector<Transformation> read_batch(istream& input, Vec& initial, Vec& target,
                                         const bxk4f_disabled::Config& disabled) {
    string magic;
    if (!(input >> magic) || magic != "BXK4F_BATCH_V1") {
        throw runtime_error("expected BXK4F_BATCH_V1; rebuild rpll and both "
                            "bxk4f executables together for the batch interface");
    }
    const int count = read_integer(input, "transformation count");
    if (count < 1 || count > MAX_TRANSFORMATION) {
        throw runtime_error("transformation count must be in [1," +
                            to_string(MAX_TRANSFORMATION) + "]");
    }
    read_vector(input, initial, "current", true);
    read_vector(input, target, "target", true);

    array<bool, MAX_TRANSFORMATION + 1> seen{};
    vector<Transformation> transformations;
    transformations.reserve(static_cast<size_t>(count));
    for (int record = 0; record < count; ++record) {
        Transformation t(initial.size());
        t.id = read_integer(input, "transformation ID");
        if (t.id < 1 || t.id > MAX_TRANSFORMATION || seen[t.id]) {
            throw runtime_error("transformation IDs must be distinct and in [1," +
                                to_string(MAX_TRANSFORMATION) + "]");
        }
        seen[t.id] = true;
        t.edge_change = read_integer(input, "edge change for " + to_string(t.id));
        const int enabled = read_integer(input, "enabled flag for " + to_string(t.id));
        if (enabled != 0 && enabled != 1) {
            throw runtime_error("enabled flag must be 0 or 1");
        }
        t.enabled = enabled == 1;
        if (t.enabled) {
            const string label = "transformation " + to_string(t.id);
            t.curve.ka = read_number(input, label + " ka");
            t.curve.kb = read_number(input, label + " kb");
            read_vector(input, t.curve.A, label + " A");
            read_vector(input, t.curve.B, label + " B");
            t.curve = normalize_curve(move(t.curve));
            // Guard overflow before using the original grid construction.
            if (!isfinite(2.0 * t.curve.kb)) {
                throw runtime_error(label + " has an excessively large scale");
            }
        }
        // Read the complete probe curve before applying configuration: the
        // batch enabled flag determines which fields are present on stdin.
        if (disabled.transformations[t.id]) t.enabled = false;
        transformations.push_back(move(t));
    }
    input >> ws;
    if (!input.eof()) throw runtime_error("extra data after the transformation table");

    for (size_t i = 0; i < initial.size(); ++i) {
        initial[i] = safe_log(initial[i]);
        target[i] = safe_log(target[i]);
    }
    // Sort by ID so that write_round_results' (i, j <= i) loop puts the larger
    // ID first in each pair, preserving the old orientation. This matters for
    // the asymmetric opposite-edge-change grid.
    sort(transformations.begin(), transformations.end(),
         [](const Transformation& a, const Transformation& b) {
             return a.id < b.id;
         });
    return transformations;
}

inline const Vec& response_at(Transformation& t, double scale) {
    auto found = t.cached_displacements.find(scale);
    if (found != t.cached_displacements.end()) return found->second;
    return t.cached_displacements.emplace(scale, displacement(scale, t.curve))
        .first->second;
}

struct SearchResult {
    double x = 0.0;
    double y = 0.0;
    double score = numeric_limits<double>::infinity();
};

template <typename Score>
SearchResult minimize_pair(Transformation& first, Transformation& second,
                           double baseline_score, const Score& score) {
    SearchResult best{0.0, 0.0, baseline_score};
    const int ec1 = first.edge_change;
    const int ec2 = second.edge_change;
    const bool both_zero = ec1 == 0 && ec2 == 0;
    const bool opposite = (ec1 < 0 && ec2 > 0) || (ec1 > 0 && ec2 < 0);

    // Match the existing behavior: one-zero pairs and same-sign nonzero pairs
    // return the baseline at (0,0), without inventing a new feasible search.
    if (!both_zero && !opposite) return best;

    const vector<double> grid = build_grid(first.curve, second.curve);
    if (first.id == second.id) {
        // One transformation has one total dose and one modeled response.
        // Keep the ordinary per-transformation grid bound (2 * probe kb),
        // rather than doubling its range through two independent slots.
        const Vec zero(first.curve.A.size(), 0.0);
        for (double total : grid) {
            const double candidate = score(response_at(first, total), zero);
            if (candidate < best.score) best = {total, 0.0, candidate};
        }
    } else if (both_zero) {
        vector<const Vec*> first_values, second_values;
        first_values.reserve(grid.size());
        second_values.reserve(grid.size());
        for (double scale : grid) {
            first_values.push_back(&response_at(first, scale));
            second_values.push_back(&response_at(second, scale));
        }
        for (size_t i = 0; i < grid.size(); ++i) {
            for (size_t j = 0; j < grid.size(); ++j) {
                const double candidate = score(*first_values[i], *second_values[j]);
                if (candidate < best.score) {
                    best = {grid[i], grid[j], candidate};
                }
            }
        }
    } else {
        for (double x : grid) {
            // Deliberately retain the original derived-y rule, including its
            // original extrapolation range, rather than changing the search.
            const double y = abs(x * static_cast<double>(ec1) /
                                 static_cast<double>(ec2));
            if (!isfinite(y)) throw runtime_error("derived pair scale overflow");
            const double candidate = score(response_at(first, x), response_at(second, y));
            if (candidate < best.score) best = {x, y, candidate};
        }
    }
    return best;
}

inline string scale_text(double scale) {
    // Use the same rounding as the original fixed/setprecision(0) stdout.
    // BAD_ZERO_SCALE must test what rpll would have read, not the unrounded x.
    ostringstream output;
    output << fixed << setprecision(0) << scale;
    return output.str();
}

inline bool text_is_zero(const string& text) { return text == "0" || text == "-0"; }

// BXK_THREADS (default 1, capped at 4) splits the pair searches across
// threads. Pairs are independent, each search stays serial (so its strict
// first-minimum tie-break is unchanged) and results print in (i, j) order,
// so the output is byte-identical for any thread count.
constexpr int MAX_THREADS = 4;

inline int thread_count(size_t work_items) {
    const char* text = getenv("BXK_THREADS");
    int threads = text && *text ? atoi(text) : 1;
    threads = max(1, min(threads, MAX_THREADS));
    return static_cast<int>(min<size_t>(threads, max<size_t>(work_items, 1)));
}

template <typename Score>
void write_round_results(vector<Transformation>& transformations,
                         double baseline_score, const Score& score) {
    if (!isfinite(baseline_score)) throw runtime_error("nonfinite baseline score");
    ostringstream output;
    output << fixed << setprecision(17);
    size_t bad_pairs = 0, active_count = 0;
    for (const auto& t : transformations) if (t.enabled) ++active_count;

    vector<pair<size_t, size_t>> pair_list;
    for (size_t i = 0; i < transformations.size(); ++i) {
        if (!transformations[i].enabled) continue;
        for (size_t j = 0; j <= i; ++j) {
            if (transformations[j].enabled) pair_list.push_back({i, j});
        }
    }

    vector<SearchResult> results(pair_list.size());
    const int threads = thread_count(pair_list.size());
    // Searches pairs first, first + stride, ... A worker stops at its first
    // error; the lowest failing pair is rethrown, as the serial loop would.
    auto search = [&](size_t first, size_t stride, vector<Transformation>& table,
                      size_t& failed_at, exception_ptr& error) {
        for (size_t p = first; p < pair_list.size(); p += stride) {
            try {
                results[p] = minimize_pair(table[pair_list[p].first],
                                           table[pair_list[p].second],
                                           baseline_score, score);
            } catch (...) {
                failed_at = p;
                error = current_exception();
                return;
            }
        }
    };
    vector<size_t> failed_at(threads, numeric_limits<size_t>::max());
    vector<exception_ptr> errors(threads);
    bool searched = false;
    if (threads > 1) {
        // Each thread fills a private displacement cache; displacement() is
        // pure, so cached values match the serial run exactly.
        vector<vector<Transformation>> tables(threads - 1, transformations);
        vector<thread> pool;
        try {
            for (int t = 1; t < threads; ++t) {
                pool.emplace_back(search, t, threads, ref(tables[t - 1]),
                                  ref(failed_at[t]), ref(errors[t]));
            }
            search(0, threads, transformations, failed_at[0], errors[0]);
            searched = true;
        } catch (const system_error&) {
            // A thread could not be started: fall back to the serial search
            // below, which rewrites every result slot.
        }
        for (auto& worker : pool) worker.join();
    }
    if (!searched) {
        fill(failed_at.begin(), failed_at.end(), numeric_limits<size_t>::max());
        fill(errors.begin(), errors.end(), nullptr);
        search(0, 1, transformations, failed_at[0], errors[0]);
    }
    const auto first_failure = min_element(failed_at.begin(), failed_at.end());
    if (*first_failure != numeric_limits<size_t>::max()) {
        rethrow_exception(errors[first_failure - failed_at.begin()]);
    }

    for (size_t p = 0; p < pair_list.size(); ++p) {
        const auto& first = transformations[pair_list[p].first];
        const auto& second = transformations[pair_list[p].second];
        const SearchResult& best = results[p];
        const string x = scale_text(best.x);
        const string y = scale_text(best.y);
        if (first.id != second.id && (text_is_zero(x) || text_is_zero(y))) {
            ++bad_pairs;
            output << "BAD_ZERO_SCALE|" << first.id << '|' << second.id
                   << '|' << x << '|' << y << '|' << best.score << '\n';
        } else {
            output << best.score << '|' << first.id << '|' << second.id
                   << '|' << x << '|' << y << '\n';
        }
    }
    const size_t pairs = pair_list.size();
    // No partial result table is published if parsing/scoring fails.
    cout << output.str();
    cout.flush();
    if (!cout) throw runtime_error("failed to write round_results output");
    cerr << "Batch bxk4f: " << active_count << " active transformations, "
         << pairs << " pairs scored, " << bad_pairs << " mixed zero-scale pairs marked bad.\n";
}
inline int parse_k(const char* text) {
    const string input(text);
    size_t used = 0;
    int k = stoi(input, &used);
    if (used != input.size() || (k != 4 && k != 5)) {
        throw runtime_error("k must be 4 or 5");
    }
    return k;
}

}  // namespace bxk4f

#endif  // BXK4F_COMMON_HPP
