from django.contrib import admin
from django.urls import path, include
from Visions.views import Receive_Data
urlpatterns = [
    path('admin/', admin.site.urls),
    path('widgets/', include('AM_Calendar.urls')),
    path('warehouse/', include('Warehouse.urls')),
    path('visions/', include('Visions.urls')),
    path('trucks/', include('Trucks.urls')),
    path('shipments/', include('Shipments.urls')),
    path('supplier/', include('Supplier.urls')),
    path('customer/', include('Customer.urls')),
    path('dashboard/', include('Dashboard.urls')),
    path('receive_data/', Receive_Data, name="receive_data"),
    path('material/', include('Material.urls')),
    path('forklift/', include('Forklift.urls')),
    path('unit/', include('Unit.urls')),
]
