#!/bin/bash
# Uninstallation script for Server Health Monitor Web Application

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

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Server Health Monitor Uninstallation${NC}"
echo -e "${BLUE}========================================${NC}"
echo

# Check if running as root
if [[ $EUID -ne 0 ]]; then
   echo -e "${RED}This script must be run as root (use sudo)${NC}"
   exit 1
fi

# Stop and disable service
echo -e "${BLUE}Stopping and disabling service...${NC}"
if systemctl is-active --quiet "$SERVICE_NAME"; then
    systemctl stop "$SERVICE_NAME"
    echo -e "${GREEN}Service stopped${NC}"
else
    echo -e "${YELLOW}Service was not running${NC}"
fi

if systemctl is-enabled --quiet "$SERVICE_NAME"; then
    systemctl disable "$SERVICE_NAME"
    echo -e "${GREEN}Service disabled${NC}"
else
    echo -e "${YELLOW}Service was not enabled${NC}"
fi

# Remove systemd service file
echo -e "${BLUE}Removing systemd service file...${NC}"
if [ -f "/etc/systemd/system/${SERVICE_NAME}.service" ]; then
    rm "/etc/systemd/system/${SERVICE_NAME}.service"
    systemctl daemon-reload
    echo -e "${GREEN}Service file removed${NC}"
else
    echo -e "${YELLOW}Service file not found${NC}"
fi

# Remove installation directory
echo -e "${BLUE}Removing installation directory...${NC}"
if [ -d "$INSTALL_DIR" ]; then
    rm -rf "$INSTALL_DIR"
    echo -e "${GREEN}Installation directory removed${NC}"
else
    echo -e "${YELLOW}Installation directory not found${NC}"
fi

# Remove service user (optional)
echo -e "${YELLOW}Do you want to remove the service user '$SERVICE_USER'? (y/n):${NC}"
read -p "Remove user [n]: " REMOVE_USER
REMOVE_USER=${REMOVE_USER:-n}

if [[ "$REMOVE_USER" =~ ^[Yy]$ ]]; then
    if id "$SERVICE_USER" &>/dev/null; then
        userdel "$SERVICE_USER"
        echo -e "${GREEN}Service user removed${NC}"
    else
        echo -e "${YELLOW}Service user not found${NC}"
    fi
else
    echo -e "${YELLOW}Service user kept${NC}"
fi

echo
echo -e "${GREEN}Uninstallation completed successfully!${NC}"
echo -e "${BLUE}All Server Health Monitor files and services have been removed.${NC}"