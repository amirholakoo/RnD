from django.urls import path
from . import views

urlpatterns = [
    path('', views.shipments, name='shipments'),
    path('add/', views.add, name='add_shipment'),
    path('edit/<int:shipment_id>/', views.edit, name='edit_shipment'),
    path('delete/<int:shipment_id>/', views.delete, name='delete_shipment'),
    path('view/<int:shipment_id>/', views.view, name='view_shipment'),
]