from django.contrib import admin
from .models import Truck
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

@admin.register(Truck)
class TruckAdmin(admin.ModelAdmin):
    list_display = (
        "name",
        "License_plate_number",
        "Truck_type",
        "status",
        "location",
        "driver_name",
        "phone",
        "Is_Deleted",
        "get_creation_datetime",
        "get_last_update",
    )
    search_fields = (
        "name",
        "License_plate_number",
        "Truck_type",
        "Truck_color",
        "driver_name",
        "phone",
        "description",
    )
    list_filter = ("status", "Is_Deleted", "Truck_type")

    @admin.display(ordering="CreationDateTime", description="زمان ساخت")
    def get_creation_datetime(self, obj):
        return to_jalali(obj.CreationDateTime)

    @admin.display(ordering="LastUpdate", description="آخرین آپدیت")
    def get_last_update(self, obj):
        return to_jalali(obj.LastUpdate)
