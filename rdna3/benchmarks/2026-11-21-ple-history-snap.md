# PLE history advance and snapshot fusion

**Decision: keep the experiment off.** This removes a graph node per snapshotted row and passed the bitwise state and frozen output checks, but no workload improved in all three service pairs. Code single-request decode was slower in every pair and its median dropped 2.24%.

## What changed

The per-row PLE path on the front stage writes live history and its post-row rollback snapshot in one GPU kernel, instead of advancing history and then copying it. The layout, destination buffers, projections, normalization, convolution, and accepted-prefix restore order are unchanged. The fast path is opt-in for native 512-expert fixed-zero PCIe graphs. It replaces one launch and one logical 360 KiB reread per snapshotted row; this is not a measured change in physical memory traffic.

## Correctness and answer regression

- The HIP probe passed on both RX 7900 XTX cards. It compared each snapshot, live history, and sentinels bitwise with the old kernels, including graph replays, compact batch slots, shortened commits, and the following window.
- The development binary's off/on arms matched exact token IDs and finish reasons across all 14 frozen request cases (96 output tokens per case). This covered repeat serial, pw2, forced-rollback mode, two-slot batch, and no-penalty controls. Presence, repetition, and frequency penalties used an explicit 64-token history in the penalized cases. The forced-mode requests recorded 32 Chinese and 23 code rollbacks in each arm; the trace does not attribute every undo to the injected miss.
- All 12 positive-temperature cases per arm matched exact IDs and finish reasons across arms, including repeated requests and continued requests that reused 126 Chinese or 116 code prefix tokens. Split and uninterrupted sampled requests were checked separately, not assumed equivalent.
- In the separate graph diagnostic, 24 matched stage/shape/parity/width buckets had the expected changes: the front stage lost one node per snapshotted row, and the final stage did not change. Both solo parities and the batch path were covered. Verifier profiling was not enabled.
- Six AIOps v7 objective cases at a 128-token limit had no new failures. Both arms passed only the exact-format case; four cases reached the output cap and the order-total case was wrong in both. This is a limited regression check, not an absolute quality pass.

## Paired service measurements on 66

Same HIP Release binary from `9ba93ba` plus the PLE experiment; dual gfx1100, ROCm 10.1, 26/22 split, Qwen3.8-Flash-Next GSQ-RCO IQ3_XXS, BF16 HC, fixed-zero PCIe graph, normal adaptation, caches disabled, same profile seed and power caps, and greedy decoding with presence penalty 1.5. Diagnostics were off. Fresh loads were interleaved on/off, off/on, on/off, with the second pair reversing workload order in both arms. All 36 scored requests reached 1,024 output tokens.

| Workload | Median off → on (tok/s) | Change | Pair 1 / 2 / 3 | Median TTFT off → on (ms) |
|---|---:|---:|---:|---:|
| Chinese single | 121.60 → 126.50 | +4.03% | +6.75% / +4.85% / −4.90% | 288.9 → 291.2 |
| Chinese dual | 167.66 → 167.36 | −0.18% | +5.56% / −3.76% / −0.18% | 587.4 → 588.5 |
| Code single | 164.90 → 161.20 | −2.24% | −3.36% / −0.55% / −2.24% | 273.3 → 273.3 |
| Code dual | 217.51 → 221.21 | +1.70% | +1.13% / −0.53% / +1.70% | 540.4 → 544.6 |

Single-request rates use engine decode across both cards; dual rates use total tokens divided by batch wall time. Changes compare arm medians. No TTFT median regressed by more than 0.8%. The predeclared adoption screen required at least one workload to improve in all three pairs and by at least 2% on its median, without a throughput loss greater than 2% or TTFT regression greater than 5%. This candidate did not meet it.

The initial b438 campaign stopped after one full pair when the cleanup check still reported a test-port listener after the load exited. Production restored successfully. The independent b438-r2 campaign added a bounded listener recheck after process teardown and reran all gates and pairs with the same engine binary. The initial pair was not included in the table. The completed repeat recorded zero monitor faults and verified production 0.1.43 restored with the same running executable hash and unchanged configuration, with the test port closed. See the [structured record](2026-11-21-ple-history-snap.json) and [reproduction notes](../experiments/README.md#ple-history-advance-and-snapshot).
