# OBS Multi-RTMP with obs-websocket Support

[![OBS Studio](https://img.shields.io/badge/OBS%20Studio-30%20%2F%2031%2B-blue?logo=obsstudio&logoColor=white)](https://obsproject.com)
[![Qt](https://img.shields.io/badge/Qt-6.6%2B-41cd52?logo=qt&logoColor=white)](https://www.qt.io)
[![Protocol](https://img.shields.io/badge/Protocol-obs--websocket%20v5-purple)](https://github.com/obsproject/obs-websocket)
[![Upstream Version](https://img.shields.io/badge/Upstream-v0.7.4.3-emerald)](https://github.com/sorayuki/obs-multi-rtmp)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE)

An enhanced release of [Sora Yuki's obs-multi-rtmp plugin](https://github.com/sorayuki/obs-multi-rtmp) featuring a complete, high-performance **obs-websocket v5 Vendor Request API** (`vendorName: "sorayuki.multi_rtmp"`). 

Stream simultaneously to YouTube, Twitch, Kick, Facebook, TikTok, or custom RTMP, RTMPS, and SRT destinations while monitoring health, reading real-time telemetry, and controlling targets remotely via Elgato Stream Deck, Python automation, Node.js scripts, Bitfocus Companion, or custom web dashboards.

---

## Table of Contents
1. [Overview & Key Features](#overview--key-features)
2. [Installation & Setup](#installation--setup)
3. [WebSocket API Reference](#websocket-api-reference)
4. [Automated Diagnostic Suite](#automated-diagnostic-suite)
5. [Code Examples](#code-examples)
6. [Architecture & Source Code Guide](#architecture--source-code-guide)
7. [Building & Packaging](#building--packaging)
8. [Credits & License](#credits--license)

---

## Overview & Key Features

- **Simultaneous Multi-Platform Streaming**: Output to unlimited RTMP/RTMPS/SRT endpoints. Share OBS's active video/audio encoders or transcode on separate encoders per destination.
- **Full obs-websocket v5 Vendor Integration**: 16 dedicated vendor request endpoints under `vendorName: "sorayuki.multi_rtmp"` for comprehensive automation.
- **Remote Stream Control**: Start, stop, or toggle targets individually, or trigger coordinated `start_all` and `stop_all` actions.
- **Dynamic Live Configuration**: Add new RTMP/SRT targets, clone existing configurations, rename targets, and rotate stream keys or server URLs on-the-fly without opening modal dialog boxes.
- **Live Stream Telemetry**: Query live duration, numerical bitrates in bits per second (`bitrate_bps`), and real-time frames per second (`fps`) for any active destination.
- **Thread-Safe Architecture**: Asynchronous request handling and UI thread marshalling protect OBS Studio and the Qt GUI against cross-thread race conditions and re-entrant deletion hazards.
- **Cross-Platform**: Tested and verified on **macOS** (Universal binary for Apple Silicon M1–M4 & Intel x86_64) and **Windows 10/11** (64-bit), with build support for **Linux** (Debian/Ubuntu & Flatpak).

---

## Installation & Setup

Download the latest pre-compiled release from the [**GitHub Releases**](../../releases) page:

### Windows (10 / 11 64-bit)

#### Option A: Automated Installer (Recommended)
Download and run `obs-multi-rtmp-*-windows-x64-Installer.exe` and follow the setup wizard.

#### Option B: Manual Zip Installation
1. Download `obs-multi-rtmp-*-windows-x64.zip`.
2. Extract the archive directly into your OBS Studio root directory (typically `C:\Program Files\obs-studio\`):
   ```text
   C:\Program Files\obs-studio\
   ├── obs-plugins\64bit\obs-multi-rtmp.dll
   └── data\obs-plugins\obs-multi-rtmp\locale\en-US.ini  <-- (Crucial for button/tab text)
   ```
   *Note: If installed into `%PROGRAMDATA%\obs-studio\plugins\obs-multi-rtmp\`, ensure the `data\locale\` folder is placed alongside `bin\64bit\` so button and tab labels render correctly without raw text keys like `Btn.Start`.*

### macOS (Apple Silicon & Intel Universal)

1. Download `obs-multi-rtmp-*-macos-universal.pkg`.
2. Double-click the `.pkg` package and follow the standard macOS installer instructions.
3. The plugin bundle is installed to:
   ```text
   ~/Library/Application Support/obs-studio/plugins/obs-multi-rtmp.plugin
   ```

### Enabling WebSocket in OBS Studio

1. In OBS Studio, open the top menu: **Tools** > **WebSocket Server Settings**.
2. Check **Enable WebSocket server** (Default port is `4455`).
3. Set or copy your server password.

---

## WebSocket API Reference

All requests use OBS WebSocket v5 `CallVendorRequest` with:
```json
"vendorName": "sorayuki.multi_rtmp"
```

### 1. Streaming Control Endpoints

| Request Type | Description | Request Data Parameters | Response Data |
| :--- | :--- | :--- | :--- |
| `start_target` | Start streaming for a specific target | `id` (string) or `name` (string) | `{"status": "start_requested", "id": "..."}` |
| `stop_target` | Stop streaming for a specific target | `id` (string) or `name` (string) | `{"status": "stop_requested", "id": "..."}` |
| `toggle_target` | Toggle streaming on/off for a target | `id` (string) or `name` (string) | `{"status": "start_requested"\|"stop_requested", "id": "..."}` |
| `start_all` | Start streaming to all configured targets | None | `{"status": "start_all_requested", "count": 3}` |
| `stop_all` | Stop streaming on all currently active targets | None | `{"status": "stop_all_requested", "count": 3}` |

### 2. Monitoring & Telemetry Endpoints

| Request Type | Description | Request Data Parameters | Response Data |
| :--- | :--- | :--- | :--- |
| `list_targets` | Get list of all targets and states | None | `{"targets": [{"id": "...", "name": "...", "protocol": "RTMP", "streaming": false, "duration": "00:00:00", "bitrate": "0 kbps", "bitrate_bps": 0, "fps": 0}]}` |
| `get_target_state` | Retrieve active status and sync flags | `id` (string) or `name` (string) | `{"id": "...", "name": "...", "streaming": false, "syncStart": false, "syncStop": false}` |
| `get_target_stats` | Retrieve parsed duration, bitrate, and FPS | `id` (string) or `name` (string) | `{"id": "...", "name": "...", "streaming": false, "duration": "00:00:00", "bitrate": "0 kbps", "bitrate_bps": 0, "fps": 0}` |

### 3. Configuration & Target Management Endpoints

| Request Type | Description | Request Data Parameters | Response Data |
| :--- | :--- | :--- | :--- |
| `add_target` | Dynamically create a new output target | `name` (string, required), `protocol` ("RTMP" \| "SRT", optional) | `{"status": "target_added", "name": "..."}` |
| `clone_target` | Duplicate an existing target with new key/name | `sourceId` (string, required), `newName` (string, optional), `newStreamKey` (string, optional) | `{"status": "target_cloned"}` |
| `update_target_name` | Rename an existing target | `id` (string, required), `newName` (string, required) | `{"status": "name_updated"}` |
| `update_stream_key` | Update stream key (or SRT `streamid`) | `id` (string, required), `streamKey` (string, required) | `{"status": "stream_key_updated"}` |
| `update_service_param` | Update service parameter (`server`, `key`, etc.) | `id` (string, required), `key` (string, required), `value` (string/bool, required) | `{"status": "service_param_updated"}` |
| `delete_target` | Delete a target (safe-guarded against open dialogs) | `id` (string, required) | `{"status": "target_deleted"}` *(or error if target is open in UI)* |
| `update_sync_start` | Toggle auto-start sync with OBS main stream | `id` (string, required), `syncStart` (bool, required) | `{"status": "sync_start_updated"}` |
| `update_sync_stop` | Toggle auto-stop sync with OBS main stream | `id` (string, required), `syncStop` (bool, required) | `{"status": "sync_stop_updated"}` |

---

## Automated Diagnostic Suite

The repository includes a standalone automated Python diagnostic tool (`python_tests/_diagnose_multi_rtmp.py`) to verify plugin health, API compatibility, and network connectivity without altering your production streaming targets.

### Features
- **Non-Destructive**: Discovers and tests existing targets in read-only mode, or creates temporary sandbox targets if starting from a clean slate.
- **13 Comprehensive Protocol Checks**: Verifies all discovery, state inspection, configuration updates, stream control, and garbage collection endpoints.
- **Automatic Sandbox Teardown**: Cleans up all test targets upon completion, leaving your dock in its original state.

### Running the Diagnostic
```bash
# 1. Install obsws-python
pip install obsws-python

# 2. Run the diagnostic suite
python python_tests/_diagnose_multi_rtmp.py --host localhost --port 4455 --password "your_password"
```

Sample output:
```text
========================================================
  OBS Multi-RTMP Plugin WebSocket Diagnostic Suite
========================================================

Connecting to OBS WebSocket server at localhost:4455...
 [PASS] Connect to OBS WebSocket Server
 [PASS] Request 1: list_targets
 [PASS] Request 2: get_target_state (by ID)
 [PASS] Request 3: get_target_stats
 [PASS] Request 4: add_target (Sandbox)
 [PASS] Request 5: update_target_name
 [PASS] Request 6: update_stream_key
 [PASS] Request 7: clone_target
 [PASS] Request 8: update_service_param
 [PASS] Request 9: update_sync_start & update_sync_stop
 [PASS] Request 10: toggle_target & stop_target (Sandbox)
 [PASS] Request 11: stop_all
 [PASS] Request 12: delete_target & Sandbox Cleanup

Total Tests Run: 13
  Passed: 13
  Failed: 0

🎉 ALL VENDOR PROTOCOL ENDPOINTS PASSED VERIFICATION!
```

---

## Code Examples

### Python (`obsws-python`)

```python
import obsws_python as obs

# Connect to OBS WebSocket
client = obs.ReqClient(host="localhost", port=4455, password="your_password")

# 1. Discover all configured streaming targets
response = client.call_vendor_request(
    vendor_name="sorayuki.multi_rtmp",
    request_type="list_targets"
)
for target in response.response_data.get("targets", []):
    print(f"Target: {target['name']} (ID: {target['id']}) - Streaming: {target['streaming']}")

# 2. Start streaming to a target by Name
client.call_vendor_request(
    vendor_name="sorayuki.multi_rtmp",
    request_type="start_target",
    request_data={"name": "YouTube Live"}
)

# 3. Dynamically update a stream key
client.call_vendor_request(
    vendor_name="sorayuki.multi_rtmp",
    request_type="update_stream_key",
    request_data={
        "id": "1930065214",
        "streamKey": "live_abcdef123456"
    }
)
```

### JavaScript / TypeScript (`obs-websocket-js`)

```javascript
import { OBSWebSocket } from 'obs-websocket-js';

const obs = new OBSWebSocket();
await obs.connect('ws://localhost:4455', 'your_password');

// List targets
const res = await obs.call('CallVendorRequest', {
  vendorName: 'sorayuki.multi_rtmp',
  requestType: 'list_targets'
});
console.log('Multi-RTMP Targets:', res.responseData.targets);

// Stop all streaming targets
await obs.call('CallVendorRequest', {
  vendorName: 'sorayuki.multi_rtmp',
  requestType: 'stop_all'
});
```

---

## Architecture & Source Code Guide

The plugin's architecture connects `libobs`, the `obs-websocket` vendor API, and the Qt GUI framework:

```text
               ┌───────────────────────────┐
               │    OBS WebSocket Client   │
               │  (Stream Deck, Python)    │
               └─────────────┬─────────────┘
                             │ CallVendorRequest (sorayuki.multi_rtmp)
                             ▼
               ┌───────────────────────────┐
               │   ws_vendor.cpp / .hpp    │  <-- Thread Marshalling
               │ (Websocket Vendor Router) │
               └─────────────┬─────────────┘
                             │ QMetaObject::invokeMethod (Qt UI Thread)
                             ▼
               ┌───────────────────────────┐
               │  multi-output-widget.cpp  │  <-- Dock Container & List View
               │ (Target Config & Control) │
               └─────────────┬─────────────┘
                             │
              ┌──────────────┴──────────────┐
              ▼                             ▼
   ┌──────────────────────┐      ┌──────────────────────┐
   │   push-widget.cpp    │      │   edit-widget.cpp    │
   │ (Output & Telemetry) │      │ (Modal Config Dialog)│
   └──────────┬───────────┘      └──────────────────────┘
              │ libobs Output API
              ▼
   ┌──────────────────────┐
   │ RTMP / SRT Stream    │
   └──────────────────────┘
```

### Key Source Files

- **`src/ws_vendor.hpp` & `src/ws_vendor.cpp`**: 
  Implements the `obs-websocket` vendor protocol router. Handles all 16 incoming vendor requests, validates payloads, parses numeric bitrates from telemetry strings, and marshals operations onto the main Qt event loop via `QMetaObject::invokeMethod`.
- **`src/multi-output-widget.hpp` & `src/multi-output-widget.cpp`**:
  Manages the primary OBS dock widget (`obs-multi-rtmp-dock`), the collection of streaming targets, JSON configuration persistence, and lifecycle methods (`AddNewTarget`, `CloneTarget`, `DeleteTarget`).
- **`src/push-widget.hpp` & `src/push-widget.cpp`**:
  Controls individual target widgets inside the dock. Handles encoder binding (main vs. independent), `libobs` output streaming threads, and active editing state tracking (`IsEditing()`).
- **`src/edit-widget.hpp` & `src/edit-widget.cpp`**:
  Implements the modal configuration dialog for video resolution, bitrate, B-frames, and server settings.
- **`src/protocols.hpp` & `src/protocols.cpp`**:
  Protocol registry abstracting differences between RTMP, RTMPS, and SRT connection parameters.

---

## Building & Packaging

### Prerequisites
- **CMake** 3.28 or newer
- **Qt 6.6+** (Widgets and Core)
- **libobs** and **obs-frontend-api** headers/libraries
- **obs-websocket** headers (`obs-websocket-api.h`)

### Native Build (macOS / Linux)
```bash
# Configure
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_QT=ON \
  -DENABLE_FRONTEND_API=ON \
  -DENABLE_WEBSOCKET=ON

# Build
cmake --build build --config Release
```

### Windows Build (Visual Studio 2022)
```powershell
cmake -B build -S . -A x64 `
  -DENABLE_QT=ON `
  -DENABLE_FRONTEND_API=ON `
  -DENABLE_WEBSOCKET=ON

cmake --build build --config Release
```

### Automated CI/CD (GitHub Actions)
The repository is configured with a GitHub Actions workflow (`.github/workflows/push.yaml`). To automatically build and package releases:
1. Create and push a version tag:
   ```bash
   git tag v0.7.4.3-ws0.2
   git push origin v0.7.4.3-ws0.2
   ```
2. GitHub Actions will compile across Windows, macOS, and Ubuntu runners and publish all artifacts with SHA-256 checksums to your GitHub Releases page.

---

## Credits & License

- **Original Plugin**: Developed by [Sora Yuki (@sorayuki)](https://github.com/sorayuki/obs-multi-rtmp).
- **obs-websocket API Vendor Integration**: Developed and maintained by [David Cool](https://github.com/davidcool/obs-multi-rtmp-websocket-support).
- **License**: Released under the [GNU General Public License v2.0 (GPL-2.0)](LICENSE).
