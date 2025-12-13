#!/usr/bin/env python3
from flask import Flask, render_template, redirect, url_for, request, jsonify, send_file, abort
import os, time, json, jdatetime, threading, logging, socket, subprocess, glob, requests
from collections import deque
"""
temp/events_log.json
"""
app = Flask(__name__)
main_path = "main.py"
Server_URL = "http://192.168.2.21:6008/receive_data/"


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


def load_sent_events():
    """Load already sent event timestamps."""
    sent_file = "sent_events.json"

    if not os.path.exists(sent_file):
        with open(sent_file, "w") as f:
            json.dump([], f)
        return set()  # Use set for faster lookups

    try:
        with open(sent_file, "r") as f:
            data = json.load(f)
            return set(data) if isinstance(data, list) else set()
    except:
        with open(sent_file, "w") as f:
            json.dump([], f)
        return set()


def save_sent_events(sent_timestamps):
    """Save list of sent event timestamps."""
    with open("sent_events.json", "w") as f:
        json.dump(sorted(list(sent_timestamps)), f)  # Sort for readability


def check_sent_events_status():
    """Check if sent_events.json exists, is empty, or was updated."""
    sent_events_file = "sent_events.json"
    
    if not os.path.exists(sent_events_file):
        print("sent_events.json not found")
        return {"exists": False, "empty": True, "mtime": 0}
    
    try:
        print("sent_events.json found")
        mtime = os.path.getmtime(sent_events_file)
        with open(sent_events_file, "r") as f:
            data = json.load(f)
            is_empty = len(data) < 10 if isinstance(data, list) else False
        return {"exists": True, "empty": is_empty, "mtime": mtime}
    except Exception as e:
        print(f"error loading sent_events.json: {e}")
        return {"exists": True, "empty": True, "mtime": 0}


def check_event_log():
    """Monitor temp/events_log.jsonl and send only new lines."""
    print("Checking event log")
    sent_events = load_sent_events()
    print(f"Loaded {len(sent_events)} sent events")
    
    last_size = 0
    if os.path.exists(EVENT_LOG):
        last_size = os.path.getsize(EVENT_LOG)
        print(f"Initial file size: {last_size}")
    
    # Track sent_events.json status
    last_sent_events_mtime = 0
    sent_events_status = check_sent_events_status()
    force_resend = False
    if sent_events_status["exists"]:
        last_sent_events_mtime = sent_events_status["mtime"]
        print(f"Initial sent_events.json status: empty={sent_events_status['empty']}, mtime={last_sent_events_mtime}")
        # If sent_events.json is empty at startup, clear sent_events to resend all
        if sent_events_status["empty"]:
            print("sent_events.json is empty at startup - clearing sent_events to resend all events")
            sent_events = set()
            save_sent_events(sent_events)
            last_size = 0  # Reset to reprocess entire file
            force_resend = True
    
    while True:
        print("Sleeping for 5 seconds")
        time.sleep(5)
        
        # Check if sent_events.json was updated/cleared
        current_sent_events_status = check_sent_events_status()
        if current_sent_events_status["exists"]:
            current_mtime = current_sent_events_status["mtime"]
            if current_mtime != last_sent_events_mtime:
                print(f"sent_events.json was updated (mtime changed: {last_sent_events_mtime} -> {current_mtime})")
                if current_sent_events_status["empty"]:
                    print("sent_events.json is now empty - clearing sent_events to resend all events")
                    sent_events = set()
                    save_sent_events(sent_events)
                    # Reset last_size to 0 to reprocess entire file
                    last_size = 0
                    force_resend = True
                last_sent_events_mtime = current_mtime
        elif last_sent_events_mtime > 0:
            # sent_events.json was deleted
            print("sent_events.json was deleted - clearing sent_events to resend all events")
            sent_events = set()
            save_sent_events(sent_events)
            last_size = 0
            last_sent_events_mtime = 0
            force_resend = True
        
        if not os.path.exists(EVENT_LOG):
            print("Event log file not found________")
            continue

        # If file size changed → new lines exist
        new_size = os.path.getsize(EVENT_LOG)
        
        # Check condition: 
        # 1. Force resend (sent_events.json was just cleared) - prioritize this
        # 2. sent_events is empty (need to resend all events)
        # 3. File size changed AND (sent_events.json is not empty OR doesn't exist)
        should_process = (
            force_resend and new_size > 0
        ) or (
            (len(sent_events) < 10 and new_size > 0)  # Resend all if sent_events is empty
        ) or (
            (new_size > last_size) and (
                not current_sent_events_status["exists"] or not current_sent_events_status["empty"]
            )
        )
        
        print(should_process,len(sent_events),new_size,'_______________')

        if should_process:  # file appended and sent_events.json allows processing, or need to resend all
            print(f"Processing: force_resend={force_resend}, sent_events_len={len(sent_events)}, new_size={new_size}, last_size={last_size}, should_process={should_process}")
            read_entire_file = (len(sent_events) == 0)
            if read_entire_file:
                print(f"Resending all events (sent_events is empty). File size: {new_size}")
            else:
                print(f"File size changed: {last_size} -> {new_size}")
            
            # If sent_events is empty, read entire file; otherwise only read new content
            if read_entire_file:
                print("sent_events is empty - reading entire file to resend all events")
                with open(EVENT_LOG, "rb") as f:
                    new_content = f.read()
            else:
                # Only read new content (from last_size to end)
                # Use binary mode to seek by bytes, then decode
                with open(EVENT_LOG, "rb") as f:
                    f.seek(last_size)  # Skip already processed content
                    new_content = f.read()
            
            # Decode and split into lines (JSONL format: one JSON object per line)
            try:
                new_text = new_content.decode('utf-8')
                # Split by newline to get individual JSON objects
                new_lines = new_text.strip().split('\n')
                # Filter out empty lines
                new_lines = [line.strip() for line in new_lines if line.strip()]
            except UnicodeDecodeError as e:
                print(f"Error decoding new content: {e}")
                last_size = new_size
                continue
            
            if not new_lines:
                print("No new lines found after filtering")
                last_size = new_size
                continue
            
            print(f"Found {len(new_lines)} new line(s)")
            
            for line in new_lines:
                # Parse JSON to extract timestamp
                try:
                    event_data = json.loads(line)
                    timestamp = event_data.get("timestamp")
                    
                    if not timestamp:
                        print(f"Skipping event without timestamp: {line[:50]}...")
                        continue
                    
                except json.JSONDecodeError as e:
                    print(f"Skipping invalid JSON line: {e}")
                    continue
                
                # Check if this timestamp was already sent
                if timestamp not in sent_events:
                    print(timestamp,"timeeeeeeeeeeeeeeeeeeeeee",sent_events)
                    print(f"Sending new event (timestamp: {timestamp})")
                    try:
                        payload = {"event": line}
                        response = requests.post(
                            Server_URL,
                            json=payload,
                            timeout=10  # Add timeout to prevent hanging
                        )
                        response.raise_for_status()  # Raise exception for bad status codes
                        print(f"Event sent successfully (status: {response.status_code})")
                        sent_events.add(timestamp)
                        save_sent_events(sent_events)
                    except requests.exceptions.RequestException as e:
                        print(f"Error sending event line: {e}")
                        # Don't mark as sent if request failed
                else:
                    print(f"Event already sent (timestamp: {timestamp})")
            
            # Update last_size after processing
            # If we read entire file, set to current size; otherwise increment normally
            if read_entire_file:
                last_size = new_size
                print(f"Processed entire file, updated last_size to {last_size}")
            else:
                last_size = new_size
            
            # Reset force_resend flag after processing
            force_resend = False
        else:
            # File size didn't change, or sent_events.json is empty
            if new_size <= last_size:
                pass  # Normal case - no new content
            elif current_sent_events_status["exists"] and current_sent_events_status["empty"] and len(sent_events) > 0:
                print("Skipping processing: sent_events.json is empty but sent_events is not cleared yet")
            # Always update last_size to track current file size
            last_size = new_size

if __name__ == "__main__":
    threading.Thread(target=check_event_log).start()
    app.run(host="0.0.0.0", port=6007, debug=True)
