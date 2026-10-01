"""US Market Desk Gauge — ตัวดึงข้อมูล (รันบน GitHub Actions)

Yahoo (ดัชนี/futures/VIX/สินค้าโภคภัณฑ์) + Alpaca (หุ้น + หุ้นร้อนแรง) + FRED (2Y, Fed Funds)
-> market.json ก้อนเล็ก (< 4KB) -> push ไป branch `data` -> ESP32 อ่านผ่าน GitHub API

ใช้แค่ standard library (Python 3.9+)

env:
  ALPACA_KEY_ID, ALPACA_SECRET   (ไม่ใส่ = ใช้ Yahoo สำหรับหุ้น core และไม่มีหุ้นร้อนแรง)
  FRED_API_KEY                   (ไม่ใส่ = ไม่มี 2Y/FED/curve)
  PUBLISH_CMD                    คำสั่ง shell ที่รันหลังเขียนไฟล์แต่ละรอบ (workflow ใช้ push)

รันเอง:  python fetcher/fetch.py --out market.json
"""
import argparse
import json
import os
import subprocess
import sys
import time
import urllib.parse
import urllib.request
from datetime import datetime, timedelta, timezone
from zoneinfo import ZoneInfo

NY = ZoneInfo("America/New_York")
UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0 Safari/537.36"

# [ชื่อแสดงผล, Yahoo symbol] — ข้อมูลมหภาค (key "q")
MACRO = [
    ("SPX", "^GSPC"), ("NDX", "^NDX"), ("DJI", "^DJI"), ("RUT", "^RUT"),
    ("ES", "ES=F"), ("NQ", "NQ=F"),
    ("10Y", "^TNX"), ("VIX", "^VIX"),
    ("DXY", "DX-Y.NYB"), ("THB", "THB=X"),
    ("GOLD", "GC=F"), ("WTI", "CL=F"), ("BRENT", "BZ=F"), ("COPPER", "HG=F"),
]
SPARKS = {"SPX": "^GSPC", "ES": "ES=F"}  # 5 วัน สำหรับหน้า Chart
SPARK_POINTS = 30

# หุ้นท็อปตาม market cap เรียงใหญ่ -> เล็ก (ต้องตรงกับ CORE ใน tools/pack_bubbles.py)
# ชื่อแบบ Alpaca (BRK.B) — Yahoo ใช้ BRK-B
CORE = [
    "NVDA", "MSFT", "AAPL", "GOOGL", "AMZN", "META", "AVGO", "TSLA", "BRK.B", "JPM",
    "WMT", "LLY", "ORCL", "V", "MA", "NFLX", "XOM", "COST", "PLTR", "JNJ",
    "HD", "PG", "AMD", "ABBV", "BAC",
]
HOT_COUNT = 15
HOT_MIN_PRICE = 5.0
# ไม่นับ ETF / กองทุน leverage เป็นหุ้นร้อนแรง
ETF_EXCLUDE = {
    "SPY", "QQQ", "IWM", "DIA", "VOO", "IVV", "VTI", "TQQQ", "SQQQ", "SPXL", "SPXS", "SPXU", "UPRO",
    "SOXL", "SOXS", "TSLL", "TSLQ", "TSLZ", "NVDL", "NVDS", "NVDQ", "UVXY", "VXX", "SVXY", "TLT",
    "HYG", "LQD", "XLF", "XLE", "XLK", "GLD", "SLV", "USO", "KWEB", "FXI", "EEM", "EWZ", "ARKK",
    "SMH", "IBIT", "ETHA", "BITO", "MSTU", "MSTZ", "CONL", "LABU", "LABD", "TZA", "TNA", "YINN",
    "SCHD", "JEPI", "JEPQ", "BND", "AGG", "VEA", "VWO", "XLV", "XLI", "XLY", "XLP", "XLU", "GDX",
    "MUU", "AMDL", "AAPU", "GGLL", "METU", "AMZU", "PLTU", "SOXX", "UCO", "BOIL", "KOLD", "NUGT",
    "GOOG", "BRK.A",  # share class ซ้ำกับตัวใน CORE
}

# วันหยุด NYSE (ปิดทั้งวัน) และวันปิดเร็ว 13:00 ET
HOLIDAYS = {
    "2026-01-01", "2026-01-19", "2026-02-16", "2026-04-03", "2026-05-25",
    "2026-06-19", "2026-07-03", "2026-09-07", "2026-11-26", "2026-12-25",
    "2027-01-01", "2027-01-18", "2027-02-15", "2027-03-26", "2027-05-31",
    "2027-06-18", "2027-07-05", "2027-09-06", "2027-11-25", "2027-12-24",
}
EARLY_CLOSE = {"2026-11-27", "2026-12-24", "2027-11-26"}

# เหตุการณ์สำคัญ (เวลา ET) — ตรวจวันจริงกับ bls.gov/schedule และ federalreserve.gov
EVENTS = [
    ("NFP", "2026-10-02 08:30"), ("CPI", "2026-10-14 08:30"), ("FOMC", "2026-10-28 14:00"),
    ("NFP", "2026-11-06 08:30"), ("CPI", "2026-11-12 08:30"), ("NFP", "2026-12-04 08:30"),
    ("FOMC", "2026-12-09 14:00"), ("CPI", "2026-12-10 08:30"),
]


def log(*a):
    print(datetime.now().strftime("%H:%M:%S"), *a, file=sys.stderr, flush=True)


def get_json(url, headers=None, timeout=15):
    h = {"User-Agent": UA, "Accept": "application/json"}
    h.update(headers or {})
    req = urllib.request.Request(url, headers=h)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read().decode())


def is_num(v):
    return isinstance(v, (int, float)) and v == v and abs(v) != float("inf")


def rnd(v, d=None):
    if d is None:
        d = 3 if abs(v) < 10 else 2
    return round(v, d)


def pct(p, prev):
    return rnd((p - prev) / prev * 100, 2) if is_num(prev) and prev else 0


# ---------------- ตลาด ----------------

def market_status(now):
    t = now.astimezone(NY)
    ymd = t.strftime("%Y-%m-%d")
    if t.weekday() >= 5 or ymd in HOLIDAYS:
        return "CLOSED"
    hm = t.hour * 60 + t.minute
    close, post_end = (13 * 60, 17 * 60) if ymd in EARLY_CLOSE else (16 * 60, 20 * 60)
    if 4 * 60 <= hm < 9 * 60 + 30:
        return "PRE"
    if 9 * 60 + 30 <= hm < close:
        return "OPEN"
    if close <= hm < post_end:
        return "POST"
    return "CLOSED"


def upcoming_events(now):
    out = []
    for name, when in EVENTS:
        at = datetime.strptime(when, "%Y-%m-%d %H:%M").replace(tzinfo=NY)
        if now - timedelta(hours=12) <= at <= now + timedelta(days=7):
            out.append({"t": name, "at": int(at.timestamp())})
    return sorted(out, key=lambda e: e["at"])[:6]


# ---------------- Yahoo ----------------

def yahoo_quotes(symbols):
    """{yahoo_sym: (price, prev_close)} — batch spark ก่อน แล้ว fallback v8 chart ทีละตัว"""
    out = {}
    for i in range(0, len(symbols), 20):
        chunk = symbols[i:i + 20]
        url = ("https://query1.finance.yahoo.com/v7/finance/spark?symbols="
               + urllib.parse.quote(",".join(chunk)) + "&range=1d&interval=5m")
        try:
            j = get_json(url)
        except Exception as e:  # noqa: BLE001
            log("spark", e)
            continue
        if isinstance(j, dict) and "spark" in j:
            for r in (j["spark"].get("result") or []):
                resp = (r.get("response") or [None])[0]
                if not resp:
                    continue
                m = resp.get("meta", {})
                closes = (resp.get("indicators", {}).get("quote") or [{}])[0].get("close") or []
                p = m.get("regularMarketPrice")
                if not is_num(p):
                    p = next((c for c in reversed(closes) if is_num(c)), None)
                if is_num(p):
                    out[r["symbol"]] = (p, m.get("previousClose") or m.get("chartPreviousClose"))
        elif isinstance(j, dict):  # รูปแบบ v8 {SYM: {close: [...]}}
            for k, r in j.items():
                closes = (r or {}).get("close") or []
                p = next((c for c in reversed(closes) if is_num(c)), None)
                if is_num(p):
                    out[k] = (p, r.get("previousClose") or r.get("chartPreviousClose"))

    for s in [s for s in symbols if s not in out]:
        try:
            m = get_json("https://query1.finance.yahoo.com/v8/finance/chart/"
                         + urllib.parse.quote(s) + "?range=1d&interval=5m")["chart"]["result"][0]["meta"]
            if is_num(m.get("regularMarketPrice")):
                out[s] = (m["regularMarketPrice"], m.get("previousClose") or m.get("chartPreviousClose"))
        except Exception as e:  # noqa: BLE001
            log("chart", s, e)
    return out


def yahoo_spark5d(sym):
    try:
        res = get_json("https://query1.finance.yahoo.com/v8/finance/chart/"
                       + urllib.parse.quote(sym) + "?range=5d&interval=1h")["chart"]["result"][0]
        closes = [c for c in res["indicators"]["quote"][0]["close"] if is_num(c)]
    except Exception as e:  # noqa: BLE001
        log("spark5d", sym, e)
        return None
    if len(closes) < 2:
        return None
    n = SPARK_POINTS
    if len(closes) > n:
        closes = [closes[round(i * (len(closes) - 1) / (n - 1))] for i in range(n)]
    return [rnd(c) for c in closes]


# ---------------- Alpaca ----------------

def alpaca_headers():
    k, s = os.environ.get("ALPACA_KEY_ID"), os.environ.get("ALPACA_SECRET")
    return {"APCA-API-KEY-ID": k, "APCA-API-SECRET-KEY": s} if k and s else None


def alpaca_snapshots(symbols, hdr):
    """{sym: (price, prev_close)} จาก IEX feed (ฟรี)"""
    url = ("https://data.alpaca.markets/v2/stocks/snapshots?feed=iex&symbols="
           + urllib.parse.quote(",".join(symbols)))
    j = get_json(url, hdr)
    today = datetime.now(NY).strftime("%Y-%m-%d")
    out = {}
    for sym, snap in j.items():
        if not snap:
            continue
        trade = (snap.get("latestTrade") or {}).get("p")
        daily = snap.get("dailyBar") or {}
        prev_bar = snap.get("prevDailyBar") or {}
        p = trade if is_num(trade) else daily.get("c")
        # ก่อนตลาดเปิดของวันใหม่ dailyBar ยังเป็นของเมื่อวาน -> ใช้มันเป็นราคาปิดก่อนหน้า
        prev = prev_bar.get("c") if str(daily.get("t", ""))[:10] == today else daily.get("c")
        if is_num(p):
            out[sym] = (p, prev)
    return out


def alpaca_most_active(hdr, top=50):
    j = get_json("https://data.alpaca.markets/v1beta1/screener/stocks/most-actives?by=trades&top=%d" % top, hdr)
    return [x["symbol"] for x in j.get("most_actives", [])]


# ---------------- FRED ----------------

def fred(series):
    key = os.environ.get("FRED_API_KEY")
    if not key:
        return None
    try:
        j = get_json("https://api.stlouisfed.org/fred/series/observations?series_id=%s&api_key=%s"
                     "&file_type=json&sort_order=desc&limit=10" % (series, key))
    except Exception as e:  # noqa: BLE001
        log("fred", series, e)
        return None
    obs = [float(o["value"]) for o in j.get("observations", []) if o["value"] != "."]
    return (obs[0], obs[1] if len(obs) > 1 else obs[0]) if obs else None


# ---------------- อารมณ์ตลาด ----------------

def cnn_fear_greed():
    """CNN Fear & Greed 0-100 (endpoint ไม่เป็นทางการ) — ดึงไม่ได้คืน None"""
    try:
        j = get_json("https://production.dataviz.cnn.io/index/fearandgreed/graphdata",
                     {"Referer": "https://edition.cnn.com/", "Origin": "https://edition.cnn.com"})
        return round(j["fear_and_greed"]["score"])
    except Exception as e:  # noqa: BLE001
        log("cnn", e)
        return None


def clamp1(x):
    return max(-1.0, min(1.0, x))


def market_mood(q, b):
    """คะแนน risk-on/off 0-100 จากข้อมูลที่มี + เหตุผลสั้นๆ (ตัวที่มีผลมากสุด 4 ตัว)

    แต่ละตัวแปลงเป็น -1..+1 (บวก = เอื้อหุ้นขึ้น) แล้วถ่วงน้ำหนัก
    ไม่ใช่การทำนาย — วัดว่าตอนนี้ตลาดเอียงไปทางกล้าเสี่ยงหรือกลัว
    """
    m = {x["s"]: x for x in q}
    parts = []  # (น้ำหนัก, คะแนน, ข้อความ)

    fut = [m[s]["c"] for s in ("ES", "NQ") if s in m]
    if fut:
        f = sum(fut) / len(fut)
        parts.append((0.30, clamp1(f / 1.0), "FUT %+.1f%%" % f))
    if "VIX" in m:
        v, vc = m["VIX"]["p"], m["VIX"]["c"]
        parts.append((0.20, 0.5 * clamp1((20 - v) / 8) + 0.5 * clamp1(-vc / 10), "VIX %.0f %+.0f%%" % (v, vc)))
    valid = [x["c"] for x in b if is_num(x.get("c"))]
    if valid:
        up, dn = sum(c > 0 for c in valid), sum(c < 0 for c in valid)
        parts.append((0.20, (up - dn) / len(valid), "UP %d/%d" % (up, len(valid))))
    if "10Y" in m and m["10Y"]["c"]:
        p, c = m["10Y"]["p"], m["10Y"]["c"]
        bp = (p - p / (1 + c / 100)) * 100
        parts.append((0.10, clamp1(-bp / 8), "10Y %+.0fbp" % bp))
    if "DXY" in m:
        parts.append((0.10, clamp1(-m["DXY"]["c"] / 0.6), "DXY %+.1f%%" % m["DXY"]["c"]))
    if "GOLD" in m:
        parts.append((0.05, clamp1(-m["GOLD"]["c"] / 1.5), "GOLD %+.1f%%" % m["GOLD"]["c"]))
    if "WTI" in m:  # เฉพาะน้ำมันพุ่งแรงที่เป็นลบ
        parts.append((0.05, clamp1(-max(0.0, m["WTI"]["c"] - 1) / 3), "OIL %+.1f%%" % m["WTI"]["c"]))
    if not parts:
        return None

    wsum = sum(w for w, _, _ in parts)
    score = round(50 + 50 * sum(w * s for w, s, _ in parts) / wsum)
    top = sorted(parts, key=lambda x: -abs(x[0] * x[1]))[:4]
    return {"v": score, "r": "  ".join(t for _, _, t in top)}


# ---------------- ประกอบ JSON ----------------

class Builder:
    def __init__(self, last):
        self.last_q = {x["s"]: x for x in last.get("q", [])}
        self.last_b = {x["s"]: x for x in last.get("b", [])}
        self.fred_cache = None
        self.fg_cache = None
        self.spark_cache, self.spark_at = {}, 0
        self.hot_cache, self.hot_at = [], 0

    def build(self):
        now = datetime.now(timezone.utc)
        market = market_status(now)

        # --- macro (Yahoo) ---
        yq = yahoo_quotes([y for _, y in MACRO])
        if time.time() - self.spark_at > 600:  # sparkline 5 วันเปลี่ยนช้า
            for s, y in SPARKS.items():
                sp = yahoo_spark5d(y)
                if sp:
                    self.spark_cache[s] = sp
            self.spark_at = time.time()

        q, tnx = [], None
        for s, y in MACRO:
            if y in yq:
                p, prev = yq[y]
                if y == "^TNX" and p > 20:  # บางช่วง Yahoo ส่งเป็น x10
                    p, prev = p / 10, (prev / 10 if prev else prev)
                item = {"s": s, "p": rnd(p), "c": pct(p, prev)}
            elif s in self.last_q:
                item = {"s": s, "p": self.last_q[s]["p"], "c": self.last_q[s]["c"]}  # แคชค่าล่าสุดรายตัว
            else:
                continue
            if s in self.spark_cache:
                item["sp"] = self.spark_cache[s]
            if s == "10Y":
                tnx = item["p"]
            q.append(item)
        if not yq:
            raise RuntimeError("no live quotes from Yahoo")

        if self.fred_cache is None:  # รายวัน — ดึงครั้งเดียวต่อรอบ workflow
            self.fred_cache = {k: fred(k) for k in ("DGS2", "DFF")}
        curve = None
        if self.fred_cache.get("DGS2"):
            p, prev = self.fred_cache["DGS2"]
            q.append({"s": "2Y", "p": rnd(p), "c": pct(p, prev)})
            if tnx is not None:
                curve = rnd(tnx - p, 2)
        if self.fred_cache.get("DFF"):
            p, prev = self.fred_cache["DFF"]
            q.append({"s": "FED", "p": rnd(p), "c": pct(p, prev)})

        if self.fg_cache is None:  # รายวันเป็นหลัก — ดึงครั้งเดียวต่อรอบ workflow
            self.fg_cache = cnn_fear_greed() or -1

        b = self.bubbles()
        return {
            "ts": int(now.timestamp()),
            "market": market,
            "q": q,
            "b": b,
            "curve": curve,
            "mood": market_mood(q, b),
            "fg": self.fg_cache if self.fg_cache >= 0 else None,
            "events": upcoming_events(now),
        }

    def bubbles(self):
        """core ตามลำดับ market cap (ครบทุกตัวเสมอ ให้ตำแหน่งตรงช่อง) + hot เรียงตามความแรง"""
        hdr = alpaca_headers()
        data, hot_syms = {}, []
        if hdr:
            try:
                if time.time() - self.hot_at > 300:  # รายชื่อ most active เปลี่ยนช้า
                    self.hot_cache = [s for s in alpaca_most_active(hdr)
                                      if s not in CORE and s not in ETF_EXCLUDE]
                    self.hot_at = time.time()
                data = alpaca_snapshots(CORE + self.hot_cache, hdr)
                hot_syms = self.hot_cache
            except Exception as e:  # noqa: BLE001
                log("alpaca", e)
        missing = [s for s in CORE if s not in data]
        if missing:  # ไม่มีคีย์ Alpaca หรือดึงไม่ได้ -> Yahoo
            yq = yahoo_quotes([s.replace(".", "-") for s in missing])
            for s in missing:
                if s.replace(".", "-") in yq:
                    data[s] = yq[s.replace(".", "-")]

        b = []
        for s in CORE:
            if s in data:
                p, prev = data[s]
                b.append({"s": s, "p": rnd(p), "c": pct(p, prev)})
            elif s in self.last_b:
                b.append({k: self.last_b[s][k] for k in ("s", "p", "c")})
            else:
                b.append({"s": s, "p": None, "c": None})

        hot = []
        for s in hot_syms:
            if s in data:
                p, prev = data[s]
                if p >= HOT_MIN_PRICE and is_num(prev) and prev:
                    hot.append({"s": s, "p": rnd(p), "c": pct(p, prev), "h": 1})
        hot.sort(key=lambda x: -abs(x["c"]))  # ร้อนสุด = ขยับแรงสุด -> วงใหญ่สุด
        return b + hot[:HOT_COUNT]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="market.json")
    ap.add_argument("--loop", type=int, default=0,
                    help="วนดึงทุก --every วินาทีจนครบเวลานี้ (เฉพาะตอน PRE/OPEN/POST)")
    ap.add_argument("--every", type=int, default=60)
    args = ap.parse_args()

    try:
        with open(args.out, encoding="utf-8") as f:
            last = json.load(f)
    except (OSError, ValueError):
        last = {}
    builder = Builder(last)
    publish = os.environ.get("PUBLISH_CMD")
    deadline = time.time() + args.loop

    while True:
        t0 = time.time()
        try:
            payload = builder.build()
            body = json.dumps(payload, separators=(",", ":"))
            with open(args.out, "w", encoding="utf-8") as f:
                f.write(body)
            log("wrote %d bytes, market=%s, q=%d, b=%d"
                % (len(body), payload["market"], len(payload["q"]), len(payload["b"])))
            if publish:
                subprocess.run(publish, shell=True, check=False)
        except Exception as e:  # noqa: BLE001 — รอบนี้พัง ESP32 ยังมีไฟล์เดิม (จะขึ้น STALE เอง)
            log("build failed:", e)
            payload = {"market": "CLOSED"}

        if payload["market"] == "CLOSED" or time.time() + args.every > deadline:
            break
        time.sleep(max(1, args.every - (time.time() - t0)))


if __name__ == "__main__":
    main()
