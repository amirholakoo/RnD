import os, time, requests
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

PLC_250_URL = "http://192.168.2.46:6010/api/settings/"
s = socket.socket()
s.bind(("0.0.0.0", 2000))
s.listen(5)
s.settimeout(1.0)

print("================= PLC TCP server started ================")

try:
    while True:
        update_health(server=True)
        try:
            conn, addr = s.accept()
            raw_data = conn.recv(1024)
            if raw_data:
                print(f"Raw bytes length: {len(raw_data)}")

                PLC_KEYS = PLC_Keys.objects.all().order_by("CreationDateTime")

                plc = PLC.objects.first()
                if not plc:
                    plc = PLC(device_id="plc_1", setting={})
                    plc.save()

                if not plc.setting:
                    plc.setting = {}

                data_dict = {}
                offset = 0

                for key in PLC_KEYS:

                    dtype = (key.value or "").lower()

                    try:

                        if dtype in ["int", "sint"]:
                            value = struct.unpack(">h", raw_data[offset:offset+2])[0]
                            offset += 2

                        elif dtype in ["uint", "word"]:
                            value = struct.unpack(">H", raw_data[offset:offset+2])[0]
                            offset += 2

                        elif dtype in ["float", "real"]:
                            raw_bytes = raw_data[offset:offset+4]
                            offset += 4
                            
                            try:
                                val1 = struct.unpack(">f", raw_bytes)[0]
                            except:
                                val1 = None
                                
                            try:
                                val2 = struct.unpack("<f", raw_bytes)[0]
                            except:
                                val2 = None
                                
                            try:
                                swapped_word = raw_bytes[2:4] + raw_bytes[0:2]
                                val3 = struct.unpack(">f", swapped_word)[0]
                            except:
                                val3 = None
                                
                            try:
                                swapped_byte = raw_bytes[::-1]
                                val4 = struct.unpack(">f", swapped_byte)[0]
                            except:
                                val4 = None

                            candidates = [v for v in [val1, val2, val3, val4] if v is not None and -100 < v < 100]
                            
                            if candidates:
                                value = candidates[0]
                            else:
                                value = 0.0 
                                print(f"Warning: No valid float found for {key.key}. Raw: {raw_bytes.hex()}")

                            value = int(round(value, 2))

                        elif dtype in ["bool", "boolean", "bit"]:
                            byte_val = raw_data[offset]
                            value = bool(byte_val)
                            offset += 2

                        elif dtype in ["dint"]:
                            value = struct.unpack(">i", raw_data[offset:offset+4])[0]
                            offset += 4

                        elif dtype in ["udint"]:
                            value = struct.unpack(">I", raw_data[offset:offset+4])[0]
                            offset += 4

                        else:
                            # default fallback
                            value = struct.unpack(">H", raw_data[offset:offset+2])[0]
                            offset += 2

                        data_dict[key.key] = value

                        print(f"{key.key} ({dtype}) = {value}")

                    except Exception as ex:
                        print(f"Decode error for {key.key}: {ex}")
                        break
                
                try:
                    new_keys_from_pm250 = ["density","Diluent","Top_Fan_Pump","Bottom_Fan_Pump","Top_PS_In","Top_PS_Out","Top_PS_Re","Bottom_PS_In","Bottom_PS_Out","Bottom_PS_Re"]
                    for new_key in new_keys_from_pm250:
                        if not PLC_Keys.objects.filter(key=new_key).first():
                            new_key_object = PLC_Keys(name=new_key,
                                               key=new_key)
                            new_key_object.save()
                    # get data from plc 250 start
                    plc_250_response = requests.get(PLC_250_URL)
                    plc_250_data = plc_250_response.json()["data"][0]["setting"]
                    data_dict["density"] = int(plc_250_data["d"])
                    data_dict["Diluent"] = int(plc_250_data["n2"])

                    data_dict["Top_Fan_Pump"] = int(plc_250_data["vpu"])
                    data_dict["Bottom_Fan_Pump"] = int(plc_250_data["vpv"])

                    data_dict["Top_PS_In"] = int(plc_250_data["bti"])
                    data_dict["Top_PS_Out"] = int(plc_250_data["bto"])
                    data_dict["Top_PS_Re"] = int(plc_250_data["btr"])
                    data_dict["Bottom_PS_In"] = int(plc_250_data["bbi"])
                    data_dict["Bottom_PS_Out"] = int(plc_250_data["bbo"])
                    data_dict["Bottom_PS_Re"] = int(plc_250_data["bbr"])
                    # get data from plc 250 end
                except Exception as ex:
                    print(ex)
                    
                plc_log = PLC_Logs(
                    plc=plc,
                    data=str(raw_data),
                    json_data=data_dict
                )
                plc_log.save()

                plc.setting.update(data_dict)
                plc.save()
                update_health(plc=True)
                conn.send(b"OK")
            conn.close()

        except socket.timeout:
            continue

        except Exception as e:
            print(f"Error: {e}")

except KeyboardInterrupt:
    print("\nShutting down PLC TCP server...")
    s.close()

finally:
    s.close()