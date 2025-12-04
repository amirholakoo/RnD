from django.db import models
from Trucks.models import Truck
from Warehouse.models import Warehouse
#from Units.models import Unit
import time

class Forklift_Drivers(models.Model):
    name = models.CharField(max_length=255,blank=True,null=True)
    phone = models.CharField(max_length=255,blank=True,null=True)
    Is_Deleted = models.BooleanField(default=False)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)

class Unload(models.Model):
    license_number = models.ForeignKey(Truck, on_delete=models.PROTECT)
    unload_location = models.ForeignKey(Warehouse, on_delete=models.PROTECT)
    #unit = models.ForeignKey(Unit, on_delete=models.PROTECT)
    quantity = models.CharField(max_length=255,blank=True,null=True)
    quality = models.CharField(max_length=255,blank=True,null=True)
    forklift_driver_name = models.ForeignKey(Forklift_Drivers, on_delete=models.PROTECT)
    Is_Deleted = models.BooleanField(default=False)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)
    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)