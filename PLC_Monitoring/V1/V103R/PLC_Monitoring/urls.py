from django.contrib import admin
from django.urls import path
from . import views

urlpatterns = [
    path("",views.dashboard),
    path("live-settings/", views.live_settings, name="live_settings"),
    # PLC CRUD
    path("api/plcs/", views.get_plcs, name="get_plcs"),
    path("api/plcs/create/", views.create_plc, name="create_plc"),
    path("api/plcs/update/", views.update_plc, name="update_plc"),
    path("api/plcs/delete/", views.delete_plc, name="delete_plc"),
    # PLC Keys CRUD
    path("api/plc-keys/", views.get_plc_keys, name="get_plc_keys"),
    path("api/plc-keys/create/", views.create_plc_key, name="create_plc_key"),
    path("api/plc-keys/update/", views.update_plc_key, name="update_plc_key"),
    path("api/plc-keys/delete/", views.delete_plc_key, name="delete_plc_key"),
    # PLC Logs SSE
    path("api/logs/stream/", views.sse_plc_logs, name="sse_plc_logs"),
    path("api/logs/", views.get_initial_logs, name="get_initial_logs"),
    # PLC Settings SSE
    path("api/settings/", views.get_plc_settings, name="get_plc_settings"),
    path("api/settings/stream/", views.sse_plc_settings, name="sse_plc_settings"),
]
