#!/bin/bash
# Installation script for Server Health Monitor Web Application
# This script installs the application securely and registers it as a systemd service

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SERVICE_NAME="server_test_web"
INSTALL_DIR="/opt/server_test_web"
SERVICE_USER="servermonitor"
SERVICE_GROUP="servermonitor"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Server Health Monitor Installation${NC}"
echo -e "${BLUE}========================================${NC}"
echo

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   echo -e "${RED}This script must be run as root (use sudo)${NC}"
   exit 1
fi

# Get installation port from user
echo -e "${YELLOW}Please enter the port number for the web server (default: 8080):${NC}"
read -p "Port [8080]: " PORT
PORT=${PORT:-8080}

# Validate port number
if ! [[ "$PORT" =~ ^[0-9]+$ ]] || [ "$PORT" -lt 1 ] || [ "$PORT" -gt 65535 ]; then
    echo -e "${RED}Invalid port number. Please enter a number between 1 and 65535.${NC}"
    exit 1
fi

echo -e "${GREEN}Using port: $PORT${NC}"
echo

# Create service user and group
echo -e "${BLUE}Creating service user and group...${NC}"
if ! id "$SERVICE_USER" &>/dev/null; then
    useradd --system --no-create-home --shell /bin/false "$SERVICE_USER"
    echo -e "${GREEN}Created user: $SERVICE_USER${NC}"
else
    echo -e "${YELLOW}User $SERVICE_USER already exists${NC}"
fi

# Create installation directory
echo -e "${BLUE}Creating installation directory...${NC}"
mkdir -p "$INSTALL_DIR"
chown "$SERVICE_USER:$SERVICE_GROUP" "$INSTALL_DIR"
chmod 755 "$INSTALL_DIR"

# Copy application files
echo -e "${BLUE}Installing application files...${NC}"
cp server_monitor.py "$INSTALL_DIR/"
chown "$SERVICE_USER:$SERVICE_GROUP" "$INSTALL_DIR/server_monitor.py"
chmod 755 "$INSTALL_DIR/server_monitor.py"

# Create configuration file with port
echo -e "${BLUE}Creating configuration...${NC}"
cat > "$INSTALL_DIR/config.py" << EOF
# Server Health Monitor Configuration
PORT = $PORT
EOF
chown "$SERVICE_USER:$SERVICE_GROUP" "$INSTALL_DIR/config.py"
chmod 644 "$INSTALL_DIR/config.py"

# Create logs directory
mkdir -p "$INSTALL_DIR/logs"
chown "$SERVICE_USER:$SERVICE_GROUP" "$INSTALL_DIR/logs"
chmod 755 "$INSTALL_DIR/logs"

# Create systemd service file
echo -e "${BLUE}Creating systemd service...${NC}"
cat > "/etc/systemd/system/${SERVICE_NAME}.service" << EOF
[Unit]
Description=Server Health Monitor Web Application
After=network.target
Wants=network.target

[Service]
Type=simple
User=$SERVICE_USER
Group=$SERVICE_GROUP
WorkingDirectory=$INSTALL_DIR
ExecStart=/usr/bin/python3 $INSTALL_DIR/server_monitor.py
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal
SyslogIdentifier=$SERVICE_NAME

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=$INSTALL_DIR/logs
ProtectKernelTunables=true
ProtectKernelModules=true
ProtectControlGroups=true

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd and enable service
echo -e "${BLUE}Registering service with systemd...${NC}"
systemctl daemon-reload
systemctl enable "$SERVICE_NAME"

echo -e "${GREEN}Installation completed successfully!${NC}"
echo
echo -e "${BLUE}Service Information:${NC}"
echo -e "  Service Name: $SERVICE_NAME"
echo -e "  Install Directory: $INSTALL_DIR"
echo -e "  Port: $PORT"
echo -e "  User: $SERVICE_USER"
echo
echo -e "${BLUE}Management Commands:${NC}"
echo -e "  Start service:    ${GREEN}systemctl start $SERVICE_NAME${NC}"
echo -e "  Stop service:     ${GREEN}systemctl stop $SERVICE_NAME${NC}"
echo -e "  Restart service:  ${GREEN}systemctl restart $SERVICE_NAME${NC}"
echo -e "  Check status:     ${GREEN}systemctl status $SERVICE_NAME${NC}"
echo -e "  View logs:        ${GREEN}journalctl -u $SERVICE_NAME -f${NC}"
echo
echo -e "${BLUE}Web Interface:${NC}"
echo -e "  URL: ${GREEN}http://localhost:$PORT${NC}"
echo
echo -e "${YELLOW}Would you like to start the service now? (y/n):${NC}"
read -p "Start service [y]: " START_SERVICE
START_SERVICE=${START_SERVICE:-y}

if [[ "$START_SERVICE" =~ ^[Yy]$ ]]; then
    echo -e "${BLUE}Starting service...${NC}"
    systemctl start "$SERVICE_NAME"
    sleep 2
    
    if systemctl is-active --quiet "$SERVICE_NAME"; then
        echo -e "${GREEN}Service started successfully!${NC}"
        echo -e "${GREEN}You can now access the web interface at: http://localhost:$PORT${NC}"
    else
        echo -e "${RED}Failed to start service. Check logs with: journalctl -u $SERVICE_NAME${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}Service installed but not started. Start it manually with: systemctl start $SERVICE_NAME${NC}"
fi

echo
echo -e "${GREEN}Installation complete!${NC}"