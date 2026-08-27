# V 2.4 Release — V204_250_6013_R

**Web UI port:** `6013` · **Line:** PM250 Main (Paper Machine 3)

## Communication

| Protocol | Interval | Timeout | Role | PLC IP / Host | Port | PLC Data Type | Decode to PLC |
|---|---|---|---|---|---|---|---|
| OPC UA | 10s | — | Client | 172.16.1.175 | 4840 | OPC node values | Read node IDs from `PLC_Keys` → store as JSON log |

## Database Structure

```mermaid
flowchart TD
    TCP_CONNECTION
    PLC --> Rolls
    PLC --> PLC_Logs
    Rolls --> PLC_Logs
    Rolls --> Roll_Breaks
    LiveSettingsGroup --> PLC_Keys
    ChartExcludedKeys
    KeyAlertConfig
    VersionControl
```

## How It Works

OPC-based PM250 main line with connection health tracking and grouped live settings. Web on **6013**.
