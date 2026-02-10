import os, time
import django
import socket

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
django.setup()

from PLC_Monitoring.models import PLC,PLC_Logs

s = socket.socket()
s.bind(("0.0.0.0", 6009))
s.listen(5)
s.settimeout(1.0)

print("PLC TCP server started")

try:
    while True:
        try:
            conn, addr = s.accept()
        except socket.timeout:
            continue

        data = conn.recv(1024).decode().strip()
        print(data)
        values = dict(x.split("=") for x in data.split(";"))

        plc = PLC.objects.filter(device_id=values["device_id"]).last()
        if not plc:
            plc = PLC(device_id=values["device_id"])
            plc.save()

        PLC_Logs.objects.create(plc=plc, data=values)

        conn.send(b"OK")
        conn.close()

except KeyboardInterrupt:
    print("\nShutting down PLC TCP server...")
finally:
    s.close()

