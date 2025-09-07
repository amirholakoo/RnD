#!/bin/bash
# Startup script for the Server Health Monitor Web Application

echo "Starting Server Health Monitor Web Application..."
echo "================================================"

# Make sure the Python script is executable
chmod +x server_monitor.py

# Start the server
python3 server_monitor.py