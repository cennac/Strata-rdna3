# Pool compute queue (async C) screen

**Decision: not adopted; the switch stays off.** The single-pool batch queue — the pool-serve redesign the rounds 38–42 analysis ranked as the primary remaining target — is measurably slower on every lane when correctly implemented. The machinery itself is proven bit-exact, so the C-publication split remains available to future designs.

## What was built

`STRATA_LAB_RDNA_POOL_QUEUE=1` (default off, `expert_source.cpp`): the serve thread builds the plan, publishes A and hands the CPU experts' batch to one runner thread through a 4-slot SPSC ring (lock-free, acquire/release ordering, `strata_cpu_pause` spins). The runner is the pool's only caller, preserving the one-batch-at-a-time invariant; when the ring is full the serve thread spins for a slot (never computing beside the runner). The runner raises each ring's C flag through a `GpuPlanSink::complete` callback after the batch lands; all four serve paths (serial, batch step, batch serve, pipelined) skip their own C raise when the dispatch queued the work. Job descriptors and quantized activations are copied into the slot, so the dispatch's tables are reusable the moment enqueue returns. Cold-landing paths and peer/remote tiers keep computing inline.

Two engineering findings from the bring-up: a `std::mutex`/`std::condition_variable` handoff threw `std::system_error(EPERM)` at `pthread_mutex_unlock` inside the runner on this HIP build (reproducible; gdb-located; the glibc-level cause remains unexplained — the mutex dump showed the serve thread as owner with waiters, kind 0), which is why the ring is lock-free; and the runner thread must be started from the main thread at construction, not lazily from the serve path.

## Result (binary `3e419e1d…`, three fresh-load pairs, 1,024 tokens)

| Workload | Queue off (median) | On (median) | Median change | Change per pair |
|---|---:|---:|---:|---:|
| Chinese single | 124.90 | 113.60 | −9.05% | −9.55% / +3.51% / −10.49% |
| Chinese dual | 162.26 | 154.42 | −4.83% | +0.22% / −10.33% / −4.83% |
| Code single | 163.30 | 146.30 | **−10.41%** | −11.49% / −7.84% / −10.08% |
| Code dual | 218.51 | 206.06 | −5.70% | −5.54% / −24.69% / −6.37% |

Frozen texts were byte-identical across arms, modes and repeats (the established SHAs), so this is a pure scheduling effect, not arithmetic.

## Why it lost

The synchronous inline compute was doing two things the queue removes: the serve thread worked as a thirteenth pool thread (`host_works_`), and each ring's compute began the instant its callback ran. The queue serializes every batch through one runner (FIFO), adds a handoff hop, and drops the serve thread's compute contribution — the wait chain moved rather than shrank. With this, every bounded restructure of the serve model tested on this rig (dual pools 4+4, pump reorder, serve quantum, B-direct, and now the async queue) has measured slower than the synchronous design; the remaining per-window gap of the development engine is structural to its pipelined window scheduling, not a removable overhead.

Production 0.1.43 was restored and verified after every window; the full campaign log and per-request data are in the [structured record](2026-11-21-pool-queue.json). Attempts b425/b426 aborted before scoring (the EPERM crash and its diagnosis) and are documented above.
