#!/usr/bin/env bash
set -e

cd -- "$(dirname -- "$(realpath -- "${BASH_SOURCE[0]}")")"

# -ffp-contract=off keeps -march=native from fusing multiply-adds, which would
# change floating-point results (and therefore expfy's learned modes 73-95).
# Never add -ffast-math for the same reason.
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17 -O3 -march=native -ffp-contract=off -Wall}"

pids=()
build() {
    local out="$1"; shift
    # shellcheck disable=SC2086
    "$CXX" $CXXFLAGS "$@" -o "$out" &
    pids+=("$!")
}

build cleanup       cleanup.cpp
build expfy         expfy.cpp
build onb           onb.cpp
build bnb           bnb.cpp
build gen_deg2      gen_deg2.cpp

build bxk4f_rseas   bxk4f_rseas.cpp
build bxk4f_rmse    bxk4f_rmse.cpp
build bxk4one_rseas bxk4one_rseas.cpp
build bxk4one_rmse  bxk4one_rmse.cpp

build otb2          otb.cpp
build orca          orca.cpp
build evs           evs.cpp
build evc1          evc1.cpp
build evc2          evc2.cpp
build evc3          evc3.cpp

status=0
for pid in "${pids[@]}"; do
    wait "$pid" || status=1
done
[ "$status" -eq 0 ] || { echo "build failed" >&2; exit 1; }

chmod +x rpll.sh rpllb.sh
