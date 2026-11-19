---
name: stock-screener
description: Screen and compare large-cap US-listed stocks on 30-day, 1-year and 5-year total return, trailing/forward P/E (including P/E vs the stock's own ~3-year history), dividend yield, and correlation/beta with the S&P 500, Nasdaq or Dow. Finds buy-the-dip candidates — strong long-term performers, not overpriced, that pulled back with the market rather than on company news. Use this whenever the user asks about stock performance, valuations, P/E ratios, dividends, which stocks have dipped, how a stock tracks an index, mega-caps / large caps / big tech, or wants to screen, rank, filter or compare stocks — even if they don't say "screen" (e.g. "is MSFT cheap right now?", "which big banks yield over 2%?", "what's dropped more than the market this month?").
compatibility: Python 3.10+ with pandas and numpy. Needs HTTPS access to query1.finance.yahoo.com (no API key).
---

# Stock screener

A small Python package (in `screener/` next to this file) that pulls historical prices and
fundamentals from Yahoo Finance's public endpoints, computes a per-stock metrics table, and
applies rule-based screens. All the numbers come from code, so answers are reproducible —
prefer running the tools over reasoning about prices from memory, which will be stale.

## Running it

Run commands from this skill's directory (the folder containing this SKILL.md):

```bash
cd <skill-dir> && python -m screener <command>
```

1. **Refresh first** unless a snapshot from today already exists (`refresh` prints where it
   wrote; `data/snapshot_meta.json` or `~/.cache/stock-screener/snapshot_meta.json` has
   `built_at`). A refresh of ~40 tickers takes ~10s and caches HTTP responses for 12h.
   ```bash
   python -m screener refresh                    # mega-caps (>= $200B market cap)
   python -m screener refresh --add NKE,SBUX     # mega-caps plus extra tickers
   python -m screener refresh --tickers KO,PEP   # only these tickers
   python -m screener refresh --force            # ignore the cache
   ```
   `--tickers` replaces the snapshot, so if the user asks about a stock that isn't in the
   current table, use `--add` to keep the rest.
   If the fetch fails with a 403 / tunnel error, the network blocks Yahoo: tell the user the
   host `query1.finance.yahoo.com` needs to be allowed rather than guessing numbers.

2. **Answer the question** with whichever command fits:
   ```bash
   python -m screener table [--cols PRESET|a,b,c] [--sort COL] [--asc]
   python -m screener screen [--all] [--criteria FILE]
   python -m screener query "<pandas expression>" [--cols ...] [--sort COL]
   python -m screener ticker SYM
   python -m screener columns
   ```
   Column presets: `default`, `perf`, `value`, `index`, `dip`. Add `--csv` to any table
   command for machine-readable output. For anything the CLI can't express (ranking by a
   custom score, group stats, charts), load the snapshot in pandas:
   ```python
   import sys; sys.path.insert(0, "<skill-dir>")
   from screener import pipeline, screen
   df = pipeline.load()                      # one row per ticker, index = symbol
   idx = pipeline.load_indexes()             # ^GSPC, ^IXIC, ^DJI
   out = screen.apply(df, screen.load_criteria())
   ```
   Query expressions are pandas `DataFrame.query` syntax over column names, e.g.
   `"pe_ttm < 25 and cagr_5y > idx_cagr_5y"` or `"symbol in ['AAPL','MSFT']"`.
   Column names containing `^` (like `corr1y_^GSPC`) need backticks inside a query.

## Key columns

Returns are fractions (0.12 = +12%) and include dividends. Full glossary in `README.md`.

- `ret_30d`, `ret_1y`, `ret_5y`, `cagr_5y`, `drawdown_52w` — performance and % off 52-week high
- `pe_ttm` (GAAP trailing), `pe_fwd`, `pe_hist_median`, `pe_vs_hist` (current / ~3y median)
- `div_yield`, `div_ttm`
- `index` — the best-correlated of S&P 500 / Nasdaq / Dow; `corr_1y`, `corr_5y`, `beta_1y`
  against it, plus `corr1y_<idx>` / `corr5y_<idx>` for all three
- `idx_*` — that index's own returns; `excess_1y`, `excess_cagr_5y` — stock minus index
- `market_move_30d` = beta × index 30-day return; `residual_30d` = the company-specific part.
  This is how "dipped with the market" is measured: a negative `ret_30d` with `residual_30d`
  near zero means the market explains the drop; a large negative residual means something
  stock-specific happened.

## The default screen

`criteria.json` encodes the user's goal: good 1-year and 5-year performance, not overpriced,
and dipped mostly because the market did. Rules are pandas expressions; `required` rules must
pass to be listed, the rest are counted in `passed` and named in `failed`. When the user wants
different thresholds, write a modified copy to a writable location (e.g. the scratchpad) and
pass `--criteria path` rather than editing the original — the user's defaults stay intact.
If they want a change to stick, edit `criteria.json` and say so.

## Interpreting and answering

- Lead with the direct answer and a compact table of the relevant tickers and columns, then
  any caveats that actually affect the conclusion. State the snapshot's as-of date.
- **GAAP P/E can be distorted by one-off charges** (e.g. a large acquisition write-off makes
  `pe_ttm` balloon while `pe_fwd` stays normal). When `pe_ttm` and `pe_fwd` disagree by more
  than ~2×, mention both rather than calling the stock expensive or cheap on one number.
- `pe_vs_hist` compares against only ~3 years of history (all the free endpoint provides), so
  treat it as "cheap/expensive relative to its recent self", not a long-run valuation.
- Low correlation with every index (e.g. energy, staples, health care) makes beta and the
  market-adjusted residual less meaningful — say so when it matters.
- Negative or missing EPS gives `pe_ttm` = NaN (shown as `-`); that's a loss, not missing data.
- The code already corrects two Yahoo data quirks: EPS history not adjusted for stock splits,
  and ADRs reporting EPS in local currency (`eps_factor` ≠ 1 marks a rescaled ticker). If a
  P/E still looks absurd, cross-check `pe_ttm` against `pe_fwd` and `pe_hist_median`.
- This is a screening aid built on an unofficial data source, not investment advice; say that
  briefly when giving buy/sell-flavoured conclusions, without lecturing.

## Extending

- Universe and benchmark indexes: `screener/universe.py`.
- New metrics: add to `metrics.compute()` (pure pandas); they appear as columns automatically.
- Different data provider: replace `screener/yahoo.py`, keeping `history()` and
  `fundamentals()` return shapes.
