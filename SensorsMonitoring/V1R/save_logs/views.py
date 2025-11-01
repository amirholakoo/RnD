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
            print("data received from device successfully",latest_data)
            device = Device.objects.filter(device_id=latest_data["device_id"]).last()
            if not device:
                device = Device(device_id=latest_data["device_id"],ip_address=ip)
                device.save()
                sensor = Device_Sensor(device=device,sensor_type=latest_data["sensor_type"])
                sensor.save()
            else:
                device.ip_address = ip
                device.save()
                sensor = Device_Sensor.objects.filter(device=device,sensor_type=latest_data["sensor_type"]).last()
                if not sensor:
                    sensor = Device_Sensor(device=device,sensor_type=latest_data["sensor_type"])
                    sensor.save()

            if device.id == 1:
                open("device.txt","a").write(f"{latest_data}\n{device.device_id} - {device.ip_address}\n{sensor.sensor_type}\n__________________________\n")
            new_log = SensorLogs(sensor=sensor,data=latest_data["data"])
            new_log.save()
            print("saved")
            return JsonResponse({'status': 'ok'})
        except Exception as e:
            print("Error:", str(e))
            return JsonResponse({'status': 'error', 'message': str(e)}, status=400)
    else:
        return JsonResponse({'status': 'error', 'message': 'Only POST allowed'}, status=405)


