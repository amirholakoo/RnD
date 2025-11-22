from django.shortcuts import render
import requests
import threading
import time, os
import json
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.conf import settings
from django.contrib import messages
from django.shortcuts import redirect
"""
/myapp/api/updateWeight1
http://81.163.7.71:8000/myapp/api/getShipmentLicenseNumbersByStatus/incoming
"""
Is_Start = False
vision = None
vision_path = os.path.join(settings.BASE_DIR, "vision_data.json")

# Initialize visions_data with error handling
visions_data = {"visions_data": []}
if os.path.exists(vision_path):
    try:
        with open(vision_path, "r") as f:
            content = f.read().strip()
            if content:  # Only try to parse if file is not empty
                visions_data = json.loads(content)
            else:
                # File exists but is empty, reinitialize it
                visions_data = {"visions_data": []}
                with open(vision_path, "w") as f:
                    json.dump(visions_data, f, indent=4)
    except (json.JSONDecodeError, ValueError) as e:
        # File is corrupted, reinitialize it
        print(f"Warning: vision_data.json is corrupted, reinitializing. Error: {e}")
        visions_data = {"visions_data": []}
        with open(vision_path, "w") as f:
            json.dump(visions_data, f, indent=4)
else:
    # File doesn't exist, create it
    with open(vision_path, "w") as f:
        json.dump(visions_data, f, indent=4)


def check_license_numbers():
    """Background thread function to periodically check license numbers"""
    url = "http://81.163.7.71:8000/myapp/api/getShipmentLicenseNumbersByStatus/incoming"
    check_interval = 1
    global visions_data
    global Is_Start
    while True:
        try:
            response = requests.get(url, timeout=10)
            response.raise_for_status()
            
            data = response.json()
            license_numbers = data.get("license_numbers", [])
            
            if license_numbers:
                print("license numbers found",license_numbers)
                visions_data["license_numbers"].append(
                    {
                        "license_number": license_numbers,
                        "timestamp": time.time(),
                        "status": "incoming",
                    }
                )
                with open(vision_path, "w") as f:
                    json.dump(visions_data, f, indent=4)
                print("license numbers saved to file")
                if not Is_Start:
                    start_vision()
            else:
                print("no license numbers found")
            
        except requests.exceptions.RequestException as e:
            print(f"Error checking license numbers: {e}")
        except json.JSONDecodeError as e:
            print(f"Error parsing JSON response: {e}")
        except Exception as e:
            print(f"Unexpected error: {e}")
        
        time.sleep(check_interval)

_monitor_thread = None

def start_monitor_thread():
    """Start the monitoring thread if not already running"""
    global _monitor_thread
    if _monitor_thread is None or not _monitor_thread.is_alive():
        _monitor_thread = threading.Thread(target=check_license_numbers, daemon=True)
        _monitor_thread.start()
        print("License number monitor thread started")

def dashboard(request):
    global visions_data
    try:
        logs = requests.get("http://172.16.0.223:6007/logs")
    except:
        logs = []

    if logs:
        logs = logs.json()["logs"]
    else:
        logs = []

    context = {
        "visions_data": visions_data["visions_data"][-50:],
        "logs": logs,
        "Is_Start": Is_Start,
    }
    return render(request, 'dashboard/dashboard.html', context)

@csrf_exempt
def start_vision_api(request):
    global Is_Start
    target = start_vision()
    if target:
        print("عملیات با موفقیت انجام شد")
        messages.success(request, "عملیات با موفقیت انجام شد")
    else:
        print("عملیات با خطا مواجه شد لطفا دوباره تلاش کنید")
        messages.error(request, "عملیات با خطا مواجه شد لطفا دوباره تلاش کنید")
    return redirect(dashboard)

def start_vision():
    global Is_Start
    if not Is_Start:
        try:
            start =requests.post("http://172.16.0.223:6007/start-vision-monitor")
        except:
            start = None
        if start is not None:
            Is_Start = True
    return Is_Start

@csrf_exempt
def Receive_Data(request):
    global vision
    global vision_path
    global visions_data
    if request.method == "POST":
        try:
            data = request.body.decode('utf-8')
            data = json.loads(data)
            vision = {
                "data": data,
                "timestamp": time.time()
            }
            # Read existing data first
            try:
                with open(vision_path, "r") as f:
                    visions_data = json.load(f)
            except (FileNotFoundError, json.JSONDecodeError):
                # If file doesn't exist or is corrupted, initialize it
                visions_data = {"visions_data": []}
            
            # Append new vision data
            visions_data["visions_data"].append(vision)
            
            # Write updated data back to file
            with open(vision_path, "w") as f:
                json.dump(visions_data, f, indent=4)
            
            print("data received from device successfully", vision)
            return JsonResponse({'status': 'ok'})
        except json.JSONDecodeError as e:
            print(f"Error parsing JSON data: {e}")
            return JsonResponse({'status': 'error', 'message': 'Invalid JSON data'}, status=400)
        except Exception as e:
            print(f"Error processing data: {e}")
            return JsonResponse({'status': 'error', 'message': str(e)}, status=500)
    return JsonResponse({'status': 'error', 'message': 'Only POST allowed'}, status=405)