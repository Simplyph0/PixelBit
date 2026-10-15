#!/usr/bin/env python3
"""PixelBit Flasher: flash a PixelBit .bin onto one or many consoles.

Works on Linux, macOS and Windows. Needs Python 3.8+ with tkinter (ships with
python.org installers; on Linux install python3-tk). It drives esptool, and will
offer to install it (and pyserial) with pip the first time.

  - lists every ACM*/USB* serial port (COMx on Windows) and keeps the list live
    as consoles are plugged in or out
  - selects the lowest-numbered port on its own (until you pick one yourself)
  - several consoles can be selected and are flashed at the same time
  - 460800 baud by default; a failed run is retried once at 115200
  - chip erase is off by default (it also wipes saved scores, settings and videos)
  - "flash new consoles as they appear" for flashing a whole batch hands-free
"""
import json
import os
import queue
import glob
import re
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
except ImportError:  # pragma: no cover
    sys.exit("tkinter is missing. Linux: install python3-tk (apt/dnf/pacman tk). "
             "macOS/Windows: use the installer from python.org.")

BAUDS = ["115200", "230400", "460800", "921600", "1500000"]
DEFAULT_BAUD = "460800"
FALLBACK_BAUD = "115200"
CONFIG = Path.home() / ".pixelbit_flasher.json"
NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)   # Windows: no console flash

# A port we care about: ttyACM*/ttyUSB* (Linux), cu.usbmodem*/cu.usbserial*/... (macOS),
# COMx that is a USB device (Windows).
PORT_RE = re.compile(r"(ttyACM|ttyUSB|cu\.usb|cu\.wchusb|cu\.SLAB|COM\d+)", re.I)
PCT_RE = re.compile(r"(\d{1,3}(?:\.\d+)?)\s*%")


# ---------------------------------------------------------------- logic (no GUI)

def port_number(dev):
    """Trailing number of a port name: /dev/ttyACM3 -> 3, COM12 -> 12."""
    m = re.search(r"(\d+)\D*$", dev)
    return int(m.group(1)) if m else 10 ** 6


def list_ports():
    """[(device, description)] of candidate consoles, lowest number first."""
    try:
        from serial.tools import list_ports as lp
    except ImportError:   # pyserial lives in esptool's own environment (pipx): scan device files
        pats = ["/dev/ttyACM*", "/dev/ttyUSB*", "/dev/cu.usb*", "/dev/cu.wchusb*", "/dev/cu.SLAB*"]
        devs = sorted({d for pat in pats for d in glob.glob(pat)}, key=lambda d: (port_number(d), d))
        return [(d, "serial device") for d in devs]
    found = []
    for p in lp.comports():
        dev = p.device
        if not PORT_RE.search(dev):
            continue
        if dev.upper().startswith("COM") and "USB" not in (p.hwid or "").upper():
            continue   # onboard / Bluetooth COM ports
        desc = p.description if p.description and p.description != "n/a" else "serial device"
        found.append((dev, desc))
    found.sort(key=lambda d: (port_number(d[0]), d[0]))
    return found


def esptool_base():
    """How to launch esptool: the `esptool` program on PATH (pipx, pip --user, distro
    packages), else `python -m esptool`. None if neither works."""
    for name in ("esptool", "esptool.py"):
        exe = shutil.which(name)
        if exe:
            return [exe]
    try:
        r = subprocess.run([sys.executable, "-c", "import esptool"], capture_output=True,
                           timeout=30, creationflags=NO_WINDOW)
        if r.returncode == 0:
            return [sys.executable, "-m", "esptool"]
    except Exception:
        pass
    return None


def esptool_ok():
    return esptool_base() is not None


_hyphen = None


def hyphens():
    """esptool 5 spells commands write-flash / hard-reset and warns about the underscores."""
    global _hyphen
    if _hyphen is None:
        base = esptool_base()
        try:
            r = subprocess.run([*base, "version"], capture_output=True, text=True, timeout=30,
                               creationflags=NO_WINDOW)
            m = re.search(r"v?(\d+)\.\d+", r.stdout + r.stderr)
            _hyphen = bool(m) and int(m.group(1)) >= 5
        except Exception:
            _hyphen = False
    return _hyphen


def cmdname(name):
    return name.replace("_", "-") if hyphens() else name


def permission_hint(port):
    """Tell the user which group to join, from the port's actual owner group (POSIX)."""
    try:
        import grp
        grp_name = grp.getgrgid(os.stat(port).st_gid).gr_name
        return (f"No permission to open {port}. Its group is '{grp_name}': run "
                f"`sudo usermod -aG {grp_name} $USER`, then log out and back in.")
    except Exception:
        return f"No permission to open {port}: add yourself to the dialout/uucp group and log in again."


def check_image(path):
    """Return a warning string if the file does not look like an ESP8266 image, else ''."""
    try:
        size = os.path.getsize(path)
        with open(path, "rb") as f:
            magic = f.read(1)
    except OSError as e:
        return str(e)
    if size < 64 * 1024:
        return f"The file is only {size} bytes; a PixelBit firmware is several hundred KB."
    if magic != b"\xe9":
        return "This does not start with the ESP8266 image marker (0xE9); it may not be firmware."
    return ""


def guess_bin():
    """Newest PixelBit*.bin in the current folder or Downloads (not the -flipped one)."""
    best = None
    for d in (Path.cwd(), Path.home() / "Downloads"):
        for f in d.glob("PixelBit*.bin"):
            if "flipped" in f.name.lower():
                continue
            if best is None or f.stat().st_mtime > best.stat().st_mtime:
                best = f
    return str(best) if best else ""


def esptool_cmd(port, baud, *args):
    return [*(esptool_base() or [sys.executable, "-m", "esptool"]), "--chip", "esp8266",
            "--port", port, "--baud", str(baud), *args]


def run_stream(cmd, on_line, on_pct):
    """Run cmd, feeding output lines (split on \\r and \\n) to on_line, percents to on_pct."""
    env = dict(os.environ, PYTHONUNBUFFERED="1", NO_COLOR="1")
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env,
                         creationflags=NO_WINDOW)
    buf = b""
    while True:
        ch = p.stdout.read(1)
        if not ch:
            break
        if ch in (b"\r", b"\n"):
            if buf:
                line = buf.decode("utf-8", "replace").strip()
                buf = b""
                if line:
                    m = PCT_RE.search(line)
                    if m and "Writing" in line:
                        on_pct(float(m.group(1)))
                    else:
                        on_line(line)
        else:
            buf += ch
    return p.wait()


# ---------------------------------------------------------------- GUI

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("PixelBit Flasher")
        self.minsize(640, 560)
        self.q = queue.Queue()
        self.busy = set()           # ports being flashed
        self.known = []             # last port list
        self.user_picked = False    # user touched the selection: stop auto-selecting
        self.cfg = self.load_cfg()
        self.build()
        self.after(100, self.pump)
        threading.Thread(target=self.poll_ports, daemon=True).start()
        threading.Thread(target=self.check_esptool, daemon=True).start()

    # -- settings
    def load_cfg(self):
        try:
            return json.loads(CONFIG.read_text())
        except Exception:
            return {}

    def save_cfg(self):
        self.cfg.update(bin=self.bin_var.get(), baud=self.baud_var.get(),
                        erase=self.erase_var.get(), auto=self.auto_var.get())
        try:
            CONFIG.write_text(json.dumps(self.cfg))
        except OSError:
            pass

    # -- layout
    def build(self):
        pad = dict(padx=8, pady=4)
        f = ttk.Frame(self, padding=8)
        f.pack(fill="both", expand=True)
        f.columnconfigure(1, weight=1)
        f.rowconfigure(3, weight=1)
        f.rowconfigure(6, weight=2)

        ttk.Label(f, text="Firmware").grid(row=0, column=0, sticky="w", **pad)
        self.bin_var = tk.StringVar(value=self.cfg.get("bin") or guess_bin())
        ttk.Entry(f, textvariable=self.bin_var).grid(row=0, column=1, sticky="ew", **pad)
        ttk.Button(f, text="Browse…", command=self.browse).grid(row=0, column=2, **pad)

        opts = ttk.Frame(f)
        opts.grid(row=1, column=0, columnspan=3, sticky="ew", **pad)
        ttk.Label(opts, text="Baud").pack(side="left")
        self.baud_var = tk.StringVar(value=self.cfg.get("baud", DEFAULT_BAUD))
        ttk.Combobox(opts, textvariable=self.baud_var, values=BAUDS, width=9).pack(side="left", padx=(4, 16))
        self.erase_var = tk.BooleanVar(value=self.cfg.get("erase", False))
        ttk.Checkbutton(opts, text="Erase chip first", variable=self.erase_var,
                        command=self.erase_warn).pack(side="left", padx=(0, 16))
        self.auto_var = tk.BooleanVar(value=False)   # never starts on: it flashes without asking
        ttk.Checkbutton(opts, text="Flash new consoles as they appear",
                        variable=self.auto_var, command=self.auto_warn).pack(side="left")

        head = ttk.Frame(f)
        head.grid(row=2, column=0, columnspan=3, sticky="ew", **pad)
        ttk.Label(head, text="Consoles (select one or more; ctrl/shift-click)").pack(side="left")
        ttk.Button(head, text="Select all", command=self.select_all).pack(side="right")

        self.tree = ttk.Treeview(f, columns=("port", "desc", "status"), show="headings",
                                 selectmode="extended", height=6)
        for c, t, w in (("port", "Port", 130), ("desc", "Device", 260), ("status", "Status", 180)):
            self.tree.heading(c, text=t)
            self.tree.column(c, width=w, stretch=(c != "port"))
        self.tree.grid(row=3, column=0, columnspan=3, sticky="nsew", **pad)
        self.tree.bind("<<TreeviewSelect>>", self.on_select)
        self.tree.bind("<ButtonRelease-1>", lambda e: setattr(self, "user_picked", True))

        self.bar = ttk.Progressbar(f, maximum=100)
        self.bar.grid(row=4, column=0, columnspan=3, sticky="ew", **pad)

        self.go = ttk.Button(f, text="Flash", command=self.flash_selected)
        self.go.grid(row=5, column=0, columnspan=3, sticky="ew", **pad)

        self.log = tk.Text(f, height=10, state="disabled", wrap="word", font=("TkFixedFont", 9))
        self.log.grid(row=6, column=0, columnspan=3, sticky="nsew", **pad)
        sb = ttk.Scrollbar(f, command=self.log.yview)
        self.log["yscrollcommand"] = sb.set
        self.status = tk.StringVar(value="Looking for consoles…")
        ttk.Label(f, textvariable=self.status).grid(row=7, column=0, columnspan=3, sticky="w", **pad)
        self.protocol("WM_DELETE_WINDOW", self.close)
        self.bind("<Return>", lambda e: self.flash_selected())

    # -- helpers
    def say(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def browse(self):
        p = filedialog.askopenfilename(title="Choose firmware",
                                       filetypes=[("Firmware", "*.bin"), ("All files", "*")],
                                       initialdir=str(Path(self.bin_var.get()).parent)
                                       if self.bin_var.get() else str(Path.home()))
        if p:
            self.bin_var.set(p)

    def erase_warn(self):
        if self.erase_var.get():
            messagebox.showwarning(
                "Erase chip",
                "Erasing the whole chip also deletes the console's high scores, settings and "
                "uploaded videos. Leave this off for a normal update.")

    def auto_warn(self):
        if self.auto_var.get():
            if not messagebox.askokcancel(
                    "Flash automatically",
                    "Every console you plug in will be flashed straight away with the firmware "
                    "above. Consoles already connected are not touched. Continue?"):
                self.auto_var.set(False)

    def select_all(self):
        self.user_picked = True
        self.tree.selection_set(self.tree.get_children())

    def on_select(self, _e=None):
        n = len(self.tree.selection())
        self.go.configure(text=f"Flash {n} consoles" if n > 1 else "Flash")

    # -- background workers
    def check_esptool(self):
        self.q.put(("esptool", esptool_ok()))

    def poll_ports(self):
        while True:
            self.q.put(("ports", list_ports()))
            time.sleep(1.0)

    def install_esptool(self):
        # pipx first (an isolated install, and pip refuses to touch "externally managed" Pythons)
        if shutil.which("pipx"):
            cmd = ["pipx", "install", "esptool"]
        else:
            cmd = [sys.executable, "-m", "pip", "install", "--user", "esptool", "pyserial"]
        self.say("Installing esptool: " + " ".join(cmd))

        def work():
            try:
                r = subprocess.run(cmd, capture_output=True, text=True, creationflags=NO_WINDOW)
                out, ok = r.stdout + r.stderr, r.returncode == 0
            except OSError as e:
                out, ok = str(e), False
            self.q.put(("installed", ok and esptool_ok(), out[-800:]))
        threading.Thread(target=work, daemon=True).start()

    # -- event pump (GUI thread)
    def pump(self):
        try:
            while True:
                self.handle(self.q.get_nowait())
        except queue.Empty:
            pass
        self.after(100, self.pump)

    def handle(self, ev):
        kind = ev[0]
        if kind == "ports":
            self.update_ports(ev[1])
        elif kind == "esptool":
            if not ev[1] and messagebox.askyesno(
                    "esptool missing", "esptool (the tool that talks to the chip) was not found on PATH.\n"
                                       "Install it now (pipx if you have it, else pip)?"):
                self.install_esptool()
        elif kind == "installed":
            self.say(ev[2])
            self.say("esptool installed." if ev[1] else "Install failed; run: pipx install esptool")
        elif kind == "status":
            if self.tree.exists(ev[1]):
                self.tree.set(ev[1], "status", ev[2])
        elif kind == "pct":
            self.bar["value"] = ev[1]
        elif kind == "log":
            self.say(ev[1])
        elif kind == "done":
            self.busy.discard(ev[1])
            if not self.busy:
                self.go.configure(state="normal")
                self.bar["value"] = 0
                self.status.set("Done. Unplug, plug in the next console, or press Flash again.")

    def update_ports(self, ports):
        old = {d for d, _ in self.known}
        new = {d for d, _ in ports}
        if ports == self.known:
            return
        selected = set(self.tree.selection())
        self.tree.delete(*self.tree.get_children())
        for dev, desc in ports:
            self.tree.insert("", "end", iid=dev, values=(dev, desc, "flashing…" if dev in self.busy else "ready"))
        keep = [d for d in selected if d in new]
        added = sorted(new - old, key=lambda d: (port_number(d), d)) if self.known or old else []
        first_scan = not self.known and not old
        self.known = ports
        if ports and (not self.user_picked or not keep):
            self.tree.selection_set(ports[0][0])         # lowest number
        else:
            self.tree.selection_set(keep)
        self.status.set(f"{len(ports)} console port(s) found" if ports else "No console found. Plug one in.")
        if added and not first_scan and self.auto_var.get():
            self.flash([d for d in added if d not in self.busy], auto=True)

    # -- flashing
    def flash_selected(self):
        self.flash(list(self.tree.selection()))

    def flash(self, ports, auto=False):
        path = self.bin_var.get().strip()
        if not ports:
            if not auto:
                messagebox.showinfo("Flash", "No console selected.")
            return
        if not os.path.isfile(path):
            messagebox.showerror("Flash", "Choose a firmware .bin file first.")
            return
        warn = check_image(path)
        if warn and not auto and not messagebox.askyesno("Check the file", warn + "\n\nFlash it anyway?"):
            return
        if self.erase_var.get() and not auto and not messagebox.askyesno(
                "Erase chip", "The chip will be erased first: scores, settings and videos are lost.\nContinue?"):
            return
        self.save_cfg()
        baud, erase = self.baud_var.get() or DEFAULT_BAUD, self.erase_var.get()
        ports = [p for p in ports if p not in self.busy]
        if not ports:
            return
        self.go.configure(state="disabled")
        self.status.set("Flashing " + ", ".join(ports) + "…")
        self.say(f"--- {os.path.basename(path)} -> {', '.join(ports)} at {baud} baud"
                 f"{' with chip erase' if erase else ''}")
        for p in ports:
            self.busy.add(p)
            threading.Thread(target=self.job, args=(p, path, baud, erase, len(ports) > 1),
                             daemon=True).start()

    def job(self, port, path, baud, erase, many):
        tag = f"[{port}] " if many else ""
        put = self.q.put

        denied = []

        def line(s):
            if "Permission denied" in s:
                denied.append(1)
            if not re.match(r"^\s*(Writing at|Wrote|Hash of data)", s) or s.startswith("Wrote"):
                put(("log", tag + s))

        def pct(v):
            if not many:
                put(("pct", v))
            put(("status", port, f"writing {v:.0f}%"))

        def attempt(b):
            if erase:
                put(("status", port, "erasing…"))
                if run_stream(esptool_cmd(port, b, "--after", cmdname("no_reset_stub"), cmdname("erase_flash")),
                              line, pct) != 0:
                    return False
            put(("status", port, "connecting…" if not erase else "writing…"))
            return run_stream(esptool_cmd(port, b, "--after", cmdname("hard_reset"), cmdname("write_flash"), "-z", "0x0", path),
                              line, pct) == 0

        t0 = time.time()
        ok = attempt(baud)
        if not ok and denied:
            put(("log", tag + permission_hint(port)))
            put(("status", port, "no permission"))
            put(("done", port))
            return
        if not ok and baud != FALLBACK_BAUD:
            put(("log", f"{tag}Failed at {baud}; retrying at {FALLBACK_BAUD}."))
            put(("status", port, "retrying slower…"))
            ok = attempt(FALLBACK_BAUD)
        if ok:
            put(("status", port, f"done ({time.time() - t0:.0f}s)"))
            put(("log", f"{tag}Flashed in {time.time() - t0:.1f}s. The console restarts by itself."))
        else:
            put(("status", port, "FAILED"))
            put(("log", f"{tag}FAILED. Hold the console's flash button / check the cable and port. "
                        f"On Linux you may need the 'dialout' (or 'uucp') group."))
        put(("done", port))

    def close(self):
        self.save_cfg()
        self.destroy()


if __name__ == "__main__":
    App().mainloop()
