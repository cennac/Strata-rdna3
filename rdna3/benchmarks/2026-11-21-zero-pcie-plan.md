# Fixed-zero PCIe host planner: indexed routing groups

**Decision: do not enable the fast path in the development template.** Host plan publication was faster in both stages, but the Chinese single-request lane was slower in all three service pairs. The adopted fixed-zero **graph** remains enabled; this is a separate switch for the host planner.

## Change and validation

`STRATA_LAB_RDNA_ZERO_PCIE_PLAN=1` replaces repeated scans of routed entries with a bounded expert-ID index and entry chains. It applies only to eligible native, fixed-zero, host-planned rings with 512 experts, top-10 routing, and 2–4 rows. The general path handles single-row calls, unsupported shapes and invalid IDs. Groups remain in first-seen order and entries within groups remain in route order. The plan protocol, fences, CPU expert execution and A/B/C publication rules are unchanged.

The CPU probe passed 6,912 checks on Windows/MSVC and on the server with GCC. It compares route classifications and active plan fields using the general planner's scan primitives, with sentinels, both slot-address rules, repeated experts, varied residency, and invalid-ID rejection covered. On the server, the synthetic builder-only cases saved 0.25–1.75 µs/plan including index initialization. These timings exclude the GPU and do not predict service throughput.

In a separate, instrumented frozen off/on pair, the Chinese and code token IDs matched across arms in serial, pipelined, forced-rollback and batch requests (96 output tokens per probe; 83 rollbacks in each arm). Diagnostic counters confirmed that all observed eligible calls used the selected path in both stages. Mean time from builder entry through flag A publication was:

| Rows | Stage 0 off → on (µs/plan) | Stage 1 off → on (µs/plan) |
|---|---:|---:|
| 2 | 2.41 → 1.06 | 2.50 → 1.06 |
| 3 | 3.79 → 1.36 | 3.73 → 1.32 |
| 4 | 5.44 → 1.78 | 5.14 → 1.67 |

Stage means are weighted by calls; some call counts differ between arms. These are diagnostic observations, not a critical-path breakdown. Per-layer, per-width and distinct-miss buckets are retained in the structured record.

## Service confirmation on 66

Same HIP/gfx1100 binary, dual RX 7900 XTX, 26/22 split, BF16 HC and the fixed-zero graph in both arms. Adaptation on, caches off, matched profile seed and power caps, greedy seed 42 and presence penalty 1.5. Three fresh-load pairs, ordered on/off, off/on, on/off, with both arms of the middle pair reversing workload order. The plan-statistics diagnostic was off. All 36 scored requests reached 1,024 output tokens.

| Workload | Off → on median (tok/s) | Change | Changes in pairs 1 / 2 / 3 | Median TTFT off → on (ms) |
|---|---:|---:|---:|---:|
| Chinese single | 123.70 → 121.60 | −1.70% | −0.89% / −4.10% / −2.27% | 295 → 291 |
| Chinese dual | 163.48 → 164.53 | +0.65% | +0.65% / +1.95% / +2.87% | 592 → 588 |
| Code single | 163.70 → 163.20 | −0.31% | +5.33% / −0.31% / −2.24% | 270 → 272 |
| Code dual | 223.00 → 219.25 | −1.68% | +0.85% / +0.45% / −1.79% | 539 → 541 |

Single-request rates are engine decode on both cards; dual rates are total output divided by batch wall time. Change is the ratio of the two arm medians, not the median of paired changes. The faster planner did not produce a broadly faster service. No claim is made here about how much of the legacy–development gap either planner explains.

Production 0.1.43 was restored and verified loaded. The running executable hash matched before and after, the original configuration hash was unchanged, port 8101 was closed, and no monitor faults were recorded. See the [structured record](2026-11-21-zero-pcie-plan.json).
