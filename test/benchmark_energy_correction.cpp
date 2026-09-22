// Reproducible serial benchmark: FCIDUMP, iterations, correction interval,
// reference energy, output JSON. Times include solver allocation and iteration;
// FCIDUMP loading is excluded. History is off during paired timing runs.
#include "../include/solver.h"
#include <fstream>
#include <sstream>

int main(int argc, char **argv)
{
    if (argc != 6 && argc != 7 && argc != 8) {
        std::cerr << "Usage: benchmark_energy_correction FCIDUMP iterations interval reference_energy output.json [trajectory-only [z_threshold]]\n";
        return 1;
    }
    const bool trajectory_only = argc >= 7 && std::string(argv[6]) == "trajectory-only";
    if (argc >= 7 && !trajectory_only) {
        std::cerr << "The optional mode must be trajectory-only\n";
        return 1;
    }
    using Det = Determinant<1>;
    using Container = ContainerRobinhood<Det, std::array<double, 2>, DeterminantHash<1>, DeterminantEqual<1>>;
    using Wf = WaveFunction<Container>;
    using Ham = Hamiltonian<1>;
    Det::constuct_masks();
    const size_t iterations = std::stoull(argv[2]), interval = std::stoull(argv[3]);
    const double reference = std::stod(argv[4]);
    const double z_threshold = argc == 8 ? std::stod(argv[7]) : 1e-3;
    Option hopt = {{"type", "molecule"}, {"molecule", {{"fcidump_path", argv[1]}, {"verbose", 0}}}};
    auto ham = Ham::init(hopt);
    Option opt = {{"num_iterations", iterations}, {"report_interval", 1},
                  {"num_coordinates", 1}, {"z_threshold", z_threshold}, {"stopping_dx_threshold", 0},
                  {"max_wavefunction_size", 524288}, {"verbose", 0}};
    std::ostringstream muted;
    auto original = std::cout.rdbuf(muted.rdbuf());
    auto run = [&](bool enabled, bool history, size_t steps) {
        opt["num_iterations"] = steps;
        opt["energy_correction"] = {{"enabled", enabled}, {"interval", interval}, {"store_history", history}};
        CDFCISolver<Ham, Wf> solver(opt);
        Wf wf;
        const auto start = std::chrono::steady_clock::now();
        solver.solve(*ham, wf);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        auto result = solver.get_result();
        Option summary = {{"seconds", seconds}, {"energy", result.energy},
                          {"stored_determinants", wf.size()}, {"coefficient_bytes_per_entry", sizeof(typename Wf::mapped_type)},
                          {"correction_seconds", result.energy_correction_seconds},
                          {"correction_evaluations", result.energy_correction_evaluations}};
        return std::make_pair(result, summary);
    };
    Option runs = Option::array();
    if (!trajectory_only) {
        run(false, false, std::min<size_t>(iterations, 200)); // warm caches
        for (int repeat = 0; repeat < 5; ++repeat) {
            std::pair<Result, Option> plain, pe;
            if (repeat % 2 == 0) { plain = run(false, false, iterations); pe = run(true, false, iterations); }
            else { pe = run(true, false, iterations); plain = run(false, false, iterations); }
            runs.push_back({{"baseline", plain.second}, {"enabled", pe.second}});
        }
    }
    auto trajectory = run(true, true, iterations);
    std::cout.rdbuf(original);
    Option records = Option::array();
    for (const auto &pe : trajectory.first.energy_correction_history) {
        records.push_back({{"iteration", pe.iteration}, {"variational_energy", pe.variational_energy},
                           {"internal_correction", pe.internal_correction}, {"external_correction", pe.external_correction},
                           {"corrected_energy", pe.corrected_energy}, {"status", pe.status},
                           {"raw_error", std::abs(pe.variational_energy - reference)},
                           {"external_only_error", std::abs(pe.variational_energy + pe.external_correction - reference)},
                           {"corrected_error", std::abs(pe.corrected_energy - reference)},
                           {"internal_residual_norm", pe.internal_residual_norm},
                           {"diagonal_evaluations", pe.diagonal_evaluations}, {"seconds", pe.seconds}});
    }
    Option output = {{"fcidump", argv[1]}, {"spin_orbitals", ham->norb}, {"electrons", ham->nelec},
                     {"reference_energy", reference}, {"options", opt}, {"trajectory_run", trajectory.second},
                     {"timings", runs}, {"trajectory", records}};
    std::ofstream file(argv[5]);
    file << output.dump(2) << '\n';
    if (!file) return 2;
    std::cout << "Saved benchmark: " << argv[5] << '\n';
}
