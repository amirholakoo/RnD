from django.shortcuts import render
from .models import *
from django.contrib import messages
from django.shortcuts import redirect, get_object_or_404
from django.views.decorators.csrf import csrf_exempt
import json
import time, jdatetime
from django.http import JsonResponse
from django.conf import settings
from Warehouse.models import Warehouse
import os, requests
from base.views import convert_to_unix_timestamp, convert_to_jalali
from Shipments.models import Shipment
from django.db.models import Q
IS_START = False

def visions(request):
    global IS_START
    visions = Vision.objects.all()
    vision_data = VisionData.objects.all().order_by('-CreationDateTime')
    
    # Filter organizing_vision_detections by multiple Types
    type_param = request.GET.get('type', '')
    show_all = request.GET.get('show_all', '')
    exclude_forklift = not show_all  # Default: exclude forklift
    selected_types = [t for t in type_param.split(',') if t] if type_param else []
    organizing_detections = OrganizingVisionData.objects.all().order_by('-start_time')
    if selected_types:
        organizing_detections = organizing_detections.filter(Type__in=selected_types)
    if exclude_forklift:
        organizing_detections = organizing_detections.exclude(class_name='forklift')
    
    context = {
        'visions': visions,
        'vision_data': vision_data,
        'IS_START': IS_START,
        'organizing_vision_detections': organizing_detections,
        'selected_types': selected_types,
        'exclude_forklift': exclude_forklift,
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
            data = data.replace('"true"', "true").replace('"false"', "false")
            data = json.loads(data)
            ip_address = request.META.get('HTTP_X_FORWARDED_FOR')
            if ip_address:
                ip_address = ip_address.split(',')[0]
            else:
                ip_address = request.META.get('REMOTE_ADDR')
            
            print("_________",ip_address,"_________",data)
            vision = False
            try:
                vision = Vision.objects.filter(vision_id=data["event"]["device_id"]).last()
                vision.server_ip = ip_address
                vision.save()
            except:
                print("vision not found")
                
            if not vision:
                try:
                    vision = Vision(name=f"vision{Vision.objects.count() + 1}",vision_id=data["event"]["device_id"],server_ip=ip_address)
                    vision.save()
                    print("vision created")
                except Exception as e:
                    print(e)

            try:
                vision_data = VisionData(vision=vision,
                                         data=data,name=data["event"]["class_name"],
                                         count=int(data["event"]["counts"]["live"]),
                                         enter=data["event"]["counts"]["forklift_entry"],
                                         exit=data["event"]["counts"]["forklift_exit"],
                                         detections_time=convert_to_unix_timestamp(data["event"]["timestamp"]))
            except:
                vision_data = VisionData(vision=vision, data=data)
            vision_data.save()
            organizing_vision_detections(vision_data)
            print("data received from device successfully", vision)
            return JsonResponse({'status': 'ok', 'message': 'Data received from device successfully'})
        except Exception as e:
            print(f"Error processing data: {e}")
            return JsonResponse({'status': 'error', 'message': str(e)}, status=500)


def organizing_vision_detections(vision_data):
    try:
        data = vision_data.data
        if not data or not vision_data.name:
            return
        print(data,"data")
        class_name = vision_data.name
        
        if class_name:
            print("is class name ______________")
            event_str = data.get("event")
            if isinstance(event_str, str):
                event_data = json.loads(event_str)
            else:
                event_data = event_str or {}

            if event_data:
                timestamp = event_data.get("timestamp", "")
                organized_data = OrganizingVisionData.objects.filter(vision=vision_data.vision,class_name=class_name,exit=vision_data.exit,enter=vision_data.enter).order_by('-end_time').first()
                print(vision_data.detections_time)
                print(OrganizingVisionData.objects.filter(vision=vision_data.vision,class_name=class_name,exit=vision_data.exit,enter=vision_data.enter).count(),"organized_data")
                if not organized_data:
                    print("organized_data not found")
                    organized_data = OrganizingVisionData(vision=vision_data.vision,
                                                            class_name=class_name,
                                                            location=vision_data.vision.warehouse,
                                                            count=vision_data.count,
                                                            Type=5 if vision_data.exit else 4 if vision_data.enter else 3,
                                                            start_time=vision_data.detections_time,
                                                            end_time=vision_data.detections_time,
                                                            enter=vision_data.enter,
                                                            exit=vision_data.exit,
                                                            time=f"{int((vision_data.detections_time - vision_data.detections_time)/3600)}:{int((vision_data.detections_time - vision_data.detections_time)/60%60)}",
                                                            time_text=f"{jdatetime.datetime.fromtimestamp(vision_data.detections_time).strftime('%a, %d %b %Y')} از ساعت {jdatetime.datetime.fromtimestamp(vision_data.detections_time).strftime('%H:%M')} تا {jdatetime.datetime.fromtimestamp(vision_data.detections_time).strftime('%H:%M')}",
                                                            enter_count=1 if vision_data.enter else 0,
                                                            exit_count=1 if vision_data.exit else 0,
                                                            move_count=1 if not vision_data.exit and not vision_data.enter else 0)
                    organized_data.save()
                else:
                    print("organized_data found")
                    found = False
                    if organized_data:
                        if ((vision_data.detections_time - organized_data.end_time) < 1800):
                            print("detection found and increasing count",vision_data.detections_time - organized_data.end_time)
                            organized_data.count += vision_data.count
                            organized_data.end_time = vision_data.detections_time

                            organized_data.enter_count += 1 if vision_data.enter else 0
                            organized_data.exit_count += 1 if vision_data.exit else 0
                            organized_data.move_count += 1 if not vision_data.exit and not vision_data.enter else 0
                            organized_data.time = f"{int((organized_data.end_time - organized_data.start_time)/3600)}:{int((organized_data.end_time - organized_data.start_time)/60%60)}"
                            organized_data.time_text = f"{jdatetime.datetime.fromtimestamp(organized_data.start_time).strftime('%a, %d %b %Y')} از ساعت {jdatetime.datetime.fromtimestamp(organized_data.start_time).strftime('%H:%M')} تا {jdatetime.datetime.fromtimestamp(organized_data.end_time).strftime('%H:%M')}"

                            organized_data.save()
                            found = True
                    if not found:
                        print("organized_data not found and creating new one")
                        organized_data = OrganizingVisionData(vision=vision_data.vision,
                                                                class_name=class_name,
                                                                location=vision_data.vision.warehouse,
                                                                count=vision_data.count,
                                                                Type=5 if vision_data.exit else 4 if vision_data.enter else 3,
                                                                start_time=vision_data.detections_time,
                                                                end_time=vision_data.detections_time,
                                                                enter=vision_data.enter,
                                                                exit=vision_data.exit,
                                                                enter_count=1 if vision_data.enter and not vision_data.exit else 0,
                                                                exit_count=1 if vision_data.exit and not vision_data.enter else 0,
                                                                move_count=1 if not vision_data.exit and not vision_data.enter else 0,
                                                                time=f"{int((vision_data.detections_time - vision_data.detections_time)/3600)}:{int((vision_data.detections_time - vision_data.detections_time)/60%60)}",
                                                                time_text=f"{jdatetime.datetime.fromtimestamp(vision_data.detections_time).strftime('%a, %d %b %Y')} از ساعت {jdatetime.datetime.fromtimestamp(vision_data.detections_time).strftime('%H:%M')} تا {jdatetime.datetime.fromtimestamp(vision_data.detections_time).strftime('%H:%M')}")
                        organized_data.save()

                # Find Type and Shipment
                moved_data = OrganizingVisionData.objects.filter(~Q(vision=vision_data.vision),class_name=class_name,exit=True,count=organized_data.count,start_time__gte=organized_data.start_time - 1800,start_time__lte=organized_data.start_time,exit_count=organized_data.enter_count).first()
                
                # Find shipment
                if not organized_data.class_name == "forklift":
                    print("shipment not found")
                    shipment = Shipment.objects.filter(Q(CreationDateTime__gte=organized_data.start_time - 3600) & Q(CreationDateTime__lte=organized_data.start_time + 3600)).last()
                    if shipment:
                        print("shipment found")
                        organized_data.shipment = shipment
                        print("shipment found and saved")

                # Find Type
                if organized_data.shipment:
                    if not organized_data.class_name == "forklift":
                        if organized_data.exit_count > organized_data.enter_count:
                            organized_data.Type = 2
                        else:
                            if not organized_data.class_name == "paper-roll":
                                organized_data.Type = 1
                        if moved_data and vision_data.enter:
                            organized_data.Type = 3
                            organized_data.location = moved_data.location
                            organized_data.destenation = organized_data.location
                            if organized_data.shipment:
                                organized_data.shipment = None
                            if moved_data.shipment:
                                moved_data.shipment = None
                        
                        if not organized_data.Type == 3 and not organized_data.Type == 2 and not organized_data.Type == 1:
                            organized_data.shipment = None

                else:
                    if not organized_data.class_name == "forklift":
                        if moved_data and vision_data.enter:
                            organized_data.Type = 3
                            organized_data.location = moved_data.location
                            organized_data.destenation = organized_data.location
                            
                            if organized_data.shipment:
                                organized_data.shipment = None
                            if moved_data.shipment:
                                moved_data.shipment = None

                organized_data.save()
                moved_data.save()
    
    except Exception as e:
        print(e)
    organized_data = OrganizingVisionData.objects.all().order_by('-start_time')
    return organized_data

@csrf_exempt
def start_vision_api(request,ip_address):
    target = start_vision(ip_address)
    if target:
        print("عملیات با موفقیت انجام شد")
        messages.success(request, "عملیات با موفقیت انجام شد")
    else:
        print("عملیات با خطا مواجه شد لطفا دوباره تلاش کنید")
        messages.error(request, "عملیات با خطا مواجه شد لطفا دوباره تلاش کنید")
    return redirect(dashboard)

def start_vision(ip_address):
    global IS_START
    try:
        start =requests.post(f"http://{ip_address}:6007/start")
        IS_START = True
    except:
        start = False
        IS_START = False
    return start

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