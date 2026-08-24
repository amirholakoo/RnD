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
        "License_plate_number",
        "driver_name",
        "phone",
        "Is_Deleted",
        "get_creation_datetime",
        "get_last_update",
    )
    search_fields = (
        "License_plate_number",
        "driver_name",
        "phone",
        "description",
    )
    list_filter = ( "Is_Deleted",)

    @admin.display(ordering="CreationDateTime", description="زمان ساخت")
    def get_creation_datetime(self, obj):
        return to_jalali(obj.CreationDateTime)

    @admin.display(ordering="LastUpdate", description="آخرین آپدیت")
    def get_last_update(self, obj):
        return to_jalali(obj.LastUpdate)
