from picamera2 import Picamera2
from picamera2.encoders import H264Encoder
from picamera2.outputs import FfmpegOutput
import os
import time
from datetime import datetime
import libcamera

# Settings dictionary for easy customization
settings = {
# 4608, 2592
    'resolution': (1920, 1080),  # 1080p resolution
    'framerate': 20,             # 30 fps
    'duration': 30,              # Duration of each clip in minutes
    'output_dir': '/home/admin/videos',  # Directory to store videos
    'retention_days': 1,          # Keep videos for 1 day
    'bitrate': 40000000,         # 40 Mbps for high quality at 4608x2592
    'profile': 'high',           # High profile for efficient compression
    'level': '5.1',              # Level 5.1 to support high resolution
    'intra_period': 10           # 10 frames for better quality and seeking
}

# Ensure the output directory exists
if not os.path.exists(settings['output_dir']):
    os.makedirs(settings['output_dir'])
    print(f"Created output directory: {settings['output_dir']}")

# Initialize and configure the camera
picam2 = Picamera2()
video_config = picam2.create_video_configuration(
    main={"size": settings['resolution']},
    controls={
        # Video configuration controls
        "FrameRate": 20,  # Default: 30 for 1080p; Range: 1-120; Sets frames per second
        "HdrMode": libcamera.controls.HdrModeEnum.SingleExposure,  # Default: Off; Range: Enum (Off, MultiExposure, etc.); Controls HDR
        "AeEnable": True,  # Default: True; Range: Boolean (True/False); Enables auto exposure
        "AwbEnable": True  # Default: True; Range: Boolean (True/False); Enables auto white balance
    }
)
picam2.configure(video_config)

# Set dynamic camera adjustments
picam2.set_controls({
    # Dynamic camera adjustment controls
    "ExposureValue": -2.0,  # Default: 0.0 (auto if AeEnable=True); Range: -8.0 to 8.0; Exposure compensation
    "AnalogueGain": 1.0,   # Default: 1.0 (auto if AeEnable=True); Range: 1.0-16.0; Sensor sensitivity
    #"DigitalGain": 1.0,    # Default: 1.0 (auto if AeEnable=True); Range: 1.0-255.0; Digital amplification
    "AwbMode": libcamera.controls.AwbModeEnum.Auto,  # Default: Auto; Range: Enum (Auto, Indoor, Outdoor, etc.); White balance mode
    "Brightness": 0.0,     # Default: 0.0; Range: -1.0 to 1.0; Brightness adjustment
    "Contrast": 1.0,       # Default: 1.0; Range: 0.0-16.0; Contrast adjustment
    "Saturation": 1.0,     # Default: 1.0; Range: 0.0-32.0; Color saturation
    "NoiseReductionMode": libcamera.controls.draft.NoiseReductionModeEnum.HighQuality,  # Default: Fast; Range: Enum (Off, Fast, HighQuality); Noise reduction
    "Sharpness": 2.0,      # Default: 1.0; Range: 0.0-16.0; Image sharpness
    "ColourGains": (1.4, 1.4)  # Default: (1.0, 1.0); Range: Tuple of two floats (0.0-16.0); Manual red/blue gains (used if AwbEnable=False)
})

# Start the camera
picam2.start()
print("Camera started with configured settings.")

# Function to generate timestamped filenames
def generate_filename():
    now = datetime.now()
    return now.strftime("%Y%m%d_%H%M%S") + '.mp4'

# Function to delete old video files
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

# Main recording loop
while True:
    try:
        # Clean up old files
        print("Checking for old files to delete...")
        delete_old_files(settings['output_dir'], settings['retention_days'])
        
        # Generate filename and start recording
        filename = generate_filename()
        filepath = os.path.join(settings['output_dir'], filename)
        print(f"Starting recording to {filepath}")
        
        #encoder = H264Encoder()
        encoder = H264Encoder(bitrate=settings['bitrate'], profile=settings['profile'])
        output = FfmpegOutput(filepath)
        picam2.start_recording(encoder, output)
        
        # Record for the specified duration
        time.sleep(settings['duration'] * 60)
        
        # Stop recording
        picam2.stop_recording()
        print(f"Finished recording to {filepath}")
        
        # Clean up resources
        del encoder
        del output
        
        # Brief pause before next recording
        time.sleep(5)
        
    except Exception as e:
        print(f"An error occurred: {e}")
        picam2.stop()
        break
