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

## For AI agents / unfamiliar callers

Call `list_capabilities` first — it returns every request this vendor
registers, each with a one-line description of its parameters, so you can
work from that response instead of needing this whole file loaded. A few
safety valves are built in and worth knowing up front:

- **Secrets are redacted by default.** `get_target_config` masks any
  service-setting field whose name contains `key`/`token`/`password`/`secret`
  (e.g. an RTMP stream key) unless you pass `reveal_secrets: true`. Don't
  pass that unless you actually need the raw value — it's easy to leak a
  key by echoing a tool response back into a transcript or log.
- **Dry runs.** `create_target` and `set_target_service_settings` both take
  a `dry_run: true` option that validates the request and reports what
  would happen, without changing anything. Use it before a call you're not
  fully sure about, especially on a target that might be live.
- **Targets created via the API start disabled.** `create_target` defaults
  `enabled` to `false`, so a newly created target is never accidentally
  swept into `start_all_targets` or sync-start — you must explicitly enable
  it (`set_target_enabled`) once you've confirmed its settings.
- **Deletion requires confirmation and refuses to touch a live target.**
  `delete_target` needs `confirm: true`, and fails with `"target_is_live"`
  if the target is currently streaming — call `stop_target` first.

## Requests

### `list_capabilities`

No request fields. Returns every request name + description, the events
this vendor emits, and a link to this doc:

```json
{
  "vendor": "obs-multi-rtmp",
  "docs": "https://github.com/sorayuki/obs-multi-rtmp/blob/master/WEBSOCKET_API.md",
  "events": ["target_state_changed"],
  "requests": [
    { "name": "list_targets", "description": "..." }
  ]
}
```

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
      "enabled": true,
      "sync_start": true,
      "sync_stop": true
    }
  ]
}
```

`state` is one of `"stopped"`, `"connecting"`, `"live"`, `"reconnecting"`. A
target with `"enabled": false` is skipped by `start_target`/`start_all_targets`
and by sync-start with the main OBS stream (see `set_target_enabled` below).

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

### `get_target_config`

Request: `{ "id": "<target id>", "reveal_secrets": false }`

Full saved configuration for one target — the same fields `list_targets`
returns, plus:

```json
{
  "service_settings": { "server": "rtmp://a.rtmp.youtube.com/live2", "key": "***redacted***" },
  "output_settings": {}
}
```

`service_settings` fields whose name contains `key`/`token`/`password`/
`secret` are replaced with `"***redacted***"` unless `reveal_secrets` is
`true`. `output_settings` is never redacted (it doesn't carry credentials).
If `id` doesn't match any target, the response is `{ "error": "target_not_found" }`.

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
  "restart_if_active": true,
  "dry_run": false
}
```

- `settings` fields match whatever the target's protocol/service expects
  (for RTMP/custom, that's `server` and `key`, same fields the dock's own
  Service tab writes). Use `get_target_config` to see a target's current
  fields, including for SRT/RIST or WHIP targets.
- `merge` (default `true`): only the given fields are overwritten; other
  existing settings are left alone. Set to `false` to replace the settings
  object wholesale.
- `restart_if_active` (default `true`): if the target is currently live, it
  is stopped and immediately restarted with the new settings — this is what
  makes a stream-key rollover seamless. Set to `false` to only update the
  saved config without touching a running stream.
- `dry_run` (default `false`): validates `id` and `settings` and reports
  whether a restart would happen, without changing anything.

Response: `{ "success": true, "restarted": true }`,
`{ "success": true, "dry_run": true, "would_restart": true }`, or
`{ "success": false, "error": "target_not_found" | "settings_must_be_an_object" }`.

### `set_target_enabled`

Request: `{ "id": "<target id>", "enabled": false }`

Enables or disables a target without deleting it. A disabled target is
skipped by `start_target`/`start_all_targets` and by sync-start; disabling
a currently-live target stops it immediately (force-stop, no confirmation
dialog). Mirrors the dock's own per-target checkbox.

Response: `{ "success": true }` or `{ "success": false, "error": "target_not_found" }`.

### `create_target`

Request:

```json
{
  "name": "YouTube",
  "protocol": "RTMP",
  "service_settings": { "server": "rtmp://a.rtmp.youtube.com/live2", "key": "xxxx-xxxx-xxxx-xxxx" },
  "output_settings": {},
  "sync_start": false,
  "sync_stop": false,
  "enabled": false,
  "dry_run": false
}
```

- `protocol` must be one of the dock's registered protocols (currently
  `RTMP`, `SRT_RIST`, `WHIP`); anything else returns `"invalid_protocol"`.
- `name` defaults to the same placeholder the dock's "Add new target"
  button uses if omitted.
- `enabled` **defaults to `false`** — a target created over the API is
  never automatically swept into `start_all_targets` or sync-start. Call
  `set_target_enabled` once you've confirmed the settings are correct.
- `dry_run` (default `false`): validates `protocol`/`service_settings`/
  `output_settings` and reports success without creating anything.
- Adding video/audio encoder overrides (as opposed to sharing OBS's main
  stream encoder) isn't supported over the API yet — use the dock's Edit
  dialog for that.

Response: `{ "success": true, "id": "<new target id>" }`,
`{ "success": true, "dry_run": true }`, or
`{ "success": false, "error": "invalid_protocol" | "settings_must_be_an_object" }`.

### `delete_target`

Request: `{ "id": "<target id>", "confirm": true }`

Deletes a target. Two safety checks, both must pass:

- `confirm` must be `true` — omitting it (or passing `false`) returns
  `{ "success": false, "error": "confirmation_required" }`.
- The target must not be live — a running target returns
  `{ "success": false, "error": "target_is_live" }`; call `stop_target`
  first, then retry.

Response: `{ "success": true }` or
`{ "success": false, "error": "target_not_found" | "confirmation_required" | "target_is_live" }`.

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
