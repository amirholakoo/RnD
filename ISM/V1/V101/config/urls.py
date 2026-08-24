from django.contrib import admin
from django.urls import path, include
from django.conf import settings
from django.conf.urls.static import static
urlpatterns = [
    path('admin/', admin.site.urls),
    path('', include('base.urls')),
    path('widgets/', include('AM_Calendar.urls')),
    path('api/licence_plate/', include('LicenceNumber.urls')),
    path('api/truck/', include('Trucks.urls')),
    path('api/shipments/', include('Shipments.urls')),
    path('api/products_machine/', include('ProductsMachine.urls')),
    path('api/products/', include('Products.urls')),
]


if settings.DEBUG:
    urlpatterns += static(settings.MEDIA_URL, document_root=settings.MEDIA_ROOT)