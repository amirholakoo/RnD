from django.contrib import admin
from django.urls import path
from . import views

urlpatterns = [
    path("",views.dashboard),
    path("live-settings/", views.live_settings, name="live_settings"),
    path("roll/<int:roll_id>/", views.roll_detail, name="roll_detail"),
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
    # Chart Excluded Keys
    path("api/chart-excluded-keys/toggle/", views.toggle_chart_excluded_key, name="toggle_chart_excluded_key"),
    # Historical Chart Data
    path("api/historical-chart/", views.get_historical_chart_data, name="get_historical_chart_data"),
    # Export
    path("api/export/logs/", views.export_all_logs_xlsx, name="export_all_logs_xlsx"),
]
