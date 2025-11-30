from django.db import models
import time
class Vision(models.Model):
    name = models.CharField(max_length=255)
    description = models.TextField(blank=True,null=True)
    warehouse = models.ForeignKey("Warehouse.Warehouse", on_delete=models.CASCADE,related_name="vision_warehouse",verbose_name="انبار")
    server_ip = models.CharField(max_length=255,blank=True,null=True)
    vision_id = models.CharField(max_length=255,blank=True,null=True)
    Is_Deleted = models.BooleanField(default=False)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)


class VisionData(models.Model):
    vision = models.ForeignKey(Vision, on_delete=models.CASCADE,related_name="vision_data",verbose_name="ویژن")
    name = models.CharField(max_length=255,blank=True,null=True)
    count = models.IntegerField(default=0,blank=True,null=True)
    data = models.JSONField(blank=True,null=True)
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)
    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)