# SynthNetGen_Mod

Given a target graph, build a synthetic graph that matches the target's
graphlet frequencies (4- or 5-node graphlets), degree sequence, and largest
adjacency eigenvalue (spectral radius).

The pipeline starts from a degree-matched random graph and repeatedly applies
local edge transformations. It scores candidate pairs of transformations
against the target's graphlet vector and keeps the best one in each round.

## Requirements

- Linux or WSL
- g++ with C++17 and OpenMP (`-fopenmp`)
- bash 4.3 or newer (for `wait -n` and namerefs)
- GNU coreutils, awk
- python3 (standard library only)

## Build

```bash
./build.sh
```

This compiles every tool in parallel with `-std=c++17 -O3 -ffp-contract=off`.
Override the flags with `CXXFLAGS`, but never add `-ffast-math`. Also keep
`-ffp-contract=off` if you add `-march`: both change floating-point results,
which changes expfy's learned modes 73–95. On an i7-13700HX, `-march=native`
made expfy and orca slower, so it is not the default.

## Usage

```bash
./rpll.sh <k:4|5> <Target> <Output> [Base_Synth] <RSEAS_Rounds> <RMSE_Rounds> <RBGO>
```

- `k`: graphlet size to match (4 or 5).
- `Target`: the target graph as an edge list (`u v` per line). It is cleaned
  first, which keeps the largest connected component and relabels nodes 0..n-1.
- `Output`: where the final synthetic graph is written.
- `Base_Synth` (optional): start from this graph instead of generating one.
  It must have the same number of nodes as the cleaned target.
- `RSEAS_Rounds` (≥ 1): rounds that use the dynamically weighted RSEAS
  objective.
- `RMSE_Rounds` (≥ 0): polishing rounds that use RMSE, appended after the
  RSEAS rounds.
- `RBGO` (≥ 1): regenerate the transformation probe curves every RBGO rounds
  within each phase.

Example: `./rpll.sh 4 target.el out.el 10 2 3`

`rpllb.sh` takes the same arguments. The differences:

- It measures graphlets by BLANT sampling (`bnb`, 100k EBE samples) instead
  of exact ORCA counts.
- It still uses ORCA for the 4-node orbit table that guides expfy's node
  selection, recomputed once per input graph.
- Its candidate stages run in parallel, with the same job pool and thread
  budget as `rpll.sh`.
- It needs the `blant` binary, `canon_maps/` and `orca_jesse_blant_table/`
  in the project directory (or in `Synthcheck/`).

Use it for dense graphs where exact counting is too slow.

### Disabled transformations: `disabled.txt`

`disabled.txt` must have exactly two lines:

1. Transformation IDs (1–95) to skip. The shipped file disables 25, 26 and
   61–95 (25 and 26 are never implemented, so they are always skipped).
2. EVC tools (1–3) to skip, or a single `0` to skip none.

Both pipelines and the `bxk4f` tools read it. Each run snapshots it at start.

### Runtime helper: `pipeline_runtime.sh`

Both scripts source `pipeline_runtime.sh`, which:

- checks that every tool is built;
- creates a private run directory under `$TMPDIR` (default `/tmp`) and runs
  there;
- logs every expfy call;
- deletes the run directory when the run ends.

`EXPFY_BIN` selects the expfy binary (default `./expfy`).

### Job pool: `job_pool.sh`

Both scripts also source `job_pool.sh`. It sets the thread budget (see below)
and provides the background-job pool. As soon as any job finishes, the next
one starts (`wait -n`). Results are always read back by label or rank, so
scheduling never changes the output.

## Environment variables

| Variable | Default | Effect |
|---|---|---|
| `THREAD_BUDGET` | 4 | Upper bound on threads running at once. It sets the defaults below. |
| `PARALLEL_JOBS` | `THREAD_BUDGET` | Parallel workers for probe generation (`generate_outinp`). |
| `ROUND_JOBS` | `PARALLEL_JOBS` | Parallel workers inside a round (EVC candidates, top pairs). |
| `ORCA_THREADS` | budget (1 in standalone orca) | Threads for orca's k=5 equation stage (max 4). Inside a pool of N workers each orca gets budget/N. |
| `BXK_THREADS` | budget (1 in standalone bxk4f) | Threads for the bxk4f pair search (max 4). |
| `EXPFY_SEED` | clock + random_device | Fixes expfy's RNG for reproducible runs. |
| `EXPFY_BIN` | `./expfy` | expfy binary to use (for example a build of `expfy_rev.cpp`). |
| `TMPDIR` | `/tmp` | Where the private run directory is created. |
| `EXPFY73_DATA_ATTEMPTS`, `EXPFY73_MIN_PRECISION`, `EXPFY73_C`, `EXPFY73_MAX_ITERS`, `EXPFY73_BOTH`, `EXPFY73_DUMP_DIR` | see `expfy.cpp` | Training settings for the learned modes 73–95. |
| `EVC_SEED`, `GEN_DEG_SEED`, `BNB_SEED` | random | Test-only seeds for evc1/2/3, gen_deg2 and BLANT sampling in bnb. Set them together with `EXPFY_SEED` and a whole run is reproducible. |

Thread counts never change results. orca and bxk4f produce byte-identical
output for any thread count.

## Pipeline overview

1. **Baseline**:
   - `cleanup` the target.
   - Unless a base graph is given, `gen_deg2` builds a random graph with the
     target's degree sequence.
   - Seven expfy passes then shape that graph.
2. **PIS spectral pre-pass**: three stages that move the largest eigenvalue
   toward the target's.
   - If the eigenvalue is too low, it is raised with evc1 or evc2, whichever
     scores the lower RSEAS.
   - If it is too high, evc2 lowers it.
3. **RSEAS rounds**: each round runs these steps.
   - Measure the current graph.
   - On refresh rounds, generate per-transformation probe curves for every
     mode (two expfy scales per mode; the modes run in parallel).
   - Try the enabled EVC eigenvalue moves (small and large run of each).
   - Score every transformation pair in one `bxk4f` batch.
   - Apply the 10 best pairs in parallel and keep the best result.
   - Refine that pair up to 3 times.
4. **RMSE rounds**: the same as RSEAS rounds, but with an RMSE objective and
   the tightest constraints.

## Tools

| Binary | Source | Role |
|---|---|---|
| `cleanup` | cleanup.cpp | Keep the largest connected component, deduplicate edges, relabel nodes 0..n-1. |
| `gen_deg2` | gen_deg2.cpp | Random connected graph with a given degree sequence (spanning tree + Chung–Lu). |
| `orca` | orca.cpp | Exact per-node orbit counts for 4- or 5-node graphlets (ORCA, modified). k=5 requires fewer than 2,097,152 nodes. |
| `otb2` | otb.cpp | Convert orca orbit counts into graphlet counts. |
| `onb` | onb.cpp | Run orca + otb2 on an edge list and print the normalized graphlet vector. It also leaves `data_middle.txt` (orbits). |
| `bnb` | bnb.cpp | Same as onb, but uses BLANT sampling (rpllb). |
| `expfy` | expfy.cpp | Apply a transformation mode (1–95, excluding 25 and 26) many times under degree, histogram and graphlet constraints. |
| `evs` | evs.cpp | Largest adjacency eigenvalue (shifted power iteration). |
| `evc1`, `evc2` | evc12.cpp | Edge-neighbourhood moves that raise or lower the eigenvalue (evc2 = `-DEVC_DESC`). |
| `evc3` | evc3.cpp | AND/OR neighbourhood move. |
| `bxk4f_rseas`, `bxk4f_rmse` | bxk4f_*.cpp + bxk4f_common.hpp | Grid-search the best scales for every transformation pair in one batch. |
| `bxk4one_rseas`, `bxk4one_rmse` | bxk4one_*.cpp | Best single scale multiplier for one pair during refinement. |

`expfy_rev.cpp` is an alternate expfy backend. Its learned mode 73 and modes
84–95 are trained in the reverse direction. `build.sh` does not build it. To
use it, compile it yourself and point `EXPFY_BIN` at the binary.

## Output files

- `<Output>`: the final synthetic graph.
- `<Output>.expfy.XXXXXXXX.log`: stderr of every expfy call in the run.

Everything else, including per-round graphs, `wcic.txt` and probe data,
lives in the private run directory and is deleted when the run ends.

`f0.txt` and `disabled.txt` are required runtime inputs; keep them.
