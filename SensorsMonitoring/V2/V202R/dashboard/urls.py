from django.urls import path
from . import views

urlpatterns = [
    path("", views.dashboard, name="dashboard"),
    path("device_info/<int:device_id>/<int:sensor_id>/", views.device_info, name="device_info"),
    path("device_info_json/<int:sensor_id>/", views.device_info_json, name="device_info_json"),
    path("ask_for_device/", views.ask_for_device, name="ask_for_device"),
    path("chart_info_json/<int:sensor_id>", views.chart_info_json, name="chart_info_json"),
    path("add_device/", views.add_device, name="add_device"),
    path("get_sensor_status/", views.get_sensor_status, name="get_sensor_status"),
    path("update_prediction/", views.update_prediction, name="update_prediction"),
    path("get_device_data/<int:device_id>/", views.get_device_data, name="get_device_data"),
    path("get_sensor_data/<int:sensor_id>/", views.get_sensor_data, name="get_sensor_data"),
    path("update_device/", views.update_device, name="update_device"),
    path("update_sensor/", views.update_sensor, name="update_sensor"),
]