# Local HC Q8: deadlock root-caused and fixed; candidate rejected on throughput

**Two outcomes.** The layer-24 deadlock that blocked the Q8 confirmation is root-caused and fixed (`res_put` now stream-ordered; validated six times on a recipe that reproduced the hang 3/3). The confirmation itself then ran clean — and **local Q8 is slower on three of four lanes**, so the candidate is rejected; BF16 stays the template default.

## The deadlock, root-caused

The 180-second issue #29 stalls had one signature: the serve loop stuck "at the CPU experts of layer 24" while no pool stall detector fired. A load-hunt (a scoring pass followed by a fresh Q8 load — a recipe that hung 3/3) plus a gdb attach before the watchdog fired captured the loop: the **adaptive tier's job thread** blocked in a synchronous `hipMemcpy` inside `res_put` (the residency-table upload), reached through `res_upload` from the tier's step path. The copy runs on the legacy stream, which waits behind the pipelined window graphs' spin kernels; those spin on host flags the serve thread raises — a host↔GPU circular wait with no exit. The engine's own design note ("the device copies of the residency table are not uploaded meanwhile — a synchronous copy") documents the intent; a tier-thread path violated it. The flakiness was timing: only when the upload raced a window in flight, which the Q8 loads' HC timing tripped roughly every other campaign load (2 of 8 campaign loads; 3/3 on the targeted recipe; 0 hangs in ten loads without a preceding scoring pass).

## The fix

`generate.cpp`'s `res_put` uploads on a lazily created per-device stream with `cudaMemcpyAsync`. Pipelined serving never waits: a mid-flight table update is as safe as the `res_patch` kernels the tier already posts on the verifiers' streams (the pool's plan is what a window actually follows; the table rows are hints the verifiers republish). Serial-window mode keeps a `cudaStreamSynchronize` on that stream — there nothing is in flight, so waiting only preserves the old semantics. Startup paths keep the wait by default (`res_serial_wait` starts true; the serve section sets it once `serial_windows` is known).

Validation: the 3/3 reproducing recipe ran six full cycles (load → warmup → four 1,024-token bursts → stop) with zero hangs on the fixed binary; a frozen four-mode parity run reproduced the BF16 baseline's canonical hashes byte-for-byte (the fix changes scheduling, not arithmetic).

## The confirmation (binary `3ecc05ed…`, three fresh-load pairs, 1,024 tokens)

| Workload | BF16 (median) | Q8 (median) | Median change | Change per pair |
|---|---:|---:|---:|---:|
| Chinese single | 125.60 | 119.90 | −4.54% | −6.62% / −0.72% / −3.40% |
| Chinese dual | 171.72 | 165.18 | −3.81% | −6.91% / −1.00% / −1.40% |
| Code single | 164.90 | 161.00 | −2.37% | −3.12% / −0.49% / −2.37% |
| Code dual | 218.72 | 219.07 | +0.16% | +1.88% / −5.97% / −0.75% |

All 36 scored requests reached full budget. The earlier one-load screen's +3.55% Chinese-dual did not reproduce; with the deadlock fixed the measurement stands. The frozen Q8 arm is internally deterministic and rollback-consistent (its own hash per prompt, all four modes equal) but necessarily differs from BF16 (approximate weights). Per-load data: [structured record](2026-11-21-hc-q8-confirmed.json). This supersedes the [blocked record](2026-11-21-hc-q8-blocked.md). Production 0.1.43 was restored and verified after every window (eleven diagnostic and campaign windows across the chase).
