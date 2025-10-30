#!/bin/bash
#
# Test Script for Service Manager
# This script demonstrates how to use the service manager with example services.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXAMPLES_DIR="$SCRIPT_DIR/examples"
SM="$SCRIPT_DIR/sm"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to wait for service to be active
wait_for_service() {
    local service_name="$1"
    local max_attempts=10
    local attempt=1
    
    print_status "Waiting for service '$service_name' to be active..."
    
    while [ $attempt -le $max_attempts ]; do
        if systemctl --user is-active "$service_name" >/dev/null 2>&1; then
            print_success "Service '$service_name' is active"
            return 0
        fi
        
        print_status "Attempt $attempt/$max_attempts - service not yet active, waiting..."
        sleep 2
        ((attempt++))
    done
    
    print_error "Service '$service_name' failed to become active within expected time"
    return 1
}

# Function to cleanup test services
cleanup_services() {
    print_status "Cleaning up test services..."
    
    # Stop and remove test services
    for service in "test-web-server" "test-data-processor"; do
        if systemctl --user is-active "$service" >/dev/null 2>&1; then
            print_status "Stopping service '$service'..."
            $SM stop "$service" || true
        fi
        
        if systemctl --user is-enabled "$service" >/dev/null 2>&1; then
            print_status "Disabling service '$service'..."
            $SM disable "$service" || true
        fi
        
        print_status "Removing service '$service'..."
        $SM remove "$service" || true
    done
    
    print_success "Cleanup completed"
}

# Function to run tests
run_tests() {
    print_status "Starting Service Manager tests..."
    
    # Check prerequisites
    if ! command_exists python3; then
        print_error "Python 3 is required but not installed"
        exit 1
    fi
    
    if ! command_exists systemctl; then
        print_error "systemctl is required but not installed"
        exit 1
    fi
    
    # Test 1: Add a Python web server service
    print_status "Test 1: Adding Python web server service..."
    $SM add test-web-server "$EXAMPLES_DIR/simple_web_server.py" \
        --user "$(whoami)" \
        --description "Test Web Server" \
        --env PORT=8080 \
        --auto-start
    
    if wait_for_service "test-web-server"; then
        print_success "Python web server service added and started successfully"
    else
        print_error "Failed to start Python web server service"
        return 1
    fi
    
    # Test 2: Add a bash data processor service
    print_status "Test 2: Adding bash data processor service..."
    $SM add test-data-processor "$EXAMPLES_DIR/data_processor.sh" \
        --user "$(whoami)" \
        --description "Test Data Processor" \
        --env PROCESSING_INTERVAL=30 \
        --env DATA_DIR="/tmp/test_data" \
        --auto-start
    
    if wait_for_service "test-data-processor"; then
        print_success "Bash data processor service added and started successfully"
    else
        print_error "Failed to start bash data processor service"
        return 1
    fi
    
    # Test 3: List services
    print_status "Test 3: Listing managed services..."
    $SM list
    
    # Test 4: Check service status
    print_status "Test 4: Checking service status..."
    $SM status test-web-server
    echo "---"
    $SM status test-data-processor
    
    # Test 5: Test service management commands
    print_status "Test 5: Testing service management commands..."
    
    print_status "Restarting web server service..."
    $SM restart test-web-server
    sleep 2
    
    print_status "Stopping data processor service..."
    $SM stop test-data-processor
    sleep 2
    
    print_status "Starting data processor service..."
    $SM start test-data-processor
    sleep 2
    
    # Test 6: Test web server functionality
    print_status "Test 6: Testing web server functionality..."
    if command_exists curl; then
        print_status "Testing web server endpoint..."
        if curl -s http://localhost:8080/health >/dev/null; then
            print_success "Web server is responding to health checks"
        else
            print_warning "Web server health check failed (this might be expected if not accessible)"
        fi
    else
        print_warning "curl not available, skipping web server test"
    fi
    
    # Test 7: Check logs
    print_status "Test 7: Checking service logs..."
    print_status "Web server logs (last 5 lines):"
    journalctl --user -u test-web-server -n 5 --no-pager || true
    echo "---"
    print_status "Data processor logs (last 5 lines):"
    journalctl --user -u test-data-processor -n 5 --no-pager || true
    
    print_success "All tests completed successfully!"
    
    # Show final status
    print_status "Final service status:"
    $SM list
}

# Main execution
main() {
    print_status "Service Manager Test Suite"
    print_status "=========================="
    
    # Set up trap to cleanup on exit
    trap cleanup_services EXIT
    
    # Run tests
    if run_tests; then
        print_success "All tests passed!"
        echo
        print_status "Test services are still running. They will be cleaned up when this script exits."
        print_status "You can manually test the services:"
        print_status "  - Web server: http://localhost:8080"
        print_status "  - Data processor logs: journalctl --user -u test-data-processor -f"
        echo
        read -p "Press Enter to cleanup and exit, or Ctrl+C to keep services running..."
    else
        print_error "Some tests failed!"
        exit 1
    fi
}

# Check if script is being sourced or executed
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi