#!/usr/bin/env python3
from flask import Flask, render_template, redirect, url_for, request, jsonify, send_file, abort
import os, time, json, jdatetime, threading, logging, socket, subprocess, glob, requests
from collections import deque
from datetime import datetime
"""
temp/events_log.json
"""
app = Flask(__name__)
main_path = "main.py"
Server_URL = "http://192.168.2.21:6008/receive_data/"
#Server_URL = "http://192.168.2.23:8000/receive_data/"


@app.route("/start")
def start():
    os.system("sudo systemctl start main.service")
    return jsonify({"status": "ok", "message": "Main started"})

@app.route("/stop")
def stop():
    os.system("sudo systemctl stop main.service")
    return jsonify({"status": "ok", "message": "Main stopped"})


@app.route("/status")
def status():
    try:
        result = subprocess.check_output(
            ["/bin/systemctl", "is-active", "main.service"],
            stderr=subprocess.STDOUT
        ).decode().strip()
    except subprocess.CalledProcessError as e:
        result = e.output.decode().strip()

    return jsonify({"status": "ok", "message": result})


@app.route("/logs", methods=["GET"])
def get_logs():
    with open("log.txt", "r") as f:
        last_100 = deque(f, maxlen=100)

    return jsonify({"status": "ok", "logs": "".join(last_100)})


EVENT_LOG = "temp/events_log.json"


def load_vision_events():

    if not os.path.exists(EVENT_LOG):
        print("Event log not found")
        return False

    try:
        print("Event log found")
        with open(EVENT_LOG, "r") as f:
            print("Event log found")
            data = []
            for x in f:
                try:
                    data.append(json.loads(x))
                except Exception as e:
                    print(e)
            return data
    except Exception as e:
        print(e)
        return False

def clear_vision_events():
    with open(EVENT_LOG, "w") as f:
        f.write("")
    return True

def load_sent_events():
    """Load already sent event timestamps."""
    sent_file = "sent_events.json"

    if not os.path.exists(sent_file):
        with open(sent_file, "w") as f:
            json.dump({"last_timestamp": 0}, f)
        return 0

    try:
        with open(sent_file, "r") as f:
            data = json.load(f)
            return data.get("last_timestamp", 0)
    except:
        with open(sent_file, "w") as f:
            json.dump({"last_timestamp": 0}, f)
        return 0

def save_sent_events(sent_timestamps):
    """Save list of sent event timestamps."""
    with open("sent_events.json", "w") as f:
        json.dump({"last_timestamp": sent_timestamps}, f)  


def convert_to_unix_timestamp(iso_timestamp):
    try:
        dt = datetime.fromisoformat(iso_timestamp.replace('Z', '+00:00'))
        return int(dt.timestamp())
    except (ValueError, AttributeError, TypeError):
        return False

def check_event_log():
    
    while True:
        last_timestamp = load_sent_events()
        vision_events = load_vision_events()
        print(f"Loaded {last_timestamp} sent events")
        if vision_events:
            print("vision events found")
            is_problem = False
        for event in vision_events:
            print(event["timestamp"])
            try:
                unix_timestamp = float(event["timestamp"])
                print("is unix as default")
            except Exception as e:
                unix_timestamp = convert_to_unix_timestamp(event["timestamp"])
                print("convert to unix timestamp", unix_timestamp)
            if unix_timestamp > last_timestamp:
                print("have a new event")
                try:
                    payload = {"event": event}
                    response = requests.post(
                        Server_URL,
                        json=payload,
                        timeout=10
                    )
                    print(f"Event sent successfully (status: {response.status_code})")
                    if response.ok:
                        save_sent_events(unix_timestamp)
                        is_problem = False
                    else:
                        print("problem sending event")
                        is_problem = True
                        while not response.ok:
                            print("try to send event again ...")
                            time.sleep(1)
                            response = requests.post(
                                Server_URL,
                                json=payload,
                                timeout=10
                            )
                            if response.ok:
                                break
                        if response.ok:
                            print("event sent successfully")
                            save_sent_events(unix_timestamp)
                            is_problem = False

                except Exception as e:
                    print(f"Error sending event line: {e}")
                    is_problem = True

            else:
                print(f"Event already sent (timestamp: {unix_timestamp})")
        
        if vision_events and not is_problem:
            clear_vision_events()
        print("Sleeping for 5 seconds")
        time.sleep(5)

if __name__ == "__main__":
    threading.Thread(target=check_event_log, daemon=True).start()
    app.run(host="0.0.0.0", port=6007, debug=False, use_reloader=False)
