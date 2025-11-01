# Central Backup System V1R

A centralized backup solution for automatically backing up SQLite databases from multiple remote devices via SSH. Features a modern web dashboard for monitoring backups, configuring settings, and managing automated backup schedules with external USB storage support.

## Features

- **Automated SQLite Database Backups** - Automatically finds and backs up `.sqlite3` files from remote devices
- **Multi-Device Support** - Backup from multiple remote devices (local and SSH-based)
- **External USB Storage** - Automatic detection and use of USB drives for backup storage
- **Web Dashboard** - Modern, responsive interface for monitoring backups and logs
- **Scheduled Backups** - Configurable automatic backup intervals
- **Persian/Jalali Calendar** - Timestamps displayed in Jalali calendar format
- **Backup Retention** - Automatic cleanup of old backups based on retention policy
- **Real-time Monitoring** - Live countdown to next scheduled backup
- **Web-based Configuration** - Easy configuration management through web interface

## Project Structure

```
CentralBackupSystem/V1R/
├── app.py                         # Flask web application
│   ├── Web dashboard routes
│   ├── Configuration management API
│   ├── Manual backup trigger
│   └── Automated backup scheduler
├── backup.py                      # Core backup engine
│   ├── SQLite database discovery
│   ├── Remote SSH backup operations
│   ├── Local backup operations
│   ├── External storage detection
│   ├── Backup retention/cleanup
│   └── rsync-based file transfers
├── config.json                    # Configuration file
│   ├── SSH credentials
│   ├── Remote device list
│   ├── Backup intervals & retention
│   ├── Directory paths
│   └── External storage settings
├── templates/                     # HTML templates
│   ├── index.html                # Main dashboard
│   │   ├── Backup statistics
│   │   ├── System information
│   │   ├── Backup file browser (tabbed by device)
│   │   └── Recent logs viewer
│   └── config.html               # Configuration page
│       ├── Backup schedule settings
│       ├── External storage settings
│       ├── Remote device management
│       ├── Path configuration
│       └── SSH credentials
├── backups/                       # Local backup storage (created at runtime)
│   └── {device_ip}/              # Per-device backup directories
├── backup.log                     # Backup operation logs (created at runtime)
└── last_backup.json              # Last backup timestamp (created at runtime)
```

## Setup Instructions

### 1. Navigate to Project Directory

```bash
cd RnD\CentralBackupSystem\V1R
```

### 2. Install System Dependencies (Linux/Ubuntu)

```bash
sudo apt update
```

```bash
sudo apt install -y python3-pip sshpass rsync
```

### 3. Create Virtual Environment

```bash
python3 -m venv venv
```

### 4. Activate Virtual Environment

**Linux/Mac:**
```bash
source venv/bin/activate
```

**Windows:**
```bash
venv\Scripts\activate
```

### 5. Install Python Dependencies

```bash
pip install -r requirements.txt
```

### 6. Configure Settings

Edit `config.json` to set your environment:

```bash
nano config.json
```

Update the following settings:
- `remote_user` - SSH username for remote devices
- `ssh_pass` - SSH password (use with caution)
- `remote_devices` - List of device IP addresses to backup
- `interval_hours` - Backup frequency (default: 24 hours)
- `retention_days` - How long to keep old backups (default: 7 days)
- `enable_external_backup` - Enable USB drive backup (default: true)
- `external_storage_path` - Base path for USB detection (e.g., `/media/admin`)

### 7. Run Backup System

```bash
python3 app.py
```

The web dashboard will be available at: `http://0.0.0.0:6003/`

Access from other devices: `http://<your-ip>:6003/`

### 8. Run Manual Backup (Optional)

To run a one-time backup without starting the web server:

```bash
python3 backup.py
```

## How It Works

### Backup Process

1. **Discovery** - Scans configured devices for `.sqlite3` files in specified directories
2. **Backup Creation** - Creates integrity-checked backups using SQLite's backup API
3. **Transfer** - For remote devices, uses rsync over SSH to pull backups locally
4. **Storage** - Saves to USB drive (if available) or local directory
5. **Cleanup** - Removes backups older than retention period

### External Storage

- Automatically detects USB drives mounted at `external_storage_path`
- Tests write permissions before using external storage
- Falls back to local storage if USB unavailable
- All backups go to external storage when USB is connected

### Device Detection

- Automatically detects if a configured IP is the localhost
- Local backups are created directly (no SSH)
- Remote backups use SSH with sshpass authentication

## Configuration Options

| Setting | Description | Default |
|---------|-------------|---------|
| `remote_user` | SSH username | `username` |
| `ssh_pass` | SSH password | `password` |
| `remote_devices` | Array of device IPs | `[]` |
| `remote_search_dir` | Directory to search for databases | `/home/admin` |
| `remote_backup_dir` | Remote backup staging directory | `/home/admin/remote-backups` |
| `backup_system_dir` | Directory to exclude from search | `/home/admin/backup-system` |
| `local_backup_base` | Local backup directory | `backups` |
| `log_file` | Log file path | `backup.log` |
| `retention_days` | Days to keep backups | `7` |
| `interval_hours` | Hours between backups | `24` |
| `enable_external_backup` | Use USB storage | `true` |
| `external_storage_path` | USB mount base path | `/media/admin/` |
| `external_backup_dir` | Subdirectory on USB | `backup-system` |

## Web Dashboard Features

### Main Dashboard (`/`)
- Real-time backup statistics
- Countdown to next scheduled backup
- System information (hostname, IP, storage status)
- Tabbed backup browser by device
- Live log viewer (last 50 entries)
- Manual backup trigger button

### Configuration Page (`/config`)
- Web-based configuration editor
- Device management (add/remove)
- Schedule and retention settings
- External storage configuration
- SSH credential management

## API Endpoints

- `POST /run-backup` - Trigger manual backup
- `GET /config` - Configuration page
- `GET /api/config` - Get current configuration (JSON)
- `POST /api/config` - Update configuration (JSON)
- `GET /api/backup-status` - Get backup timing information

## Security Notes

⚠️ **Important Security Considerations:**
- SSH passwords are stored in plain text in `config.json`
- Use SSH keys instead for production environments
- Restrict access to the web dashboard (runs on all interfaces)
- Consider using a reverse proxy with authentication
- Keep `config.json` permissions restricted (`chmod 600 config.json`)

## Troubleshooting

### SSH Connection Fails
- Verify `sshpass` is installed: `which sshpass`
- Test SSH manually: `sshpass -p 'password' ssh user@host`
- Check firewall rules on remote devices

### External Storage Not Detected
- Verify USB drive is mounted: `ls /media/admin/`
- Check write permissions: `touch /media/admin/test`
- Review logs for detection errors

### Backups Not Running
- Check logs: `tail -f backup.log`
- Verify cron/scheduler is running
- Check `last_backup.json` for timestamp

## Running as a Service (Linux)

Create a systemd service file: `/etc/systemd/system/backup-system.service`

```ini
[Unit]
Description=Central Backup System
After=network.target

[Service]
Type=simple
User=admin
WorkingDirectory=/home/admin/backup-system
ExecStart=/home/admin/backup-system/venv/bin/python3 /home/admin/backup-system/app.py
Restart=always

[Install]
WantedBy=multi-user.target
```

Enable and start the service:

```bash
sudo systemctl daemon-reload
```

```bash
sudo systemctl enable backup-system.service
```

```bash
sudo systemctl start backup-system.service
```

Check status:

```bash
sudo systemctl status backup-system.service
```

## License

[Specify your license here]

## Author

[Your name/organization]

