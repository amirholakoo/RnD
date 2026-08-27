import os,json,requests
import django
import socket
import time
from django.utils import timezone
from django.db import connections

# Django setup
os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
django.setup()

from PLC_Monitoring.models import *

def update_health(server=None, plc=None):
    obj = TCP_CONNECTION.objects.first()
    if not obj: obj = TCP_CONNECTION()
    
    if server: obj.ServerLastUpdate = time.time()
    if plc: obj.ClientLastUpdate = time.time()
    obj.save()
# PLC Configuration
# PLC_IP = "192.168.1.10"
PLC_IP = "172.16.1.73"
PLC_PORT = 8001
TIMEOUT = 2    # seconds
INTERVAL = 1      # seconds between sends
THERMAL_API_URL = "http://192.168.2.22:6006/view/api/plc_data/"
PLC_250_URL = "http://192.168.2.46:6010/api/settings/"
is_running = None
roll_obj = None
def send_to_plc(payload: str) -> str:
    """Send payload to PLC and return response"""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(TIMEOUT)
    try:
        s.connect((PLC_IP, PLC_PORT))
        s.send(payload.encode())
        response = s.recv(1024)
        return response
    except Exception as e:
        return f"ERROR: {e}"
    finally:
        s.close()

# Main loop
def main_loop():
    global is_running
    global roll_obj
    count = 1
    while True:
        #close_old_connections()
        connections.close_all()
        update_health(server=True)
        try:
            last_log = PLC_Logs.objects.order_by("-CreationDateTime").first()
            params = {}
            if last_log and last_log.roll is not None:
                if last_log.is_running:
                    params['roll_number'] = last_log.roll.roll_number
            response = requests.get(THERMAL_API_URL, params=params)
            data = response.json()
            value = data["temperature"]
            payload = f"t{int(float(value) + 0.5)}.0"

            response = send_to_plc(payload)
            print(f"Sent: {payload} | Received: {response}")

            if response:
                if isinstance(response, bytes):
                    response = response.decode("utf-8", errors="ignore")
                response = response.strip("\x00\r\n ")
                # Save response to DB
                if "ERROR" in response:
                    continue

                last_log = PLC_Logs.objects.order_by("-CreationDateTime").first()
                if last_log and response == last_log.data:
                    last_log.LastUpdate = timezone.now()
                    last_log.save()
                    continue
                    
                if response.startswith("n=") and "=" in response:
                    count+=1
                    data = dict(item.split("=", 1) for item in response.split(";") if "=" in item)
                    print(data)
                    plc_obj, created = PLC.objects.get_or_create(device_id=data["n"])


                    try:
                        # get data from plc 250 start
                        plc_250_response = requests.get(PLC_250_URL, params=params)
                        plc_250_data = plc_250_response.json()["data"][0]["setting"]
                        print(plc_250_data)
                        data["density"] = plc_250_data["d"]
                        data["n2"] = plc_250_data["n2"]
                        # get data from plc 250 end
                    except Exception as ex:
                        print(ex)


                    # check if the log is already in the database
                    incoming_keys = data.keys()
                    new_data = {}

                    if plc_obj.setting is None:
                        plc_obj.setting = {}
                    if count <= 250:
                        for key, value in data.items():
                            if key not in plc_obj.setting:
                                new_data[key] = value
                                continue
                            if plc_obj.setting[key] != value:
                                new_data[key] = value
                        print("====================",new_data,"=======================")
                        

                        data = new_data
                    else:
                        count=1

                    if not data :
                        return
                    plc_log = PLC_Logs(plc=plc_obj,
                                       json_data=plc_obj.setting if count >= 249 else data)
                    plc_log.save()

                    if "cr" in data:
                        roll_obj, created = Rolls.objects.get_or_create(roll_number=str(int(data["cr"])))
                        # if roll_obj.plc_setting is None:
                        #     roll_obj.plc_setting = {}
                        roll_obj.roll_number = int(data["cr"])
                        roll_obj.plc_setting.update(plc_obj.setting)
                        #roll_obj.plc_setting.update(data)
                        roll_obj.save()
                        plc_log.roll = roll_obj
                        plc_log.save()
                        roll_obj.avg_final_data()
                    if "ru" in data:
                        if data["ru"] == "1":
                            plc_log.is_running = True
                            is_running = True
                        else:
                            plc_log.is_running = False
                            is_running = False
                        plc_log.save()
                    else:
                        if is_running is not None:
                            plc_log.is_running = is_running
                            plc_log.save()
                    if plc_obj.setting is None:
                        plc_obj.setting = {}

                    # save new key in plc_keys table
                    existing_keys = set(PLC_Keys.objects.filter(key__in=incoming_keys).values_list("key", flat=True))
                    missing_keys = incoming_keys - existing_keys
                    now = timezone.now()
                    PLC_Keys.objects.bulk_create([PLC_Keys(key=key,value=str(type(value).__name__),CreationDateTime=now,LastUpdate=now)for key in missing_keys])

                    plc_obj.setting.update(data)
                    for key, value in data.items():
                        plc_obj.setting[key] = value
                        if roll_obj:
                            if roll_obj.plc_setting is None:
                                roll_obj.plc_setting = {}
                            roll_obj.plc_setting[key] = value
                            roll_obj.save()

                    if roll_obj:
                        plc_log.roll = roll_obj
                        plc_log.save()
                        if roll_obj.plc is None:
                            roll_obj.plc = plc_obj
                            roll_obj.save()
                        if "me1" in data:
                            if not roll_obj.Printed_length > int(data["me1"]):
                                roll_obj.Printed_length = int(data["me1"])
                                roll_obj.save()
                        if "b" in data and data["b"] == "1":
                            new_break = int(data["b"])
                            last_break_log = None
                            if PLC_Logs.objects.count() > 0:
                                last_break_log = (
                                    PLC_Logs.objects
                                    .filter(plc=plc_obj, json_data__has_key="b")
                                    .exclude(id=plc_log.id)
                                    .order_by("-CreationDateTime")
                                    .first()
                                )

                            old_break = None
                            if last_break_log is not None and last_break_log and last_break_log.json_data:
                                old_break = int(last_break_log.json_data.get("b", 0))
                            if old_break != new_break and new_break == 1:
                                roll_obj.Paper_breaks += 1
                                roll_obj.save()
                                Roll_Breaks.objects.create(roll=roll_obj)
                    else:
                        roll_number = plc_obj.setting.get("cr")
                        if roll_number is not None:
                            roll_obj, _ = Rolls.objects.get_or_create(roll_number=int(roll_number))
                            plc_log.roll = roll_obj
                            plc_log.save()
                    plc_obj.save()


                else:
                    PLC_Logs.objects.create(data=response)
                
                update_health(plc=True)
        except Exception as e:
            print(f"Worker error: {e}")
        finally:
            connections.close_all()
        time.sleep(INTERVAL)

if __name__ == "__main__":
    main_loop()
