# stock_screener

Custom large-cap stock screener: 30-day / 1-year / 5-year performance, P/E (vs. its own
history), dividend yield, and correlation/beta with the most relevant index
(S&P 500, Nasdaq Composite, DJIA). Built to find strong long-term performers that are
not overpriced and have dipped *with* the market rather than on company-specific news.

Dependencies: Python 3.10+, pandas, numpy. Data: Yahoo Finance public JSON endpoints
(no API key), cached on disk for 12h in `data/cache/`.

## Usage

```bash
python -m screener refresh                 # fetch + rebuild data/snapshot.csv (mega-caps >= $200B)
python -m screener refresh --tickers AAPL,MSFT,KO --force   # custom list, bypass cache
python -m screener table                   # overview  (presets: default, perf, value, index, dip)
python -m screener table --cols value --sort pe_vs_hist --asc
python -m screener screen                  # apply criteria.json; --all to show failures too
python -m screener query "div_yield > 0.02 and cagr_5y > 0.15" --cols name,div_yield,cagr_5y
python -m screener ticker NVDA             # every metric for one stock
python -m screener columns                 # list column names
```

Every table command accepts `--csv` for machine-readable output.

## Columns

| column | meaning |
|---|---|
| `price`, `market_cap`, `high_52w` | last close, market cap (USD), 52-week high |
| `ret_30d`, `ret_1y`, `ret_5y` | total return (dividend-adjusted) over the period |
| `cagr_5y` | 5-year annualized total return |
| `drawdown_52w` | % below 52-week high |
| `eps_ttm`, `pe_ttm` | trailing-12-month diluted EPS (GAAP) and P/E |
| `pe_fwd` | forward P/E (Yahoo) - useful when GAAP EPS has one-off charges (e.g. MRK) |
| `pe_hist_median`, `pe_vs_hist` | median month-end trailing P/E over ~3 years; current / median |
| `div_ttm`, `div_yield` | dividends paid in the last 365 days, and yield |
| `corr1y_<idx>`, `corr5y_<idx>` | correlation vs each index (1y daily, 5y weekly returns) |
| `index`, `corr_1y`, `corr_5y`, `beta_1y` | best-correlated index, and stats against it |
| `idx_*` | the chosen index's own returns / drawdown |
| `excess_1y`, `excess_cagr_5y` | stock minus index |
| `market_move_30d` | `beta_1y * idx_ret_30d`: the 30-day move the market "explains" |
| `residual_30d` | `ret_30d - market_move_30d`: company-specific part of the 30-day move |

## Data handling notes

* Yahoo's EPS history is **not split-adjusted** while prices are; `metrics.split_adjust`
  detects the jump around each split and fixes it (NVDA, NFLX).
* ADRs report EPS in local currency per ordinary share (TSM, ASML). `eps_calibration`
  rescales EPS so the current P/E matches Yahoo's own figure (`eps_factor` column).
* P/E history uses EPS only once it would have been public (period end + 45 days).
* Fundamentals history from the free endpoint covers ~3 years, hence a 3-year P/E median.

## Screening criteria

`criteria.json` holds rules as pandas query expressions over the columns above. Rules
listed in `required` must pass for a ticker to appear; the rest are scored (`passed`)
and listed in `failed`. Copy the file and pass `--criteria myfile.json` to try variants.

## Layout (for turning into a skill/app)

```
screener/yahoo.py     data access + cache (swap here for another provider)
screener/metrics.py   pure metric functions
screener/universe.py  ticker universes and benchmark indexes
screener/pipeline.py  build/load snapshot (data/snapshot.csv, data/indexes.csv)
screener/screen.py    rule engine
screener/__main__.py  CLI
criteria.json         default screen
```

A skill only needs to run `refresh` once, then answer questions with `query`, `ticker`,
`screen`, or by loading `data/snapshot.csv` with pandas. An app can call
`pipeline.build()` / `pipeline.load()` and `screen.apply()` directly.

Not investment advice; data is from an unofficial source and may contain errors.
