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
    tasks = re.findall(r'xTaskCreate\(\s*(\w+)\s*,\s*"([^"]+)"\s*,\s*(\d+)', code)
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


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", action="store_true", help="build with PlatformIO first")
    ap.add_argument("--env", action="append", help="PlatformIO environment(s), default knomiv2 and knomiv1")
    ap.add_argument("--pio", default="pio", help="the pio command (default: pio)")
    ap.add_argument("--plugin", help="path to the OctoPrint-KNOMI repo, to check the sidebar tables too")
    ap.add_argument("--device", help="IP of a KNOMI running the new firmware")
    ap.add_argument("--soak", type=int, default=120, help="seconds to watch the device (default 120)")
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
    if a.device:
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
