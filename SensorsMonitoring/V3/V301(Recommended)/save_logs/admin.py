from datetime import datetime

import jdatetime
from django.contrib import admin
from .models import SensorLogs, Device, Device_Sensor


def to_jalali(timestamp):
    if not timestamp:
        return "-"
    gregorian_datetime = datetime.fromtimestamp(timestamp)
    jalali_datetime = jdatetime.datetime.fromgregorian(datetime=gregorian_datetime)
    return jalali_datetime.strftime("%Y-%m-%d %H:%M:%S")


@admin.register(Device)
class DeviceAdmin(admin.ModelAdmin):
    list_display = (
        "device_id",
        "ip_address",
        "location",
        "name",
        "Is_Known",
        "Is_AI",
        "AI_Target",
        "get_creation_datetime",
        "get_last_update",
    )
    search_fields = (
        "device_id",
        "ip_address",
        "location",
        "name",
        "AI_Target",
        "description",
    )
    list_filter = ("Is_Known", "Is_AI")

    @admin.display(ordering="CreationDateTime", description="Creation (Jalali)")
    def get_creation_datetime(self, obj):
        return to_jalali(obj.CreationDateTime)

    @admin.display(ordering="LastUpdate", description="Last Update (Jalali)")
    def get_last_update(self, obj):
        return to_jalali(obj.LastUpdate)


@admin.register(Device_Sensor)
class DeviceSensorAdmin(admin.ModelAdmin):
    list_display = (
        "device",
        "sensor_type",
        "Is_AI",
        "AI_Target",
        "get_device_name",
        "get_device_location",
        "get_creation_datetime",
        "get_last_update",
    )
    search_fields = (
        "device__device_id",
        "device__name",
        "sensor_type",
        "AI_Target",
        "description",
        "device__location",
    )
    list_filter = ("Is_AI",)
    list_select_related = ("device",)

    @admin.display(ordering="device__name", description="Device Name")
    def get_device_name(self, obj):
        return obj.device.name

    @admin.display(ordering="device__location", description="Device Location")
    def get_device_location(self, obj):
        return obj.device.location

    @admin.display(ordering="CreationDateTime", description="Creation (Jalali)")
    def get_creation_datetime(self, obj):
        return to_jalali(obj.CreationDateTime)

    @admin.display(ordering="LastUpdate", description="Last Update (Jalali)")
    def get_last_update(self, obj):
        return to_jalali(obj.LastUpdate)


@admin.register(SensorLogs)
class SensorLogsAdmin(admin.ModelAdmin):
    list_display = (
        "sensor",
        "get_sensor_name",
        "get_sensor_is_ai",
        "get_device",
        "get_device_name",
        "get_device_location",
        "get_creation_datetime",
        "get_last_update",
    )
    search_fields = (
        "sensor__device__device_id",
        "sensor__device__name",
        "sensor__device__location",
        "sensor__sensor_type",
        "data",
    )
    list_select_related = ("sensor", "sensor__device")

    @admin.display(description="Device")
    def get_device(self, obj):
        return obj.sensor.device

    @admin.display(ordering="sensor__sensor_type", description="Sensor Name")
    def get_sensor_name(self, obj):
        return obj.sensor.sensor_type

    @admin.display(ordering="sensor__device__name", description="Device Name")
    def get_device_name(self, obj):
        return obj.sensor.device.name

    @admin.display(ordering="sensor__device__location", description="Device Location")
    def get_device_location(self, obj):
        return obj.sensor.device.location

    @admin.display(
        boolean=True,
        ordering="sensor__Is_AI",
        description="Sensor Is AI",
    )
    def get_sensor_is_ai(self, obj):
        return obj.sensor.Is_AI

    @admin.display(ordering="CreationDateTime", description="Creation (Jalali)")
    def get_creation_datetime(self, obj):
        return to_jalali(obj.CreationDateTime)

    @admin.display(ordering="LastUpdate", description="Last Update (Jalali)")
    def get_last_update(self, obj):
        return to_jalali(obj.LastUpdate)