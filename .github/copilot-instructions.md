# Copilot Instructions — Arduino Workspace

## CRITICAL AGENT RULES — Read First

### Never Use `workbench.action.terminal.sendSequence`

**NEVER call `run_vscode_command` with `workbench.action.terminal.sendSequence`** to run terminal commands. This is a VS Code keyboard-sequence injector, not a terminal runner. Using it to execute scripts will cause an infinite retry loop that spams the user with popups and requires manual intervention to stop.

**Always use `terminal-tools_sendCommand`** to run commands in the terminal.

### Don't Loop on Failed Tool Calls

If a tool call fails or returns an error, **do not retry it more than twice**. Switch to an alternative tool or approach. Retrying a broken call in a loop is the #1 cause of agent lockups.

### Don't Save All Files Unnecessarily

Do not call `workbench.action.files.saveAll` or `workbench.action.files.save` unless the user has explicitly asked you to save something. File edits via `replace_string_in_file` / `create_file` are already persisted to disk.

### Terminal Reuse — Never Spawn Extra Windows

**The user hates having many terminal windows open.** Always reuse named terminals. Never create a new terminal if one already exists for that purpose.

**Protocol before every `terminal-tools_sendCommand` call:**
1. **Check first** — call `terminal-tools_listTerminals` if unsure whether a terminal exists.
2. **Reuse** — pass the existing name; `terminal-tools_sendCommand` reuses it automatically.
3. **Never call `terminal-tools_createTerminal`** unless a genuinely new purpose arises that has no existing named terminal.
4. **Keep the total terminal count low.** When a new terminal would exceed a reasonable limit, reuse `general`.

**Reading build output:** After sending a long-running build command, read results with `mcp_io_github_won_read_file` on a tee'd output file rather than polling the terminal repeatedly. Always tee build output: `... 2>&1 | Tee-Object build_out.txt`.

---

## Visual Reference Log Protocol

Copilot **cannot recover images** from summarized conversations. Once a conversation is summarized, all shared screenshots, renders, and annotated images are permanently lost. To prevent this:

### When the User Shares an Image

1. **Save the image file** to an `images/` folder in the repo:
   - VS Code stores pasted chat images at `%APPDATA%\Code\User\workspaceStorage\vscode-chat-images\` as timestamped JPEG/PNG files.
   - Copy the most recent file(s) to `images/IMG-NN.jpeg` (sequential numbering).
   - Use `Get-ChildItem "$env:APPDATA\Code\User\workspaceStorage\vscode-chat-images" | Sort-Object LastWriteTime -Descending` to find the latest images.
2. **Immediately** append an entry to `VISUAL_REFERENCE_LOG.md` with:
   - Sequential number and date
   - **Embedded image**: `![IMG-N](images/IMG-NN.jpeg)` right after the heading
   - **Detailed text description** of what the image shows — enough that a future Copilot session can understand the image without seeing it
   - What file/function the image relates to
   - Whether it shows a **problem**, a **desired state**, or a **reference**
   - What decisions or code changes were made in response
3. **Never skip this step** — even if the image seems trivial. The log is the only way to preserve visual context across sessions.
4. **Reference log entries by number** in code comments when making changes based on an image.

### When Starting a New Session

- Read `VISUAL_REFERENCE_LOG.md` before modifying any geometry or visual code. It contains the visual ground truth the user has established.

---

## Workspace Overview

Arduino C++ sketches for the ESP32-2432S028 "Cheap Yellow Display" (CYD). Uses TFT_eSPI + LVGL 8.x + XPT2046 touch. Two motor-control sketches: CYD_StepperControl (single-motor) and CYD_MillControl (dual-axis mill table).

## Arduino / ESP32 (CYD Projects)

### Hardware: ESP32-2432S028

- **Display**: ILI9341 320×240 on HSPI (SPI2), always active
- **Touch**: XPT2046 on VSPI (SPI3) with custom pin mapping (CLK=25, MOSI=32, MISO=39, CS=33)
- **SD Card**: Shares VSPI — must be used on-demand only (take over bus, release after)
- **RGB LED**: GPIOs 4 (red), 16 (green), 17 (blue) — active LOW (inverted PWM)
- **Backlight**: GPIO 21

### Configuration Files

- `User_Setup.h` → copy to `Arduino/libraries/TFT_eSPI/User_Setup.h` (pin mappings for this CYD variant)
- `lv_conf.h` → copy to `Arduino/libraries/lv_conf.h` (LVGL 8.x configuration)

### Architecture Pattern (CYD_StepperControl / CYD_MillControl)

FreeRTOS dual-core: Core 0 runs stepper motor loop (tight timing, AccelStepper), Core 1 runs LVGL UI + touch. Communication via `volatile` command enum and atomic variables — no mutexes on the stepper path.

### CYD_MillControl — Dual-Axis Mill Table

- **Motor 1 (X)**: NEMA 23 (23HS32-4004S, 4A) via DMA860S — STEP=22, DIR=27, direct drive, T8×2 lead screw, 32 microstep → 3200 steps/mm
- **Motor 2 (Y)**: NEMA 17 (~1.7A) via TB6600 — STEP=26, DIR=16 (repurposed green LED), direct drive, T8×2 lead screw, 16 microstep → 1600 steps/mm
- **Power**: 36V 10A DC PSU shared (parallel) → both drivers + buck converter (36V→5V) for CYD
- **Sequential only**: One motor active at a time; axis selector locked while motor runs
- **Limit switches**: Hardware ENA cutoff per axis (NC switches in series, no GPIOs)
- **GPIO 16 conflict**: Repurposed from green LED to Motor 2 DIR — green LED unavailable in this sketch
- **AxisConfig struct**: Per-motor stepsPerMm, speed tiers (mm/s), and acceleration stored in const array
- **TB6600 min pulse**: 5µs (vs DMA860S 2.5µs) — set per AccelStepper instance via `setMinPulseWidth()`

### Touch Calibration

GPIO 36/39 have phantom interrupt issues when WiFi is active — use polling mode for XPT2046 (no IRQ pin). Rotation = 3 for landscape.

## "Save This" Protocol

When the user says **"save this"**, **"save that"**, or **"update instructions"**:

1. **Extract** the key learnings from the current conversation — focus on facts that would be lost between sessions: new conventions, gotchas discovered through debugging, hardware findings, format requirements, tool-specific behaviors, or user preferences.
2. **Categorize** each item under the appropriate existing section, or create a new section if it doesn't fit.
3. **Deduplicate** — if the insight already exists in this file, update/refine it rather than adding a duplicate.
4. **Write concretely** — include specific values, code snippets, or filenames. Avoid vague advice like "be careful with X"; instead write "X requires Y because Z".
5. **Read this file first** before editing to avoid clobbering recent additions.
6. **Show the user** what was added/changed (brief summary, not the full file).

---

## Amazon Link Fetching Protocol

When the user asks for Amazon purchase links, follow this exact procedure. Amazon blocks simple `urllib` / `Invoke-WebRequest` calls with CAPTCHAs unless the request looks like a real browser.

### Step 1 — Write a Temporary Python Fetch Script

Create `_fetch_amazon.py` at the repo root (or a convenient location). Use **Python `urllib.request`** (no pip installs required) with a realistic Chrome User-Agent:

```python
import urllib.request, re, time

UA = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
    "AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/131.0.0.0 Safari/537.36"
)

def fetch_search(query):
    """Amazon search → list of ASIN strings."""
    url = f"https://www.amazon.com/s?k={query.replace(' ', '+')}"
    req = urllib.request.Request(url, headers={
        "User-Agent": UA,
        "Accept-Language": "en-US,en;q=0.9",
        "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9",
    })
    with urllib.request.urlopen(req, timeout=15) as resp:
        html = resp.read().decode("utf-8", errors="replace")
    asins, seen = [], set()
    for m in re.finditer(r'data-asin="([A-Z0-9]{10})"', html):
        a = m.group(1)
        if a not in seen:
            seen.add(a)
            asins.append(a)
    return asins[:10]

def fetch_product(asin):
    """Fetch product page → {asin, title, price, url}."""
    url = f"https://www.amazon.com/dp/{asin}"
    req = urllib.request.Request(url, headers={
        "User-Agent": UA,
        "Accept-Language": "en-US,en;q=0.9",
        "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9",
    })
    with urllib.request.urlopen(req, timeout=15) as resp:
        html = resp.read().decode("utf-8", errors="replace")
    title_m = re.search(r'<title[^>]*>(.*?)</title>', html, re.DOTALL)
    title = re.sub(r'\s*[-:]\s*Amazon\.com.*$', '',
                   title_m.group(1).strip()) if title_m else "No title"
    price_m = re.search(
        r'class="a-price-whole">(\d+)</span>.*?'
        r'class="a-price-fraction">(\d+)', html, re.DOTALL)
    price = f"${price_m.group(1)}.{price_m.group(2)}" if price_m else "N/A"
    return {"asin": asin, "title": title, "price": price, "url": url}
```

### Step 2 — Two-Phase Retrieval (Search → Product Pages)

Amazon search results contain ASINs but **not** readable titles (CAPTCHA-gated HTML). The working approach:

1. **Search phase**: Call `fetch_search("...")` to extract `data-asin` attributes. This reliably returns 8-10 ASINs even with a CAPTCHA warning in the HTML, because Amazon still renders the product grid.
2. **Product phase**: For the top 5 ASINs, call `fetch_product(asin)` to hit each `/dp/ASIN` page individually. These product pages return the full `<title>` tag with the product name. Add `time.sleep(0.8)` between requests to avoid rate limiting.
3. **Price extraction** often fails (Amazon lazy-loads prices via JS). Report `"N/A"` — the user will see the price when they click through.

### Step 3 — Output Format

Print results as a table the user can scan:

```
SHOPPING LIST:

  ITEM CATEGORY:
    PRICE  Product Title (truncated to 80 chars)
           https://www.amazon.com/dp/BXXXXXXXXX
```

Always include **clickable search URLs** at the bottom as fallback:
```
https://www.amazon.com/s?k=search+terms+here
```

### Step 4 — Present to User in Chat

Format the final answer as a **Markdown table** with clickable links. Include:
- Product name
- Pack quantity (important for hardware — user needs specific count)
- Direct link
- A note if any result is the wrong size/spec
- The search URL so the user can browse alternatives

### Key Gotchas

- **Do NOT use `Invoke-WebRequest` in PowerShell** via Desktop Commander `start_process` — the `$` variables in PowerShell commands get stripped by the MCP tool's argument parser, causing syntax errors. Always use a Python script.
- **Do NOT use Desktop Commander's `read_file` with `isUrl=true`** for Amazon — it sends a bare request with no User-Agent and gets HTTP 503.
- **Delete `_fetch_amazon.py`** after use — it's a throwaway helper, not part of the project.
- **Amazon CAPTCHA**: The search HTML may contain "robot" or "captcha" strings but still includes `data-asin` attributes. Don't abort on CAPTCHA warnings — extract ASINs anyway.
- **404 on some ASINs**: Some guessed/old ASINs return 404. Always prefer ASINs discovered from the live search, not hardcoded ones.

---

## OCP CAD Viewer — PowerShell Fix

The OCP CAD Viewer extension launches its Python backend via `terminal.sendText()`. On Windows with PowerShell, quoted executable paths require the `&` call operator or they're treated as string expressions and silently fail.

**Fix**: Set in `.vscode/settings.json`:
```json
{
    "OcpCadViewer.advanced.shellCommandPrefix": "& "
}
```

This prepends `& ` to every command the extension sends to the terminal, making `"C:\...\python.exe" -m ocp_vscode ...` into `& "C:\...\python.exe" -m ocp_vscode ...`.

---

## Skills — When to Load Which Skill

Skills are found in `.agents/skills/`. Load them with `read_file` on demand — don't load all of them blindly.

### Arduino / Hardware
| Skill | When to use |
|---|---|
| `arduino-code-generator` | Generate Arduino/ESP32/RP2040 code snippets: sensors, actuators, timers, protocols |
| `arduino-project-builder` | Build complete multi-component Arduino projects from scratch |
| `arduino-serial-monitor` | Read and analyze Arduino serial monitor output for debugging |
| `battery-selector` | Choose battery type, chemistry, and charging solution |
| `bom-generator` | Generate Bill of Materials / parts shopping list with supplier links |
| `circuit-debugger` | Systematic hardware debugging: no power, hot components, intermittent failures |
| `datasheet-interpreter` | Extract pin assignments, I2C addresses, timing, register maps from datasheets |
| `enclosure-designer` | Design 3D-printable enclosures; OpenSCAD templates and print settings |
| `error-message-explainer` | Interpret Arduino/ESP32 compiler errors in plain English |
| `esp32-serial-commands` | Send commands to ESP32 via serial to emulate button presses / automate testing |
| `esp32-serial-logging` | Capture real-time ESP32 serial logs to file for crash analysis |
| `power-budget-calculator` | Calculate total current draw, battery life, sleep mode savings |

### Planning & Workflow
| Skill | When to use |
|---|---|
| `brainstorming` | Before ANY creative work — explore intent and requirements first |
| `dispatching-parallel-agents` | 2+ independent tasks that can run without shared state |
| `executing-plans` | Execute a written implementation plan in a separate session with checkpoints |
| `finishing-a-development-branch` | Implementation complete — decide merge, PR, or cleanup |
| `hierarchical-agents` | Generate hierarchical AGENTS.md files for AI-efficient codebase navigation |
| `plan-builder` | Write minimal/standard/full implementation plans based on complexity |
| `subagent-driven-development` | Execute plans with independent tasks in the current session |
| `task-coordinator` | Orchestrate multi-step workflows with parallel execution and crash recovery |
| `using-git-worktrees` | Create isolated git worktrees before starting feature work |
| `writing-plans` | Write a spec or plan before touching code on multi-step tasks |

### Code Quality & Debugging
| Skill | When to use |
|---|---|
| `code-review-facilitator` | Automated code review checklist for Arduino/ESP32 projects |
| `fix-reporter` | Document a solved problem as categorized knowledge for future lookup |
| `fluff-detector` | Strip human-oriented filler from LLM outputs and skill files |
| `receiving-code-review` | Process incoming review feedback rigorously; don't blindly apply suggestions |
| `requesting-code-review` | Request review after completing a feature or before merging |
| `systematic-debugging` | Before proposing any fix — structured root-cause analysis workflow |
| `verification-before-completion` | Run and confirm output BEFORE claiming work is done or tests pass |

### Testing & TDD
| Skill | When to use |
|---|---|
| `tdd-workflow` | Red-green-refactor TDD cycle with increment decomposition |
| `test-driven-development` | Write tests before implementation code on any feature or bugfix |

### Documentation & Diagrams
| Skill | When to use |
|---|---|
| `code-story` | Transform git history into narrative documentary-style stories |
| `create-adr` | Document Architecture Decision Records for significant technical choices |
| `mermaid-builder` | Create syntactically correct Mermaid diagrams (flowcharts, sequence, ER, etc.) |
| `pr-screenshot-docs` | Before/after screenshots for PRs that include visual changes |
| `readme-craft` | Production-grade README.md patterns: hero, quick start, tables, troubleshooting |
| `readme-generator` | Auto-generate full README.md for Arduino/ESP32 projects |
| `research-compound` | Read/update folder-specific AGENTS.md before and after researching a domain |
| `structured-logging` | Production logging patterns: structured JSON, correlation IDs, log levels |

### Frontend / Design
| Skill | When to use |
|---|---|
| `design-tool-picker` | Unsure which design skill to use — decision tree to pick the right one |
| `frontend-css-patterns` | Framework-agnostic CSS: typography, color, motion, spatial composition |
| `frontend-design` | Build distinctive production-grade web components or pages |
| `frontend-design-philosophy` | Aesthetic direction and anti-patterns for non-generic UI |

### Agent & Skill Infrastructure
| Skill | When to use |
|---|---|
| `add-config-field` | Add new field to `.agents.yml` config schema (updates templates + migration) |
| `config-reader` | Read `.agents.yml` / `.agents.local.yml` with dot-notation field access |
| `init-agents-config` | Generate `.agents.yml` from scratch (Rails, Python, Node, Generic templates) |
| `skill-auditor` | Audit all installed skills — find bloated, overlapping, or improvable ones |
| `skill-linter` | Validate skills against agentskills.io spec before publishing |
| `using-superpowers` | Session start — establishes how to discover and invoke skills |
| `writing-skills` | Create new skills or edit/verify existing ones |

---

## Parallelism Protocol — Never Wait When You Can Work

**Prime directive: idle time is failure.** At no point should this agent be waiting for one thing to finish before starting another, unless a true data dependency exists. Every second of blocking is wasted time.

### Rule 1 — Classify Before Acting

Before executing ANY multi-step task, spend one moment classifying each subtask:

```
INDEPENDENT  → can start immediately, no prior result needed
DEPENDENT    → needs output X from step Y before it can begin
SEQUENTIAL   → must follow a specific order (e.g., compile then upload)
```

Dispatch ALL independent tasks simultaneously. Only queue dependent tasks behind their actual blocker — not behind unrelated tasks.

### Rule 2 — Batch All Tool Calls

Every tool invocation that does not depend on the result of another in-flight call **must** be issued in the same parallel batch. Never call tools sequentially when they could be called together.

**Wrong — sequential (blocks unnecessarily):**
```
read_file(A) → wait → read_file(B) → wait → read_file(C)
```

**Right — parallel batch:**
```
read_file(A) + read_file(B) + read_file(C)  →  all results arrive together
```

Apply this to: file reads, directory listings, grep searches, semantic searches, file edits on different files, sub-agent dispatch.

### Rule 3 — Use Sub-Agents for Blocking Work

Any task that would cause this agent to sit and wait (long compile, large file analysis, web fetch, multi-file refactor) **must** be handed off to a sub-agent via `runSubagent`. The main agent continues other work immediately.

**Trigger phrases that require sub-agent dispatch:**
- "build and check for errors" → sub-agent builds, tees output to `build_out.txt`, main agent reads file when ready
- "search the codebase for X" → sub-agent does exhaustive search, returns findings
- "analyze all files in folder Y" → sub-agent reads and summarizes, returns structured result
- "compile and upload to ESP32" → sub-agent handles the full flash cycle

**Sub-agent prompt must include:**
1. Exact scope — what files, what task
2. What to return — specific format, not "a summary"
3. What NOT to do — don't touch other files, don't ask questions

### Rule 4 — Never Poll; Tee and Read

When a terminal command runs long, **never** poll the terminal repeatedly waiting for output. Always tee output to a file and proceed with unblocked work:

```powershell
arduino-cli compile ... 2>&1 | Tee-Object build_out.txt
```

Return to read `build_out.txt` only after completing other available work or when the file is confirmed written.

### Rule 5 — Dependency Graph Before Long Tasks

For tasks with 4+ steps, sketch a dependency graph mentally (or explicitly) before starting:

```
[Read file A] ──┐
[Read file B] ──┼──→ [Analyze together] ──→ [Write result]
[Read file C] ──┘

[Fetch schema] ──→ [Generate code]   (independent track — runs in parallel)
```

Dispatch all root nodes (no dependencies) simultaneously. Only block a node behind its actual upstream output.

### Rule 6 — Independent File Edits Are Always Parallel

When making changes to multiple files that don't depend on each other, **always** use `multi_replace_string_in_file` or dispatch simultaneous `replace_string_in_file` calls in one batch. Never edit file A, wait, then edit file B.

### Anti-Patterns — Never Do These

| Anti-pattern | Why it's wrong | Fix |
|---|---|---|
| Read file → wait → read another file | Both reads are independent | Batch both reads together |
| Edit file A → wait → edit file B | Files are independent | Use `multi_replace_string_in_file` |
| Run compile → wait → start writing docs | Docs don't need compile result | Start docs immediately, check compile output when available |
| Search for X → wait → search for Y | Both searches are independent | Launch both searches in parallel |
| Ask user to wait while building | Never make user wait for blocking work | Sub-agent handles build, main thread continues |
| Retry failed tool call in a loop | Infinite loop, no progress | Max 2 retries, then switch approach |

### Decision Checklist — Before Every Action

Ask these questions in order:
1. **Can I do multiple things right now without one blocking another?** → YES: batch them all.
2. **Will this take more than a few seconds and block me?** → YES: sub-agent + tee output.
3. **Do I need result X before I can start Y?** → NO: start Y immediately alongside X.
4. **Am I about to call a tool sequentially that I called last time?** → Check if next call can be batched with it.

### Parallelism Patterns for This Workspace

**Arduino build + doc update (common):**
```
Sub-agent: compile sketch, tee to build_out.txt
Main agent: update README / comments / wiring notes simultaneously
Main agent: read build_out.txt when sub-agent confirms completion
```

**Multi-file refactor (common):**
```
Read all affected files in one parallel batch
Plan all edits
Apply all edits in one multi_replace_string_in_file call
```

**Research + implementation (common):**
```
Sub-agent: search codebase for all usages of function X
Main agent: draft the new implementation based on known interface
Merge: integrate sub-agent findings into draft
```
