from django.urls import path
from . import views

urlpatterns = [
    path('add_truck/', views.add_truck, name='add_truck'),
    path('edit_truck/<int:truck_id>/', views.edit_truck, name='edit_truck'),
    path('delete_truck/<int:truck_id>/', views.delete_truck, name='delete_truck'),
    path('list_trucks/', views.list_trucks, name='list_trucks'),
    path('view_truck/<int:truck_id>/', views.view_truck, name='view_truck'),
]