#!/usr/bin/env python3
from flask import Flask, render_template, redirect, url_for, request, jsonify
import os, time, json, jdatetime, threading, logging, socket, subprocess

app = Flask(__name__)

CONFIG_FILE = "config.json"
BACKUP_SCRIPT = "backup.py"
LAST_BACKUP_FILE = "last_backup.json"

def load_config():
    """Load configuration from JSON file"""
    try:
        if os.path.exists(CONFIG_FILE):
            with open(CONFIG_FILE, 'r') as f:
                return json.load(f)
    except:
        pass
    return {}

def save_config(config_data):
    """Save configuration to JSON file"""
    try:
        with open(CONFIG_FILE, 'w') as f:
            json.dump(config_data, f, indent=2)
        return True
    except Exception as e:
        logging.error(f"Failed to save config: {e}")
        return False

def get_config_value(key, default=None):
    """Get a configuration value"""
    config = load_config()
    return config.get(key, default)

# Import after defining helper functions
try:
    from backup import REMOTE_DEVICES, LOCAL_BACKUP_BASE, ENABLE_EXTERNAL_BACKUP, EXTERNAL_STORAGE_PATH, is_external_storage_available, get_external_storage_path, load_config as reload_backup_config
except ImportError:
    REMOTE_DEVICES = []
    LOCAL_BACKUP_BASE = "backups"
    ENABLE_EXTERNAL_BACKUP = False
    EXTERNAL_STORAGE_PATH = ""
    is_external_storage_available = lambda: False
    get_external_storage_path = lambda: None
    reload_backup_config = lambda: {}

LOG_FILE = get_config_value("log_file", "backup.log")

@app.route("/")
def index():
    logs = []
    if os.path.exists(LOG_FILE):
        with open(LOG_FILE, "r") as f:
            logs = f.readlines()[-50:]
    
    # Get local IP to determine localhost device
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
    except:
        local_ip = "127.0.0.1"
    
    # Read backups from appropriate locations
    # If external storage is available, ALL backups are there
    # Otherwise, ALL backups are in local folder
    backups_by_device = {}
    external_path = get_external_storage_path()
    
    if external_path:
        # External storage available - read ALL backups from external
        from backup import EXTERNAL_BACKUP_DIR
        backup_base = os.path.join(external_path, EXTERNAL_BACKUP_DIR)
    else:
        # No external storage - read ALL backups from local
        backup_base = LOCAL_BACKUP_BASE
    
    if os.path.exists(backup_base):
        for device in os.listdir(backup_base):
            device_dir = os.path.join(backup_base, device)
            if os.path.isdir(device_dir):
                backups_by_device[device] = []
                for f in sorted(os.listdir(device_dir), reverse=True):
                    full = os.path.join(device_dir, f)
                    if os.path.isfile(full):
                        backups_by_device[device].append({
                            "name": f,
                            "size": round(os.path.getsize(full) / 1024 / 1024, 2),
                            "time": jdatetime.datetime.fromtimestamp(os.path.getmtime(full)).strftime("%Y-%m-%d %H:%M")
                        })
    
    system_info = {
        "hostname": socket.gethostname(), 
        "local_ip": local_ip, 
        "devices": REMOTE_DEVICES,
        "external_storage_enabled": ENABLE_EXTERNAL_BACKUP,
        "external_storage_path": external_path if external_path else EXTERNAL_STORAGE_PATH,
        "external_storage_available": bool(external_path)
    }
    
    return render_template("index.html", logs=logs, backups_by_device=backups_by_device, system_info=system_info)

@app.route("/run-backup", methods=["POST"])
def run_backup():
    # Manual backup script execution
    def run_and_update():
        subprocess.run(["python3", BACKUP_SCRIPT])
        # Update last backup timestamp
        with open(LAST_BACKUP_FILE, "w") as f:
            json.dump({"last_backup": time.time()}, f)
    
    threading.Thread(target=run_and_update, daemon=True).start()
    time.sleep(2)
    return redirect(url_for("index"))

@app.route("/config")
def config_page():
    """Configuration page"""
    config = load_config()
    return render_template("config.html", config=config)

@app.route("/api/config", methods=["GET"])
def get_config():
    """API endpoint to get configuration"""
    config = load_config()
    return jsonify(config)

@app.route("/api/config", methods=["POST"])
def update_config():
    """API endpoint to update configuration"""
    try:
        new_config = request.json
        
        # Validate required fields
        required_fields = ["retention_days", "interval_hours", "enable_external_backup"]
        for field in required_fields:
            if field not in new_config:
                return jsonify({"success": False, "error": f"Missing field: {field}"}), 400
        
        # Validate types and values
        if not isinstance(new_config["retention_days"], int) or new_config["retention_days"] < 1:
            return jsonify({"success": False, "error": "retention_days must be a positive integer"}), 400
        
        if not isinstance(new_config["interval_hours"], (int, float)) or new_config["interval_hours"] < 1:
            return jsonify({"success": False, "error": "interval_hours must be a positive number"}), 400
        
        if not isinstance(new_config["enable_external_backup"], bool):
            return jsonify({"success": False, "error": "enable_external_backup must be a boolean"}), 400
        
        # Save configuration
        if save_config(new_config):
            return jsonify({"success": True, "message": "Configuration updated successfully. Restart required for changes to take effect."})
        else:
            return jsonify({"success": False, "error": "Failed to save configuration"}), 500
            
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route("/api/backup-status")
def backup_status():
    """API endpoint to get backup timing information"""
    last = 0
    if os.path.exists(LAST_BACKUP_FILE):
        try:
            with open(LAST_BACKUP_FILE, "r") as f:
                data = json.load(f)
                last = data.get("last_backup", 0)
        except:
            pass
    
    now = time.time()
    interval_hours = get_config_value("interval_hours", 24)
    next_backup = last + (interval_hours * 3600)
    time_until_next = max(0, next_backup - now)
    
    return {
        "last_backup": last,
        "last_backup_formatted": jdatetime.datetime.fromtimestamp(last).strftime("%Y-%m-%d %H:%M:%S") if last > 0 else "Never",
        "next_backup": next_backup,
        "next_backup_formatted": jdatetime.datetime.fromtimestamp(next_backup).strftime("%Y-%m-%d %H:%M:%S") if last > 0 else "Pending",
        "seconds_until_next": int(time_until_next),
        "hours_until_next": round(time_until_next / 3600, 1)
    }

def schedule_backup():
    while True:
        try:
            # Reload interval from config
            interval_hours = get_config_value("interval_hours", 24)
            
            last = 0
            if os.path.exists(LAST_BACKUP_FILE):
                with open(LAST_BACKUP_FILE, "r") as f:
                    data = json.load(f)
                    last = data.get("last_backup", 0)

            now = time.time()
            if now - last >= interval_hours * 3600:
                logging.info("Starting scheduled backup...")
                from backup import main as backup_func
                backup_func()
                logging.info("Backup finished successfully.")
                with open(LAST_BACKUP_FILE, "w") as f:
                    json.dump({"last_backup": now}, f)

        except Exception as e:
            logging.error(f"Scheduled backup failed: {e}")
        time.sleep(60)

def start_scheduler():
    t = threading.Thread(target=schedule_backup, daemon=True)
    t.start()

if __name__ == "__main__":
    start_scheduler()
    app.run(host="0.0.0.0", port=6003, debug=True)
