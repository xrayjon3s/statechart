"""Rule-based screening. Rules are pandas query expressions over snapshot columns."""
from __future__ import annotations

import json
from pathlib import Path

import pandas as pd

DEFAULT_CRITERIA = Path(__file__).resolve().parent.parent / "criteria.json"


def load_criteria(path: str | Path | None = None) -> dict:
    return json.loads(Path(path or DEFAULT_CRITERIA).read_text())


def rule_mask(df: pd.DataFrame, expr: str) -> pd.Series:
    """Boolean mask for a pandas query expression; NaN comparisons evaluate False."""
    hits = df.query(expr, engine="python").index
    return pd.Series(df.index.isin(hits), index=df.index)


def apply(df: pd.DataFrame, criteria: dict) -> pd.DataFrame:
    """Adds one bool column per rule, plus `passed` (count), `required_ok` and `all_ok`."""
    out = df.copy()
    names = []
    for rule in criteria["rules"]:
        out[f"ok_{rule['name']}"] = rule_mask(df, rule["expr"])
        names.append(rule["name"])
    ok = out[[f"ok_{n}" for n in names]]
    out["passed"] = ok.sum(axis=1)
    req = criteria.get("required", [])
    out["required_ok"] = out[[f"ok_{n}" for n in req]].all(axis=1) if req else True
    out["all_ok"] = ok.all(axis=1)
    out["failed"] = ok.apply(lambda r: ",".join(n for n, v in zip(names, r) if not v), axis=1)
    return out.sort_values(["all_ok", "required_ok", "passed", "excess_cagr_5y"], ascending=False)
