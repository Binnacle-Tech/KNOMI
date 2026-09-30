#!/usr/bin/env python3
"""Pre-release checks for the KNOMI firmware. Catches the kinds of bugs that shipped before:

  stack     each FreeRTOS task's worst-case stack use, from the compiled firmware (the OP23-OP25
            boot loop: the UI task had 4 KB and Coaster needed more)
  routes    web routes answered by the wrong handler (the OP17 one-click update: /update grabbed
            /update/github)
  tables    tables that must line up with their enums (Coaster's faces were out of order)
  pages     JavaScript in the built-in web pages parses
  size      firmware fits its OTA slot with room to spare
  device    optional: runs on a real KNOMI for a while and watches for restarts, low stacks and
            broken JSON endpoints

Usage (from the repo root):
  python tools/check.py                      static checks + the last build in .pio
  python tools/check.py --build              build first (pio run), then check
  python tools/check.py --device 192.168.1.87 --soak 180
                                             after flashing: watch the KNOMI for 3 minutes
  python tools/check.py --plugin ../OctoPrint-KNOMI   also check the sidebar matches the firmware
  python tools/check.py --device 192.168.1.87 --flash release-op32/knomiv2-octoprint-firmware.bin
  python tools/check.py --device 192.168.1.87 --perf 600       measure fps, CPU and memory over WiFi (OP33+)
                                             flash over WiFi, then watch it (refuses if any check failed)

Exit code 0 = all good, 1 = something failed. Warnings don't fail the run.
"""
import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")
APP_SLOT = 0x480000          # app0/app1 size in spiffs_16MB.csv
STACK_MARGIN = 0.6           # estimated worst case should stay under 60 % of a task's stack
BIG_FRAME = 2048             # one function using this much stack is worth a look

results = []                 # (level, check, message)


def fail(check, msg): results.append(("FAIL", check, msg))
def warn(check, msg): results.append(("WARN", check, msg))
def ok(check, msg): results.append(("ok", check, msg))


def read(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def strip_comments(code):
    code = re.sub(r"/\*.*?\*/", "", code, flags=re.S)
    return re.sub(r"//[^\n]*", "", code)


# ---------------------------------------------------------------- tables

def enum_names(code, last):
    """Names of the enum that ends with `last` (e.g. M_COUNT), in order, without `last`."""
    m = re.search(r"enum\s*\{([^}]*\b" + last + r"\b[^}]*)\}", code)
    if not m:
        return None
    names = [n.strip().split("=")[0].strip() for n in m.group(1).split(",")]
    names = [n for n in names if n]
    return names[:names.index(last)]


def array_items(code, name, strings=True):
    m = re.search(r"\b" + re.escape(name) + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\};", code, re.S)
    if not m:
        return None
    body = m.group(1)
    if strings:
        return re.findall(r'"((?:[^"\\]|\\.)*)"', body)
    rows = re.findall(r"\{[^{}]*\}", body)
    return rows if rows else [x for x in body.split(",") if x.strip()]


def check_tables(plugin):
    path = os.path.join(SRC, "knomi_coaster.cpp")
    code = strip_comments(read(path))
    pairs = [  # (enum end, array, is-string-array)
        ("M_COUNT", "MOOD_NAMES", True), ("M_COUNT", "MOODS", False),
        ("Q_COUNT", "QUIRK_NAMES", True), ("Q_COUNT", "QUIRK_DUR", False),
        ("DECO_COUNT", "DECO_KEYS", True), ("LK_COUNT", "LIKE_NAMES", True),
    ]
    bad = 0
    for last, arr, strings in pairs:
        names = enum_names(code, last)
        items = array_items(code, arr, strings)
        if names is None or items is None:
            warn("tables", f"couldn't find {last} or {arr} in knomi_coaster.cpp")
            continue
        if len(names) != len(items):
            fail("tables", f"{arr} has {len(items)} entries but its enum has {len(names)} ({last})")
            bad += 1
    # MOODS rows carry a comment with the mood name: they must follow the enum order
    raw = read(path)
    m = re.search(r"MOODS\[M_COUNT\]\s*=\s*\{(.*?)\n\};", raw, re.S)
    names = array_items(code, "MOOD_NAMES")
    if m and names:
        tags = re.findall(r"/\*\s*([a-z ]+?)\s*\*/", m.group(1))
        alias = {"impatient": "heating up", "cooling": "cooling off", "almost": "almost there", "nervous": "hanging on",
                 "shocked": "shocked", "ready": "ready"}
        for i, (tag, name) in enumerate(zip(tags, names)):
            if alias.get(tag, tag) != name and not name.startswith(tag):
                fail("tables", f"MOODS row {i} is '{tag}' but MOOD_NAMES[{i}] is '{name}' (rows out of order)")
                bad += 1
                break
    if plugin:
        js = os.path.join(plugin, "octoprint_knomi", "static", "js", "coaster.js")
        if os.path.exists(js):
            src = read(js)
            mm = re.search(r"var M = (\{.*?\});\n", src, re.S)
            quirks = array_items(code, "QUIRK_NAMES") or []
            if mm and names:
                side = set(json.loads(mm.group(1)).keys())
                missing = [n for n in names if n not in side]
                if missing:
                    fail("tables", "the OctoPrint sidebar doesn't know these moods: " + ", ".join(missing))
                    bad += 1
            qd = re.search(r"var QDUR = (\{.*?\});", src, re.S)
            if qd:
                side_q = set(k.strip() for k in re.findall(r'"?([a-z ]+)"?\s*:', qd.group(1)))
                missing = [q for q in quirks if q and q not in side_q]
                if missing:
                    fail("tables", "the OctoPrint sidebar doesn't know these quirks: " + ", ".join(missing))
                    bad += 1
        else:
            warn("tables", f"no sidebar script at {js}")
    if not bad:
        ok("tables", "mood, quirk, decoration and likes tables line up" + (" (and the sidebar)" if plugin else ""))


# ---------------------------------------------------------------- routes

def find_calls(code, start):
    """server.on(...) calls from `start` on: (path, methods, has_exact_filter)."""
    out = []
    for m in re.finditer(r'server\.on\(\s*"([^"]+)"\s*,\s*([A-Z_| ]+)\s*,', code):
        # find the end of this call: matching parenthesis
        depth, i = 0, m.start() + len("server.on")
        while i < len(code):
            if code[i] == "(":
                depth += 1
            elif code[i] == ")":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        tail = code[i + 1:i + 60]
        exact = re.match(r"\s*\.setFilter\(\s*exact\(", tail) is not None
        methods = set(x.strip() for x in m.group(2).split("|"))
        out.append((m.group(1), methods, exact))
    return out


def check_routes():
    path = os.path.join(SRC, "webserver.cpp")
    code = strip_comments(read(path))
    extra = strip_comments(read(os.path.join(SRC, "backup.cpp"))) if os.path.exists(os.path.join(SRC, "backup.cpp")) else ""
    # route groups by function, then the order webserver_setup registers them in
    funcs = {}
    for m in re.finditer(r"static void (\w+_routes)\(void\)\s*\{", code):
        body_start = m.end()
        depth, i = 1, body_start
        while depth and i < len(code):
            depth += {"{": 1, "}": -1}.get(code[i], 0)
            i += 1
        funcs[m.group(1)] = code[body_start:i]
    setup = re.search(r"void webserver_setup\(void\)\s*\{(.*)", code, re.S)
    if not setup:
        warn("routes", "couldn't find webserver_setup()")
        return
    body = setup.group(1)
    order = []   # (path, methods, exact, source)
    for m in re.finditer(r"(\w+_routes)\((?:server)?\)|AsyncElegantOTA\.begin|server\.on\(", body):
        tok = m.group(0)
        if tok.startswith("AsyncElegantOTA"):
            order.append(("/update", {"HTTP_ANY"}, False, "AsyncElegantOTA"))
        elif tok.startswith("server.on"):
            for r in find_calls(body[m.start():m.start() + 4000], 0)[:1]:
                order.append(r + ("webserver_setup",))
        else:
            name = m.group(1)
            src = funcs.get(name, extra if name == "backup_routes" else "")
            for r in find_calls(src, 0):
                order.append(r + (name,))
    bad = 0
    for i, (p1, m1, exact1, s1) in enumerate(order):
        for p2, m2, _, s2 in order[i + 1:]:
            same_method = "HTTP_ANY" in m1 or "HTTP_ANY" in m2 or m1 & m2
            if p2.startswith(p1.rstrip("/") + "/") and p1 != "/" and same_method and not exact1:
                fail("routes", f'{p2} ({s2}) is answered by {p1} ({s1}), registered first. '
                               f'Add .setFilter(exact("{p1}")) or register {p2} earlier.')
                bad += 1
    if not bad:
        ok("routes", f"{len(order)} web routes, none hidden by another")


# ---------------------------------------------------------------- pages

def check_pages():
    node = shutil.which("node")
    if not node:
        warn("pages", "node not found, skipped the JavaScript syntax check")
        return
    bad = 0
    count = 0
    import tempfile
    for h in sorted(glob.glob(os.path.join(SRC, "*_html.h"))):
        page = read(h)
        raw = "".join(x[1] for x in re.findall(r'R"(\w*)\((.*?)\)\1"', page, re.S))
        for j, script in enumerate(re.findall(r"<script[^>]*>(.*?)</script>", raw, re.S)):
            if "index_html" in h:
                script = script.replace("%", "0")   # template placeholders
            with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False, encoding="utf-8") as f:
                f.write(script)
                tmp = f.name
            r = subprocess.run([node, "--check", tmp], capture_output=True, text=True)
            os.unlink(tmp)
            count += 1
            if r.returncode:
                err = (r.stderr.strip().splitlines() or ["?"])
                fail("pages", f"{os.path.basename(h)} script {j + 1} doesn't parse: {err[-1]}")
                bad += 1
    if not bad:
        ok("pages", f"{count} page scripts parse")
    # every page uses the shared look: same head (colors, color modes) and the same top bar
    same = 0
    for h in sorted(glob.glob(os.path.join(SRC, "*_html.h"))) + [os.path.join(SRC, "webserver.cpp")]:
        page = read(h)
        if re.search(r"<header class=[\"']rail", page):
            fail("pages", f"{os.path.basename(h)} has its own top bar; use BINNACLE_RAIL so every page matches")
            same += 1
        if h.endswith("_html.h") and not ("BINNACLE_HEAD" in page and "BINNACLE_RAIL" in page):
            fail("pages", f"{os.path.basename(h)} doesn't use BINNACLE_HEAD and BINNACLE_RAIL (binnacle_css.h)")
            same += 1
    if not same:
        ok("pages", "every page uses the shared head and top bar")


# ---------------------------------------------------------------- build: size and stack

def tool(name):
    for base in (os.path.expanduser("~/.platformio/packages"),):
        for p in glob.glob(os.path.join(base, "toolchain-xtensa-esp32*", "bin", "xtensa-esp32*-elf-" + name + "*")):
            return p
    return shutil.which("xtensa-esp32s3-elf-" + name) or shutil.which("xtensa-esp32-elf-" + name)


def call_graph(elf, objdump):
    """{function: (frame bytes, [callees])} from the disassembly (windowed ABI: `entry a1, N`)."""
    out = subprocess.run([objdump, "-d", "--no-show-raw-insn", "-C", elf], capture_output=True, text=True, errors="replace").stdout
    graph, cur = {}, None
    head = re.compile(r"^[0-9a-f]+ <(.+)>:$")
    entry = re.compile(r"\sentry\s+a1,\s*(0x[0-9a-f]+|\d+)")
    call = re.compile(r"\scall(?:0|4|8|12)\s+[0-9a-f]+ <([^>+]+)>")
    for line in out.splitlines():
        m = head.match(line)
        if m:
            cur = m.group(1)
            graph[cur] = [0, set(), False]
            continue
        if cur is None:
            continue
        g = graph[cur]
        if not g[2]:
            e = entry.search(line)
            if e:
                g[0] = int(e.group(1), 0); g[2] = True
                continue
        c = call.search(line)
        if c:
            g[1].add(c.group(1))
    return {k: (v[0], v[1]) for k, v in graph.items()}


# Calls that go through function pointers (the VFS layer under LittleFS, the network stack
# under HTTPClient, TLS) can't be followed in the disassembly. These floors are rough measured
# depths for what hides behind them, so paths through them aren't underestimated.
HIDDEN_DEPTH = [
    (r"^fs::FS::open", 2000), (r"^fs::File::(write|read|close|flush|seek)", 1400), (r"^fs::FS::(remove|rename|exists|mkdir)", 1800),
    (r"^HTTPClient::(GET|POST|PUT|sendRequest|begin|getString)", 1800), (r"^WiFiClientSecure::connect", 6000),
    (r"^deserializeJson|^ArduinoJson.*deserialize", 600),
]


def hidden(fn):
    return max((d for rx, d in HIDDEN_DEPTH if re.search(rx, fn)), default=0)


def worst(graph, fn, memo, stack):
    if fn in memo:
        return memo[fn]
    if fn not in graph or fn in stack:
        return 0, [fn]
    frame, callees = graph[fn]
    stack.add(fn)
    best, path = 0, []
    for c in callees:
        d, p = worst(graph, c, memo, stack)
        d = max(d, hidden(c))
        if d > best:
            best, path = d, p
    stack.discard(fn)
    memo[fn] = (frame + best, [fn] + path)
    return memo[fn]


def short(name):
    return re.sub(r"\(.*", "", name)


def check_build(env):
    build = os.path.join(ROOT, ".pio", "build", env)
    elf = os.path.join(build, "firmware.elf")
    binf = os.path.join(build, "firmware.bin")
    if not os.path.exists(elf):
        warn(f"build {env}", "no build found, run with --build")
        return
    size = os.path.getsize(binf)
    pct = size * 100 / APP_SLOT
    if pct > 95:
        fail(f"size {env}", f"firmware is {size // 1024} KB, {pct:.0f}% of the OTA slot")
    elif pct > 85:
        warn(f"size {env}", f"firmware is {size // 1024} KB, {pct:.0f}% of the OTA slot")
    else:
        ok(f"size {env}", f"firmware is {size // 1024} KB, {pct:.0f}% of the OTA slot")

    objdump = tool("objdump")
    if not objdump:
        warn(f"stack {env}", "xtensa objdump not found, skipped the stack check")
        return
    graph = call_graph(elf, objdump)
    code = "\n".join(strip_comments(read(p)) for p in glob.glob(os.path.join(SRC, "**", "*.cpp"), recursive=True))
    tasks = re.findall(r'xTaskCreate(?:PinnedToCore)?\(\s*(\w+)\s*,\s*"([^"]+)"\s*,\s*(\d+)', code)
    # LVGL calls our draw/event callbacks through function pointers: follow them by hand
    callbacks = [f for f in graph if re.search(r"(_cb|_event|draw_face|face_event)\(_?lv_event", f)]
    memo = {}
    bad = 0
    for fn, name, stack_size in tasks:
        stack_size = int(stack_size)
        root = next((f for f in graph if short(f) == fn), None)
        if not root:
            ok(f"stack {env}", f"task {name}: not in this build")
            continue
        est, path = worst(graph, root, memo, set())
        if fn == "lvgl_ui_task" and callbacks:
            # the screen refresh (a timer, called through a pointer) sends draw events to our callbacks
            refr = [f for f in graph if short(f) in ("lv_timer_handler", "_lv_disp_refr_timer", "lv_event_send")]
            base = max((worst(graph, f, memo, set())[0] for f in refr), default=0)
            cb = max((worst(graph, c, memo, set()) for c in callbacks), key=lambda x: x[0])
            if 48 + base + cb[0] > est:
                est, path = 48 + base + cb[0], ["lv_timer_handler", "screen refresh"] + cb[1]
        est += 512   # interrupts, FreeRTOS bookkeeping, calls we can't see
        chain = " > ".join(short(p) for p in path[:6]) + (" > ..." if len(path) > 6 else "")
        msg = f"task {name}: about {est} of {stack_size} bytes worst case ({chain})"
        if est > stack_size * 0.85:
            fail(f"stack {env}", msg + ". Too close to overflowing: raise its stack in xTaskCreate or move big locals to the heap")
            bad += 1
        elif est > stack_size * STACK_MARGIN:
            warn(f"stack {env}", msg + f". Over {int(STACK_MARGIN * 100)}%: raise its stack")
        else:
            ok(f"stack {env}", msg)
    big = sorted(((frame, f) for f, (frame, _) in graph.items() if frame >= BIG_FRAME and "src" not in f), reverse=True)
    ours = set(re.findall(r"\b(\w+)\s*\([^;{]*\)\s*\{", code))
    for frame, f in big:
        if short(f).split("::")[-1] in ours:
            warn(f"stack {env}", f"{short(f)} uses {frame} bytes of stack by itself: consider the heap")


def build(envs, pio):
    for env in envs:
        print(f"building {env}...", flush=True)
        r = subprocess.run([pio, "run", "-e", env], cwd=ROOT, capture_output=True, text=True)
        if r.returncode:
            tail = "\n".join((r.stdout + r.stderr).splitlines()[-25:])
            fail(f"build {env}", "the build failed:\n" + tail)
        else:
            ok(f"build {env}", "builds")


# ---------------------------------------------------------------- device

def get_json(url, timeout=5):
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode("utf-8", "replace"))


def flash_device(ip, binfile):
    """Upload firmware over WiFi (the KNOMI's /update page), then wait for it to come back."""
    import hashlib
    import uuid
    base = f"http://{ip}"
    data = open(binfile, "rb").read()
    try:
        before = get_json(base + "/log/info").get("fw", "?")
    except Exception as e:
        fail("flash", f"can't reach the KNOMI at {ip}: {e}")
        return False
    print(f"flashing {os.path.basename(binfile)} ({len(data) // 1024} KB) to {ip}, running {before} now...", flush=True)
    boundary = uuid.uuid4().hex
    md5 = hashlib.md5(data).hexdigest()
    body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"MD5\"\r\n\r\n{md5}\r\n"
            f"--{boundary}\r\nContent-Disposition: form-data; name=\"firmware\"; filename=\"firmware\"\r\n"
            f"Content-Type: application/octet-stream\r\n\r\n").encode() + data + f"\r\n--{boundary}--\r\n".encode()
    req = urllib.request.Request(base + "/update", data=body, method="POST",
                                 headers={"Content-Type": f"multipart/form-data; boundary={boundary}"})
    try:
        with urllib.request.urlopen(req, timeout=180) as r:
            reply = r.read().decode("utf-8", "replace")
    except Exception as e:
        fail("flash", f"upload failed: {e}")
        return False
    if "OK" not in reply.upper() and reply.strip():
        fail("flash", f"the KNOMI didn't accept it: {reply.strip()[:120]}")
        return False
    print("uploaded, waiting for it to restart...", flush=True)
    time.sleep(8)
    for _ in range(60):
        try:
            info = get_json(base + "/log/info", timeout=3)
            if info.get("uptime", 999) < 120:
                ok("flash", f"{before} -> {info.get('fw')} (running from {info.get('slot', '?')})")
                return True
        except Exception:
            pass
        time.sleep(2)
    fail("flash", "the KNOMI didn't come back within 2 minutes (if it keeps crashing, self-rescue switches back after 3 tries)")
    return False


def check_device(ip, soak):
    base = f"http://{ip}"
    try:
        info = get_json(base + "/log/info")
    except Exception as e:
        fail("device", f"can't reach the KNOMI at {ip}: {e}")
        return
    print(f"watching {ip} ({info.get('fw')}) for {soak} s...", flush=True)
    bad = 0
    for path in ("/status.json", "/coaster.json", "/coaster/state", "/coaster/card", "/update/progress", "/layout.json"):
        try:
            get_json(base + path)
        except Exception as e:
            fail("device", f"{path} doesn't return JSON ({e})")
            bad += 1
    if info.get("mood") in (None, "?"):
        fail("device", "the log page can't read Coaster's mood (/coaster/state JSON too big or broken)")
        bad += 1
    last_up, lows, t0 = info.get("uptime", 0), {}, time.time()
    while time.time() - t0 < soak:
        time.sleep(5)
        try:
            info = get_json(base + "/log/info")
        except Exception:
            continue   # busy or restarting: the uptime tells
        up = info.get("uptime", 0)
        if up < last_up:
            fail("device", f"the KNOMI restarted (uptime {last_up} s -> {up} s, reason: {info.get('reset')})")
            bad += 1
        last_up = up
        for task, free in (info.get("stacks") or {}).items():
            lows[task] = min(free, lows.get(task, free))
    if lows:
        for task, free in sorted(lows.items()):
            if free < 512:
                fail("device", f"task {task} has only {free} bytes of stack left")
                bad += 1
            elif free < 1536:
                warn("device", f"task {task} has {free} bytes of stack left")
        ok("device", "stack left per task: " + ", ".join(f"{t} {f}" for t, f in sorted(lows.items())))
    else:
        warn("device", "this firmware doesn't report stacks (older than OP27)")
    if not bad:
        ok("device", f"ran {soak} s without restarting ({info.get('fw')}, running from {info.get('slot', '?')})")


def perf_device(ip, seconds, csv_path):
    """Read /perf every 2 s over WiFi and sum it up, split into idle and printing."""
    import csv
    base = f"http://{ip}"
    try:
        info = get_json(base + "/log/info")
        get_json(base + "/perf")   # starts a fresh window
    except Exception as e:
        fail("perf", f"can't reach /perf on {ip} ({e}); it needs OP33 or newer")
        return
    print(f"measuring {ip} ({info.get('fw')}) for {seconds} s, writing {csv_path}...", flush=True)
    rows, tasks, t0 = [], set(), time.time()
    while time.time() - t0 < seconds:
        time.sleep(2)
        try:
            p = get_json(base + "/perf", timeout=4)
        except Exception:
            continue
        if "fps" not in p:
            continue
        row = {"t": round(time.time() - t0, 1)}
        for k in ("printing", "fps", "frame_ms", "frame_ms_max", "px_per_frame", "face_ms", "face_ms_max",
                  "faces_per_s", "flush_ms", "flush_ms_max", "logic_pct", "logic_ms_max", "heap", "heap_min", "heap_block", "frag", "psram"):
            row[k] = p.get(k)
        row["cpu0"], row["cpu1"] = (p.get("cpu") or [None, None])[:2]
        for name, pct in (p.get("tasks") or {}).items():
            row["task_" + name] = pct
            tasks.add("task_" + name)
        rows.append(row)
        if len(rows) % 15 == 0:
            print(f"  {row['t']:.0f} s: {row['fps']} fps, face {row['face_ms']} ms, cpu {row['cpu0']}%/{row['cpu1']}%, "
                  f"heap {row['heap'] // 1024} KB (block {row['heap_block'] // 1024} KB)"
                  + (" printing" if row["printing"] else ""), flush=True)
    if not rows:
        fail("perf", "got no samples")
        return
    cols = list(rows[0].keys() - tasks) + sorted(tasks)
    cols = ["t", "printing"] + [c for c in cols if c not in ("t", "printing")]
    with open(csv_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        w.writerows(rows)

    def avg(rs, k):
        v = [r[k] for r in rs if r.get(k) is not None]
        return sum(v) / len(v) if v else 0

    for label, rs in (("idle", [r for r in rows if not r["printing"]]), ("printing", [r for r in rows if r["printing"]])):
        if not rs:
            continue
        ok(f"perf {label}", f"{len(rs) * 2} s: {avg(rs, 'fps'):.1f} fps, frame {avg(rs, 'frame_ms'):.1f} ms avg / "
                            f"{max(r['frame_ms_max'] for r in rs)} ms worst, face {avg(rs, 'face_ms'):.2f} ms avg / "
                            f"{max(r['face_ms_max'] for r in rs):.1f} ms worst"
                            + (f", sending {avg(rs, 'flush_ms'):.1f} ms" if any(r.get('flush_ms') for r in rs) else "")
                            + (f", Coaster's logic {avg(rs, 'logic_pct'):.1f}% of a core" if any(r.get('logic_pct') for r in rs) else ""))
        ok(f"perf {label}", f"CPU core0 {avg(rs, 'cpu0'):.0f}%, core1 {avg(rs, 'cpu1'):.0f}%; tasks busy: " +
           ", ".join(f"{t[5:]} {avg(rs, t):.1f}%" for t in sorted(tasks)))
    lo = min(r["heap"] for r in rows)
    ok("perf memory", f"heap {rows[0]['heap'] // 1024} -> {rows[-1]['heap'] // 1024} KB, lowest {lo // 1024} KB, "
                      f"all-time lowest {rows[-1]['heap_min'] // 1024} KB, biggest block {min(r['heap_block'] for r in rows) // 1024} KB, "
                      f"fragmentation up to {max(r['frag'] for r in rows):.0f}%")
    if rows[-1]["heap"] < rows[0]["heap"] - 8192 and seconds >= 600:
        warn("perf memory", "free memory dropped more than 8 KB over the run: maybe a leak, run longer to be sure")
    ok("perf", f"samples saved to {csv_path}")


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", action="store_true", help="build with PlatformIO first")
    ap.add_argument("--env", action="append", help="PlatformIO environment(s), default knomiv2 and knomiv1")
    ap.add_argument("--pio", default="pio", help="the pio command (default: pio)")
    ap.add_argument("--plugin", help="path to the OctoPrint-KNOMI repo, to check the sidebar tables too")
    ap.add_argument("--device", help="IP of a KNOMI running the new firmware")
    ap.add_argument("--soak", type=int, default=120, help="seconds to watch the device (default 120)")
    ap.add_argument("--flash", nargs="?", const="auto", metavar="BIN",
                    help="with --device: upload firmware over WiFi first (default: the knomiv2 build in .pio), then watch it")
    ap.add_argument("--perf", type=int, metavar="SECONDS",
                    help="with --device: measure frame rate, CPU and memory for this long over WiFi (no flashing)")
    ap.add_argument("--csv", default="knomi-perf.csv", help="where --perf saves its samples (default knomi-perf.csv)")
    a = ap.parse_args()
    envs = a.env or ["knomiv2", "knomiv1"]

    if os.path.isdir(SRC):     # the script can also run on its own, just for --device
        check_tables(a.plugin)
        check_routes()
        check_pages()
        if a.build:
            build(envs, a.pio)
        for env in envs:
            check_build(env)
    if a.device and a.flash:
        binfile = a.flash if a.flash != "auto" else os.path.join(ROOT, ".pio", "build", "knomiv2", "firmware.bin")
        if not os.path.exists(binfile):
            fail("flash", f"no firmware at {binfile}")
        elif not any(r[0] == "FAIL" for r in results) and flash_device(a.device, binfile):
            pass
        elif any(r[0] == "FAIL" for r in results[:-1]):
            fail("flash", "not flashing: fix the failed checks first")
    if a.device and a.perf:
        perf_device(a.device, a.perf, a.csv)
    elif a.device and not any(r[1] == "flash" and r[0] == "FAIL" for r in results):
        check_device(a.device, a.soak)

    width = max(len(c) for _, c, _ in results)
    for level, check, msg in results:
        print(f"{level:4}  {check:{width}}  {msg}")
    fails = sum(1 for r in results if r[0] == "FAIL")
    warns = sum(1 for r in results if r[0] == "WARN")
    print(f"\n{'FAILED' if fails else 'PASSED'}: {fails} failed, {warns} warnings")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
