"""Export snapshot + weekly prices as compact JSON for the browser visualizations in viz/."""
from __future__ import annotations

import json
import math
from pathlib import Path

import pandas as pd

from . import pipeline
from .universe import INDEXES


def _num(v, nd=4):
    return None if v is None or (isinstance(v, float) and math.isnan(v)) else round(float(v), nd)


def viz_data() -> dict:
    df, w = pipeline.load(), pipeline.load_weekly()
    w = w.iloc[:-1] if w.iloc[-1].isna().mean() > 0.5 else w  # drop a partial final week
    dates = [d.date().isoformat() for d in w.index]

    def growth(col: pd.Series) -> list:
        first = col.first_valid_index()
        base = col.loc[first]
        return [None if pd.isna(v) else round(100 * v / base, 1) for v in col]

    stocks = []
    for sym, r in df.iterrows():
        if sym not in w.columns:
            continue
        col = w[sym]
        first = col.first_valid_index()
        stocks.append({
            "s": sym, "n": r.get("name", sym), "sec": r.get("sector") if isinstance(r.get("sector"), str) else "",
            "pe": _num(r["pe_ttm"], 2), "fpe": _num(r["pe_fwd"], 2), "pevh": _num(r["pe_vs_hist"], 3),
            "r30": _num(r["ret_30d"]), "r1": _num(r["ret_1y"]), "r5": _num(r["ret_5y"]),
            "since": first.date().isoformat(), "dy": _num(r["div_yield"]), "cap": _num(r["market_cap"], 0),
            "idx": r["index"], "beta": _num(r["beta_1y"], 2),
            "v": growth(col),
        })
    idx = {k: {"name": n, "v": growth(w[k]), "r5": _num(w[k].dropna().iloc[-1] / w[k].dropna().iloc[0] - 1)}
           for k, n in INDEXES.items() if k in w.columns}
    meta = json.loads((pipeline.DATA_DIR / "snapshot_meta.json").read_text())
    return {"asOf": str(df["as_of"].max()), "builtAt": meta.get("built_at"), "universe": meta.get("universe"),
            "dates": dates, "stocks": stocks, "indexes": idx}


def write_viz_data(out_dir: str | Path) -> Path:
    out = Path(out_dir) / "data.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(viz_data(), separators=(",", ":"), allow_nan=False))
    return out
