#!/bin/bash
#SBATCH -p amd_256
#SBATCH -N 1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH -J cdfci_serial
#SBATCH -t 01:00:00
#SBATCH --output=cdfci_serial_%j.out
#SBATCH --error=cdfci_serial_%j.out

# Submit from the project root with:
#   sbatch run_cdfci_serial.sh
# or pass an input JSON relative to the project root:
#   sbatch run_cdfci_serial.sh examples/demo_input_cdfci.json
# The default input prints progress and energy corrections every 1000 steps.
set -euo pipefail

source /public3/soft/modules/module.sh
module load gcc/12.2

ulimit -s unlimited
ulimit -l unlimited 2>/dev/null || true
export OMP_NUM_THREADS=1
export OMP_DYNAMIC=false
export MKL_NUM_THREADS=1

PROJECT_DIR="${CDFCI_PROJECT_DIR:-${SLURM_SUBMIT_DIR:-$PWD}}"
APP="${CDFCI_APP:-${PROJECT_DIR}/build_a2_gcc12/bin/cdfci}"
INPUT_ARG="${1:-examples/demo_input_energy_correction.json}"
if [[ "${INPUT_ARG}" = /* ]]; then
    INPUT="${INPUT_ARG}"
else
    INPUT="${PROJECT_DIR}/${INPUT_ARG}"
fi

if [[ ! -x "${APP}" ]]; then
    echo "CDFCI executable not found: ${APP}" >&2
    echo "Compile first: cmake --build build_a2_gcc12 --target cdfci --parallel 2" >&2
    exit 1
fi
if [[ ! -f "${INPUT}" ]]; then
    echo "Input JSON not found: ${INPUT}" >&2
    exit 1
fi

echo "CDFCI serial job ${SLURM_JOB_ID:-manual}"
echo "Executable: ${APP}"
echo "Input: ${INPUT}"
echo "Started: $(date '+%F %T %z')"
echo "------------------------------------------------------------"

# Relative FCIDUMP paths in the JSON are resolved from the JSON directory.
cd "$(dirname "${INPUT}")"
set +e
srun --ntasks=1 --cpus-per-task=1 "${APP}" "$(basename "${INPUT}")"
status=$?
set -e

echo "------------------------------------------------------------"
echo "Finished: $(date '+%F %T %z')"
echo "Exit status: ${status}"
exit "${status}"
