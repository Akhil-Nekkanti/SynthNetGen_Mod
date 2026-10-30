# SynthNetGen_Mod

## Setup
```bash
chmod +x build.sh
./build.sh
```

## Usage

Generate a starting graph automatically:

```bash
./rpll.sh <k:4|5> <target> <output> <RSEAS_rounds> <RMSE_rounds> <r>
```

Or supply an existing starting graph:

```bash
./rpll.sh <k:4|5> <target> <output> <synth_base> <RSEAS_rounds> <RMSE_rounds> <r>
```

Use `./rpllb.sh` in either command to estimate graphlet frequencies with BLANT instead of ORCA.

- `k`: graphlet size, either 4 or 5.
- `target`: target graph edge-list file.
- `output`: destination for the generated graph.
- `synth_base`: optional starting graph with the same node count as the cleaned target.
- `RSEAS_rounds`: number of RSEAS optimization rounds; must be positive.
- `RMSE_rounds`: number of subsequent RMSE optimization rounds; may be zero.
- `r`: baseline results are computed every `r` rounds.

For parallel workers, use

```bash
PARALLEL_JOBS=4 ./rpllb.sh 4 target.el output.el 12 3 3
```

Use `rpllb.sh` instead of `rpll.sh` to use `blant` instead or `orca`.
