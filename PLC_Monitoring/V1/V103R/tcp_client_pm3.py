import os,json,requests
import django
import socket
import time

# Django setup
os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
django.setup()

from PLC_Monitoring.models import *

# PLC Configuration
# PLC_IP = "192.168.1.10"
PLC_IP = "172.16.1.40"
PLC_PORT = 8000
TIMEOUT = 2       # seconds
INTERVAL = 1      # seconds between sends
THERMAL_API_URL = "http://192.168.2.22:6002/view/api/plc_data/"
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
    while True:
        try:
            response = requests.get(THERMAL_API_URL)
            data = response.json()
            value = data["temperature"]
            payload = f"t{value:.2f}"
            print(len(payload))
            print(payload)
            response = send_to_plc(payload)
            print(f"Sent: {payload} | Received: {response}")

            if response:
                if isinstance(response, bytes):
                    response = response.decode("utf-8", errors="ignore")
                response = response.strip("\x00\r\n ")
                # Save response to DB
                last_log = PLC_Logs.objects.order_by("-CreationDateTime").first()
                if last_log and response == last_log.data:
                    last_log.LastUpdate = time.time()
                    last_log.save()
                    continue
                if response.startswith("n=") and "=" in response:
                    data = dict(item.split("=", 1) for item in response.split(";") if "=" in item)
                    print(data)
                    plc_obj, created = PLC.objects.get_or_create(device_id=data["n"])
                    plc_log = PLC_Logs(plc=plc_obj,
                                       data=response,
                                       json_data=data)
                    plc_log.save()

                    if "cr" in data:
                        roll_obj, created = Rolls.objects.get_or_create(roll_number=str(int(data["cr"])))
                        roll_obj.roll_number = int(data["cr"])
                        roll_obj.plc_setting = data
                        roll_obj.save()
                        plc_log.roll = roll_obj
                        plc_log.save()
                    if "ru" in data:
                        if data["ru"] == "1":
                            plc_log.is_running = True
                        else:
                            plc_log.is_running = False
                        plc_log.save()
                    else:
                        if not is_running is None:
                            plc_log.is_running = is_running
                            plc_log.save()
                    if plc_obj.setting is None:
                        plc_obj.setting = {}

                    incoming_keys = data.keys()
                    existing_keys = set(PLC_Keys.objects.filter(key__in=incoming_keys).values_list("key", flat=True))
                    missing_keys = incoming_keys - existing_keys
                    now = time.time()
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
                        if roll_obj.plc is None:
                            roll_obj.plc = plc_obj
                            roll_obj.save()
                        if "me1" in data:
                            roll_obj.Printed_length = int(data["me1"])
                            roll_obj.save()
                        if "b" in data:
                            new_break = int(data["b"])

                            last_break_log = (
                                PLC_Logs.objects
                                .filter(plc=plc_obj, json_data__has_key="b")
                                .exclude(id=plc_log.id)
                                .order_by("-CreationDateTime")
                                .first()
                            )

                            old_break = None
                            if last_break_log and last_break_log.json_data:
                                old_break = int(last_break_log.json_data.get("b", 0))
                        
                            if old_break != new_break and new_break == 1:
                                roll_obj.Paper_breaks += 1
                                roll_obj.save()
                                Roll_Breaks.objects.create(roll=roll_obj,break_time=time.time(),break_reason="Paper break")

                    plc_obj.save()


                else:
                    PLC_Logs.objects.create(data=response)
                # plc_obj = PLC.objects.get_or_create(device_id="PLC1")  # Adjust device_id
                # PLC_Logs.objects.create(plc=plc_obj, data={"sent": payload, "received": response})
                if PLC_Logs.objects.count() > 10000:
                    delted_count, _ = PLC_Logs.objects.all().delete()
                    print(f"Deleted {delted_count} logs")
        except Exception as e:
            print(f"Worker error: {e}")

        time.sleep(INTERVAL)

if __name__ == "__main__":
    main_loop()
