from django.urls import path
from . import views

urlpatterns = [
    path('', views.customers, name='customers'),
    path('add/', views.add, name='add_customer'),
    path('edit/<int:customer_id>/', views.edit, name='edit_customer'),
    path('delete/<int:customer_id>/', views.delete, name='delete_customer'),
    path('view/<int:customer_id>/', views.view, name='view_customer'),
]