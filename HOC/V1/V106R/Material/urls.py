from django.urls import path
from . import views

urlpatterns = [
    path('', views.materials, name='materials'),
    path('add/', views.add, name='add_material'),
    path('edit/<int:material_id>/', views.edit, name='edit_material'),
    path('delete/<int:material_id>/', views.delete, name='delete_material'),
    path('view/<int:material_id>/', views.view, name='view_material'),
    path('material_types/', views.material_types, name='material_types'),
    path('add_material_type/', views.add_material_type, name='add_material_type'),
    path('edit_material_type/<int:material_type_id>/', views.edit_material_type, name='edit_material_type'),
    path('delete_material_type/<int:material_type_id>/', views.delete_material_type, name='delete_material_type'),
    path('view_material_type/<int:material_type_id>/', views.view_material_type, name='view_material_type'),
]