#!/usr/bin/env python3
from flask import Flask, render_template, redirect, url_for, request, jsonify, send_file, abort
import os, time, json, jdatetime, threading, logging, socket, subprocess, glob, requests

app = Flask(__name__)

@app.route("/start-vision-monitor", methods=["POST"])
def start_vision_monitor():
    from main import main as start_vision
    start_vision()
    print("Vision monitor started")
    return jsonify({"status": "ok"})

@app.route("/stop-vision-monitor", methods=["POST"])
def stop_vision_monitor():
    from main import stop_vision
    stop_vision()
    return jsonify({"status": "ok"})

@app.route("/logs", methods=["GET"])
def get_logs():
    with open("log.txt", "r") as f:
        logs = f.read()
    return jsonify({"logs": logs})


LOG_FILE = "sent_log.json"

def load_sent_log():
    """Load the list of already sent files. If log does not exist, create it."""
    if not os.path.exists(LOG_FILE):
        # Create an empty log file
        with open(LOG_FILE, "w") as f:
            json.dump([], f)
        return []
    try:
        with open(LOG_FILE, "r") as f:
            return json.load(f)
    except:
        # If the file is corrupted, reset it
        with open(LOG_FILE, "w") as f:
            json.dump([], f)
        return []

def save_sent_log(sent_files):
    """Save the updated list of sent files."""
    with open(LOG_FILE, "w") as f:
        json.dump(sent_files, f)

def get_all_files():
    """Return all JSON files matching pattern."""
    return sorted(glob.glob("result/bale_report_*.json"), key=os.path.getmtime)

def check_data():
    sent_files = load_sent_log()  # list of already sent files

    while True:
        time.sleep(1)
        all_files = get_all_files()

        for file in all_files:
            if file not in sent_files:
                try:
                    with open(file, "r") as f:
                        data = json.load(f)
                except Exception as e:
                    print(f"Error reading {file}: {e}")
                    continue

                if data:
                    try:
                        requests.post("http://192.168.2.21:6008/receive_data/", json=data)
                        print(f"Sent data from {file}")
                        sent_files.append(file)
                        save_sent_log(sent_files)
                    except Exception as e:
                        print(f"Error sending {file} to dashboard: {e}")

if __name__ == "__main__":
    threading.Thread(target=check_data).start()
    app.run(host="0.0.0.0", port=6007, debug=False)
