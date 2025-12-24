from django.contrib import admin
from .models import PLC, PLC_Logs

admin.site.register(PLC)
admin.site.register(PLC_Logs)