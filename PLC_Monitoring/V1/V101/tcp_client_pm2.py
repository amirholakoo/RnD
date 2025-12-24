import os,json,requests
import django
import socket
import time

# Django setup
os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
django.setup()

from PLC_Monitoring.models import *

# PLC Configuration
PLC_IP = "192.168.1.10"
PLC_PORT = 8001
TIMEOUT = 2       # seconds
INTERVAL = 1      # seconds between sends
THERMAL_API_URL = "http://192.168.2.22:6006/view/api/plc_data/"
# Command to send
def build_command():
    # Example: you can make dynamic
    return "CMD=STATUS;USER=admin"

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
                    PLC_Logs.objects.create(plc=plc_obj,
                                            data=response,
                                            json_data=data)
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
