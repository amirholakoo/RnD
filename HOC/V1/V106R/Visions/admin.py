from django.contrib import admin
from .models import *
from datetime import datetime
import jdatetime

def to_jalali(timestamp):
    if timestamp:
        try:
            to_date = datetime.fromtimestamp(float(timestamp))
            return jdatetime.datetime.fromgregorian(datetime=to_date).strftime("%Y-%m-%d %H:%M:%S")
        except:
            return ""
    return ""

@admin.register(Vision)
class VisionAdmin(admin.ModelAdmin):
    list_display = (
        "name",
        "warehouse",
        "server_ip",
        "vision_id",
        "Is_Deleted",
        "get_creation_datetime",
        "get_last_update",
    )
    search_fields = (
        "name",
        "description",
        "warehouse__name",
        "server_ip",
        "vision_id",
    )
    list_filter = ("Is_Deleted", "warehouse")
    list_select_related = ("warehouse",)

    @admin.display(ordering="CreationDateTime", description="زمان ساخت")
    def get_creation_datetime(self, obj):
        return to_jalali(obj.CreationDateTime)

    @admin.display(ordering="LastUpdate", description="آخرین آپدیت")
    def get_last_update(self, obj):
        return to_jalali(obj.LastUpdate)

@admin.register(VisionData)
class VisionDataAdmin(admin.ModelAdmin):
    list_display = (
        "vision",
        "name",
        "count",
        "enter",
        "exit",
        "get_detections_time",
        "get_creation_datetime",
        "get_last_update",
    )
    search_fields = (
        "vision__name",
        "name",
    )
    list_filter = ("vision",)
    list_select_related = ("vision",)

    @admin.display(ordering="CreationDateTime", description="زمان ساخت")
    def get_creation_datetime(self, obj):
        return to_jalali(obj.CreationDateTime)

    @admin.display(ordering="LastUpdate", description="آخرین آپدیت")
    def get_last_update(self, obj):
        return to_jalali(obj.LastUpdate)

    @admin.display(ordering="detections_time", description="زمان تشخیص")
    def get_detections_time(self, obj):
        return to_jalali(obj.detections_time)


@admin.register(OrganizingVisionData)
class OrganizingVisionDataAdmin(admin.ModelAdmin):
    list_display = (
        "id",
        "class_name",
        "Type",
        "location",
        "forklift_driver_name",
        "count",
        "quantity",
        "quality",
        "shipment",
        "get_start_time",
        "get_end_time",
        "Is_Deleted",
        "get_creation_datetime",
        "get_last_update",
    )
    search_fields = (
        "location__name",
        "forklift_driver_name__name",
        "quantity",
        "quality",
        "shipment__id",
    )
    list_filter = ("Type", "Is_Deleted", "location")
    list_select_related = ("location", "forklift_driver_name", "shipment")

    @admin.display(ordering="start_time", description="زمان شروع")
    def get_start_time(self, obj):
        return to_jalali(obj.start_time)

    @admin.display(ordering="end_time", description="زمان پایان")
    def get_end_time(self, obj):
        return to_jalali(obj.end_time)

    @admin.display(ordering="CreationDateTime", description="زمان ساخت")
    def get_creation_datetime(self, obj):
        return to_jalali(obj.CreationDateTime)

    @admin.display(ordering="LastUpdate", description="آخرین آپدیت")
    def get_last_update(self, obj):
        return to_jalali(obj.LastUpdate)