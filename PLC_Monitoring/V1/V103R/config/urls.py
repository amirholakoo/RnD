from django.contrib import admin
from django.urls import path, include

urlpatterns = [
    path('admin/', admin.site.urls),
    path("", include("PLC_Monitoring.urls")),
    path('widgets/', include('AM_Calendar.urls')),
]
