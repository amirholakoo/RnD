from django.urls import path
from . import views

urlpatterns = [
    path("", views.dashboard, name="dashboard"),
    path("start_vision/", views.start_vision_api, name="start_vision_api"),
]