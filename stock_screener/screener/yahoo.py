"""Thin client for Yahoo Finance's public (no-auth) JSON endpoints, with a disk cache.

Endpoints used:
  * /v8/finance/chart/{sym}                          -> daily OHLC, adjusted close, dividends
  * /ws/fundamentals-timeseries/v1/finance/timeseries -> EPS, P/E, forward P/E, market cap
"""
from __future__ import annotations

import hashlib
import json
import os
import time
import urllib.parse
import urllib.request
from pathlib import Path

import pandas as pd

BASE = "https://query1.finance.yahoo.com"
UA = "Mozilla/5.0 (X11; Linux x86_64) stock-screener/0.1"


def _data_dir() -> Path:
    """$SCREENER_DATA_DIR, else <project>/data if writable (dev checkout), else ~/.cache/stock-screener
    (installed skills may live in a read-only directory)."""
    if os.environ.get("SCREENER_DATA_DIR"):
        return Path(os.environ["SCREENER_DATA_DIR"]).expanduser()
    project = Path(__file__).resolve().parent.parent
    if os.access(project, os.W_OK):
        return project / "data"
    return Path.home() / ".cache" / "stock-screener"


DATA_DIR = _data_dir()
CACHE_DIR = DATA_DIR / "cache"
DEFAULT_TTL = 12 * 3600  # seconds


def _get_json(url: str, ttl: int = DEFAULT_TTL, retries: int = 3) -> dict:
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    path = CACHE_DIR / (hashlib.sha1(url.encode()).hexdigest() + ".json")
    if ttl > 0 and path.exists() and time.time() - path.stat().st_mtime < ttl:
        return json.loads(path.read_text())
    last_err = None
    for attempt in range(retries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=20) as resp:
                data = json.loads(resp.read())
            path.write_text(json.dumps(data))
            return data
        except Exception as e:  # network / HTTP errors: back off and retry
            last_err = e
            time.sleep(1.5 * (attempt + 1))
    raise RuntimeError(f"fetch failed: {url}: {last_err}")


def history(symbol: str, years: float = 6.0, ttl: int = DEFAULT_TTL) -> tuple[pd.DataFrame, pd.Series, dict]:
    """Daily history. Returns (prices df[close, adjclose, volume], dividends series, meta)."""
    now = int(time.time())
    # round period1 to the day so the cache key is stable within a day
    p1 = (now - int(years * 365.25 * 86400)) // 86400 * 86400
    q = urllib.parse.urlencode({"period1": p1, "period2": now // 3600 * 3600,
                                "interval": "1d", "events": "div,split"})
    url = f"{BASE}/v8/finance/chart/{urllib.parse.quote(symbol)}?{q}"
    r = _get_json(url, ttl)["chart"]["result"][0]
    tz = r["meta"].get("exchangeTimezoneName", "America/New_York")
    idx = pd.to_datetime(r["timestamp"], unit="s", utc=True).tz_convert(tz).normalize().tz_localize(None)
    quote = r["indicators"]["quote"][0]
    adj = r["indicators"].get("adjclose", [{}])[0].get("adjclose", quote["close"])
    df = pd.DataFrame({"close": quote["close"], "adjclose": adj, "volume": quote["volume"]}, index=idx)
    df = df[~df.index.duplicated(keep="last")].dropna(subset=["close"])
    divs = r.get("events", {}).get("dividends", {})
    if divs:
        d = pd.Series({pd.to_datetime(v["date"], unit="s").normalize(): v["amount"] for v in divs.values()})
        d = d.sort_index()
    else:
        d = pd.Series(dtype=float)
    meta = dict(r["meta"])
    meta["splits"] = sorted((pd.to_datetime(v["date"], unit="s").normalize(), v["numerator"] / v["denominator"])
                            for v in r.get("events", {}).get("splits", {}).values())
    return df, d, meta


FUNDAMENTAL_TYPES = ["quarterlyDilutedEPS", "trailingDilutedEPS", "trailingPeRatio",
                     "trailingForwardPeRatio", "trailingMarketCap"]


def fundamentals(symbol: str, years: float = 7.0, ttl: int = DEFAULT_TTL) -> dict[str, pd.Series]:
    """Fundamental time series keyed by type -> Series indexed by asOfDate."""
    now = int(time.time())
    p1 = (now - int(years * 365.25 * 86400)) // 86400 * 86400
    q = urllib.parse.urlencode({"type": ",".join(FUNDAMENTAL_TYPES), "period1": p1,
                                "period2": now // 86400 * 86400 + 86400})
    url = f"{BASE}/ws/fundamentals-timeseries/v1/finance/timeseries/{urllib.parse.quote(symbol)}?{q}"
    out: dict[str, pd.Series] = {}
    for res in _get_json(url, ttl)["timeseries"]["result"]:
        t = res["meta"]["type"][0]
        pts = [x for x in res.get(t, []) if x and x.get("reportedValue")]
        s = pd.Series({pd.Timestamp(x["asOfDate"]): x["reportedValue"]["raw"] for x in pts}, dtype=float)
        out[t] = s.sort_index()
    return out
