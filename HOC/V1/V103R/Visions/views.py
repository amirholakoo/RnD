from django.shortcuts import render
from .models import Vision, VisionData
from django.contrib import messages
from django.shortcuts import redirect, get_object_or_404
from django.views.decorators.csrf import csrf_exempt
import json
import time
from django.http import JsonResponse
from django.conf import settings
from Warehouse.models import Warehouse
import os, requests

def visions(request):
    visions = Vision.objects.all()
    vision_data = VisionData.objects.all().order_by('-CreationDateTime')
    context = {
        'visions': visions,
        'vision_data': vision_data
    }
    return render(request, 'visions/visions.html', context)

def add(request):
    warehouses = Warehouse.objects.all().filter(Is_Deleted=False)
    if request.method == 'POST':
        name = request.POST.get('name')
        vision_id = request.POST.get('vision_id')
        warehouse = request.POST.get('warehouse')
        description = request.POST.get('description')
        server_ip = request.POST.get('server_ip')
        try:
            vision = Vision(name=name, vision_id=vision_id, description=description, server_ip=server_ip, warehouse=Warehouse.objects.get(id=warehouse))
            vision.save()
            messages.success(request, 'Vision با موفقیت افزوده شد')
            return redirect('visions')
        except Exception as e:
            print(e)
            messages.error(request, 'خطا در افزودن Vision')
    context = {
        'warehouses': warehouses
    }
    return render(request, 'visions/add.html', context)

def edit(request, vision_id):
    vision = get_object_or_404(Vision, id=vision_id)
    warehouses = Warehouse.objects.all().filter(Is_Deleted=False)
    if request.method == 'POST':
        name = request.POST.get('name')
        vision_id = request.POST.get('vision_id')
        warehouse = request.POST.get('warehouse')
        description = request.POST.get('description')
        server_ip = request.POST.get('server_ip')
        try:
            vision.name = name
            vision.vision_id = vision_id
            vision.description = description
            vision.server_ip = server_ip
            vision.warehouse = Warehouse.objects.get(id=warehouse)
            vision.save()
            messages.success(request, 'Vision با موفقیت ویرایش شد')
        except Exception as e:
            messages.error(request, 'خطا در ویرایش Vision')
    context = {
        'vision': vision,
        'warehouses': Warehouse.objects.all().filter(Is_Deleted=False)
    }
    return render(request, 'visions/edit.html', context)

def delete(request, vision_id):
    vision = get_object_or_404(Vision, id=vision_id)
    try:
        vision.Is_Deleted = True
        vision.save()
        messages.success(request, 'Vision با موفقیت حذف شد')
    except Exception as e:
        messages.error(request, 'خطا در حذف Vision')
    return redirect('visions')

def view(request, vision_id):
    vision = get_object_or_404(Vision, id=vision_id)
    context = {
        'vision': vision
    }
    return render(request, 'visions/view.html', context)


@csrf_exempt
def Receive_Data(request):
    if request.method == "POST":
        try:
            data = request.body.decode('utf-8')
            data = json.loads(data)

            ip_address = request.META.get('HTTP_X_FORWARDED_FOR')
            if ip_address:
                ip_address = ip_address.split(',')[0]
            else:
                ip_address = request.META.get('REMOTE_ADDR')
            
            print("_________",ip_address,"_________",data)

            try:
                vision = Vision.objects.filter(vision_id=data["device_id"]).last()
            except:
                vision = Vision.objects.all().first()
            if not vision:
                vision = Vision(name=f"vision{Vision.objects.count() + 1}",vision_id=data["device_id"],server_ip=ip_address)
                vision.save()
            try:
                vision_data = VisionData(vision=vision, data=data,name=json.loads(data["event"])["class_name"],count=int(json.loads(data["event"])["counts"]["live"]))
            except:
                vision_data = VisionData(vision=vision, data=data)
            vision_data.save()
            print("data received from device successfully", vision)
            return JsonResponse({'status': 'ok', 'message': 'Data received from device successfully'})
        except Exception as e:
            print(f"Error processing data: {e}")
            return JsonResponse({'status': 'error', 'message': str(e)}, status=500)


@csrf_exempt
def start_vision_api(request):
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
            start =requests.post("http://172.16.0.223:6007/start")
        except:
            start = None
        if start is not None:
            Is_Start = True
    return Is_Start

@csrf_exempt
def get_logs(request):
    if request.method == "GET":
        try:
            logs = requests.get("http://172.16.0.223:6007/logs")
        except:
            logs = False

        if logs:
            return JsonResponse({'status': 'ok', 'logs': logs.json()["logs"]})
        else:
            return JsonResponse({'status': 'error', 'message': 'No logs found'})
    return JsonResponse({'status': 'error', 'message': 'Only GET allowed'}, status=405)