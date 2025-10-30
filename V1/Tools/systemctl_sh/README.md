# Systemctl Service Manager

A comprehensive tool for managing systemd services from bash and Python scripts. This tool allows you to easily add, remove, and manage services with simple one-line commands.

## Features

- **Easy Service Creation**: Convert bash scripts or Python scripts (with virtual environments) into systemd services
- **One-Line Commands**: Simple interface for quick service management
- **Virtual Environment Support**: Full support for Python virtual environments
- **User/System Services**: Support for both user and system-wide services
- **Environment Variables**: Pass custom environment variables to services
- **Service Database**: Track all managed services
- **Comprehensive Logging**: Detailed logging for troubleshooting
- **Auto-restart**: Services automatically restart on failure

## Installation

1. Clone or download the service manager files:
   ```bash
   # Make sure the scripts are executable
   chmod +x service_manager.py
   chmod +x sm
   ```

2. The tool is ready to use! No additional installation required.

## Quick Start

### Using the wrapper script (recommended)

```bash
# Add a bash script as a service
./sm add my-app /path/to/script.sh --user myuser

# Add a Python script with virtual environment
./sm add my-python-app /path/to/script.py --venv /path/to/venv

# Manage services
./sm start my-app
./sm stop my-app
./sm restart my-app
./sm status my-app

# List all managed services
./sm list

# Remove a service
./sm remove my-app
```

### Using the Python script directly

```bash
# Add a service
python3 service_manager.py add-service my-app /path/to/script.sh --user myuser

# Manage services
python3 service_manager.py manage-service my-app start
python3 service_manager.py manage-service my-app status

# List services
python3 service_manager.py list-services

# Remove service
python3 service_manager.py remove-service my-app
```

## Detailed Usage

### Adding Services

#### Bash Script Service
```bash
./sm add my-bash-service /path/to/script.sh \
    --user myuser \
    --description "My Bash Application" \
    --working-dir /path/to/working/directory \
    --env NODE_ENV=production \
    --env PORT=3000 \
    --auto-start
```

#### Python Script Service with Virtual Environment
```bash
./sm add my-python-service /path/to/script.py \
    --venv /path/to/virtual/environment \
    --user myuser \
    --description "My Python Application" \
    --working-dir /path/to/working/directory \
    --env DJANGO_SETTINGS_MODULE=myapp.settings \
    --system \
    --auto-start
```

### Service Management

```bash
# Start a service
./sm start my-service

# Stop a service
./sm stop my-service

# Restart a service
./sm restart my-service

# Enable service to start on boot
./sm enable my-service

# Disable service from starting on boot
./sm disable my-service

# Check service status
./sm status my-service

# List all managed services
./sm list
```

### Removing Services

```bash
# Remove a user service
./sm remove my-service

# Remove a system service
python3 service_manager.py remove-service my-service --system
```

## Command Line Options

### Add Service Options

- `--venv <path>`: Path to Python virtual environment
- `--user <user>`: User to run the service as (default: current user)
- `--description <desc>`: Service description
- `--working-dir <dir>`: Working directory for the service
- `--env KEY=VALUE`: Environment variable (can be used multiple times)
- `--system`: Install as system service (requires sudo)
- `--auto-start`: Start service after installation

### Service Management Actions

- `start`: Start the service
- `stop`: Stop the service
- `restart`: Restart the service
- `enable`: Enable service to start on boot
- `disable`: Disable service from starting on boot
- `status`: Show detailed service status

## Examples

### Example 1: Web Server Service

```bash
# Create a simple web server script
cat > /tmp/simple_server.py << 'EOF'
#!/usr/bin/env python3
import http.server
import socketserver
import os

PORT = int(os.environ.get('PORT', 8000))

class MyHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write(b'Hello from my service!')

with socketserver.TCPServer(("", PORT), MyHTTPRequestHandler) as httpd:
    print(f"Server running at port {PORT}")
    httpd.serve_forever()
EOF

# Add as service
./sm add web-server /tmp/simple_server.py \
    --user admin \
    --description "Simple Web Server" \
    --env PORT=8080 \
    --auto-start

# Check status
./sm status web-server
```

### Example 2: Data Processing Service

```bash
# Create a data processing script
cat > /tmp/data_processor.sh << 'EOF'
#!/bin/bash

while true; do
    echo "$(date): Processing data..."
    # Your data processing logic here
    sleep 60
done
EOF

chmod +x /tmp/data_processor.sh

# Add as service
./sm add data-processor /tmp/data_processor.sh \
    --user admin \
    --description "Data Processing Service" \
    --working-dir /tmp \
    --env PROCESSING_INTERVAL=60

# Start the service
./sm start data-processor
```

### Example 3: Python Service with Virtual Environment

```bash
# Create a virtual environment
python3 -m venv /tmp/myapp_venv
source /tmp/myapp_venv/bin/activate

# Install dependencies
pip install requests

# Create the application
cat > /tmp/myapp.py << 'EOF'
#!/usr/bin/env python3
import requests
import time
import os

def main():
    while True:
        try:
            # Your application logic here
            print(f"{time.ctime()}: Application running...")
            time.sleep(30)
        except KeyboardInterrupt:
            print("Application stopped")
            break
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(10)

if __name__ == "__main__":
    main()
EOF

# Add as service with virtual environment
./sm add myapp /tmp/myapp.py \
    --venv /tmp/myapp_venv \
    --user admin \
    --description "My Python Application" \
    --auto-start
```

## Service Files Location

- **User services**: `~/.config/systemd/user/`
- **System services**: `/etc/systemd/system/`
- **Service database**: `/var/lib/service_manager/services.json`
- **Logs**: `/var/log/service_manager.log`

## Troubleshooting

### Check Service Status
```bash
# Using the wrapper
./sm status my-service

# Using systemctl directly
systemctl --user status my-service  # for user services
sudo systemctl status my-service    # for system services
```

### View Service Logs
```bash
# View logs using journalctl
journalctl --user -u my-service -f  # for user services
sudo journalctl -u my-service -f    # for system services
```

### Common Issues

1. **Permission denied**: Make sure the script is executable and the user has proper permissions
2. **Service not found**: Check if the service exists using `./sm list`
3. **Virtual environment issues**: Ensure the venv path is correct and contains the Python executable
4. **Service won't start**: Check the service logs for error messages

### Debug Mode

For detailed debugging, you can run the Python script directly with verbose output:

```bash
python3 -u service_manager.py add-service my-service /path/to/script.py --user myuser
```

## Advanced Usage

### System-wide Services

To install services system-wide (accessible to all users):

```bash
./sm add my-service /path/to/script.sh --system --user root
```

### Environment Variables

Pass multiple environment variables:

```bash
./sm add my-service /path/to/script.py \
    --env NODE_ENV=production \
    --env PORT=3000 \
    --env DATABASE_URL=postgresql://user:pass@localhost/db
```

### Working Directory

Set a specific working directory for the service:

```bash
./sm add my-service /path/to/script.sh --working-dir /var/lib/myapp
```

## File Structure

```
systemctl_sh/
├── service_manager.py    # Main Python script
├── sm                   # Bash wrapper script
├── README.md           # This documentation
└── examples/           # Example scripts (optional)
```

## Requirements

- Python 3.6+
- systemd
- Appropriate permissions for service management

## License

This tool is provided as-is for managing systemd services. Use at your own discretion.