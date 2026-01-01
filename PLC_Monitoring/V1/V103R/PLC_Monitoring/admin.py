from django.contrib import admin
from .models import PLC, PLC_Logs, PLC_Keys, Rolls, Roll_Breaks


@admin.register(PLC)
class PLCAdmin(admin.ModelAdmin):
    list_display = ('id', 'device_id', 'ip_address', 'location', 'name', 'Is_Known', 'CreationDateTime', 'LastUpdate')
    list_filter = ('Is_Known', 'location')
    search_fields = ('device_id', 'ip_address', 'name', 'location')
    ordering = ('-CreationDateTime',)


@admin.register(Rolls)
class RollsAdmin(admin.ModelAdmin):
    list_display = ('id', 'plc', 'roll_number', 'Printed_length', 'Paper_breaks', 'Is_Deleted', 'CreationDateTime', 'LastUpdate')
    list_filter = ('plc', 'Is_Deleted', 'roll_number')
    search_fields = ('roll_number',)
    ordering = ('-CreationDateTime',)


@admin.register(Roll_Breaks)
class RollBreaksAdmin(admin.ModelAdmin):
    list_display = ('id', 'roll', 'break_reason', 'CreationDateTime', 'LastUpdate')
    list_filter = ('roll',)
    search_fields = ('break_reason',)
    ordering = ('-CreationDateTime',)


@admin.register(PLC_Logs)
class PLCLogsAdmin(admin.ModelAdmin):
    list_display = ('id', 'roll', 'is_running', 'plc', 'data', 'CreationDateTime', 'LastUpdate')
    list_filter = ('plc', 'roll', 'is_running')
    search_fields = ('data',)
    ordering = ('-CreationDateTime',)


@admin.register(PLC_Keys)
class PLCKeysAdmin(admin.ModelAdmin):
    list_display = ('id', 'name', 'fa_name', 'key', 'value', 'description', 'CreationDateTime', 'LastUpdate')
    search_fields = ('name', 'fa_name', 'key', 'value')
    ordering = ('-CreationDateTime',)