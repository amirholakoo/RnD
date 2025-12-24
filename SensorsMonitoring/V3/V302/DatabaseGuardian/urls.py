from django.urls import path
from . import views

app_name = 'database_guardian'

urlpatterns = [
    path('settings/', views.settings_view, name='settings'),
    path('api/save-config/', views.save_config, name='save_config'),
    path('api/save-selection/', views.save_db_selection, name='save_selection'),
    path('api/rotate/', views.rotate_database, name='rotate'),
    path('api/check-rotation/', views.check_rotation, name='check_rotation'),
    path('api/stats/', views.get_db_stats, name='stats'),
    path('api/initialize/', views.initialize_system, name='initialize'),
]

