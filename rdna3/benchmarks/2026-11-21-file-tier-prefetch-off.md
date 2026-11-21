# File-tier prefetch off on dual gfx1100

**Decision: retain the default.** Disabling file-tier prefetch improved the code dual-request median by 3.03%, but reduced both single-request medians by more than 2%. This is a comparison of two effective configurations, not an isolated measurement of I/O-thread overhead.

## Effective paths and method

The control left `STRATA_IO_PREFETCH` unset and selected the Linux file-tier default: whole-blob preads with eight I/O workers and routing look-ahead. With `STRATA_IO_PREFETCH=0`, both the I/O prefetch and routing look-ahead startup markers disappeared. On this mapped `experts.bin` configuration, `FileExpertSource::warms()` falls false without `io_pf_`; the conditions that independently enable the router predictor were not active. This differs from the earlier [router-only screen](2026-11-20-lookahead-off.md), which left I/O prefetch enabled. Neither test isolates the incremental I/O-only effect of this configuration change.

Both arms used binary `3ecc05edda3f0f9991537de379346d42c12189bd982bcd0a4516880c5500f2ec`, BF16 HC, fixed-zero PCIe verifier graph, and the dual-7900 XTX 26/22 split. Weights, expert-profile seed, cache policy, power limits and sampling settings were matched. There were three independent load pairs, ordered on/off, off/on, on/off; the middle pair reversed workload order for both arms. All 36 scored requests reached 1,024 output tokens. Two earlier attempts stopped before the off arm was scored because a reused assertion expected routing look-ahead to stay active. Neither attempt was included in the comparison; the corrected validator was tested against both saved startup logs before the new window.

## Results

| Workload | On median (tok/s) | Off median (tok/s) | Median change | Paired changes | Median TTFT on → off (ms) |
|---|---:|---:|---:|---:|---:|
| Chinese single | 126.50 | 123.70 | −2.21% | −4.40% / +0.24% / −2.90% | 290 → 289 |
| Chinese dual | 167.54 | 167.77 | +0.14% | −5.35% / +0.31% / +0.61% | 590 → 584 |
| Code single | 164.80 | 161.30 | −2.12% | −3.36% / −0.42% / −1.10% | 270 → 270 |
| Code dual | 217.38 | 223.96 | +3.03% | +0.12% / +5.97% / +4.16% | 570 → 535 |

One request uses both cards; its score is engine decode throughput. Dual throughput is total output divided by batch wall time, including first-token wait. Median change is the ratio of arm medians. Code dual improved in all pairs, although the first difference was negligible. Code single slowed in all pairs.

In separate frozen 384-token checks, Chinese and code response text matched across both arms, serial repeats, pipelining, and forced rollback. The forced requests recorded 124 Chinese and 76 code rollbacks in each arm. Unforced pipelined requests had the same window counts in both arms and ran at 18.05 → 17.55 ms/window (Chinese, 197 windows) and 17.35 → 16.83 ms/window (code, 158 windows). These are single observations at fixed residency, not confidence bounds or evidence of a gain with adaptation enabled. The scored workload is the basis for the template decision.

The final window lasted 15 min 44 s. Production 0.1.43 was verified loaded on port 8100, the configuration hash was unchanged, and the test listener on 8101 was closed. No monitor faults were recorded. See the [structured results](2026-11-21-file-tier-prefetch-off.json) for per-request measurements and selection markers.
