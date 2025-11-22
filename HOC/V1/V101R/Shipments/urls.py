from django.urls import path
from . import views

urlpatterns = [
    path('add_shipment/', views.add_shipment, name='add_shipment'),
    path('edit_shipment/<int:shipment_id>/', views.edit_shipment, name='edit_shipment'),
    path('delete_shipment/<int:shipment_id>/', views.delete_shipment, name='delete_shipment'),
    path('list_shipments/', views.list_shipments, name='list_shipments'),
    path('view_shipment/<int:shipment_id>/', views.view_shipment, name='view_shipment'),
]