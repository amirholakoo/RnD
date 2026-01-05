from django.http import JsonResponse, StreamingHttpResponse
from django.views.decorators.csrf import csrf_exempt
from django.db import transaction
from django.shortcuts import render
from .models import *
import json
import time

def dashboard(request):
    context = {
        'all_plcs': PLC.objects.all(),
    }
    return render(request, 'plc_monitoring/dashboard.html', context)

def get_plcs(request):
    """Get all PLCs"""
    plcs = PLC.objects.all()
    data = []
    for plc in plcs:
        data.append({
            'id': plc.id,
            'device_id': plc.device_id,
            'name': plc.name,
            'ip_address': plc.ip_address,
            'location': plc.location,
            'Is_Known': plc.Is_Known,
            'description': plc.description,
            'CreationDateTime': plc.CreationDateTime,
            'LastUpdate': plc.LastUpdate
        })
    return JsonResponse({'status': 'ok', 'data': data})

@csrf_exempt
def create_plc(request):
    """Create a new PLC"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Method not allowed'}, status=405)
    
    try:
        device_id = request.POST.get('device_id')
        name = request.POST.get('name', '')
        ip_address = request.POST.get('ip_address', '')
        location = request.POST.get('location', '')
        description = request.POST.get('description', '')
        is_known = request.POST.get('Is_Known') == 'on'
        
        if not device_id:
            return JsonResponse({'status': 'error', 'message': 'شناسه دستگاه الزامی است'})
        
        if PLC.objects.filter(device_id=device_id).exists():
            return JsonResponse({'status': 'error', 'message': 'شناسه دستگاه تکراری است'})
        
        plc = PLC.objects.create(
            device_id=device_id,
            name=name,
            ip_address=ip_address,
            location=location,
            description=description,
            Is_Known=is_known
        )
        
        return JsonResponse({
            'status': 'ok',
            'data': {
                'id': plc.id,
                'device_id': plc.device_id,
                'name': plc.name,
                'ip_address': plc.ip_address,
                'location': plc.location,
                'Is_Known': plc.Is_Known
            }
        })
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})

@csrf_exempt
def update_plc(request):
    """Update an existing PLC"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Method not allowed'}, status=405)
    
    try:
        plc_id = request.POST.get('id')
        device_id = request.POST.get('device_id')
        name = request.POST.get('name', '')
        ip_address = request.POST.get('ip_address', '')
        location = request.POST.get('location', '')
        description = request.POST.get('description', '')
        is_known = request.POST.get('Is_Known') == 'on'
        
        if not plc_id or not device_id:
            return JsonResponse({'status': 'error', 'message': 'فیلدهای اجباری را پر کنید'})
        
        plc = PLC.objects.get(id=plc_id)
        
        if PLC.objects.filter(device_id=device_id).exclude(id=plc_id).exists():
            return JsonResponse({'status': 'error', 'message': 'شناسه دستگاه تکراری است'})
        
        plc.device_id = device_id
        plc.name = name
        plc.ip_address = ip_address
        plc.location = location
        plc.description = description
        plc.Is_Known = is_known
        plc.save()
        
        return JsonResponse({
            'status': 'ok',
            'data': {
                'id': plc.id,
                'device_id': plc.device_id,
                'name': plc.name,
                'ip_address': plc.ip_address,
                'location': plc.location,
                'Is_Known': plc.Is_Known
            }
        })
    except PLC.DoesNotExist:
        return JsonResponse({'status': 'error', 'message': 'دستگاه یافت نشد'})
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})

@csrf_exempt
def delete_plc(request):
    """Delete a PLC"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Method not allowed'}, status=405)
    
    try:
        plc_id = request.POST.get('id')
        
        if not plc_id:
            return JsonResponse({'status': 'error', 'message': 'شناسه دستگاه الزامی است'})
        
        plc = PLC.objects.get(id=plc_id)
        plc.delete()
        
        return JsonResponse({'status': 'ok', 'message': 'دستگاه با موفقیت حذف شد'})
    except PLC.DoesNotExist:
        return JsonResponse({'status': 'error', 'message': 'دستگاه یافت نشد'})
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})

def get_plc_keys(request):
    """Get all PLC keys"""
    keys = PLC_Keys.objects.all()
    
    data = []
    for key in keys:
        data.append({
            'id': key.id,
            'name': key.name,
            'fa_name': key.fa_name,
            'key': key.key,
            'value': key.value,
            'description': key.description,
            'CreationDateTime': key.CreationDateTime,
            'LastUpdate': key.LastUpdate
        })
    
    return JsonResponse({'status': 'ok', 'data': data})

@csrf_exempt
def create_plc_key(request):
    """Create a new PLC key"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Method not allowed'}, status=405)
    
    try:
        name = request.POST.get('name', '')
        fa_name = request.POST.get('fa_name', '')
        key = request.POST.get('key')
        value = request.POST.get('value', '')
        description = request.POST.get('description', '')
        
        if not key:
            return JsonResponse({'status': 'error', 'message': 'نام کلید الزامی است'})
        
        plc_key = PLC_Keys.objects.create(
            name=name,
            fa_name=fa_name,
            key=key,
            value=value,
            description=description
        )
        
        return JsonResponse({
            'status': 'ok',
            'data': {
                'id': plc_key.id,
                'name': plc_key.name,
                'fa_name': plc_key.fa_name,
                'key': plc_key.key,
                'value': plc_key.value,
                'description': plc_key.description
            }
        })
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})

@csrf_exempt
def update_plc_key(request):
    """Update an existing PLC key"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Method not allowed'}, status=405)
    
    try:
        key_id = request.POST.get('id')
        name = request.POST.get('name', '')
        fa_name = request.POST.get('fa_name', '')
        key = request.POST.get('key')
        value = request.POST.get('value', '')
        description = request.POST.get('description', '')
        
        if not key_id or not key:
            return JsonResponse({'status': 'error', 'message': 'فیلدهای اجباری را پر کنید'})
        
        plc_key = PLC_Keys.objects.get(id=key_id)
        plc_key.name = name
        plc_key.fa_name = fa_name
        plc_key.key = key
        plc_key.value = value
        plc_key.description = description
        plc_key.save()
        
        return JsonResponse({
            'status': 'ok',
            'data': {
                'id': plc_key.id,
                'name': plc_key.name,
                'fa_name': plc_key.fa_name,
                'key': plc_key.key,
                'value': plc_key.value,
                'description': plc_key.description
            }
        })
    except PLC_Keys.DoesNotExist:
        return JsonResponse({'status': 'error', 'message': 'کلید یافت نشد'})
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})

@csrf_exempt
def delete_plc_key(request):
    """Delete a PLC key"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Method not allowed'}, status=405)
    
    try:
        key_id = request.POST.get('id')
        
        if not key_id:
            return JsonResponse({'status': 'error', 'message': 'شناسه کلید الزامی است'})
        
        plc_key = PLC_Keys.objects.get(id=key_id)
        plc_key.delete()
        
        return JsonResponse({'status': 'ok', 'message': 'کلید با موفقیت حذف شد'})
    except PLC_Keys.DoesNotExist:
        return JsonResponse({'status': 'error', 'message': 'کلید یافت نشد'})
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})

def sse_plc_logs(request):
    """SSE endpoint for streaming PLC_Logs in real-time (new logs + updated logs)"""
    def event_stream():
        last_id = 0
        last_update_time = time.time()
        
        while True:
            logs_data = []
            
            # Get new logs
            new_logs = PLC_Logs.objects.filter(id__gt=last_id).select_related('plc').order_by('-id')[:50]
            if new_logs:
                last_id = new_logs[0].id
                for log in reversed(new_logs):
                    logs_data.append({
                        'id': log.id,
                        'plc_id': log.plc.id if log.plc else None,
                        'plc_device_id': log.plc.device_id if log.plc else 'نامشخص',
                        'plc_name': log.plc.name if log.plc else '',
                        'data': log.data,
                        'json_data': log.json_data,
                        'CreationDateTime': log.CreationDateTime,
                        'LastUpdate': log.LastUpdate,
                        'is_update': False
                    })
            
            # Get updated logs (LastUpdate changed since last check)
            updated_logs = PLC_Logs.objects.filter(
                id__lte=last_id,
                LastUpdate__gt=last_update_time
            ).select_related('plc').order_by('-LastUpdate')[:20]
            
            for log in updated_logs:
                logs_data.append({
                    'id': log.id,
                    'plc_id': log.plc.id if log.plc else None,
                    'plc_device_id': log.plc.device_id if log.plc else 'نامشخص',
                    'plc_name': log.plc.name if log.plc else '',
                    'data': log.data,
                    'json_data': log.json_data,
                    'CreationDateTime': log.CreationDateTime,
                    'LastUpdate': log.LastUpdate,
                    'is_update': True
                })
            
            last_update_time = time.time()
            
            if logs_data:
                yield f"data: {json.dumps(logs_data, ensure_ascii=False)}\n\n"
            
            time.sleep(1)
    
    response = StreamingHttpResponse(event_stream(), content_type='text/event-stream')
    response['Cache-Control'] = 'no-cache'
    response['X-Accel-Buffering'] = 'no'
    return response

def get_initial_logs(request):
    """Get initial PLC_Logs for table"""
    limit = int(request.GET.get('limit', 50))
    logs = PLC_Logs.objects.select_related('plc').order_by('-id')[:limit]
    logs_data = []
    for log in logs:
        logs_data.append({
            'id': log.id,
            'plc_id': log.plc.id if log.plc else None,
            'plc_device_id': log.plc.device_id if log.plc else 'نامشخص',
            'plc_name': log.plc.name if log.plc else '',
            'data': log.data,
            'json_data': log.json_data,
            'CreationDateTime': log.CreationDateTime,
            'LastUpdate': log.LastUpdate
        })
    return JsonResponse({'status': 'ok', 'data': logs_data})

def live_settings(request):
    """Live PLC settings page for a specific PLC"""
    plc_id = request.GET.get('plc')
    if not plc_id:
        return render(request, 'plc_monitoring/live_settings.html', {'plc': None})
    
    try:
        plc = PLC.objects.get(id=plc_id)
        rolls = Rolls.objects.filter(plc=plc, Is_Deleted=False).order_by('-CreationDateTime')
    except PLC.DoesNotExist:
        plc = None
        rolls = []
    
    return render(request, 'plc_monitoring/live_settings.html', {'plc': plc, 'rolls': rolls})

def get_plc_settings(request):
    """Get all PLCs with their settings"""
    plcs = PLC.objects.all()
    data = []
    for plc in plcs:
        data.append({
            'id': plc.id,
            'device_id': plc.device_id,
            'name': plc.name,
            'setting': plc.setting,
            'LastUpdate': plc.LastUpdate
        })
    return JsonResponse({'status': 'ok', 'data': data})

def sse_plc_settings(request):
    """SSE endpoint for streaming PLC settings in real-time"""
    plc_id = request.GET.get('plc')
    
    def event_stream():
        last_update_time = 0
        
        while True:
            try:
                plc = PLC.objects.get(id=plc_id)
                
                if plc.LastUpdate and plc.LastUpdate > last_update_time:
                    last_update_time = plc.LastUpdate
                    data = {
                        'id': plc.id,
                        'device_id': plc.device_id,
                        'name': plc.name,
                        'setting': plc.setting,
                        'LastUpdate': plc.LastUpdate
                    }
                    yield f"data: {json.dumps(data, ensure_ascii=False)}\n\n"
            except PLC.DoesNotExist:
                pass
            
            time.sleep(1)
    
    response = StreamingHttpResponse(event_stream(), content_type='text/event-stream')
    response['Cache-Control'] = 'no-cache'
    response['X-Accel-Buffering'] = 'no'
    return response