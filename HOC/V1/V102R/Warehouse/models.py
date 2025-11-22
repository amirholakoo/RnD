from django.db import models
import time

class Warehouse(models.Model):
    name = models.CharField(max_length=255)
    description = models.TextField(blank=True,null=True)
    location = models.CharField(max_length=255,blank=True,null=True)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)
