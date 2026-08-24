import os
import asyncio
import django
from django.utils import timezone
from asgiref.sync import sync_to_async

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
django.setup()

from asyncua import Client
from PLC_Monitoring.models import PLC, PLC_Logs, PLC_Keys, Rolls, Roll_Breaks

OPC_URL = "opc.tcp://172.16.1.175:4840"
OPC_DEVICE_ID = "pm3-main"
INTERVAL = 10
DEFAULT_NODE_IDS = [
    "ns=4;s=.FAN_BOT_OFST1",
    "ns=4;s=.HDBOX1_OFST1",
    "ns=4;s=SPEED_TUNE.BOT_VAC1_ASS",
]

is_running = None
roll_obj = None


def get_node_ids_to_read():
    from_db = set(PLC_Keys.objects.values_list("key", flat=True))
    return list(from_db) if from_db else DEFAULT_NODE_IDS

def process_opc_data(data: dict):
    global is_running, roll_obj
    if not data or "n" not in data:
        return
    plc_obj = PLC.objects.filter(device_id=data["n"]).first()
    if not plc_obj:
        return
    incoming_keys = set(data.keys())
    is_same = True
    if PLC_Logs.objects.count() > 0:
        is_same_log = (
            PLC_Logs.objects
            .order_by("-CreationDateTime")
            .filter(plc=plc_obj, json_data__has_key=next(iter(incoming_keys), None))
            .first()
        )
        if is_same_log and is_same_log.json_data:
            for key in incoming_keys:
                if is_same_log.json_data.get(key) != data.get(key):
                    is_same = False
                    break
    else:
        is_same = False
        
    if is_same:
        last_log = PLC_Logs.objects.order_by("-CreationDateTime").first()
        if last_log and last_log.json_data == data:
            last_log.LastUpdate = timezone.now()
            last_log.save()
        return
    
    plc_log = PLC_Logs(plc=plc_obj, data=data, json_data=data)
    plc_log.save()

    if "cr" in data:
        roll_obj, created = Rolls.objects.get_or_create(roll_number=str(int(float(data["cr"]))))
        roll_obj.roll_number = int(float(data["cr"]))
        roll_obj.save()
        plc_log.roll = roll_obj
        plc_log.save()
        roll_obj.avg_final_data()
    if "ru" in data:
        plc_log.is_running = data["ru"] == "1"
        is_running = plc_log.is_running
        plc_log.save()
    else:
        if is_running is not None:
            plc_log.is_running = is_running
            plc_log.save()

    if plc_obj.setting is None:
        plc_obj.setting = {}
    existing_keys = set(PLC_Keys.objects.filter(key__in=incoming_keys).values_list("key", flat=True))
    missing_keys = incoming_keys - existing_keys
    now = timezone.now()
    for key in missing_keys:
        val = data.get(key, "")
        PLC_Keys.objects.get_or_create(
            key=key,
            defaults={"value": type(val).__name__, "CreationDateTime": now, "LastUpdate": now},
        )

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
            try:
                me1 = int(float(data["me1"]))
                if roll_obj.Printed_length is None or roll_obj.Printed_length < me1:
                    roll_obj.Printed_length = me1
                    roll_obj.save()
            except (ValueError, TypeError):
                pass
        if "b" in data and data["b"] == "1":
            try:
                new_break = int(data["b"])
                last_break_log = (
                    PLC_Logs.objects
                    .filter(plc=plc_obj, json_data__has_key="b")
                    .exclude(id=plc_log.id)
                    .order_by("-CreationDateTime")
                    .first()
                )
                old_break = int(last_break_log.json_data.get("b", 0)) if last_break_log and last_break_log.json_data else None
                if old_break != new_break and new_break == 1:
                    roll_obj.Paper_breaks = (roll_obj.Paper_breaks or 0) + 1
                    roll_obj.save()
                    Roll_Breaks.objects.create(roll=roll_obj)
            except (ValueError, TypeError):
                pass
    else:
        roll_number = plc_obj.setting.get("cr")
        if roll_number is not None:
            try:
                roll_obj, _ = Rolls.objects.get_or_create(roll_number=int(float(roll_number)))
                plc_log.roll = roll_obj
                plc_log.save()
            except (ValueError, TypeError):
                pass
    plc_obj.save()

async_process_opc_data = sync_to_async(process_opc_data, thread_sensitive=True)
async_get_node_ids_to_read = sync_to_async(get_node_ids_to_read,thread_sensitive=True)

async def read_opc_values(client, node_ids):
    data = {}
    for nid in node_ids:
        try:
            node = client.get_node(nid)
            value = await node.read_value()
            data[nid] = str(value) if value is not None else ""
        except Exception as e:
            data[nid] = ""
    return data


async def main_loop():
    global roll_obj
    while True:
        try:
            node_ids = await async_get_node_ids_to_read()
            async with Client(url=OPC_URL) as client:
                data = await read_opc_values(client, node_ids)
            data["n"] = OPC_DEVICE_ID
            print(f"1 ========= {data} ==========")
            await async_process_opc_data(data)
        except Exception as e:
            print(f"OPC worker error: {e}")
        await asyncio.sleep(INTERVAL)


if __name__ == "__main__":
    asyncio.run(main_loop())
