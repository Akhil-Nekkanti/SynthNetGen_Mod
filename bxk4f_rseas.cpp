// Batch bxk4f, RSEAS objective: one invocation evaluates every active pair.
// The shared batch reader and pair search live in bxk4f_common.hpp.
#include "bxk4f_common.hpp"
#include "rseas_metric_k45.hpp"

using namespace bxk4f;

namespace {

double score_displacements(const Vec& target, const Vec& initial,
                            const Vec& v1, const Vec& v2,
                            const rseas_metric::Config& config) {
    // Same two-branch RSEAS formula as rpll. At most 21 coordinates are used,
    // so no allocation is needed in the inner grid loop.
    array<double, 21> distances{};
    for (size_t i = 0; i < target.size(); ++i) {
        const double predicted = initial[i] - v1[i] - v2[i];
        distances[i] = abs(predicted - target[i]);
    }
    return rseas_metric::score_distances(distances.data(), target.size(), config);
}

double parse_number(const char* text) {
    const string input(text);
    size_t used = 0;
    double value = stod(input, &used);
    if (used != input.size() || !isfinite(value)) {
        throw runtime_error("invalid numeric argument: '" + input + "'");
    }
    return value;
}

}  // namespace

int main(int argc, char* argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    if (argc < 2) {
        cerr << "Usage: " << argv[0]
             << " <k:4|5> <e> <w1> ... <wD> [disabled_file]"
             << " < batch_input.txt > round_results.tmp\n";
        return 1;
    }
    try {
        const int k = parse_k(argv[1]);
        const size_t dimension = k == 4 ? 6 : 21;
        const int required_arguments = static_cast<int>(dimension) + 3;
        if (argc != required_arguments && argc != required_arguments + 1) {
            throw runtime_error("expected k, e, and exactly " +
                                to_string(dimension) + " weights, with an optional disabled file path");
        }
        const auto disabled = bxk4f_disabled::load(
            argc == required_arguments + 1 ? argv[required_arguments] : "disabled.txt");
        rseas_metric::Config config;
        config.error_range = parse_number(argv[2]);
        if (config.error_range < 0.0) throw runtime_error("e must be nonnegative");
        config.weights.resize(dimension);
        for (size_t i = 0; i < dimension; ++i) {
            config.weights[i] = parse_number(argv[3 + i]);
            if (config.weights[i] <= 0.0) throw runtime_error("weights must be positive");
        }
        Vec initial(dimension), target(dimension), zero(dimension, 0.0);
        auto transformations = read_batch(cin, initial, target, disabled);
        const auto score = [&](const Vec& a, const Vec& b) {
            return score_displacements(target, initial, a, b, config);
        };
        write_round_results(transformations, score(zero, zero), score);
    } catch (const exception& error) {
        cerr << "bxk4f RSEAS: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
