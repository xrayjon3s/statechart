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

UNIVERSES = {"megacap": MEGACAP_CANDIDATES}
