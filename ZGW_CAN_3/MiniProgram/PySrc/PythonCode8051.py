# CODE BLOCK BEGIN Global_Definitions 
from TSMaster import *
import can
import struct
from pathlib import Path

BLF_DIR = Path(r"C:\Users\Daniel\Desktop\GitHub Repositories\ZGW_CAN_3\Logging\Bus")
OUT_TXT = r"C:\Users\Daniel\Desktop\GitHub Repositories\Diagnostic Export\diagnostic_export.txt"

RESP_ID = 0x703

# -------- PRECOMPILED STRUCTS --------
FF_UNPACK = struct.Struct("<H6BHHff").unpack_from
ROUTINE41_UNPACK = struct.Struct("<7I").unpack_from

# -------- OUTPUT BUFFERS --------
dtc_lines = []
did_lines = []
routine_lines = []

# -------- ISO-TP PARSER (HOT PATH) --------
def parse_blf(blf_path: Path):
    trace = blf_path.name
    reader = can.BLFReader(blf_path)

    buf = bytearray()
    exp_len = 0

    did_map = {k: 0 for k in (0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x80)}

    for m in reader:
        if m.arbitration_id != RESP_ID:
            continue

        d = m.data
        pci = d[0] >> 4

        if pci == 0x0:  # SF
            payload = d[1:1 + (d[0] & 0x0F)]
        elif pci == 0x1:  # FF
            exp_len = ((d[0] & 0x0F) << 8) | d[1]
            buf[:] = d[2:]
            continue
        elif pci == 0x2:  # CF
            buf.extend(d[1:])
            if len(buf) < exp_len:
                continue
            payload = buf[:exp_len]
            buf.clear()
        else:
            continue

        if len(payload) < 2:
            continue

        sid = payload[0]

        # -------- DTC --------
        if sid == 0x59 and payload[1] == 0x04:
            i = 2
            plen = len(payload)
            while i + 23 <= plen:
                dtc = payload[i+2]
                if 0x50 <= dtc <= 0x62:
                    ff = payload[i+3:i+23]
                    occ,y,m,d_,h,mi,s,l1,t30,ntc,isense = FF_UNPACK(ff)
                    dtc_lines.append((
                        trace, hex(dtc), occ,
                        f"{y:04d}-{m:02d}-{d_:02d} {h:02d}:{mi:02d}:{s:02d}",
                        l1, t30, ntc, isense
                    ))

                i += 23

        # -------- DID --------
        elif sid == 0x62 and len(payload) >= 4:
            did = payload[2]
            if did in (0x01,0x02,0x03,0x04) and len(payload) >= 7:
                did_map[did] = int.from_bytes(payload[3:7], "little")
            elif did == 0x05 and len(payload) >= 5:
                did_map[did] = f"Reason={payload[3]} Info={payload[4]}"
            elif did in (0x06,0x07):
                did_map[did] = payload[3]
            elif did == 0x80 and len(payload) >= 7:
                did_map[did] = f"{payload[3]}.{payload[4]}.{payload[5]}.{payload[6]}"

        # -------- ROUTINES --------
        elif sid == 0x71 and len(payload) >= 5:
            rid = (payload[2] << 8) | payload[3]

            if rid == 0x41 and len(payload) >= 32:
                v = ROUTINE41_UNPACK(payload, 4)
                routine_lines.append((
                         trace,
                         v[0]/1000, v[1], v[2],
                         v[3]/100, v[4], v[5]/100,
                         v[6], 0
                ))


            elif rid == 0x43 and routine_lines:
                   t = routine_lines[-1]
                   routine_lines[-1] = (
                            t[0],  # trace
                            t[1],  # ISENSE
                            t[2],  # VFB_T30
                            t[3],  # VFB_L1
                            t[4],  # NTC
                            t[5],  # VREFINT
                            t[6],  # MCU_TEMP
                            t[7],  # CPU_LOAD
                             payload[-1]  # LOAD_STATUS
                    )


    for k,v in did_map.items():
        did_lines.append((trace, hex(k), v))
# -------- MAIN --------
for blf in BLF_DIR.glob("*.blf"):
    parse_blf(blf)
def fmt_row(fmt, row):
    return fmt.format(*row)

with open(OUT_TXT, "w", encoding="utf-8", buffering=8*1024*1024) as f:
    traces = sorted(set(x[0] for x in dtc_lines + did_lines + routine_lines))

    for trace in traces:
        f.write("="*80 + "\n")
        f.write(f"TRACE: {trace}\n")
        f.write("="*80 + "\n\n")

        # -------- DTCs --------
        f.write("[DTCs]\n" + "-"*80 + "\n")
        f.write("DTC    OCC  DATE & TIME           L1[mV]  T30[mV]  NTC[°C]  ISENSE[A]\n")
        f.write("-"*80 + "\n")

        for t,dtc,occ,date,l1,t30,ntc,isense in filter(lambda x: x[0]==trace, dtc_lines):
            f.write(f"{dtc:<6} {occ:<4} {date:<19} {l1:<7} {t30:<8} {ntc:>7.2f} {isense:>9.2f}\n")

        f.write("\n")

        # -------- DIDs --------
        f.write("[DIDs]\n" + "-"*80 + "\n")
        f.write("DID    VALUE\n")
        f.write("-"*80 + "\n")

        for t,did,val in filter(lambda x: x[0]==trace, did_lines):
            f.write(f"{did:<6} {val}\n")

        f.write("\n")

        # -------- ROUTINES --------
        f.write("[ROUTINES – RID 0x41]\n" + "-"*80 + "\n")
        f.write("ISENSE[A]  VFB_T30[mV]  VFB_L1[mV]  NTC[°C]  VREF[mV]  MCU[°C]  CPU[%]  LOAD\n")
        f.write("-"*80 + "\n")

        for r in filter(lambda x: x[0]==trace, routine_lines):
            _,ia,t30,l1,ntc,vref,mcu,cpu,load = r
            f.write(f"{ia:>9.3f}  {t30:<12}  {l1:<11}  {ntc:>7.2f}  {vref:<8}  {mcu:>7.2f}  {cpu:>6}  {load}\n")

        f.write("\n\n")

print("DONE")
# CODE BLOCK END Global_Definitions 
# CODE BLOCK BEGIN Instance 
Instance = MpInstance('PythonCode8051')
# CODE BLOCK END Instance 
# CODE BLOCK BEGIN Step_Function  NQ__
def step() -> None:
    pass
# CODE BLOCK END Step_Function 
# CODE BLOCK BEGIN Configuration
""" 
[UI]
UICommon=0,-1,-1,0,UHl0aG9uIENvZGUgRWRpdG9yIFtQeXRob25Db2RlODA1MV0_,100,417,3157586389001336625,0
ScriptName=PythonCode8051
DisplayName=PythonCode8051
DBDeps=(none)
LastBuildTime=2026-01-31 13:13:32
"""
# CODE BLOCK END Configuration

