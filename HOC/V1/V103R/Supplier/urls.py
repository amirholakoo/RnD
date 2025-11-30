from django.urls import path
from . import views

urlpatterns = [
    path('', views.suppliers, name='suppliers'),
    path('add/', views.add, name='add_supplier'),
    path('edit/<int:supplier_id>/', views.edit, name='edit_supplier'),
    path('delete/<int:supplier_id>/', views.delete, name='delete_supplier'),
    path('view/<int:supplier_id>/', views.view, name='view_supplier'),
]