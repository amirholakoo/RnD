from django.contrib import admin
from .models import PLC, PLC_Logs, PLC_Keys, Rolls, Roll_Breaks, ChartExcludedKeys
import jdatetime
from datetime import datetime


class JalaliDateTimeMixin:
    """Mixin for Jalali datetime display in admin"""
    def jalali_creation_datetime(self, obj):
        if obj.CreationDateTime:
            dt = datetime.fromtimestamp(obj.CreationDateTime)
            jdt = jdatetime.datetime.fromgregorian(datetime=dt)
            return jdt.strftime('%Y/%m/%d %H:%M:%S')
        return '-'
    jalali_creation_datetime.short_description = 'زمان ساخت'
    jalali_creation_datetime.admin_order_field = 'CreationDateTime'

    def jalali_last_update(self, obj):
        if obj.LastUpdate:
            dt = datetime.fromtimestamp(obj.LastUpdate)
            jdt = jdatetime.datetime.fromgregorian(datetime=dt)
            return jdt.strftime('%Y/%m/%d %H:%M:%S')
        return '-'
    jalali_last_update.short_description = 'آخرین آپدیت'
    jalali_last_update.admin_order_field = 'LastUpdate'


@admin.register(PLC)
class PLCAdmin(JalaliDateTimeMixin, admin.ModelAdmin):
    list_display = ('id', 'device_id', 'ip_address', 'location', 'name', 'Is_Known', 'jalali_creation_datetime', 'jalali_last_update')
    list_filter = ('Is_Known', 'location')
    search_fields = ('device_id', 'ip_address', 'name', 'location')
    ordering = ('-CreationDateTime',)


@admin.register(Rolls)
class RollsAdmin(JalaliDateTimeMixin, admin.ModelAdmin):
    list_display = ('id', 'plc', 'roll_number', 'Printed_length', 'Paper_breaks', 'Is_Deleted', 'jalali_creation_datetime', 'jalali_last_update')
    list_filter = ('plc', 'Is_Deleted', 'roll_number')
    search_fields = ('roll_number',)
    ordering = ('-CreationDateTime',)


@admin.register(Roll_Breaks)
class RollBreaksAdmin(JalaliDateTimeMixin, admin.ModelAdmin):
    list_display = ('id', 'roll', 'break_reason', 'jalali_creation_datetime', 'jalali_last_update')
    list_filter = ('roll',)
    search_fields = ('break_reason',)
    ordering = ('-CreationDateTime',)


@admin.register(PLC_Logs)
class PLCLogsAdmin(JalaliDateTimeMixin, admin.ModelAdmin):
    list_display = ('id', 'roll', 'is_running', 'plc', 'data', 'jalali_creation_datetime', 'jalali_last_update')
    list_filter = ('plc', 'roll', 'is_running')
    search_fields = ('data',)
    ordering = ('-CreationDateTime',)


@admin.register(PLC_Keys)
class PLCKeysAdmin(JalaliDateTimeMixin, admin.ModelAdmin):
    list_display = ('id', 'name', 'fa_name', 'key', 'value', 'description', 'jalali_creation_datetime', 'jalali_last_update')
    search_fields = ('name', 'fa_name', 'key', 'value')
    ordering = ('-CreationDateTime',)


@admin.register(ChartExcludedKeys)
class ChartExcludedKeysAdmin(JalaliDateTimeMixin, admin.ModelAdmin):
    list_display = ('id', 'key', 'jalali_creation_datetime')
    search_fields = ('key',)
    ordering = ('-CreationDateTime',)