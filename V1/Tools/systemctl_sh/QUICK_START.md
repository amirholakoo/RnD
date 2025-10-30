# Quick Start Guide

## One-Line Commands

### Add Services
```bash
# Bash script
./sm add my-app /path/to/script.sh --user myuser

# Python script with venv
./sm add my-python-app /path/to/script.py --venv /path/to/venv --user myuser
```

### Manage Services
```bash
./sm start my-app
./sm stop my-app
./sm restart my-app
./sm status my-app
./sm list
```

### Remove Services
```bash
./sm remove my-app
```

## Test the Implementation

Run the test script to see it in action:
```bash
./test_service_manager.sh
```

## Examples

### Web Server Service
```bash
./sm add web-server examples/simple_web_server.py \
    --user admin \
    --env PORT=8080 \
    --auto-start
```

### Data Processor Service
```bash
./sm add data-processor examples/data_processor.sh \
    --user admin \
    --env PROCESSING_INTERVAL=60 \
    --auto-start
```

## Key Features

- ✅ **One-line commands** for service management
- ✅ **Bash script support** with proper execution
- ✅ **Python script support** with virtual environments
- ✅ **Environment variables** configuration
- ✅ **Auto-restart** on failure
- ✅ **Service database** tracking
- ✅ **Comprehensive logging**
- ✅ **User and system services** support