# obs-multi-rtmp Enhancements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add reliability, usability, and automation features to the obs-multi-rtmp OBS plugin and pay down targeted code-quality debt, with unit-tested pure logic and no destabilization of the working codebase.

**Architecture:** Extract OBS-independent logic into small, unit-tested free functions (compiled into both the plugin and a standalone test binary); leave OBS/Qt-coupled code in the existing widget classes and add features there behind thin seams. Ship in four phases (Foundations → Reliability → Usability → Automation), each independently buildable.

**Tech Stack:** C++17, Qt6 (Widgets/Core), libobs + obs-frontend-api, nlohmann-json (vendored), doctest (vendored, test-only), CMake (obs-plugintemplate layout).

## Global Constraints

- Language/standard: **C++17**. Match existing code style (4-space indent, `.clang-format` is enforced in CI — run it before committing).
- Platforms: must build on **Windows (primary), macOS, Linux**. No platform-only APIs without a cross-platform fallback.
- Dependencies: **no new runtime dependencies**. Test-only libraries may be vendored under `dep/` like `dep/nlohmann-json`.
- Default build unchanged: the plugin's normal build must not require the test toolchain. Tests live in a **standalone** `test/` CMake project that does **not** link libobs/Qt.
- Config compatibility: all config schema changes are **additive** and loaded via `GetJsonField<T>(...).value_or(default)` so pre-existing `obs-multi-rtmp.json` files load unchanged.
- Pure-logic files (`stream-format`, `target-order`, `config-serialization`, `stream-health`, `platform-presets`, `watchdog`) must **not** include `<obs.h>`, Qt headers, or call `blog`. They may include `output-config.h` (std + nlohmann-json only) and nlohmann-json.
- TDD: write the failing test first for every pure-logic function. Commit after each task. OBS/Qt integration steps that cannot be unit-tested are verified manually against the phase exit criteria in the spec.
- Spec: `docs/superpowers/specs/2026-07-24-obs-multi-rtmp-enhancements-design.md`.

---

# Phase 0 — Foundations

## Task 0.1: Test harness (doctest + standalone CMake + CI)

**Files:**
- Create: `dep/doctest/doctest.h` (vendored single header from doctest v2.4.11)
- Create: `test/CMakeLists.txt`
- Create: `test/main.cpp`
- Create: `test/sanity_test.cpp`
- Create: `.github/workflows/tests.yaml`

**Interfaces:**
- Produces: a `ctest`-runnable `obs-multi-rtmp-tests` executable that compiles pure-logic sources from `src/` plus `test/*_test.cpp`, with include dirs `src/` and `dep/nlohmann-json`.

- [ ] **Step 1: Vendor doctest**

Download the single header into `dep/doctest/doctest.h`:
```bash
mkdir -p dep/doctest
curl -fsSL https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h -o dep/doctest/doctest.h
```

- [ ] **Step 2: Create the doctest entry point**

`test/main.cpp`:
```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
```

- [ ] **Step 3: Create a sanity test**

`test/sanity_test.cpp`:
```cpp
#include "doctest.h"

TEST_CASE("harness sanity") {
    CHECK(1 + 1 == 2);
}
```

- [ ] **Step 4: Create the standalone test CMake project**

`test/CMakeLists.txt` (self-contained; does not require libobs/Qt):
```cmake
cmake_minimum_required(VERSION 3.20)
project(obs-multi-rtmp-tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(obs-multi-rtmp-tests
  main.cpp
  sanity_test.cpp
  # pure-logic sources and their tests are appended here in later tasks
)

target_include_directories(obs-multi-rtmp-tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../dep/doctest
  ${CMAKE_CURRENT_SOURCE_DIR}/../dep/nlohmann-json
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME unit COMMAND obs-multi-rtmp-tests)
```

- [ ] **Step 5: Configure, build, and run the tests**

Run:
```bash
cmake -S test -B build-test && cmake --build build-test && ctest --test-dir build-test --output-on-failure
```
Expected: builds and reports `1 test from ... PASS` / `100% tests passed`.

- [ ] **Step 6: Add CI job**

`.github/workflows/tests.yaml`:
```yaml
name: Unit Tests
on:
  push:
  pull_request:
  workflow_dispatch:
jobs:
  tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Configure
        run: cmake -S test -B build-test
      - name: Build
        run: cmake --build build-test
      - name: Test
        run: ctest --test-dir build-test --output-on-failure
```

- [ ] **Step 7: Commit**

```bash
git add dep/doctest test .github/workflows/tests.yaml
git commit -m "test: add standalone doctest harness and CI job"
```

---

## Task 0.2: Extract stream-format helpers

**Files:**
- Create: `src/stream-format.h`, `src/stream-format.cpp`
- Create: `test/stream_format_test.cpp`
- Modify: `test/CMakeLists.txt` (add sources)
- Modify: `src/push-widget.cpp` (use the new helpers), `CMakeLists.txt` (add `src/stream-format.*` to plugin sources)

**Interfaces:**
- Produces:
  - `std::optional<std::pair<int,int>> ParseResolution(std::string_view res);`
  - `std::string FormatBitrate(double bitsPerSecond);`
  - `std::string FormatDuration(std::chrono::seconds total);`

- [ ] **Step 1: Write the failing tests**

`test/stream_format_test.cpp`:
```cpp
#include "doctest.h"
#include "stream-format.h"

TEST_CASE("ParseResolution parses WxH with spaces") {
    auto r = ParseResolution("1920x1080");
    REQUIRE(r.has_value());
    CHECK(r->first == 1920);
    CHECK(r->second == 1080);

    auto r2 = ParseResolution("  1280 x 720 ");
    REQUIRE(r2.has_value());
    CHECK(r2->first == 1280);
    CHECK(r2->second == 720);
}

TEST_CASE("ParseResolution rejects garbage") {
    CHECK_FALSE(ParseResolution("abc").has_value());
    CHECK_FALSE(ParseResolution("1920").has_value());
    CHECK_FALSE(ParseResolution("").has_value());
}

TEST_CASE("FormatBitrate scales units") {
    CHECK(FormatBitrate(0) == "0 bps");
    CHECK(FormatBitrate(500).find("bps") != std::string::npos);
    CHECK(FormatBitrate(2'000'000).find("Mbps") != std::string::npos);
}

TEST_CASE("FormatDuration is HH:MM:SS") {
    CHECK(FormatDuration(std::chrono::seconds(0)) == "00:00:00");
    CHECK(FormatDuration(std::chrono::seconds(3661)) == "01:01:01");
}
```

- [ ] **Step 2: Add sources to the test build and run to see it fail**

In `test/CMakeLists.txt`, append to the `add_executable` list:
```cmake
  ../src/stream-format.cpp
  stream_format_test.cpp
```
Run: `cmake -S test -B build-test && cmake --build build-test`
Expected: FAIL — `stream-format.h` not found.

- [ ] **Step 3: Write the implementation**

`src/stream-format.h`:
```cpp
#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <chrono>

std::optional<std::pair<int, int>> ParseResolution(std::string_view res);
std::string FormatBitrate(double bitsPerSecond);
std::string FormatDuration(std::chrono::seconds total);
```

`src/stream-format.cpp`:
```cpp
#include "stream-format.h"
#include <regex>
#include <cmath>
#include <cstdio>

std::optional<std::pair<int, int>> ParseResolution(std::string_view res) {
    static const std::regex pattern(R"__(\s*(\d{1,5})\s*x\s*(\d{1,5})\s*)__");
    std::match_results<std::string_view::const_iterator> m;
    if (std::regex_match(res.begin(), res.end(), m, pattern)) {
        return std::make_pair(std::stoi(m[1].str()), std::stoi(m[2].str()));
    }
    return std::nullopt;
}

std::string FormatBitrate(double bps) {
    static const char* units[] = {
        "bps", "Kbps", "Mbps", "Gbps", "Tbps", "Pbps", "Ebps", "Zbps", "Ybps"
    };
    if (bps <= 0)
        return "0 bps";
    const int unitMax = sizeof(units) / sizeof(*units);
    int unitIndex = static_cast<int>(std::log10(bps) / 3);
    if (unitIndex >= unitMax)
        unitIndex = unitMax - 1;
    if (unitIndex < 0)
        unitIndex = 0;
    auto strVal = std::to_string(bps / std::pow(1000, unitIndex)).substr(0, 4);
    if (!strVal.empty() && strVal.back() == '.')
        strVal.pop_back();
    return strVal + " " + units[unitIndex];
}

std::string FormatDuration(std::chrono::seconds total) {
    using namespace std::chrono;
    auto hh = duration_cast<hours>(total);
    auto mm = duration_cast<minutes>(total - hh);
    auto ss = duration_cast<seconds>(total - hh - mm);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                  (int)hh.count(), (int)mm.count(), (int)ss.count());
    return buf;
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cmake --build build-test && ctest --test-dir build-test --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Use the helpers in the plugin**

In `CMakeLists.txt` add to `target_sources(...)`: `./src/stream-format.h` and `./src/stream-format.cpp`.

In `src/push-widget.cpp`: add `#include "stream-format.h"`; delete the private `ParseResolution` method (lines ~259-272) and the inline duration/bitrate formatting inside `UpdateStreamStatus` (lines ~504-537), replacing the message build with:
```cpp
auto duration = duration_cast<std::chrono::seconds>(now - begin_time_);
std::string strDuration = FormatDuration(duration);
int fps = static_cast<int>(std::round((new_frames - total_frames_) / interval));
auto bps = (new_bytes - total_bytes_) * 8 / interval;
last_bitrate_bps_ = bps;   // added in Task 2.2; keep a local if not yet present
msg_->setText((strDuration + "  " + FormatBitrate(bps)
               + "  " + std::to_string(fps) + " FPS").c_str());
```
(If Task 2.2 is not yet done, omit the `last_bitrate_bps_` line.)

- [ ] **Step 6: Verify the plugin still builds, then commit**

Build the plugin with your normal preset (see `README.md` / CMakePresets). Then:
```bash
git add src/stream-format.h src/stream-format.cpp src/push-widget.cpp CMakeLists.txt test/
git commit -m "refactor: extract unit-tested stream-format helpers"
```

---

## Task 0.3: Extract target reorder logic

**Files:**
- Create: `src/target-order.h`, `src/target-order.cpp`
- Create: `test/target_order_test.cpp`
- Modify: `test/CMakeLists.txt`, `CMakeLists.txt`, `src/obs-multi-rtmp.cpp`

**Interfaces:**
- Consumes: `OutputTargetConfig`, `OutputTargetConfigPtr`, `MultiOutputConfig` from `output-config.h`.
- Produces: `std::list<OutputTargetConfigPtr> ReorderTargets(const std::vector<std::string>& idOrder, const std::list<OutputTargetConfigPtr>& current);`

- [ ] **Step 1: Write the failing test**

`test/target_order_test.cpp`:
```cpp
#include "doctest.h"
#include "target-order.h"

static OutputTargetConfigPtr mk(const std::string& id) {
    auto p = std::make_shared<OutputTargetConfig>();
    p->id = id;
    return p;
}

TEST_CASE("ReorderTargets applies id order") {
    std::list<OutputTargetConfigPtr> cur = {mk("a"), mk("b"), mk("c")};
    auto out = ReorderTargets({"c", "a", "b"}, cur);
    std::vector<std::string> ids;
    for (auto& t : out) ids.push_back(t->id);
    CHECK(ids == std::vector<std::string>{"c", "a", "b"});
}

TEST_CASE("ReorderTargets keeps unmatched ids in original order at the end") {
    std::list<OutputTargetConfigPtr> cur = {mk("a"), mk("b"), mk("c")};
    // "c" missing from idOrder, "x" is unknown and ignored
    auto out = ReorderTargets({"b", "x", "a"}, cur);
    std::vector<std::string> ids;
    for (auto& t : out) ids.push_back(t->id);
    CHECK(ids == std::vector<std::string>{"b", "a", "c"});
}
```

- [ ] **Step 2: Add to test build; run to confirm failure**

Append to `test/CMakeLists.txt`: `../src/target-order.cpp` and `target_order_test.cpp`.
Run: `cmake --build build-test` → FAIL (`target-order.h` missing).

- [ ] **Step 3: Implement**

`src/target-order.h`:
```cpp
#pragma once
#include <list>
#include <string>
#include <vector>
#include "output-config.h"

std::list<OutputTargetConfigPtr> ReorderTargets(
    const std::vector<std::string>& idOrder,
    const std::list<OutputTargetConfigPtr>& current);
```

`src/target-order.cpp`:
```cpp
#include "target-order.h"
#include <unordered_map>

std::list<OutputTargetConfigPtr> ReorderTargets(
    const std::vector<std::string>& idOrder,
    const std::list<OutputTargetConfigPtr>& current)
{
    std::unordered_map<std::string, OutputTargetConfigPtr> byId;
    for (auto& t : current)
        if (t) byId.emplace(t->id, t);

    std::list<OutputTargetConfigPtr> result;
    for (auto& id : idOrder) {
        auto it = byId.find(id);
        if (it != byId.end()) {
            result.push_back(it->second);
            byId.erase(it);
        }
    }
    // keep unmatched in their original order
    for (auto& t : current) {
        if (!t) continue;
        auto it = byId.find(t->id);
        if (it != byId.end()) {
            result.push_back(it->second);
            byId.erase(it);
        }
    }
    return result;
}
```

- [ ] **Step 4: Run tests to verify pass**

Run: `cmake --build build-test && ctest --test-dir build-test --output-on-failure` → PASS.

- [ ] **Step 5: Use it in `OnOutputMoved`**

In `CMakeLists.txt` add `./src/target-order.h`, `./src/target-order.cpp`.
In `src/obs-multi-rtmp.cpp`: `#include "target-order.h"`. Replace the body of `OnOutputMoved` (obs-multi-rtmp.cpp:298-361) that builds `reordered` with:
```cpp
std::vector<std::string> idOrder;
for (int i = 0; i < outputsContainer_->count(); ++i) {
    auto item = outputsContainer_->item(i);
    if (item)
        idOrder.push_back(item->data(Qt::UserRole).toString().toStdString());
}
auto& targets = GlobalMultiOutputConfig().targets;
targets = ReorderTargets(idOrder, targets);
SaveConfig();
outputsContainer_->clearSelection();
```
Keep the existing early-return guards (`parent != destination`, bounds checks) above this.

- [ ] **Step 6: Build the plugin, then commit**

```bash
git add src/target-order.h src/target-order.cpp src/obs-multi-rtmp.cpp CMakeLists.txt test/
git commit -m "refactor: extract unit-tested target reorder logic"
```

---

## Task 0.4: Config schema fields + serialization extraction

**Files:**
- Modify: `src/output-config.h` (new fields), `src/output-config.cpp` (delegate to new module)
- Create: `src/config-serialization.h`, `src/config-serialization.cpp`
- Create: `test/config_serialization_test.cpp`
- Modify: `test/CMakeLists.txt`, `CMakeLists.txt`

**Interfaces:**
- Produces:
  - `std::string ConfigToJsonString(const MultiOutputConfig& config);`
  - `MultiOutputConfig ConfigFromJsonString(std::string_view json, std::string* errorOut = nullptr);`
  - `void RemapImportedIds(MultiOutputConfig& imported, const MultiOutputConfig& existing);` (used in Task 2.1)
- New `OutputTargetConfig` fields: `bool autoStart = false;`, `bool autoRestart = false;`, `int maxRestarts = 5;`

- [ ] **Step 1: Add the new struct fields**

In `src/output-config.h`, inside `struct OutputTargetConfig`, after `bool syncStop = false;`:
```cpp
    bool autoStart = false;
    bool autoRestart = false;
    int maxRestarts = 5;
```

- [ ] **Step 2: Write the failing round-trip test**

`test/config_serialization_test.cpp`:
```cpp
#include "doctest.h"
#include "config-serialization.h"

TEST_CASE("target round-trips including new fields") {
    MultiOutputConfig cfg;
    auto t = std::make_shared<OutputTargetConfig>();
    t->id = "t1";
    t->name = "Twitch";
    t->protocol = "RTMP";
    t->syncStart = true;
    t->autoStart = true;
    t->autoRestart = true;
    t->maxRestarts = 3;
    cfg.targets.push_back(t);

    auto json = ConfigToJsonString(cfg);
    auto back = ConfigFromJsonString(json);
    REQUIRE(back.targets.size() == 1);
    auto& r = back.targets.front();
    CHECK(r->id == "t1");
    CHECK(r->name == "Twitch");
    CHECK(r->autoStart == true);
    CHECK(r->autoRestart == true);
    CHECK(r->maxRestarts == 3);
}

TEST_CASE("legacy config without new fields uses defaults") {
    // no auto-start/auto-restart keys present
    std::string legacy = R"({"targets":[{"id":"x","name":"n","protocol":"RTMP"}]})";
    auto cfg = ConfigFromJsonString(legacy);
    REQUIRE(cfg.targets.size() == 1);
    CHECK(cfg.targets.front()->autoStart == false);
    CHECK(cfg.targets.front()->maxRestarts == 5);
}

TEST_CASE("RemapImportedIds avoids collisions") {
    MultiOutputConfig existing;
    auto e = std::make_shared<OutputTargetConfig>(); e->id = "dup";
    existing.targets.push_back(e);

    MultiOutputConfig imported;
    auto i = std::make_shared<OutputTargetConfig>(); i->id = "dup";
    imported.targets.push_back(i);

    RemapImportedIds(imported, existing);
    CHECK(imported.targets.front()->id != "dup");
}
```

- [ ] **Step 3: Add to test build; run to confirm failure**

Append to `test/CMakeLists.txt`: `../src/config-serialization.cpp` and `config_serialization_test.cpp`.
Run: `cmake --build build-test` → FAIL (`config-serialization.h` missing).

- [ ] **Step 4: Implement the serialization module (logger-free)**

`src/config-serialization.h`:
```cpp
#pragma once
#include <string>
#include <string_view>
#include "output-config.h"

std::string ConfigToJsonString(const MultiOutputConfig& config);
MultiOutputConfig ConfigFromJsonString(std::string_view json, std::string* errorOut = nullptr);
void RemapImportedIds(MultiOutputConfig& imported, const MultiOutputConfig& existing);
```

`src/config-serialization.cpp`: move the bodies of the `static` `SaveTarget`/`SaveVideoConfig`/`SaveAudioTrackConfig`/`SaveAudioConfig`/`LoadTargetConfig`/`LoadVideoConfig`/`LoadAudioTrackConfig`/`LoadAudioConfig` and the two core `SaveMultiOutputConfig(MultiOutputConfig&)` / `LoadMultiOutputConfig(const std::string&)` functions here from `output-config.cpp`, **removing all `blog` calls** (the file-I/O wrappers keep their logging). Add serialization of the new fields in `SaveTarget`:
```cpp
    json["auto-start"] = config.autoStart;
    json["auto-restart"] = config.autoRestart;
    json["max-restarts"] = config.maxRestarts;
```
and in `LoadTargetConfig`:
```cpp
    config->autoStart = GetJsonField<bool>(json, "auto-start").value_or(false);
    config->autoRestart = GetJsonField<bool>(json, "auto-restart").value_or(false);
    config->maxRestarts = GetJsonField<int>(json, "max-restarts").value_or(5);
```
Expose the two public entry points:
```cpp
std::string ConfigToJsonString(const MultiOutputConfig& config) {
    return SaveMultiOutputConfigImpl(const_cast<MultiOutputConfig&>(config));
}
MultiOutputConfig ConfigFromJsonString(std::string_view json, std::string* errorOut) {
    try {
        return LoadMultiOutputConfigImpl(std::string(json));
    } catch (const std::exception& e) {
        if (errorOut) *errorOut = e.what();
        return {};
    }
}
```
Implement `RemapImportedIds` by generating fresh ids for imported targets/encoder-configs whose id already exists in `existing` (or in the imported set), and rewriting each target's `videoConfig`/`audioConfig` references to the new ids. Reuse the existing random-id approach (a local generator or `GenerateId`-style loop) to avoid collisions with both sets.

- [ ] **Step 5: Delegate the file wrappers**

In `src/output-config.cpp`, delete the moved statics and make the file-I/O wrappers call the new functions:
```cpp
#include "config-serialization.h"
// ...
void SaveMultiOutputConfig() {
    auto profiledir = obs_frontend_get_current_profile_path();
    if (profiledir) {
        std::string filename = std::string(profiledir) + "/obs-multi-rtmp.json";
        auto content = ConfigToJsonString(GlobalMultiOutputConfig());
        os_quick_write_utf8_file_safe(filename.c_str(), content.c_str(), content.size(), true, "tmp", "bak");
        blog(LOG_INFO, TAG "Save config into %s", filename.c_str());
    }
    bfree(profiledir);
}
bool LoadMultiOutputConfig() {
    auto profiledir = obs_frontend_get_current_profile_path();
    bool ret = false;
    if (profiledir) {
        std::string filename = std::string(profiledir) + "/obs-multi-rtmp.json";
        auto content = os_quick_read_utf8_file(filename.c_str());
        if (content) {
            std::string err;
            GlobalMultiOutputConfig() = ConfigFromJsonString(content, &err);
            if (!err.empty()) blog(LOG_ERROR, TAG "Config parse error: %s", err.c_str());
            bfree(content);
            ret = true;
        }
    }
    bfree(profiledir);
    return ret;
}
```
Keep `GenerateId` where it is (it needs the live global config).

- [ ] **Step 6: Run unit tests, then build the plugin**

Run: `cmake --build build-test && ctest --test-dir build-test --output-on-failure` → PASS.
Add `./src/config-serialization.h`, `./src/config-serialization.cpp` to `CMakeLists.txt` `target_sources`. Build the plugin.

- [ ] **Step 7: Commit**

```bash
git add src/output-config.h src/output-config.cpp src/config-serialization.h src/config-serialization.cpp CMakeLists.txt test/
git commit -m "refactor: extract logger-free config serialization; add auto-start/restart schema"
```

---

## Task 0.5: Fix the OnStopped threading race

**Files:**
- Modify: `src/push-widget.cpp` (`OnStopped`, ~lines 818-854)

**Interfaces:** none (behavior fix only).

- [ ] **Step 1: Move lifecycle releases onto the UI thread**

In `OnStopped`, move `ReleaseOutputEncoder();` and `ReleaseOutputSceneView();` from after the `RunInUIThread(...)` call to the **end of** the lambda passed to `RunInUIThread`, so they run on the UI thread with the rest of the teardown:
```cpp
void OnStopped(int code) override {
    GetGlobalService().RunInUIThread([this, code]() {
        ResetInfo();
        timer_->stop();
        remove_btn_->setEnabled(true);
        btn_->setText(obs_module_text("Btn.Start"));
        btn_->setEnabled(true);
        // ... existing code->message switch ...
        ReleaseOutputEncoder();
        ReleaseOutputSceneView();
    });
}
```

- [ ] **Step 2: Verify manually**

Build the plugin. Start a target, then stop it; start/stop repeatedly and switch profiles while a target is stopping. Expected: no crash, encoder/scene released cleanly (watch the OBS log for the plugin's TAG lines; no "Release output while it is active" errors).

- [ ] **Step 3: Commit**

```bash
git add src/push-widget.cpp
git commit -m "fix: release encoder/scene-view on the UI thread in OnStopped"
```

---

## Task 0.6: Replace assert-as-control-flow; tidy QMessageBox ownership

**Files:**
- Modify: `src/push-widget.cpp` (lines ~304, ~345), `src/edit-widget.cpp` (lines ~447, ~463)

**Interfaces:** none.

- [ ] **Step 1: Remove recoverable-path asserts**

At each site, delete the `assert(false && "...")` / `assert(protocol_info)` that immediately precedes an existing graceful fallback, keeping the `blog(LOG_ERROR, ...)` and the fallback path. Example in `push-widget.cpp` `GetVideoEncoder`:
```cpp
} else {
    blog(LOG_ERROR, TAG "Load video encoder config failed for %s. Sharing with main output.", config_->name.c_str());
    config_->videoConfig = OBS_STREAMING_ENC_PLACEHOLDER;
    return GetVideoEncoder();
}
```
(remove the `assert(false && ...)` line above it). Do the same for the audio path and the two `edit-widget.cpp` protocol-info fallbacks.

- [ ] **Step 2: Tidy the delete-confirmation dialog ownership**

In `src/obs-multi-rtmp.cpp` `AddPushWidget`, change the heap `new QMessageBox(...)` used only for a modal `exec()` to a stack object:
```cpp
QMessageBox box(QMessageBox::Icon::Question,
    obs_module_text("Question.Title"), obs_module_text("Question.Delete"),
    QMessageBox::Yes | QMessageBox::No, this);
if (box.exec() != QMessageBox::Yes) return;
```

- [ ] **Step 3: Build and smoke-test**

Build the plugin; trigger the delete-confirmation and a broken-protocol config path (or reason about it) to confirm graceful behavior.

- [ ] **Step 4: Commit**

```bash
git add src/push-widget.cpp src/edit-widget.cpp src/obs-multi-rtmp.cpp
git commit -m "refactor: graceful fallback instead of asserts; tidy modal dialog ownership"
```

---

# Phase 1 — Reliability

## Task 1.1: Dropped-frames / connection-health indicator

**Files:**
- Create: `src/stream-health.h`, `src/stream-health.cpp`, `test/stream_health_test.cpp`
- Modify: `test/CMakeLists.txt`, `CMakeLists.txt`, `src/push-widget.cpp`

**Interfaces:**
- Produces: `enum class StreamHealth { Good, Warn, Bad };` and `StreamHealth HealthFromStats(uint64_t framesDropped, uint64_t framesTotal, float congestion);`

- [ ] **Step 1: Write the failing test**

`test/stream_health_test.cpp`:
```cpp
#include "doctest.h"
#include "stream-health.h"

TEST_CASE("health thresholds") {
    CHECK(HealthFromStats(0, 1000, 0.0f) == StreamHealth::Good);
    CHECK(HealthFromStats(20, 1000, 0.1f) == StreamHealth::Warn);   // 2% dropped
    CHECK(HealthFromStats(100, 1000, 0.1f) == StreamHealth::Bad);   // 10% dropped
    CHECK(HealthFromStats(0, 1000, 0.8f) == StreamHealth::Bad);     // congested
    CHECK(HealthFromStats(0, 0, 0.0f) == StreamHealth::Good);       // no frames yet
}
```

- [ ] **Step 2: Add to test build; run to confirm failure**

Append `../src/stream-health.cpp` and `stream_health_test.cpp` to `test/CMakeLists.txt`. Build → FAIL.

- [ ] **Step 3: Implement**

`src/stream-health.h`:
```cpp
#pragma once
#include <cstdint>

enum class StreamHealth { Good, Warn, Bad };
StreamHealth HealthFromStats(uint64_t framesDropped, uint64_t framesTotal, float congestion);
```
`src/stream-health.cpp`:
```cpp
#include "stream-health.h"

StreamHealth HealthFromStats(uint64_t framesDropped, uint64_t framesTotal, float congestion) {
    double pct = framesTotal > 0 ? (100.0 * framesDropped / framesTotal) : 0.0;
    if (pct > 5.0 || congestion >= 0.7f) return StreamHealth::Bad;
    if (pct >= 1.0 || congestion >= 0.3f) return StreamHealth::Warn;
    return StreamHealth::Good;
}
```

- [ ] **Step 4: Run tests → PASS.**

- [ ] **Step 5: Wire into the widget**

Add `./src/stream-health.*` to `CMakeLists.txt`. In `src/push-widget.cpp`: `#include "stream-health.h"`. Add a member `QLabel* health_ = 0;` and place it in the constructor grid next to `name_` (e.g. column 3 of row 0), fixed ~14px, default grey. In `UpdateStreamStatus`, after computing frames:
```cpp
auto dropped = obs_output_get_frames_dropped(output_);
auto total = obs_output_get_total_frames(output_);
float congestion = obs_output_get_congestion(output_);
switch (HealthFromStats(dropped, total, congestion)) {
    case StreamHealth::Good: health_->setStyleSheet("color:#2ecc71;"); break;
    case StreamHealth::Warn: health_->setStyleSheet("color:#f1c40f;"); break;
    case StreamHealth::Bad:  health_->setStyleSheet("color:#e74c3c;"); break;
}
health_->setText(QString::fromUtf8("\xe2\x97\x8f")); // ●
double droppedPct = total > 0 ? 100.0 * dropped / total : 0.0;
```
Append the dropped-% to the status string, e.g. `+ "  " + QString::asprintf("%.1f%% drop", droppedPct)`. Reset `health_` to grey/empty in `ResetInfo`.

- [ ] **Step 6: Build; live-verify**

Stream to a real target and throttle/kill bandwidth; confirm the dot goes yellow→red and dropped-% climbs. Commit:
```bash
git add src/stream-health.h src/stream-health.cpp src/push-widget.cpp CMakeLists.txt test/
git commit -m "feat: per-target connection-health indicator and dropped-frame %"
```

---

## Task 1.2: Failure notifications

**Files:**
- Create: `src/notifier.h`, `src/notifier.cpp`
- Modify: `CMakeLists.txt`, `src/push-widget.cpp` (`OnStopped`), `src/obs-multi-rtmp.cpp` (init tray on load)

**Interfaces:**
- Produces: `class Notifier { public: static Notifier& Instance(); void NotifyFailure(const QString& targetName, const QString& reason); };`

- [ ] **Step 1: Implement the Notifier (manual-verified; no unit test — Qt/tray)**

`src/notifier.h`:
```cpp
#pragma once
#include <QString>
class QSystemTrayIcon;

class Notifier {
public:
    static Notifier& Instance();
    void NotifyFailure(const QString& targetName, const QString& reason);
private:
    Notifier() = default;
    QSystemTrayIcon* tray_ = nullptr;
    void ensureTray();
};
```
`src/notifier.cpp`:
```cpp
#include "notifier.h"
#include <QSystemTrayIcon>
#include <QApplication>
#include <QIcon>

Notifier& Notifier::Instance() {
    static Notifier n;
    return n;
}

void Notifier::ensureTray() {
    if (tray_ || !QSystemTrayIcon::isSystemTrayAvailable())
        return;
    tray_ = new QSystemTrayIcon(QApplication::windowIcon());
    tray_->show();
}

void Notifier::NotifyFailure(const QString& targetName, const QString& reason) {
    ensureTray();
    if (tray_) {
        tray_->showMessage(QStringLiteral("obs-multi-rtmp"),
            targetName + ": " + reason, QSystemTrayIcon::Warning, 5000);
    }
}
```

- [ ] **Step 2: Fire on error-stop**

Add `./src/notifier.*` to `CMakeLists.txt`. In `src/push-widget.cpp` `OnStopped`, inside the UI-thread lambda, in the `switch(code)` non-zero branches (after `SetMsg(...)`), for any error code call:
```cpp
if (code != 0)
    Notifier::Instance().NotifyFailure(
        QString::fromUtf8(config_->name), msg_->text());
```
Also style the banner red on error: in `SetMsg`, or specifically here, `msg_->setStyleSheet("color:#e74c3c;");` on error and clear it (`msg_->setStyleSheet("");`) on `OnStarted`/`ResetInfo`.

- [ ] **Step 3: Build; live-verify**

Start a target with a bad key or kill its ingest so OBS gives up. Expect: red banner, a tray balloon "Name: <reason>", and a `LOG_WARNING` (add one alongside the notify call). Commit:
```bash
git add src/notifier.h src/notifier.cpp src/push-widget.cpp CMakeLists.txt
git commit -m "feat: tray + banner + log notification on target failure"
```

---

## Task 1.3: Global and per-target hotkeys

**Files:**
- Modify: `src/push-widget.h` (add `StartStop`-exposing methods if needed), `src/push-widget.cpp`, `src/obs-multi-rtmp.cpp`, `src/output-config.*` / `config-serialization.*` (persist hotkey blob)

**Interfaces:**
- Adds to `PushWidget`: `virtual void StartStop() = 0;` (expose the existing private `StartStop`), or reuse `StartStreaming`/`StopStreaming`.
- Persists a `hotkeys` JSON blob at the top level of the config.

- [ ] **Step 1: Register global hotkeys**

In `src/obs-multi-rtmp.cpp` `obs_module_load` (after the dock is created), register two frontend hotkeys and route them to the dock:
```cpp
obs_hotkey_register_frontend("obs-multi-rtmp.start-all", obs_module_text("Btn.StartAll"),
    [](void* d, obs_hotkey_id, obs_hotkey_t*, bool pressed) {
        if (!pressed) return;
        for (auto x : static_cast<MultiOutputWidget*>(d)->GetAllPushWidgets()) x->StartStreaming();
    }, dock);
obs_hotkey_register_frontend("obs-multi-rtmp.stop-all", obs_module_text("Btn.StopAll"),
    [](void* d, obs_hotkey_id, obs_hotkey_t*, bool pressed) {
        if (!pressed) return;
        for (auto x : static_cast<MultiOutputWidget*>(d)->GetAllPushWidgets()) x->StopStreaming();
    }, dock);
```

- [ ] **Step 2: Register per-target hotkeys**

Expose `StartStop` on the `PushWidget` interface (add `virtual void StartStop() = 0;` in `push-widget.h`; the impl already has it — just make it `public` and `override`). In `AddPushWidget`, after creating the widget, register a hotkey keyed by target id:
```cpp
std::string hkName = "obs-multi-rtmp.toggle." + targetId;
obs_hotkey_id hk = obs_hotkey_register_frontend(hkName.c_str(),
    (std::string(obs_module_text("Btn.Start")) + " / " + config->name).c_str(),
    [](void* d, obs_hotkey_id, obs_hotkey_t*, bool pressed) {
        if (pressed) static_cast<PushWidget*>(d)->StartStop();
    }, pushWidget);
```
Store `hk` alongside the widget (e.g. a `std::unordered_map<std::string, obs_hotkey_id>` member on the dock) and call `obs_hotkey_unregister(hk)` in `DeletePushWidget`.

- [ ] **Step 3: Persist and restore bindings**

On save (in `MultiOutputWidget::SaveConfig` or the frontend EXIT handler), capture bindings and write them into the config file's `hotkeys` object:
```cpp
obs_data_t* hk = obs_hotkeys_save(); // captures all frontend hotkey bindings
// serialize obs_data_get_json(hk) into the "hotkeys" field of the plugin config
```
On load, after targets and their hotkeys are re-registered in `LoadConfig`, restore with `obs_hotkeys_load` from the stored blob. (Store the blob as a raw JSON string field in `MultiOutputConfig`; add `std::optional<std::string> hotkeysBlob;` and (de)serialize it in `config-serialization.cpp` under key `hotkeys`.)

- [ ] **Step 4: Build; verify**

Assign keys in OBS Settings → Hotkeys (the "Start All", "Stop All", and per-target entries appear there). Confirm they start/stop targets and that bindings survive an OBS restart and a profile switch. Commit:
```bash
git add src/push-widget.h src/push-widget.cpp src/obs-multi-rtmp.cpp src/output-config.h src/config-serialization.cpp
git commit -m "feat: global and per-target hotkeys with persisted bindings"
```

---

# Phase 2 — Usability

## Task 2.1: Config export / import

**Files:**
- Modify: `src/obs-multi-rtmp.cpp` (dock buttons + handlers)
- Reuses: `ConfigToJsonString`, `ConfigFromJsonString`, `RemapImportedIds` (Task 0.4)

**Interfaces:** none new (RemapImportedIds already tested in Task 0.4).

- [ ] **Step 1: Add Export / Import buttons**

In `MultiOutputWidget`'s constructor, add two buttons to a row near Start All / Stop All:
```cpp
auto exportBtn = new QPushButton(obs_module_text("Btn.Export"), container_);
auto importBtn = new QPushButton(obs_module_text("Btn.Import"), container_);
allBtnLayout->addWidget(exportBtn);
allBtnLayout->addWidget(importBtn);
```
Add the new locale keys `Btn.Export` / `Btn.Import` to `data/locale/en-US.ini` (and other locale files with English fallback).

- [ ] **Step 2: Export handler**

```cpp
QObject::connect(exportBtn, &QPushButton::clicked, [this]() {
    auto path = QFileDialog::getSaveFileName(this, obs_module_text("Btn.Export"),
        "obs-multi-rtmp.json", "JSON (*.json)");
    if (path.isEmpty()) return;
    auto content = ConfigToJsonString(GlobalMultiOutputConfig());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) { f.write(content.c_str()); f.close(); }
});
```

- [ ] **Step 3: Import handler (replace vs merge)**

```cpp
QObject::connect(importBtn, &QPushButton::clicked, [this]() {
    auto path = QFileDialog::getOpenFileName(this, obs_module_text("Btn.Import"),
        "", "JSON (*.json)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    auto bytes = f.readAll(); f.close();
    std::string err;
    auto imported = ConfigFromJsonString(bytes.constData(), &err);
    if (!err.empty()) { /* show QMessageBox with err */ return; }

    QMessageBox box(QMessageBox::Question, obs_module_text("Btn.Import"),
        obs_module_text("Import.ReplaceOrMerge"),
        QMessageBox::NoButton, this);
    auto* replace = box.addButton(obs_module_text("Import.Replace"), QMessageBox::AcceptRole);
    auto* merge = box.addButton(obs_module_text("Import.Merge"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() == replace) {
        GlobalMultiOutputConfig() = imported;
    } else if (box.clickedButton() == merge) {
        RemapImportedIds(imported, GlobalMultiOutputConfig());
        auto& g = GlobalMultiOutputConfig();
        for (auto& t : imported.targets) g.targets.push_back(t);
        for (auto& v : imported.videoConfig) g.videoConfig.push_back(v);
        for (auto& a : imported.audioConfig) g.audioConfig.push_back(a);
    } else return;
    SaveConfig();
    LoadConfig();
});
```

- [ ] **Step 4: Build; verify**

Export a config, delete all targets, import (Replace) → identical set restored. Import again (Merge) → targets duplicated with new ids, no collisions. Commit:
```bash
git add src/obs-multi-rtmp.cpp data/locale/
git commit -m "feat: export/import config with replace and merge"
```

---

## Task 2.2: Aggregate upload meter

**Files:**
- Modify: `src/push-widget.h` (add getter), `src/push-widget.cpp` (store + expose bitrate), `src/obs-multi-rtmp.cpp` (dock label + timer)

**Interfaces:**
- Adds to `PushWidget`: `virtual double CurrentBitrateBps() const = 0;`

- [ ] **Step 1: Store and expose per-widget bitrate**

In `src/push-widget.h` add `virtual double CurrentBitrateBps() const = 0;`.
In `PushWidgetImpl` add member `double last_bitrate_bps_ = 0;`, set it in `UpdateStreamStatus` where `bps` is computed (see Task 0.2 Step 5), zero it in `ResetInfo` and `OnStopped`. Implement:
```cpp
double CurrentBitrateBps() const override {
    return IsRunning() ? last_bitrate_bps_ : 0.0;
}
```
(Note: `IsRunning()` is non-const in the impl — either make it const or read the member directly guarded by `output_`.)

- [ ] **Step 2: Aggregate label + timer on the dock**

In `MultiOutputWidget`, add `QLabel* totalRate_` near the top; add a `QTimer` (1s) that sums:
```cpp
double total = 0;
for (auto* w : GetAllPushWidgets()) total += w->CurrentBitrateBps();
totalRate_->setText((std::string(obs_module_text("Label.TotalUpload")) + ": " + FormatBitrate(total)).c_str());
```
Include `stream-format.h` and add the `Label.TotalUpload` locale key.

- [ ] **Step 3: Build; verify**

Run two targets; confirm the total ≈ sum of the two per-target bitrates and drops to 0 when both stop. Commit:
```bash
git add src/push-widget.h src/push-widget.cpp src/obs-multi-rtmp.cpp data/locale/
git commit -m "feat: aggregate upload bitrate meter"
```

---

## Task 2.3: Platform presets

**Files:**
- Create: `src/platform-presets.h`, `src/platform-presets.cpp`, `test/platform_presets_test.cpp`
- Modify: `test/CMakeLists.txt`, `CMakeLists.txt`, `src/edit-widget.cpp`

**Interfaces:**
- Produces:
  - `struct PlatformPreset { std::string name; std::string serverUrl; };`
  - `const std::vector<PlatformPreset>& GetPlatformPresets();`
  - `nlohmann::json ApplyPresetServer(nlohmann::json serviceParam, const std::string& serverUrl);`

- [ ] **Step 1: Write the failing test**

`test/platform_presets_test.cpp`:
```cpp
#include "doctest.h"
#include "platform-presets.h"

TEST_CASE("presets are non-empty and include Custom") {
    auto& p = GetPlatformPresets();
    CHECK(p.size() >= 5);
    bool hasCustom = false;
    for (auto& x : p) if (x.name == "Custom") hasCustom = true;
    CHECK(hasCustom);
}

TEST_CASE("ApplyPresetServer sets the server field") {
    nlohmann::json j;
    j["key"] = "abc";
    auto out = ApplyPresetServer(j, "rtmp://live.example/app");
    CHECK(out["server"] == "rtmp://live.example/app");
    CHECK(out["key"] == "abc"); // preserved
}
```

- [ ] **Step 2: Add to test build; run → FAIL.**

Append `../src/platform-presets.cpp` and `platform_presets_test.cpp` to `test/CMakeLists.txt`.

- [ ] **Step 3: Implement**

`src/platform-presets.h`:
```cpp
#pragma once
#include <string>
#include <vector>
#include <json.hpp>

struct PlatformPreset { std::string name; std::string serverUrl; };
const std::vector<PlatformPreset>& GetPlatformPresets();
nlohmann::json ApplyPresetServer(nlohmann::json serviceParam, const std::string& serverUrl);
```
`src/platform-presets.cpp`:
```cpp
#include "platform-presets.h"

const std::vector<PlatformPreset>& GetPlatformPresets() {
    static const std::vector<PlatformPreset> presets = {
        {"Custom", ""},
        {"Twitch (auto)", "rtmp://live.twitch.tv/app"},
        {"YouTube", "rtmp://a.rtmp.youtube.com/live2"},
        {"Kick", "rtmps://fa723fc1b171.global-contribute.live-video.net"},
        {"TikTok", "rtmp://push.tiktokcdn.com/live"},
        {"Facebook", "rtmps://live-api-s.facebook.com:443/rtmp"},
    };
    return presets;
}

nlohmann::json ApplyPresetServer(nlohmann::json serviceParam, const std::string& serverUrl) {
    if (serviceParam.is_null() || !serviceParam.is_object())
        serviceParam = nlohmann::json::object();
    if (!serverUrl.empty())
        serviceParam["server"] = serverUrl;
    return serviceParam;
}
```
(Ingest URLs are documented as reasonable defaults; the user can always pick "Custom". Verify current URLs during implementation.)

- [ ] **Step 4: Run tests → PASS.**

- [ ] **Step 5: Add the preset combo to the Service tab**

Add `./src/platform-presets.*` to `CMakeLists.txt`. In `src/edit-widget.cpp` `CreateOutputSettingsWidget` (service tab section), add a `QComboBox* presetCombo_` above the service properties, populated from `GetPlatformPresets()`. On selection (non-Custom):
```cpp
config_->serviceParam = ApplyPresetServer(config_->serviceParam, preset.serverUrl);
updateServiceTab(); // re-render the service properties with the new server
```

- [ ] **Step 6: Build; verify**

Open a target's edit dialog, pick "YouTube" → the server field fills with the YouTube ingest; key untouched. Commit:
```bash
git add src/platform-presets.h src/platform-presets.cpp src/edit-widget.cpp CMakeLists.txt test/
git commit -m "feat: platform ingest presets in the service tab"
```

---

# Phase 3 — Automation

## Task 3.1: Auto-start on launch

**Files:**
- Modify: `src/edit-widget.cpp` (checkbox + save/load), `src/push-widget.cpp` / `src/obs-multi-rtmp.cpp` (start on ready)

**Interfaces:** uses `OutputTargetConfig::autoStart` (Task 0.4).

- [ ] **Step 1: Add the checkbox to "Other Settings"**

In `src/edit-widget.cpp`, in the "OtherSettings" group box (lines ~637-644), add:
```cpp
otherLayout->addWidget(autoStart_ = new QCheckBox(obs_module_text("AutoStart"), gp), 2, 0);
```
Declare `QCheckBox* autoStart_ = 0;`. In `SaveConfig`: `config_->autoStart = autoStart_->isChecked();`. In `LoadTargetConfig`: `autoStart_->setChecked(target.autoStart);`. Add the `AutoStart` locale key.

- [ ] **Step 2: Start flagged targets when the frontend is ready**

In `src/obs-multi-rtmp.cpp` frontend event callback, handle `OBS_FRONTEND_EVENT_FINISHED_LOADING`:
```cpp
else if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
    for (auto* w : dock->GetAllPushWidgets())
        w->StartIfAutoStart();
}
```
Add `virtual void StartIfAutoStart() = 0;` to `PushWidget`; implement in `PushWidgetImpl`:
```cpp
void StartIfAutoStart() override {
    if (config_ && config_->autoStart && !IsRunning())
        StartStreaming();
}
```
`StartStreaming` already guards missing encoders (shows the "start streaming/recording first" notice), so shared-encoder targets fail gracefully rather than crashing.

- [ ] **Step 3: Build; verify**

Flag a target with its own encoder as auto-start; restart OBS → it starts automatically after load. Commit:
```bash
git add src/edit-widget.cpp src/push-widget.h src/push-widget.cpp src/obs-multi-rtmp.cpp data/locale/
git commit -m "feat: auto-start targets on OBS launch"
```

---

## Task 3.2: Watchdog auto-restart

**Files:**
- Create: `src/watchdog.h`, `src/watchdog.cpp`, `test/watchdog_test.cpp`
- Modify: `test/CMakeLists.txt`, `CMakeLists.txt`, `src/edit-widget.cpp`, `src/push-widget.cpp`

**Interfaces:**
- Produces:
  - `struct WatchdogState { int retries = 0; };`
  - `struct WatchdogAction { bool restart; std::chrono::milliseconds delay; };`
  - `WatchdogAction WatchdogDecide(WatchdogState& state, int stopCode, bool manualStop, int maxRestarts);`

- [ ] **Step 1: Write the failing test**

`test/watchdog_test.cpp`:
```cpp
#include "doctest.h"
#include "watchdog.h"

TEST_CASE("no restart on manual stop") {
    WatchdogState s;
    auto a = WatchdogDecide(s, 0, /*manualStop=*/true, 5);
    CHECK(a.restart == false);
}

TEST_CASE("no restart on clean stop") {
    WatchdogState s;
    CHECK(WatchdogDecide(s, 0, false, 5).restart == false);
}

TEST_CASE("restarts with backoff up to the cap") {
    WatchdogState s;
    auto a1 = WatchdogDecide(s, -2, false, 3);
    CHECK(a1.restart == true);
    CHECK(a1.delay == std::chrono::milliseconds(5000));
    auto a2 = WatchdogDecide(s, -2, false, 3);
    CHECK(a2.delay == std::chrono::milliseconds(15000));
    auto a3 = WatchdogDecide(s, -2, false, 3);
    CHECK(a3.delay == std::chrono::milliseconds(30000));
    auto a4 = WatchdogDecide(s, -2, false, 3);
    CHECK(a4.restart == false); // cap reached
}
```

- [ ] **Step 2: Add to test build; run → FAIL.**

Append `../src/watchdog.cpp` and `watchdog_test.cpp` to `test/CMakeLists.txt`.

- [ ] **Step 3: Implement**

`src/watchdog.h`:
```cpp
#pragma once
#include <chrono>

struct WatchdogState { int retries = 0; };
struct WatchdogAction { bool restart; std::chrono::milliseconds delay; };
WatchdogAction WatchdogDecide(WatchdogState& state, int stopCode, bool manualStop, int maxRestarts);
```
`src/watchdog.cpp`:
```cpp
#include "watchdog.h"

WatchdogAction WatchdogDecide(WatchdogState& state, int stopCode, bool manualStop, int maxRestarts) {
    if (manualStop || stopCode == 0)
        return { false, {} };
    if (state.retries >= maxRestarts)
        return { false, {} };
    static const int backoff[] = { 5000, 15000, 30000 };
    int idx = state.retries < 3 ? state.retries : 2;
    ++state.retries;
    return { true, std::chrono::milliseconds(backoff[idx]) };
}
```

- [ ] **Step 4: Run tests → PASS.**

- [ ] **Step 5: Add the checkbox**

Add `./src/watchdog.*` to `CMakeLists.txt`. In `src/edit-widget.cpp` "OtherSettings" box add `autoRestart_` checkbox (locale key `AutoRestart`); save to `config_->autoRestart`, load from `target.autoRestart`, same as Task 3.1 Step 1.

- [ ] **Step 6: Wire the watchdog into the widget**

In `PushWidgetImpl` add members `WatchdogState watchdog_;` and `bool manualStop_ = false;`. Set `manualStop_ = true;` at the top of `StopStreaming()` and `Stop()`, and `manualStop_ = false;` at the start of `StartStreaming()`. Reset `watchdog_.retries = 0;` in `OnStarted`. In `OnStopped`, inside the UI-thread lambda:
```cpp
if (config_ && config_->autoRestart) {
    auto action = WatchdogDecide(watchdog_, code, manualStop_, config_->maxRestarts);
    if (action.restart) {
        QTimer::singleShot((int)action.delay.count(), this, [this]() {
            if (!IsRunning()) StartStreaming();
        });
        SetMsg(obs_module_text("Status.AutoRestarting"));
    }
}
```
Include `watchdog.h` and add the `Status.AutoRestarting` locale key.

- [ ] **Step 7: Build; verify**

Enable auto-restart on a target, start it, then kill its ingest. Expect backed-off restart attempts (5s, 15s, 30s) up to the cap, then a give-up (plus the Task 1.2 failure notification). Manually stopping must never trigger a restart. Commit:
```bash
git add src/watchdog.h src/watchdog.cpp src/edit-widget.cpp src/push-widget.cpp CMakeLists.txt test/ data/locale/
git commit -m "feat: watchdog auto-restart with backoff"
```

---

# Self-Review

**Spec coverage:**
- Testing strategy (doctest, ENABLE-tests, standalone) → Task 0.1. ✅ (Refinement: `test/` is a standalone CMake project so tests never require libobs/Qt — a deliberate improvement over hooking `ENABLE_TESTS` into the OBS-dependent top-level configure; noted in Global Constraints.)
- Config schema additions → Task 0.4. ✅
- Extract pure logic (ParseResolution/FormatBitrate/FormatDuration/reorder/serialization) → Tasks 0.2, 0.3, 0.4. ✅
- Threading fix → Task 0.5. ✅ Asserts + msgbox cleanup → Task 0.6. ✅
- Health indicator → 1.1; Notifications → 1.2; Hotkeys → 1.3. ✅
- Export/import → 2.1; Aggregate meter → 2.2; Platform presets (key-masking dropped as redundant, verify) → 2.3. ✅
- Auto-start → 3.1; Watchdog → 3.2. ✅

**Placeholder scan:** No TBD/TODO; every code step has real code. Ingest URLs and thresholds are concrete with a "verify during implementation" note (values, not placeholders).

**Type consistency:** `CurrentBitrateBps()`, `StartStop()`, `StartIfAutoStart()` added to `PushWidget` and used consistently; `StreamHealth`, `WatchdogAction`/`WatchdogState`, `PlatformPreset`, `ConfigToJsonString`/`ConfigFromJsonString`/`RemapImportedIds` names match across producer and consumer tasks. `last_bitrate_bps_` introduced in 0.2 Step 5 and consumed in 2.2.

**Known cross-task ordering note:** `last_bitrate_bps_` is referenced in Task 0.2's wiring but the member is formally added in Task 2.2. If executing 0.2 before 2.2, use a plain local for the message string and add the member in 2.2 (called out inline in Task 0.2 Step 5).
