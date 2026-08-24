from django.contrib import admin
from .models import Material, MaterialType
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

@admin.register(Material)
class MaterialAdmin(admin.ModelAdmin):
    list_display = (
        "name",
        "material_type",
        "supplier_name",
        "Is_Active",
        "Is_Deleted",
        "get_creation_datetime",
        "get_last_update",
    )
    search_fields = (
        "name",
        "description",
        "supplier_name",
        "material_type__material_type",
    )
    list_filter = ("Is_Active", "Is_Deleted", "material_type")
    list_select_related = ("material_type",)

    @admin.display(ordering="CreationDateTime", description="زمان ساخت")
    def get_creation_datetime(self, obj):
        return to_jalali(obj.CreationDateTime)

    @admin.display(ordering="LastUpdate", description="آخرین آپدیت")
    def get_last_update(self, obj):
        return to_jalali(obj.LastUpdate)

@admin.register(MaterialType)
class MaterialTypeAdmin(admin.ModelAdmin):
    list_display = (
        "material_type",
        "supplier_name",
        "Is_Active",
        "Is_Deleted",
        "get_creation_datetime",
        "get_last_update",
    )
    search_fields = (
        "material_type",
        "supplier_name",
        "description",
    )
    list_filter = ("Is_Active", "Is_Deleted")

    @admin.display(ordering="CreationDateTime", description="زمان ساخت")
    def get_creation_datetime(self, obj):
        return to_jalali(obj.CreationDateTime)

    @admin.display(ordering="LastUpdate", description="آخرین آپدیت")
    def get_last_update(self, obj):
        return to_jalali(obj.LastUpdate)
