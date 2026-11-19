"""Per-ticker metric computation. Pure functions over pandas data from yahoo.py."""
from __future__ import annotations

import math

import numpy as np
import pandas as pd

from .universe import INDEXES

EPS_REPORT_LAG_DAYS = 45  # quarter-end -> earnings published; avoids look-ahead in P/E history


def value_on_or_before(s: pd.Series, when: pd.Timestamp) -> float:
    s = s.loc[:when].dropna()
    return float(s.iloc[-1]) if len(s) else math.nan


def period_return(px: pd.Series, days: int) -> float:
    """Return over `days` calendar days ending at the last observation."""
    end = px.index[-1]
    start = end - pd.Timedelta(days=days)
    if px.index[0] > start + pd.Timedelta(days=7):  # not enough history
        return math.nan
    p0 = value_on_or_before(px, start)
    return float(px.iloc[-1] / p0 - 1)


def cagr(total_return: float, years: float) -> float:
    if math.isnan(total_return) or total_return <= -1:
        return math.nan
    return (1 + total_return) ** (1 / years) - 1


def drawdown_from_high(px: pd.Series, days: int = 365) -> float:
    window = px.loc[px.index[-1] - pd.Timedelta(days=days):]
    return float(px.iloc[-1] / window.max() - 1)


def corr_beta(stock: pd.Series, index: pd.Series, days: int = 365, freq: str | None = None) -> tuple[float, float]:
    """Correlation and beta of returns over the trailing window. freq='W-FRI' for weekly."""
    start = stock.index[-1] - pd.Timedelta(days=days)
    df = pd.concat([stock, index], axis=1, join="inner").loc[start:]
    if freq:
        df = df.resample(freq).last()
    r = df.pct_change().dropna()
    if len(r) < 20:
        return math.nan, math.nan
    c = float(r.iloc[:, 0].corr(r.iloc[:, 1]))
    b = float(r.iloc[:, 0].cov(r.iloc[:, 1]) / r.iloc[:, 1].var())
    return c, b


def split_adjust(eps: pd.Series, splits: list[tuple[pd.Timestamp, float]]) -> pd.Series:
    """Yahoo's EPS history is not split-adjusted (prices are). For each split of ratio k,
    find the consecutive EPS pair near the split date that jumps by ~k and divide the
    older values by k."""
    eps = eps.copy()
    for when, k in splits:
        if k == 1 or eps.empty:
            continue
        near = [i for i in range(1, len(eps))
                if abs((eps.index[i] - when).days) <= 400 and eps.iloc[i] != 0]
        for i in near:
            jump = eps.iloc[i - 1] / eps.iloc[i]
            if 0.5 * k <= jump <= 2 * k:
                eps.iloc[:i] = eps.iloc[:i] / k
                break
    return eps


def ttm_eps_series(fund: dict[str, pd.Series], splits=()) -> pd.Series:
    """TTM diluted EPS (split-adjusted), indexed by the date it became public (period end + lag)."""
    ttm = fund.get("trailingDilutedEPS", pd.Series(dtype=float))
    q = fund.get("quarterlyDilutedEPS", pd.Series(dtype=float))
    if len(q) >= 4:  # fill in any quarters newer than the TTM series
        rolled = q.rolling(4).sum().dropna()
        ttm = pd.concat([ttm, rolled[~rolled.index.isin(ttm.index)]]).sort_index()
    ttm = split_adjust(ttm, list(splits))
    ttm.index = ttm.index + pd.Timedelta(days=EPS_REPORT_LAG_DAYS)
    return ttm


def eps_calibration(price: float, eps_now: float, yahoo_pe: pd.Series, as_of: pd.Timestamp) -> float:
    """Factor converting reported EPS into price-currency, per-listed-share EPS.

    ADRs (e.g. TSM, ASML) report EPS in local currency per ordinary share. Yahoo's own
    trailing P/E is in consistent units, so if it's recent and disagrees by >20% we
    rescale EPS to match it. Returns 1.0 when no correction is needed."""
    if not len(yahoo_pe) or not eps_now or eps_now <= 0:
        return 1.0
    ypx_date, ype = yahoo_pe.index[-1], float(yahoo_pe.iloc[-1])
    if ype <= 0 or (as_of - ypx_date).days > 21:
        return 1.0
    factor = (price / ype) / eps_now
    return factor if abs(math.log(factor)) > math.log(1.2) else 1.0


def pe_history(close: pd.Series, ttm_eps: pd.Series) -> pd.Series:
    """Month-end trailing P/E using only EPS public as of each date. Negative EPS -> NaN."""
    if ttm_eps.empty:
        return pd.Series(dtype=float)
    monthly = close.resample("ME").last().loc[ttm_eps.index[0]:]
    eps = ttm_eps.reindex(monthly.index.union(ttm_eps.index)).ffill().reindex(monthly.index)
    pe = monthly / eps
    return pe[eps > 0].dropna()


def compute(symbol: str, prices: pd.DataFrame, divs: pd.Series, meta: dict,
            fund: dict[str, pd.Series], index_adj: dict[str, pd.Series],
            index_metrics: dict[str, dict]) -> dict:
    close, adj = prices["close"], prices["adjclose"]
    price = float(close.iloc[-1])
    row: dict = {
        "symbol": symbol,
        "name": meta.get("longName") or meta.get("shortName") or symbol,
        "exchange": meta.get("fullExchangeName", ""),
        "currency": meta.get("currency", ""),
        "as_of": close.index[-1].date().isoformat(),
        "price": price,
        "high_52w": float(close.loc[close.index[-1] - pd.Timedelta(days=365):].max()),
    }
    mcap = fund.get("trailingMarketCap", pd.Series(dtype=float))
    row["market_cap"] = float(mcap.iloc[-1]) if len(mcap) else math.nan

    # Total returns (adjusted close includes dividends)
    row["ret_30d"] = period_return(adj, 30)
    row["ret_1y"] = period_return(adj, 365)
    row["ret_5y"] = period_return(adj, int(5 * 365.25))
    row["cagr_5y"] = cagr(row["ret_5y"], 5)
    row["drawdown_52w"] = drawdown_from_high(close)

    # Valuation
    ttm = ttm_eps_series(fund, meta.get("splits", ()))
    eps_now = float(ttm.iloc[-1]) if len(ttm) else math.nan
    row["eps_factor"] = eps_calibration(price, eps_now, fund.get("trailingPeRatio", pd.Series(dtype=float)),
                                        close.index[-1])
    ttm = ttm * row["eps_factor"]
    eps_now *= row["eps_factor"]
    row["eps_ttm"] = eps_now
    row["pe_ttm"] = price / eps_now if eps_now and eps_now > 0 else math.nan
    fpe = fund.get("trailingForwardPeRatio", pd.Series(dtype=float))
    row["pe_fwd"] = float(fpe.iloc[-1]) if len(fpe) else math.nan
    hist = pe_history(close, ttm)
    row["pe_hist_median"] = float(hist.median()) if len(hist) >= 6 else math.nan
    row["pe_hist_months"] = int(len(hist))
    row["pe_vs_hist"] = row["pe_ttm"] / row["pe_hist_median"] if row["pe_hist_median"] else math.nan

    # Dividends
    d12 = divs.loc[close.index[-1] - pd.Timedelta(days=365):].sum() if len(divs) else 0.0
    row["div_ttm"] = float(d12)
    row["div_yield"] = float(d12) / price

    # Index relationships
    best, best_c = None, -2.0
    for isym in INDEXES:
        c1, b1 = corr_beta(adj, index_adj[isym], 365)
        c5, _ = corr_beta(adj, index_adj[isym], int(5 * 365.25), "W-FRI")
        row[f"corr1y_{isym}"] = c1
        row[f"corr5y_{isym}"] = c5
        if not math.isnan(c1) and c1 > best_c:
            best, best_c = isym, c1
    if best is None:
        raise ValueError(f"not enough price history from Yahoo ({len(adj)} days) to compare with an index")
    row["index"] = best
    row["index_name"] = INDEXES.get(best, "")
    row["corr_1y"] = best_c
    row["corr_5y"] = row[f"corr5y_{best}"]
    _, row["beta_1y"] = corr_beta(adj, index_adj[best], 365)

    im = index_metrics[best]
    for k in ("ret_30d", "ret_1y", "ret_5y", "cagr_5y", "drawdown_52w"):
        row[f"idx_{k}"] = im[k]
    row["excess_1y"] = row["ret_1y"] - im["ret_1y"]
    row["excess_cagr_5y"] = row["cagr_5y"] - im["cagr_5y"]
    # How much of the last 30 days is explained by the market (beta * index move)?
    row["market_move_30d"] = row["beta_1y"] * im["ret_30d"]
    row["residual_30d"] = row["ret_30d"] - row["market_move_30d"]
    return row


def index_metrics(adj: pd.Series) -> dict:
    r5 = period_return(adj, int(5 * 365.25))
    return {"ret_30d": period_return(adj, 30), "ret_1y": period_return(adj, 365),
            "ret_5y": r5, "cagr_5y": cagr(r5, 5), "drawdown_52w": drawdown_from_high(adj)}
