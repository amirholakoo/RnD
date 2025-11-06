from save_logs.models import *
import time

def check_fake_data(get_response):
    print("Custom middleware initialized.")

    def middleware(request):
        response = get_response(request)
        if request.method == "POST":
            print("Checking fake data")
            target = Device_Sensor.objects.all()
            for sensor in target:
                if sensor.sensor_logs.count() < 10 and (time.time() - sensor.sensor_logs.first().CreationDateTime) > 60*30:
                    if not sensor.Is_AI:
                        print(f"Fake data detected for sensor {sensor.sensor_type} with device: {sensor.device}")
                        sensor.delete()
        return response

    return middleware