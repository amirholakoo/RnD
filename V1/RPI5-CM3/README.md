# Raspberry Pi Video Recording and Streaming

This repository contains scripts for recording High-Quality + High Bit-rate video using a Raspberry Pi. It includes experimental support for Wide Dynamic Range (WDR) features, tailored for Raspberry Pi 5 setups.

## Installation

To use these scripts, you need:

- A Raspberry Pi (tested on Raspberry Pi 5)
- Raspberry Pi OS installed
- Camera module enabled (configure via `raspi-config`)
- Python 3 installed
- Required Python libraries (see `requirements.txt`)

To set up the environment:

1. Enable the camera module:
   ```bash
   sudo raspi-config
   # Navigate to Interface Options > Camera > Enable
   ```
2. Install the required Python libraries:
   ```bash
   pip install -r requirements.txt
   ```

## Usage

The repository includes the following script:

- `record_and_stream_wpar.py`: An experimental script for recording and streaming video with WDR support.

To run the script:
```bash
python record_and_stream_wpar.py
```

## Known Issues

- The `record_and_stream_wdr.py` script has problems with recordings due to the MultiExposure setting. This experimental feature may cause inconsistent or corrupted video output and is not yet fully reliable.

## License

This project is licensed under the MIT License.