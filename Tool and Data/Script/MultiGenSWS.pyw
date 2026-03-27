import struct
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from pathlib import Path
import can
import re

START_ADDR = 0x08003800
TOTAL_SIZE = 1024

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
    ("SMon_P_10sCycles","I",2000),
    ("SMon_P_RetryCntS2bTestOn","I",564000),
    ("SMon_P_DischargeTimeCycles","I",132000),
    ("SMon_P_NTC_PullUp_ResistorVale","I",91000),
    ("SMon_P_LongDischargeTimeCycles","I",564000),
    ("SMon_P_LowDisTimeCyc","I",229600),
    ("SMon_P_ClsFailureWaitTime","I",190),
    ("SMon_P_LowVoltage","I",800),
    ("SMon_P_UV_KL30","I",9750),
    ("SMon_P_OV_KL30","I",15500),
    ("SMon_P_UV_CLS","I",700),
    ("SMon_P_Varef","I",3300),
    ("SMon_P_ADC_MaxValue","I",4095),
    ("SMon_P_BetaConst","I",3950),
    ("SMon_P_NTC_L1_TwoPointCalibration_ParamB","I",4984),
    ("SMon_P_StatusVoltageL1Filter","I",250),
    ("SMon_P_I2TDebounceTime","I",20),
    ("SMon_P_Rtcntmax","I",4),
    ("SMon_P_CLSTime","I",22),
    ("SMon_P_WaitTimeOVUV","I",20),
    ("SMon_P_WaitTimeCPC","I",10),
    ("SMon_P_I2TDecrementPercentFactor","I",85),
    ("SMon_P_NTC_L1_TwoPointCalibration_ParamA","I",12332),

    ("SMon_P_VFB_T30_TwoPointCalibration_ParamA","f",8.28363),
    ("SMon_P_VFB_T30_TwoPointCalibration_ParamB","f",21.93),
    ("SMon_P_VFB_L1_TwoPointCalibration_ParamA","f",8.28363),
    ("SMon_P_VFB_L1_TwoPointCalibration_ParamB","f",8.28363),
    ("SMon_P_ISenseNominal","f",20.0),
    ("SMon_P_I2TRating","f",2000.0),
    ("SMon_P_RoomTempKelvin","f",297.15),
    ("SMon_P_VoltsAt25","f",1430.0),
    ("SMon_P_AvgSlope","f",4.30),
    ("SMon_P_RoomTemperature","f",25.0),
    ("SMon_P_Kelvin","f",273.15),
    ("SMon_P_VoltageDivider","f",10.10),
    ("SMon_P_AlphaFilter","f",0.9999),
    ("SMon_P_AlphaFilterExtChIsense","f",0.005),
    ("SMon_P_TwoPointCalib_ConvFacISense","f",15.91),
    ("SMon_P_TwoPointCalib_NoLoad_ISense","f",613.2),
    ("SMon_P_ExternalChargerThreshold","f",-1.0),
    ("SMon_P_NTCTemperatureMax","f",70.0),
    ("SMon_P_NTCTemperatureRelease","f",60.0),
]

# ===================== HEX =====================
def checksum(rec):
    return ((~(sum(rec)&0xFF)+1)&0xFF)

def make_record(addr, rectype, data):
    rec=[len(data),(addr>>8)&0xFF,addr&0xFF,rectype]+list(data)
    return ":"+"".join(f"{b:02X}" for b in rec)+f"{checksum(rec):02X}"

def generate_hex(values, out_path):
    buf=bytearray([0xFF]*TOTAL_SIZE)
    idx=0

    for (_,fmt,_),v in zip(params_def,values):
        buf[idx:idx+4]=struct.pack("<"+fmt,float(v) if fmt=="f" else int(v))
        idx+=4

    buf[254]=0x1E
    buf[255]=0xAA

    lines=[":020000040800F2"]
    addr=START_ADDR & 0xFFFF

    for i in range(0,TOTAL_SIZE,32):
        lines.append(make_record(addr,0x00,buf[i:i+32]))
        addr+=32

    lines.append(":00000001FF")

    with open(out_path,"w") as f:
        f.write("\n".join(lines))

obd_lines = []

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
        l1_raw  = data[1]
        i_raw   = data[2]
        ntc_raw = data[3]

        # reverse scaling from DCM
        t30 = (t30_raw * 32000) / 255
        l1  = (l1_raw  * 32000) / 255
        i   = (i_raw   * 50000) / 255
        ntc = (ntc_raw * 145.0) / 255

        return f"T30={t30:.0f}mV, L1={l1:.0f}mV, I={i:.0f}mA, NTC={ntc:.1f}C"

    return data.hex()

def decode_mode02(pid, frame, data):

    if pid != 0xFE:
        return data.hex()

    # FIX: first byte is frame index, not FF index
    if len(data) < 1:
        return "NO_FF_DATA"

    frame_id = data[0]
    ff = data[1:]

    if len(ff) < 8:
        return "NO_FF_DATA"

    load = ff[0]
    ect  = ff[1]
    iat  = ff[2]

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
        val = data[i+3]

        name = MODE06_NAMES.get(tid, f"TID_{tid:02X}")

        status = "OK" if val == 1 else "NOT_OK"

        results.append(f"{name}={status}")

    return ", ".join(results)
# ===================== DIAG =====================
def run_diag_export(blf_dir, out_path):

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
    obd_lines = []   # FIX: local, not global

    for blf in Path(blf_dir).glob("*.blf"):

        trace = blf.name
        reader = can.BLFReader(blf)

        buf = bytearray()
        exp_len = 0
        did_map = {}

        for m in reader:

            if m.arbitration_id != RESP_ID:
                continue

            d = m.data
            pci = d[0] >> 4

            payload = None   # FIX: always initialize

            # ---------- ISO-TP ----------
            if pci == 0x0:
                payload = d[1:1 + (d[0] & 0x0F)]

            elif pci == 0x1:
                buf.clear()
                exp_len = ((d[0] & 0x0F) << 8) | d[1]

                valid_len = m.dlc - 2   # FIX
                buf.extend(d[2:2 + valid_len])

            elif pci == 0x2:
                valid_len = m.dlc - 1   # FIX: use actual CAN length
                buf.extend(d[1:1 + valid_len])

                if len(buf) < exp_len:
                    continue

                payload = buf[:exp_len]
                buf.clear()

            else:
                continue

            # ---------- SAFE GUARD ----------
            if payload is None or len(payload) < 2:
                continue

            sid = payload[0]

            # ---------- OBD ----------
            if sid in (0x41, 0x42, 0x46, 0x49):

                if sid == 0x41:
                    pid = payload[1]
                    data = payload[2:]

                    decoded = decode_mode01(pid, data)

                    obd_lines.append((trace, "MODE01", f"PID {pid:02X}", decoded))

                elif sid == 0x42:

                    pid = payload[1]
                    frame = payload[2]
                    data = payload[2:]   # include frame in data

                    decoded = decode_mode02(pid, frame, data)

                    obd_lines.append((trace, "MODE02", f"PID {pid:02X} FRAME {frame}", decoded))

                elif sid == 0x46 and len(payload) >= 3:
                    mid = payload[1]
                    data = payload[2:]

                    decoded = decode_mode06(mid, data)

                    obd_lines.append((
                        trace,
                        "MODE06",
                        f"MID {mid:02X}",
                        decoded
                    ))

                elif sid == 0x49:
                    pid = payload[1]
                    data = payload[2:]

                    try:
                        txt = bytes(data).decode("ascii", errors="ignore").strip()
                    except:
                        txt = data.hex()

                    if pid == 0x02:
                        obd_lines.append((trace, "MODE09", "VIN", txt))
                    elif pid == 0x04:
                        obd_lines.append((trace, "MODE09", "CALID", txt))
                    elif pid == 0x0A:
                        obd_lines.append((trace, "MODE09", "ECU_NAME", txt))
                    else:
                        obd_lines.append((trace, "MODE09", f"PID {pid:02X}", txt))

            # ---------- DTC ----------
            if sid == 0x59 and payload[1] == 0x04:

                i = 2
                while i + RECORD_SIZE <= len(payload):

                    dtc = (payload[i] << 16) | (payload[i+1] << 8) | payload[i+2]
                    ff = payload[i+3:i+3+FF_SIZE]

                    occ,y,m,d_,h,mi,s,vehStatus,l1,t30,ntc,isense = FF_UNPACK(ff)

                    date = f"{2000+y:04d}-{m:02d}-{d_:02d} {h:02d}:{mi:02d}:{s:02d}"
                    veh_text = VEH_STATUS_MAP.get(vehStatus, "NOT_VALID")

                    dtc_lines.append((
                        trace, f"{dtc:06X}", occ, date, veh_text, l1, t30, ntc, isense
                    ))

                    i += RECORD_SIZE

            # ---------- DID ----------
            elif sid == 0x62:

                did = payload[2]
                data = payload[3:]

                if did in (0x01,0x02,0x03,0x04):
                    val = int.from_bytes(data[:4], "little")

                elif did in (0x08,0x09,0x0A):
                    val = int.from_bytes(data[:2], "little")

                elif did == 0x05:
                    val = f"Reason={data[0]} Info={data[1]}"

                elif did in (0x06,0x07):
                    val = data[0]

                elif did == 0x80:
                    val = f"{data[0]}.{data[1]}.{data[2]}.{data[3]}"

                elif 0x0B <= did <= 0x1C:
                    val = FLOAT_UNPACK(data, 0)[0]

                else:
                    val = data.hex()

                did_map[did] = val

            # ---------- ROUTINE ----------
            elif sid == 0x71 and len(payload) >= 5:

                rid = (payload[2] << 8) | payload[3]

                if rid == 0x43 and len(payload) >= 4 + 18*4:
                    values = ROUTINE_MINMAXAVG_UNPACK(payload, 4)
                    routine_lines.append((trace, values))

        for k,v in did_map.items():
            did_lines.append((trace, hex(k), v))

    # ---------- WRITE ----------
    with open(out_path, "w", encoding="utf-8") as f:

        traces = sorted(set(x[0] for x in dtc_lines + did_lines + routine_lines + obd_lines))

        for trace in traces:

            f.write("="*110 + "\n")
            f.write(f"TRACE: {trace}\n")
            f.write("="*110 + "\n\n")

            # ----- DTC -----
            f.write("[DTCs]\n" + "-"*110 + "\n")
            for t,dtc,occ,date,veh,l1,t30,ntc,isense in filter(lambda x: x[0]==trace, dtc_lines):
                f.write(f"{dtc:<6} {occ:<4} {date:<19} {veh:<23}{l1:<8}{t30:<8}{ntc:>7.2f}{isense:>9.2f}\n")

            # ----- DID -----
            f.write("\n[DIDs]\n" + "-"*80 + "\n")
            for t,did,val in filter(lambda x: x[0]==trace, did_lines):
                did_int = int(did,16)
                name = DID_NAME_MAP.get(did_int,"UNKNOWN")
                f.write(f"{did:<6} {name:<30} {val}\n")

            # ----- OBD -----
            f.write("\n[OBD]\n" + "-"*80 + "\n")
            for t,typ,ident,val in filter(lambda x: x[0]==trace, obd_lines):
                f.write(f"{typ:<8} {ident:<16} {val}\n")

            # ----- ROUTINE -----
            f.write("\n[MIN/MAX/AVG]\n" + "-"*80 + "\n")
            for t,values in filter(lambda x: x[0]==trace, routine_lines):
                for name,val in zip(MINMAX_LABELS, values):
                    f.write(f"{name:<25}{val:>10.3f}\n")

            f.write("\n\n")

# ===================== MAP PARSER =====================
def parse_map_file(map_path):
    symbols = {}
    with open(map_path, "r", errors="ignore") as f:
        lines = f.readlines()

    for i in range(len(lines)-1):
        if ".bss." in lines[i] or ".data." in lines[i]:
            parts = lines[i].strip().split(".")
            if len(parts) >= 3:
                name = parts[-1]

                m = re.search(r"0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)", lines[i+1])
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

    # ---- UPDATE EXISTING ----
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

    # ---- FIND INSERT POINT ----
    insert_idx = None
    for i, line in enumerate(lines):
        if "/end MODULE" in line:
            insert_idx = i
            break

    if insert_idx is None:
        raise Exception("Invalid A2L: no /end MODULE found")

    # ---- BUILD NEW ENTRIES CLEAN ----
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

    # ---- INSERT ----
    lines[insert_idx:insert_idx] = new_entries

    # ---- WRITE ----
    with open(a2l_out, "w") as f:
        f.writelines(lines)

# ===================== GUI =====================
root=tk.Tk()
root.title("Tool")

nb=ttk.Notebook(root)
t1=ttk.Frame(nb);t2=ttk.Frame(nb);t3=ttk.Frame(nb)
nb.add(t1,text="HEX");nb.add(t2,text="Diag");nb.add(t3,text="A2L")
nb.pack(fill="both",expand=True)

# HEX
entries=[]
for i,(n,_,d) in enumerate(params_def):
    ttk.Label(t1,text=n).grid(row=i,column=0)
    e=ttk.Entry(t1);e.insert(0,str(d));e.grid(row=i,column=1)
    entries.append(e)

ttk.Button(t1,text="HEX",command=lambda:generate_hex([e.get() for e in entries],filedialog.asksaveasfilename())).grid(row=len(entries),column=0)

# DIAG
e_blf=ttk.Entry(t2,width=60);e_blf.pack()
ttk.Button(t2,text="BLF",command=lambda:e_blf.insert(0,filedialog.askdirectory())).pack()
ttk.Button(t2,text="Run",command=lambda:run_diag_export(e_blf.get(),filedialog.asksaveasfilename())).pack()

# A2L
e_map=ttk.Entry(t3,width=60);e_map.pack()
ttk.Button(t3,text="MAP",command=lambda:(e_map.delete(0,tk.END),e_map.insert(0,filedialog.askopenfilename()))).pack()

e_a2l=ttk.Entry(t3,width=60);e_a2l.pack()
ttk.Button(t3,text="A2L",command=lambda:(e_a2l.delete(0,tk.END),e_a2l.insert(0,filedialog.askopenfilename()))).pack()

def run_a2l():
    symbols=parse_map_file(e_map.get())
    win=tk.Toplevel(root)
    lb=tk.Listbox(win,selectmode=tk.MULTIPLE,width=60,height=20)
    lb.pack()

    names=sorted(symbols.keys())
    for n in names: lb.insert(tk.END,n)

    def go():
        sel=[names[i] for i in lb.curselection()]
        update_and_extend_a2l(e_map.get(),e_a2l.get(),filedialog.asksaveasfilename(),sel)
        win.destroy()

    ttk.Button(win,text="Apply",command=go).pack()

ttk.Button(t3,text="Run A2L",command=run_a2l).pack()

root.mainloop()