# V 2.7 Release — V207_150_6011_R

**Web UI port:** `6011` · **Line:** PM150 (Paper Machine 2)

## Communication

| Protocol | Interval | Timeout | Role | PLC IP / Host | Port | PLC Data Type | Decode to PLC |
|---|---|---|---|---|---|---|---|
| TCP | 1s | 2s | Client | 172.16.1.73 | 8001 | ASCII `key=value;` | Send `t{temp}.0` → parse response; merge PM250 via `:6010/api/settings/` |

## Database Structure

```mermaid
flowchart TD
    TCP_CONNECTION
    PLC --> Rolls
    PLC --> PLC_Logs
    Rolls --> PLC_Logs
    Rolls --> Roll_Breaks
    PLC_Keys
    ChartExcludedKeys
    KeyAlertConfig
    VersionControl
```

## How It Works

PM150 TCP client with PM250 cross-sync and Redis session cache. Web on **6011**.
