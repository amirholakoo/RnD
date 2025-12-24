from django.contrib import admin
from .models import Forklift_Drivers, Unload
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

@admin.register(Forklift_Drivers)
class ForkliftDriversAdmin(admin.ModelAdmin):
    list_display = (
        "name",
        "phone",
        "Is_Deleted",
        "get_creation_datetime",
        "get_last_update",
    )
    search_fields = (
        "name",
        "phone",
    )
    list_filter = ("Is_Deleted",)

    @admin.display(ordering="CreationDateTime", description="زمان ساخت")
    def get_creation_datetime(self, obj):
        return to_jalali(obj.CreationDateTime)

    @admin.display(ordering="LastUpdate", description="آخرین آپدیت")
    def get_last_update(self, obj):
        return to_jalali(obj.LastUpdate)

@admin.register(Unload)
class UnloadAdmin(admin.ModelAdmin):
    list_display = (
        "shipment",
        "unload_location",
        "forklift_driver_name",
        "quantity",
        "quality",
        "Is_Deleted",
        "get_creation_datetime",
        "get_last_update",
    )
    search_fields = (
        "shipment__Truck__License_plate_number",
        "unload_location__name",
        "forklift_driver_name__name",
        "quantity",
        "quality",
    )
    list_filter = ("Is_Deleted", "unload_location", "forklift_driver_name")
    list_select_related = ("shipment", "unload_location", "forklift_driver_name")

    @admin.display(ordering="CreationDateTime", description="زمان ساخت")
    def get_creation_datetime(self, obj):
        return to_jalali(obj.CreationDateTime)

    @admin.display(ordering="LastUpdate", description="آخرین آپدیت")
    def get_last_update(self, obj):
        return to_jalali(obj.LastUpdate)
