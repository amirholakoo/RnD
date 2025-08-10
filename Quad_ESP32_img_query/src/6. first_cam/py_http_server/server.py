from flask import Flask, request
import time
import os
import re
from datetime import datetime
import logging

app = Flask(__name__)

# Configure logging for server
logging.basicConfig(filename='server.log', level=logging.INFO, format='%(asctime)s - %(message)s')

# Create base images and logs folders if they don't exist
os.makedirs('images', exist_ok=True)
os.makedirs('logs', exist_ok=True)

def sanitize_filename(name):
    """Sanitize device ID to ensure safe folder names."""
    return re.sub(r'\W+', '_', name)

def generate_unique_filename(base_path, base_name):
    """Generate a unique filename with date, time, and microseconds."""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    filename = f"{base_name}_{timestamp}.jpg"
    full_path = os.path.join(base_path, filename)
    counter = 1
    while os.path.exists(full_path):
        filename = f"{base_name}_{timestamp}_{counter}.jpg"
        full_path = os.path.join(base_path, filename)
        counter += 1
    return full_path

@app.route('/request_send', methods=['GET'])
def request_send():
    """Handle ESP32 request to check if server is ready to receive an image."""
    device_id = request.headers.get('X-Device-ID', 'Unknown')
    logging.info(f"Request from {device_id}: path=/request_send, response=ready, status=200")
    return "ready", 200

@app.route('/send_image', methods=['POST'])
def send_image():
    """Receive and save images from ESP32 devices."""
    device_id = request.headers.get('X-Device-ID', 'Unknown')
    sanitized_device_id = sanitize_filename(device_id)
    image_data = request.data
    
    if not image_data:
        logging.warning(f"Error from {device_id}: path=/send_image, error=No image data received, status=400")
        return "No image data received", 400
    
    # Create device-specific folder if it doesn't exist
    device_folder = os.path.join('images', sanitized_device_id)
    os.makedirs(device_folder, exist_ok=True)
    
    # Generate unique filename with date, time, and microseconds
    base_name = "image"
    filename = generate_unique_filename(device_folder, base_name)
    
    try:
        with open(filename, 'wb') as f:
            f.write(image_data)
        logging.info(f"Image received from {device_id}: path=/send_image, size={len(image_data)} bytes, saved as {filename}, response=Image received, status=200")
        return "Image received", 200
    except Exception as e:
        logging.error(f"Error saving image from {device_id}: path=/send_image, error={str(e)}, status=500")
        return "Failed to save image", 500

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

if __name__ == '__main__':
    logging.info("Starting server on 0.0.0.0:5000")
    app.run(host='0.0.0.0', port=5000)