from flask import Flask, request, render_template, jsonify
import os
import re
from datetime import datetime
import logging

app = Flask(__name__)

# Configure logging for server
logging.basicConfig(filename='server.log', level=logging.INFO, format='%(asctime)s - %(message)s')

# Create static/images folder if it doesn't exist
os.makedirs('static/images', exist_ok=True)

# Slot assignment
assigned_slots = {}  # MAC to slot

def sanitize_filename(name):
    """Sanitize device ID to ensure safe folder names."""
    return re.sub(r'\W+', '_', name)

def assign_slot(mac):
    """Assign a slot to the MAC address if available."""
    if len(assigned_slots) < 4 and mac not in assigned_slots:
        slot = len(assigned_slots) + 1
        assigned_slots[mac] = slot
        return slot
    elif mac in assigned_slots:
        return assigned_slots[mac]
    else:
        return None

@app.route('/request_send', methods=['GET'])
def request_send():
    """Handle ESP32 request to check if server is ready to receive an image."""
    device_id = request.headers.get('X-Device-ID', 'Unknown')
    logging.info(f"Request from {device_id}: path=/request_send, response=ready, status=200")
    return "ready", 200

@app.route('/send_image', methods=['POST'])
def send_image():
    """Receive and save images from ESP32 devices to assigned slots."""
    device_id = request.headers.get('X-Device-ID', 'Unknown')
    image_data = request.data
    
    if not image_data:
        logging.warning(f"Error from {device_id}: path=/send_image, error=No image data received, status=400")
        return "No image data received", 400
    
    slot = assign_slot(device_id)
    if slot is not None:
        filename = f"static/images/slot{slot}.jpg"
        with open(filename, 'wb') as f:
            f.write(image_data)
        logging.info(f"Image received from {device_id} (slot {slot}): path=/send_image, size={len(image_data)} bytes, saved as {filename}, response=Image received, status=200")
        return "Image received", 200
    else:
        logging.warning(f"Device {device_id} not assigned to a slot: path=/send_image, status=403")
        return "Not assigned to a slot", 403

@app.route('/log', methods=['POST'])
def log_message():
    """Receive and save log messages from ESP32 devices."""
    device_id = request.headers.get('X-Device-ID', 'Unknown')
    sanitized_device_id = sanitize_filename(device_id)
    log_message = request.data.decode('utf-8')
    
    # Create device-specific log folder if it doesn't exist
    log_folder = os.path.join('logs', sanitized_device_id)
    os.makedirs(log_folder, exist_ok=True)
    
    # Log filename
    log_filename = os.path.join(log_folder, 'device.log')
    
    try:
        with open(log_filename, 'a') as f:
            timestamp = datetime.now().strftime("%Y-%m-d %H:%M:%S")
            f.write(f"{timestamp} - {log_message}\n")
        return "Log received", 200
    except Exception as e:
        return "Failed to save log", 500

@app.route('/get_slots', methods=['GET'])
def get_slots():
    """Return the current slot assignments."""
    slots = {str(i): next((mac for mac, s in assigned_slots.items() if s == i), None) for i in range(1, 5)}
    return jsonify(slots)

@app.route('/')
def index():
    """Serve the main page with camera feeds."""
    return render_template('index.html')

if __name__ == '__main__':
    logging.info("Starting server on 0.0.0.0:5000")
    app.run(host='0.0.0.0', port=5000)