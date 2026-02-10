from django.urls import path
from . import views

urlpatterns = [
    path('', views.visions, name='visions'),
    path('add/', views.add, name='add_vision'),
    path('edit/<int:vision_id>/', views.edit, name='edit_vision'),
    path('delete/<int:vision_id>/', views.delete, name='delete_vision'),
    path('view/<int:vision_id>/', views.view, name='view_vision'),
    path('get_logs/', views.get_logs, name='get_logs'),
    path('start_vision_api/', views.start_vision_api, name='start_vision_api'),
]