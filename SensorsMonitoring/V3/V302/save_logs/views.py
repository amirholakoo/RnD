from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.db import transaction
from .models import *
from .cache import device_cache
import json

latest_data = None
request_counter = 0
CHECK_ROTATION_EVERY = 1000

def check_database_rotation():
    """Check if database rotation is needed"""
    try:
        from DatabaseGuardian.models import DatabaseConfig
        from DatabaseGuardian.rotation_manager import RotationManager
        
        config = DatabaseConfig.get_config()
        if config.auto_rotate:
            manager = RotationManager()
            needs_rotation, reason = manager.check_rotation_needed()
            if needs_rotation:
                manager.create_new_database()
                device_cache.clear()
                return True
    except Exception as e:
        print(f"Rotation check error: {e}")
    return False

@csrf_exempt
def post_data(request):
    """Optimized sensor data endpoint with caching and single transaction"""
    global latest_data, request_counter
    if request.method != "POST":
        return JsonResponse({'status': 'error', 'message': 'Only POST allowed'}, status=405)
    
    # Check rotation every N requests
    request_counter += 1
    if request_counter >= CHECK_ROTATION_EVERY:
        request_counter = 0
        check_database_rotation()
    
    x_forwarded_for = request.META.get('HTTP_X_FORWARDED_FOR')
    ip = x_forwarded_for.split(',')[0] if x_forwarded_for else request.META.get('REMOTE_ADDR')
            
    try:
        raw = request.body.decode('utf-8')
        latest_data = json.loads(raw)
        
        IS_AI = latest_data.get("is_ai", False)
        device_id = latest_data["device_id"]
        sensor_type = latest_data["sensor_type"]
        
        with transaction.atomic():
            # Check cache first
            device = device_cache.get_device(device_id)
            
            if not device:
                device = Device.objects.filter(device_id=device_id).first()
                
            if not device:
                device = Device(
                    device_id=device_id,
                    Is_AI=IS_AI,
                    AI_Target=latest_data.get("target_device_id") if IS_AI else None,
                    ip_address=ip
                )
                device.save()
                device_cache.set_device(device_id, device)
                
                sensor = Device_Sensor(
                    device=device,
                    sensor_type=sensor_type,
                    Is_AI=IS_AI,
                    AI_Target=latest_data.get("sensor_target") if IS_AI else None
                )
                sensor.save()
                device_cache.set_sensor(device_id, sensor_type, sensor)
            else:
                # Update IP only if changed
                if device.ip_address != ip:
                    device.ip_address = ip
                    device.save(update_fields=['ip_address', 'LastUpdate'])
                    device_cache.set_device(device_id, device)
                
                # Check sensor cache
                sensor = device_cache.get_sensor(device_id, sensor_type)
                
                if not sensor:
                    sensor = Device_Sensor.objects.filter(device=device, sensor_type=sensor_type).first()
                    
                if not sensor:
                    sensor = Device_Sensor(
                        device=device,
                        sensor_type=sensor_type,
                        Is_AI=IS_AI,
                        AI_Target=latest_data.get("sensor_target") if IS_AI else None
                    )
                    sensor.save()
                    
                device_cache.set_sensor(device_id, sensor_type, sensor)

            # Save logs
            now = time.time()

            if IS_AI:
                for key, value in latest_data["data"].items():
                    target_values = value[:3]
                    creation_time = now + 3600
                    
                    last_log = SensorLogs.objects.filter(sensor=sensor).order_by('-CreationDateTime').first()
                    
                    if not last_log or (creation_time - last_log.CreationDateTime >= 3300):
                        logs_to_create = []
                        for x in target_values:
                            logs_to_create.append(SensorLogs(
                                sensor=sensor,
                                CreationDateTime=creation_time,
                                LastUpdate=creation_time,
                                data=json.dumps({"temperature": x})
                            ))
                            creation_time += 3600
                        SensorLogs.objects.bulk_create(logs_to_create)
            else:
                SensorLogs.objects.create(
                    sensor=sensor,
                    data=json.dumps(latest_data["data"]) if isinstance(latest_data["data"], dict) else latest_data["data"],
                    CreationDateTime=now,
                    LastUpdate=now
                )
        
            return JsonResponse({'status': 'ok'})
        
    except Exception as e:
        print("Error:", str(e))
        return JsonResponse({'status': 'error', 'message': str(e)}, status=400)