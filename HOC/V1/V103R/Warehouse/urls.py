from django.urls import path
from . import views

urlpatterns = [
    path('add/', views.add, name='add_warehouse'),
    path('edit/<int:warehouse_id>/', views.edit, name='edit_warehouse'),
    path('delete/<int:warehouse_id>/', views.delete, name='delete_warehouse'),
    path('', views.warehouses, name='warehouses'),
    path('view/<int:warehouse_id>/', views.view, name='view_warehouse'),
]