// Batch bxk4f, RMSE objective: one invocation evaluates every active pair.
// The shared batch reader and pair search live in bxk4f_common.hpp.
#include "bxk4f_common.hpp"

using namespace bxk4f;

namespace {

double score_displacements(const Vec& target, const Vec& initial,
                            const Vec& v1, const Vec& v2) {
    double sum = 0.0;
    for (size_t i = 0; i < target.size(); ++i) {
        const double predicted = initial[i] - v1[i] - v2[i];
        const double error = predicted - target[i];
        sum += error * error;
    }
    return sqrt(sum);
}

}  // namespace

int main(int argc, char* argv[]) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    if (argc != 2 && argc != 3) {
        cerr << "Usage: " << argv[0]
             << " <k:4|5> [disabled_file] < batch_input.txt > round_results.tmp\n";
        return 1;
    }
    try {
        const int k = parse_k(argv[1]);
        const auto disabled = bxk4f_disabled::load(argc == 3 ? argv[2] : "disabled.txt");
        const size_t dimension = k == 4 ? 6 : 21;
        Vec initial(dimension), target(dimension), zero(dimension, 0.0);
        auto transformations = read_batch(cin, initial, target, disabled);
        const auto score = [&](const Vec& a, const Vec& b) {
            return score_displacements(target, initial, a, b);
        };
        write_round_results(transformations, score(zero, zero), score);
    } catch (const exception& error) {
        cerr << "bxk4f RMSE: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
