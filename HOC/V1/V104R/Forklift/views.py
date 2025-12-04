from django.shortcuts import render
from Trucks.models import Truck
from Warehouse.models import Warehouse
from Forklift.models import *
from Visions.models import VisionData
from django.shortcuts import redirect
from django.contrib import messages
import json
import jdatetime
from datetime import datetime
from base.views import convert_to_unix_timestamp, convert_to_jalali

def forklift_panel(request):
    context = {
        'unloads': Unload.objects.all().filter(Is_Deleted=False)
    }
    return render(request, 'forklift/forklift_panel.html', context)

def get_vision_detections():
    vision_detections = []
    
    try:
        vision_data_list = VisionData.objects.all()
        for vision_data in vision_data_list:
            data = vision_data.data
            
            if isinstance(data, str):
                try:
                    data = json.loads(data)
                except json.JSONDecodeError:
                    continue
            
            if not data or not vision_data.name:
                continue
            
            class_name = vision_data.name
            
            if class_name and class_name != "forklift":
                event_str = data.get("event") if isinstance(data, dict) else vision_data.data.get("event")
                if isinstance(event_str, str):
                    event_data = json.loads(event_str)
                else:
                    event_data = event_str or {}

                timestamp = event_data.get("timestamp", "")
                unix_timestamp = convert_to_unix_timestamp(timestamp) if timestamp else ""
                if not any(detection["name"] == class_name for detection in vision_detections):
                    vision_detections.append({"name": class_name, "count": vision_data.count,"vision":vision_data.vision,"start_time":unix_timestamp,"end_time":unix_timestamp})
                else:
                    found = False
                    for detection in vision_detections:
                        if detection["name"] == class_name and ((unix_timestamp - detection["end_time"]) < 1800):
                            detection["count"] += vision_data.count
                            detection["end_time"] = unix_timestamp
                            found = True
                    if not found:
                        vision_detections.append({"name": class_name, "count": vision_data.count,"vision":vision_data.vision,"start_time":unix_timestamp,"end_time":unix_timestamp})

    
    except Exception as e:
        print(e)
    return vision_detections


def unload(request):
    if request.method == 'POST':
        try:
            license_number = Truck.objects.get(id=request.POST.get('license_number_id'))
            unload_location = Warehouse.objects.get(id=request.POST.get('unload_location_id'))
            #unit = request.POST.get('unit')
            quantity = request.POST.get('quantity')
            quality = request.POST.get('quality')
            forklift_driver_name = Forklift_Drivers.objects.get(id=request.POST.get('forklift_driver_name_id'))
            unload = Unload(license_number=license_number, unload_location=unload_location, quantity=quantity, quality=quality, forklift_driver_name=forklift_driver_name)
            unload.save()
            messages.success(request, 'تخلیه با موفقیت اضافه شد')
            return redirect('forklift_panel')
        except Exception as e:
            print(e)
            messages.error(request, 'خطا در اضافه شدن تخلیه: '+str(e))
            return redirect('unload')
    
    vision_detections = get_vision_detections()
    
    context = {
        'trucks': Truck.objects.all(),
        'warehouses': Warehouse.objects.all(),
        'forklift_drivers': Forklift_Drivers.objects.all(),
        'vision_detections': vision_detections
    }
    return render(request, 'forklift/unload.html', context)

def edit_unload(request, id):
    unload = Unload.objects.get(id=id)
    if request.method == 'POST':
        try:
            unload.quantity = request.POST.get('quantity')
            unload.quality = request.POST.get('quality')
            unload.save()
            messages.success(request, 'تخلیه با موفقیت ویرایش شد')
            return redirect('forklift_panel')

        except Exception as e:
            messages.error(request, 'خطا در ویرایش تخلیه: '+str(e))
            return redirect('edit_unload', id=id)
    unload = Unload.objects.get(id=id)
    context = {
        'unload': unload,
        'trucks': Truck.objects.all(),
        'warehouses': Warehouse.objects.all(),
        'forklift_drivers': Forklift_Drivers.objects.all()
    }
    return render(request, 'forklift/edit_unload.html', context)

def delete_unload(request, id):
    try:
        unload = Unload.objects.get(id=id)
        unload.Is_Deleted = True
        unload.save()
        messages.success(request, 'تخلیه با موفقیت حذف شد')
        return redirect('forklift_panel')
    except Exception as e:
        messages.error(request, 'خطا در حذف تخلیه: '+str(e))
        return redirect('delete_unload', id=id)

def view_unload(request, id):
    try:
        unload = Unload.objects.get(id=id)
    except Exception as e:
        messages.error(request, 'خطا در مشاهده تخلیه: '+str(e))
        return redirect('forklift_panel')
    context = {
        'unload': unload
    }
    return render(request, 'forklift/view_unload.html', context)

def forklift_drivers(request):
    context = {
        'forklift_drivers': Forklift_Drivers.objects.all()
    }
    return render(request, 'forklift/forklift_drivers.html', context)

def add_forklift_driver(request):
    if request.method == 'POST':
        try:
            name = request.POST.get('name')
            phone = request.POST.get('phone')
            forklift_driver = Forklift_Drivers(name=name, phone=phone)
            forklift_driver.save()
            messages.success(request, 'راننده لیفت تراک با موفقیت اضافه شد')
            return redirect('forklift_drivers')
        except Exception as e:
            messages.error(request, 'خطا در اضافه شدن راننده لیفت تراک: '+str(e))
            return redirect('add_forklift_driver')
    return render(request, 'forklift/add_forklift_driver.html')

def edit_forklift_driver(request, id):
    forklift_driver = Forklift_Drivers.objects.get(id=id)
    if request.method == 'POST':
        try:
            name = request.POST.get('name')
            phone = request.POST.get('phone')
            forklift_driver.name = name
            forklift_driver.phone = phone
            forklift_driver.save()
            messages.success(request, 'راننده لیفت تراک با موفقیت ویرایش شد')
            return redirect('forklift_drivers')
        except Exception as e:
            messages.error(request, 'خطا در ویرایش راننده لیفت تراک: '+str(e))
    context = {
        'forklift_driver': forklift_driver
    }
    return render(request, 'forklift/edit_forklift_driver.html', context)

def delete_forklift_driver(request, id):
    try:
        forklift_driver = Forklift_Drivers.objects.get(id=id)
        forklift_driver.Is_Deleted = True
        forklift_driver.save()
        messages.success(request, 'راننده لیفت تراک با موفقیت حذف شد')
        return redirect('forklift_drivers')
    except Exception as e:
        messages.error(request, 'خطا در حذف راننده لیفت تراک: '+str(e))
        return redirect('forklift_drivers', id=id)

def view_forklift_driver(request, id):
    forklift_driver = Forklift_Drivers.objects.get(id=id)
    context = {
        'forklift_driver': forklift_driver
    }
    return render(request, 'forklift/view_forklift_driver.html', context)