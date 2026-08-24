from django.urls import path
from . import views

urlpatterns = [
    path('year=<int:year>month=<int:month>', views.run_calendar),
]