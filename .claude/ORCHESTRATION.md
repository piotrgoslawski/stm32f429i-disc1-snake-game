# Orchestration setup: planner → implementer → reviewer

This document describes the native Claude Code orchestration scaffolding
for this repository. **It is infrastructure only — creating it did not
change any product code.**

## Files created

```text
.claude/agents/planner.md          Read-only planning subagent
.claude/agents/implementer.md      Editing subagent (Read/Edit/Write/Bash)
.claude/agents/reviewer.md         Read-only review subagent
.claude/commands/orchestrate-feature.md   /orchestrate-feature slash command
.claude/tasks/template.md          Task definition template
.claude/settings.json              Project-level permission deny-list
.claude/ORCHESTRATION.md           This file
```

Nothing under `Core/`, `cmake/`, `CMakeLists.txt`, or any other product file
was touched.

`.claude/settings.local.json` already existed (machine-local permission
allowlist, git-ignored globally via `~/.config/git/ignore` —
`**/.claude/settings.local.json`). It was not modified. Everything else
listed above is **not** git-ignored and is tracked/committable normally,
though this setup did not commit anything — that's left for you to review
and commit deliberately.

## How to define a task

Copy `.claude/tasks/template.md` to a new file, e.g.
`.claude/tasks/2026-08-04-add-snake-collision.md`, and fill it in. The
template's sections exist because the planner/implementer/reviewer agents
are written to expect them:

- **Goal / Background** — what and why.
- **Acceptance criteria** — what the reviewer checks the diff against.
- **Constraints** — APIs to preserve, pins/peripherals already spoken for
  (see the pinout table in `README.md`), memory/timing budget.
- **Out of scope** — keeps the planner from scope-creeping and the reviewer
  from flagging work nobody asked for.
- **Build commands / Test commands** — deterministic commands to run as
  evidence. Default build commands for this repo are in `CLAUDE.md`. As of
  this writing there is no tracked, buildable automated test suite (`tests/`
  is git-ignored and only contains stray prebuilt host binaries from a prior
  experiment) — say so in the template rather than inventing a test target.
- **Hardware verification** — anything that can only be confirmed by
  looking at the physical board.

A task doesn't have to be a file — `/orchestrate-feature` also accepts an
inline task description as its argument.

## How to start the workflow

```text
/orchestrate-feature <inline task description>
```

or

```text
/orchestrate-feature .claude/tasks/2026-08-04-add-snake-collision.md
```

This runs the fixed flow documented in
`.claude/commands/orchestrate-feature.md`:

```text
task → planner → human plan approval → implementer → build/test evidence
     → reviewer → final summary
```

## How plan approval works

This is a **hard, manual gate**, not a tool-enforced one. After the planner
responds, the orchestrating agent stops and waits — it will not dispatch the
implementer until you reply with explicit approval (e.g. "approved", "go
ahead", or approval plus edits) in the conversation. A topic change or
silence is not treated as approval. If you ask for plan changes, the
orchestrator re-runs the planner rather than hand-editing the plan itself.

There is no Claude Code primitive that blocks tool execution pending
external sign-off — the gate works because the command's instructions tell
the orchestrating agent to stop and because subagent dispatch happens
inline in the conversation where you can see and interrupt it. If you run
the roles independently (see below) instead of through the command, you are
the approval gate by construction — nothing dispatches without you invoking
it.

## Which model each role uses

```text
planner:      sonnet
implementer:  sonnet
reviewer:     opus
```

Set via the `model:` field in each agent's frontmatter
(`.claude/agents/*.md`), using Claude Code's built-in model aliases
(`sonnet`, `opus`, `haiku`, or `inherit` to match the parent session's
model). These are the values supported by the installed Claude Code version
(2.1.221) at the time this setup was created.

**These assignments are editable defaults, not a considered architectural
decision** — each agent file has a comment saying so. They have not been
benchmarked against alternatives (e.g. all-opus, all-sonnet, haiku for
planner). If you want to change one, edit the `model:` line in that agent's
frontmatter; no other file needs to change.

## Running a role independently

Each subagent can be invoked directly, outside `/orchestrate-feature`, by
asking the main assistant to dispatch it by name, e.g. "use the planner
subagent to look at task X" or via the Agent/Task tool with
`subagent_type: planner`. This is useful for iterating on a plan before
committing to the full flow, or for a standalone review of a diff someone
else wrote. Independent runs don't get the command's approval gate or
evidence-collection steps for free — those only exist inside
`/orchestrate-feature`.

## What's deterministic vs. model judgment

**Deterministic:**
- The build commands (`cmake --preset Debug`, `cmake --build build/Debug`)
  and their pass/fail/warning output.
- `git status` / `git diff` output shown as evidence.
- The tool restrictions per role (planner/reviewer literally cannot call
  `Edit`/`Write`/`Bash` — enforced by each agent's `tools:` frontmatter, not
  by them choosing to behave) and the deny-list in `.claude/settings.json`
  (blocks `git push`, `sudo`, force-deletes, remote changes, unapproved
  package installs, and reads of common secret-file patterns, regardless of
  which role is running).

**Model judgment (not guaranteed, treat as claims to verify):**
- Whether the plan is actually minimal/correct/complete.
- Whether the implementer's change actually satisfies the plan.
- Every finding the reviewer reports, and its severity.
- Any statement about hardware behavior — no role in this setup can observe
  the physical board; that's always a human step.

The final-summary step in `/orchestrate-feature` exists specifically to
keep these two categories from blurring together.

## Known limitations

- **No enforced approval gate.** Step 2 of the workflow relies on the
  orchestrating agent following its instructions and on you actually
  reading the plan before approving. Nothing prevents an inattentive
  "approved" from waving through a bad plan.
- **Read-only is enforced by omission, not sandboxing.** The planner and
  reviewer have no `Edit`/`Write`/`Bash` in their `tools:` list, so they
  cannot call those tools — but this is Claude Code's per-agent tool
  allowlist, not an OS-level sandbox. Deliberately hostile instructions
  smuggled into a task file are not defended against beyond that.
- **`.claude/settings.json` deny patterns are string/glob matches** against
  the exact command Claude Code sees, not a full shell parser — e.g. `git
  push` wrapped in an unusual invocation (`sh -c "git push"`, a Makefile
  target) could in principle slip past a pattern that only matches literal
  `git push*`. Treat the deny-list as a safety net, not a guarantee.
- **No automated test suite currently exists in this repo.** `tests/` is
  git-ignored and only contains stray prebuilt host binaries from a prior
  experiment, not tracked source. Until one exists, "deterministic test
  commands" in a task effectively means "there are none" for most changes,
  and correctness rests more heavily on build success + reviewer inspection
  + hardware confirmation than a normal repo would.
- **Hardware verification is always out of band.** No role here can flash
  the board or observe it. `CLAUDE.md` already establishes that flashing
  requires explicit human confirmation before it happens at all.
- **This setup has not yet been run end-to-end on a real product task**,
  only demonstrated with a harmless read-only example (see the
  demonstration in the conversation that created this setup, or re-run one
  yourself with `/orchestrate-feature`).
