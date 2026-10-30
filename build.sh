#!/usr/bin/env bash
set -e

cd -- "$(dirname -- "$(realpath -- "${BASH_SOURCE[0]}")")"

# No -march=native by default: on an i7-13700HX it made expfy (the dominant
# cost) and orca ~15-30% slower, and it ties binaries to the build machine.
# -ffp-contract=off keeps any -march you add from fusing multiply-adds, which
# would change floating-point results (expfy's learned modes 73-95).
# Never add -ffast-math for the same reason.
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17 -O3 -ffp-contract=off -Wall}"

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
build evc1          evc12.cpp
build evc2          evc12.cpp -DEVC_DESC
build evc3          evc3.cpp

status=0
for pid in "${pids[@]}"; do
    wait "$pid" || status=1
done
[ "$status" -eq 0 ] || { echo "build failed" >&2; exit 1; }

chmod +x rpll.sh rpllb.sh
