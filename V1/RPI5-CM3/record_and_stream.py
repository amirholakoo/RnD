from picamera2 import Picamera2
from picamera2.encoders import H264Encoder
from picamera2.outputs import FfmpegOutput
import os
import time
from datetime import datetime

# Settings
settings = {
    'resolution': (1920, 1080),
    'framerate': 15,
    'duration': 30,  # minutes
    'output_dir': '/home/admin/videos',
    'retention_days': 1
}

# Ensure output directory exists
if not os.path.exists(settings['output_dir']):
    os.makedirs(settings['output_dir'])

# Initialize and start camera
picam2 = Picamera2()
video_config = picam2.create_video_configuration(main={"size": settings['resolution']}, controls={"FrameRate": settings['framerate']})
picam2.configure(video_config)
picam2.start()
print("Camera started.")

def generate_filename():
    now = datetime.now()
    return now.strftime("%Y%m%d_%H%M%S") + '.mp4'

def delete_old_files(directory, retention_days):
    now = time.time()
    cutoff = now - (retention_days * 86400)
    for filename in os.listdir(directory):
        if filename.endswith('.mp4'):
            filepath = os.path.join(directory, filename)
            mtime = os.path.getmtime(filepath)
            if mtime < cutoff:
                os.remove(filepath)
                print(f"Deleted old file: {filename}")

while True:
    try:
        print("Checking for old files...")
        delete_old_files(settings['output_dir'], settings['retention_days'])
        
        filename = generate_filename()
        filepath = os.path.join(settings['output_dir'], filename)
        print(f"Preparing to record to {filepath}")
        
        # Create new encoder and output for each recording
        encoder = H264Encoder()
        output = FfmpegOutput(filepath)
        print("Starting recording...")
        picam2.start_recording(encoder, output)
        print("Recording started.")
        
        # Wait for the duration
        print(f"Sleeping for {settings['duration']} minutes...")
        time.sleep(settings['duration'] * 60 - 5)
        print("Sleep complete.")
        
        # Stop recording and clean up
        print("Stopping recording...")
        picam2.stop_recording()
        print("Recording stopped.")
        
        # Explicitly delete objects to free resources
        del encoder
        del output
        
        # Add a short delay to ensure file finalization
        print("Waiting 5 seconds before next recording...")
        time.sleep(5)
        
    except Exception as e:
        print(f"An error occurred: {e}")
        picam2.stop()
        break

# Note: Camera is stopped only if an error occurs or script is interrupted