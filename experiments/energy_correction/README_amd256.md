# C2 benchmark on BSCC-A2 / amd_256

Submit `submit_c2_benchmark.slurm` from the CDFCI project root. It runs raw
CDFCI, IP/PT2+Olsen, and IP/PT2 only sequentially on one allocated node, with a
fresh wavefunction for each solve. It does not compile or run N2/small suites.

The defaults reproduce PR #2's C2 workload: 64 threads, 64 coordinates, 800000
iterations, report interval 10000, z_threshold 3e-8, and reference energy
-75.7319603747 Ha. Slurm requests 96 GiB and 18 hours. The entry ceiling
1696512081 is passed directly to the benchmark; the application's input JSON
max_memory is not used by this benchmark executable. To change resources, edit
the script's SBATCH directives; solver settings are near the top of the script.

Compile separately on the server, from the project root:

```bash
source /public3/soft/modules/module.sh
module load gcc/12.2
module load cmake/3.28.0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DCDFCI_ENABLE_OPENMP=ON -DCDFCI_BUILD_EXPERIMENTS=ON
cmake --build build --target cdfci_energy_correction_benchmark_omp --parallel 8
sbatch submit_c2_benchmark.slurm
```

Python 3's standard library is sufficient for running and reporting timings.
NumPy and Matplotlib are optional: if they are unavailable, plotting is skipped
and all three solves still finish. GCC is loaded inside the job for its runtime
libraries; CMake is only needed at build time. A cached/local Eigen installation
is reused by CMake. Replacing only this Slurm script does not require a rebuild
once the updated ip-only benchmark has been built.

The out file `c2_ip_olsen_<job-id>.out` contains the energy rows as they are
reported. Follow it with `tail -f c2_ip_olsen_<job-id>.out`. If a solve is killed,
the rows already flushed remain in the out file; the JSON for that benchmark
invocation is written only when its solves finish, and is not a restart file.

Outputs are under
`experiment_results/energy_correction/<job-id>/c2_ccpvdz/`: paired raw/corrected
JSON, IP-only JSON, and `runtime_summary.csv` with all three solve times. When
NumPy and Matplotlib are available, the detailed CSV/JSON and PDF/PNG plots from
PR #2 are generated too. For offline servers, copy the result directory to a
computer with these dependencies and run, from the project root:

```bash
RESULT=experiment_results/energy_correction/JOB_ID/c2_ccpvdz
python3 experiments/energy_correction/summarize_large_scale.py \
  "$RESULT/05_large_scale_with_timings.json" "$RESULT"
python3 experiments/energy_correction/compare_ip_olsen.py \
  "$RESULT/05_ip_only.json" "$RESULT/05_large_scale_with_timings.json" "$RESULT"
```

The timings include solver output at report points in this script; original
PR measurements used quiet reporting. Repeated runs and different hardware may
change wall times. Compare the three new runs with each other.

The required companion files are `test/benchmark_energy_correction.cpp`,
`experiments/energy_correction/compare_ip_olsen.py`, and
`experiments/energy_correction/summarize_large_scale.py`. Copy these with the
Slurm script, then rebuild; an older executable does not accept `ip-only`.
