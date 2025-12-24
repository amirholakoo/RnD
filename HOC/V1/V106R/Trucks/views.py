from django.shortcuts import render
from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from .models import *
import json, threading, time, requests
from django.contrib import messages
from django.shortcuts import redirect
from Shipments.models import Shipment
from Customer.models import Customer
from Unit.models import Unit
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

# r = requests.get(f"http://81.163.7.71:8000/myapp/api/getUnitNamesBasedOnLicenseOfShipment",params={"lic_number": "x"})
# print(r.status_code)

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
                    truck = Truck.objects.filter(License_plate_number=license_number).first()
                    if not truck:
                        truck = Truck(License_plate_number=license_number)
                        truck.save()
                        print("truck added",license_number)

                    print("check..")
                    shipment = Shipment.objects.filter(Is_Deleted=False).filter(Truck=truck).filter(CreationDateTime__gte=time.time()-4000).first()
                    print(shipment)
                    if not shipment:
                        print("shipment not found",truck.id)
                        shipment = Shipment(Truck=truck)
                        shipment.save()
                        try:
                            
                            # shipment_details = requests.post(f"http://81.163.7.71:8000/myapp/api/getShipmentDetails2ByLicenseNumber",params={"license_number": license_number})
                            # shipment_details =shipment_details.json()
                            # print(shipment_details)
                            # customer = Customer(name=shipment_details["customer_name"])
                            # customer.save()
                            # shipment.Customer = customer
                            # shipment.list_of_reels = shipment_details["list_of_reels"]
                            # shipment.weight1 = shipment_details["weight1"]
                            # shipment.weight2 = shipment_details["weight2"]
                            # shipment.net_weight = shipment_details["net_weight"]
                            # if "unloaded_location" in shipment_details:
                            #     if "tolid" in shipment_details["unloaded_location"].lower():
                            #         shipment.location = Warehouse.objects.filter(name__icontains="تولید").first()
                            #     elif "akhal" in shipment_details["unloaded_location"].lower():
                            #         shipment.location = Warehouse.objects.filter(name__icontains="آخال").first()
                            #     elif "sangin" in shipment_details["unloaded_location"].lower():
                            #         shipment.location = Warehouse.objects.filter(name__icontains="سنگین").first()
                            #     elif "khamir" in shipment_details["unloaded_location"].lower():
                            #         shipment.location = Warehouse.objects.filter(name__icontains="خمیر").first()
                            try:
                                response = requests.get(
                                    "http://81.163.7.71:8000/myapp/api/getUnitNamesBasedOnLicenseOfShipment",
                                    params={"lic_number": license_number},
                                    timeout=5
                                )
                                response.raise_for_status()

                                unit_details = response.json()
                                print(unit_details)

                                last_unit = None

                                for unit_name in unit_details["unit_names"]:
                                    print(unit_name)
                                    last_unit, _ = Unit.objects.get_or_create(name=unit_name)
                                    print(last_unit)

                                if last_unit:
                                    shipment.unit = last_unit
                                    shipment.save()

                            except requests.RequestException as e:
                                print("Request error:", e)
                        except Exception as e:
                            print(e)
                        print("shipment added",shipment.id)
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