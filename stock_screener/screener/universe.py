"""Ticker universes and benchmark indexes."""

INDEXES = {
    "^GSPC": "S&P 500",
    "^IXIC": "Nasdaq Composite",
    "^DJI": "Dow Jones Industrial Average",
}

# Candidate US-listed mega-caps. The pipeline filters by live market cap
# (see MEGACAP_MIN_CAP), so this list can be generous.
MEGACAP_CANDIDATES = [
    "AAPL", "MSFT", "NVDA", "GOOGL", "AMZN", "META", "AVGO", "TSLA", "BRK-B",
    "LLY", "JPM", "V", "WMT", "ORCL", "MA", "XOM", "UNH", "COST", "NFLX",
    "JNJ", "HD", "PG", "ABBV", "BAC", "PLTR", "AMD", "KO", "CVX", "CRM",
    "TSM", "ASML", "CSCO", "IBM", "MU", "WFC", "GS", "MS", "PEP", "MRK", "TMUS",
]

MEGACAP_MIN_CAP = 200e9  # USD

UNIVERSES = {"megacap": MEGACAP_CANDIDATES}  # "sp500" is fetched live: see sp500_constituents()


SP500_URL = "https://en.wikipedia.org/wiki/List_of_S%26P_500_companies"


def sp500_constituents(data_dir, ttl_days: float = 7) -> "pd.DataFrame":
    """Current S&P 500 members (symbol, name, sector) from Wikipedia, cached for a week.
    Symbols are converted to Yahoo form (BRK.B -> BRK-B)."""
    import io
    import time
    import urllib.request

    import pandas as pd

    cache = data_dir / "sp500.csv"
    if cache.exists() and time.time() - cache.stat().st_mtime < ttl_days * 86400:
        return pd.read_csv(cache)
    req = urllib.request.Request(SP500_URL, headers={"User-Agent": "Mozilla/5.0 stock-screener/0.1"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        html = resp.read().decode("utf-8")
    t = pd.read_html(io.StringIO(html), attrs={"id": "constituents"})[0]
    df = pd.DataFrame({"symbol": t["Symbol"].str.replace(".", "-", regex=False),
                       "name": t["Security"], "sector": t["GICS Sector"]})
    data_dir.mkdir(parents=True, exist_ok=True)
    df.to_csv(cache, index=False)
    return df
