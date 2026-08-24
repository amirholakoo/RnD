from django.urls import path, include
from . import views
urlpatterns = [
    # path('reportsPlate/', views.ReportsPlate),
    path('recommended/', views.Recommended_Plate),
]
