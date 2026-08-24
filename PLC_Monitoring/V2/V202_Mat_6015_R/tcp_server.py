import os, time
import django
import socket
import struct

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
django.setup()

from PLC_Monitoring.models import *


def update_health(server=None, plc=None):
    obj = TCP_CONNECTION.objects.first()
    if not obj: obj = TCP_CONNECTION()
    
    if server: obj.ServerLastUpdate = time.time()
    if plc: obj.ClientLastUpdate = time.time()
    obj.save()


s = socket.socket()
s.bind(("0.0.0.0", 3000))
s.listen(5)
s.settimeout(1.0)

print("================= PLC TCP server started ================")

try:
    while True:
        update_health(server=True)
        try:
            conn, addr = s.accept()
            raw_data = conn.recv(1024)
            
            print(f"1-raw_data: {raw_data}")
            if raw_data:
                update_health(plc=True)
                clean_bytes = raw_data.replace(b'\x00', b'')
                raw_data = clean_bytes.decode("utf-8", errors="ignore").strip()
                
                print(f"Cleaned raw_data: {raw_data}")
                if "ERROR" in raw_data:
                    continue
                last_log = PLC_Logs.objects.order_by("-CreationDateTime").first()
                raw_data = "n=MAT-MAKING;" + raw_data
                print(raw_data)
                if last_log and raw_data == last_log.data:
                    last_log.save()
                    continue
                    
                if raw_data.startswith("n=") and "=" in raw_data:
                    data = dict(item.split("=", 1) for item in raw_data.split(";") if "=" in item)
                    plc_obj, created = PLC.objects.get_or_create(device_id=data["n"])

                    # check if the log is already in the database
                    incoming_keys = data.keys()
                    print(incoming_keys)
                    is_same = True
                    if PLC_Logs.objects.count() > 0:
                        print("============")
                        is_same_log = (
                            PLC_Logs.objects
                            .order_by("-CreationDateTime")
                            .filter(plc=plc_obj, json_data__has_key=list(incoming_keys)[1])
                            .first()
                        )
                        print(is_same_log)
                        if is_same_log is not None:
                            for key in incoming_keys:
                                old_value = is_same_log.json_data.get(key)
                                if old_value != data[key]:
                                    is_same = False
                                    break
                        else:
                            is_same = False
                    else:
                        is_same = False
                    print(is_same)
                    if is_same:
                        continue
                    print(is_same)
                    plc_log = PLC_Logs(plc=plc_obj,
                                       data=raw_data,
                                       json_data=data)
                    plc_log.save()
                    if plc_obj.setting is None:
                        plc_obj.setting = {}

                    # save new key in plc_keys table
                    existing_keys = set(PLC_Keys.objects.filter(key__in=incoming_keys).values_list("key", flat=True))
                    missing_keys = incoming_keys - existing_keys
                    PLC_Keys.objects.bulk_create([PLC_Keys(key=key,value=str(type(key).__name__))for key in missing_keys])

                    plc_obj.setting.update(data)
                    for key, value in data.items():
                        plc_obj.setting[key] = value
                    plc_obj.save()


                else:
                    PLC_Logs.objects.create(data=raw_data)
            conn.sendall(b"OK")

        except Exception as e:
            print(f"Worker error: {e}")

except KeyboardInterrupt:
    print("\nShutting down PLC TCP server...")
    s.close()

finally:
    s.close()