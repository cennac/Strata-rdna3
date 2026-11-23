# v0.2.19 stock, optimized, and production throughput (b458)

The pristine upstream v0.2.19 binary (`ce560089…`, engine 0.1.65) completed two fresh scored loads with upstream tuning defaults. The controller supplied only runtime infrastructure and the common workload settings; no RDNA compatibility fallback was needed. The optimized column uses the pinned [b457 development baseline](2026-11-23-refv2.md), engine 0.1.65 (`0f4ae265…`). The production column reuses the [historical same-protocol 0.1.43 baseline](2026-11-19-round10.md) (`58854970…`); production was not rebenchmarked in this window.

| Workload | Production 0.1.43 | Stock v0.2.19 | Optimized dev/0.2.19 | Optimized vs stock | Optimized vs production |
|---|---:|---:|---:|---:|---:|
| Chinese single | 139.10 | 63.40 | 120.70 | +90.4% | −13.2% |
| Chinese dual | 185.75 | 112.57 | 162.02 | +43.9% | −12.8% |
| Code single | 195.50 | 79.70 | 158.30 | +98.6% | −19.0% |
| Code dual | 235.83 | 106.51 | 200.35 | +88.1% | −15.0% |

| Workload | Stock fresh loads (tok/s) | Optimized fresh loads (tok/s) | Stock TTFT median (ms) | Optimized TTFT median (ms) |
|---|---:|---:|---:|---:|
| Chinese single | 62.6 / 64.2 | 120.7 / 120.7 | 786.8 | 288.2 |
| Chinese dual | 112.77 / 112.38 | 159.40 / 164.64 | 1585.3 | 590.5 |
| Code single | 80.2 / 79.2 | 155.3 / 161.3 | 712.3 | 268.8 |
| Code dual | 113.49 / 99.53 | 223.77 / 176.94 | 1450.8 | 543.3 |

Dual RX 7900 XTX, ROCm 10.1, 26/22 split, Qwen3.8-Flash-Next GSQ-RCO IQ3_XXS, fixed expert-profile seed, greedy seed 42 with presence penalty 1.5, prompt and conversation caches disabled. Every scored stock request completed 1,024 tokens. One lane reports engine decode; two lanes report total output tokens divided by batch wall time. The v0.2.19 columns are medians of two loads per version, compared across windows, not a paired performance claim. Stock and optimized configurations can take different paths: the stock engine disabled solo window pipelining alongside the two batch slots, while all four measured dual bursts logged pipelined batch execution.

Two earlier attempts were unscored due to harness configuration checks and do not enter the medians. The completed run restored production and verified its executable hash and configuration were unchanged, the test listener was closed, and no telemetry faults were recorded. No runtime setting was adopted from this measurement.

For subsequent optimization rounds, report the same three columns. Reuse the production and same-version stock measurements while their identities and the workload protocol remain unchanged; only refresh the comparison column whose baseline changes. Adoption decisions still use paired speed and quality gates.
