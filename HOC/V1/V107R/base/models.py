from django.db import models
import time
class VersionControl(models.Model):
    version = models.CharField(max_length=6,default="0",verbose_name="نسخه نرم افزار")
    CreationDateTime = models.CharField(max_length=50,default=time.time(),verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.CharField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)
        