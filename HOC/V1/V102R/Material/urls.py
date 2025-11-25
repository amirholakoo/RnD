from django.urls import path
from . import views

urlpatterns = [
    path('', views.materials, name='materials'),
    path('add/', views.add, name='add_material'),
    path('edit/<int:material_id>/', views.edit, name='edit_material'),
    path('delete/<int:material_id>/', views.delete, name='delete_material'),
    path('view/<int:material_id>/', views.view, name='view_material'),
]