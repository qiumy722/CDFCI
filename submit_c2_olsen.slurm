#!/bin/bash
#SBATCH -p amd_256
#SBATCH -N 1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=64
#SBATCH --mem=96G
#SBATCH -J c2_olsen_omp
#SBATCH -t 18:00:00
#SBATCH --output=c2_olsen_omp_%j.out
#SBATCH --error=c2_olsen_omp_%j.out

set -euo pipefail

source /public3/soft/modules/module.sh
module load gcc/12.2

# Compile separately. Submit from the project root.
PROJECT_DIR="${CDFCI_PROJECT_DIR:-${SLURM_SUBMIT_DIR:-$PWD}}"
BUILD_DIR="${CDFCI_EXPERIMENT_BUILD_DIR:-${PROJECT_DIR}/build}"
APP="${BUILD_DIR}/bin/cdfci_energy_correction_benchmark_omp"
FCIDUMP="${PROJECT_DIR}/data/c2_ccpvdz_psi4.FCIDUMP"
OUTPUT="${CDFCI_EXPERIMENT_OUTPUT:-${PROJECT_DIR}/experiment_results/energy_correction/${SLURM_JOB_ID:-manual}}/c2_ccpvdz"

# Match the previous 800,000-step test: up to 64 coordinates per block step.
ITERATIONS="${CDFCI_C2_ITERATIONS:-800000}"
INTERVAL="${CDFCI_CORRECTION_INTERVAL:-10000}"
Z_THRESHOLD="${CDFCI_C2_Z_THRESHOLD:-3e-8}"
MAX_WAVEFUNCTION_SIZE="${CDFCI_C2_MAX_WAVEFUNCTION_SIZE:-1696512081}"
NUM_COORDINATES="${CDFCI_NUM_COORDINATES:-64}"
REFERENCE_ENERGY=-75.7319603747

# Both CDFCI and Olsen follow the CPU count allocated by Slurm.
export OMP_NUM_THREADS="${SLURM_CPUS_PER_TASK:-64}"
export OMP_DYNAMIC=false
export OMP_PROC_BIND=spread
export OMP_PLACES=cores
export OPENBLAS_NUM_THREADS=1
export MKL_NUM_THREADS=1
export CDFCI_BENCHMARK_VERBOSE=1

cd "${PROJECT_DIR}"
[[ -x "${APP}" ]] || { echo "Benchmark executable not found: ${APP}" >&2; exit 1; }
[[ -f "${FCIDUMP}" ]] || { echo "FCIDUMP not found: ${FCIDUMP}" >&2; exit 1; }
mkdir -p "${OUTPUT}"

echo "C2/cc-pVDZ IP/PT2+Olsen threads=${OMP_NUM_THREADS} coordinates=${NUM_COORDINATES} iterations=${ITERATIONS} report_interval=${INTERVAL} z_threshold=${Z_THRESHOLD}"
echo "results=${OUTPUT}"

# Run only IP/PT2+Olsen. No baseline solve, warm-up solve, or Python dependency.
srun --ntasks=1 --cpus-per-task="${OMP_NUM_THREADS}" --cpu-bind=cores \
    "${APP}" "${FCIDUMP}" "${ITERATIONS}" "${INTERVAL}" "${REFERENCE_ENERGY}" \
    "${OUTPUT}/olsen_parallel.json" olsen-only \
    "${Z_THRESHOLD}" "${MAX_WAVEFUNCTION_SIZE}" "${NUM_COORDINATES}" 1

echo "Completed: ${OUTPUT}"
