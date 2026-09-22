#!/bin/bash
#SBATCH -p amd_256
#SBATCH -N 1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH -J cdfci_omp_test
#SBATCH -t 00:10:00
#SBATCH --output=cdfci_omp_test_%j.out
#SBATCH --error=cdfci_omp_test_%j.out

set -euo pipefail

# One CDFCI process with four OpenMP threads on one BSCC-A2 node.
# CDFCI is not an MPI program; do not launch multiple processes with mpirun.
source /public3/soft/modules/module.sh
module load gcc/12.2

ulimit -s unlimited
ulimit -l unlimited 2>/dev/null || true

export OMP_NUM_THREADS="${SLURM_CPUS_PER_TASK}"
export OMP_DYNAMIC=false
export OMP_PROC_BIND=close
export OMP_PLACES=cores
export MKL_NUM_THREADS=1

# Optional JSON argument: sbatch submit_cdfci_omp_test.slurm examples/input.json
PROJECT_DIR="${CDFCI_PROJECT_DIR:-${SLURM_SUBMIT_DIR}}"
APP="${CDFCI_APP:-${PROJECT_DIR}/build_a2_omp/bin/cdfci_omp}"
INPUT_ARG="${1:-examples/demo_input_energy_correction.json}"

if [[ "${INPUT_ARG}" = /* ]]; then
    INPUT="${INPUT_ARG}"
else
    INPUT="${PROJECT_DIR}/${INPUT_ARG}"
fi

if [[ ! -x "${APP}" ]]; then
    echo "Error: OpenMP executable not found or not executable: ${APP}" >&2
    echo "Build it first with: cmake --build build_a2_omp --target cdfci_omp --parallel 2" >&2
    exit 1
fi
if [[ ! -f "${INPUT}" ]]; then
    echo "Error: input JSON not found: ${INPUT}" >&2
    exit 1
fi

echo "CDFCI OpenMP smoke test"
echo "Job ID:       ${SLURM_JOB_ID}"
echo "Node:         ${SLURMD_NODENAME:-unknown}"
echo "Threads:      ${OMP_NUM_THREADS}"
echo "Executable:   ${APP}"
echo "Input:        ${INPUT}"
echo "Start time:   $(date '+%F %T %z')"
echo "------------------------------------------------------------"

# Resolve relative FCIDUMP paths from the JSON file's directory.
cd "$(dirname "${INPUT}")"
set +e
srun --ntasks=1 --cpus-per-task="${SLURM_CPUS_PER_TASK}" \
     "${APP}" "$(basename "${INPUT}")"
status=$?
set -e

echo "------------------------------------------------------------"
echo "Finish time:  $(date '+%F %T %z')"
echo "Exit status:  ${status}"
exit "${status}"
