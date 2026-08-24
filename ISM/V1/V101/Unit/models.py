from django.db import models
import time

class Unit(models.Model):
    shipment = models.ForeignKey("Shipments.Shipment", on_delete=models.PROTECT,blank=True,null=True,verbose_name="مرسوله",related_name="unit_shipment")
    name = models.CharField(max_length=255,verbose_name="نام واحد")
    description = models.TextField(blank=True,null=True,verbose_name="توضیحات")
    Is_Deleted = models.BooleanField(default=False,verbose_name="حذف شده")
    CreationDateTime = models.FloatField(max_length=50,verbose_name="زمان ساخت",null=True,blank=True)
    LastUpdate = models.FloatField(max_length=50,verbose_name="آخرین آپدیت",null=True,blank=True)

    def save(self, *args, **kwargs):
        if not self.CreationDateTime:
            self.CreationDateTime = time.time()
        self.LastUpdate = time.time()
        super().save(*args, **kwargs)