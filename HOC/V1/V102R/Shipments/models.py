from django.db import models
import time
from Trucks.models import Truck
class Shipment(models.Model):
    status_choices = ((1,"Pending"),(2,"In Progress"),(3,"Completed"),(4,"Cancelled"))
    name = models.CharField(max_length=255,blank=True,null=True)
    description = models.TextField(blank=True,null=True)
    Truck = models.ForeignKey(Truck, on_delete=models.PROTECT)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)

