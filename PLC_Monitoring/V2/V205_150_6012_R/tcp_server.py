import os, time
import django
import socket
import struct

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
django.setup()

from PLC_Monitoring.models import *

s = socket.socket()
s.bind(("0.0.0.0", 2000))
s.listen(5)
s.settimeout(1.0)

print("================= PLC TCP server started ================")

try:
    while True:
        try:
            conn, addr = s.accept()
        except socket.timeout:
            continue

        raw_data = conn.recv(1024)

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

                # elif dtype in ["float", "real"]:
                #     value = struct.unpack(">f", raw_data[offset:offset+4])[0]
                #     value = round(value, 2)
                #     offset += 4
                # elif dtype in ["float", "real"]:
                #     b = raw_data[offset:offset+4]
                #     # word swap (common in PLC)
                #     b = b[2:4] + b[0:2]
                #     value = struct.unpack(">f", b)[0]
                #     value = round(value, 2)
                #     offset += 4
                # elif dtype in ["float", "real"]:
                #     b = raw_data[offset:offset+4]

                #     candidates = {
                #         "big": struct.unpack(">f", b)[0],
                #         "little": struct.unpack("<f", b)[0],
                #         "word_swap": struct.unpack(">f", b[2:4] + b[0:2])[0],
                #         "byte_swap": struct.unpack(">f", b[::-1])[0],
                #     }

                #     # یکی را موقتاً چاپ کن تا درستش را پیدا کنی
                #     print(f"{key.key} raw={b.hex()} candidates={candidates}")

                #     # فعلاً مثلاً word_swap را انتخاب کن یا بعداً درستش را بگذار
                #     value = round(candidates["byte_swap"], 2)
                #     offset += 4
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

        plc_log = PLC_Logs(
            plc=plc,
            data=str(raw_data),
            json_data=data_dict
        )
        plc_log.save()

        plc.setting.update(data_dict)
        plc.save()

        conn.send(b"OK")
        conn.close()

except KeyboardInterrupt:
    print("\nShutting down PLC TCP server...")
    s.close()

finally:
    s.close()