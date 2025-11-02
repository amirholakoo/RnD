from django.contrib import admin
from .models import SensorLogs, Device, Device_Sensor

# Register your models here.
admin.site.register(SensorLogs)
admin.site.register(Device)
admin.site.register(Device_Sensor)