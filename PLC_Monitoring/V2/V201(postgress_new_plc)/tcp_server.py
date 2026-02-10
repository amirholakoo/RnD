import os, time
import django
import socket
import struct

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
django.setup()

from PLC_Monitoring.models import *
plc_key_names = ["1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20"]
s = socket.socket()
s.bind(("0.0.0.0", 2000))
s.listen(5)
s.settimeout(1.0)

print("PLC TCP server started")

try:
    while True:
        try:
            conn, addr = s.accept()
        except socket.timeout:
            continue

        #data = conn.recv(1024).decode().strip()
        data = conn.recv(1024)
        #words = struct.unpack(f'<{len(data)//2}H', data)
        #print(f"================== {words} ===================")
        #words = struct.unpack(f'!{len(data)//2}H', data)
        #print(f"================== {words} ===================")
        data = struct.unpack(f'>{len(data)//2}H', data)
        #print(f"================== {data} ===================")
        #floats = struct.unpack('<f', data[4:8])
        #print(f"================== {floats} ===================")
        # print(type(data))
        print(f"=============== {data} =================")
        # for k in plc_key_names:
        #     get_or_creat_plc_key, created = PLC.objects.get_or_create(key=k)
        PLC_KEYS = PLC_Keys.objects.all().order_by("CreationDateTime")
        plc = PLC.objects.first()
        if not plc:
            plc = PLC(device_id="plc_1",
                      setting={})
            plc.save()
        if not plc.setting:
            plc.setting = {}
        data_dict = {}
        i =0
        for x in PLC_KEYS:
            print(x.key)
            try:
                data_dict[x.key] = data[i]
                i += 1
            except Exception as ex:
                print(ex)
                break

        plc_log = PLC_Logs(plc=plc,
                           data=data,
                           json_data=data_dict)
        plc_log.save()
        plc.setting.update(data_dict)
        plc.save()
        # values = dict(x.split("=") for x in data.split(";"))

        # plc = PLC.objects.filter(device_id=values["device_id"]).last()
        # if not plc:
        #     plc = PLC(device_id=values["device_id"])
        #     plc.save()

        # PLC_Logs.objects.create(plc=plc, data=values)

        conn.send(b"OK")
        conn.close()

except KeyboardInterrupt:
    print("\nShutting down PLC TCP server...")
    s.close()
finally:
    s.close()

