#!/usr/bin/env python3
import os, subprocess, logging, time, jdatetime, socket, shutil, sqlite3, json
from datetime import datetime

# ======================
# Load Configuration
# ======================
CONFIG_FILE = "config.json"

def load_config():
    """Load configuration from JSON file"""
    default_config = {
        "remote_user": "admin",
        "ssh_pass": "pi",
        "remote_devices": ["192.168.2.46", "192.168.2.20", "192.168.2.22"],
        "remote_search_dir": "/home/admin",
        "remote_backup_dir": "/home/admin/remote-backups",
        "backup_system_dir": "/home/admin/backup-system",
        "local_backup_base": "backups",
        "log_file": "backup.log",
        "retention_days": 7,
        "enable_external_backup": True,
        "external_storage_path": "/media/admin/",
        "external_backup_dir": "backup-system",
        "interval_hours": 24
    }
    
    try:
        if os.path.exists(CONFIG_FILE):
            with open(CONFIG_FILE, 'r') as f:
                config = json.load(f)
                # Merge with defaults (in case new keys are added)
                default_config.update(config)
        else:
            # Create config file with defaults
            with open(CONFIG_FILE, 'w') as f:
                json.dump(default_config, f, indent=2)
    except Exception as e:
        logging.warning(f"Error loading config, using defaults: {e}")
    
    return default_config

# Load configuration
config = load_config()


# ======================
# Settings from Config
# ======================
REMOTE_USER = config["remote_user"]
SSH_PASS = config["ssh_pass"]
REMOTE_DEVICES = config["remote_devices"]
REMOTE_SEARCH_DIR = config["remote_search_dir"]
REMOTE_BACKUP_DIR = config["remote_backup_dir"]
BACKUP_SYSTEM_DIR = config["backup_system_dir"]
LOCAL_BACKUP_BASE = config["local_backup_base"]
LOG_FILE = config["log_file"]
RETENTION_DAYS = config["retention_days"]
ENABLE_EXTERNAL_BACKUP = config["enable_external_backup"]
EXTERNAL_STORAGE_PATH = config["external_storage_path"]
EXTERNAL_BACKUP_DIR = config["external_backup_dir"]
PG_CONFIG = config.get("postgresql", {})

# Get local IP addresses
def get_local_ips():
    ips = set(['127.0.0.1', 'localhost'])
    try:
        hostname = socket.gethostname()
        ips.add(hostname)
        
        # Get all IPs from hostname resolution
        ips.update(socket.gethostbyname_ex(hostname)[2])
        
        # Also get the actual network IP by connecting to external address
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            network_ip = s.getsockname()[0]
            s.close()
            if network_ip and network_ip != '127.0.0.1':
                ips.add(network_ip)
        except:
            pass
    except:
        pass
    return ips

LOCAL_IPS = get_local_ips()

# Create directories if they don't exist
log_dir = os.path.dirname(LOG_FILE)
if log_dir:
    os.makedirs(log_dir, exist_ok=True)

# Create local backup directory if it doesn't exist (only for localhost)
if not os.path.exists(LOCAL_BACKUP_BASE):
    os.makedirs(LOCAL_BACKUP_BASE)

# Create log file if it doesn't exist
if not os.path.exists(LOG_FILE):
    open(LOG_FILE, 'a').close()

logging.basicConfig(filename=LOG_FILE, level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")

# ======================
# Helper Functions
# ======================
def is_local_host(host):
    return host in LOCAL_IPS

def run_cmd(host, cmd):
    """Run command locally or via SSH depending on host"""
    if is_local_host(host):
        try:
            r = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=60)
            return r.stdout.strip() if r.returncode == 0 else None
        except Exception as e:
            logging.error(f"Local command failed: {e}")
            return None
    else:
        try:
            r = subprocess.run(
                ["sshpass", "-p", SSH_PASS, "ssh", "-o", "StrictHostKeyChecking=no", f"{REMOTE_USER}@{host}", cmd],
                capture_output=True, text=True, timeout=60
            )
            return r.stdout.strip() if r.returncode == 0 else None
        except Exception as e:
            logging.error(f"SSH command failed on {host}: {e}")
            return None

# ======================
# Core Functions
# ======================
def find_sqlite_files(host):
    # Exclude both backup-system and remote-backups directories from search
    cmd = f"find {REMOTE_SEARCH_DIR} \\( -path {REMOTE_BACKUP_DIR} -o -path {BACKUP_SYSTEM_DIR} \\) -prune -o -name '*.sqlite3' -type f -print 2>/dev/null"
    result = run_cmd(host, cmd)
    return result.split("\n") if result else []

def backup_sqlite_local(db_path, backup_path):
    """Backup SQLite database locally"""
    try:
        os.makedirs(os.path.dirname(backup_path), exist_ok=True)
        src = sqlite3.connect(f'file:{db_path}?mode=ro', uri=True, timeout=30)
        dst = sqlite3.connect(backup_path)
        with dst:
            src.backup(dst, pages=100, sleep=0.25)
        dst.close()
        cur = src.cursor()
        cur.execute('PRAGMA integrity_check')
        result = cur.fetchone()[0]
        src.close()
        if result == 'ok':
            return backup_path
        else:
            os.remove(backup_path)
            return None
    except Exception as e:
        if os.path.exists(backup_path):
            os.remove(backup_path)
        logging.error(f"Local backup failed for {db_path}: {e}")
        return None

def backup_sqlite(host, db_path):
    timestamp = jdatetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    project = os.path.basename(os.path.dirname(db_path))
    db_name = os.path.basename(db_path)
    backup_name = f"{project}_{db_name}_{timestamp}.sqlite3"
    
    if is_local_host(host):
        # For localhost, backup directly to local backups folder (no remote-backups needed)
        # Determine target directory: external if available, otherwise local
        external_path = get_external_storage_path()
        if external_path:
            target_base = os.path.join(external_path, EXTERNAL_BACKUP_DIR)
            target_dir = os.path.join(target_base, host)
        else:
            target_dir = os.path.join(LOCAL_BACKUP_BASE, host)
        
        os.makedirs(target_dir, exist_ok=True)
        backup_path = os.path.join(target_dir, backup_name)
        return backup_sqlite_local(db_path, backup_path)
    else:
        # For remote devices, backup to remote-backups folder on the remote machine
        remote_backup = f"{REMOTE_BACKUP_DIR}/{backup_name}"
        script = f"""
mkdir -p {REMOTE_BACKUP_DIR} && \
python3 -c "
import sqlite3, os
try:
    src = sqlite3.connect('file:{db_path}?mode=ro', uri=True, timeout=30)
    dst = sqlite3.connect('{remote_backup}')
    with dst:
        src.backup(dst, pages=100, sleep=0.25)
    dst.close()
    cur = src.cursor()
    cur.execute('PRAGMA integrity_check')
    result = cur.fetchone()[0]
    src.close()
    if result == 'ok':
        print('{remote_backup}')
    else:
        os.remove('{remote_backup}')
except Exception:
    if os.path.exists('{remote_backup}'):
        os.remove('{remote_backup}')
"
"""
        result = run_cmd(host, script)
        return result if result and result == remote_backup else None

def pull_backups(host):
    if is_local_host(host):
        # For localhost, backups are already in the right place (no need to pull)
        logging.info(f"Localhost backups already in place, skipping pull")
        return True
    
    # For remote devices, pull to external storage if available, otherwise local
    external_path = get_external_storage_path()
    if external_path:
        target_base = os.path.join(external_path, EXTERNAL_BACKUP_DIR)
        target_dir = os.path.join(target_base, host)
    else:
        target_dir = os.path.join(LOCAL_BACKUP_BASE, host)
    
    os.makedirs(target_dir, exist_ok=True)
    
    # Use rsync to pull from remote device
    try:
        subprocess.run(
            ["sshpass", "-p", SSH_PASS, "rsync", "-az", "--timeout=300",
             f"{REMOTE_USER}@{host}:{REMOTE_BACKUP_DIR}/", target_dir + "/"],
            check=True, capture_output=True
        )
        logging.info(f"Pulled remote backups to: {target_dir}")
        return True
    except Exception as e:
        logging.error(f"Rsync failed: {e}")
        return False

def cleanup_old_backups(host):
    if not is_local_host(host):
        # For remote devices, cleanup on remote machine's remote-backups directory
        run_cmd(host, f"find {REMOTE_BACKUP_DIR} -type f -mtime +{RETENTION_DAYS} -delete 2>/dev/null")
    
    # Cleanup pulled backups - use external storage if available, otherwise local (for both localhost and remote)
    external_path = get_external_storage_path()

    # Candidate directories to clean (covers sqlite and postgresql locations)
    candidate_dirs = []
    if external_path:
        base_ext = os.path.join(external_path, EXTERNAL_BACKUP_DIR)
        candidate_dirs.append(os.path.join(base_ext, host))
        candidate_dirs.append(os.path.join(base_ext, 'postgresql', host))
    # Local locations
    candidate_dirs.append(os.path.join(LOCAL_BACKUP_BASE, host))
    candidate_dirs.append(os.path.join(LOCAL_BACKUP_BASE, 'postgresql', host))

    # Remove duplicates while preserving order
    seen = set()
    dirs_to_check = []
    for d in candidate_dirs:
        if d and d not in seen:
            seen.add(d)
            dirs_to_check.append(d)

    cutoff = time.time() - (RETENTION_DAYS * 86400)
    for target_dir in dirs_to_check:
        if not os.path.exists(target_dir):
            continue
        try:
            for fname in os.listdir(target_dir):
                path = os.path.join(target_dir, fname)
                try:
                    if os.path.isfile(path) and os.path.getmtime(path) < cutoff:
                        os.remove(path)
                        logging.info(f"Removed old backup: {path}")
                except Exception as e:
                    logging.warning(f"Failed to remove old backup {path}: {e}")
        except Exception as e:
            logging.warning(f"Failed to scan backup directory {target_dir}: {e}")

def get_external_storage_path():
    """
    Scan the external storage base directory and find any mounted USB drive.
    Returns the full path to the first available USB drive, or None if not found.
    
    Example: /media/admin/ might contain e805-4c41, returns /media/admin/e805-4c41
    """
    if not ENABLE_EXTERNAL_BACKUP:
        return None
    
    try:
        # Remove trailing slash for consistency
        base_path = EXTERNAL_STORAGE_PATH.rstrip('/')
        
        # Check if base directory exists
        if not os.path.exists(base_path):
            return None
        
        # Scan for subdirectories (mounted USB drives)
        for item in os.listdir(base_path):
            item_path = os.path.join(base_path, item)
            
            # Check if it's a directory
            if os.path.isdir(item_path):
                # Test if it's writable
                try:
                    test_file = os.path.join(item_path, '.backup_test')
                    with open(test_file, 'w') as f:
                        f.write('test')
                    os.remove(test_file)
                    # Found a writable USB drive!
                    logging.info(f"Found external storage: {item_path}")
                    return item_path
                except:
                    # Not writable, try next one
                    continue
        
        return None
    except Exception as e:
        logging.warning(f"Error scanning external storage: {e}")
        return None

def is_external_storage_available():
    """Check if external storage is mounted and writable"""
    return get_external_storage_path() is not None

# ======================
# Postgresql Backup
# ======================
def backup_postgresql():
        if not PG_CONFIG or not PG_CONFIG.get("hosts"):
            print("PostgreSQL config or hosts not found, skipping...")
            return

        date_str = datetime.now().strftime("%Y%m%d_%H%M%S")
        pg_hosts_config = PG_CONFIG.get("hosts", {})

        for host_entry in pg_hosts_config.values():  # iterate over host entries
            host = host_entry.get("host")
            if not host:
                continue

            databases = host_entry.get("databases", [])

            for db_data in databases:
                db_name = db_data.get("name")
                db_user = db_data.get("user")
                db_pass = db_data.get("password")

                if not db_name or not db_user or not db_pass:
                    print(f"Skipping DB entry due to missing name, user, or password: {db_data}")
                    continue

                filename = f"{db_name}_{date_str}.sql"

                # If the host is local, run pg_dump locally and also copy to external if available
                if is_local_host(host):
                    host_name_for_dir = host.replace('.', '_')
                    backup_dir = os.path.join(LOCAL_BACKUP_BASE, "postgresql", host_name_for_dir)
                    os.makedirs(backup_dir, exist_ok=True)
                    filepath = os.path.join(backup_dir, filename)

                    print(f"Backing up local PostgreSQL DB: {db_name} as {filepath}")

                    env = os.environ.copy()
                    env['PGPASSWORD'] = db_pass
                    try:
                        with open(filepath, 'wb') as f:
                            process = subprocess.run(
                                ["pg_dump", "-U", db_user, "-h", "localhost", db_name],
                                stdout=f,
                                stderr=subprocess.PIPE,
                                env=env,
                                timeout=300
                            )

                        if process.returncode != 0:
                            err = process.stderr.decode('utf-8', errors='ignore')
                            print(f"Error backing up local {db_name}: {err}")
                            if os.path.exists(filepath) and os.path.getsize(filepath) == 0:
                                os.remove(filepath)
                            continue
                        else:
                            print(f"Successfully backed up local {db_name} to {filepath}")

                        # Copy to external storage if available
                        external_path = get_external_storage_path()
                        if external_path:
                            target_base = os.path.join(external_path, EXTERNAL_BACKUP_DIR)
                            target_dir = os.path.join(target_base, host)
                            os.makedirs(target_dir, exist_ok=True)
                            try:
                                shutil.copy2(filepath, os.path.join(target_dir, filename))
                                print(f"Copied local backup to external: {os.path.join(target_dir, filename)}")
                            except Exception as e:
                                logging.warning(f"Failed to copy local pg backup to external: {e}")

                    except subprocess.TimeoutExpired:
                        print(f"Error: Local backup timed out for {db_name}")
                        if os.path.exists(filepath) and os.path.getsize(filepath) == 0:
                            os.remove(filepath)
                    except FileNotFoundError as e:
                        print(f"Error: pg_dump not found. Ensure PostgreSQL client is installed. Details: {e}")
                        if os.path.exists(filepath) and os.path.getsize(filepath) == 0:
                            os.remove(filepath)
                    except Exception as e:
                        print(f"An unexpected error occurred during local backup of {db_name}: {e}")
                        if os.path.exists(filepath) and os.path.getsize(filepath) == 0:
                            os.remove(filepath)

                else:
                    # Remote host: create backup on remote REMOTE_BACKUP_DIR, then pull it back
                    remote_backup_path = os.path.join(REMOTE_BACKUP_DIR, filename)
                    # Construct remote command to create remote backup file
                    remote_cmd = (
                        f"mkdir -p {REMOTE_BACKUP_DIR} && "
                        f"PGPASSWORD=\"{db_pass}\" pg_dump -U \"{db_user}\" -h localhost \"{db_name}\" > {remote_backup_path}"
                    )

                    print(f"Backing up PostgreSQL DB: {db_name} on {host} (remote)")

                    try:
                        process = subprocess.run(
                            [
                                "sshpass", "-p", SSH_PASS,
                                "ssh",
                                "-o", "StrictHostKeyChecking=no",
                                f"{REMOTE_USER}@{host}",
                                remote_cmd
                            ],
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            timeout=600
                        )

                        if process.returncode != 0:
                            err = process.stderr.decode('utf-8', errors='ignore')
                            print(f"Error creating remote backup for {db_name} on {host}: {err}")
                            continue
                        else:
                            print(f"Remote backup created: {remote_backup_path} on {host}")

                        # After creating remote backup, pull backups for this host (will put copy in local or external)
                        pulled = pull_backups(host)
                        if pulled:
                            print(f"Pulled remote backups for {host}")
                        else:
                            logging.error(f"Failed to pull remote backups for {host} after creating pg dump")

                    except subprocess.TimeoutExpired:
                        print(f"Error: Remote backup timed out for {db_name} on {host}")
                    except FileNotFoundError as e:
                        print(f"Error: SSH/rsync tools not found. Ensure ssh/sshpass installed. Details: {e}")
                    except Exception as e:
                        print(f"An unexpected error occurred during remote backup of {db_name} on {host}: {e}")
# ======================
# Main
# ======================
def main():
    logging.info("==== Backup started ====")
    logging.info(f"Local IPs detected: {LOCAL_IPS}")
    print(f"Local IPs detected: {LOCAL_IPS}")
    # Check external storage status
    external_path = get_external_storage_path()
    if ENABLE_EXTERNAL_BACKUP:
        if external_path:
            logging.info(f"Backups will be saved to EXTERNAL storage: {external_path}/{EXTERNAL_BACKUP_DIR}")
        else:
            logging.warning(f"External storage not available (scanned: {EXTERNAL_STORAGE_PATH}), using LOCAL storage: {LOCAL_BACKUP_BASE}")
    else:
        logging.info(f"External backup disabled, using LOCAL storage: {LOCAL_BACKUP_BASE}")
    
    for host in REMOTE_DEVICES:
        is_local = is_local_host(host)
        logging.info(f"Processing {host} ({'LOCAL' if is_local else 'REMOTE'})")
        
        dbs = find_sqlite_files(host)
        if not dbs:
            logging.warning(f"No SQLite files found on {host}")
        else:
            print(f"Found {len(dbs)} SQLite files on {host}")

        for db in dbs:
            print(f"Backing up {db} on {host}")
            backup = backup_sqlite(host, db)
            if backup:
                logging.info(f"Backed up {db} -> {backup}")
            else:
                logging.error(f"Failed to backup {db} on {host}")
        print(f"Pulling backups for {host}")
        pull_backups(host)

        cleanup_old_backups(host)

    backup_postgresql()

    logging.info("==== Backup finished ====")

if __name__ == "__main__":
    main()
