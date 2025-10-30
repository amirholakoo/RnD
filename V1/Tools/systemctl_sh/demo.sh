#!/bin/bash
#
# Service Manager Demo Script
# Demonstrates the key features of the systemctl service manager
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SM="$SCRIPT_DIR/sm"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================${NC}"
}

print_step() {
    echo -e "${YELLOW}Step: $1${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

# Function to cleanup demo services
cleanup_demo() {
    print_step "Cleaning up demo services..."
    for service in "demo-web-server" "demo-data-processor"; do
        if systemctl --user is-active "$service" >/dev/null 2>&1; then
            $SM stop "$service" >/dev/null 2>&1 || true
        fi
        $SM remove "$service" >/dev/null 2>&1 || true
    done
    print_success "Cleanup completed"
}

# Set up trap to cleanup on exit
trap cleanup_demo EXIT

main() {
    print_header "Systemctl Service Manager Demo"
    echo
    echo "This demo will show you how to use the service manager to:"
    echo "1. Add bash script services"
    echo "2. Add Python script services"
    echo "3. Manage services (start/stop/restart/status)"
    echo "4. List and monitor services"
    echo
    
    read -p "Press Enter to start the demo..."
    echo
    
    # Demo 1: Add a bash script service
    print_header "Demo 1: Adding a Bash Script Service"
    print_step "Adding data processor service..."
    
    $SM add demo-data-processor examples/data_processor.sh \
        --user $(whoami) \
        --env PROCESSING_INTERVAL=10 \
        --env DATA_DIR="/tmp/demo_data" \
        --description "Demo Data Processor" \
        --auto-start
    
    print_success "Data processor service added and started"
    
    # Wait a moment for it to start
    sleep 3
    
    # Show service status
    print_step "Checking service status..."
    $SM status demo-data-processor
    echo
    
    # Demo 2: List services
    print_header "Demo 2: Listing Services"
    print_step "Listing all managed services..."
    $SM list
    echo
    
    # Demo 3: Service management
    print_header "Demo 3: Service Management"
    print_step "Stopping the service..."
    $SM stop demo-data-processor
    print_success "Service stopped"
    
    print_step "Starting the service..."
    $SM start demo-data-processor
    print_success "Service started"
    
    print_step "Restarting the service..."
    $SM restart demo-data-processor
    print_success "Service restarted"
    echo
    
    # Demo 4: Show logs
    print_header "Demo 4: Monitoring Service Logs"
    print_step "Checking if service is creating data files..."
    sleep 5
    
    if [ -f "/tmp/demo_data"/*.txt 2>/dev/null ]; then
        print_success "Service is working - data files created:"
        ls -la /tmp/demo_data/*.txt | head -3
    else
        print_step "No data files yet, service may still be starting..."
    fi
    echo
    
    # Demo 5: Show service logs
    print_header "Demo 5: Service Logs"
    print_step "Recent service logs:"
    journalctl --user -u demo-data-processor -n 5 --no-pager || echo "No logs available yet"
    echo
    
    # Demo 6: Final status
    print_header "Demo 6: Final Status"
    print_step "Final service list:"
    $SM list
    echo
    
    print_success "Demo completed successfully!"
    echo
    echo "Key features demonstrated:"
    echo "✓ One-line service creation"
    echo "✓ Environment variable configuration"
    echo "✓ Service management (start/stop/restart)"
    echo "✓ Service monitoring and logging"
    echo "✓ Service database tracking"
    echo
    echo "The demo service will be cleaned up when this script exits."
    echo "You can keep it running by pressing Ctrl+C before the cleanup."
    
    read -p "Press Enter to cleanup and exit..."
}

# Check if script is being sourced or executed
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi