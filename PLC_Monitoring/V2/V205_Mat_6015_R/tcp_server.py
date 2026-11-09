import os, time, django, jdatetime, socket, struct
from datetime import datetime
from django.utils import timezone
from django.utils.dateparse import parse_datetime
from django.db import transaction

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
django.setup()

from PLC_Monitoring.models import *


def _to_number(value):
    """convert weight to number"""
    if value is None or value == "":
        return None

    try:
        number = float(value)
        return int(number) if number.is_integer() else number
    except (TypeError, ValueError):
        return None

def _get_temperature(plc_obj, temperature_keys):
    if not temperature_keys:
        return None

    key = temperature_keys[0]
    print(f"[TEMP] key={key}, value={value!r}")
    print(f"[TEMP] موجود در setting: {key in plc_obj.setting}")

    return _to_number((plc_obj.setting or {}).get(key))


def _update_temperature_details(step, plc_obj):
    temperature_keys = step.get("temperature_keys")

    if not temperature_keys:
        return

    temperature_end = _get_temperature(
        plc_obj,
        temperature_keys,
    )

    temperature_start = _to_number(
        step.get("temperature_start")
    )

    step["temperature_end"] = temperature_end

    step["temperature_change"] = (
        temperature_end - temperature_start
        if temperature_end is not None and temperature_start is not None
        else None
    )

def _get_weight(plc_obj, tank):
    """get weight from plc setting"""
    weight_key = TANK_CONFIG[tank]["weight_key"]
    return _to_number((plc_obj.setting or {}).get(weight_key))


def _generate_material_number(plc_obj, tank, now):
    """
    output example : T1050727102
    T + Tank number (1-4) + Date YYMMDD + record number
    """
    jalali_now = jdatetime.datetime.fromgregorian(
        datetime=timezone.localtime(now)
    )
    date_part = jalali_now.strftime("%y%m%d")

    sequence = Materials.objects.filter(
        plc=plc_obj,
        tank=tank,
    ).count() + 1

    return f"T{tank}{date_part}{sequence}"


def _close_current_step(material, now, plc_obj):
    """close step - save last weight and temp"""
    details = material.steps_details or {}
    steps = details.get("steps", [])

    if not steps:
        return

    current_step = steps[-1]

    if current_step.get("end"):
        return

    current_step["end"] = now.isoformat()

    start_time = datetime.fromisoformat(current_step["start"])

    current_step["duration_seconds"] = max(
        0,
        int((now - start_time).total_seconds())
    )

    # last weight
    weight_end = _get_weight(plc_obj, material.tank)
    weight_start = _to_number(current_step.get("weight_start"))

    current_step["weight_end"] = weight_end
    current_step["weight_change"] = (
        weight_end - weight_start
        if weight_end is not None and weight_start is not None
        else None
    )

    current_step.setdefault("weight_details", [])

    # calculate temp only if temperature_key exist
    _update_temperature_details(current_step, plc_obj)

    material.steps_details = details
    material.LastUpdate = now
    material.save(update_fields=["steps_details", "LastUpdate"])


def _finish_material(material, now):
    """ End - calculate duration """
    details = material.steps_details or {}
    details["ended_at"] = now.isoformat()

    started_at = details.get("started_at")
    if started_at:
        start_time = datetime.fromisoformat(started_at)
        material.duration = max(
            0, int((now - start_time).total_seconds())
        )

    material.steps_details = details
    material.LastUpdate = now
    material.save(
        update_fields=["steps_details", "duration", "LastUpdate"]
    )


def _start_material(plc_obj, tank, status, now):
    """ create material record in database """
    config = TANK_CONFIG[tank]
    step_config = config["steps"].get(status)

    if not step_config:
        return None

    material = Materials.objects.create(
        plc=plc_obj,
        tank=tank,
        material_number=_generate_material_number(plc_obj, tank, now),
        duration=0,
        steps_details={
            "started_at": now.isoformat(),
            "ended_at": None,
            "steps": [],
        },
    )

    _start_step(material, plc_obj, status, now)
    return material


def _start_step(material, plc_obj, status, now):
    """ start new step and set weight/temp """
    config = TANK_CONFIG[material.tank]
    step_config = config["steps"].get(status)

    if not step_config:
        return

    details = material.steps_details or {}
    steps = details.setdefault("steps", [])

    step_data = {
        "status": status,
        "name": step_config["name"],
        "start": now.isoformat(),
        "end": None,
        "duration_seconds": None,

        "weight_start": _get_weight(plc_obj, material.tank),
        "weight_end": None,
        "weight_change": None,
        "weight_keys": list(step_config.get("weight_keys", [])),
        "weight_details": [],
    }
    
    # ثبت دما فقط برای مراحلی که temperature_keys دارند
    temperature_keys = step_config.get("temperature_keys", [])

    if temperature_keys:
        temperature_start = _get_temperature(
            plc_obj,
            temperature_keys,
        )

        step_data.update({
            "temperature_keys": list(temperature_keys),
            "temperature_start": temperature_start,
            "temperature_end": temperature_start,
            "temperature_change": (
                0 if temperature_start is not None else None
            ),
        })
    
    steps.append(step_data)

    material.steps_details = details
    material.LastUpdate = now
    material.save(update_fields=["steps_details", "LastUpdate"])

def _update_current_step(material, now, plc_obj):
    """به‌روزرسانی زنده زمان و تغییر وزن مرحله جاری."""

    details = material.steps_details or {}
    steps = details.get("steps", [])

    if not steps:
        return

    current_step = steps[-1]

    # مرحله بسته‌شده نباید به‌روزرسانی شود.
    if current_step.get("end"):
        return

    try:
        start_time = datetime.fromisoformat(current_step["start"])
    except (TypeError, ValueError, KeyError):
        return

    # محاسبه زمان سپری‌شده
    current_step["duration_seconds"] = max(
        0,
        int((now - start_time).total_seconds())
    )

    # محاسبه تغییر وزن نسبت به ابتدای مرحله
    weight_start = _to_number(current_step.get("weight_start"))
    weight_end = _get_weight(plc_obj, material.tank)

    current_step["weight_end"] = weight_end
    current_step["weight_change"] = (
        weight_end - weight_start
        if weight_end is not None and weight_start is not None
        else None
    )

    current_step.setdefault("weight_details", [])
    _update_temperature_details(current_step, plc_obj)
    # ثبت تغییرات در دیتابیس
    material.steps_details = details
    material.LastUpdate = now

    material.save(update_fields=["steps_details", "LastUpdate"])

@transaction.atomic
def process_material_step(plc_obj, tank, status):
    """
    ثبت تغییر وضعیت مراحل پخت.

    status = 0:
        پایان پخت فعال

    status = 1 در حالی که پخت دیگری فعال است:
        پایان پخت قبلی و شروع پخت جدید

    سایر وضعیت‌های غیرصفر:
        بستن مرحله قبلی و شروع مرحله جدید
    """
    try:
        tank = int(tank)
        status = int(status)
    except (TypeError, ValueError):
        return

    if tank not in TANK_CONFIG:
        return

    now = timezone.now()

    # رکوردهای این مخزن را بررسی می‌کنیم تا آخرین پخت باز پیدا شود.
    records = Materials.objects.filter(
        plc=plc_obj,
        tank=tank,
        Is_Deleted=False,
    ).order_by("-CreationDateTime", "-pk")

    active_material = None

    for record in records:
        details = record.steps_details or {}
        if details.get("ended_at") is None and details.get("steps"):
            active_material = record
            break

    # وضعیت صفر یعنی خاموش؛ پخت فعال را می‌بندیم.
    if status == 0:
        if active_material:
            _close_current_step(active_material, now, plc_obj)
            _finish_material(active_material, now)
        return

    # وضعیت باید در دیکشنری مراحل مخزن تعریف شده باشد.
    if status not in TANK_CONFIG[tank]["steps"]:
        return

    # اگر هنوز پختی نداریم، اولین پخت را شروع می‌کنیم.
    if active_material is None:
        _start_material(plc_obj, tank, status, now)
        return

    details = active_material.steps_details or {}
    steps = details.get("steps", [])
    current_status = steps[-1].get("status") if steps else None

    # وضعیت تغییری نکرده؛ مرحله تکراری نساز.
    if current_status == status:
        _update_current_step(active_material,now,plc_obj)
        return

    # رفتن از هر وضعیت غیرصفر به وضعیت 1 یعنی پخت قبلی تمام و پخت جدید شروع شود.
    if status == 1:
        _close_current_step(active_material, now, plc_obj)
        _finish_material(active_material, now)
        _start_material(plc_obj, tank, status, now)
        return

    # انتقال معمولی بین مراحل یک پخت
    _close_current_step(active_material, now, plc_obj)
    _start_step(active_material, plc_obj, status, now)


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
            print(conn)
            print(f"1-raw_data: {raw_data}")
            if raw_data:
                update_health(plc=True)
                clean_bytes = raw_data.replace(b'\x00', b'')
                raw_data = clean_bytes.decode("utf-8", errors="ignore").strip()
                if "ERROR" in raw_data:
                    continue
                last_log = PLC_Logs.objects.order_by("-CreationDateTime").first()
                raw_data = "n=MAT-MAKING;" + raw_data
                print("new raw_data recived")
                if last_log and raw_data == last_log.data:
                    last_log.save()
                    continue
                    
                if raw_data.startswith("n=") and "=" in raw_data:
                    data = dict(item.split("=", 1) for item in raw_data.split(";") if "=" in item)
                    plc_obj, created = PLC.objects.get_or_create(device_id=data["n"])


                    # =========================
                    # Convert WA bits to warnings
                    # =========================

                    bit_data = {}

                    for key, value in data.items():

                        if key in ("wa1", "wa2", "wa3", "wa4", "wa5","wa6","wa7","wa8","wa9","wb1"):
                            try:
                                word_value = int(value)

                                # Each WA contains 16 bits: bit 0 to bit 15
                                if not (0 <= word_value) or not (word_value <= 65535):
                                    print(f"Invalid 16-bit value: {key}={value}")
                                    continue

                                for bit in range(16):
                                    bit_key = f"{key}-{bit}"
                                    bit_data[bit_key] = (word_value >> bit) & 1

                            except (ValueError, TypeError):
                                print(f"Invalid WA value: {key}={value}")

                    # Remove original WA values

                    # for key in ("wa1", "wa2", "wa3", "wa4", "wa5"):
                    #     data.pop(key, None)

                    # Add individual bits
                    data.update(bit_data)
                    
                    # check if the log is already in the database
                    incoming_keys = data.keys()
                    new_data = {}
                    if plc_obj.setting is None:
                        plc_obj.setting = {}
                    for key, value in data.items():
                        if key not in plc_obj.setting:
                            new_data[key] = value
                            continue
                        if plc_obj.setting[key] != value:
                            new_data[key] = value
                    data = new_data
                    if not data :
                        continue
                    plc_log = PLC_Logs(plc=plc_obj,
                                       json_data=data)
                    plc_log.save()

                    # save new key in plc_keys table
                    existing_keys = set(PLC_Keys.objects.filter(key__in=incoming_keys).values_list("key", flat=True))
                    missing_keys = incoming_keys - existing_keys
                    PLC_Keys.objects.bulk_create([PLC_Keys(key=key,value=str(type(key).__name__))for key in missing_keys])

                    plc_obj.setting.update(data)
                    for key, value in data.items():
                        plc_obj.setting[key] = value
                    plc_obj.save()

                    for tank, config in TANK_CONFIG.items():
                        status_key = config["status_key"]
                        weight_key = config["weight_key"]

                        if status_key in data:
                            process_material_step(
                                plc_obj,
                                tank,
                                data[status_key],
                            )

                        elif weight_key in data:
                            # وزن تغییر کرده، اما وضعیت مخزن ثابت است.
                            process_material_step(
                                plc_obj,
                                tank,
                                plc_obj.setting.get(status_key, 0),
                            )

                    

                    print(
                        f"wa1: {plc_obj.setting['wa1']}\n wa2: {plc_obj.setting['wa2']}\n wa3: {plc_obj.setting['wa3']}\n wa4: {plc_obj.setting['wa4']}\n wa5: {plc_obj.setting['wa5']}\n"
                    )
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