#!/bin/bash
#SBATCH -p amd_256
#SBATCH -N 1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=64
#SBATCH --mem=96G
#SBATCH -J c2_ip_olsen
#SBATCH -t 18:00:00
#SBATCH --output=c2_ip_olsen_%j.out
#SBATCH --error=c2_ip_olsen_%j.out

set -euo pipefail

source /public3/soft/modules/module.sh
module load gcc/12.2

# Submit from the project root. Build the benchmark separately before submission.
PROJECT_DIR="${CDFCI_PROJECT_DIR:-${SLURM_SUBMIT_DIR:-$PWD}}"
BUILD_DIR="${CDFCI_EXPERIMENT_BUILD_DIR:-${PROJECT_DIR}/build}"
APP="${BUILD_DIR}/bin/cdfci_energy_correction_benchmark_omp"
FCIDUMP="${PROJECT_DIR}/data/c2_ccpvdz_psi4.FCIDUMP"
OUTPUT="${CDFCI_EXPERIMENT_OUTPUT:-${PROJECT_DIR}/experiment_results/energy_correction/${SLURM_JOB_ID:-manual}}/c2_ccpvdz"

# Same C2 settings as PR #2. MAX_WAVEFUNCTION_SIZE is an entry count, not GB.
ITERATIONS="${CDFCI_C2_ITERATIONS:-800000}"
INTERVAL="${CDFCI_CORRECTION_INTERVAL:-10000}"
Z_THRESHOLD="${CDFCI_C2_Z_THRESHOLD:-3e-8}"
MAX_WAVEFUNCTION_SIZE="${CDFCI_C2_MAX_WAVEFUNCTION_SIZE:-1696512081}"
NUM_COORDINATES="${CDFCI_NUM_COORDINATES:-64}"
REFERENCE_ENERGY=-75.7319603747

export OMP_NUM_THREADS="${SLURM_CPUS_PER_TASK:-64}"
export OMP_DYNAMIC=false
export OMP_PROC_BIND=spread
export OMP_PLACES=cores
export OPENBLAS_NUM_THREADS=1
export MKL_NUM_THREADS=1
export MPLBACKEND=Agg
export MPLCONFIGDIR="${SLURM_TMPDIR:-/tmp}/cdfci-matplotlib-${SLURM_JOB_ID:-manual}"
# Flush the solver's normal energy rows at every report point.
export CDFCI_BENCHMARK_VERBOSE=1

cd "${PROJECT_DIR}"
[[ -x "${APP}" ]] || { echo "Benchmark executable not found: ${APP}" >&2; exit 1; }
[[ -f "${FCIDUMP}" ]] || { echo "FCIDUMP not found: ${FCIDUMP}" >&2; exit 1; }
mkdir -p "${OUTPUT}" "${MPLCONFIGDIR}"

echo "C2/cc-pVDZ threads=${OMP_NUM_THREADS} coordinates=${NUM_COORDINATES} iterations=${ITERATIONS} report_interval=${INTERVAL} z_threshold=${Z_THRESHOLD}"
echo "results=${OUTPUT}"

PAIRED_JSON="${OUTPUT}/05_large_scale_with_timings.json"
IP_JSON="${OUTPUT}/05_ip_only.json"
ARGS=("${FCIDUMP}" "${ITERATIONS}" "${INTERVAL}" "${REFERENCE_ENERGY}")
SOLVER_ARGS=("${Z_THRESHOLD}" "${MAX_WAVEFUNCTION_SIZE}" "${NUM_COORDINATES}" 1)

# One fresh raw solve, then one fresh IP+Olsen solve; includes a 200-step warm-up.
srun --ntasks=1 --cpus-per-task="${OMP_NUM_THREADS}" --cpu-bind=cores \
    "${APP}" "${ARGS[@]}" "${PAIRED_JSON}" timed "${SOLVER_ARGS[@]}"

# One fresh IP-only solve with the same settings and warm-up.
srun --ntasks=1 --cpus-per-task="${OMP_NUM_THREADS}" --cpu-bind=cores \
    "${APP}" "${ARGS[@]}" "${IP_JSON}" ip-only "${SOLVER_ARGS[@]}"

# Basic timing needs only the Python standard library, including on offline nodes.
python3 - "${PAIRED_JSON}" "${IP_JSON}" "${OUTPUT}" <<'PY'
import csv
import json
import os
import sys

with open(sys.argv[1]) as handle:
    paired = json.load(handle)
with open(sys.argv[2]) as handle:
    ip = json.load(handle)
rows = [
    ("raw CDFCI", paired["baseline_trajectory_run"]["seconds"]),
    ("IP/PT2", ip["trajectory_run"]["seconds"]),
    ("IP/PT2+Olsen", paired["trajectory_run"]["seconds"]),
]
with open(os.path.join(sys.argv[3], "runtime_summary.csv"), "w", newline="") as handle:
    writer = csv.writer(handle)
    writer.writerow(["mode", "solver_seconds"])
    writer.writerows(rows)
for mode, seconds in rows:
    print("{}: {:.6f} seconds".format(mode, seconds))
PY

if python3 -c 'import numpy, matplotlib' >/dev/null 2>&1; then
    srun --ntasks=1 --cpus-per-task=1 \
        python3 -u experiments/energy_correction/summarize_large_scale.py \
        "${PAIRED_JSON}" "${OUTPUT}"
    srun --ntasks=1 --cpus-per-task=1 \
        python3 -u experiments/energy_correction/compare_ip_olsen.py \
        "${IP_JSON}" "${PAIRED_JSON}" "${OUTPUT}"
else
    echo "Plotting skipped: NumPy/Matplotlib unavailable. JSON and runtime_summary.csv saved."
fi

echo "Completed: ${OUTPUT}"
