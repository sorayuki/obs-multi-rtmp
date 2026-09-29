# obs-websocket vendor API

obs-multi-rtmp registers an [obs-websocket](https://github.com/obsproject/obs-websocket)
vendor named **`obs-multi-rtmp`**. If obs-websocket isn't installed, this is
a silent no-op — the dock still works exactly as before.

This lets external tools drive individual stream targets (start/stop,
inspect status, swap a stream key) over obs-websocket's `CallVendorRequest`,
without touching the Qt dock. Typical use case: a script that rotates a
YouTube stream key (e.g. for a 12-hour rollover) and needs to push the new
key into just the YouTube target and reconnect it, or a monitoring script
that wants live per-platform status instead of scraping OBS's log file.

All requests/responses are plain JSON objects, sent/received via
obs-websocket's `CallVendorRequest` (vendor name `obs-multi-rtmp`).

## Requests

### `list_targets`

No request fields.

```json
{
  "targets": [
    {
      "id": "1234567890",
      "name": "YouTube",
      "protocol": "RTMP",
      "state": "live",
      "sync_start": true,
      "sync_stop": true
    }
  ]
}
```

`state` is one of `"stopped"`, `"connecting"`, `"live"`, `"reconnecting"`.

### `get_target_status`

Request: `{ "id": "<target id>" }`

Response: same fields as a `list_targets` entry, plus:

```json
{
  "last_error_code": 0,
  "duration_ms": 123456,
  "bitrate_bps": 6000000,
  "fps": 60.0
}
```

`last_error_code` mirrors OBS's own output stop codes (`0` = clean,
`-1` = wrong URL, `-2` = connect failed, `-3` = handshake failed,
`-4` = server refused, other = unknown). If `id` doesn't match any target,
the response is `{ "error": "target_not_found" }`.

### `start_target` / `stop_target`

Request: `{ "id": "<target id>" }`

Response: `{ "success": true }` or `{ "success": false, "error": "target_not_found" }`.

`stop_target` force-stops immediately — it does **not** show the "drop
delayed frames?" confirmation dialog that the dock's Stop button shows when
a stream delay is configured, since there's no one to answer it.

### `start_all_targets` / `stop_all_targets`

No request fields. Response: `{ "success": true, "count": <n> }`.

### `set_target_service_settings`

Request:

```json
{
  "id": "<target id>",
  "settings": { "server": "rtmp://a.rtmp.youtube.com/live2", "key": "xxxx-xxxx-xxxx-xxxx" },
  "merge": true,
  "restart_if_active": true
}
```

- `settings` fields match whatever the target's protocol/service expects
  (for RTMP/custom, that's `server` and `key`, same fields the dock's own
  Service tab writes). Use `get_target_status`/the dock's Edit dialog to
  confirm field names for SRT/RIST or WHIP targets.
- `merge` (default `true`): only the given fields are overwritten; other
  existing settings are left alone. Set to `false` to replace the settings
  object wholesale.
- `restart_if_active` (default `true`): if the target is currently live, it
  is stopped and immediately restarted with the new settings — this is what
  makes a stream-key rollover seamless. Set to `false` to only update the
  saved config without touching a running stream.

Response: `{ "success": true, "restarted": true }` or
`{ "success": false, "error": "target_not_found" | "settings_must_be_an_object" }`.

## Events

### `target_state_changed`

Emitted whenever a target's state changes (connecting / live / reconnecting
/ stopped):

```json
{
  "id": "1234567890",
  "name": "YouTube",
  "state": "live",
  "last_error_code": 0
}
```

Subscribe to vendor events via obs-websocket's general event subscription
mechanism to build a dashboard or trigger notifications without polling.
