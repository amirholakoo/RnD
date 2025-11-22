from django.db import models
import time

class Truck(models.Model):
    name = models.CharField(max_length=255,blank=True,null=True)
    License_plate_number = models.CharField(max_length=16)
    Truck_type = models.CharField(max_length=255,blank=True,null=True)
    Truck_color = models.CharField(max_length=255,blank=True,null=True)
    description = models.TextField(blank=True,null=True)
    Is_Deleted = models.BooleanField(default=False)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)
