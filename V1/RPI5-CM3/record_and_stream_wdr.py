from picamera2 import Picamera2
from picamera2.encoders import H264Encoder
from picamera2.outputs import FfmpegOutput
import os
import time
from datetime import datetime
import libcamera

# Define settings for video recording
settings = {
    'resolution': (1920, 1080),  # Compatible resolution for video with HDR
    'framerate': 15,            # Frames per second
    'duration': 1,             # Recording duration in minutes per clip
    'output_dir': '/home/admin/videos',  # Directory to save video files
    'retention_days': 1         # Number of days to retain video files
}

# Create output directory if it doesn't exist
if not os.path.exists(settings['output_dir']):
    os.makedirs(settings['output_dir'])
    print(f"Created output directory: {settings['output_dir']}")

# Initialize and configure the camera with HDR enabled
picam2 = Picamera2()
video_config = picam2.create_video_configuration(
    main={"size": settings['resolution']},
    controls={
        "FrameRate": settings['framerate'],
        "HdrMode": libcamera.controls.HdrModeEnum.HdrModeMultiExposure  # Enable WDR via HDR
    }
)
picam2.configure(video_config)
picam2.start()
print("Camera initialized and started with HDR (WDR) enabled.")

# Verify HDR mode (for debugging)
current_hdr_mode = picam2.controls.get("HdrMode")
print(f"Current HDR mode: {current_hdr_mode}")

# Function to generate timestamped filenames
def generate_filename():
    now = datetime.now()
    return now.strftime("%Y%m%d_%H%M%S") + '.mp4'

# Function to delete old video files based on retention period
def delete_old_files(directory, retention_days):
    now = time.time()
    cutoff = now - (retention_days * 86400)  # Convert days to seconds
    for filename in os.listdir(directory):
        if filename.endswith('.mp4'):
            filepath = os.path.join(directory, filename)
            mtime = os.path.getmtime(filepath)
            if mtime < cutoff:
                os.remove(filepath)
                print(f"Deleted old file: {filename}")

# Main recording loop
while True:
    try:
        # Clean up old files
        print("Checking for old files to delete...")
        delete_old_files(settings['output_dir'], settings['retention_days'])

        # Set up new recording
        filename = generate_filename()
        filepath = os.path.join(settings['output_dir'], filename)
        print(f"Preparing to record to {filepath}")

        encoder = H264Encoder()
        output = FfmpegOutput(filepath)
        print("Starting recording...")
        picam2.start_recording(encoder, output)
        print("Recording started.")

        # Record for the specified duration
        print(f"Recording for {settings['duration']} minutes...")
        time.sleep(settings['duration'] * 60)  # Convert minutes to seconds
        print("Recording duration complete.")

        # Stop recording
        print("Stopping recording...")
        picam2.stop_recording()
        print("Recording stopped.")

        # Clean up resources
        del encoder
        del output

        # Brief pause before starting the next recording
        print("Waiting 5 seconds before next recording...")
        time.sleep(5)

    except Exception as e:
        print(f"An error occurred: {e}")
        picam2.stop()
        break