from django.contrib import admin
from django.urls import path, include
from Dashboard.views import Receive_Data
urlpatterns = [
    path('admin/', admin.site.urls),
    path('widgets/', include('AM_Calendar.urls')),
    # path('warehouse/', include('Warehouse.urls')),
    # path('trucks/', include('Trucks.urls')),
    # path('shipments/', include('Shipments.urls')),
    path('dashboard/', include('Dashboard.urls')),
    path('receive_data/', Receive_Data, name="receive_data"),
]
