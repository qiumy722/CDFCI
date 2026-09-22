#!/bin/bash
#SBATCH -p amd_256
#SBATCH -N 1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH -J cdfci_serial
#SBATCH -t 01:00:00
#SBATCH --output=cdfci_serial_%j.out
#SBATCH --error=cdfci_serial_%j.out

set -euo pipefail

# BSCC-A2 requires explicit initialization of the module command.  Load the
# GCC runtime used to build this serial executable; no MPI module is needed.
source /public3/soft/modules/module.sh
module load gcc/12.2

ulimit -s unlimited
ulimit -l unlimited 2>/dev/null || true

# Prevent OpenMP/MKL libraries from creating extra worker threads.
export OMP_NUM_THREADS=1
export OMP_DYNAMIC=false
export MKL_NUM_THREADS=1

# Run `sbatch submit_cdfci_serial.slurm path/to/input.json`.
# If no input is supplied, the H2O demo with 1000-step correction reports is used.
PROJECT_DIR="${CDFCI_PROJECT_DIR:-${SLURM_SUBMIT_DIR}}"
APP="${CDFCI_APP:-${PROJECT_DIR}/build_a2_gcc12/bin/cdfci}"
INPUT_ARG="${1:-examples/demo_input_energy_correction.json}"

if [[ "${INPUT_ARG}" = /* ]]; then
    INPUT="${INPUT_ARG}"
else
    INPUT="${PROJECT_DIR}/${INPUT_ARG}"
fi

if [[ ! -x "${APP}" ]]; then
    echo "Error: serial executable not found or not executable: ${APP}" >&2
    echo "Compile it first with: cmake --build build_a2_gcc12 --target cdfci --parallel 2" >&2
    exit 1
fi

if [[ ! -f "${INPUT}" ]]; then
    echo "Error: input JSON not found: ${INPUT}" >&2
    exit 1
fi

echo "CDFCI serial job"
echo "Job ID:       ${SLURM_JOB_ID}"
echo "Node:         ${SLURMD_NODENAME:-unknown}"
echo "Executable:   ${APP}"
echo "Input:        ${INPUT}"
echo "Output file:  ${SLURM_SUBMIT_DIR}/cdfci_serial_${SLURM_JOB_ID}.out"
echo "Start time:   $(date '+%F %T %z')"
echo "------------------------------------------------------------"

# Relative FCIDUMP/checkpoint paths in the JSON are resolved from the JSON
# file's directory.
cd "$(dirname "${INPUT}")"
set +e
srun --ntasks=1 --cpus-per-task=1 "${APP}" "$(basename "${INPUT}")"
status=$?
set -e
echo "------------------------------------------------------------"
echo "Finish time:  $(date '+%F %T %z')"
echo "Exit status:  ${status}"
exit "${status}"
