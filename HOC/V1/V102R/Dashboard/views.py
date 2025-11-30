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
from Warehouse.models import Warehouse
"""
/myapp/api/updateWeight1
http://81.163.7.71:8000/myapp/api/getShipmentLicenseNumbersByStatus/incoming
http://81.163.7.71:8000/myapp/api/getShipmentLicenseNumbersByStatus
https://github.com/amirholakoo/ISMv900/blob/main/myapp/models.py
http://81.163.7.71:8000/myapp/api/getShipmentLicenseNumbersByStatus/LoadingUnloading
"""

def check_license_numbers():
    """Background thread function to periodically check license numbers"""
    url = "http://81.163.7.71:8000/myapp/api/getShipmentLicenseNumbersByStatus/LoadingUnloading"
    check_interval = 1
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
    context = {
        "warehouses": Warehouse.objects.all(),
    }
    return render(request, 'dashboard/dashboard.html', context)