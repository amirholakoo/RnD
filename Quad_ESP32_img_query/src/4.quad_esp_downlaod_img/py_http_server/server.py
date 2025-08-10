from flask import Flask, request
import time
import os
import re
from datetime import datetime
import logging

app = Flask(__name__)

# Configure logging
logging.basicConfig(filename='server.log', level=logging.INFO, format='%(asctime)s - %(message)s')

# Create base images folder if it doesn't exist
os.makedirs('images', exist_ok=True)

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

if __name__ == '__main__':
    logging.info("Starting server on 0.0.0.0:5000")
    app.run(host='0.0.0.0', port=5000)