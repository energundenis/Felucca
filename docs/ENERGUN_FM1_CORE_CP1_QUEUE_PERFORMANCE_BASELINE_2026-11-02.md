# ENERGUN FM-1 CORE — CP1 queue performance baseline

Date: 2026-11-02  
Branch: `energun-metal-core`  
Purpose: record the first observed timing evidence without claiming an unsupported project-wide speedup.

## Evidence collected

| Run | Mode | Run ID | Job ID | Queue start (UTC) | Queue terminal status (UTC) | Observed queue wall time |
|---|---|---:|---:|---|---|---:|
| Earlier | Sequential controller | 41462258057 | 125334040115 | 14:29:10.7323886 | 14:29:11.0033414 | 0.271 s |
| Current | Dependency-aware parallel controller | 41481399097 | 125400350235 | 16:27:02.9652993 | 16:27:03.4661803 | 0.501 s |

The earlier and current queue executions are **not a valid before/after benchmark**:
- The earlier queue used the previous sequential controller and did not execute the A/B selector as a ready executable stage; that stage was only represented by evidence/blocked state.
- The current queue executes Sidecar V1 format (38 checks), Base-generation binding (16 checks), and A/B selection (17 checks), and also runs five scheduler unit tests before the queue.
- The current run therefore performs more work. The raw 0.271 s vs 0.501 s comparison must not be interpreted as a slowdown or speedup.

## What is proven

- Current run: Sidecar format and Base-generation binding started at 16:27:03.0135386 and 16:27:03.0139273 UTC respectively — 0.3887 ms apart in the runner log.
- Both completed successfully before A/B selection started at 16:27:03.2297217 UTC.
- A/B selection passed; queue then stopped at the expected `pair-commit-order` blocker because the production pair transaction coordinator is not implemented.
- Queue result: `BLOCKED` by design; GitHub Actions workflow result: `SUCCESS`.
- The scheduler's barrier-based unit test proves independent tasks can overlap and that the join waits for both tasks. It is a correctness test, not a performance benchmark.

## Initial measured conclusion

**Measured project-wide acceleration: unknown.**  
**Measured CP1 queue speedup: not yet available.**

A single parallel run and a non-equivalent earlier sequential run are insufficient for a defensible percentage. No claim of 15% overall acceleration is made; that earlier figure was a planning estimate only.

## Measurement plan

1. Add per-stage monotonic start/end timestamps and durations to `report.json` and `report.md`.
2. Record queue wall time, total sum of stage durations, critical-path duration, dependency wait time, maximum observed concurrency, and execution mode.
3. Add a sequential mode that executes the exact same ready-stage set and commands, with the same setup and evidence inputs.
4. Benchmark sequential and parallel modes on the same commit and comparable GitHub-hosted runners, at least three repetitions each; compare medians.
5. Report elapsed-time reduction as `(1 - parallel_median / sequential_median) * 100%`, and speedup factor as `sequential_median / parallel_median`.
6. Track full CI job time separately from queue time. Track CP throughput/time-to-green separately before estimating whole-project acceleration.

## Safety and interpretation

All measurements concern host-side CI and simulated Flash only. They do not imply target-runtime or physical FM-1 validation. No firmware storage map, object IDs, Flash sectors, bootloader, OTA or recovery behavior is changed by this measurement record.
