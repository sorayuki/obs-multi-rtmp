#!/usr/bin/env python3
"""
OBS Multi-RTMP WebSocket Vendor Protocol - Full Diagnostic Test Suite
File: _diagnose_multi_rtmp.py
Target Plugin: obs-multi-rtmp (vendor: sorayuki.multi_rtmp)
Author: David Cool
"""

import sys
import time
import argparse
from typing import Dict, Any, List, Optional

try:
    import obsws_python as obs
except ImportError:
    print("❌ Error: 'obsws-python' is not installed.")
    print("Please install it with: pip3 install obsws-python")
    sys.exit(1)

GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
CYAN = "\033[96m"
BOLD = "\033[1m"
RESET = "\033[0m"


class MultiRTMPDiagnostics:
    def __init__(self, host: str, port: int, password: str):
        self.host = host
        self.port = port
        self.password = password
        self.client: Optional[obs.ReqClient] = None
        self.vendor = "sorayuki.multi_rtmp"
        self.results: List[Dict[str, Any]] = []
        self.temp_target_ids: List[str] = []
        self.discovered_targets: List[Dict[str, Any]] = []

    def record(self, name: str, success: bool, details: str = "", data: Any = None):
        status_symbol = f"{GREEN}PASS{RESET}" if success else f"{RED}FAIL{RESET}"
        print(f" [{status_symbol}] {BOLD}{name}{RESET}")
        if details:
            print(f"        {details}")
        self.results.append({
            "name": name,
            "success": success,
            "details": details,
            "data": data
        })

    def call_vendor(self, request_type: str, request_data: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        if not self.client:
            raise RuntimeError("Client not connected.")
        response = self.client.call_vendor_request(
            vendor_name=self.vendor,
            request_type=request_type,
            request_data=request_data or {}
        )
        return response.response_data or {}

    def fetch_all_targets(self) -> List[Dict[str, Any]]:
        res = self.call_vendor("list_targets")
        return res.get("targets", [])

    def find_target_by_name(self, name: str) -> Optional[Dict[str, Any]]:
        targets = self.fetch_all_targets()
        for t in targets:
            if t.get("name") == name:
                return t
        return None

    def run_all(self):
        print(f"\n{BOLD}{CYAN}========================================================")
        print("  OBS Multi-RTMP Plugin WebSocket Diagnostic Suite")
        print(f"========================================================{RESET}\n")

        # 1. Connect
        try:
            print(f"Connecting to OBS WebSocket server at {self.host}:{self.port}...")
            self.client = obs.ReqClient(host=self.host, port=self.port, password=self.password)
            self.record("Connect to OBS WebSocket Server", True, f"Connected to {self.host}:{self.port}")
        except Exception as e:
            self.record("Connect to OBS WebSocket Server", False, f"Failed to connect: {e}")
            self.print_summary()
            return

        # Clean up any leftover Diag-* sandbox targets from prior runs
        try:
            initial_targets = self.fetch_all_targets()
            for t in initial_targets:
                if t.get("name", "").startswith("Diag-"):
                    self.call_vendor("delete_target", {"id": str(t.get("id"))})
        except Exception:
            pass

        # 2. Test list_targets
        try:
            self.discovered_targets = self.fetch_all_targets()
            target_names = [t.get("name", "Unnamed") for t in self.discovered_targets]
            self.record(
                "Request 1: list_targets",
                True,
                f"Discovered {len(self.discovered_targets)} targets: {', '.join(target_names) if target_names else 'None (Clean slate)'}",
                self.discovered_targets
            )
        except Exception as e:
            self.record("Request 1: list_targets", False, f"Error: {e}")

        # Choose a target for read-only queries (get_target_state & get_target_stats)
        # If the user has an existing target (e.g. YouTube, Twitch, custom), use the first existing one!
        # If no target exists at all, we dynamically create a temporary dummy target for the read checks.
        test_read_target_id = None
        test_read_target_name = None
        created_dummy_for_read = False

        if self.discovered_targets:
            chosen = self.discovered_targets[0]
            test_read_target_id = str(chosen.get("id"))
            test_read_target_name = chosen.get("name", "Target")
        else:
            # Create a bootstrap dummy target so read tests still run 100% autonomously!
            bootstrap_name = f"Diag-Bootstrap-{int(time.time())}"
            try:
                self.call_vendor("add_target", {"name": bootstrap_name, "protocol": "RTMP"})
                time.sleep(0.3)
                t_obj = self.find_target_by_name(bootstrap_name)
                if t_obj:
                    test_read_target_id = str(t_obj.get("id"))
                    test_read_target_name = bootstrap_name
                    self.temp_target_ids.append(test_read_target_id)
                    created_dummy_for_read = True
            except Exception as e:
                print(f"        Warning: Could not create bootstrap dummy target: {e}")

        # 3. Test get_target_state
        if test_read_target_id:
            try:
                res = self.call_vendor("get_target_state", {"id": test_read_target_id})
                streaming = res.get("streaming", False)
                source_note = "(Dynamic Dummy)" if created_dummy_for_read else f"('{test_read_target_name}')"
                self.record(
                    "Request 2: get_target_state (by ID)",
                    True,
                    f"Queried state safely {source_note} (ID {test_read_target_id}): streaming={streaming}",
                    res
                )
            except Exception as e:
                self.record("Request 2: get_target_state (by ID)", False, f"Error: {e}")
        else:
            self.record("Request 2: get_target_state (by ID)", False, "No target available or could not create dummy target")

        # 4. Test get_target_stats
        if test_read_target_id:
            try:
                res = self.call_vendor("get_target_stats", {"id": test_read_target_id})
                self.record(
                    "Request 3: get_target_stats",
                    True,
                    f"Queried stats safely: streaming={res.get('streaming', False)}, bps={res.get('bitrate_bps', 0)}, fps={res.get('fps', 0)} FPS",
                    res
                )
            except Exception as e:
                self.record("Request 3: get_target_stats", False, f"Error: {e}")
        else:
            self.record("Request 3: get_target_stats", False, "No target available to query stats")

        # 5. Test add_target (Creates Primary Sandbox target)
        sandbox_target_id = None
        sandbox_target_name = f"Diag-Sandbox-{int(time.time())}"
        try:
            res = self.call_vendor("add_target", {
                "name": sandbox_target_name,
                "protocol": "RTMP"
            })
            time.sleep(0.3)
            target_obj = self.find_target_by_name(sandbox_target_name)
            if target_obj:
                sandbox_target_id = str(target_obj.get("id"))
                self.temp_target_ids.append(sandbox_target_id)
                self.record(
                    "Request 4: add_target (Sandbox)",
                    True,
                    f"Created sandbox target '{sandbox_target_name}' (Assigned ID: {sandbox_target_id})",
                    target_obj
                )
            else:
                self.record("Request 4: add_target (Sandbox)", False, f"Target not found in dock: {res}")
        except Exception as e:
            self.record("Request 4: add_target (Sandbox)", False, f"Error: {e}")

        # 6. Test update_target_name
        renamed_sandbox_name = f"{sandbox_target_name}-Renamed"
        if sandbox_target_id:
            try:
                res = self.call_vendor("update_target_name", {
                    "id": sandbox_target_id,
                    "newName": renamed_sandbox_name
                })
                ok = res.get("status") == "name_updated" or res.get("success", False)
                self.record(
                    "Request 5: update_target_name",
                    ok,
                    f"Renamed sandbox target to '{renamed_sandbox_name}' (response: {res})",
                    res
                )
            except Exception as e:
                self.record("Request 5: update_target_name", False, f"Error: {e}")

        # 7. Test update_stream_key
        if sandbox_target_id:
            try:
                res = self.call_vendor("update_stream_key", {
                    "id": sandbox_target_id,
                    "streamKey": "test_stream_key_abc123"
                })
                ok = res.get("status") in ("stream_key_updated", "key_updated") or res.get("success", False)
                self.record(
                    "Request 6: update_stream_key",
                    ok,
                    f"Updated stream key on sandbox target (response: {res})",
                    res
                )
            except Exception as e:
                self.record("Request 6: update_stream_key", False, f"Error: {e}")

        # 8. Test clone_target
        # Select source: prefer an existing pre-configured target, or fall back to the sandbox target
        clone_source_id = None
        if self.discovered_targets:
            clone_source_id = str(self.discovered_targets[0].get("id"))
        else:
            clone_source_id = sandbox_target_id

        cloned_target_id = None
        cloned_target_name = f"Diag-Clone-{int(time.time())}"
        if clone_source_id:
            try:
                res = self.call_vendor("clone_target", {
                    "sourceId": clone_source_id,
                    "newName": cloned_target_name,
                    "newStreamKey": "cloned_key_xyz789"
                })
                time.sleep(0.3)
                cloned_obj = self.find_target_by_name(cloned_target_name)
                if cloned_obj:
                    cloned_target_id = str(cloned_obj.get("id"))
                    self.temp_target_ids.append(cloned_target_id)
                    self.record(
                        "Request 7: clone_target",
                        True,
                        f"Cloned target into '{cloned_target_name}' (Assigned ID: {cloned_target_id})",
                        cloned_obj
                    )
                else:
                    self.record("Request 7: clone_target", False, f"Clone response: {res}")
            except Exception as e:
                self.record("Request 7: clone_target", False, f"Error: {e}")

        # 9. Test update_service_param
        # Run against the cloned sandbox target or standard sandbox target
        target_for_param = cloned_target_id or sandbox_target_id
        if target_for_param:
            try:
                res = self.call_vendor("update_service_param", {
                    "id": target_for_param,
                    "key": "server",
                    "value": "rtmp://127.0.0.1:1935/live"
                })
                ok = res.get("status") in ("service_param_updated", "param_updated") or res.get("success", False)
                self.record(
                    "Request 8: update_service_param",
                    ok,
                    f"Updated service parameter on sandbox target (response: {res})",
                    res
                )
            except Exception as e:
                self.record("Request 8: update_service_param", False, f"Error: {e}")

        # 10. Test update_sync_start & update_sync_stop
        if sandbox_target_id:
            try:
                res1 = self.call_vendor("update_sync_start", {
                    "id": sandbox_target_id,
                    "syncStart": True
                })
                res2 = self.call_vendor("update_sync_stop", {
                    "id": sandbox_target_id,
                    "syncStop": True
                })
                ok1 = res1.get("status") == "sync_start_updated" or res1.get("success", False)
                ok2 = res2.get("status") == "sync_stop_updated" or res2.get("success", False)
                self.record(
                    "Request 9: update_sync_start & update_sync_stop",
                    ok1 and ok2,
                    f"Toggled sync_start ({res1.get('status')}) & sync_stop ({res2.get('status')})",
                    {"start": res1, "stop": res2}
                )
            except Exception as e:
                self.record("Request 9: update_sync_start & update_sync_stop", False, f"Error: {e}")

        # 11. Test toggle_target & stop_target (Sandbox)
        target_for_toggle = cloned_target_id or sandbox_target_id
        if target_for_toggle:
            try:
                res_toggle = self.call_vendor("toggle_target", {"id": target_for_toggle})
                time.sleep(0.5)
                res_stop = self.call_vendor("stop_target", {"id": target_for_toggle})
                self.record(
                    "Request 10: toggle_target & stop_target (Sandbox)",
                    True,
                    f"Tested toggle & stop controls (toggle: {res_toggle.get('status') or res_toggle.get('success')}, stop: {res_stop.get('status') or res_stop.get('success')})",
                    {"toggle": res_toggle, "stop": res_stop}
                )
            except Exception as e:
                self.record("Request 10: toggle_target & stop_target (Sandbox)", False, f"Error: {e}")

        # 12. Test stop_all
        try:
            res = self.call_vendor("stop_all")
            ok = res.get("status") in ("all_stopped", "stop_all_requested") or res.get("success", False) or "count" in res
            self.record(
                "Request 11: stop_all",
                ok,
                f"Issued stop_all vendor request successfully (response: {res})",
                res
            )
        except Exception as e:
            self.record("Request 11: stop_all", False, f"Error: {e}")

        # 13. Test delete_target & Clean up all sandbox targets
        cleanup_success = True
        for tid in list(self.temp_target_ids):
            try:
                res = self.call_vendor("delete_target", {"id": tid})
                ok = res.get("status") == "target_deleted" or res.get("success", False)
                if ok:
                    self.temp_target_ids.remove(tid)
                else:
                    cleanup_success = False
            except Exception as e:
                cleanup_success = False
                print(f"        Cleanup error for ID {tid}: {e}")

        self.record(
            "Request 12: delete_target & Sandbox Cleanup",
            cleanup_success and len(self.temp_target_ids) == 0,
            f"Successfully deleted all temporary sandbox targets ({len(self.temp_target_ids)} remaining in dock)"
        )

        self.print_summary()

    def print_summary(self):
        print(f"\n{BOLD}{CYAN}========================================================")
        print("                  DIAGNOSTIC REPORT")
        print(f"========================================================{RESET}")
        
        passes = sum(1 for r in self.results if r["success"])
        fails = sum(1 for r in self.results if not r["success"])
        total = len(self.results)
        
        print(f"\nTotal Tests Run: {total}")
        print(f"  Passed: {GREEN}{BOLD}{passes}{RESET}")
        print(f"  Failed: {RED}{BOLD}{fails}{RESET}")

        if fails == 0 and total > 0:
            print(f"\n{BOLD}{GREEN}🎉 ALL VENDOR PROTOCOL ENDPOINTS PASSED VERIFICATION!{RESET}")
            print(f"{GREEN}The obs-multi-rtmp plugin with obs-websocket support is operating cleanly and ready for public release.{RESET}\n")
        else:
            print(f"\n{BOLD}{YELLOW}⚠️ Some checks encountered issues. Review the log above for details.{RESET}\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="OBS Multi-RTMP WebSocket Diagnostic Suite")
    parser.add_argument("--host", default="localhost", help="OBS host (default: localhost)")
    parser.add_argument("--port", type=int, default=4455, help="OBS WebSocket port (default: 4455)")
    parser.add_argument("--password", default="", help="OBS WebSocket password")
    
    args = parser.parse_args()
    
    diag = MultiRTMPDiagnostics(host=args.host, port=args.port, password=args.password)
    diag.run_all()
