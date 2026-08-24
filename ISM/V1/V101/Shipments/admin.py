from django.contrib import admin
from .models import Shipment
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

@admin.register(Shipment)
class ShipmentAdmin(admin.ModelAdmin):
    list_display = (
        "name",
        "status",
        "shipment_type",
        "location",
        "Customer",
        "Supplier",
        "Truck",
        "material",
        "get_receive_date",
        "get_creation_datetime",
        "get_last_update",
    )
    search_fields = (
        "name",
        "location__name",
        "Customer__name",
        "Supplier__name",
        "Truck__License_plate_number",
        "material__name",
        "description",
    )
    list_filter = ("status", "shipment_type", "Is_Deleted", "location", "invoice_status", "payment_status")
    list_select_related = ("location", "Customer", "Supplier", "Truck", "material")

    @admin.display(ordering="receive_date", description="تاریخ دریافت")
    def get_receive_date(self, obj):
        return to_jalali(obj.receive_date)

    @admin.display(ordering="CreationDateTime", description="زمان ساخت")
    def get_creation_datetime(self, obj):
        return to_jalali(obj.CreationDateTime)

    @admin.display(ordering="LastUpdate", description="آخرین آپدیت")
    def get_last_update(self, obj):
        return to_jalali(obj.LastUpdate)
