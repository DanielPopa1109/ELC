#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
parse_uds_hist_excel_plot.py  (multi-group, 18 hist values)
- Reads CSV/XLSX CAN exports with per-byte response columns like '*Resp_0'..'*Resp_7'
- Reassembles ISO-TP UDS 0x71/0x62 responses
- Filters to the intended routine/DID if provided
- Aggregates ALL matching column groups with --all-groups
- Writes an Excel workbook with a tidy table (hist) and charts (charts)

Layout of 0x71 RC payload (after SID/sub/rid):
  [0]  SMon_ISenseL1_RMS_5s       (mA)
  [1]  SMon_ISenseL1_RMS_10s      (mA)
  [2]  SMon_ISenseL1_RMS_30s      (mA)
  [3]  SMon_VfbT30_RMS_5s         (mV)
  [4]  SMon_VfbT30_RMS_10s        (mV)
  [5]  SMon_VfbT30_RMS_30s        (mV)
  [6]  SMon_VfbL1_RMS_5s          (mV)
  [7]  SMon_VfbL1_RMS_10s         (mV)
  [8]  SMon_VfbL1_RMS_30s         (mV)
  [9]  SMon_NTC_RMS_5s            (raw)
  [10] SMon_NTC_RMS_10s           (raw)
  [11] SMon_NTC_RMS_30s           (raw)
  [12] SMon_Vrefint_RMS_5s        (raw)
  [13] SMon_Vrefint_RMS_10s       (raw)
  [14] SMon_Vrefint_RMS_30s       (raw)
  [15] SMon_McuTemp_RMS_5s        (raw)
  [16] SMon_McuTemp_RMS_10s       (raw)
  [17] SMon_McuTemp_RMS_30s       (raw)

Usage examples:
  python parse_uds_hist_excel_plot.py input.csv  --id 703 --out uds_histograms_plots.xlsx
  python parse_uds_hist_excel_plot.py input.xlsx --id 703 --sheet 0 --rid 0x0041 --sub 0x00 --all-groups --out out.xlsx
"""

import argparse, re
from pathlib import Path

NVALS = 18  # number of uint32 histogram values in the payload


def read_table(path, sheet=None):
    import pandas as pd
    p = Path(path)
    if not p.exists():
        raise FileNotFoundError(f"Input not found: {p}")
    if p.suffix.lower() in ['.xlsx', '.xls']:
        engine = 'openpyxl' if p.suffix.lower() == '.xlsx' else None
        if sheet is None:
            df = pd.read_excel(p, sheet_name=0, engine=engine)
        else:
            df = pd.read_excel(p, sheet_name=sheet, engine=engine)
    else:
        import pandas as pd
        df = None
        for sep in [None, ';', ',', '\t', '|']:
            try:
                tmp = pd.read_csv(p, sep=sep, engine='python')
                if tmp.shape[1] >= 3:
                    df = tmp
                    break
            except Exception:
                pass
        if df is None or df.shape[1] < 3:
            df = None
            for enc in ['utf-8-sig', 'latin-1']:
                try:
                    tmp = pd.read_csv(p, sep=None, engine='python', encoding=enc)
                    if tmp.shape[1] >= 3:
                        df = tmp
                        break
                except Exception:
                    pass
        if df is None:
            raise RuntimeError("Unable to read CSV with common delimiters/encodings.")
    return df


def detect_time_col(df):
    for c in ['Hardware Time', 'Relative Time', 'Date Time', 'Time', 'Timestamp', 'Time Stamp']:
        if c in df.columns:
            return c
    for c in df.columns:
        if 'time' in str(c).lower():
            return c
    return None


def find_resp_groups(columns, idfilter=None):
    pat = re.compile(r'^(.*?)(?:[_\s\-.]*)Resp[_\s\-]?([0-7])\s*$', re.I)
    groups = {}
    for name in columns:
        m = pat.match(str(name))
        if not m:
            continue
        base = m.group(1).strip()
        idx = int(m.group(2))
        d = groups.get(base, {})
        d[idx] = name
        groups[base] = d
    candidates = []
    for base, mapping in groups.items():
        if all(i in mapping for i in range(8)):
            candidates.append((base, mapping))
    if not candidates:
        return []
    if idfilter is not None:
        s = str(idfilter)
        pri = [c for c in candidates if re.search(rf'\b{s}\b', c[0]) or s in c[0]]
        if pri:
            return pri
    return candidates


def extract_row_bytes(row, mapping):
    out = []
    for i in range(8):
        v = row[mapping[i]]
        try:
            if isinstance(v, str):
                vv = v.strip().lower().replace('0x', '')
                if re.fullmatch(r'[0-9a-f]{1,2}', vv):
                    out.append(int(vv, 16))
                else:
                    out.append(int(float(v)))
            else:
                out.append(int(float(v)))
        except Exception:
            out.append(0)
    out = [max(0, min(255, x)) for x in out]
    return out


def le32(buf, off):
    return buf[off] | (buf[off + 1] << 8) | (buf[off + 2] << 16) | (buf[off + 3] << 24)


def parse_hist_rows(df, time_col, mapping, expect_did=None, expect_rid=None, expect_sub=None):
    rows = []
    collecting = False
    need = 0
    buf = bytearray()
    t0 = None

    for _, row in df.iterrows():
        b = extract_row_bytes(row, mapping)
        pci = b[0] & 0xF0

        if pci == 0x10:  # FF
            total = ((b[0] & 0x0F) << 8) | b[1]
            payload = b[2:8]
            buf = bytearray(payload)
            need = total - len(payload)
            collecting = True
            t0 = row[time_col] if time_col else None
            continue

        if not collecting:
            continue

        if pci == 0x20:  # CF
            pl = b[1:8]
            take = min(len(pl), need)
            buf.extend(pl[:take])
            need -= take
            if need > 0:
                continue

            # Finished assembling one ISO-TP payload
            collecting = False

            # Minimum lengths:
            #  - 0x71: SID + sub + RID (2 bytes) + NVALS*4
            #  - 0x62: SID + DID (2 bytes)         + NVALS*4
            min_71 = 1 + 1 + 2 + 4 * NVALS
            min_62 = 1 + 2 + 4 * NVALS

            sid = buf[0]
            if sid == 0x71 and len(buf) >= min_71:
                sub = buf[1]
                rid = (buf[2] << 8) | buf[3]
                if expect_rid is not None and rid != expect_rid:
                    continue
                if expect_sub is not None and sub != expect_sub:
                    continue
                offset = 4
            elif sid == 0x62 and len(buf) >= min_62:
                did = (buf[1] << 8) | buf[2]
                if expect_did is not None and did != expect_did:
                    continue
                offset = 3
            else:
                continue

            # Extract 18 uint32 values
            if len(buf) < offset + 4 * NVALS:
                continue
            vals = [le32(buf, offset + 4 * i) for i in range(NVALS)]
            rows.append((t0, vals))

    return rows


def write_xlsx_with_charts(rows, out_xlsx):
    import pandas as pd
    import numpy as np
    from pandas import ExcelWriter

    if not rows:
        raise RuntimeError("No UDS histogram responses decoded.")

    recs = []
    for t0, v in rows:
        # v[0..17] as described in module docstring
        rec = {
            'time': t0,
            'ISense5s_mA':       v[0],
            'ISense10s_mA':      v[1],
            'ISense30s_mA':      v[2],
            'VfbT30_5s_mV':      v[3],
            'VfbT30_10s_mV':     v[4],
            'VfbT30_30s_mV':     v[5],
            'VfbL1_5s_mV':       v[6],
            'VfbL1_10s_mV':      v[7],
            'VfbL1_30s_mV':      v[8],
            'NTC_5s_raw':        v[9],
            'NTC_10s_raw':       v[10],
            'NTC_30s_raw':       v[11],
            'Vrefint_5s_raw':    v[12],
            'Vrefint_10s_raw':   v[13],
            'Vrefint_30s_raw':   v[14],
            'McuTemp_5s_raw':    v[15],
            'McuTemp_10s_raw':   v[16],
            'McuTemp_30s_raw':   v[17],
        }

        # Derived units for current / voltage
        rec['ISense5s_A']   = rec['ISense5s_mA'] / 1000.0
        rec['ISense10s_A']  = rec['ISense10s_mA'] / 1000.0
        rec['ISense30s_A']  = rec['ISense30s_mA'] / 1000.0

        rec['VfbT30_5s_V']  = rec['VfbT30_5s_mV'] / 1000.0
        rec['VfbT30_10s_V'] = rec['VfbT30_10s_mV'] / 1000.0
        rec['VfbT30_30s_V'] = rec['VfbT30_30s_mV'] / 1000.0

        rec['VfbL1_5s_V']   = rec['VfbL1_5s_mV'] / 1000.0
        rec['VfbL1_10s_V']  = rec['VfbL1_10s_mV'] / 1000.0
        rec['VfbL1_30s_V']  = rec['VfbL1_30s_mV'] / 1000.0

        recs.append(rec)

    df = pd.DataFrame(recs)

    # Numeric x-axis; fallback to sample index
    try:
        x_series = pd.to_numeric(df['time'], errors='coerce')
        if x_series.isna().any():
            raise ValueError
        df['t_sec'] = x_series
        xcol = 't_sec'
        xname = 'time (s)'
    except Exception:
        df.insert(0, 'sample', np.arange(len(df)))
        xcol = 'sample'
        xname = 'sample'

    with ExcelWriter(out_xlsx, engine='xlsxwriter') as xw:
        df.to_excel(xw, index=False, sheet_name='hist')
        wb = xw.book
        wsC = wb.add_worksheet('charts')

        def col_idx(cname):
            return df.columns.get_loc(cname)

        # Voltage chart
        chartV = wb.add_chart({'type': 'line'})
        for c in ['VfbT30_5s_V', 'VfbT30_10s_V', 'VfbT30_30s_V',
                  'VfbL1_5s_V', 'VfbL1_10s_V', 'VfbL1_30s_V']:
            chartV.add_series({
                'name':       ['hist', 0, col_idx(c)],
                'categories': ['hist', 1, col_idx(xcol), len(df), col_idx(xcol)],
                'values':     ['hist', 1, col_idx(c),    len(df), col_idx(c)],
            })
        chartV.set_title({'name': 'Voltages vs ' + xname})
        chartV.set_x_axis({'name': xname})
        chartV.set_y_axis({'name': 'Voltage (V)'})
        wsC.insert_chart('B2', chartV, {'x_scale': 1.3, 'y_scale': 1.1})

        # Current chart
        chartI = wb.add_chart({'type': 'line'})
        for c in ['ISense5s_A', 'ISense10s_A', 'ISense30s_A']:
            chartI.add_series({
                'name':       ['hist', 0, col_idx(c)],
                'categories': ['hist', 1, col_idx(xcol), len(df), col_idx(xcol)],
                'values':     ['hist', 1, col_idx(c),    len(df), col_idx(c)],
            })
        chartI.set_title({'name': 'Currents vs ' + xname})
        chartI.set_x_axis({'name': xname})
        chartI.set_y_axis({'name': 'Current (A)'})
        wsC.insert_chart('B22', chartI, {'x_scale': 1.3, 'y_scale': 1.1})

        # NTC chart
        chartNTC = wb.add_chart({'type': 'line'})
        for c in ['NTC_5s_raw', 'NTC_10s_raw', 'NTC_30s_raw']:
            chartNTC.add_series({
                'name':       ['hist', 0, col_idx(c)],
                'categories': ['hist', 1, col_idx(xcol), len(df), col_idx(xcol)],
                'values':     ['hist', 1, col_idx(c),    len(df), col_idx(c)],
            })
        chartNTC.set_title({'name': 'NTC vs ' + xname})
        chartNTC.set_x_axis({'name': xname})
        chartNTC.set_y_axis({'name': 'NTC (raw)'})
        wsC.insert_chart('B42', chartNTC, {'x_scale': 1.3, 'y_scale': 1.1})

        # Vrefint chart
        chartVref = wb.add_chart({'type': 'line'})
        for c in ['Vrefint_5s_raw', 'Vrefint_10s_raw', 'Vrefint_30s_raw']:
            chartVref.add_series({
                'name':       ['hist', 0, col_idx(c)],
                'categories': ['hist', 1, col_idx(xcol), len(df), col_idx(xcol)],
                'values':     ['hist', 1, col_idx(c),    len(df), col_idx(c)],
            })
        chartVref.set_title({'name': 'Vrefint vs ' + xname})
        chartVref.set_x_axis({'name': xname})
        chartVref.set_y_axis({'name': 'Vrefint (raw)'})
        wsC.insert_chart('B62', chartVref, {'x_scale': 1.3, 'y_scale': 1.1})

        # McuTemp chart
        chartMcu = wb.add_chart({'type': 'line'})
        for c in ['McuTemp_5s_raw', 'McuTemp_10s_raw', 'McuTemp_30s_raw']:
            chartMcu.add_series({
                'name':       ['hist', 0, col_idx(c)],
                'categories': ['hist', 1, col_idx(xcol), len(df), col_idx(xcol)],
                'values':     ['hist', 1, col_idx(c),    len(df), col_idx(c)],
            })
        chartMcu.set_title({'name': 'McuTemp vs ' + xname})
        chartMcu.set_x_axis({'name': xname})
        chartMcu.set_y_axis({'name': 'McuTemp (raw)'})
        wsC.insert_chart('B82', chartMcu, {'x_scale': 1.3, 'y_scale': 1.1})



def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('input', help='Input .xlsx/.xls/.csv')
    ap.add_argument('--sheet', help='Excel sheet name or 0-based index', default=None)
    ap.add_argument('--id', help='Responder ID hint embedded in column names, e.g., 703', default=None)
    ap.add_argument('--did', help='Filter: expected DID for 0x62 positive response (hex like 0xF150)', default=None)
    ap.add_argument('--rid', help='Filter: expected Routine ID for 0x71 positive response (hex like 0x0041)', default=None)
    ap.add_argument('--sub', help='Filter: expected subfunction for 0x71 positive response (hex like 0x00)', default=None)
    ap.add_argument('--all-groups', action='store_true', help='Parse ALL matching Resp_0..7 groups instead of only the first')
    ap.add_argument('--out', default='uds_histograms_plots.xlsx', help='Output Excel file with charts')
    args = ap.parse_args()

    sheet = args.sheet
    if sheet is not None:
        try:
            sheet = int(sheet)
        except Exception:
            pass

    df = read_table(args.input, sheet=sheet)
    time_col = detect_time_col(df)

    # Group detection
    idfilter = None
    if args.id is not None:
        idfilter = re.sub(r'[^0-9A-Fa-f]', '', str(args.id))
        try:
            idfilter = str(int(idfilter, 0))
        except Exception:
            pass

    groups = find_resp_groups(df.columns, idfilter=idfilter)
    if not groups:
        raise RuntimeError("Could not find any Resp_0..Resp_7 column groups. Use --id to hint the responder.")
    selected = groups if args.all_groups else groups[:1]

    def to_int_or_none(x):
        if x is None:
            return None
        s = str(x).strip()
        try:
            return int(s, 0)
        except Exception:
            try:
                return int(s, 16)
            except Exception:
                return None

    exp_did = to_int_or_none(args.did)
    exp_rid = to_int_or_none(args.rid)
    exp_sub = to_int_or_none(args.sub)

    all_rows = []
    for base, mapping in selected:
        use_cols = [mapping[i] for i in range(8)]
        if time_col and time_col not in use_cols:
            use_cols = [time_col] + use_cols
        df_small = df[use_cols].copy()
        rows = parse_hist_rows(df_small, time_col, mapping,
                               expect_did=exp_did, expect_rid=exp_rid, expect_sub=exp_sub)
        for t0, vals in rows:
            all_rows.append((t0, vals))

    if not all_rows:
        raise RuntimeError("No matching UDS histogram responses found in the selected groups with the given filters.")

    out_xlsx = Path(args.out)
    write_xlsx_with_charts(all_rows, out_xlsx)
    print(f"OK: wrote Excel with charts: {out_xlsx}")


if __name__ == '__main__':
    main()
