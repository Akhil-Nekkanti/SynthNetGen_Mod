#!/usr/bin/env bash

# Shared by both pipelines: thread budget and the background-job pool.
#
# At most THREAD_BUDGET (default 4) threads run at once. Serial steps let
# orca (ORCA_THREADS) and the bxk4f pair search (BXK_THREADS) use the whole
# budget; inside a pool of N parallel workers each orca gets budget/N.
# Thread counts never change results: both tools are byte-identical for any
# thread count.
THREAD_BUDGET="${THREAD_BUDGET:-4}"
export ORCA_THREADS="${ORCA_THREADS:-$THREAD_BUDGET}"
export BXK_THREADS="${BXK_THREADS:-$THREAD_BUDGET}"

# Threads each of JOBS concurrent workers may use within the budget.
worker_threads() {
    local THREADS=$((THREAD_BUDGET / $1))
    echo $((THREADS > 0 ? THREADS : 1))
}

# pool_wait_slot RUNNING_VAR FAILED_VAR MAX
# Block until fewer than MAX jobs are running. wait -n returns as soon as any
# job finishes, so one slow job does not leave other slots idle. A failed job
# sets FAILED_VAR=1. Callers read results back by label or rank, never in
# completion order, so scheduling cannot change the output.
pool_wait_slot() {
    local -n POOL_RUNNING_REF="$1" POOL_FAILED_REF="$2"
    while (( POOL_RUNNING_REF >= $3 )); do
        wait -n || POOL_FAILED_REF=1
        POOL_RUNNING_REF=$((POOL_RUNNING_REF - 1))
    done
}

# pool_wait_all RUNNING_VAR FAILED_VAR: wait for every remaining job.
pool_wait_all() {
    pool_wait_slot "$1" "$2" 1
}
