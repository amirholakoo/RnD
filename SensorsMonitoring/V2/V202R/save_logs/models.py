from django.db import models
import time

class Device(models.Model):
    device_id = models.CharField(max_length=50,unique=True)
    ip_address = models.CharField(max_length=100,blank=True,null=True)
    location = models.CharField(max_length=100,blank=True,null=True)
    name = models.CharField(max_length=100,blank=True,null=True)
    Is_Known = models.BooleanField(default=False)
    description = models.TextField(blank=True,null=True)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)


class Device_Sensor(models.Model):
    device = models.ForeignKey(Device,on_delete=models.CASCADE,related_name="sensors")
    sensor_type = models.CharField(max_length=50)
    description = models.TextField(blank=True,null=True)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)

class SensorLogs(models.Model):
    sensor = models.ForeignKey(Device_Sensor,on_delete=models.CASCADE,related_name="sensor_logs")
    data = models.TextField()
    CreationDateTime = models.FloatField(verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)