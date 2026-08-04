---
description: Run the planner -> human approval -> implementer -> build/test -> reviewer workflow for one task, on this STM32F429I-DISC1 embedded repo.
argument-hint: <task description, or path to a .claude/tasks/*.md file>
---

You are running the project's planner → implementer → reviewer orchestration
for a single task. Follow this flow exactly and do not skip or reorder
steps. Full background on this workflow is in `.claude/ORCHESTRATION.md` —
read it now if you haven't already this session.

Task input: $ARGUMENTS

If the task input is a path to a file under `.claude/tasks/`, read that file
and use it as the task definition (it follows the structure in
`.claude/tasks/template.md`). If it's inline text, treat it as the task
`Goal`/`Background` and fill in the rest of the template's sections yourself
from context, flagging anything you had to guess as an open question for
the planner to surface.

## Step 1 — Planner

Dispatch the `planner` subagent with the full task input. The planner is
strictly read-only; it must not edit anything. Its output must follow the
fixed structure defined in `.claude/agents/planner.md` (Summary / Repository
findings / Acceptance criteria / Implementation steps / Files likely to
change / Risks / Verification plan / Open questions).

Present the planner's output to me in full, unedited.

## Step 2 — Human plan approval (hard gate)

**Stop here.** Do not proceed to Step 3 under any circumstances until I
have explicitly approved the plan in this conversation (e.g. "approved",
"go ahead", or approval with specific edits to the plan). Do not treat
silence, a topic change, or an unrelated message as approval. If I ask for
changes to the plan, return to Step 1 with the revised task and produce a
new plan — do not patch the old plan yourself.

## Step 3 — Implementer

Once the plan is approved, dispatch the `implementer` subagent with the
original task and the exact approved plan (including any edits I made in
Step 2). Its output must follow the fixed structure defined in
`.claude/agents/implementer.md` (Status / Summary / Files changed / Plan
deviations / Checks executed / Known limitations / Remaining risks).

Present the implementer's output in full.

## Step 4 — Deterministic build/test evidence

Independently confirm the implementer's "Checks executed" claims by running
the same deterministic commands yourself (or the defaults from
`CLAUDE.md` if none were specified) and show the **actual command output**
in your response — not a paraphrase of it. At minimum, for any change that
touches build inputs:

```sh
export PATH="/opt/st/stm32cubeclt_1.22.0/GNU-tools-for-STM32/bin:/opt/st/stm32cubeclt_1.22.0/Ninja/bin:$PATH"
cmake --preset Debug
cmake --build build/Debug
```

Also run `git status` and `git diff` and include their output — this is the
evidence the reviewer works from. Confirm nothing was committed by the
implementer (it must not be — flag it as a violation if it was).

## Step 5 — Reviewer

Dispatch the `reviewer` subagent with: the original task, the approved plan
from Step 2, the implementer's report from Step 3, and the actual
`git diff` / build output from Step 4. The reviewer is strictly read-only.
Its output must follow the fixed structure defined in
`.claude/agents/reviewer.md` (Verdict / Summary / Acceptance-criteria
coverage / Findings / Missing evidence / Suggested verification).

Present the reviewer's output in full.

## Step 6 — Final summary

Produce a final summary with exactly three labeled sections so it's clear
what kind of claim each line is:

```text
Model claims
  - Things the planner/implementer/reviewer asserted but that are not
    independently backed by command output in this conversation.

Deterministic evidence
  - Things directly backed by command output shown in Step 4 (or Step 5's
    inspection), e.g. "cmake --build succeeded, 0 errors, 2 warnings (see
    above)".

Needs human / hardware verification
  - Anything CLAUDE.md flags as unobservable by the agent: LCD output, LED
    timing, UART byte content, SWD-observed state, or any other physical
    check. List these explicitly even if the reviewer approved — a clean
    review is not hardware confirmation.
```

## Rules for the whole flow

- Do not silently retry the implementer or "auto-repair" reviewer findings.
  If the reviewer's verdict is `request changes` or lists any `blocker`/
  `major` finding, stop after Step 6 and ask me explicitly whether to start
  a repair iteration (which would restart at Step 1 with an updated task
  describing what needs fixing). Do not start one on your own.
- Do not push, publish, open a PR, or otherwise touch anything outside the
  local working tree at any step.
- If any subagent's output doesn't follow its required structure, say so
  rather than silently reformatting it — that's a signal the orchestration
  setup itself needs attention.
