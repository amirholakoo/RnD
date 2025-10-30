# Server Health Monitor Web Application

A simple web-based server health monitoring tool that provides real-time status updates for multiple servers.

## Features

- **Real-time monitoring**: Automatically checks server health every 10 seconds
- **Clean web interface**: Simple, responsive design that works on desktop and mobile
- **Auto-refresh**: Updates status without page reload
- **Same servers as bash script**: Monitors the exact same servers as `server_test.sh`

## Installation as System Service

### Automated Installation

1. **Run the installation script** (requires sudo):
   ```bash
   sudo ./install.sh
   ```
   
   The script will:
   - Ask for a port number (default: 8080)
   - Create a secure service user
   - Install files to `/opt/server_test_web`
   - Register as a systemd service named `server_test_web`
   - Optionally start the service

2. **Access the web interface**:
   ```
   http://localhost:[PORT]
   ```

### Manual Development Mode

1. **Start the server**:
   ```bash
   ./start_server.sh
   ```
   
   Or directly:
   ```bash
   python3 server_monitor.py
   ```

2. **Open your browser** and go to:
   ```
   http://localhost:8080
   ```

3. **Stop the server**: Press `Ctrl+C` in the terminal

## Service Management

Once installed as a system service, use these commands:

```bash
# Start the service
sudo systemctl start server_test_web

# Stop the service
sudo systemctl stop server_test_web

# Restart the service
sudo systemctl restart server_test_web

# Check service status
sudo systemctl status server_test_web

# View logs
sudo journalctl -u server_test_web -f

# Enable auto-start on boot
sudo systemctl enable server_test_web

# Disable auto-start on boot
sudo systemctl disable server_test_web
```

## Uninstallation

To remove the service and all files:

```bash
sudo ./uninstall.sh
```

## Monitored Servers

The web application monitors the same servers as the original bash script:

- Lab Admin
- Lab React App
- Loading/Unloading Service
- DHT22 Thermal Cam data logger Server/Dashboard
- TOTAL monitor Server/Dashboard
- Thermal Camera Frontend Server
- Thermal Camera Temperature

## API Endpoint

The application also provides a JSON API endpoint:
```
GET /api/status
```

Returns a JSON array with the current status of all servers.

## Configuration

To modify the monitored servers, edit the `SERVER_NAMES` and `SERVER_URLS` arrays in `server_monitor.py`.

## Requirements

- Python 3.x
- No additional packages required (uses only standard library)