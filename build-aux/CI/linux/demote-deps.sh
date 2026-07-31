#!/usr/bin/env bash
# Usage: demote-deps.sh <in.deb> [Recommends|Suggests] [regex]
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <in.deb> [Recommends|Suggests] [regex]" >&2
  exit 1
fi

in_deb="$1"
dest_field="${2:-Recommends}"
match_regex="${3:-libobs}"

if [[ "$dest_field" != "Recommends" && "$dest_field" != "Suggests" ]]; then
  echo "Destination field must be Recommends or Suggests" >&2
  exit 1
fi

echo "Demoting dependencies matching '${match_regex}' to ${dest_field}"

work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

dpkg-deb -R "$in_deb" "$work_dir"
control_file="$work_dir/DEBIAN/control"

get_field() {
  local key="$1"
  awk -v key="$key" '
    BEGIN { value=""; collecting=0 }
    $0 ~ "^" key ":" {
      collecting=1
      sub("^" key ":[[:space:]]*", "", $0)
      value=$0
      next
    }
    collecting == 1 {
      if ($0 ~ "^[[:space:]]") {
        sub("^[[:space:]]+", "", $0)
        value = value " " $0
        next
      }
      collecting=0
    }
    END { print value }
  ' "$control_file"
}

split_csv() {
  tr ',' '\n' | sed -E 's/^[[:space:]]+//; s/[[:space:]]+$//' | sed '/^$/d'
}

join_csv() {
  awk 'BEGIN { first=1 } { if (!first) printf(", "); printf("%s", $0); first=0 } END { print "" }'
}

dedup() {
  awk '!seen[$0]++'
}

depends_value="$(get_field Depends)"
destination_value="$(get_field "$dest_field")"

mapfile -t depends < <(printf '%s\n' "$depends_value" | split_csv)
declare -a moved=()
declare -a kept=()
for dependency in "${depends[@]}"; do
  [[ -z "$dependency" ]] && continue
  if [[ "$dependency" =~ $match_regex ]]; then
    moved+=("$dependency")
  else
    kept+=("$dependency")
  fi
done

if [[ -n "$destination_value" ]]; then
  mapfile -t existing_destination < <(printf '%s\n' "$destination_value" | split_csv)
  moved+=("${existing_destination[@]}")
fi

mapfile -t moved < <(printf '%s\n' "${moved[@]:-}" | sed '/^$/d' | dedup)
mapfile -t kept < <(printf '%s\n' "${kept[@]:-}" | sed '/^$/d' | dedup)

new_depends="$(printf '%s\n' "${kept[@]:-}" | join_csv || true)"
new_destination="$(printf '%s\n' "${moved[@]:-}" | join_csv || true)"

awk '
  BEGIN { skip=0 }
  {
    if ($0 ~ "^(Depends|Recommends|Suggests):") { skip=1; next }
    if (skip == 1) {
      if ($0 ~ "^[[:space:]]") { next }
      skip=0
    }
    print
  }
' "$control_file" > "$control_file.clean"

{
  cat "$control_file.clean"
  if [[ -n "$new_depends" ]]; then
    echo "Depends: $new_depends"
  fi
  if [[ -n "$new_destination" ]]; then
    echo "$dest_field: $new_destination"
  fi
} > "$control_file.new"

sed -i '/^[[:space:]]*$/d' "$control_file.new"
mv "$control_file.new" "$control_file"
rm -f "$control_file.clean"

rm -f "$in_deb"
dpkg-deb -b "$work_dir" "$in_deb" >/dev/null
echo "$in_deb"
