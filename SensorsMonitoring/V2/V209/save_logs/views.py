from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.shortcuts import render
from .models import *
import json
latest_data = None

# d = Device.objects.all().first()
# d.Is_Known = False
# d.save()
# for x in range(1,10):
#     device = Device(device_id=f"device_{x}",ip_address=f"192.168.1.{x}",location=f"location_{x}",name=f"device_{x}",Is_Known=True,description=f"این دستگاه در انبار سنگین استفاده میشود")
#     device.save()
#     for z in range(1,10):
#         sensor = Device_Sensor(device=device,sensor_type=f"sensor_{z}")
#         sensor.save()
#         for y in range(1,100):
#             log = SensorLogs(sensor=sensor,data="{'device_id':'1000_sensor_{z}','data':{'temperature':100,'humidity':100,'pressure':100}}")
#             log.save()

# log = SensorLogs(sensor=Device_Sensor.objects.all().first(),data="{'device_id':'1000_sensor_{z}','data':{'temperature':100,'humidity':100,'pressure':100}}")
# log.save()

# target = Device.objects.all().first()
# target.Is_Known = False
# target.save()

@csrf_exempt
def post_data(request):
    global latest_data
    if request.method == "POST":
        x_forwarded_for = request.META.get('HTTP_X_FORWARDED_FOR')
        if x_forwarded_for:
            ip = x_forwarded_for.split(',')[0]
        else:
            ip = request.META.get('REMOTE_ADDR')
            
        try:
            raw = request.body.decode('utf-8')
            latest_data = raw
            latest_data = json.loads(latest_data)
            IS_AI = False
            if  "is_ai" in latest_data:
                if latest_data["is_ai"] == True:
                    IS_AI = True
                    
            print("data received from device successfully",latest_data)
            device = Device.objects.filter(device_id=latest_data["device_id"]).last()
            if not device:
                device = Device(device_id=latest_data["device_id"],Is_AI=True if IS_AI else False,AI_Target=latest_data["target_device_id"] if IS_AI else None,ip_address=ip)
                device.save()
                sensor = Device_Sensor(device=device,sensor_type=latest_data["sensor_type"],Is_AI=True if IS_AI else False,AI_Target=latest_data["sensor_target"] if IS_AI else None)
                sensor.save()
            else:
                device.ip_address = ip
                device.save()
                sensor = Device_Sensor.objects.filter(device=device,sensor_type=latest_data["sensor_type"]).last()
                if not sensor:
                    sensor = Device_Sensor(device=device,sensor_type=latest_data["sensor_type"],Is_AI=True if IS_AI else False,AI_Target=latest_data["sensor_target"] if IS_AI else None)
                    sensor.save()

            if IS_AI:
                for key, value in latest_data["data"].items():
                    target_values = value[:3]
                    creation_time = time.time()+3600
                    now = time.time()
                    last_log = SensorLogs.objects.filter(sensor=sensor).last()
                    if last_log:
                        if creation_time - last_log.CreationDateTime >= 3300:
                            for x in target_values:
                                new_log = SensorLogs(sensor=sensor,CreationDateTime=creation_time,data={"temperature":x})
                                new_log.save()
                                new_log.LastUpdate = creation_time
                                new_log.save()
                                creation_time += 3600
                    else:
                        for x in target_values:
                            new_log = SensorLogs(sensor=sensor,CreationDateTime=creation_time,data={"temperature":x})
                            new_log.save()
                            new_log.LastUpdate = creation_time
                            new_log.save()
                            creation_time += 3600
            else:
                new_log = SensorLogs(sensor=sensor,data=latest_data["data"])
                new_log.save()
            print("saved")
            return JsonResponse({'status': 'ok'})
        except Exception as e:
            print("Error:", str(e))
            return JsonResponse({'status': 'error', 'message': str(e)}, status=400)
    else:
        return JsonResponse({'status': 'error', 'message': 'Only POST allowed'}, status=405)

# import jdatetime
# import datetime
# print(str(jdatetime.datetime.fromgregorian(datetime=datetime.datetime.fromtimestamp(float(SensorLogs.objects.filter(sensor__id=317).last().CreationDateTime))).strftime(" %m-%d %H:%M")))