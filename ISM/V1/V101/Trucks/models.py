from django.db import models
import time

class Truck(models.Model):
    name = models.CharField(max_length=255,blank=True,null=True)
    License_plate_number = models.CharField(max_length=16)
    driver_name = models.CharField(max_length=255, blank=True,null=True)
    phone = models.CharField(max_length=20, blank=True,null=True)
    description = models.TextField(blank=True,null=True)
    Is_Deleted = models.BooleanField(default=False)
    CreationDateTime = models.DateTimeField(auto_now_add=True, db_index=True, null=True, blank=True)
    LastUpdate = models.DateTimeField(auto_now=True, null=True, blank=True)
