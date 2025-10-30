#!/bin/bash
#
# Data Processor Example
# A bash script that simulates data processing and can be run as a systemd service.
#

# Configuration
PROCESSING_INTERVAL=${PROCESSING_INTERVAL:-60}
LOG_FILE=${LOG_FILE:-/tmp/data_processor.log}
DATA_DIR=${DATA_DIR:-/tmp/data}

# Function to log messages
log_message() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

# Function to process data
process_data() {
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local data_file="$DATA_DIR/data_$(date '+%Y%m%d_%H%M%S').txt"
    
    # Create data directory if it doesn't exist
    mkdir -p "$DATA_DIR"
    
    # Simulate data processing
    log_message "Processing data batch..."
    
    # Generate some sample data
    cat > "$data_file" << EOF
{
    "timestamp": "$timestamp",
    "batch_id": "$(uuidgen 2>/dev/null || echo "batch_$(date +%s))",
    "processed_items": $((RANDOM % 100 + 1)),
    "status": "completed"
}
EOF
    
    log_message "Data batch processed: $data_file"
    
    # Simulate some processing time
    sleep $((RANDOM % 5 + 1))
    
    log_message "Data processing completed successfully"
}

# Function to cleanup old data files
cleanup_old_data() {
    local max_age=${MAX_DATA_AGE:-7}  # days
    log_message "Cleaning up data files older than $max_age days..."
    
    find "$DATA_DIR" -name "data_*.txt" -type f -mtime +$max_age -delete 2>/dev/null
    
    local remaining_files=$(find "$DATA_DIR" -name "data_*.txt" -type f | wc -l)
    log_message "Cleanup completed. $remaining_files data files remaining"
}

# Function to handle shutdown signals
cleanup_and_exit() {
    log_message "Received shutdown signal, cleaning up..."
    log_message "Data processor stopped"
    exit 0
}

# Set up signal handlers
trap cleanup_and_exit SIGTERM SIGINT

# Main processing loop
main() {
    log_message "Data processor started"
    log_message "Processing interval: $PROCESSING_INTERVAL seconds"
    log_message "Log file: $LOG_FILE"
    log_message "Data directory: $DATA_DIR"
    log_message "PID: $$"
    
    # Initial cleanup
    cleanup_old_data
    
    # Main processing loop
    while true; do
        process_data
        
        # Cleanup every 10 iterations
        if [ $((RANDOM % 10)) -eq 0 ]; then
            cleanup_old_data
        fi
        
        log_message "Sleeping for $PROCESSING_INTERVAL seconds..."
        sleep "$PROCESSING_INTERVAL"
    done
}

# Check if running as a service or directly
if [ "${1:-}" = "--test" ]; then
    log_message "Running in test mode (single iteration)"
    process_data
    cleanup_old_data
    log_message "Test completed"
else
    main
fi