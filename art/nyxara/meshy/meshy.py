"""Minimal Meshy API client for the Nyxara character (key from MESHY_API_KEY; never printed).

Usage:
  python meshy.py balance
  python meshy.py create <resource> <json-file-or-inline-json> <label>   -> prints task id, logs it
  python meshy.py wait <resource> <task_id> [out_dir]                     -> polls, saves JSON + downloads files
Resources: v1/text-to-image, v1/image-to-3d, v1/multi-image-to-3d, v2/text-to-3d, v1/remesh,
           v1/rigging, v1/animations
"""
import json, os, sys, time, urllib.request, pathlib, datetime

BASE = "https://api.meshy.ai/openapi/"
HERE = pathlib.Path(__file__).resolve().parent
LOG = HERE / "task_log.jsonl"
KEY = os.environ["MESHY_API_KEY"]


def call(method, path, body=None):
    req = urllib.request.Request(BASE + path, method=method,
                                 headers={"Authorization": "Bearer " + KEY, "Content-Type": "application/json"},
                                 data=json.dumps(body).encode() if body is not None else None)
    try:
        with urllib.request.urlopen(req, timeout=120) as r:
            return json.loads(r.read() or b"{}")
    except urllib.error.HTTPError as e:
        raise SystemExit(f"HTTP {e.code}: {e.read().decode(errors='replace')[:800]}")


def download(url, dest):
    dest.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url, timeout=600) as r, open(dest, "wb") as f:
        f.write(r.read())
    return dest


def urls_in(obj, prefix=""):
    if isinstance(obj, dict):
        for k, v in obj.items():
            yield from urls_in(v, f"{prefix}{k}.")
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            yield from urls_in(v, f"{prefix}{i}.")
    elif isinstance(obj, str) and obj.startswith("https://") and "thumbnail" not in prefix and "preview" not in prefix:
        yield prefix.rstrip("."), obj


def main():
    cmd = sys.argv[1]
    if cmd == "balance":
        print(json.dumps(call("GET", "v1/balance")))
    elif cmd == "create":
        res, payload, label = sys.argv[2], sys.argv[3], sys.argv[4]
        body = json.loads(pathlib.Path(payload).read_text(encoding="utf-8")) if payload.endswith(".json") else json.loads(payload)
        before = call("GET", "v1/balance").get("balance")
        tid = call("POST", res, body)["result"]
        with open(LOG, "a", encoding="utf-8") as f:
            f.write(json.dumps({"time": datetime.datetime.now().isoformat(timespec="seconds"), "label": label,
                                "resource": res, "task_id": tid, "balance_before": before, "request": body}) + "\n")
        print(tid)
    elif cmd == "wait":
        res, tid = sys.argv[2], sys.argv[3]
        out = pathlib.Path(sys.argv[4]) if len(sys.argv) > 4 else HERE / tid
        while True:
            t = call("GET", f"{res}/{tid}")
            st = t.get("status")
            if st in ("SUCCEEDED", "FAILED", "CANCELED"):
                break
            time.sleep(10)
        out.mkdir(parents=True, exist_ok=True)
        (out / "task.json").write_text(json.dumps(t, indent=2), encoding="utf-8")
        print("STATUS", st, t.get("task_error") or "")
        if st == "SUCCEEDED":
            for key, url in urls_in({k: v for k, v in t.items() if k not in ("thumbnail_url",)}):
                name = key.replace(".", "_")
                ext = pathlib.Path(url.split("?")[0]).suffix or ".bin"
                print("SAVED", download(url, out / (name + ext)))
        print("BALANCE", call("GET", "v1/balance").get("balance"))


if __name__ == "__main__":
    main()
