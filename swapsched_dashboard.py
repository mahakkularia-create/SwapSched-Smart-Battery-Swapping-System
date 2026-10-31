"""
SwapSched live laptop dashboard.
Install: py -m pip install pyserial
Run:     py swapsched_dashboard.py
Select the Arduino COM port and click Connect.
"""
import tkinter as tk
from tkinter import ttk, messagebox
import threading
import queue
import time
import csv

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None

BAUD = 9600

class Dashboard:
    def __init__(self, root):
        self.root = root
        self.root.title("SwapSched | Live 4-Bay Dashboard")
        self.root.geometry("900x620")
        self.root.minsize(760, 520)
        self.q = queue.Queue()
        self.ser = None
        self.reader_thread = None
        self.running = False
        self.last_data = None
        self.rows = []

        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass

        top = ttk.Frame(root, padding=12)
        top.pack(fill="x")
        ttk.Label(top, text="SWAPSCHED", font=("Segoe UI", 20, "bold")).pack(side="left")
        ttk.Label(top, text="LIVE VEHICLE & CHARGING MONITOR", font=("Segoe UI", 10)).pack(side="left", padx=16)

        controls = ttk.Frame(root, padding=(12, 0, 12, 10))
        controls.pack(fill="x")
        ttk.Label(controls, text="Arduino port:").pack(side="left")
        self.port_var = tk.StringVar()
        self.port_box = ttk.Combobox(controls, textvariable=self.port_var, width=16, state="readonly")
        self.port_box.pack(side="left", padx=6)
        ttk.Button(controls, text="Refresh", command=self.refresh_ports).pack(side="left")
        self.connect_btn = ttk.Button(controls, text="Connect", command=self.connect)
        self.connect_btn.pack(side="left", padx=6)
        ttk.Label(controls, text="Policy:").pack(side="left", padx=(18, 4))
        self.policy_var = tk.StringVar(value="P3 – Chance")
        self.policy_box = ttk.Combobox(controls, textvariable=self.policy_var,
                                       values=["P2 – Mean", "P3 – Chance"], width=15, state="readonly")
        self.policy_box.pack(side="left")
        self.policy_box.bind("<<ComboboxSelected>>", self.change_policy)
        self.status_var = tk.StringVar(value="Disconnected")
        ttk.Label(controls, textvariable=self.status_var, foreground="#a33").pack(side="right")

        self.cards = []
        grid = ttk.Frame(root, padding=12)
        grid.pack(fill="x")
        for i in range(4):
            card = ttk.LabelFrame(grid, text=f"VEHICLE 0{i+1}  |  BAY 0{i+1}", padding=14)
            card.grid(row=i//2, column=i%2, sticky="nsew", padx=7, pady=7)
            grid.columnconfigure(i%2, weight=1)
            grid.rowconfigure(i//2, weight=1)
            state = tk.StringVar(value="Waiting for Arduino data")
            soc = tk.StringVar(value="SoC: --%")
            action = tk.StringVar(value="Relay: OFF")
            label = ttk.Label(card, textvariable=state, font=("Segoe UI", 14, "bold"))
            label.pack(anchor="w")
            ttk.Label(card, textvariable=soc, font=("Segoe UI", 18)).pack(anchor="w", pady=6)
            ttk.Label(card, textvariable=action).pack(anchor="w")
            bar = ttk.Progressbar(card, maximum=100, length=300, mode="determinate")
            bar.pack(fill="x", pady=(10, 0))
            self.cards.append((state, soc, action, bar, label))

        metrics = ttk.LabelFrame(root, text="STATION INFORMATION", padding=12)
        metrics.pack(fill="x", padx=19, pady=8)
        self.metric_vars = {}
        metric_names = [
            ("clock", "RTC time"), ("step", "Simulation step"),
            ("arrivals", "New swap requests"), ("served", "Vehicles served"),
            ("stockouts", "Unserved requests"), ("ready", "Ready batteries"),
            ("tariff", "Simulated tariff"), ("temp", "Temperature"),
            ("humidity", "Humidity"), ("active", "Charging bays")
        ]
        for idx, (key, title) in enumerate(metric_names):
            cell = ttk.Frame(metrics)
            cell.grid(row=idx//5, column=idx%5, sticky="w", padx=10, pady=7)
            ttk.Label(cell, text=title, font=("Segoe UI", 8)).pack(anchor="w")
            var = tk.StringVar(value="--")
            ttk.Label(cell, textvariable=var, font=("Segoe UI", 12, "bold")).pack(anchor="w")
            self.metric_vars[key] = var

        bottom = ttk.Frame(root, padding=12)
        bottom.pack(fill="x")
        ttk.Button(bottom, text="Save received data as CSV", command=self.save_csv).pack(side="left")
        ttk.Label(bottom, text="Vehicle names are simulated identifiers; SoC and tariff are model values.",
                  font=("Segoe UI", 8)).pack(side="right")

        self.refresh_ports()
        self.root.after(100, self.process_queue)
        self.root.protocol("WM_DELETE_WINDOW", self.close)

    def refresh_ports(self):
        if list_ports is None:
            self.status_var.set("Install pyserial first")
            return
        ports = [p.device for p in list_ports.comports()]
        self.port_box["values"] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])

    def connect(self):
        if serial is None:
            messagebox.showerror("Missing package", "Run: py -m pip install pyserial")
            return
        if self.ser and self.ser.is_open:
            self.running = False
            try: self.ser.close()
            except Exception: pass
            self.ser = None
            self.connect_btn.config(text="Connect")
            self.status_var.set("Disconnected")
            return
        port = self.port_var.get().strip()
        if not port:
            messagebox.showwarning("Select port", "Choose the Arduino COM port first.")
            return
        try:
            self.ser = serial.Serial(port, BAUD, timeout=1)
            time.sleep(2)  # Arduino resets when serial opens
            self.running = True
            self.reader_thread = threading.Thread(target=self.reader, daemon=True)
            self.reader_thread.start()
            self.connect_btn.config(text="Disconnect")
            self.status_var.set(f"Connected: {port}")
        except Exception as e:
            messagebox.showerror("Serial connection failed", str(e))

    def reader(self):
        while self.running and self.ser:
            try:
                line = self.ser.readline().decode("utf-8", errors="ignore").strip()
                if line.startswith("DATA,"):
                    self.q.put(line)
            except Exception as e:
                self.q.put(("ERROR", str(e)))
                break

    def change_policy(self, _event=None):
        if self.ser and self.ser.is_open:
            code = "2" if self.policy_var.get().startswith("P2") else "3"
            try:
                self.ser.write(code.encode("ascii"))
            except Exception as e:
                self.status_var.set(f"Policy send failed: {e}")

    def process_queue(self):
        try:
            while True:
                item = self.q.get_nowait()
                if isinstance(item, tuple) and item[0] == "ERROR":
                    self.status_var.set(item[1])
                    continue
                self.update_data(item)
        except queue.Empty:
            pass
        self.root.after(100, self.process_queue)

    def update_data(self, line):
        try:
            p = next(csv.reader([line]))
            # DATA,step,policy,clock,arrivals,served,stockouts,ready,tariff,temp,humidity,soc1..soc4,mask
            if len(p) != 16:
                return
            d = {
                "step": int(p[1]), "policy": int(p[2]), "clock": p[3],
                "arrivals": int(p[4]), "served": int(p[5]), "stockouts": int(p[6]),
                "ready": int(p[7]), "tariff": float(p[8]), "temp": int(p[9]),
                "humidity": int(p[10]), "soc": [int(x) for x in p[11:15]],
                "mask": int(p[15])
            }
            self.last_data = d
            self.rows.append(d.copy())
            self.metric_vars["clock"].set(d["clock"])
            self.metric_vars["step"].set(str(d["step"]))
            self.metric_vars["arrivals"].set(str(d["arrivals"]))
            self.metric_vars["served"].set(str(d["served"]))
            self.metric_vars["stockouts"].set(str(d["stockouts"]))
            self.metric_vars["ready"].set(str(d["ready"]))
            self.metric_vars["tariff"].set(f"₹{d['tariff']:.1f}/kWh")
            self.metric_vars["temp"].set("--" if d["temp"] < 0 else f"{d['temp']} °C")
            self.metric_vars["humidity"].set("--" if d["humidity"] < 0 else f"{d['humidity']} %")
            active = [str(i+1) for i in range(4) if d["mask"] & (1 << i)]
            self.metric_vars["active"].set(", ".join(active) if active else "None")
            self.policy_var.set("P2 – Mean" if d["policy"] == 2 else "P3 – Chance")

            for i, soc in enumerate(d["soc"]):
                state, soc_var, action, bar, label = self.cards[i]
                is_charging = bool(d["mask"] & (1 << i))
                if is_charging:
                    state.set("CHARGING")
                    action.set("Relay: ON")
                elif soc >= 95:
                    state.set("FULLY CHARGED")
                    action.set("Relay: OFF")
                else:
                    state.set("WAITING")
                    action.set("Relay: OFF")
                soc_var.set(f"Simulated SoC: {soc}%")
                bar["value"] = max(0, min(100, soc))
            self.status_var.set("Receiving live Arduino data")
        except Exception:
            # Ignore malformed/incomplete serial lines; next DATA row will refresh.
            return

    def save_csv(self):
        if not self.rows:
            messagebox.showinfo("No data", "No dashboard data has been received yet.")
            return
        from tkinter import filedialog
        path = filedialog.asksaveasfilename(defaultextension=".csv",
                    filetypes=[("CSV files", "*.csv")], initialfile="swapsched_live_log.csv")
        if not path:
            return
        fields = ["step","policy","clock","arrivals","served","stockouts","ready","tariff","temp","humidity","soc","mask"]
        with open(path, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            for row in self.rows:
                out = row.copy()
                out["soc"] = "|".join(map(str, out["soc"]))
                writer.writerow(out)
        messagebox.showinfo("Saved", f"Saved {len(self.rows)} rows.")

    def close(self):
        self.running = False
        try:
            if self.ser and self.ser.is_open:
                self.ser.close()
        except Exception:
            pass
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = Dashboard(root)
    root.mainloop()
