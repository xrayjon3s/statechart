"""CLI: python -m screener <command> ...

  refresh [--tickers A,B | --add A,B] [--universe megacap] [--min-cap 2e11] [--force]
  table   [--sort COL] [--asc] [--cols preset|a,b,c] [--csv]
  screen  [--criteria FILE] [--all] [--csv]
  query   "<pandas expr>" [--sort COL] [--cols ...] [--csv]
  ticker  SYM
  columns
  export-viz [OUT_DIR]
"""
from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import pandas as pd

from . import pipeline, screen

PCT = {"ret_30d", "ret_1y", "ret_5y", "cagr_5y", "drawdown_52w", "div_yield", "excess_1y",
       "excess_cagr_5y", "market_move_30d", "residual_30d", "idx_ret_30d", "idx_ret_1y",
       "idx_ret_5y", "idx_cagr_5y", "idx_drawdown_52w"}

PRESETS = {
    "default": ["name", "price", "market_cap", "ret_30d", "ret_1y", "ret_5y", "cagr_5y",
                "pe_ttm", "pe_fwd", "pe_vs_hist", "div_yield", "index", "corr_1y", "beta_1y"],
    "perf": ["ret_30d", "idx_ret_30d", "ret_1y", "idx_ret_1y", "excess_1y", "cagr_5y",
             "idx_cagr_5y", "excess_cagr_5y", "drawdown_52w"],
    "value": ["price", "eps_ttm", "pe_ttm", "pe_fwd", "pe_hist_median", "pe_vs_hist",
              "div_ttm", "div_yield"],
    "index": ["index", "corr1y_^GSPC", "corr1y_^IXIC", "corr1y_^DJI", "corr_5y", "beta_1y",
              "ret_30d", "market_move_30d", "residual_30d"],
    "dip": ["ret_30d", "drawdown_52w", "index", "idx_ret_30d", "idx_drawdown_52w", "beta_1y",
            "market_move_30d", "residual_30d", "pe_vs_hist"],
}


def fmt(col: str, v) -> str:
    if v is None or (isinstance(v, float) and math.isnan(v)):
        return "-"
    if isinstance(v, (bool,)) or str(v) in ("True", "False"):
        return "Y" if str(v) == "True" else "."
    if col in PCT:
        return f"{v * 100:+.1f}%" if col != "div_yield" else f"{v * 100:.2f}%"
    if col == "market_cap":
        return f"{v / 1e12:.2f}T" if v >= 1e12 else f"{v / 1e9:.0f}B"
    if col == "name":
        return str(v)[:22]
    if isinstance(v, float):
        return f"{v:.2f}"
    return str(v)


def render(df: pd.DataFrame, cols: list[str]) -> str:
    cols = [c for c in cols if c in df.columns]
    table = [["symbol", *cols]] + [[s, *(fmt(c, r[c]) for c in cols)] for s, r in df.iterrows()]
    widths = [max(len(row[i]) for row in table) for i in range(len(table[0]))]
    lines = ["  ".join(cell.ljust(w) if i < 2 else cell.rjust(w)
                       for i, (cell, w) in enumerate(zip(row, widths))) for row in table]
    lines.insert(1, "-" * len(lines[0]))
    return "\n".join(lines)


def parse_cols(spec: str | None, default: str = "default") -> list[str]:
    spec = spec or default
    return PRESETS[spec] if spec in PRESETS else [c.strip() for c in spec.split(",")]


def output(df: pd.DataFrame, cols: list[str], as_csv: bool) -> None:
    if as_csv:
        df[[c for c in cols if c in df.columns]].to_csv(sys.stdout)
    else:
        print(render(df, cols))


def main(argv: list[str] | None = None) -> None:
    p = argparse.ArgumentParser(prog="screener", description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    r = sub.add_parser("refresh", help="fetch data and rebuild the snapshot")
    r.add_argument("--tickers", help="replace the universe with these tickers")
    r.add_argument("--add", help="add these tickers to the universe (exempt from --min-cap)")
    r.add_argument("--universe", default="megacap", choices=["megacap", "sp500"])
    r.add_argument("--min-cap", type=float); r.add_argument("--force", action="store_true")

    for name in ("table", "query", "screen"):
        s = sub.add_parser(name)
        if name == "query":
            s.add_argument("expr")
        if name == "screen":
            s.add_argument("--criteria"); s.add_argument("--all", action="store_true")
        s.add_argument("--sort"); s.add_argument("--asc", action="store_true")
        s.add_argument("--cols"); s.add_argument("--csv", action="store_true")

    t = sub.add_parser("ticker"); t.add_argument("symbol")
    sub.add_parser("columns")
    e = sub.add_parser("export-viz", help="write data.json for the charts in viz/")
    e.add_argument("out_dir", nargs="?", default=str(Path(__file__).resolve().parent.parent / "viz"))
    a = p.parse_args(argv)

    if a.cmd == "export-viz":
        from .export import write_viz_data
        out = write_viz_data(a.out_dir)
        print(f"wrote {out} ({out.stat().st_size / 1024:.0f} KB)")
        return

    if a.cmd == "refresh":
        syms = [x.strip().upper() for x in a.tickers.split(",")] if a.tickers else None
        extra = [x.strip().upper() for x in a.add.split(",")] if a.add else None
        df = pipeline.build(syms, a.universe, a.min_cap, ttl=0 if a.force else pipeline.yahoo.DEFAULT_TTL,
                            extra=extra)
        print(f"snapshot: {len(df)} tickers -> {pipeline.SNAPSHOT}")
        print(render(pipeline.load_indexes(), ["name", "price", "ret_30d", "ret_1y", "cagr_5y", "drawdown_52w"]))
        return

    df = pipeline.load()
    if a.cmd == "columns":
        print("\n".join(df.columns)); return
    if a.cmd == "ticker":
        row = df.loc[a.symbol.upper()]
        for c, v in row.items():
            print(f"{c:18} {fmt(c, v)}")
        return

    cols = parse_cols(a.cols)
    if a.cmd == "query":
        df = df.query(a.expr, engine="python")
    if a.cmd == "screen":
        crit = screen.load_criteria(a.criteria)
        df = screen.apply(df, crit)
        if not a.all:
            df = df[df["required_ok"]]
        print(crit["description"])
        for rule in crit["rules"]:
            req = "*" if rule["name"] in crit.get("required", []) else " "
            print(f" {req} {rule['name']:14} {rule['why']}")
        print("   (* = required to be listed; use --all to show every ticker)\n")
        cols = parse_cols(a.cols, "dip") if a.cols else \
            ["price", "ret_1y", "cagr_5y", "pe_ttm", "pe_vs_hist", "div_yield", "drawdown_52w",
             "ret_30d", "index", "residual_30d", "passed", "failed"]
    if a.sort:
        df = df.sort_values(a.sort, ascending=a.asc)
    output(df, cols, a.csv)


if __name__ == "__main__":
    main()
