#!/bin/bash
# A script to check the health of multiple servers.

# Define an array of server names for clear reporting.
# These are the English names for the services.
declare -a SERVER_NAMES=(
    "Lab Admin"
    "Lab React App"
    "Loading/Unloading Service"
    "DHT22 Thermal Cam data logger Server/Dashboard"
    "TOTAL monitor Server/Dashboard"
    "Thermal Camera Frontend Server"
    "Thermal Camera Temperature"
)

# Define an array of server URLs to check.
# Non-English characters in URLs have been properly encoded.
declare -a SERVER_URLS=(
    "http://192.168.2.46:8000/admin/"
    "http://192.168.2.46:5173"
    "http://192.168.2.46:18888"
    "http://192.168.2.46:8001/dashboard/"
    "http://192.168.2.20:7500/dashboard/"
    "http://172.16.15.20:8001/view/"
    "http://172.16.15.20:5000/temperature"
)

echo "Starting server health checks..."
echo "---------------------------------"

# Get the total number of servers to check.
num_servers=${#SERVER_URLS[@]}

# Loop through the arrays from 0 to the total number of servers.
for (( i=0; i<${num_servers}; i++ )); do
    NAME=${SERVER_NAMES[$i]}
    URL=${SERVER_URLS[$i]}

    # Use curl to get the HTTP status code.
    # -L : Follow redirects
    # -s : Silent mode
    # -o /dev/null : Discard the response body
    # --connect-timeout 5 : Timeout after 5 seconds of no connection
    # -w "%{http_code}" : Output only the status code
    status_code=$(curl -L -s -o /dev/null --connect-timeout 5 -w "%{http_code}" "$URL")

    # Check if the status code is 200 (OK).
    if [ "$status_code" -eq 200 ]; then
      echo "[ OK ] $NAME is healthy. (Status: $status_code)"
    else
      # Reports any other status code, including 000 for connection failed.
      echo "[FAIL] $NAME may be down. (Status: $status_code)"
    fi
done

echo "---------------------------------"
echo "All checks complete."
