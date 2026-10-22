#!/usr/bin/env python3
"""Automated Hue/Home Assistant -> ESP32 OmniGlow capture test runner.

Secrets are read from environment variables, never from this file.
"""

import argparse
import json
import os
import re
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path


TESTS = [
    ("off", {"off": True}),
    ("red_10", {"rgb_color": [255, 0, 0], "brightness": 26}),
    ("red_20", {"rgb_color": [255, 0, 0], "brightness": 51}),
    ("red_50", {"rgb_color": [255, 0, 0], "brightness": 128}),
    ("red_75", {"rgb_color": [255, 0, 0], "brightness": 191}),
    ("red_100", {"rgb_color": [255, 0, 0], "brightness": 255}),
    ("green_100", {"rgb_color": [0, 255, 0], "brightness": 255}),
    ("blue_100", {"rgb_color": [0, 0, 255], "brightness": 255}),
    ("warmwhite_10", {"color_temp_kelvin": 2700, "brightness": 26}),
    ("warmwhite_50", {"color_temp_kelvin": 2700, "brightness": 128}),
    ("warmwhite_100", {"color_temp_kelvin": 2700, "brightness": 255}),
    ("coolwhite_10", {"color_temp_kelvin": 6500, "brightness": 26}),
    ("coolwhite_50", {"color_temp_kelvin": 6500, "brightness": 128}),
    ("coolwhite_100", {"color_temp_kelvin": 6500, "brightness": 255}),
    ("temperature_2700K", {"color_temp_kelvin": 2700, "brightness": 255}),
    ("temperature_4000K", {"color_temp_kelvin": 4000, "brightness": 255}),
    ("temperature_6500K", {"color_temp_kelvin": 6500, "brightness": 255}),
]


def env(name: str) -> str:
    value = os.environ.get(name)
    if not value:
        raise SystemExit(f"Ontbrekende omgevingsvariabele: {name}")
    return value.rstrip("/")


def request_json(url: str, method="GET", payload=None, headers=None):
    body = None
    request_headers = {"Accept": "application/json"}
    if headers:
        request_headers.update(headers)
    if payload is not None:
        body = json.dumps(payload).encode()
        request_headers["Content-Type"] = "application/json"
    request = urllib.request.Request(url, data=body, headers=request_headers, method=method)
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            raw = response.read().decode("utf-8", errors="replace")
            if not raw:
                return {}
            return json.loads(raw)
    except urllib.error.HTTPError as error:
        detail = error.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"HTTP {error.code} op {url}: {detail}") from error
    except urllib.error.URLError as error:
        raise RuntimeError(f"Kan {url} niet bereiken: {error.reason}") from error


def set_hue_state(ha_url, token, entity_id, state):
    endpoint = f"{ha_url}/api/services/light/turn_off" if state.get("off") else f"{ha_url}/api/services/light/turn_on"
    data = {"entity_id": entity_id, "transition": 0}
    data.update({key: value for key, value in state.items() if key != "off"})
    request_json(endpoint, "POST", data, {"Authorization": f"Bearer {token}"})


def arm_esp32(esp_url, label):
    query = urllib.parse.urlencode({"label": label})
    request = urllib.request.Request(f"{esp_url}/capture?{query}", method="GET")
    with urllib.request.urlopen(request, timeout=20) as response:
        if response.status not in (200, 303):
            raise RuntimeError(f"ESP32 capture starten gaf HTTP {response.status}")


def wait_for_capture(esp_url, label, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        status = request_json(f"{esp_url}/status")
        if status.get("saved") and label in status.get("message", ""):
            return
        time.sleep(0.25)
    raise TimeoutError(f"Geen opgeslagen capture ontvangen voor {label}")


def get_results(esp_url):
    request = urllib.request.Request(f"{esp_url}/results", method="GET")
    with urllib.request.urlopen(request, timeout=30) as response:
        return response.read().decode("utf-8", errors="replace")


def extract_capture(results_text, label):
    pattern = rf"LABEL: {re.escape(label)}\nBYTES: ([0-9A-Fa-f ]+)"
    match = re.search(pattern, results_text)
    if not match:
        return None
    return bytes.fromhex(match.group(1))


def main():
    parser = argparse.ArgumentParser(description="Automatische Hue/ESP32 OmniGlow capture-test")
    parser.add_argument("--dry-run", action="store_true", help="toon tests, stuur niets aan")
    parser.add_argument("--only", help="voer alleen dit label uit")
    parser.add_argument("--settle", type=float, default=3.0, help="wachttijd na Hue-instelling")
    parser.add_argument("--timeout", type=float, default=15.0, help="capture-timeout")
    parser.add_argument("--output", default="captures/automated", help="map voor testresultaten")
    args = parser.parse_args()

    tests = [test for test in TESTS if not args.only or test[0] == args.only]
    if not tests:
        raise SystemExit(f"Onbekend label: {args.only}")

    if args.dry_run:
        for label, state in tests:
            print(label, state)
        return

    ha_url = env("HA_URL")
    ha_token = env("HA_TOKEN")
    entity_id = env("HA_ENTITY_ID")
    esp_url = env("ESP32_URL")
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    run_dir = output_dir / run_id
    run_dir.mkdir()

    manifest = {"run_id": run_id, "entity_id": entity_id, "captures": []}
    for index, (label, state) in enumerate(tests, 1):
        print(f"[{index}/{len(tests)}] {label}: Hue instellen")
        set_hue_state(ha_url, ha_token, entity_id, state)
        time.sleep(args.settle)
        print(f"[{index}/{len(tests)}] {label}: ESP32 capture starten")
        arm_esp32(esp_url, label)
        wait_for_capture(esp_url, label, args.timeout)
        results = get_results(esp_url)
        (run_dir / "all-results.txt").write_text(results, encoding="utf-8")
        frame = extract_capture(results, label)
        if frame is None:
            raise RuntimeError(f"Frame met label {label} ontbreekt in ESP32-resultaat")
        (run_dir / f"{label}.hex").write_text(frame.hex(" ").upper() + "\n", encoding="utf-8")
        manifest["captures"].append({"label": label, "bytes": len(frame), "file": f"{label}.hex"})
        print(f"[{index}/{len(tests)}] {label}: {len(frame)} bytes opgeslagen")

    (run_dir / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"Klaar. Resultaten staan in {run_dir}")


if __name__ == "__main__":
    main()
