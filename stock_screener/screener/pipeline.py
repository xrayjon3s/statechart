"""Fetch data for a universe, compute metrics, and persist a snapshot."""
from __future__ import annotations

import json
import sys
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from pathlib import Path

import pandas as pd

from . import metrics, yahoo
from .universe import INDEXES, MEGACAP_MIN_CAP, UNIVERSES

DATA_DIR = Path(__file__).resolve().parent.parent / "data"
SNAPSHOT = DATA_DIR / "snapshot.csv"
INDEX_SNAPSHOT = DATA_DIR / "indexes.csv"


def build(symbols: list[str] | None = None, universe: str = "megacap",
          min_cap: float | None = None, ttl: int = yahoo.DEFAULT_TTL, workers: int = 8) -> pd.DataFrame:
    if symbols is None:
        symbols = UNIVERSES[universe]
        min_cap = MEGACAP_MIN_CAP if min_cap is None and universe == "megacap" else min_cap

    index_adj, idx_rows = {}, {}
    for isym, name in INDEXES.items():
        px, _, _ = yahoo.history(isym, ttl=ttl)
        index_adj[isym] = px["adjclose"]
        idx_rows[isym] = {"symbol": isym, "name": name, "price": float(px["close"].iloc[-1]),
                          **metrics.index_metrics(px["adjclose"])}

    def one(sym: str) -> dict | None:
        try:
            px, divs, meta = yahoo.history(sym, ttl=ttl)
            fund = yahoo.fundamentals(sym, ttl=ttl)
            return metrics.compute(sym, px, divs, meta, fund, index_adj, idx_rows)
        except Exception as e:
            print(f"warning: {sym}: {e}", file=sys.stderr)
            return None

    with ThreadPoolExecutor(workers) as ex:
        rows = [r for r in ex.map(one, symbols) if r]
    df = pd.DataFrame(rows).set_index("symbol")
    if min_cap:
        df = df[df["market_cap"] >= min_cap]
    df = df.sort_values("market_cap", ascending=False)

    DATA_DIR.mkdir(exist_ok=True)
    df.to_csv(SNAPSHOT)
    pd.DataFrame(idx_rows.values()).set_index("symbol").to_csv(INDEX_SNAPSHOT)
    (DATA_DIR / "snapshot_meta.json").write_text(json.dumps(
        {"built_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
         "universe": universe, "count": len(df)}, indent=2))
    return df


def load() -> pd.DataFrame:
    if not SNAPSHOT.exists():
        raise SystemExit("No snapshot yet: run `python -m screener refresh` first.")
    return pd.read_csv(SNAPSHOT, index_col="symbol")


def load_indexes() -> pd.DataFrame:
    return pd.read_csv(INDEX_SNAPSHOT, index_col="symbol")
