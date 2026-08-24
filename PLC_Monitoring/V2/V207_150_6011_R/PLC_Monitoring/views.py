from django.http import JsonResponse, StreamingHttpResponse, HttpResponse
from django.views.decorators.csrf import csrf_exempt
from django.db import transaction
from django.shortcuts import render
from django.utils import timezone
from datetime import timedelta
from .models import *
import json
import time, csv
import jdatetime
from io import BytesIO
from openpyxl import Workbook
from openpyxl.styles import Font, Alignment, Border, Side, PatternFill

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
            'CreationDateTime': plc.CreationDateTime.timestamp() if plc.CreationDateTime else None,
            'LastUpdate': plc.LastUpdate.timestamp() if plc.LastUpdate else None
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
            'order_index': key.order_index,
            'description': key.description,
            'CreationDateTime': key.CreationDateTime.timestamp() if key.CreationDateTime else None,
            'LastUpdate': key.LastUpdate.timestamp() if key.LastUpdate else None
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


@csrf_exempt
def update_key_settings_by_key(request):
    """Update live_background and value_max for a PLC key by key name"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Method not allowed'}, status=405)
    try:
        key = request.POST.get('key')
        if not key:
            return JsonResponse({'status': 'error', 'message': 'کلید الزامی است'})
        plc_key = PLC_Keys.objects.filter(key=key).first()
        if not plc_key:
            return JsonResponse({'status': 'error', 'message': 'کلید یافت نشد'})
        if 'live_background' in request.POST:
            plc_key.live_background = request.POST.get('live_background', 'false').lower() in ('true', '1', 'yes')
        if 'value_max' in request.POST:
            try:
                plc_key.value_max = float(request.POST.get('value_max', 100))
            except (ValueError, TypeError):
                plc_key.value_max = 100
        plc_key.save()
        return JsonResponse({
            'status': 'ok',
            'data': {'live_background': plc_key.live_background, 'value_max': plc_key.value_max}
        })
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})


def sse_plc_logs(request):
    """SSE endpoint for streaming PLC_Logs in real-time (new logs + updated logs)"""
    def event_stream():
        last_id = 0
        last_update_time = timezone.now()
        
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
                        'CreationDateTime': log.CreationDateTime.timestamp() if log.CreationDateTime else None,
                        'LastUpdate': log.LastUpdate.timestamp() if log.LastUpdate else None,
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
                    'CreationDateTime': log.CreationDateTime.timestamp() if log.CreationDateTime else None,
                    'LastUpdate': log.LastUpdate.timestamp() if log.LastUpdate else None,
                    'is_update': True
                })
            
            last_update_time = timezone.now()
            
            if logs_data:
                yield f"data: {json.dumps(logs_data, ensure_ascii=False, default=str)}\n\n"
            
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
            'CreationDateTime': log.CreationDateTime.timestamp() if log.CreationDateTime else None,
            'LastUpdate': log.LastUpdate.timestamp() if log.LastUpdate else None
        })
    return JsonResponse({'status': 'ok', 'data': logs_data})

def live_settings(request):
    """Live PLC settings page for a specific PLC with pagination and search"""
    from django.core.paginator import Paginator, EmptyPage, PageNotAnInteger
    
    plc_id = request.GET.get('plc')
    if not plc_id:
        return render(request, 'plc_monitoring/live_settings.html', {'plc': None})
    
    try:
        plc = PLC.objects.get(id=plc_id)
        rolls = Rolls.objects.filter(plc=plc, Is_Deleted=False).order_by('-CreationDateTime')
        
        # Search by roll number
        search_query = request.GET.get('search', '').strip()
        if search_query:
            rolls = rolls.filter(roll_number__icontains=search_query)
        
        # Pagination - 50 per page
        paginator = Paginator(rolls, 50)
        page = request.GET.get('page', 1)
        try:
            rolls_page = paginator.page(page)
        except PageNotAnInteger:
            rolls_page = paginator.page(1)
        except EmptyPage:
            rolls_page = paginator.page(paginator.num_pages)
            
    except PLC.DoesNotExist:
        plc = None
        rolls_page = None
        search_query = ''
        paginator = None
    
    # Get key translations for charts
    key_translations = {}
    plc_keys = PLC_Keys.objects.all()
    for pk in plc_keys:
        key_translations[pk.key] = pk.fa_name or pk.name or pk.key
    
    # Get excluded keys for settings panel
    excluded_keys = set(ChartExcludedKeys.objects.values_list('key', flat=True))
    keys_with_status = []
    for pk in plc_keys:
        keys_with_status.append({
            'key': pk.key,
            'fa_name': pk.fa_name or pk.name or pk.key,
            'is_excluded': pk.key in excluded_keys
        })
    
    return render(request, 'plc_monitoring/live_settings.html', {
        'plc': plc, 
        'rolls': rolls_page,
        'search_query': search_query if plc else '',
        'paginator': paginator,
        'key_translations': json.dumps(key_translations, ensure_ascii=False),
        'keys_with_status': json.dumps(keys_with_status, ensure_ascii=False)
    })

def get_historical_chart_data(request):
    """Get historical chart data for a PLC based on time range"""
    plc_id = request.GET.get('plc')
    time_range = request.GET.get('range', '1h')
    interval = int(request.GET.get('interval', 60))  # Default 60 seconds
    
    if not plc_id:
        return JsonResponse({'status': 'error', 'message': 'PLC ID required'})
    
    # Calculate time threshold based on range
    now = timezone.now()
    time_thresholds = {
        '1-h': now - timedelta(hours=1),
        '4-h': now - timedelta(hours=4),
        '8-h': now - timedelta(hours=8),
        '24-h': now - timedelta(hours=24),
        '48-h': now - timedelta(hours=48),
        '1-week': now - timedelta(weeks=1),
        '2-week': now - timedelta(weeks=2),
        '1-month': now - timedelta(days=30),
        '3-month': now - timedelta(days=90),
        '6-month': now - timedelta(days=180),
        '9-month': now - timedelta(days=270),
        '1-year': now - timedelta(days=360),
    }
    threshold = time_thresholds.get(time_range, now - timedelta(hours=1))
    
    # Get excluded keys
    excluded_keys = set(ChartExcludedKeys.objects.values_list('key', flat=True))
    
    # Get key translations
    key_translations = {}
    plc_keys = PLC_Keys.objects.all()
    for pk in plc_keys:
        key_translations[pk.key] = pk.fa_name or pk.name or pk.key
    
    try:
        # Get rolls in time range
        rolls = Rolls.objects.filter(
            plc_id=plc_id,
            Is_Deleted=False,
            CreationDateTime__gte=threshold
        ).order_by('CreationDateTime')
        
        # Get all logs for these rolls
        roll_ids = list(rolls.values_list('id', flat=True))
        logs = PLC_Logs.objects.filter(
            roll_id__in=roll_ids,
            is_running=True
        ).order_by('CreationDateTime')
        
        # Collect all unique keys
        all_keys = set()
        for log in logs:
            if log.json_data:
                for key in log.json_data.keys():
                    if key not in excluded_keys:
                        all_keys.add(key)
        
        # Build raw data grouped by interval (last value in each interval)
        # interval_data[key][interval_timestamp] = last_value
        interval_data = {key: {} for key in all_keys}
        
        for log in logs:
            if not log.CreationDateTime or not log.json_data:
                continue
            # Round timestamp to interval
            ts = log.CreationDateTime.timestamp()
            interval_ts = int(ts // interval) * interval * 1000
            for key in all_keys:
                if key in log.json_data:
                    try:
                        value = float(log.json_data[key])
                        interval_data[key][interval_ts] = value  # Last value wins
                    except (ValueError, TypeError):
                        pass
        
        # Get order_index for keys
        key_order = {}
        for pk in plc_keys:
            key_order[pk.key] = pk.order_index if pk.order_index is not None else 9999
        
        # Convert to series format
        chart_series = []
        for key, data in interval_data.items():
            if data:
                sorted_data = [{'x': ts, 'y': val} for ts, val in sorted(data.items())]
                fa_name = key_translations.get(key, key)
                order = key_order.get(key, 9999)  # Unknown keys go to end
                chart_series.append({'name': key, 'fa_name': fa_name, 'data': sorted_data, 'order_index': order})
        
        # Sort by order_index, then by name for consistent ordering
        chart_series.sort(key=lambda x: (x['order_index'], x['name']))
        
        # Build roll annotations (first log of each roll)
        roll_annotations = []
        for roll in rolls:
            first_log = PLC_Logs.objects.filter(roll=roll, is_running=True).order_by('CreationDateTime').first()
            if first_log and first_log.CreationDateTime:
                roll_annotations.append({
                    'x': int(first_log.CreationDateTime.timestamp() * 1000),
                    'label': f'{roll.roll_number or roll.id}'
                })
        
        # Build roll breaks annotations
        break_annotations = []
        roll_breaks = Roll_Breaks.objects.filter(roll_id__in=roll_ids).order_by('CreationDateTime')
        for rb in roll_breaks:
            if rb.CreationDateTime:
                break_annotations.append({
                    'x': int(rb.CreationDateTime.timestamp() * 1000),
                    'label': rb.break_reason or 'پارگی'
                })
        
        # Build stopped ranges (is_running=False periods, only if > 5 minutes)
        stopped_ranges = []
        all_logs = PLC_Logs.objects.filter(
            roll_id__in=roll_ids
        ).order_by('CreationDateTime').values('CreationDateTime', 'is_running')
        
        range_start = None
        MIN_STOPPED_DURATION_MS = 300000  # 5 minutes in milliseconds
        
        for log in all_logs:
            if not log['CreationDateTime']:
                continue
            dt = log['CreationDateTime']
            if hasattr(dt, 'timestamp'):
                ts_ms = int(dt.timestamp() * 1000)
            else:
                continue
            if not log['is_running'] and range_start is None:
                range_start = ts_ms
            elif log['is_running'] and range_start is not None:
                range_end = ts_ms
                if range_end - range_start >= MIN_STOPPED_DURATION_MS:
                    stopped_ranges.append({'x': range_start, 'x2': range_end})
                range_start = None
        # Handle case where last logs are is_running=False
        if range_start is not None:
            range_end = int(timezone.now().timestamp() * 1000)
            if range_end - range_start >= MIN_STOPPED_DURATION_MS:
                stopped_ranges.append({'x': range_start, 'x2': range_end})
        
        return JsonResponse({
            'status': 'ok',
            'chart_series': chart_series,
            'roll_annotations': roll_annotations,
            'break_annotations': break_annotations,
            'stopped_ranges': stopped_ranges
        })
        
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})

def roll_detail(request, roll_id):
    """Roll detail page showing roll info and plc_setting"""
    # Get dynamic excluded keys from database
    excluded_keys = set(ChartExcludedKeys.objects.values_list('key', flat=True))
    interval = int(request.GET.get('interval', 60))  # Default 60 seconds
    
    try:
        roll = Rolls.objects.select_related('plc').get(id=roll_id)
        logs = PLC_Logs.objects.filter(roll=roll).filter(is_running=True).order_by('CreationDateTime')
        
        # Collect all unique keys from json_data
        all_keys = set()
        for log in logs:
            if log.json_data:
                for key in log.json_data.keys():
                    if key not in excluded_keys:
                        all_keys.add(key)
        
        # Get key translations and order from PLC_Keys
        key_translations = {}
        key_order = {}
        plc_keys = PLC_Keys.objects.filter(key__in=all_keys)
        for pk in plc_keys:
            key_translations[pk.key] = pk.fa_name or pk.name or pk.key
            key_order[pk.key] = pk.order_index if pk.order_index is not None else 9999
        
        # Build data grouped by interval (last value in each interval)
        interval_data = {key: {} for key in all_keys}
        
        for log in logs:
            if not log.CreationDateTime or not log.json_data:
                continue
            # Round timestamp to interval
            ts = log.CreationDateTime.timestamp()
            interval_ts = int(ts // interval) * interval * 1000
            for key in all_keys:
                if key in log.json_data:
                    try:
                        value = float(log.json_data[key])
                        interval_data[key][interval_ts] = value  # Last value wins
                    except (ValueError, TypeError):
                        pass
        
        # Convert to list format for ApexCharts with fa_name
        chart_series = []
        for key, data in interval_data.items():
            if data:
                sorted_data = [{'x': ts, 'y': val} for ts, val in sorted(data.items())]
                fa_name = key_translations.get(key, key)
                order = key_order.get(key, 9999)
                chart_series.append({'name': key, 'fa_name': fa_name, 'data': sorted_data, 'order_index': order})
        
        # Sort by order_index, then by name for consistent ordering
        chart_series.sort(key=lambda x: (x['order_index'], x['name']))
        
        # Build roll breaks annotations
        break_annotations = []
        roll_breaks = Roll_Breaks.objects.filter(roll=roll).order_by('CreationDateTime')
        for rb in roll_breaks:
            if rb.CreationDateTime:
                break_annotations.append({
                    'x': int(rb.CreationDateTime.timestamp() * 1000),
                    'label': rb.break_reason or 'پارگی'
                })
        
        # Build stopped ranges (is_running=False periods, only if > 5 minutes)
        stopped_ranges = []
        all_logs = PLC_Logs.objects.filter(roll=roll).order_by('CreationDateTime').values('CreationDateTime', 'is_running')
        
        range_start = None
        MIN_STOPPED_DURATION_MS = 300000  # 5 minutes in milliseconds
        
        for log in all_logs:
            if not log['CreationDateTime']:
                continue
            dt = log['CreationDateTime']
            if hasattr(dt, 'timestamp'):
                ts_ms = int(dt.timestamp() * 1000)
            else:
                continue
            if not log['is_running'] and range_start is None:
                range_start = ts_ms
            elif log['is_running'] and range_start is not None:
                range_end = ts_ms
                if range_end - range_start >= MIN_STOPPED_DURATION_MS:
                    stopped_ranges.append({'x': range_start, 'x2': range_end})
                range_start = None
        if range_start is not None:
            range_end = int(timezone.now().timestamp() * 1000)
            if range_end - range_start >= MIN_STOPPED_DURATION_MS:
                stopped_ranges.append({'x': range_start, 'x2': range_end})
        
    except Rolls.DoesNotExist:
        roll = None
        chart_series = []
        break_annotations = []
        stopped_ranges = []
    
    # Get all PLC_Keys with excluded status for UI
    all_plc_keys = PLC_Keys.objects.all()
    keys_with_status = []
    for pk in all_plc_keys:
        keys_with_status.append({
            'key': pk.key,
            'fa_name': pk.fa_name or pk.name or pk.key,
            'is_excluded': pk.key in excluded_keys
        })
    
    return render(request, 'plc_monitoring/roll_detail.html', {
        'roll': roll,
        'chart_series': json.dumps(chart_series, ensure_ascii=False),
        'keys_with_status': json.dumps(keys_with_status, ensure_ascii=False),
        'stopped_ranges': json.dumps(stopped_ranges, ensure_ascii=False),
        'break_annotations': json.dumps(break_annotations, ensure_ascii=False),
        'current_interval': interval
    })

@csrf_exempt
def roll_detail_api(request):
    """ roll details api for lab system | POST """
    
    if request.method == "POST":
        roll_from_request = request.POST.get("roll_from_request") or request.GET.get("roll_from_request")
        
        if roll_from_request:
            try:
                roll_number = int(roll_from_request)
                rolls = Rolls.objects.filter(roll_number__gte=roll_number)
            except (ValueError, TypeError):
                rolls = Rolls.objects.all()
        else:
            rolls = Rolls.objects.all()[:20]
        
        # for x in rolls:
        #     x.avg_final_data()

        plc_keys = list(PLC_Keys.objects.all().values())
        data = list(rolls.values(
            "plc_setting",
            "roll_number",
            "CreationDateTime",
            "Paper_breaks",
            "Printed_length",
        ))
        return JsonResponse({"status":200,"data":data,"plc_keys":plc_keys})
    return JsonResponse({"error":"only POST requests"})


@csrf_exempt
def last_roll(request):
    """ last roll for system Shipment Tower | POST """
    if request.method == "POST":
        try:
            target = Rolls.objects.all().last()
            if target.roll_logs.last().is_running:
                target = Rolls.objects.get(roll_number=target.roll_number-1)

        except (ValueError, TypeError):
            target = None

        if target:
            data = {
                "plc_setting": target.plc_setting,
                "roll_number": target.roll_number,
                "CreationDateTime": target.CreationDateTime,
                "Paper_breaks": target.Paper_breaks,
                "Printed_length": target.Printed_length,
            }
            return JsonResponse({"status": 200, "data": data})

        return JsonResponse({"status": 404, "error": "No roll found"})

    return JsonResponse({"error": "only POST requests"}, status=405)


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
            'LastUpdate': plc.LastUpdate.timestamp() if plc.LastUpdate else None
        })
    return JsonResponse({'status': 'ok', 'data': data})

def sse_plc_settings(request):
    """SSE endpoint for streaming PLC settings in real-time"""
    plc_id = request.GET.get('plc')
    
    def event_stream():
        last_update_time = None
        
        while True:
            health = TCP_CONNECTION.objects.first()
            now = time.time()
            if not health:
                health = TCP_CONNECTION(
                    ServerLastUpdate=now,
                    ClientLastUpdate=now,
                )
                health.save()
            server_status = "OK" if (now - health.ServerLastUpdate) < 30 else "ERROR"
            plc_status = "OK" if (now - health.ClientLastUpdate) < 30 else "ERROR"
            data = {
                'server_status':server_status,
                'plc_status':plc_status,
            }
            try:
                plc = PLC.objects.get(id=plc_id)
                
                if plc.LastUpdate and (last_update_time is None or plc.LastUpdate > last_update_time):
                    last_update_time = plc.LastUpdate
                    data['id']= plc.id,
                    data['device_id']= plc.device_id
                    data['name']= plc.name
                    data['setting']= plc.setting
                    data['LastUpdate']= plc.LastUpdate.timestamp() if plc.LastUpdate else None
                    data['server_status']=server_status
                    data['plc_status']=plc_status
                    # yield f"data: {json.dumps(data, ensure_ascii=False, default=str)}\n\n"
            except PLC.DoesNotExist:
                pass
            
            yield f"data: {json.dumps(data, ensure_ascii=False, default=str)}\n\n"
            time.sleep(1)
    
    response = StreamingHttpResponse(event_stream(), content_type='text/event-stream')
    response['Cache-Control'] = 'no-cache'
    response['X-Accel-Buffering'] = 'no'
    return response

@csrf_exempt
def toggle_chart_excluded_key(request):
    """Toggle a key in chart excluded keys"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Method not allowed'}, status=405)
    
    try:
        key = request.POST.get('key')
        if not key:
            return JsonResponse({'status': 'error', 'message': 'کلید الزامی است'})
        
        excluded = ChartExcludedKeys.objects.filter(key=key).first()
        if excluded:
            excluded.delete()
            return JsonResponse({'status': 'ok', 'action': 'removed', 'message': f'کلید {key} از لیست حذف شد'})
        else:
            ChartExcludedKeys.objects.create(key=key)
            return JsonResponse({'status': 'ok', 'action': 'added', 'message': f'کلید {key} به لیست اضافه شد'})
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})

@csrf_exempt
def update_key_order(request):
    """Update order_index for a PLC key"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Method not allowed'}, status=405)
    
    try:
        key = request.POST.get('key')
        order_index = request.POST.get('order_index')
        
        if not key:
            return JsonResponse({'status': 'error', 'message': 'کلید الزامی است'})
        
        try:
            order_index = int(order_index) if order_index else 0
        except ValueError:
            order_index = 0
        
        plc_key = PLC_Keys.objects.filter(key=key).first()
        if plc_key:
            plc_key.order_index = order_index
            plc_key.save()
            return JsonResponse({'status': 'ok', 'message': 'ترتیب بروزرسانی شد'})
        else:
            return JsonResponse({'status': 'error', 'message': 'کلید یافت نشد'})
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})

@csrf_exempt
def update_keys_order_bulk(request):
    """Bulk update order_index for multiple PLC keys (for drag & drop)"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Method not allowed'}, status=405)
    
    try:
        orders_json = request.POST.get('orders')
        if not orders_json:
            return JsonResponse({'status': 'error', 'message': 'داده‌ای ارسال نشده'})
        
        orders = json.loads(orders_json)
        
        with transaction.atomic():
            for key, order_index in orders.items():
                PLC_Keys.objects.filter(key=key).update(order_index=order_index)
        
        return JsonResponse({'status': 'ok', 'message': 'ترتیب بروزرسانی شد'})
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})


@csrf_exempt
def get_key_alert_config(request):
    """Get alert configuration for a key"""
    key = request.GET.get('key')
    if not key:
        return JsonResponse({'status': 'error', 'message': 'کلید الزامی است'})
    
    try:
        config = KeyAlertConfig.objects.get(key=key)
        return JsonResponse({
            'status': 'ok',
            'data': {
                'key': config.key,
                'min_value': config.min_value,
                'min_value_2': config.min_value_2,
                'max_value': config.max_value,
                'max_value_2': config.max_value_2,
                'color_max': config.color_max,
                'color_max_2': config.color_max_2,
                'color_min': config.color_min,
                'color_min_2': config.color_min_2,
                'alert_types': config.alert_types or {}
            }
        })
    except KeyAlertConfig.DoesNotExist:
        return JsonResponse({
            'status': 'ok',
            'data': {
                'key': key,
                'min_value': None,
                'min_value_2': None,
                'max_value': None,
                'max_value_2': None,
                'color_max': '#ff8800',
                'color_max_2': '#ff4444',
                'color_min': '#ff4444',
                'color_min_2': '#ff8800',
                'alert_types': {}
            }
        })
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})


@csrf_exempt
def save_key_alert_config(request):
    """Save alert configuration for a key"""
    if request.method != 'POST':
        return JsonResponse({'status': 'error', 'message': 'Method not allowed'}, status=405)
    
    try:
        key = request.POST.get('key')
        min_value = request.POST.get('min_value')
        min_value_2 = request.POST.get('min_value_2')
        max_value = request.POST.get('max_value')
        max_value_2 = request.POST.get('max_value_2')
        color_max = request.POST.get('color_max', '#ff4444')
        color_max_2 = request.POST.get('color_max_2', '#ff8800')
        color_min = request.POST.get('color_min', '#ff8800')
        color_min_2 = request.POST.get('color_min_2', '#ffaa00')
        alert_types_json = request.POST.get('alert_types', '{}')
        
        if not key:
            return JsonResponse({'status': 'error', 'message': 'کلید الزامی است'})
        
        try:
            min_value = float(min_value) if min_value else None
        except (ValueError, TypeError):
            min_value = None
        
        try:
            min_value_2 = float(min_value_2) if min_value_2 else None
        except (ValueError, TypeError):
            min_value_2 = None
        
        try:
            max_value = float(max_value) if max_value else None
        except (ValueError, TypeError):
            max_value = None
        
        try:
            max_value_2 = float(max_value_2) if max_value_2 else None
        except (ValueError, TypeError):
            max_value_2 = None
        
        try:
            alert_types = json.loads(alert_types_json) if alert_types_json else {}
        except:
            alert_types = {}
        
        config, created = KeyAlertConfig.objects.get_or_create(key=key)
        config.min_value = min_value
        config.min_value_2 = min_value_2
        config.max_value = max_value
        config.max_value_2 = max_value_2
        config.color_max = color_max
        config.color_max_2 = color_max_2
        config.color_min = color_min
        config.color_min_2 = color_min_2
        config.alert_types = alert_types
        config.save()
        
        return JsonResponse({
            'status': 'ok',
            'message': 'تنظیمات با موفقیت ذخیره شد',
            'data': {
                'key': config.key,
                'min_value': config.min_value,
                'min_value_2': config.min_value_2,
                'max_value': config.max_value,
                'max_value_2': config.max_value_2,
                'color_max': config.color_max,
                'color_max_2': config.color_max_2,
                'color_min': config.color_min,
                'color_min_2': config.color_min_2,
                'alert_types': config.alert_types
            }
        })
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})


@csrf_exempt
def get_all_alert_configs(request):
    """Get all alert configurations"""
    try:
        configs = KeyAlertConfig.objects.all()
        data = {}
        for config in configs:
            data[config.key] = {
                'min_value': config.min_value_2,
                'min_value_2': config.min_value,
                'max_value': config.max_value,
                'max_value_2': config.max_value_2,
                'color_max': config.color_max,
                'color_max_2': config.color_max_2,
                'color_min': config.color_min_2,
                'color_min_2': config.color_min,
                'alert_types': config.alert_types or {}
            }
        return JsonResponse({'status': 'ok', 'data': data})
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)})


class Echo:
    def write(self, value):
        return value


def export_logs_csv(request):
    interval = int(request.GET.get('interval', 1))
    plc_id = request.GET.get('plc')
    from_date = request.GET.get('from_date')
    to_date = request.GET.get('to_date')
    time_range = request.GET.get('range')
    now = timezone.now()
    from_time = False
    filename=f"PLC-150-history-4h-from-{jdatetime.datetime.now().strftime('%Y-%m-%d')}.csv"
    threshold_start = now - timedelta(hours=1)
    threshold_end = now

    if time_range:
        try:
            from_time = time_range.split("-")
        except:
            pass

        if from_time:
            if "h" in from_time:
                threshold_start = now - timedelta(hours=int(from_time[0]))
            if "day" in from_time:
                threshold_start = now - timedelta(days=int(from_time[0]))
            if "week" in from_time:
                threshold_start = now - timedelta(weeks=int(from_time[0]))
            if "month" in from_time:
                threshold_start = now - timedelta(days=int(from_time[0])*30)
            if "year" in from_time:
                threshold_start = now - timedelta(days=int(from_time[0])*365)
            filename=f"PLC-150-history-{time_range}-from-{jdatetime.datetime.now().strftime('%Y-%m-%d')}.csv"
    # ---- Date parsing ----
    if from_date and to_date:
        try:
            f = list(map(int, from_date.split('/')))
            t = list(map(int, to_date.split('/')))

            threshold_start = jdatetime.datetime(f[0], f[1], f[2], 0, 0, 0).togregorian()
            threshold_end = jdatetime.datetime(t[0], t[1], t[2], 23, 59, 59).togregorian()

            filename=f"PLC-150-history-from-{from_date}-to-{to_date}.csv"
        except:
            return HttpResponse('فرمت تاریخ نامعتبر است')

    # ---- HARD LIMIT (important) ----
    if (threshold_end - threshold_start).days > 120:
        pass
        #return HttpResponse('بازه زمانی خیلی بزرگ است (حداکثر 120 روز)')

    # ---- Query (optimized) ----
    logs = PLC_Logs.objects.filter(
        is_running=True,
        CreationDateTime__gte=threshold_start,
        CreationDateTime__lte=threshold_end
    )

    if plc_id:
        logs = logs.filter(plc_id=plc_id)

    logs = logs.values(
        'CreationDateTime',
        'json_data',
        'plc__name',
        'plc__device_id',
        'roll__roll_number',
        'roll_id'
    ).order_by('CreationDateTime').iterator(chunk_size=1000)

    # ---- Keys (DO NOT scan all logs anymore) ----
    all_keys = list(PLC_Keys.objects.values_list('key', flat=True))
    all_keys_fa = list(PLC_Keys.objects.values_list('fa_name', flat=True))

    # ---- Streaming ----
    pseudo_buffer = Echo()
    writer = csv.writer(pseudo_buffer)

    def stream():
        # header
        headers = ['Row', 'Machine', 'RollNumber', 'DateTime'] + all_keys
        headers_fa = ['ردیف','نام ماشین','شماره رول','زمان'] + all_keys_fa
        yield ('\ufeff').encode('utf-8')
        yield writer.writerow(headers)
        yield writer.writerow(headers_fa)

        row_index = 1

        # grouping state (ONLY one group in memory)
        current_group = None

        for log in logs:
            dt = log['CreationDateTime']
            if not dt:
                continue

            ts = dt.timestamp()
            interval_ts = int(ts // interval) * interval
            group_key = (log['roll_id'], interval_ts)

            if current_group and current_group['key'] != group_key:
                # flush previous group
                yield writer.writerow(current_group['row'])
                row_index += 1
                current_group = None

            if not current_group:
                dt_j = jdatetime.datetime.fromtimestamp(ts)
                dt_str = dt_j.strftime('%Y/%m/%d %H:%M:%S')

                current_group = {
                    'key': group_key,
                    'row': [
                        row_index,
                        log.get('plc__name') or log.get('plc__device_id'),
                        log.get('roll__roll_number'),
                        dt_str
                    ] + [''] * len(all_keys),
                    'data': {}
                }

            # merge json
            json_data = log.get('json_data') or {}
            for i, key in enumerate(all_keys):
                if key in json_data:
                    current_group['row'][4 + i] = json_data[key]

        # flush last group
        if current_group:
            yield writer.writerow(current_group['row'])

    response = StreamingHttpResponse(stream(), content_type='text/csv;charset=utf-8')
    response['Content-Disposition'] = f'attachment; filename={filename}'
    return response