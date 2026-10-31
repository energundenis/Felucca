# ENERGUN FM-1 CORE — AC79 SDK pin and CI cache

Date: 2026-10-31

## Required SDK revision

- Official remote: `https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git`
- Release tag: `AC79NN_SDK_V1.2.1_2023-12-13`
- Full commit: `d179b4484759423312073f5fbb232501aa491047`
- Annotated tag object: `b02c8c9354c077238ccbf764e36eff53782e1c16`
- Required file check: `cpu/wl82/tools/uboot.boot`

The official Gitee tag listing identifies this release and abbreviated commit `d179b44`. The full commit was resolved from the matching annotated tag object in two public Git mirrors. The mirrors are used only to independently resolve tag metadata; CI fetches source exclusively from the official Gitee remote.

## CI behavior

1. Restore `~/fw-AC79_AIoT_SDK` from GitHub Actions cache keyed by runner OS and the full commit SHA.
2. If the cache exists, verify its checked-out HEAD equals the pinned commit.
3. On a cache miss, initialize a shallow Git repository and fetch the explicit release tag from the official Gitee remote.
4. Use HTTP/1.1 and low-speed protection. Retry at most three times with bounded 20-minute fetch attempts and backoff.
5. Check out the exact full commit in detached mode and verify the full SHA again.
6. Verify the required SDK file, export `AC79_SDK`, and save the cache before firmware compilation.
7. Never fall back to a different SDK version or an unofficial mirror.

The cache is an optimization only. A cache miss still requires the official source to be reachable. If it is not, CI fails explicitly rather than silently using a different revision.

## Scope and safety

- This changes CI dependency acquisition only.
- It does not modify Felucca firmware sources, ENERGUN DSP, submodule pins, device package identity or factory firmware.
- No FM-1 flashing or device write is performed.
- First cache population still requires a successful official Gitee fetch. Subsequent runs can restore the exact pinned SDK from GitHub Actions cache.
