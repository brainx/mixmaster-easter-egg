# Remailer Server Architecture (NEW-2026)

> New module — not in vintage Mixmaster 3.0. Source files tagged `NEW-2026`. Full changelog: [`MODERNIZATION.md`](../../MODERNIZATION.md)

Mixmaster 3.0 remailer code is split into a small pipeline instead of one monolithic `mix_decrypt()` in `rem.c`.

## Pipeline

```
ingest (mailin) → classify → dispatch → pool → outbound flush
```

| Stage | Module | Entry points |
|-------|--------|----------------|
| **Classify** | `remailer_dispatch.c` | `remailer_classify()` — parse headers, detect Type I/II/admin mail |
| **Dispatch** | `remailer_dispatch.c` | `remailer_dispatch()` — route to crypto, admin, or logging |
| **Admin / abuse** | `remailer_admin.c` | `remailer_blockrequest()`, `remailer_get_otherrequests_reply()` |
| **Server loop** | `remailer_server.c` | `remailer_server_tick()`, `remailer_server_daemon()` |
| **Legacy shim** | `remailer_dispatch.c` | `mix_decrypt()` → `remailer_process_message()` |

## Binaries

| Binary | Role |
|--------|------|
| `mixmaster` | Client + optional remailer flags (unchanged CLI) |
| `mixremailer` | **Server-only** — `-R` stdin, `-M` maintain, `-S` flush pool, `-D` daemon |

## What stayed in `rem.c`

Pool helpers (`mix_pool`, `logmail`, `t2_decrypt`), expiry (`idexp`, `pgpmaxexp`), dummy traffic.

Type I crypto remains in `rem1.c`; Type II packet handling in `rem2.c`; pool batching in `pool.c`.
