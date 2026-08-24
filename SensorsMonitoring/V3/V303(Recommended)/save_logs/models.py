from django.db import models
import time

class Device(models.Model):
    device_id = models.CharField(max_length=50,unique=True,db_index=True)
    ip_address = models.CharField(max_length=100,blank=True,null=True,db_index=True)
    location = models.CharField(max_length=100,blank=True,null=True,db_index=True)
    name = models.CharField(max_length=100,blank=True,null=True,db_index=True)
    Is_Known = models.BooleanField(default=False,db_index=True)
    Is_AI = models.BooleanField(default=False,db_index=True)
    AI_Target = models.CharField(max_length=50,blank=True,null=True,db_index=True)
    description = models.TextField(blank=True,null=True,db_index=True)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True,db_index=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)


class Device_Sensor(models.Model):
    device = models.ForeignKey(Device,on_delete=models.CASCADE,related_name="sensors",db_index=True)
    sensor_type = models.CharField(max_length=50,db_index=True)
    Is_AI = models.BooleanField(default=False,db_index=True)
    AI_Target = models.CharField(max_length=50,blank=True,null=True,db_index=True)
    description = models.TextField(blank=True,null=True,db_index=True)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True,db_index=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)

class SensorLogs(models.Model):
    sensor = models.ForeignKey(Device_Sensor,on_delete=models.CASCADE,related_name="sensor_logs",db_index=True)
    data = models.TextField(db_index=True)
    CreationDateTime = models.FloatField(verbose_name="زمان ساخت",null=True,blank=True,db_index=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True,db_index=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)