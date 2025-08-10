from flask import Flask, request, render_template, jsonify
import os
import re
from datetime import datetime
import logging
from PIL import Image
import io

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
    if mac in assigned_slots:
        return assigned_slots[mac]
    elif len(assigned_slots) < 4:
        slot = len(assigned_slots) + 1
        assigned_slots[mac] = slot
        return slot
    else:
        return None

@app.route('/send_image', methods=['POST'])
def send_image():
    """Receive and save grayscale images from ESP32 devices to assigned slots."""
    device_id = request.headers.get('X-Device-ID', 'Unknown')
    try:
        width = int(request.headers.get('X-Image-Width'))
        height = int(request.headers.get('X-Image-Height'))
    except (KeyError, TypeError, ValueError) as e:
        logging.error(f"Invalid headers from {device_id}: {e}")
        return "Invalid headers", 400

    image_data = request.data
    if not image_data:
        logging.warning(f"No image data received from {device_id}")
        return "No image data received", 400

    # Validate image size
    expected_size = width * height
    if len(image_data) != expected_size:
        logging.error(f"Image size mismatch from {device_id}: expected {expected_size}, got {len(image_data)}")
        return "Image size mismatch", 400

    try:
        # Create PIL image from raw grayscale data
        img = Image.frombytes('L', (width, height), image_data)
        slot = assign_slot(device_id)
        if slot is not None:
            filename = f"static/images/slot{slot}.png"
            img.save(filename, 'PNG')
            logging.info(f"Image received from {device_id} (slot {slot}): size={len(image_data)} bytes, saved as {filename}")
            return "Image received", 200
        else:
            logging.warning(f"Device {device_id} not assigned to a slot")
            return "Not assigned to a slot", 403
    except Exception as e:
        logging.error(f"Error processing image from {device_id}: {e}")
        return "Error processing image", 500

@app.route('/log', methods=['POST'])
def log_message():
    """Receive and save log messages from ESP32 devices."""
    device_id = request.headers.get('X-Device-ID', 'Unknown')
    sanitized_device_id = sanitize_filename(device_id)
    log_message = request.data.decode('utf-8', errors='ignore')
    
    # Create device-specific log folder if it doesn't exist
    log_folder = os.path.join('logs', sanitized_device_id)
    os.makedirs(log_folder, exist_ok=True)
    
    # Log filename
    log_filename = os.path.join(log_folder, 'device.log')
    
    try:
        with open(log_filename, 'a') as f:
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            f.write(f"{timestamp} - {log_message}\n")
        return "Log received", 200
    except Exception as e:
        logging.error(f"Failed to save log from {device_id}: {e}")
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