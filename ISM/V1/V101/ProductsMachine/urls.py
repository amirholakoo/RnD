from django.urls import path
from . import views
urlpatterns = [
    path('add', views.add_machine, name='add_machine'),
    path('edit/<int:id>/', views.edit_machine, name='edit_machine'),
    path('delete/<int:id>/', views.delete_machine, name='delete_machine'),
    path('view/<int:id>/', views.view_machine, name='view_machine'),
]
