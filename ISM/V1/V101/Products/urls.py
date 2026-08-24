from django.urls import path
from . import views
urlpatterns = [
    path('add', views.add_products, name='add_products'),
    path('edit/<int:id>/', views.edit_products, name='edit_products'),
    path('delete/<int:id>/', views.delete_products, name='delete_products'),
    path('view/<int:id>/', views.view_products, name='view_products'),
    path('products_type/add', views.add_products_type, name='add_products_type'),
    path('products_list_stream', views.ProductsListSSE, name='products_list_stream'),
    path('call_api_for_last_roll', views.call_api_for_last_roll, name='api_for_last_rol'),
    path('qr_setting', views.QR_SettingsView, name='qr_setting'),
]
