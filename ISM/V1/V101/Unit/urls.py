from django.urls import path
from . import views

urlpatterns = [
    path('', views.units, name='units'),
    path('add/', views.add, name='add_unit'),
    path('edit/<int:unit_id>/', views.edit, name='edit_unit'),
    path('delete/<int:unit_id>/', views.delete, name='delete_unit'),
    path('view/<int:unit_id>/', views.view, name='view_unit'),
]