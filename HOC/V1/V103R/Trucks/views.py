from django.shortcuts import render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from .models import *
import json, threading, time, requests
from django.contrib import messages
from django.shortcuts import redirect


def trucks(request):
    trucks = Truck.objects.all()
    context = {
        'trucks': trucks
    }
    return render(request, 'trucks/trucks.html', context)

def add_truck(request):
    if request.method == "POST":
        try:
            truck = Truck(
                name=request.POST.get("name",''),
                License_plate_number=request.POST.get("License_plate_number",''),
                Truck_type=request.POST.get("Truck_type",''),
                Truck_color=request.POST.get("Truck_color",''),
                description=request.POST.get("description",''),
                phone=request.POST.get("phone",''),
                driver_name=request.POST.get("driver_name",''),
                driver_doc=request.POST.get("driver_doc",''),
                status=request.POST.get("status",''),
                location=request.POST.get("location",''),
            )
            truck.save()
            messages.success(request,"با موفقیت اضافه شد")
            return redirect("trucks")
        except Exception as e:
            messages.error(request,"خطا در اضافه شدن Truck: "+str(e))
            return redirect("add_truck")
    return render(request, 'trucks/add.html')

def edit_truck(request, id):
    truck = Truck.objects.get(id=id)
    if request.method == "POST":
        try:
            truck = Truck.objects.get(id=id)
            truck.name = request.POST.get("name",'')
            truck.License_plate_number = request.POST.get("License_plate_number",'')
            truck.Truck_type = request.POST.get("Truck_type",'')
            truck.Truck_color = request.POST.get("Truck_color",'')
            truck.description = request.POST.get("description",'')
            truck.phone = request.POST.get("phone",'')
            truck.driver_name = request.POST.get("driver_name",'')
            truck.driver_doc = request.POST.get("driver_doc",'')
            truck.status = request.POST.get("status",'')
            truck.location = request.POST.get("location",'')
            truck.save()
            messages.success(request,"با موفقیت ویرایش شد")
            return redirect("trucks")
        except Exception as e:
            messages.error(request,"خطا در ویرایش Truck: "+str(e))
            return redirect("edit_truck", id=id)
    context = {
        'truck': truck
    }
    return render(request, 'trucks/edit.html', context)

def delete_truck(request, id):
        try:
            truck = Truck.objects.get(id=id)
            truck.Is_Deleted = True
            truck.save()
            messages.success(request,"با موفقیت حذف شد")
            return redirect("trucks")
        except Exception as e:
            messages.error(request,"خطا در حذف Truck: "+str(e))
            return redirect("trucks", id=id)

def view_truck(request, id):
    truck = Truck.objects.get(id=id)
    context = {
        'truck': truck
    }
    return render(request, 'trucks/view.html', context)



def check_license_numbers():
    """Background thread function to periodically check license numbers"""
    url = "http://81.163.7.71:8000/myapp/api/getShipmentLicenseNumbersByStatus/LoadingUnloading"
    check_interval = 10
    while True:
        try:
            response = requests.get(url, timeout=10)
            response.raise_for_status()
            
            data = response.json()
            license_numbers = data.get("license_numbers", [])
            
            if license_numbers:
                print("license numbers found",license_numbers)
                for license_number in license_numbers:
                    trucks = Truck.objects.filter(License_plate_number=license_number)
                    if not trucks.first():
                        truck = Truck(License_plate_number=license_number)
                        truck.save()
                        print("truck added",license_number)
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