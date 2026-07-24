# obs-multi-rtmp Enhancements — Design Spec

**Date:** 2026-07-24
**Fork:** https://github.com/njbolt3/obs-multi-rtmp
**Target platform (primary):** Windows. All work must remain cross-platform (Windows/macOS/Linux) per the existing CI matrix.
**Audience:** implementation plan author + reviewer.

## Goal

Add a set of reliability, usability, and automation features to the obs-multi-rtmp OBS plugin, and pay down targeted code-quality debt, without destabilizing the existing, working codebase. Changes target the personal fork; upstreaming is not a goal, but changes should stay modular enough to keep it possible.

## Non-goals

- No full mock/abstraction layer over the OBS runtime (rejected: too invasive for the payoff).
- No rewrite of the existing widget architecture.
- No new external runtime dependencies (test-only header libs are allowed, vendored like `dep/nlohmann-json`).
- No changes to the plugin's packaging/signing pipeline behavior in default builds.

## Constraints & context

- C++17, Qt6, built on `obs-plugintemplate` with CMake presets and CI for all three platforms.
- Almost all runtime logic calls into the live OBS API (`obs_output_*`, `obs_encoder_*`, `obs_service_*`), which cannot execute in a unit test.
- Config is persisted per-profile as `obs-multi-rtmp.json` in the profile dir. The loader already tolerates missing fields via `GetJsonField<T>(...).value_or(...)`, so additive schema changes are backward-compatible.
- Server URL and stream key are rendered by OBS's **generic service properties** UI, not a custom form. OBS already masks the stream key (password field) for the RTMP-custom service.

## Testing strategy (decided: "extract-and-unit-test pure logic")

Extract OBS-independent logic into small free functions and unit-test them with a vendored single-header framework (**doctest**, added at `dep/doctest/doctest.h`). A new `test/` target builds only when the CMake option `ENABLE_TESTS` is `ON` (default `OFF`), so plugin packaging is unaffected; a dedicated CI job turns it on.

Logic that remains OBS-coupled is left as-is behind thin seams — no attempt to unit-test OBS interaction.

## Config schema additions (backward-compatible)

Add to `OutputTargetConfig` (`src/output-config.h`), each serialized in `SaveTarget`/`LoadTargetConfig` with a default so old files load unchanged:

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `autoStart` | `bool` | `false` | Start this target on OBS launch |
| `autoRestart` | `bool` | `false` | Watchdog restarts target on error-stop |
| `maxRestarts` | `int` | `5` | Watchdog retry cap per streaming session |

Hotkey bindings are persisted using OBS's own `obs_hotkey_save`/`obs_hotkey_load` blob, stored under a new top-level `hotkeys` key in the config JSON (not per-target struct fields).

---

## Phase 0 — Foundations

Rationale: land the test harness and de-risking fixes before feature work, so every later phase has regression coverage and a stable base.

### 0.1 Test harness
- Vendor `dep/doctest/doctest.h`.
- Add `test/CMakeLists.txt` and `test/*.cpp`; wire an `ENABLE_TESTS` option (default OFF) into the top-level `CMakeLists.txt`.
- Add a CI job (`.github/workflows/`) that configures with `-DENABLE_TESTS=ON` and runs the test binary. Prefer extending existing workflow(s); keep the default build path unchanged.

### 0.2 Extract testable logic
New translation units, each with a header exposing pure functions and a doctest file:
- `src/stream-format.{h,cpp}`:
  - `std::optional<std::pair<int,int>> ParseResolution(std::string_view)` — moved from `PushWidgetImpl::ParseResolution` (push-widget.cpp:259); regex hoisted to a function-local `static const`.
  - `std::string FormatBitrate(double bps)` — extracted from the `strBps` lambda (push-widget.cpp:519).
  - `std::string FormatDuration(std::chrono::seconds)` — extracted from the duration formatting (push-widget.cpp:504).
- `src/target-order.{h,cpp}`:
  - `std::list<OutputTargetConfigPtr> ReorderTargets(const std::vector<std::string>& idOrder, const std::list<OutputTargetConfigPtr>& current)` — the algorithm currently inline in `MultiOutputWidget::OnOutputMoved` (obs-multi-rtmp.cpp:298). Preserves the "keep unmatched in previous order" behavior.
- `src/config-serialization.{h,cpp}`:
  - `std::string ConfigToJsonString(const MultiOutputConfig&)` and `MultiOutputConfig ConfigFromJsonString(std::string_view, std::string* errorOut)`.
  - Move the existing `static` `SaveTarget`/`LoadTargetConfig`/…/`SaveMultiOutputConfig(MultiOutputConfig&)`/`LoadMultiOutputConfig(const std::string&)` bodies here, **logger-free** (drop the `blog` calls or route them through an optional callback). The file-I/O wrappers `SaveMultiOutputConfig()`/`LoadMultiOutputConfig()` in `output-config.cpp` then call these.

Tests: round-trip config (including all new fields and the `.value_or` compatibility path for legacy files missing them), resolution parsing edge cases, bitrate/duration formatting boundaries, and reorder correctness (including unmatched/duplicate/missing IDs).

### 0.3 Threading fix — `OnStopped`
Move `ReleaseOutputEncoder()` and `ReleaseOutputSceneView()` (push-widget.cpp:852-853) inside the `RunInUIThread` task in `OnStopped`, so all output-object lifecycle transitions run on the UI thread — consistent with `~PushWidgetImpl`/`ReleaseOutput`. Verify no OBS-internal reference to the encoder remains at that point (the "stop" signal has already fired).

### 0.4 `assert`-as-control-flow → explicit handling
Replace asserts that guard recoverable runtime/config conditions with logging + graceful fallback:
- push-widget.cpp:304, 345 (missing encoder config → already falls back to shared main encoder).
- edit-widget.cpp:447, 463 (invalid protocol → already falls back to first protocol).
Keep asserts only for genuine programmer-invariant violations.

### 0.5 Minor cleanups
- Consistent `QMessageBox` ownership (stack-allocate the modal `exec()` dialogs, or parent + `deleteLater`).
- Any other trivially-safe tidy-ups discovered during extraction; nothing behavior-changing.

**Phase 0 exit criteria:** plugin builds unchanged on all platforms; `ENABLE_TESTS=ON` build runs green in CI; no behavior change observable to users.

---

## Phase 1 — Reliability (Tier 1)

### 1.1 Dropped-frames / connection-health indicator
- In `UpdateStreamStatus` (push-widget.cpp:487, 1s timer), additionally read `obs_output_get_frames_dropped()`, `obs_output_get_total_frames()`, and `obs_output_get_congestion()`.
- Pure logic (`src/stream-health.{h,cpp}`, unit-tested): `enum class Health { Good, Warn, Bad }; Health HealthFromStats(uint64_t dropped, uint64_t total, float congestion)`. Thresholds (tunable, documented in code): Good `<1%` dropped and low congestion; Warn `1–5%` or rising congestion; Bad `>5%` or high congestion.
- UI: add a small colored indicator (QLabel styled via stylesheet) to the `PushWidgetImpl` grid near the name; append dropped-% to the status line text.

### 1.2 Failure notifications
- Trigger on `OnStopped(code)` with `code != 0` (OBS has fully stopped, incl. after reconnect exhaustion).
- Three surfaces: (a) red in-dock banner — enhance the existing `SetMsg` error path with error styling; (b) `QSystemTrayIcon::showMessage` balloon; (c) `blog(LOG_WARNING, …)`.
- New `src/notifier.{h,cpp}`: a small singleton owning one `QSystemTrayIcon`. If the tray is unavailable (`QSystemTrayIcon::isSystemTrayAvailable()` false), degrade to banner + log only. Include the target name and a human-readable reason mapped from the stop code (the code→message mapping already exists at push-widget.cpp:829-849; reuse it).

### 1.3 Global hotkeys
- Register "Start All" and "Stop All" once (frontend hotkeys via `obs_hotkey_register_frontend`) at dock construction.
- Register per-target start/stop hotkeys in `AddPushWidget`; unregister in `DeletePushWidget`. Use a stable hotkey name derived from the target `id`.
- Persist bindings via `obs_hotkey_save` into the config `hotkeys` blob on save; restore via `obs_hotkey_load` after targets are recreated in `LoadConfig`.

**Phase 1 exit criteria:** health indicator reflects real dropped-frame/congestion state during a live stream; killing a target's ingest produces a tray balloon + banner + log; hotkeys start/stop targets and survive a profile reload.

---

## Phase 2 — Usability (Tier 2)

### 2.1 Config export / import
- Two buttons in the dock (near the existing Start All / Stop All row).
- Export: `QFileDialog` save → `ConfigToJsonString(GlobalMultiOutputConfig())` to a `.json` file.
- Import: `QFileDialog` open → `ConfigFromJsonString`; prompt **Replace** vs **Merge**.
  - Merge uses pure `RemapImportedIds(imported, existing)` (unit-tested) to regenerate colliding target/encoder-config IDs and rewrite references, so imported targets can't clobber existing ones.
- After import, rebuild the widget list via the existing `LoadConfig()` path and `SaveConfig()`.

### 2.2 Aggregate upload meter
- A label at the top of the dock showing combined current upload bitrate across active targets.
- Add `double CurrentBitrateBps() const` to `PushWidget` (value already computed each tick in `UpdateStreamStatus`; store the last interval value in a member).
- `MultiOutputWidget` sums across `GetAllPushWidgets()` on the same 1s cadence and renders via `FormatBitrate`.

### 2.3 Platform presets
- A preset combo box in the Service tab of the edit dialog (built in `CreateOutputSettingsWidget`/`updateServiceTab`, edit-widget.cpp:418-458).
- Presets (Twitch, YouTube, Kick, TikTok, Facebook, Custom) map to known ingest `server` URLs. Selecting a preset writes `server` into `config_->serviceParam` and re-renders the service properties; the key field stays user-entered.
- The preset list lives in a small data table (`src/platform-presets.{h,cpp}`); the URL-injection helper is pure and unit-tested.
- Stream-key masking: **not implemented** — OBS already renders the RTMP-custom key as a password field. Verify during implementation; only add handling if a supported protocol is found to render the key in cleartext.

**Phase 2 exit criteria:** a config exports and re-imports (both replace and merge) with identical behavior and no ID collisions; the aggregate meter matches the sum of per-target bitrates; selecting a preset fills the correct ingest URL.

---

## Phase 3 — Automation (Tier 3)

### 3.1 Auto-start on launch
- `autoStart` checkbox added to the "Other Settings" group box (edit-widget.cpp:637-644), alongside sync-start/stop.
- On the frontend "finished loading" event, start each `autoStart` target. Targets sharing the main output's encoder must wait until the main output's encoders exist; guard accordingly (reuse the existing encoder-availability check in `PrepareOutputEncoders`, which already surfaces a "start streaming/recording first" path).

### 3.2 Watchdog auto-restart
- `autoRestart` checkbox in the same group box; `maxRestarts` cap.
- Pure state machine (`src/watchdog.{h,cpp}`, unit-tested): `struct WatchdogState { int retries; ... }; WatchdogAction Decide(WatchdogState&, int stopCode, bool manualStop, std::chrono::steady_clock::duration sinceLastStart)` → `Restart(delay)` with backoff (e.g. 5s → 15s → 30s, capped) or `GiveUp`.
- Track whether a stop was user-initiated vs error (set a flag in `StopStreaming`/`Stop` vs `OnStopped(code≠0)`) so the watchdog never fights an intentional stop. Restarts are scheduled via `QTimer` on the UI thread. Reset retry count on a successful `OnStarted`.

**Phase 3 exit criteria:** `autoStart` targets come up after launch (respecting shared-encoder ordering); pulling the plug on a target with `autoRestart` triggers backed-off restarts up to the cap and then gives up with a notification; a manual stop is never auto-restarted.

---

## Risks & open questions

- **Auto-start timing (3.1):** starting at launch before OBS's main output/encoders are ready can fail for shared-encoder targets. Mitigation: gate on frontend-ready + existing encoder-availability check; targets with their own encoder can start immediately.
- **Tray notifications (1.2):** `QSystemTrayIcon` behavior varies; on some environments the icon must be visible to post messages. Mitigation: keep one owned icon and the banner+log fallback.
- **Hotkey persistence (1.3):** dynamic per-target registration + save/load ordering needs care so bindings reattach to the right target after `LoadConfig`. Covered by the Phase 1 exit criteria (survive profile reload).
- **Threading fix (0.3):** needs live verification that no OBS-internal reference to the encoder remains when released on the UI thread post-stop.

## Verification approach

- Unit tests (doctest) for all extracted/new pure logic: serialization round-trip, reorder, resolution/bitrate/duration formatting, health thresholds, ID remap, watchdog decisions.
- Manual/live verification per phase exit criteria on the Windows build (primary) and at least a macOS smoke build.
- CI must stay green on all three platforms with default (`ENABLE_TESTS=OFF`) builds, plus the new tests job.

## Phasing summary

Each phase is independently shippable and leaves the plugin in a coherent state:
- **Phase 0:** harness + extraction + threading/assert fixes (no user-visible change).
- **Phase 1:** health indicator, failure notifications, hotkeys.
- **Phase 2:** export/import, aggregate meter, platform presets.
- **Phase 3:** auto-start, watchdog auto-restart.
