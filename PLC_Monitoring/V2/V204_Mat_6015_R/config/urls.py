from django.contrib import admin
from django.urls import path, include
from base.views import get_latest_version
urlpatterns = [
    path('admin/', admin.site.urls),
    path("", include("PLC_Monitoring.urls")),
    path('widgets/', include('AM_Calendar.urls')),
    path("api/version/", get_latest_version)
]
