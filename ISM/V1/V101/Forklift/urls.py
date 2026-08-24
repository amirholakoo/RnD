from django.urls import path
from . import views

urlpatterns = [
    path('', views.forklift_panel, name='forklift_panel'),
    path('unload/', views.unload, name='unload'),
    path('unload/edit/<int:id>/', views.edit_unload, name='edit_unload'),
    path('unload/delete/<int:id>/', views.delete_unload, name='delete_unload'),
    path('unload/view/<int:id>/', views.view_unload, name='view_unload'),
    path('forklift_drivers/', views.forklift_drivers, name='forklift_drivers'),
    path('forklift_drivers/add/', views.add_forklift_driver, name='add_forklift_driver'),
    path('forklift_drivers/edit/<int:id>/', views.edit_forklift_driver, name='edit_forklift_driver'),
    path('forklift_drivers/delete/<int:id>/', views.delete_forklift_driver, name='delete_forklift_driver'),
    path('forklift_drivers/view/<int:id>/', views.view_forklift_driver, name='view_forklift_driver'),
]