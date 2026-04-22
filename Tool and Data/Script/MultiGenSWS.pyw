import struct
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from pathlib import Path
import can
import re

START_ADDR = 0x08003800
TOTAL_SIZE = 292

MINMAX_LABELS = [
    "T30_Min_10s",
    "T30_Max_10s",
    "T30_Avg_10s",
    "L1_Min_10s",
    "L1_Max_10s",
    "L1_Avg_10s",
    "L1I_Min_10s",
    "L1I_Max_10s",
    "L1I_Avg_10s",
    "T30_Min_SW",
    "T30_Max_SW",
    "T30_Avg_SW",
    "L1_Min_SW",
    "L1_Max_SW",
    "L1_Avg_SW",
    "L1I_Min_SW",
    "L1I_Max_SW",
    "L1I_Avg_SW",
]

DID_NAME_MAP = {
    0x01: "ResetCounter",
    0x02: "TimeInSleep",
    0x03: "TimeActive",
    0x04: "TimeWithoutReset",
    0x05: "ResetData",
    0x06: "CpuLoad",
    0x07: "SecureAccess",
    0x08: "WakeupLine_Wakeups",
    0x09: "CAN_Wakeups",
    0x0A: "DiagWakeups",

    0x0B: "T30_Min_10s",
    0x0C: "T30_Max_10s",
    0x0D: "T30_Avg_10s",

    0x0E: "L1_Min_10s",
    0x0F: "L1_Max_10s",
    0x10: "L1_Avg_10s",

    0x11: "L1I_Min_10s",
    0x12: "L1I_Max_10s",
    0x13: "L1I_Avg_10s",

    0x14: "T30_Min_SW",
    0x15: "T30_Max_SW",
    0x16: "T30_Avg_SW",

    0x17: "L1_Min_SW",
    0x18: "L1_Max_SW",
    0x19: "L1_Avg_SW",

    0x1A: "L1I_Min_SW",
    0x1B: "L1I_Max_SW",
    0x1C: "L1I_Avg_SW",

    0x1D: "LoadStatus",

    0x7E: "ActiveDiagnosticSession",
    0x7F: "ActiveSoftwareBlock",

    0x80: "SoftwareVersion",
}

# ===================== PARAMS =====================
params_def = [
    ("SMon_P_10sCycles", "I", 2000),
    ("SMon_P_RetryCntS2bTestOn", "I", 564000),
    ("SMon_P_DischargeTimeCycles", "I", 132000),
    ("SMon_P_NTC_PullUp_ResistorVale", "I", 91000),
    ("SMon_P_LongDischargeTimeCycles", "I", 564000),
    ("SMon_P_LowDisTimeCyc", "I", 229600),
    ("SMon_P_ClsFailureWaitTime", "I", 190),
    ("SMon_P_LowVoltage", "I", 1200),
    ("SMon_P_UV_KL30", "I", 6900),
    ("SMon_P_OV_KL30", "I", 17200),
    ("SMon_P_UV_CLS", "I", 700),
    ("SMon_P_Varef", "I", 3300),
    ("SMon_P_ADC_MaxValue", "I", 4095),
    ("SMon_P_BetaConst", "I", 3950),
    ("SMon_P_NTC_L1_TwoPointCalibration_ParamB", "I", 4984),
    ("SMon_P_StatusVoltageL1Filter", "I", 250),
    ("SMon_P_I2TDebounceTime", "I", 20),
    ("SMon_P_Rtcntmax", "I", 13),
    ("SMon_P_CLSTime", "I", 22),
    ("SMon_P_WaitTimeOVUV", "I", 80),
    ("SMon_P_WaitTimeCPC", "I", 10),
    ("SMon_P_I2TDecrementPercentFactor", "I", 85),
    ("SMon_P_NTC_L1_TwoPointCalibration_ParamA", "I", 12332),
    ("SMon_P_BattRestTimeTicks", "I", 200),
    ("SMon_P_VFB_T30_TwoPointCalibration_ParamA", "f", 8.16),
    ("SMon_P_VFB_T30_TwoPointCalibration_ParamB", "f", 21.93),
    ("SMon_P_VFB_L1_TwoPointCalibration_ParamA", "f", 8.13),
    ("SMon_P_VFB_L1_TwoPointCalibration_ParamB", "f", 8.13),
    ("SMon_P_ISenseNominal", "f", 28.000),
    ("SMon_P_I2TRating", "f", 1176.0),
    ("SMon_P_RoomTempKelvin", "f", 297.15),
    ("SMon_P_VoltsAt25", "f", 1430.0),
    ("SMon_P_AvgSlope", "f", 4.30),
    ("SMon_P_RoomTemperature", "f", 25.0),
    ("SMon_P_Kelvin", "f", 273.15),
    ("SMon_P_VoltageDivider", "f", 10.10),
    ("SMon_P_AlphaFilter", "f", 0.90),
    ("SMon_P_AlphaFilterExtChIsense", "f", 0.0001),
    ("SMon_P_TwoPointCalib_ConvFacISense", "f", 15.91),
    ("SMon_P_TwoPointCalib_NoLoad_ISense", "f", 613.2),
    ("SMon_P_ExternalChargerThreshold", "f", -1.0),
    ("SMon_P_NTCTemperatureMax", "f", 90.0),
    ("SMon_P_NTCTemperatureRelease", "f", 80.0),
    ("SMon_P_BattNominalCapacity_Ah", "f", 77.0),
    ("SMon_P_BattInitialSoC_pct", "f", 1.0),
    ("SMon_P_BattMinSoC_pct", "f", 1.0),
    ("SMon_P_BattMaxSoC_pct", "f", 100.0),
    ("SMon_P_BattRestCurrent_A", "f", 0.100),
    ("SMon_P_BattRestVoltDelta_V", "f", 0.02),
    ("SMon_P_BattWeakVolt_V", "f", 11.80),
    ("SMon_P_BattDeepDischargeVolt_V", "f", 11.10),
    ("SMon_P_BattCrankVolt_V", "f", 9.60),
    ("SMon_P_BattAlphaVolt", "f", 0.02),
    ("SMon_P_BattAlphaCurr", "f", 0.05),
    ("SMon_P_BattAlphaRint", "f", 0.10),
    ("SMon_P_BattChargeEfficiency", "f", 0.90),
    ("SMon_P_BattOcvCorrectionGain", "f", 0.20),
    ("SMon_P_BattSoHMin_pct", "f", 1.0),
    ("SMon_P_BattSoHMax_pct", "f", 100.0),
    ("SMon_P_BattNominalRint_Ohm", "f", 0.015),
    ("SMon_P_BattBadRint_Ohm", "f", 0.060),
    ("SMon_P_BattCurrentAlpha", "f", 0.0001),
    ("SMon_P_BattCurrentDeadband_A", "f", 1.0),
    ("SMon_P_ISenseZeroOffset_A", "f", 0.3),
    ("SMon_P_BattCurrentHys_A", "f", 0.3),
    ("SMon_P_BattChargePathDeltaOn_V", "f", 0.25),
    ("SMon_P_BattChargePathDeltaOff_V", "f", 0.10),
    ("SMon_P_BattRintMinStep_A", "f", 0.20),
    ("SMon_P_BattAvgCurrentAlpha", "f", 0.05),
    ("SMon_P_BattHybridBlendLowLoad", "f", 0.15),
    ("SMon_P_BattChargeVoltageCompGain", "f", 0.35),
    ("SMon_P_BattRestDetectCurrent_A", "f", 0.30),
]

# ===================== HEX =====================
def checksum(rec):
    return ((~(sum(rec) & 0xFF) + 1) & 0xFF)

def make_record(addr, rectype, data):
    rec = [len(data), (addr >> 8) & 0xFF, addr & 0xFF, rectype] + list(data)
    return ":" + "".join(f"{b:02X}" for b in rec) + f"{checksum(rec):02X}"

def generate_hex(values, out_path):
    if not out_path:
        return

    expected_payload_bytes = len(params_def) * 4
    if expected_payload_bytes > TOTAL_SIZE:
        messagebox.showerror(
            "HEX Error",
            f"Parameter payload is too large: {expected_payload_bytes} bytes. "
            f"Only {TOTAL_SIZE} bytes are available."
        )
        return

    buf = bytearray([0xFF] * TOTAL_SIZE)
    idx = 0

    try:
        for (_, fmt, _), v in zip(params_def, values):
            if fmt == "f":
                packed = struct.pack("<f", float(v))
            else:
                packed = struct.pack("<I", int(v, 0) if isinstance(v, str) else int(v))
            buf[idx:idx + 4] = packed
            idx += 4
    except ValueError as e:
        messagebox.showerror("HEX Error", f"Invalid parameter value.\n{e}")
        return

    buf[290] = 0x21
    buf[291] = 0xAA

    lines = [":020000040800F2"]
    addr = START_ADDR & 0xFFFF

    for i in range(0, TOTAL_SIZE, 32):
        lines.append(make_record(addr, 0x00, buf[i:i + 32]))
        addr += 32

    lines.append(":00000001FF")

    try:
        with open(out_path, "w", encoding="utf-8") as f:
            f.write("\n".join(lines))
        messagebox.showinfo("HEX", "HEX file generated successfully.")
    except Exception as e:
        messagebox.showerror("HEX Error", str(e))

MODE06_NAMES = {
    0x01: "UV_KL30",
    0x02: "OV_KL30",
    0x03: "SHORT_TO_BATT",
    0x04: "OVERCURRENT",
    0x05: "LOCKED",
    0x06: "CLS_FAILURE",
    0x07: "UV_L1",
    0x08: "NTC_ERROR",
}

def decode_mode01(pid, data):
    if pid == 0xA7 and len(data) >= 4:
        t30_raw = data[0]
        l1_raw = data[1]
        i_raw = data[2]
        ntc_raw = data[3]

        t30 = (t30_raw * 32000) / 255
        l1 = (l1_raw * 32000) / 255
        i = (i_raw * 50000) / 255
        ntc = (ntc_raw * 145.0) / 255

        return f"T30={t30:.0f}mV, L1={l1:.0f}mV, I={i:.0f}mA, NTC={ntc:.1f}C"

    return data.hex(" ").upper()

def decode_mode02(pid, frame, data):
    if pid != 0xFE:
        return data.hex(" ").upper()

    if len(data) < 1:
        return "NO_FF_DATA"

    frame_id = data[0]
    ff = data[1:]

    if len(ff) < 8:
        return "NO_FF_DATA"

    load = ff[0]
    ect = ff[1]
    iat = ff[2]
    rpm = ((ff[3] << 8) | ff[4]) / 4.0
    speed = ff[5]
    amb = ff[6]
    rdy = ff[7]

    return (
        f"[FRAME {frame_id}] "
        f"LOAD={load}% ECT={ect}C IAT={iat}C "
        f"RPM={rpm:.0f} SPEED={speed}km/h AMB={amb}C RDY={rdy}"
    )

def decode_mode06(mid, data):
    results = []

    for i in range(0, len(data), 4):
        if i + 4 > len(data):
            break

        tid = data[i]
        val = data[i + 3]

        name = MODE06_NAMES.get(tid, f"TID_{tid:02X}")
        status = "OK" if val == 1 else "NOT_OK"
        results.append(f"{name}={status}")

    return ", ".join(results) if results else data.hex(" ").upper()

def decode_mode09(pid, data):
    if pid == 0x02:
        txt = bytes(data).decode("ascii", errors="ignore").strip("\x00 ").strip()
        return txt if txt else data.hex(" ").upper()

    if pid == 0x04:
        return data.hex(" ").upper()

    if pid == 0x0A:
        txt = bytes(data).decode("ascii", errors="ignore").strip("\x00 ").strip()
        return txt if txt else data.hex(" ").upper()

    txt = bytes(data).decode("ascii", errors="ignore").strip("\x00 ").strip()
    if txt:
        return txt
    return data.hex(" ").upper()

# ===================== DIAG =====================
def run_diag_export(blf_dir, out_path):
    if not blf_dir or not out_path:
        return

    RESP_ID = 0x703

    FF_SIZE = 24
    RECORD_SIZE = 3 + FF_SIZE

    FF_UNPACK = struct.Struct("<H6BBHHff3x").unpack_from
    FLOAT_UNPACK = struct.Struct("<f").unpack_from
    ROUTINE_MINMAXAVG_UNPACK = struct.Struct("<18f").unpack_from

    VEH_STATUS_MAP = {
        1: "Park_Ignition_OFF",
        2: "Park_Ignition_ON",
        3: "Standing",
        4: "Driving",
        5: "Vehicle Error",
        6: "Vehicle Diagnosis",
        15: "Signal invalid"
    }

    dtc_lines = []
    did_lines = []
    routine_lines = []
    obd_lines = []

    try:
        blf_files = list(Path(blf_dir).glob("*.blf"))
    except Exception as e:
        messagebox.showerror("Diag Error", str(e))
        return

    if not blf_files:
        messagebox.showerror("Diag Error", "No .blf files found in selected folder.")
        return

    for blf in blf_files:
        trace = blf.name
        reader = can.BLFReader(str(blf))

        buf = bytearray()
        exp_len = 0
        did_map = {}

        for m in reader:
            if m.arbitration_id != RESP_ID:
                continue

            d = bytes(m.data)
            if not d:
                continue

            pci = d[0] >> 4
            payload = None

            if pci == 0x0:
                sf_len = d[0] & 0x0F
                payload = d[1:1 + sf_len]

            elif pci == 0x1:
                buf.clear()
                exp_len = ((d[0] & 0x0F) << 8) | d[1]
                valid_len = max(0, min(len(d) - 2, exp_len))
                buf.extend(d[2:2 + valid_len])

                if len(buf) >= exp_len:
                    payload = bytes(buf[:exp_len])
                    buf.clear()

            elif pci == 0x2:
                valid_len = max(0, len(d) - 1)
                buf.extend(d[1:1 + valid_len])

                if len(buf) < exp_len:
                    continue

                payload = bytes(buf[:exp_len])
                buf.clear()

            else:
                continue

            if payload is None or len(payload) < 2:
                continue

            sid = payload[0]

            if sid in (0x41, 0x42, 0x46, 0x49):
                if sid == 0x41:
                    pid = payload[1]
                    data = payload[2:]
                    decoded = decode_mode01(pid, data)
                    obd_lines.append((trace, "MODE01", f"PID {pid:02X}", decoded))

                elif sid == 0x42 and len(payload) >= 3:
                    pid = payload[1]
                    frame = payload[2]
                    data = payload[2:]
                    decoded = decode_mode02(pid, frame, data)
                    obd_lines.append((trace, "MODE02", f"PID {pid:02X} FRAME {frame}", decoded))

                elif sid == 0x46 and len(payload) >= 3:
                    mid = payload[1]
                    data = payload[2:]
                    decoded = decode_mode06(mid, data)
                    obd_lines.append((trace, "MODE06", f"MID {mid:02X}", decoded))

                elif sid == 0x49 and len(payload) >= 2:
                    pid = payload[1]
                    data = payload[2:]
                    decoded = decode_mode09(pid, data)

                    if pid == 0x02:
                        obd_lines.append((trace, "MODE09", "VIN", decoded))
                    elif pid == 0x04:
                        obd_lines.append((trace, "MODE09", "CALID", decoded))
                    elif pid == 0x0A:
                        obd_lines.append((trace, "MODE09", "ECU_NAME", decoded))
                    else:
                        obd_lines.append((trace, "MODE09", f"PID {pid:02X}", decoded))

            if sid == 0x59 and len(payload) >= 2 and payload[1] == 0x04:
                i = 2
                while i + RECORD_SIZE <= len(payload):
                    dtc = (payload[i] << 16) | (payload[i + 1] << 8) | payload[i + 2]
                    ff = payload[i + 3:i + 3 + FF_SIZE]

                    occ, y, mth, d_, h, mi, s, vehStatus, l1, t30, ntc, isense = FF_UNPACK(ff)
                    date = f"{2000 + y:04d}-{mth:02d}-{d_:02d} {h:02d}:{mi:02d}:{s:02d}"
                    veh_text = VEH_STATUS_MAP.get(vehStatus, "NOT_VALID")

                    dtc_lines.append((
                        trace, f"{dtc:06X}", occ, date, veh_text, l1, t30, ntc, isense
                    ))
                    i += RECORD_SIZE

            elif sid == 0x62 and len(payload) >= 3:
                did = payload[2]
                data = payload[3:]

                if did in (0x01, 0x02, 0x03, 0x04) and len(data) >= 4:
                    val = int.from_bytes(data[:4], "little")

                elif did in (0x08, 0x09, 0x0A) and len(data) >= 2:
                    val = int.from_bytes(data[:2], "little")

                elif did == 0x05 and len(data) >= 2:
                    val = f"Reason={data[0]} Info={data[1]}"

                elif did in (0x06, 0x07) and len(data) >= 1:
                    val = data[0]

                elif did == 0x80 and len(data) >= 4:
                    val = f"{data[0]}.{data[1]}.{data[2]}.{data[3]}"

                elif 0x0B <= did <= 0x1C and len(data) >= 4:
                    val = FLOAT_UNPACK(data, 0)[0]

                else:
                    val = data.hex(" ").upper()

                did_map[did] = val

            elif sid == 0x71 and len(payload) >= 5:
                rid = (payload[2] << 8) | payload[3]

                if rid == 0x43 and len(payload) >= 4 + 18 * 4:
                    values = ROUTINE_MINMAXAVG_UNPACK(payload, 4)
                    routine_lines.append((trace, values))

        for k, v in did_map.items():
            did_lines.append((trace, hex(k), v))

    try:
        with open(out_path, "w", encoding="utf-8") as f:
            traces = sorted(set(x[0] for x in dtc_lines + did_lines + routine_lines + obd_lines))

            for trace in traces:
                f.write("=" * 110 + "\n")
                f.write(f"TRACE: {trace}\n")
                f.write("=" * 110 + "\n\n")

                f.write("[DTCs]\n" + "-" * 110 + "\n")
                for t, dtc, occ, date, veh, l1, t30, ntc, isense in filter(lambda x: x[0] == trace, dtc_lines):
                    f.write(f"{dtc:<6} {occ:<4} {date:<19} {veh:<23}{l1:<8}{t30:<8}{ntc:>7.2f}{isense:>9.2f}\n")

                f.write("\n[DIDs]\n" + "-" * 80 + "\n")
                for t, did, val in filter(lambda x: x[0] == trace, did_lines):
                    did_int = int(did, 16)
                    name = DID_NAME_MAP.get(did_int, "UNKNOWN")
                    f.write(f"{did:<6} {name:<30} {val}\n")

                f.write("\n[OBD]\n" + "-" * 80 + "\n")
                for t, typ, ident, val in filter(lambda x: x[0] == trace, obd_lines):
                    f.write(f"{typ:<8} {ident:<16} {val}\n")

                f.write("\n[MIN/MAX/AVG]\n" + "-" * 80 + "\n")
                for t, values in filter(lambda x: x[0] == trace, routine_lines):
                    for name, val in zip(MINMAX_LABELS, values):
                        f.write(f"{name:<25}{val:>10.3f}\n")

                f.write("\n\n")

        messagebox.showinfo("Diag", "Diagnostic export completed successfully.")
    except Exception as e:
        messagebox.showerror("Diag Error", str(e))

# ===================== MAP PARSER =====================
def parse_map_file(map_path):
    symbols = {}
    with open(map_path, "r", errors="ignore") as f:
        lines = f.readlines()

    for i in range(len(lines) - 1):
        if ".bss." in lines[i] or ".data." in lines[i]:
            parts = lines[i].strip().split(".")
            if len(parts) >= 3:
                name = parts[-1]
                m = re.search(r"0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)", lines[i + 1])
                if m:
                    addr = int(m.group(1), 16)
                    size = int(m.group(2), 16)
                    symbols[name] = (addr, size)

    return symbols

def infer_datatype(size):
    if size == 1:
        return "UBYTE", 1
    elif size == 2:
        return "UWORD", 2
    elif size == 4:
        return "ULONG", 4
    else:
        return "ULONG", 4

# ===================== A2L =====================
def a2l_type(size):
    if size == 1:
        return "__u8"
    elif size == 2:
        return "uint16"
    elif size == 4:
        return "uint32"
    else:
        return "uint32"

def update_and_extend_a2l(map_path, a2l_in, a2l_out, selected):
    symbols = parse_map_file(map_path)

    with open(a2l_in, "r", errors="ignore") as f:
        lines = f.readlines()

    current = None
    idx = 0

    for i, line in enumerate(lines):
        m = re.search(r"/begin (MEASUREMENT|CHARACTERISTIC)\s+(\w+)(\[(\d+)\])?", line)
        if m:
            current = m.group(2)
            idx = int(m.group(4)) if m.group(4) else 0

        if ("ECU_ADDRESS" in line or "VALUE" in line) and current in symbols:
            base, size = symbols[current]
            _, step = infer_datatype(size)
            addr = base + idx * step
            lines[i] = re.sub(r"0x[0-9A-Fa-f]+", f"0x{addr:08X}", line)

    insert_idx = None
    for i, line in enumerate(lines):
        if "/end MODULE" in line:
            insert_idx = i
            break

    if insert_idx is None:
        raise Exception("Invalid A2L: no /end MODULE found")

    new_entries = []
    new_entries.append("  /* ==== ADDED VARIABLES ==== */\n")

    for name in selected:
        if name not in symbols:
            continue

        addr, size = symbols[name]
        dtype = a2l_type(size)
        encoding = "UTF32" if dtype in ["uint32", "float"] else "UTF8"

        new_entries.append(f"  /begin CHARACTERISTIC {name} \"\"\n")
        new_entries.append(f"    VALUE 0x{addr:08X} {dtype} 0 NO_COMPU_METHOD 0 0\n")
        new_entries.append(f"    ENCODING {encoding}\n")
        new_entries.append(f"    NUMBER 0\n")
        new_entries.append(f"  /end CHARACTERISTIC\n")
        new_entries.append("\n")

    lines[insert_idx:insert_idx] = new_entries

    with open(a2l_out, "w", encoding="utf-8") as f:
        f.writelines(lines)

# ===================== GUI =====================
root = tk.Tk()
root.title("Tool")

nb = ttk.Notebook(root)
t1 = ttk.Frame(nb)
t2 = ttk.Frame(nb)
t3 = ttk.Frame(nb)
nb.add(t1, text="HEX")
nb.add(t2, text="Diag")
nb.add(t3, text="A2L")
nb.pack(fill="both", expand=True)

# HEX
hex_container = ttk.Frame(t1)
hex_container.pack(fill="both", expand=True)

hex_canvas = tk.Canvas(hex_container)
hex_scrollbar = ttk.Scrollbar(hex_container, orient="vertical", command=hex_canvas.yview)
hex_scrollable_frame = ttk.Frame(hex_canvas)

hex_scrollable_frame.bind(
    "<Configure>",
    lambda e: hex_canvas.configure(scrollregion=hex_canvas.bbox("all"))
)

hex_canvas.create_window((0, 0), window=hex_scrollable_frame, anchor="nw")
hex_canvas.configure(yscrollcommand=hex_scrollbar.set)

hex_canvas.pack(side="left", fill="both", expand=True)
hex_scrollbar.pack(side="right", fill="y")

entries = []
for i, (n, _, d) in enumerate(params_def):
    ttk.Label(hex_scrollable_frame, text=n).grid(row=i, column=0, sticky="w", padx=4, pady=2)
    e = ttk.Entry(hex_scrollable_frame, width=20)
    e.insert(0, str(d))
    e.grid(row=i, column=1, sticky="w", padx=4, pady=2)
    entries.append(e)

def on_generate_hex():
    out_path = filedialog.asksaveasfilename(
        defaultextension=".hex",
        filetypes=[("HEX files", "*.hex"), ("All files", "*.*")]
    )
    if not out_path:
        return
    generate_hex([e.get() for e in entries], out_path)

ttk.Button(hex_scrollable_frame, text="Generate HEX", command=on_generate_hex).grid(
    row=len(entries), column=0, columnspan=2, pady=10
)

# DIAG
diag_frame = ttk.Frame(t2)
diag_frame.pack(fill="both", expand=True, padx=8, pady=8)

e_blf = ttk.Entry(diag_frame, width=60)
e_blf.pack(fill="x", pady=4)

def choose_blf_dir():
    path = filedialog.askdirectory()
    if path:
        e_blf.delete(0, tk.END)
        e_blf.insert(0, path)

def on_run_diag():
    out_path = filedialog.asksaveasfilename(
        defaultextension=".txt",
        filetypes=[("Text files", "*.txt"), ("All files", "*.*")]
    )
    if not out_path:
        return
    run_diag_export(e_blf.get(), out_path)

ttk.Button(diag_frame, text="BLF Folder", command=choose_blf_dir).pack(pady=4)
ttk.Button(diag_frame, text="Run", command=on_run_diag).pack(pady=4)

# A2L
a2l_frame = ttk.Frame(t3)
a2l_frame.pack(fill="both", expand=True, padx=8, pady=8)

e_map = ttk.Entry(a2l_frame, width=60)
e_map.pack(fill="x", pady=4)

def choose_map():
    path = filedialog.askopenfilename()
    if path:
        e_map.delete(0, tk.END)
        e_map.insert(0, path)

ttk.Button(a2l_frame, text="MAP", command=choose_map).pack(pady=4)

e_a2l = ttk.Entry(a2l_frame, width=60)
e_a2l.pack(fill="x", pady=4)

def choose_a2l():
    path = filedialog.askopenfilename()
    if path:
        e_a2l.delete(0, tk.END)
        e_a2l.insert(0, path)

ttk.Button(a2l_frame, text="A2L", command=choose_a2l).pack(pady=4)

def run_a2l():
    try:
        symbols = parse_map_file(e_map.get())
    except Exception as e:
        messagebox.showerror("A2L Error", str(e))
        return

    win = tk.Toplevel(root)
    win.title("Select symbols")

    lb = tk.Listbox(win, selectmode=tk.MULTIPLE, width=60, height=20)
    lb.pack(fill="both", expand=True, padx=8, pady=8)

    names = sorted(symbols.keys())
    for n in names:
        lb.insert(tk.END, n)

    def go():
        sel = [names[i] for i in lb.curselection()]
        out_path = filedialog.asksaveasfilename(
            defaultextension=".a2l",
            filetypes=[("A2L files", "*.a2l"), ("All files", "*.*")]
        )
        if not out_path:
            return

        try:
            update_and_extend_a2l(e_map.get(), e_a2l.get(), out_path, sel)
            win.destroy()
            messagebox.showinfo("A2L", "A2L updated successfully.")
        except Exception as e:
            messagebox.showerror("A2L Error", str(e))

    ttk.Button(win, text="Apply", command=go).pack(pady=8)

ttk.Button(a2l_frame, text="Run A2L", command=run_a2l).pack(pady=8)

root.mainloop()