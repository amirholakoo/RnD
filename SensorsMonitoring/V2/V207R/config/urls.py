from django.contrib import admin
from django.urls import path, include

urlpatterns = [
    path('admin/', admin.site.urls),
    path("", include("save_logs.urls")),
    path("dashboard/", include("dashboard.urls"))
]
