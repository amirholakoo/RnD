from django.urls import path
from . import views

urlpatterns = [
    path('', views.trucks, name='trucks'),
    path('add/', views.add_truck, name='add_truck'),
    path('edit/<int:id>/', views.edit_truck, name='edit_truck'),
    path('delete/<int:id>/', views.delete_truck, name='delete_truck'),
    path('view/<int:id>/', views.view_truck, name='view_truck'),
]