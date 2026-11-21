# Pinned penalty-history staging: stream-sync removal rejected

**Decision: keep `STRATA_LAB_RDNA_PIN_PENALTY_HIST` off.** The change was output-identical on every frozen check, but the Chinese lanes lost throughput in all three service pairs (single −5.81%, dual −4.68% on arm medians). A correct-but-slower scheduling change is rejected by the predeclared screen.

## What changed

The solo pw2 penalty-history upload used a pageable staging vector, and after enqueueing the 256–768-byte H2D copy the serve thread synchronized the **entire last-stage stream** (the code comment said this was only to keep the pageable vector alive until copied). That wait also drained the previous window's commit replay and sampling before window A's graph could be enqueued, so the pump thread was blocked away from stage 0.

The opt-in candidate stages the upload through one page-locked buffer (`cudaHostAlloc`, 64 KiB) with a `cudaEventDisableTiming` event guarding the buffer's rewrite, drops the full-stream synchronize, and lets the copy, window graph and sampling order on the shared stream. Allocation failure falls back to the exact pageable path. Serial and batch paths, graph capture, sampler and stream topology are unchanged. One file: `engine/src/program/generate.cpp` (+39/−4 on `9ba93ba`).

## Correctness: all gates passed

- Frozen raw (14 requests × 96 tokens per arm): exact token IDs and `length` finishes identical across off/on arms — serial repeat, pw2, forced-rollback, two-slot batch, no-penalty controls, with an explicit 64-token penalty window. Forced mode recorded 32 Chinese / 23 code rollbacks in each arm; pw2 trace counts matched exactly (41 speculative launches / 29 undos Chinese, 33 / 14 code), so window scheduling itself was unchanged.
- Positive-temperature sampling (12 requests per arm): exact repeats and prefix continuation identical across arms; continuations reused 126 Chinese / 116 code prefix tokens.
- Objective v7 (6 cases, 128-token cap): no new failures; the same five cases failed in both arms (four hit the cap, the order JSON was wrong in both).
- Graph histograms (`STRATA_VERIFY_NODES=1`, profiling unset): all 24 matched stage/parity/shape/width buckets identical between arms — no node changes, as expected for a host-side change.
- Engine binary `ce3b287a…`, built from clean `9ba93ba` plus only this file; every scored load's effective environment and argv verified against the plan.

## Paired service measurements on 66

Dual gfx1100, 26/22 split, fixed-zero PCIe graph, BF16 HC, normal adaptation, caches off, matched profile seed, greedy with presence penalty 1.5; three interleaved fresh-load pairs (on/off, off/on, on/off, second pair workload-reversed). All 36 scored requests reached 1,024 tokens; zero monitor faults; production 0.1.43 restored and verified (same running hash, unchanged config, test port closed).

| Workload | Median off → on (tok/s) | Change | Pair 1 / 2 / 3 | Median TTFT change |
|---|---:|---:|---:|---:|
| Chinese single | 127.30 → 119.90 | −5.81% | −7.70% / −8.26% / −0.82% | +0.22% |
| Chinese dual | 165.06 → 157.34 | −4.68% | −4.03% / −0.38% / −4.68% | +0.45% |
| Code single | 159.70 → 160.60 | +0.56% | +0.75% / −0.81% / +0.12% | −0.57% |
| Code dual | 219.23 → 218.85 | −0.17% | +1.25% / −10.63% / −0.17% | +0.46% |

Single-request rates are engine decode; dual rates are total tokens over batch wall time. All six Chinese comparisons point down, beyond this rig's usual ±5% pair spread in four of them. No TTFT median regressed meaningfully.

## Reading

The removed synchronize was evidently acting as useful pacing, not as a bottleneck: with the serve thread free to run ahead, window A's graph lands behind the previous window's commit on the shared last-stage stream, and the pump's changed interleave with stage 0 consistently cost the Chinese lanes. (Attribution is a hypothesis; the paired numbers are the evidence.) Together with the PLE-snapshot null result, host-side blocking at the penalty upload is not where the remaining development-vs-legacy window gap lives; the GPU-side window cost remains the likely territory.

See the [structured record](2026-11-21-pin-penalty-hist.json) and the [experiment notes](../experiments/README.md#pinned-penalty-history-staging).
