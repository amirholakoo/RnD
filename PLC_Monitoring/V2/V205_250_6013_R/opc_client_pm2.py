import os, time
import asyncio
import django
from django.utils import timezone
from asgiref.sync import sync_to_async

os.environ.setdefault("DJANGO_SETTINGS_MODULE", "config.settings")
django.setup()

from asyncua import Client
from PLC_Monitoring.models import *


def update_health(server=None, plc=None):
    obj = TCP_CONNECTION.objects.first()
    if not obj: obj = TCP_CONNECTION()
    
    if server: obj.ServerLastUpdate = time.time()
    if plc: obj.ClientLastUpdate = time.time()
    obj.save()

PLC_250_URL = "http://192.168.2.46:6010/api/settings/"
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
    

    new_data = {}

    if plc_obj.setting is None:
        plc_obj.setting = {}
    for key, value in data.items():
        if key not in plc_obj.setting:
            new_data[key] = value
            continue
        if plc_obj.setting[key] != value:
            new_data[key] = value

    print("====================",new_data,"=======================")
    data = new_data

    if not data :
        return

    plc_log = PLC_Logs(plc=plc_obj, json_data=data)
    plc_log.save()

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

    
    plc_obj.save()

async_process_opc_data = sync_to_async(process_opc_data, thread_sensitive=True)
async_get_node_ids_to_read = sync_to_async(get_node_ids_to_read,thread_sensitive=True)
async_update_health = sync_to_async(update_health,thread_sensitive=True)
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
        await async_update_health(server=True)
        try:
            node_ids = await async_get_node_ids_to_read()
            async with Client(url=OPC_URL) as client:
                data = await read_opc_values(client, node_ids)
            await async_update_health(plc=True)
            data["n"] = OPC_DEVICE_ID
            #print(f"1 ========= {data} ==========")
            await async_process_opc_data(data)
        except Exception as e:
            print(f"OPC worker error: {e}")
        await asyncio.sleep(INTERVAL)


if __name__ == "__main__":
    asyncio.run(main_loop())
