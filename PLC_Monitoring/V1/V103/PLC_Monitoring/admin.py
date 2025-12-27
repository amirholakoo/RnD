from django.contrib import admin
from .models import PLC, PLC_Logs, PLC_Keys


@admin.register(PLC)
class PLCAdmin(admin.ModelAdmin):
    list_display = ('id', 'device_id', 'ip_address', 'location', 'name', 'Is_Known', 'CreationDateTime', 'LastUpdate')
    list_filter = ('Is_Known', 'location')
    search_fields = ('device_id', 'ip_address', 'name', 'location')
    ordering = ('-CreationDateTime',)


@admin.register(PLC_Logs)
class PLCLogsAdmin(admin.ModelAdmin):
    list_display = ('id', 'plc', 'data', 'CreationDateTime', 'LastUpdate')
    list_filter = ('plc',)
    search_fields = ('data',)
    ordering = ('-CreationDateTime',)


@admin.register(PLC_Keys)
class PLCKeysAdmin(admin.ModelAdmin):
    list_display = ('id', 'name', 'key', 'value', 'description', 'CreationDateTime', 'LastUpdate')
    search_fields = ('name', 'key', 'value')
    ordering = ('-CreationDateTime',)