---
name: cdba-cli
description: Operate CDBA-connected development boards through the `cdba` command-line client for listing boards, inspecting board metadata, booting a provided image, continuing normal boot, and interacting with the board console. Use when a user asks to access or operate boards via an existing cdba server (local or remote), especially for workflows like `cdba -l`, `cdba -i -b BOARD`, and `cdba -b BOARD [boot.img]`.
---

# Cdba CLI

Use this skill to run board operations through the local `cdba` binary in `$PATH`.

## Workflow

1. Collect required inputs from the user.
2. Run preflight checks.
3. Discover boards on the server.
4. Inspect the target board.
5. Boot image (or continue existing OS boot).
6. Keep or close interactive console session as requested.

## Required Inputs

Collect these from the user before running board operations:

- `host`: cdba server hostname/IP (for remote mode)
- `board`: board identifier

If either value is missing, ask the user directly and do not assume defaults.

## Connectivity Preflight

In this environment, do not use generic SSH shell probes like `ssh <host> 'echo ok'`.
Some servers are integrated as custom shells and may reject normal shell commands.

Use `cdba` itself as the transport/authentication probe:

```bash
cdba -l -h <host>
```

Interpret common failures before continuing:

- Authentication issue: output includes `Permission denied`, `publickey`, or equivalent auth failure text.
- Server channel/session issue: output includes `administratively prohibited: Rejected` or `Broken pipe`.
- Network issue: timeout, refused, unreachable, or DNS failure.

Report the specific class of failure and stop unless the user asks to retry.

## Concurrency Rule

Run `cdba` commands one at a time for a given host and board pair.

- Never invoke multiple `cdba` commands in parallel for the same host and board (for example, do not run `-l` and `-i` concurrently for one target).
- Parallel `cdba` commands are acceptable only when targeting different boards.
- Start the next `cdba` command only after the previous one has fully exited and its result has been handled.
- Treat `cdba` operations as single-flight for a host/board pair.

## Interactive I/O Rule

For boot and console sessions, keep `cdba` attached to live stdin/stdout.

- Do not redirect or pipe interactive `cdba` output (`>`, `>>`, `|`, `tee`) unless the user explicitly asks.
- Keep the process in a TTY/session that allows sending keys and seeing UART output in real time.
- Use stdin to interact with the board and to exit cleanly with `Ctrl+a`, then `q`.

## 1) Preflight

Run:

```bash
command -v cdba
```

When a boot image is provided, verify it before booting:

```bash
test -f /path/to/boot.img
```

Use remote mode when the user provided a server host:

```bash
cdba -l -h <host>
```

If `-h` is omitted, `cdba` tries to run `cdba-server` locally.

## 2) Discover Boards

List available boards:

```bash
cdba -l -h <host>
```

## 3) Inspect Board

Fetch board details before booting:

```bash
cdba -i -b <board> -h <host>
```

## 4) Boot Flows

Boot a specific image:

```bash
cdba -b <board> -h <host> /path/to/boot.img
```

Continue boot from already-installed system (no image transfer):

```bash
cdba -b <board> -h <host>
```

Default policy: always pass an explicit boot image path for boot operations.
In this environment, image-less continue may fail or be inconsistent on custom servers.
Only run `cdba -b <board> -h <host>` without an image if the user explicitly asks for that behavior.

Useful timeout controls:

```bash
cdba -b <board> -h <host> -t <total-seconds> -T <inactivity-seconds> /path/to/boot.img
```

For boards with known long silent UEFI phases, prefer a high total timeout and no inactivity timeout, for example:

```bash
cdba -b <board> -h <host> -t 1800 -T 0 /path/to/boot.img
```

## 5) Interactive Console Controls

During a running console session, send `Ctrl+a` followed by:

- `q`: quit session
- `P`: power on
- `p`: power off
- `s`: request status update
- `V`: VBUS on
- `v`: VBUS off
- `B`: send break
- `o`: pulse power key
- `O`: toggle power key
- `f`: pulse fastboot key
- `F`: toggle fastboot key
- `a`: send literal Ctrl+a to the console

Always mention `Ctrl+a` then `q` when handing control back to the user.

## Canonical Example

Boot image with user-provided host and board:

```bash
cdba -l -h <host>
cdba -i -b <board> -h <host>
cdba -b <board> -h <host> /path/to/boot.img
```

## Execution Guidance

- Prefer running list/info before boot unless the user explicitly asks to skip checks.
- If `host` or `board` is missing, ask the user for it before executing `cdba`.
- If output contains `channel 0: open failed: administratively prohibited: Rejected` (often followed by `Broken pipe`), stop and report server-side failure immediately. Do not retry unless the user asks.
- If a board is missing from `cdba -l`, stop and report instead of guessing.
- For normal boot workflows, always provide `/path/to/boot.img` and do not omit it unless explicitly requested by the user.
- If `cdba` is long-running, keep polling and summarize key console milestones.
- Do not run `cdba` commands in parallel for the same host/board; execute them sequentially and wait for each to exit.
- Do not redirect interactive `cdba` sessions away from live stdin/stdout.
- Do not invent unsupported flags; use only options shown by local `cdba` usage.
